#pragma once

#include "drivers/symbolic.hpp" // compile_time_factorial
#include "dual/taylor_dual.hpp"
#include "expr/equation.hpp"
#include "md/md.hpp"
#include "rt/coupling.hpp"
#include "rt/derivative.hpp"
#include "rt/expr.hpp"
#include "rt/graph.hpp"
#include "rt/interpret.hpp"

// The JIT is optional.  Everything below has the same signature either way --
// without it the batch calls run the interpreter, and the call site does not
// change.  That is what makes this one API rather than two.
#ifdef DDX_HAS_JIT
#include "jit/kernel.hpp"
#endif

#include <algorithm>
#include <concepts>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

// The runtime half of Equation.  Keyed on the RTExpression<T> *pattern*, not on
// a constraint: a `requires` over <TFirst, TRest...> has the same pattern as
// the compile-time specialisation in expr/equation.hpp and would be ambiguous
// with it, because CExpression and a runtime concept do not subsume one
// another.
namespace ddx::impl {

namespace rt_detail {

// A point: a bare number, a named value, or a range of numbers.
template <typename A, typename T>
concept CPointArg =
    Numeric<std::remove_cvref_t<A>> || CNamedValue<A> ||
    (std::ranges::input_range<A> &&
     Numeric<std::remove_cvref_t<std::ranges::range_value_t<A>>>);

// A batch: a contiguous range of column pointers.  This is what keeps the
// per-point and batch overloads apart -- CEvalArg admits any input_range, so
// without it a span of columns matches the point overload and tries to become
// a scalar.
template <typename R, typename Ptr>
concept CColumns = std::ranges::contiguous_range<R> &&
                   std::convertible_to<std::ranges::range_value_t<R>, Ptr>;

} // namespace rt_detail

template <Numeric T, typename... Rest>
  requires(std::same_as<Rest, rt::RTExpression<T>> && ...)
class Equation<rt::RTExpression<T>, Rest...> {
public:
  using value_type = T;
  static constexpr std::size_t output_dim = 1 + sizeof...(Rest);

  // Owning: the arena is handed over with the expressions built in it, so the
  // caller never has to keep it alive.  The Builder is heap-allocated, so the
  // node pointers inside the expressions stay valid across this move.
  Equation(std::unique_ptr<rt::Builder<T>> owned, rt::RTExpression<T> first,
           Rest... rest)
      : Equation(first, rest...) {
    // Same object either way; only who frees it changes.
    arena_ = ArenaPtr{owned.release(), reclaim};
  }

  constexpr explicit Equation(rt::RTExpression<T> first, Rest... rest)
      : arena_(first.builder(), borrow) {
    if (arena_ == nullptr) {
      throw std::invalid_argument(
          "Equation: the expression has no graph (a bare literal names none)");
    }
    roots_.reserve(output_dim);
    roots_.push_back(first.id(*arena_));
    (roots_.push_back(rest.id(*arena_)), ...);

    // Eagerly, not on first use.  One sweep over the graph is microseconds --
    // it is the JIT compile that is worth deferring -- and computing it here
    // keeps every accessor below const, constexpr and free of mutable state.
    derivative_ = rt::jacobian(*arena_, roots_);
  }

  [[nodiscard]] constexpr std::size_t arity() const {
    return arena_->symbols().size();
  }
  [[nodiscard]] constexpr std::span<const std::string> symbols() const {
    return arena_->symbols();
  }

  // Every spelling make_point accepts, resolved against a run-time symbol list
  // rather than a compile-time one.
  template <rt_detail::CPointArg<T>... Args>
  [[nodiscard]] constexpr std::vector<T> point(const Args &...args) const {
    std::vector<T> at(arity(), T{});
    if constexpr ((CNamedValue<Args> && ...) && sizeof...(Args) > 0) {
      (assign_named(at, args), ...);
    } else if constexpr (sizeof...(Args) == 1 &&
                         (std::ranges::input_range<Args> && ...)) {
      assign_range(at, args...);
    } else {
      const std::array<T, sizeof...(Args)> positional{static_cast<T>(args)...};
      assign_range(at, positional);
    }
    return at;
  }

  [[nodiscard]] constexpr auto
  evaluate(const rt_detail::CPointArg<T> auto &...args) const {
    const auto values = rt::evaluate_all(*arena_, point(args...));
    if constexpr (output_dim == 1) {
      return values[roots_[0]];
    } else {
      return roots_ |
             std::views::transform([&](rt::NodeId r) { return values[r]; }) |
             std::ranges::to<std::vector<T>>();
    }
  }

  [[nodiscard]] constexpr std::vector<T>
  gradient(const rt_detail::CPointArg<T> auto &...args) const
    requires(output_dim == 1)
  {
    return harvest(derivative_.partial, point(args...));
  }

  // Row-major, m x n, matching Equation::jacobian's extents<output_dim,
  // input_dim>.
  [[nodiscard]] constexpr std::vector<T>
  jacobian(const rt_detail::CPointArg<T> auto &...args) const {
    return harvest(derivative_.partial, point(args...));
  }

  // Dense and row-major: n x n for a scalar function, m x n x n for a system,
  // which is the shape Equation::hessian returns on the compile-time side
  // (nd_tensor_t and nd_stack_t respectively).  The graph holds each block
  // compressed by colour; scattering on the way out means a caller never sees
  // that.
  [[nodiscard]] std::vector<T>
  hessian(const rt_detail::CPointArg<T> auto &...args) const {
    const auto &blocks = this->cached_hessians();
    const auto values = rt::evaluate_all(*arena_, point(args...));
    const std::size_t n = arity();

    std::vector<T> out(output_dim * n * n);
    // m x n x n, named once here rather than as `(k * n + i) * n + j` at the
    // one place that writes it.
    const impl::md::mdspan dense{
        out.data(), impl::md::dextents<std::size_t, 3>{output_dim, n, n}};
    for (const auto [k, block] : std::views::enumerate(blocks)) {
      for (const auto [i, j] : std::views::cartesian_product(
               std::views::iota(0uz, n), std::views::iota(0uz, n))) {
        dense[static_cast<std::size_t>(k), i, j] = values[block.at(i, j)];
      }
    }
    return out;
  }

  // How many sweeps the Hessian costs: one per colour, not one per symbol.
  // Worth exposing, because whether that is a saving is a property of the
  // expression -- a mixing rule couples everything and colours in n.
  [[nodiscard]] std::size_t hessian_colors() const
    requires(output_dim == 1)
  {
    return this->cached_hessians().front().colors();
  }

  // The K-th derivative of a one-variable function, by one Taylor sweep rather
  // than K nested duals.  Exactly what univariate_derivative_impl does on the
  // compile-time side: seed c[0] = x0 and c[1] = 1, then read c[Order] and
  // un-normalise it -- TaylorDual stores f^(k)/k!.
  //
  // The arity check is a throw, not a constraint: on the compile-time side
  // input_dim is known to the type, and here it is not.
  template <std::size_t Order>
  [[nodiscard]] T univariate_derivative(T x0) const
    requires(output_dim == 1 && Order > 0)
  {
    if (arity() != 1) {
      throw std::invalid_argument(
          "univariate_derivative: needs exactly one symbol, this has " +
          std::to_string(arity()));
    }
    using Taylor = impl::TaylorDual<T, Order>;
    Taylor seed;
    seed.c[0] = x0;
    seed.c[1] = T{1};

    const std::array<Taylor, 1> at{seed};
    const auto values = rt::evaluate_all(*arena_, at);
    return values[roots_[0]].c[Order] *
           static_cast<T>(impl::detail::compile_time_factorial(Order));
  }

  // --- batch ---------------------------------------------------------------
  //
  // Spans of columns, not pointer-to-pointer: `xs[j]` is the column for symbol
  // j and each output span holds one pointer per output column, every column n
  // long.  A vector or an array converts on its own, and the sizes are checked
  // against what the graph actually produces -- a mismatched column count is
  // the failure mode this ABI has, and it is silent memory corruption when it
  // is not caught here.
  //
  // Compilation is lazy and cached: an Equation only ever asked for single
  // points never pays the ~9 ms, and one asked repeatedly pays it once.

  void gradient(const rt_detail::CColumns<const T *> auto &xs,
                const rt_detail::CColumns<T *> auto &f,
                const rt_detail::CColumns<T *> auto &g, std::size_t n) const
    requires(output_dim == 1)
  {
    dispatch(compiled(false), as_columns(xs), as_columns(f), as_columns(g), {},
             n);
  }

  void jacobian(const rt_detail::CColumns<const T *> auto &xs,
                const rt_detail::CColumns<T *> auto &f,
                const rt_detail::CColumns<T *> auto &g, std::size_t n) const {
    dispatch(compiled(false), as_columns(xs), as_columns(f), as_columns(g), {},
             n);
  }

  void hessian(const rt_detail::CColumns<const T *> auto &xs,
               const rt_detail::CColumns<T *> auto &f,
               const rt_detail::CColumns<T *> auto &g,
               const rt_detail::CColumns<T *> auto &h, std::size_t n) const
    requires(output_dim == 1)
  {
    dispatch(compiled(true), as_columns(xs), as_columns(f), as_columns(g),
             as_columns(h), n);
  }

  // How many columns each block needs, so a caller can size its buffers without
  // rederiving the shape.
  [[nodiscard]] std::size_t value_columns() const {
    return compiled(false).graph.layout().values;
  }
  [[nodiscard]] std::size_t jacobian_columns() const {
    return compiled(false).graph.layout().jacobian;
  }
  [[nodiscard]] std::size_t hessian_columns() const
    requires(output_dim == 1)
  {
    return compiled(true).graph.layout().hessian;
  }

private:
  template <CNamedValue V>
  constexpr void assign_named(std::vector<T> &at, const V &nv) const {
    const auto name = std::remove_cvref_t<V>::symbol.view();
    const auto it = std::ranges::find(symbols(), name);
    if (it == symbols().end()) {
      throw std::invalid_argument("Equation: no such symbol: " +
                                  std::string(name));
    }
    at[static_cast<std::size_t>(it - symbols().begin())] =
        static_cast<T>(nv.value);
  }

  constexpr void assign_range(std::vector<T> &at,
                              const std::ranges::input_range auto &r) const {
    if (std::ranges::size(r) != at.size()) {
      throw std::invalid_argument("Equation: the point has the wrong arity");
    }
    std::ranges::transform(r, at.begin(),
                           [](const auto &v) { return static_cast<T>(v); });
  }

  // Lazy, unlike the gradient.  A Hessian is one sweep per colour over a graph
  // the sweeps themselves keep growing -- measured at ~1.2M nodes for a
  // 50-species UNIQUAC.  Paying that when the caller only wanted a gradient
  // would be indefensible, and it is the one accessor here that cannot be
  // constexpr as a result.
  const std::vector<rt::Hessian> &cached_hessians() const {
    if (hessians_.empty()) {
      hessians_ = roots_ | std::views::transform([&](rt::NodeId r) {
                    return rt::hessian(*arena_, r);
                  }) |
                  std::ranges::to<std::vector<rt::Hessian>>();
    }
    return hessians_;
  }

  [[nodiscard]] constexpr std::vector<T>
  harvest(const std::vector<rt::NodeId> &nodes,
          const std::vector<T> &at) const {
    const auto values = rt::evaluate_all(*arena_, at);
    return nodes |
           std::views::transform([&](rt::NodeId n) { return values[n]; }) |
           std::ranges::to<std::vector<T>>();
  }

  // A frozen graph plus, where the JIT is built and the scalar is one it can
  // emit, the kernel compiled from it.
  struct Compiled {
    rt::Graph<T> graph;
    // What this one was frozen for.  A member of the cached value rather than a
    // second member beside the cache: the two are set by one aggregate
    // initialiser, so there is no state in which they disagree.  It cannot be
    // read back off the graph -- a zero-arity equation compresses to no Hessian
    // columns at all, and would re-freeze on every call.
    bool has_hessian = false;
#ifdef DDX_HAS_JIT
    // Empty until freeze() fills it, and empty for good where T is not the
    // scalar the JIT emits.  The initialiser is what says so: freeze names only
    // `.graph`, and a member with nothing of its own to say would be a warning
    // rather than a default.
    jit::Kernel kernel{};
#endif
  };

  // The with-Hessian graph is a superset of the without, so one cache serves
  // both: it only ever grows, and asking for less than it holds is free.
  const Compiled &compiled(bool want_hessian) const {
    if (!compiled_ || (want_hessian && !compiled_->has_hessian)) {
      compiled_ = freeze(want_hessian);
    }
    return *compiled_;
  }

  Compiled freeze(bool want_hessian) const {
    rt::GraphBuilder<T> gb{*arena_};
    gb.values_from(roots_);
    gb.jacobian_from(derivative_.partial);
    if (want_hessian) {
      gb.hessian();
    }
    Compiled out{.graph = gb.build(), .has_hessian = want_hessian};
#ifdef DDX_HAS_JIT
    if constexpr (std::same_as<T, double>) {
      out.kernel = compiler().compile(out.graph);
    }
#endif
    return out;
  }

#ifdef DDX_HAS_JIT
  // One LLJIT per process.  A Kernel does not own its code, so the compiler has
  // to outlive every kernel it produced; a function-local static does that
  // without asking the caller to arrange it.
  static jit::Compiler &compiler() {
    static jit::Compiler instance;
    return instance;
  }
#endif

  // The interpreter, column by column: the fallback when there is no JIT, and
  // the reference the JIT is tested against.
  void interpret(const Compiled &c, std::span<const T *const> xs,
                 std::span<T *const> f, std::span<T *const> g,
                 std::span<T *const> h, std::size_t n) const {
    const auto blocks = c.graph.output_blocks();
    std::vector<T> at(arity());

    for (std::size_t i = 0; i < n; ++i) {
      for (const auto [j, column] : std::views::enumerate(xs)) {
        at[static_cast<std::size_t>(j)] = column[i];
      }
      const auto values = rt::evaluate_all(*arena_, at);

      // Each block against its own columns, so there is no counter walking one
      // flat output list that all three have to leave in the right place.
      const auto fill = [&](std::span<T *const> columns,
                            std::span<const rt::NodeId> block) {
        for (const auto [column, o] : std::views::zip(columns, block)) {
          column[i] = values[o];
        }
      };
      fill(f, blocks.values);
      fill(g, blocks.jacobian);
      fill(h, blocks.hessian);
    }
  }

  // Any contiguous range of columns reduces to the same span; the ABI below
  // wants pointers and a count, and this is the one place that conversion
  // happens.
  template <std::ranges::contiguous_range R>
  static auto as_columns(const R &r) {
    using Ptr = std::ranges::range_value_t<R>;
    return std::span<Ptr const>{std::ranges::data(r), std::ranges::size(r)};
  }

  static void expect(std::size_t got, std::size_t want, const char *what) {
    if (got != want) {
      throw std::invalid_argument(std::string("Equation: ") + what + " needs " +
                                  std::to_string(want) + " columns, got " +
                                  std::to_string(got));
    }
  }

  void dispatch(const Compiled &c, std::span<const T *const> xs,
                std::span<T *const> f, std::span<T *const> g,
                std::span<T *const> h, std::size_t n) const {
    const auto blocks = c.graph.output_blocks();
    expect(xs.size(), arity(), "the point");
    expect(f.size(), blocks.values.size(), "values");
    expect(g.size(), blocks.jacobian.size(), "the Jacobian");
    expect(h.size(), blocks.hessian.size(), "the Hessian");
    run(c, xs, f, g, h, n);
  }

  void run(const Compiled &c, std::span<const T *const> xs,
           std::span<T *const> f, std::span<T *const> g, std::span<T *const> h,
           std::size_t n) const {
#ifdef DDX_HAS_JIT
    if constexpr (std::same_as<T, double>) {
      c.kernel(xs, f, g, h, n);
      return;
    }
#endif
    interpret(c, xs, f, g, h, n);
  }

  // Maybe-owning: the deleter is the only thing that differs between an arena
  // the caller keeps and one this Equation was handed.  A raw pointer beside an
  // owning one would be two members obliged to agree, with nothing making them.
  using ArenaPtr = std::unique_ptr<rt::Builder<T>, void (*)(rt::Builder<T> *)>;
  static constexpr void borrow(rt::Builder<T> *) noexcept {}
  static constexpr void reclaim(rt::Builder<T> *b) noexcept { delete b; }

  ArenaPtr arena_{nullptr, borrow};
  std::vector<rt::NodeId> roots_;

  // Eager: one reverse sweep, microseconds, and having it in the constructor is
  // what keeps every per-point accessor const and constexpr.
  rt::Jacobian derivative_;
  // Lazy, because it is three orders of magnitude dearer: one sweep per colour
  // over a graph the sweeps keep growing, measured at ~1.2M nodes and 350 ms
  // for a 50-species mixture.  Same reason the compiled kernel is lazy.
  mutable std::vector<rt::Hessian> hessians_;
  mutable std::optional<Compiled> compiled_;
};

// Partial specialisations contribute no implicit deduction guides, and the one
// in expr/equation.hpp is CExpression-constrained, so this is the whole CTAD
// surface for a runtime equation.  It has to live here, in the class template's
// own namespace -- ddx::Equation is a using-declaration for this.
template <Numeric T, typename... Ts>
Equation(rt::RTExpression<T>, Ts...) -> Equation<rt::RTExpression<T>, Ts...>;

} // namespace ddx::impl

namespace ddx::rt {

// Build an Equation without naming an arena:
//
//   const auto eq = ddx::rt::equation([] {
//     const auto x = var("x");
//     const auto y = var("y");
//     return exp(x) * sin(y);
//   });
//
// The arena is created here and moved into the Equation, so it lives exactly as
// long as the thing using it -- which means the Equation can be returned and
// stored, where one built over a caller's Builder cannot.
//
// The callback takes nothing: equation() makes an arena current for its
// duration, so `var("x")` is the whole spelling and Builder never appears.
// Return a range of expressions instead of one and you get a system.
template <impl::Numeric T = double, std::invocable Assemble>
[[nodiscard]] auto equation(Assemble &&assemble) {
  auto arena = std::make_unique<Builder<T>>();
  auto built = [&] {
    const auto scope = detail::scoped_arena(*arena);
    return std::forward<Assemble>(assemble)();
  }();

  using Built = std::remove_cvref_t<decltype(built)>;
  if constexpr (std::same_as<Built, RTExpression<T>>) {
    return impl::Equation<RTExpression<T>>{std::move(arena), built};
  } else {
    // A fixed-size range of expressions is a system.  output_dim lives in the
    // type, so the size has to come from the type too -- which std::array has
    // and a vector does not.
    constexpr std::size_t outputs = std::tuple_size_v<Built>;
    static_assert(outputs > 0,
                  "equation: a system needs at least one function");
    return impl::index_apply<outputs - 1>([&]<std::size_t... Rest>() {
      return impl::Equation<RTExpression<T>, detail::Repeat<Rest, T>...>{
          std::move(arena), built[0], built[Rest + 1]...};
    });
  }
}

} // namespace ddx::rt

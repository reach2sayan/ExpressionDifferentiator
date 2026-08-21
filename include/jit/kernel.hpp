#pragma once

#include "expr/expressions.hpp" // ddx::impl::Numeric
#include "util/pinned.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

// The JIT's public surface.
namespace ddx::rt {
// The JIT emits machine types, so it compiles graphs over a machine scalar.
// A graph over anything else -- Numeric admits matrices and quaternions -- is
// interpretable but not compilable, which is why everything below names
// Graph<double> rather than the template.  The constraint has to match the
// definition, hence the one ddx include; it carries no dependency of its own.
template <impl::Numeric T> class Graph;
} // namespace ddx::rt

namespace ddx::jit {

// Which vector math library the loop vectoriser may call.
enum class VecLib : std::uint8_t { None, Auto, Libmvec };

#ifndef DDX_JIT_DEFAULT_OPT
#define DDX_JIT_DEFAULT_OPT 2
#endif
#ifndef DDX_JIT_DEFAULT_CONTRACT
#define DDX_JIT_DEFAULT_CONTRACT 1
#endif

inline constexpr unsigned default_opt_level = DDX_JIT_DEFAULT_OPT;
inline constexpr bool default_contract = DDX_JIT_DEFAULT_CONTRACT != 0;

struct Options {
  // Never 0: that disables the loop vectoriser
  unsigned opt_level = default_opt_level;
  VecLib veclib = VecLib::Auto;
  bool contract = default_contract; // Follows DDX_FP_FLAGS
};

// One compiled graph.  Cheap to copy
class Kernel {
public:
  using function_type = void (*)(const double *const *, double *const *,
                                 double *const *, double *const *, std::size_t);

  Kernel() = default;
  Kernel(function_type fn, std::size_t arity, std::size_t values,
         std::size_t jacobian, std::size_t hessian) noexcept
      : fn_(fn), arity_(arity), values_(values), jacobian_(jacobian),
        hessian_(hessian) {}

  // Spans on the C++ surface, raw pointers only in function_type, which is the
  // JIT's actual ABI: a block that was not requested is `{}` here rather than a
  // null pointer whose length the callee has to infer.
  //
  // xs[j] is the column for symbol j, g[j] the column for the partial in that
  // symbol, all of length n.  A kernel computes exactly the outputs its graph
  // was frozen with, so g is read only when outputs() > 1 -- freeze with the
  // value alone and it is never touched.
  // Each pointer is an array of columns, each column n long.  A block that was
  // not requested has no columns, and its pointer is never read.
  void operator()(std::span<const double *const> xs,
                  std::span<double *const> f, std::span<double *const> g,
                  std::span<double *const> h, std::size_t n) const {
    fn_(xs.data(), f.data(), g.data(), h.data(), n);
  }

  [[nodiscard]] explicit operator bool() const noexcept {
    return fn_ != nullptr;
  }
  [[nodiscard]] std::size_t arity() const noexcept { return arity_; }
  [[nodiscard]] std::size_t values() const noexcept { return values_; }
  [[nodiscard]] std::size_t jacobian_columns() const noexcept {
    return jacobian_;
  }
  [[nodiscard]] std::size_t hessian_columns() const noexcept {
    return hessian_;
  }
  [[nodiscard]] std::size_t outputs() const noexcept {
    return values_ + jacobian_ + hessian_;
  }

private:
  function_type fn_ = nullptr;
  std::size_t arity_ = 0;
  std::size_t values_ = 0;
  std::size_t jacobian_ = 0;
  std::size_t hessian_ = 0;
};

// One owner: a Compiler *is* the LLJIT, and the code a Kernel points at lives
// in it.  Movable, so it can be handed on or held in a static.
class Compiler : private impl::noncopyable {
public:
  Compiler();
  ~Compiler();
  Compiler(Compiler &&) noexcept;
  Compiler &operator=(Compiler &&) noexcept;

  [[nodiscard]] Kernel compile(const rt::Graph<double> &g,
                               const Options &opt = {});

private:
  friend class Ir;
  [[nodiscard]] std::string render_ir(const rt::Graph<double> &g,
                                      const Options &opt) const;

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// The optimised IR a graph would compile to.  A borrowing handle rather than a
// string, so that IR prints through the same formatter as everything else and
// the pipeline only runs if something actually reads it.
//
// It owns neither end and cannot: a Compiler is move-only because it *is* the
// LLJIT, so there is no sharing it, and copying a graph to print it would be
// backwards.  Both have to outlive the handle -- which the deleted overloads
// make the compiler check, rather than leaving it as a comment nobody reads.
// Name what you render; a temporary graph is a dangling one the moment the
// handle is stored.
class Ir {
public:
  Ir(const Compiler &c, const rt::Graph<double> &g, Options opt = {}) noexcept
      : compiler_(c), graph_(g), options_(opt) {}
  Ir(const Compiler &&, const rt::Graph<double> &, Options = {}) = delete;
  Ir(const Compiler &, const rt::Graph<double> &&, Options = {}) = delete;

  [[nodiscard]] std::string str() const {
    return compiler_.render_ir(graph_, options_);
  }

private:
  const Compiler &compiler_;
  const rt::Graph<double> &graph_;
  Options options_;
};

} // namespace ddx::jit

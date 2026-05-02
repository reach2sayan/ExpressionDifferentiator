#pragma once
#include "dual.hpp"
#include "equation.hpp"
#include "gradient.hpp"
#include <Eigen/Dense>
#include <boost/mp11/algorithm.hpp>
#include <ranges>

// Build a std::tuple of Jacobian rows — one row (std::tuple of derivatives)
// per expression in the std::tuple es.
template <typename... Syms, typename... Exprs>
constexpr auto make_jac_rows(const std::tuple<Exprs...> &es,
                             mp::mp_list<Syms...> = {}) {
  return std::apply(
      [](const auto &...exprs) {
        return std::tuple(make_derivatives(mp::mp_list<Syms...>{}, exprs)...);
      },
      es);
}

// ===========================================================================
// Equation<TFirst, TRest...> — f: ℝⁿ → ℝᵐ  (sizeof...(TRest) > 0).
//
// Compile-time path (input_dim > 0):
//   Symbols extracted from expression types; full Jacobian precomputed.
//   J[i][j] = ∂fᵢ/∂xⱼ  (row-major, output_dim × input_dim).
//
// Runtime path (input_dim == 0, RuntimeVariable nodes only):
//   n_inputs given at construction; reverse-mode Jacobian on demand.
// ===========================================================================
template <ExpressionConcept TFirst, ExpressionConcept... TRest>
  requires(
      sizeof...(TRest) > 0 &&
      (std::same_as<typename TFirst::value_type, typename TRest::value_type> &&
       ...))
class Equation<TFirst, TRest...> {
public:
  using value_type = typename TFirst::value_type;
  using symbols = sort_tuple_t<
      tuple_union_t<typename extract_symbols_from_expr<TFirst>::type,
                    typename extract_symbols_from_expr<TRest>::type...>>;

  static constexpr std::size_t output_dim = 1 + sizeof...(TRest);
  static constexpr std::size_t input_dim = mp::mp_size<symbols>::value;

private:
  using Exprs = std::tuple<TFirst, TRest...>;
  using jacobian_t = decltype(make_jac_rows(std::declval<Exprs>(), symbols{}));
  Exprs expressions;
  jacobian_t jacobian;
  std::size_t runtime_input_dim_{input_dim};

  friend std::ostream &operator<<(std::ostream &out, const Equation &ve) {
    static_for<output_dim>([&]<std::size_t I>() {
      out << "f" << I << ": " << std::get<I>(ve.expressions);
      if constexpr (input_dim > 0) {
        out << " grad: ";
        print_tup(out, std::get<I>(ve.jacobian));
      }
      out << '\n';
    });
    return out;
  }

public:
  // Compile-time constructor: symbols extracted from expression types.
  constexpr Equation(TFirst first, TRest... rest)
      : expressions{first, rest...},
        jacobian{make_jac_rows(expressions, symbols{})},
        runtime_input_dim_{input_dim} {}

  // Runtime constructor: no compile-time symbols; n_inputs given at
  // construction.
  Equation(std::size_t n_inputs, TFirst first, TRest... rest)
    requires(input_dim == 0)
      : expressions{first, rest...}, jacobian{}, runtime_input_dim_{n_inputs} {}

  // Evaluate all components — returns std::array<value_type, output_dim>.
  [[nodiscard]] constexpr auto eval() const {
    return std::apply(detail::eval_func, expressions);
  }

  // Full symbolic Jacobian — compile-time path only (output_dim × input_dim).
  [[nodiscard]] auto eval_jacobian() const
    requires(input_dim > 0)
  {
    Eigen::Matrix<value_type, output_dim, input_dim> J;
    static_for<output_dim>([&]<std::size_t I>() {
      auto row = std::apply(detail::eval_func, std::get<I>(jacobian));
      for (auto j : std::views::iota(std::size_t{0}, input_dim)) {
        J(I, j) = row[j];
      }
    });
    return J;
  }

  // Reverse-mode Jacobian — compile-time path (fixed-size output_dim ×
  // input_dim). Returns {f_vals, J}.
  [[nodiscard]] auto eval_jacobian_reverse() const
    requires(input_dim > 0)
  {
    auto f_vals = eval();
    Eigen::Matrix<value_type, output_dim, input_dim> J;
    static_for<output_dim>([&]<std::size_t I>() {
      std::array<value_type, input_dim> row{};
      std::get<I>(expressions).backward(symbols{}, value_type{1}, row);
      for (auto j : std::views::iota(std::size_t{0}, input_dim)) {
        J(I, j) = row[j];
      }
    });
    return std::pair{f_vals, J};
  }

  // Reverse-mode Jacobian — runtime path (output_dim × dynamic-width matrix).
  // Returns {f_vals, J}.
  [[nodiscard]] auto eval_jacobian_reverse() const
    requires(input_dim == 0)
  {
    auto f_vals = eval();
    Eigen::Matrix<value_type, output_dim, Eigen::Dynamic> J(output_dim,
                                                            runtime_input_dim_);
    J.setZero();
    static_for<output_dim>([&]<std::size_t I>() {
      Eigen::VectorX<value_type> row =
          Eigen::VectorX<value_type>::Zero(runtime_input_dim_);
      std::get<I>(expressions).backward(mp::mp_list<>{}, value_type{1}, row);
      J.row(I) = std::move(row);
    });
    return std::pair{f_vals, J};
  }

  // Reverse-mode — compile-time path, with point update.
  [[nodiscard]] auto
  eval_jacobian_reverse(Eigen::Vector<value_type, input_dim> values)
    requires(input_dim > 0)
  {
    update(symbols{}, values);
    return eval_jacobian_reverse();
  }

  // Reverse-mode — runtime path, with point update.
  [[nodiscard]] auto eval_jacobian_reverse(Eigen::VectorX<value_type> values)
    requires(input_dim == 0)
  {
    update(values);
    return eval_jacobian_reverse();
  }

  // Forward-mode Jacobian: one seeded pass per input variable.
  // Evaluates at the values currently stored in the expression variables.
  // Returns {f_vals, J} where f_vals[i] = fᵢ and J[i][j] = ∂fᵢ/∂xⱼ.
  [[nodiscard]] auto eval_jacobian_forward()
    requires is_dual_v<value_type> && (input_dim > 0)
  {
    using S = dual_scalar_t<value_type>;
    Eigen::Matrix<S, output_dim, input_dim> J;
    Eigen::Vector<value_type, input_dim> seeds;

    std::array<value_type, input_dim> current{};
    std::apply(
        [&](const auto &...exprs) { (exprs.collect(symbols{}, current), ...); },
        expressions);

    for (auto j : std::views::iota(std::size_t{0}, input_dim)) {
      for (auto i : std::views::iota(std::size_t{0}, input_dim)) {
        seeds[i] =
            value_type{current[i].template get<0>(), i == j ? S{1} : S{}};
      }
      update(symbols{}, seeds);
      auto vals = eval();
      for (auto i : std::views::iota(std::size_t{0}, output_dim))
        J(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)) =
            vals[i].template get<1>();
    }

    for (auto i : std::views::iota(std::size_t{0}, input_dim)) {
      seeds[i] = value_type{current[i].template get<0>(), S{}};
    }
    update(symbols{}, seeds);
    auto dual_vals = eval();
    std::array<S, output_dim> f_vals;
    for (auto i : std::views::iota(std::size_t{0}, output_dim))
      f_vals[i] = dual_vals[i].template get<0>();

    return std::pair{f_vals, J};
  }

  // Forward-mode — with point update. Seeds variables to values (zero tangent)
  // then delegates to the no-arg overload.
  [[nodiscard]] auto eval_jacobian_forward(
      Eigen::Vector<dual_scalar_t<value_type>, input_dim> values)
    requires is_dual_v<value_type> && (input_dim > 0)
  {
    using S = dual_scalar_t<value_type>;
    Eigen::Vector<value_type, input_dim> seeds;
    for (std::size_t i = 0; i < input_dim; ++i) {
      seeds[i] = value_type{values[i], S{}};
    }
    update(symbols{}, seeds);
    return eval_jacobian_forward();
  }

  // Forward-over-reverse Hessian: n backward passes with dual-seeded variables.
  // Requires value_type = Dual<S>. Returns {f_vals, H} where H[k] is the
  // input_dim × input_dim Hessian matrix of fₖ.
  [[nodiscard]] auto eval_hessian()
    requires is_dual_v<value_type> && (input_dim > 0)
  {
    using S = dual_scalar_t<value_type>;
    std::array<Eigen::Matrix<S, input_dim, input_dim>, output_dim> H;
    for (auto &h : H)
      h.setZero();

    std::array<value_type, input_dim> current{};
    std::apply(
        [&](const auto &...exprs) { (exprs.collect(symbols{}, current), ...); },
        expressions);

    Eigen::Vector<value_type, input_dim> seeds;
    for (auto j : std::views::iota(std::size_t{0}, input_dim)) {
      for (auto i : std::views::iota(std::size_t{0}, input_dim)) {
        seeds[i] =
            value_type{current[i].template get<0>(), i == j ? S{1} : S{}};
      }
      update(symbols{}, seeds);
      static_for<output_dim>([&]<std::size_t K>() {
        std::array<value_type, input_dim> grads{};
        std::get<K>(expressions).backward(symbols{}, value_type{1}, grads);
        for (auto i : std::views::iota(std::size_t{0}, input_dim)) {
          H[K](static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)) =
              grads[i].template get<1>();
        }
      });
    }

    for (auto i : std::views::iota(std::size_t{0}, input_dim)) {
      seeds[i] = value_type{current[i].template get<0>(), S{}};
    }
    update(symbols{}, seeds);
    auto dual_vals = eval();
    std::array<S, output_dim> f_vals;
    for (auto i : std::views::iota(std::size_t{0}, output_dim)) {
      f_vals[i] = dual_vals[i].template get<0>();
    }
    return std::pair{f_vals, H};
  }

  // Forward-over-reverse Hessian — with point update.
  [[nodiscard]] auto
  eval_hessian(Eigen::Vector<dual_scalar_t<value_type>, input_dim> values)
    requires is_dual_v<value_type> && (input_dim > 0)
  {
    using S = dual_scalar_t<value_type>;
    Eigen::Vector<value_type, input_dim> seeds;
    for (std::size_t i = 0; i < input_dim; ++i)
      seeds[i] = value_type{values[i], S{}};
    update(symbols{}, seeds);
    return eval_hessian();
  }

  // Forward-over-forward Hessian via nested Dual<Dual<S>> variables.
  // Requires value_type = Dual<Dual<S>>. n² seeded evaluations.
  // Returns {f_vals, H} where H[k](i,j) = ∂²fₖ/∂xᵢ∂xⱼ.
  [[nodiscard]] auto eval_hessian_forward()
    requires is_dual_v<value_type> && is_dual_v<dual_scalar_t<value_type>> &&
             (input_dim > 0)
  {
    using D = dual_scalar_t<value_type>; // Dual<S>
    using S = dual_scalar_t<D>;          // base scalar

    std::array<Eigen::Matrix<S, input_dim, input_dim>, output_dim> H;

    std::array<value_type, input_dim> current{};
    std::apply(
        [&](const auto &...exprs) { (exprs.collect(symbols{}, current), ...); },
        expressions);

    Eigen::Vector<value_type, input_dim> seeds;
    for (auto i : std::views::iota(std::size_t{0}, input_dim)) {
      for (auto j : std::views::iota(std::size_t{0}, input_dim)) {
        for (auto k : std::views::iota(std::size_t{0}, input_dim)) {
          S real_k = current[k].template get<0>().template get<0>();
          seeds[k] = value_type{D{real_k, k == j ? S{1} : S{}},
                                D{k == i ? S{1} : S{}, S{}}};
        }
        update(symbols{}, seeds);
        auto vals = eval();
        static_for<output_dim>([&]<std::size_t K>() {
          H[K](static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)) =
              vals[K].template get<1>().template get<1>();
        });
      }
    }

    for (auto k : std::views::iota(std::size_t{0}, input_dim)) {
      S real_k = current[k].template get<0>().template get<0>();
      seeds[k] = value_type{D{real_k, S{}}, D{}};
    }
    update(symbols{}, seeds);
    auto dual_vals = eval();
    std::array<S, output_dim> f_vals;
    for (auto k : std::views::iota(std::size_t{0}, output_dim)) {
      f_vals[k] = dual_vals[k].template get<0>().template get<0>();
    }
    return std::pair{f_vals, H};
  }

  // Forward-over-forward Hessian — with point update.
  [[nodiscard]] auto eval_hessian_forward(
      Eigen::Vector<dual_scalar_t<dual_scalar_t<value_type>>, input_dim> values)
    requires is_dual_v<value_type> && is_dual_v<dual_scalar_t<value_type>> &&
             (input_dim > 0)
  {
    using D = dual_scalar_t<value_type>;
    using S = dual_scalar_t<D>;
    Eigen::Vector<value_type, input_dim> seeds;
    for (auto i : std::views::iota(0u, input_dim)) {
      seeds[i] = value_type{D{values[i], S{}}, D{}};
    }
    update(symbols{}, seeds);
    return eval_hessian_forward();
  }

  // Update compile-time variables in all expressions and Jacobian rows.
  constexpr void update(const symbols &syms, const auto &updates) {
    auto update_func = detail::update_func_t{syms, updates};
    auto apply_to_tuple_func = [&](auto &...jac_rows) {
      (std::apply(update_func, jac_rows), ...);
    };
    std::apply(update_func, expressions);
    std::apply(apply_to_tuple_func, jacobian);
  }

  // Update runtime variables — convenience overload (no symbol list needed).
  void update(const auto &values)
    requires(input_dim == 0)
  {
    auto update_func = detail::update_func_t{mp::mp_list<>{}, values};
    std::apply(update_func, expressions);
  }
};

template <ExpressionConcept T, ExpressionConcept... Ts>
Equation(T, Ts...) -> Equation<T, Ts...>;

template <ExpressionConcept T, ExpressionConcept... Ts>
Equation(std::size_t, T, Ts...) -> Equation<T, Ts...>;

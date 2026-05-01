#pragma once
#include "dual.hpp"
#include "equation.hpp"
#include "gradient.hpp"

#include <Eigen/Dense>
#include <boost/mp11/algorithm.hpp>

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
      for (std::size_t j = 0; j < input_dim; ++j)
        J(I, j) = row[j];
    });
    return J;
  }

  // Reverse-mode Jacobian — compile-time path (fixed-size output_dim ×
  // input_dim).
  [[nodiscard]] auto eval_jacobian_reverse() const
    requires(input_dim > 0)
  {
    Eigen::Matrix<value_type, output_dim, input_dim> J;
    static_for<output_dim>([&]<std::size_t I>() {
      std::array<value_type, input_dim> row{};
      std::get<I>(expressions).backward(symbols{}, value_type{1}, row);
      for (std::size_t j = 0; j < input_dim; ++j) {
        J(I, j) = row[j];
      }
    });
    return J;
  }

  // Reverse-mode Jacobian — runtime path (output_dim × dynamic-width matrix).
  [[nodiscard]] auto eval_jacobian_reverse() const
    requires(input_dim == 0)
  {
    Eigen::Matrix<value_type, output_dim, Eigen::Dynamic> J(output_dim,
                                                            runtime_input_dim_);
    J.setZero();
    static_for<output_dim>([&]<std::size_t I>() {
      Eigen::VectorX<value_type> row =
          Eigen::VectorX<value_type>::Zero(runtime_input_dim_);
      std::get<I>(expressions).backward(mp::mp_list<>{}, value_type{1}, row);
      J.row(I) = std::move(row);
    });
    return J;
  }

  // Forward-mode Jacobian: one seeded pass per input variable.
  // Only available when value_type = Dual<S> and input_dim > 0.
  // values: evaluation point as plain scalars (ordered by the sorted symbol
  // list). Returns J[i][j] = ∂fᵢ/∂xⱼ  (output_dim × input_dim).
  [[nodiscard]] auto eval_jacobian_forward(
      Eigen::Vector<dual_scalar_t<value_type>, input_dim> values)
    requires is_dual_v<value_type> && (input_dim > 0)
  {
    using S = dual_scalar_t<value_type>;
    Eigen::Matrix<S, output_dim, input_dim> J;
    Eigen::Vector<value_type, input_dim> seeds;

    for (std::size_t j = 0; j < input_dim; ++j) {
      for (std::size_t i = 0; i < input_dim; ++i)
        seeds[i] = value_type{values[i], i == j ? S{1} : S{}};
      update(symbols{}, seeds);
      auto vals = eval();
      for (std::size_t i = 0; i < output_dim; ++i)
        J(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)) =
            vals[i].template get<1>();
    }
    for (std::size_t i = 0; i < input_dim; ++i)
      seeds[i] = value_type{values[i], S{}};
    update(symbols{}, seeds);
    return J;
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

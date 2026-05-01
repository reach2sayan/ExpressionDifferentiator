#pragma once
#include "dual.hpp"
#include "expressions.hpp"
#include "gradient.hpp"
#include "operations.hpp"
#include "traits.hpp"

#include <Eigen/Dense>
#include <boost/mp11/algorithm.hpp>
#include <concepts>
#include <tuple>
namespace mp = boost::mp11;

namespace detail {
struct eval_func_t {
  constexpr auto operator()(const auto &...exprs) const {
    return std::array{exprs.eval()...};
  }
};
template <class Syms, class Updates> struct update_func_t {
  const Syms &syms;
  const Updates &updates;
  constexpr auto operator()(auto &...ds) const {
    (ds.update(syms, updates), ...);
  }
};

inline constexpr eval_func_t eval_func{};
} // namespace detail

// Pretty-print a std::tuple of expressions, one per line.
template <typename... Ts>
constexpr std::ostream &print_tup(std::ostream &out,
                                  const std::tuple<Ts...> &tup) {
  out << "(\n";
  bool first = true;

  static_for<sizeof...(Ts)>([&]<std::size_t I>() {
    if (!first) {
      out << "\n";
    }
    out << std::get<I>(tup);
    first = false;
  });
  out << "\n)";
  return out;
}

// Build a std::tuple of partial derivatives — one per symbol in the mp_list.
// Each element type is make_all_constant_except<C>(expr).derivative().
template <typename... Syms, ExpressionConcept Expr>
constexpr auto make_derivatives(mp::mp_list<Syms...>, const Expr &expr) {
  return std::make_tuple(
      make_all_constant_except<Syms::value>(expr).derivative()...);
}

template <typename T>
concept EquationConcept = ExpressionConcept<T> and std::constructible_from<T>;

template <ExpressionConcept... Ts>
class Equation;

template <ExpressionConcept TExpression>
class Equation<TExpression> {
  TExpression expression;

public:
  using symbols = typename extract_symbols_from_expr<TExpression>::type;
  using derivatives_t = decltype(make_derivatives(symbols{}, expression));

private:
  derivatives_t derivatives;
  friend std::ostream &operator<<(std::ostream &out, const Equation &e) {
    out << "Equation\n" << e.expression << "\nDerivatives\n";
    print_tup(out, e.derivatives);
    return out;
  }

public:
  using value_type = typename TExpression::value_type;
  constexpr static std::size_t number_of_derivatives =
      std::tuple_size_v<derivatives_t>;
  constexpr operator value_type() const { return expression.eval(); }

  template <std::size_t N>
  constexpr decltype(auto) operator[](std::integral_constant<std::size_t, N>) {
    if constexpr (N == 0) {
      return (expression);
    } else {
      return std::get<N - 1>(derivatives);
    }
  }
  template <std::size_t N>
  constexpr decltype(auto)
  operator[](std::integral_constant<std::size_t, N>) const {
    if constexpr (N == 0) {
      return (expression);
    } else {
      return std::get<N - 1>(derivatives);
    }
  }

  constexpr void update(const symbols &syms, const auto &updates) {
    expression.update(syms, updates);
    auto update_func = detail::update_func_t{syms, updates};
    std::apply(update_func, derivatives);
  }

  [[nodiscard]] constexpr auto eval() const { return expression.eval(); }
  [[nodiscard]] constexpr auto eval_derivatives() const {
    return std::apply(detail::eval_func, derivatives);
  }

  constexpr Equation(const TExpression &e)
      : expression{e}, derivatives{make_derivatives(symbols{}, e)} {}
};

template <ExpressionConcept T> Equation(T &&) -> Equation<std::decay_t<T>>;

// Build a std::tuple of Jacobian rows — one row (std::tuple of derivatives)
// per expression in the std::tuple es.
template <typename... Syms, typename... Exprs>
constexpr auto make_jac_rows(const std::tuple<Exprs...> &es,
                             mp::mp_list<Syms...> = {}) {
  return std::apply(
      [](const auto &...exprs) {
        return std::make_tuple(
            make_derivatives(mp::mp_list<Syms...>{}, exprs)...);
      },
      es);
}

// ===========================================================================
// Equation<TFirst, TRest...> — f: ℝⁿ → ℝᵐ  (sizeof...(TRest) > 0).
// Holds one expression per output component and precomputes the full Jacobian.
// All components must share the same value_type.
// J[i][j] = ∂fᵢ/∂xⱼ  (row-major, output_dim × input_dim).
// ===========================================================================
template <ExpressionConcept TFirst, ExpressionConcept... TRest>
  requires(sizeof...(TRest) > 0 &&
           (std::same_as<typename TFirst::value_type,
                         typename TRest::value_type> &&
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

  friend std::ostream &operator<<(std::ostream &out, const Equation &ve) {
    static_for<output_dim>([&]<std::size_t I>() {
      out << "f" << I << ": " << std::get<I>(ve.expressions) << " grad: ";
      print_tup(out, std::get<I>(ve.jacobian));
      out << '\n';
    });
    return out;
  }

public:
  constexpr Equation(TFirst first, TRest... rest)
      : expressions{first, rest...},
        jacobian{make_jac_rows(expressions, symbols{})} {}

  // Evaluate all components — returns std::array<value_type, output_dim>.
  [[nodiscard]] constexpr auto eval() const {
    return std::apply(detail::eval_func, expressions);
  }

  using jacobian_matrix_t =
      Eigen::Matrix<value_type, output_dim, input_dim>;

  // Full Jacobian — returns (output_dim × input_dim) Eigen matrix.
  [[nodiscard]] auto eval_jacobian() const {
    jacobian_matrix_t J;
    static_for<output_dim>([&]<std::size_t I>() {
      auto row = std::apply(detail::eval_func, std::get<I>(jacobian));
      for (std::size_t j = 0; j < input_dim; ++j)
        J(I, j) = row[j];
    });
    return J;
  }

  // Reverse-mode Jacobian: one reverse-mode gradient pass per output component.
  [[nodiscard]] auto eval_jacobian_reverse() const {
    jacobian_matrix_t J;
    static_for<output_dim>([&]<std::size_t I>() {
      std::array<value_type, input_dim> row{};
      std::get<I>(expressions).backward(symbols{}, value_type{1}, row);
      for (std::size_t j = 0; j < input_dim; ++j)
        J(I, j) = row[j];
    });
    return J;
  }

  // Forward-mode Jacobian: one seeded pass per input variable.
  // Only available when value_type = Dual<S>.
  // values: evaluation point as plain scalars (one per input variable, ordered
  //         by the sorted symbol list).
  // Returns J[i][j] = ∂fᵢ/∂xⱼ  (output_dim × input_dim).
  [[nodiscard]] auto
  eval_jacobian_forward(Eigen::Vector<dual_scalar_t<value_type>, input_dim> values)
    requires is_dual_v<value_type>
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

  // Update live variables in all expressions and Jacobian rows.
  constexpr void update(const symbols &syms, const auto &updates) {
    auto update_func = detail::update_func_t{syms, updates};
    auto apply_to_tuple_func = [&](auto &...jac_rows) {
      (std::apply(update_func, jac_rows), ...);
    };
    std::apply(update_func, expressions);
    std::apply(apply_to_tuple_func, jacobian);
  }
};

template <ExpressionConcept T, ExpressionConcept... Ts>
Equation(T, Ts...) -> Equation<T, Ts...>;

#pragma once
#include "expressions.hpp"
#include "operations.hpp"
#include "traits.hpp"
#include <boost/hana.hpp>
#include <concepts>

// Pretty-print a hana::tuple of expressions, one per line.
template <typename... Ts>
constexpr std::ostream &print_tup(std::ostream &out,
                                  const boost::hana::tuple<Ts...> &tup) {
  out << "(\n";
  bool first = true;
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (..., ([&] {
       if (!first)
         out << "\n";
       out << boost::hana::at_c<Is>(tup);
       first = false;
     }()));
  }(std::make_index_sequence<sizeof...(Ts)>{});
  out << "\n)";
  return out;
}

// Build a hana::tuple of partial derivatives — one per symbol.
// hana::transform gives a heterogeneous tuple where each element has the
// type of make_all_constant_except<C>(expr).derivative().
constexpr auto make_derivatives(const auto &symbols,
                                const ExpressionConcept auto &expr) {
  return boost::hana::transform(symbols, [&expr](auto sym) {
    constexpr char C = std::decay_t<decltype(sym)>::value;
    return make_all_constant_except<C>(expr).derivative();
  });
}

template <typename T>
concept EquationConcept = ExpressionConcept<T> and std::constructible_from<T>;

template <ExpressionConcept TExpression> class Equation {
  TExpression expression;

public:
  using symbols = typename extract_symbols_from_expr<TExpression>::type;
  using derivatives_t =
      decltype(make_derivatives(std::declval<symbols>(), expression));

private:
  derivatives_t derivatives;
  friend std::ostream &operator<<(std::ostream &out, const Equation &e) {
    out << "Equation\n" << e.expression << "\nDerivatives\n";
    print_tup(out, e.derivatives);
    return out;
  }

public:
  using value_type = typename TExpression::value_type;
  constexpr static size_t number_of_derivatives =
      decltype(boost::hana::size(std::declval<derivatives_t>()))::value;
  constexpr operator value_type() const { return expression.eval(); }

  // operator[IDX(0)] returns the expression itself; IDX(k) returns the k-th
  // partial derivative (1-based).
  template <size_t N>
  constexpr decltype(auto) operator[](std::integral_constant<size_t, N>) {
    if constexpr (N == 0)
      return (expression);
    else
      return boost::hana::at_c<N - 1>(derivatives);
  }
  template <size_t N>
  constexpr decltype(auto) operator[](std::integral_constant<size_t, N>) const {
    if constexpr (N == 0)
      return (expression);
    else
      return boost::hana::at_c<N - 1>(derivatives);
  }

  // symbols must be exactly the equation's own symbol list — enforced by type.
  constexpr void update(const symbols &symbols, const auto &updates) {
    expression.update(symbols, updates);
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      (boost::hana::at_c<Is>(derivatives).update(symbols, updates), ...);
    }(std::make_index_sequence<number_of_derivatives>{});
  }

  [[nodiscard]] constexpr auto eval() const { return expression.eval(); }
  [[nodiscard]] constexpr auto eval_derivatives() const {
    return boost::hana::unpack(
        boost::hana::transform(derivatives,
                               [](const auto &d) { return d.eval(); }),
        [](auto... vs) { return std::array{vs...}; });
  }
  constexpr Equation(const TExpression &e)
      : expression{e}, derivatives{make_derivatives(symbols{}, e)} {}
};

template <ExpressionConcept T> Equation(T &&) -> Equation<std::decay_t<T>>;

template <typename SymsList, typename Exprs>
constexpr auto make_jac_rows(const Exprs &es, SymsList symbols = {}) {
  return boost::hana::transform(es, [&](const auto &e) {
    return make_derivatives(std::move(symbols), e);
  });
}

// ===========================================================================
// VectorEquation — f: ℝⁿ → ℝᵐ.
// Holds one expression per output component and precomputes the full Jacobian.
// All components must share the same value_type.
// J[i][j] = ∂fᵢ/∂xⱼ  (row-major, output_dim × input_dim).
// ===========================================================================
template <ExpressionConcept TFirst, ExpressionConcept... TRest>
  requires(
      std::same_as<typename TFirst::value_type, typename TRest::value_type> &&
      ...)
class VectorEquation {
public:
  using value_type = typename TFirst::value_type;
  using symbols = sort_tuple_t<
      tuple_union_t<typename extract_symbols_from_expr<TFirst>::type,
                    typename extract_symbols_from_expr<TRest>::type...>>;

  static constexpr std::size_t output_dim = 1 + sizeof...(TRest);
  static constexpr std::size_t input_dim =
      decltype(boost::hana::size(std::declval<symbols>()))::value;

private:
  using Exprs = boost::hana::tuple<TFirst, TRest...>;
  using jacobian_t = decltype(make_jac_rows<symbols>(std::declval<Exprs>()));
  Exprs expressions;
  jacobian_t jacobian;

  friend std::ostream &operator<<(std::ostream &out, const VectorEquation &ve) {
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      (..., ([&] {
         out << "f" << Is << ": " << boost::hana::at_c<Is>(ve.expressions)
             << "grad: ";
         print_tup(out, boost::hana::at_c<Is>(ve.jacobian));
         out << '\n';
       }()));
    }(std::make_index_sequence<output_dim>{});
    return out;
  }

public:
  constexpr VectorEquation(TFirst first, TRest... rest)
      : expressions{first, rest...},
        jacobian{make_jac_rows(expressions, symbols{})} {}

  // Evaluate all components — returns std::array<value_type, output_dim>.
  [[nodiscard]] constexpr auto eval() const {
    return boost::hana::unpack(
        boost::hana::transform(
            expressions, [](const auto &e) -> value_type { return e.eval(); }),
        [](auto... vs) { return std::array<value_type, output_dim>{vs...}; });
  }

  // Full Jacobian — returns std::array<std::array<value_type, input_dim>,
  // output_dim>.
  [[nodiscard]] constexpr auto eval_jacobian() const {
    auto rows = boost::hana::transform(jacobian, [](const auto &row) {
      return boost::hana::unpack(
          boost::hana::transform(row, [](const auto &d) { return d.eval(); }),
          [](auto... vs) { return std::array{vs...}; });
    });
    // Explicit Row type to prevent C++20 aggregate CTAD from deducing through
    // the inner array's elements instead of treating it as a single element.
    return boost::hana::unpack(rows, []<typename... Rows>(Rows... rs) {
      using Row = std::common_type_t<std::decay_t<Rows>...>;
      return std::array<Row, sizeof...(Rows)>{rs...};
    });
  }

  // Update live variables in all expressions and Jacobian rows.
  constexpr void update(const symbols &symbols, const auto &updates) {
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      (boost::hana::at_c<Is>(expressions).update(symbols, updates), ...);
    }(std::make_index_sequence<output_dim>{});
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      (..., ([&] {
         auto &row = boost::hana::at_c<Is>(jacobian);
         [&]<std::size_t... Js>(std::index_sequence<Js...>) {
           (boost::hana::at_c<Js>(row).update(symbols, updates), ...);
         }(std::make_index_sequence<input_dim>{});
       }()));
    }(std::make_index_sequence<output_dim>{});
  }
};

template <ExpressionConcept T, ExpressionConcept... Ts>
VectorEquation(T, Ts...) -> VectorEquation<T, Ts...>;

#pragma once
#include "expressions.hpp"
#include "traits.hpp"

#include <boost/mp11/algorithm.hpp>
#include <concepts>
#include <tuple>

namespace diff {

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
  return std::tuple(
      make_all_constant_except<Syms::value>(expr).derivative()...);
}

template <typename T>
concept EquationConcept = ExpressionConcept<T> and std::constructible_from<T>;
template <ExpressionConcept... Ts> class Equation;

template <ExpressionConcept TExpression> class Equation<TExpression> {
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

  [[nodiscard]] constexpr auto evaluate() const { return expression.eval(); }
  [[nodiscard]] constexpr auto eval_derivatives() const {
    return std::apply(detail::eval_func, derivatives);
  }

  constexpr Equation(const TExpression &e)
      : expression{e}, derivatives{make_derivatives(symbols{}, e)} {}
};

template <ExpressionConcept T> Equation(T &&) -> Equation<std::decay_t<T>>;

} // namespace diff

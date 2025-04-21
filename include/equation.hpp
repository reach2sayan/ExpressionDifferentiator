//
// Created by sayan on 4/17/25.
//

#pragma once
#include "expressions.hpp"
#include "operations.hpp"
#include "traits.hpp"
#include <array>
#include <strings.h>
#include <type_traits>

template <class... T>
constexpr inline std::ostream &print_tup(std::ostream &out,
                                         const std::tuple<T...> &_tup) {
  auto print_tup_helper = []<class TupType, size_t... I>(
                              std::ostream &out, const TupType &_tup,
                              std::index_sequence<I...>) -> std::ostream & {
    out << "(";
    (..., (out << (I == 0 ? "" : ", ") << std::get<I>(_tup)));
    out << ")\n";
    return out;
  };
  return print_tup_helper(out, _tup, std::make_index_sequence<sizeof...(T)>());
}

template <typename... Chars, typename Op, typename LHS, typename RHS>
constexpr auto make_derivatives(const std::tuple<Chars...> &labels,
                                const Expression<Op, LHS, RHS> &expr) {

  auto make_derivatives_impl = []<typename Tuple, typename _Op, typename _LHS,
                                  typename _RHS, std::size_t... Is>(
                                   const Tuple &,
                                   const Expression<_Op, _LHS, _RHS> &expr,
                                   std::index_sequence<Is...>) {
    return std::make_tuple(
        make_all_constant_except<std::tuple_element_t<Is, Tuple>::value>(expr)
            .derivative()...);
  };
  return make_derivatives_impl(labels, expr,
                               std::index_sequence_for<Chars...>{});
}

template <typename TExpression> class Equation;

struct TupleSupport {};

template <typename TExpression> class Equation {
private:
  TExpression expression;

public:
  using symbolslist = typename extract_symbols_from_expr<TExpression>::type;
  using derivatives_t =
      decltype(make_derivatives(std::declval<symbolslist>(), expression));

private:
  derivatives_t derivatives;

  friend std::ostream &operator<<(std::ostream &out, const Equation &e) {
    out << "Equation\n"
        << e.get_expression() << "\n"
        << "Derivatives\n";
    print_tup(out, e.get_derivatives_helper());
    return out;
  }
  constexpr const auto &get_expression() const { return expression; }
  constexpr auto &get_expression() { return expression; }
  constexpr const auto &get_derivatives_helper() const { return derivatives; }

public:
  using value_type = typename TExpression::value_type;
  constexpr static size_t number_of_derivatives =
      std::tuple_size_v<derivatives_t>;
  constexpr operator value_type() const { return expression; }
  template <size_t N>
  constexpr decltype(auto) operator[](std::integral_constant<size_t, N>) {
    if constexpr (N == 0) {
      return get_expression();
    } else {
      return std::get<N - 1>(derivatives);
    }
  }
  constexpr Equation(const TExpression &e)
      : expression{e}, derivatives{make_derivatives(symbolslist{}, e)} {}
};

template <typename T> Equation(T &&) -> Equation<std::decay_t<T>>;

//
// Created by sayan on 4/17/25.
//

#pragma once
#include "expressions.hpp"
#include "operations.hpp"
#include "traits.hpp"
#include <ranges>
#include <array>
#include <type_traits>

template <typename TExpression>
constexpr auto construct_derivatives(const TExpression &e);

template <typename TExpression>
constexpr auto collect_variable_labels(const TExpression &expression);

template <typename TExpression> class Equation {
public:
  constexpr static size_t var_count = std::decay_t<TExpression>::var_count;
private:
  TExpression expression;
  //std::array<TExpression, var_count> derivatives;
public:
  using value_type = typename TExpression::value_type;

  constexpr operator value_type() const { return expression; }
  constexpr const TExpression &get_expression() const { return expression; }

  template <typename TTExpression>
  constexpr Equation(TTExpression &&e)
      : expression{std::forward<TTExpression>(e)} {}
      //  derivatives{construct_derivatives(e)} {}
};

template <typename T> Equation(T &&) -> Equation<std::decay_t<T>>;

/*
template <typename TExpression>
constexpr auto construct_derivatives(const TExpression &e) {
  auto labels = collect_variable_labels(e);
  auto derivatives = make_filled_array<std::decay_t<TExpression>, labels.size()>(e);
  size_t i = 0;
  for (auto label : labels) {
    derivatives[i++] = make_all_constant_except<label>(e);
  }
  return derivatives;
}*/

template <typename TExpression>
constexpr auto collect_variable_labels(const TExpression &expression) {
  constexpr std::size_t N = TExpression::var_count;
  std::array<char, N> result{};
  std::size_t index = 0;
  make_labels_array(expression, result, index);
  return result;
}

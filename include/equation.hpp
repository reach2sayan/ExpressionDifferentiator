//
// Created by sayan on 4/17/25.
//

#pragma once
#include "operations.hpp"
#include "expressions.hpp"
#include "traits.hpp"

template <typename TExpression> class Equation {
  TExpression expression;
  friend constexpr auto collect_vars(const TExpression& expr);
  //std::array<TExpression, NUMVAR> derivatives;
public:
  using value_type = typename TExpression::value_type;
  constexpr static size_t NUMVAR = TExpression::var_count;
  constexpr operator value_type() const { return expression; }
  constexpr const TExpression& get_expression() const { return expression; }
  constexpr Equation(TExpression e) : expression{std::move(e)} {}
};

template <typename TExpression>
constexpr auto construct_derivatives(const TExpression &e) {
  constexpr auto numvar = TExpression::var_count;
  for (size_t i = 0; i < numvar; ++i) {
  }
}

template <typename TExpression>
constexpr auto collect_vars(const TExpression &expression) {
  constexpr std::size_t N = TExpression::var_count;
  std::array<char, N> result{};
  std::size_t index = 0;
  make_labels_array(expression, result, index);
  return result;
}

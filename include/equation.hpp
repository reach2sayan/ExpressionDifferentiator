//
// Created by sayan on 4/17/25.
//

#pragma once

#include "operations.hpp"
#include "values.hpp"
#include "expressions.hpp"

template<typename TExpression>
class Equation {
  using value_type = typename TExpression::value_type;
  //constexpr static size_t NUMVAR = TExpression::var_count;
  TExpression expression;
  //std::array<TExpression, NUMVAR> derivatives;
public:
  constexpr operator value_type() const {
    return expression;
  }
  constexpr Equation(TExpression e) : expression{std::move(e)} {}
};

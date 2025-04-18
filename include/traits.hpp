//
// Created by sayan on 4/18/25.
//

#pragma once

#include "expressions.hpp"
#include "operations.hpp"

template <typename T> struct make_constant {
  using type = T;
};

template <typename T> struct make_constant<Variable<T>> {
  using type = Constant<T>;
};

template <typename Op, typename LHS, typename RHS>
struct make_constant<Expression<Op, LHS, RHS>> {
  using type = Expression<Op, typename make_constant<LHS>::type,
                          typename make_constant<RHS>::type>;
};

template <typename TExpression>
using as_const_expression =
    make_constant<Expression<typename TExpression::op_type, typename TExpression::lhs_type,
                             typename TExpression::rhs_type>>::type;

static_assert(
    std::is_same_v<as_const_expression<Expression<
                       MultiplyOp<int>, Variable<int>, Constant<int>>>,
                   Expression<MultiplyOp<int>, Constant<int>, Constant<int>>>);

static_assert(
    std::is_same_v<as_const_expression<Expression<
                       MultiplyOp<int>, Variable<int>, Variable<int>>>,
                   Expression<MultiplyOp<int>, Constant<int>, Constant<int>>>);
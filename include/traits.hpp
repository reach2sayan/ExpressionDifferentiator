//
// Created by sayan on 4/18/25.
//

#pragma once
#include "expressions.hpp"
#include "values.hpp"
#include <array>

template <typename T> constexpr static bool is_const = false;
template <typename T> constexpr static bool is_const<Constant<T>> = true;

template <typename T, char symbol='X'> struct make_constant {
  using type = T;
};

template <typename T, char symbol> struct make_constant<Variable<T,symbol>> {
  using type = Constant<T>;
};

template <typename Op, typename... TExpressions>
struct make_constant<Expression<Op, TExpressions...>> {
  using type = Expression<Op, typename make_constant<TExpressions>::type...>;
};

template <typename TExpression>
using as_const_expression = make_constant<
    Expression<typename TExpression::op_type, typename TExpression::lhs_type,
               typename TExpression::rhs_type>>::type;

template <char symbol, typename Expr> struct replace_matching_variable;
template <char symbol, typename T> struct replace_matching_variable {
  using type = T;
};

template <char symbol, typename T>
struct replace_matching_variable<symbol, Variable<T,symbol>> {
  using type = Constant<T>;
};

template <char symbol, typename Op, typename... TExpressions>
struct replace_matching_variable<symbol, Expression<Op, TExpressions...>> {
  using type = Expression<
      Op, typename replace_matching_variable<symbol, TExpressions>::type...>;
};

template <char symbol, typename T>
using replace_matching_variable_t =
    typename replace_matching_variable<symbol, T>::type;

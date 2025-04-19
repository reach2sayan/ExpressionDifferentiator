//
// Created by sayan on 4/18/25.
//

#pragma once
#include "expressions.hpp"
#include "values.hpp"
#include <array>
#include <type_traits>

template <typename T> constexpr static bool is_const = false;
template <typename T> constexpr static bool is_const<Constant<T>> = true;

template <typename T, char symbol = 'X'> struct make_constant {
  using type = T;
};

template <typename T, char symbol> struct make_constant<Variable<T, symbol>> {
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
struct replace_matching_variable<symbol, Variable<T, symbol>> {
  using type = Constant<T>;
};

template <char symbol, typename Op, typename... TExpressions>
struct replace_matching_variable<symbol, Expression<Op, TExpressions...>> {
  using type = Expression<
      Op, typename replace_matching_variable<symbol, TExpressions>::type...>;
};

template <char symbol, typename TExpression>
using replace_matching_variable_t =
    typename replace_matching_variable<symbol, TExpression>::type;

template <char symbol, typename T>
constexpr auto replace_variable(const Variable<T, symbol> &var) {
  return Constant<T>(var);
}

template <char symbol, typename T, char othersymbol>
constexpr auto replace_variable(const Variable<T, othersymbol> &var)
    -> std::enable_if_t<(symbol != othersymbol), Variable<T, othersymbol>> {
  return var;
}

template <char symbol, typename T> auto replace_variable(const Constant<T> &c) {
  return c;
}

template <char symbol, typename Op, typename LHS, typename RHS>
auto replace_variable(const Expression<Op, LHS, RHS> &expr)
    -> Expression<Op, replace_matching_variable_t<symbol, LHS>,
                  replace_matching_variable_t<symbol, RHS>> {
  auto lexpr = expr.expressions().first;
  auto rexpr = expr.expressions().second;
  return {replace_variable<symbol>(std::move(lexpr)),
          replace_variable<symbol>(std::move(rexpr))};
}

template <char symbol, typename Op, typename LHS>
auto replace_variable(const MonoExpression<Op, LHS> &expr)
    -> MonoExpression<Op, replace_matching_variable_t<symbol, LHS>> {
  return {replace_variable<symbol>(expr.expressions())};
}
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
    -> std::enable_if_t<(symbol != othersymbol),
                        Variable<T, othersymbol>> { // without the enable_if_t
                                                    // it is ambiguous for two
                                                    // variables with same label
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

template <typename T, char C, std::size_t N>
void collect_vars_array(const Variable<T, C> &, std::array<char, N> &out,
                        std::size_t &index) {
  out[index++] = C;
}

template <typename T, std::size_t N>
void collect_vars_array(const Constant<T> &, std::array<char, N> &,
                        std::size_t &) {
  // no-op
}

template <typename Op, typename LHS, typename RHS, std::size_t N>
void collect_vars_array(const Expression<Op, LHS, RHS> &expr,
                        std::array<char, N> &out, std::size_t &index) {
  collect_vars_array(expr.expressions().first, out, index);
  collect_vars_array(expr.expressions().second, out, index);
}

template <char symbol, typename Expr> struct constify_unmatched_var;

template <char symbol, typename T>
struct constify_unmatched_var<symbol, Variable<T, symbol>> {
  using type = Variable<T, symbol>;
};

template <char symbol, typename T, char othersymbol>
struct constify_unmatched_var<symbol, Variable<T, othersymbol>> {
  using type = Constant<T>;
};

template <char symbol, typename T>
struct constify_unmatched_var<symbol, Constant<T>> {
  using type = Constant<T>;
};

template <char symbol, typename Op, typename LHS, typename RHS>
struct constify_unmatched_var<symbol, Expression<Op, LHS, RHS>> {
  using type =
      Expression<Op, typename constify_unmatched_var<symbol, LHS>::type,
                 typename constify_unmatched_var<symbol, RHS>::type>;
};

template <char symbol, typename Expr>
using constify_unmatched_var_t =
    typename constify_unmatched_var<symbol, Expr>::type;

template <char symbol, typename T>
constexpr auto transform_unmatched_var(const Variable<T, symbol> &v) {
  return v;
}

template <char symbol, typename T, char othersymbol>
constexpr auto transform_unmatched_var(const Variable<T, othersymbol> &var)
    -> std::enable_if_t<(symbol != othersymbol), Constant<T>> {
  return Constant<T>{var};
}

template <char Symbol, typename T>
constexpr auto transform_unmatched_var(const Constant<T> &c) {
  return c;
}

template <char symbol, typename Op, typename LHS, typename RHS>
constexpr auto transform_unmatched_var(const Expression<Op, LHS, RHS> &expr)
    -> constify_unmatched_var_t<symbol, Expression<Op, LHS, RHS>> {
  auto new_lhs = transform_unmatched_var<symbol>(expr.expressions().first);
  auto new_rhs = transform_unmatched_var<symbol>(expr.expressions().second);
  return {new_lhs, new_rhs};
}
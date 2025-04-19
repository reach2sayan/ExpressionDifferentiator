//
// Created by sayan on 4/18/25.
//

#pragma once
#include "expressions.hpp"
#include "values.hpp"

template <typename T> constexpr static bool is_const = false;
template <typename T> constexpr static bool is_const<Constant<T>> = true;

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
using as_const_expression = make_constant<
    Expression<typename TExpression::op_type, typename TExpression::lhs_type,
               typename TExpression::rhs_type>>::type;

template <char symbol, typename Expr> struct replace_matching_variable;

// Default: keep type unchanged
template <char symbol, typename T> struct replace_matching_variable {
  using type = T;
};

// Replace Variable<T> if symbol matches
template <char symbol, typename T>
struct replace_matching_variable<symbol, Variable<T>> {
  using type = std::conditional_t<(Variable<T>::symbol == symbol), Constant<T>,
                                  Variable<T>>;
};

// Expression<Op, LHS, RHS>
template <char symbol, typename Op, typename LHS, typename RHS>
struct replace_matching_variable<symbol, Expression<Op, LHS, RHS>> {
  using type =
      Expression<Op, typename replace_matching_variable<symbol, LHS>::type,
                 typename replace_matching_variable<symbol, RHS>::type>;
};

// Helper alias
template <char symbol, typename T>
using replace_matching_variable_t =
    typename replace_matching_variable<symbol, T>::type;

template<typename Expr>
constexpr std::size_t count_symbols(const Expr&) {
  return 0;
}

template<typename T>
constexpr std::size_t count_symbols(const Variable<T>&) {
  return 1;
}

template<typename Op, typename LHS, typename RHS>
constexpr std::size_t count_symbols(const Expression<Op, LHS, RHS>& expr) {
  return count_symbols(expr.expressions().first) + count_symbols(expr.expressions().second);
}

template<typename Expr, std::size_t N>
constexpr void collect_symbols_impl(const Expr&, std::array<char, N>&, std::size_t&) {}

template<typename T, std::size_t N>
constexpr void collect_symbols_impl(const Variable<T>& v, std::array<char, N>& out, std::size_t& i) {
  out[i++] = v.symbol;
}

template<typename Op, typename LHS, typename RHS, std::size_t N>
constexpr void collect_symbols_impl(const Expression<Op, LHS, RHS>& expr, std::array<char, N>& out, std::size_t& i) {
  collect_symbols_impl(expr.expressions().first, out, i);
  collect_symbols_impl(expr.expressions().second, out, i);
}

template<typename Expr>
constexpr auto collect_symbols(const Expr& expr) {
  constexpr std::size_t count = expr.var_count;
  std::array<char, count> result{};
  std::size_t i = 0;
  collect_symbols_impl(expr, result, i);
  return result;
}


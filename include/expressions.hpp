//
// Created by sayan on 4/13/25.
//

#pragma once

#include <ostream>
#include <tuple>

template <typename Op> class BaseExpression {
public:
  using value_type = typename Op::value_type;
};

template <typename Op, typename Exp>
class MonoExpression : public BaseExpression<Op> {
  Exp expression;
  friend std::ostream &operator<<(std::ostream &out, const MonoExpression &e) {
    out << '(' << e.expression << ')';
    return out;
  }

public:
  constexpr auto &expressions() const { return expression; }
  using lhs_type = Exp;
  using value_type = typename BaseExpression<Op>::value_type;
  constexpr MonoExpression(Exp);
  constexpr auto derivative() const { return Op::derivative(expression); }
  constexpr operator value_type() const { return eval(); }
  constexpr auto eval() const { return Op::eval(expression); }
};

template <typename Op, typename Exp>
constexpr MonoExpression<Op, Exp>::MonoExpression(Exp expr)
    : expression{std::move(expr)} {}

template <typename Op, typename LHS, typename RHS>
class Expression : public BaseExpression<Op> {
  std::pair<LHS, RHS> inner_expressions;
  friend std::ostream &operator<<(std::ostream &out, const Expression &e) {
    out << '(';
    std::apply([&out](const auto &...e) { Op::print(out, e...); },
               e.inner_expressions);
    out << ')';
    return out;
  }

public:
  using op_type = Op;
  using lhs_type = LHS;
  using rhs_type = RHS;
  using value_type = typename BaseExpression<Op>::value_type;
  constexpr auto &expressions() const { return inner_expressions; }
  constexpr Expression(LHS, RHS);
  constexpr auto eval() const;
  constexpr operator value_type() const { return eval(); }
  constexpr auto derivative() const;
};

template <typename Op, typename LHS, typename RHS>
constexpr Expression<Op, LHS, RHS>::Expression(LHS lhs, RHS rhs)
    : inner_expressions({std::move(lhs), std::move(rhs)}) {}

template <typename Op, typename LHS, typename RHS>
constexpr auto Expression<Op, LHS, RHS>::eval() const {
  return std::apply([](const auto &...e) { return Op::eval(e...); },
                    inner_expressions);
}

template <typename Op, typename LHS, typename RHS>
constexpr auto Expression<Op, LHS, RHS>::derivative() const {
  return std::apply([](const auto &...e) { return Op::derivative(e...); },
                    inner_expressions);
}

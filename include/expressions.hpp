//
// Created by sayan on 4/13/25.
//

#pragma once

#include <ostream>
#include <tuple>

template <typename Op, typename LHS, typename RHS> class Expression {
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
  using value_type = typename Op::value_type;
  constexpr auto &expressions() const { return inner_expressions; }
  constexpr static size_t var_count = LHS::var_count + RHS::var_count;
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

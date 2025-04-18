//
// Created by sayan on 4/13/25.
//

#pragma once

#include <functional>
#include <ostream>
#include <tuple>

template <typename Op, typename E1, typename E2> class Expression {
  std::tuple<E1, E2> inner_expressions;
  friend std::ostream &operator<<(std::ostream &out, const Expression &e) {
    out << '(';
    std::apply([&out](const auto &...e) { Op::print(out, e...); },
               e.inner_expressions);
    out << ')';
    return out;
  }

public:
  using value_type = typename Op::value_type;
  constexpr static size_t var_count = E1::var_count + E2::var_count;
  constexpr Expression(E1, E2);
  constexpr auto eval() const;
  constexpr operator value_type() const { return eval(); }
  constexpr auto derivative() const;
};

template <typename Op, typename E1, typename E2>
constexpr Expression<Op, E1, E2>::Expression(E1 e1, E2 e2)
    : inner_expressions({std::move(e1),std::move(e2)}) {}

template <typename Op, typename E1, typename E2>
constexpr auto Expression<Op, E1, E2>::eval() const {
  return std::apply([](const auto &...e) { return Op::eval(e...); },
                    inner_expressions);
}

template <typename Op, typename E1, typename E2>
constexpr auto Expression<Op, E1, E2>::derivative() const {
  return std::apply(
      [](const auto &...e) { return Op::derivative(e...); },
      inner_expressions);
}

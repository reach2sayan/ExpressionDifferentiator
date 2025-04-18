//
// Created by sayan on 4/13/25.
//

#pragma once

#include <functional>
#include <ostream>
#include <tuple>

template <typename Op, typename... E> class Expression {
  std::tuple<E...> inner_expressions;
  friend std::ostream &operator<<(std::ostream &out, const Expression &e) {
    out << '(';
    std::apply([&out](const auto &...e) { Op::print(out, e...); },
               e.inner_expressions);
    out << ')';
    return out;
  }

public:
  using value_type = typename Op::value_type;
  constexpr Expression(E... e);
  constexpr auto eval() const;
  constexpr operator value_type() const { return eval(); }
  constexpr auto derivative() const;
};

template <typename Op, typename... E>
constexpr Expression<Op, E...>::Expression(E... e)
    : inner_expressions(std::move(e)...) {}

template <typename Op, typename... E>
constexpr auto Expression<Op, E...>::eval() const {
  return std::apply([](const auto &...e) { return Op::eval(e...); },
                    inner_expressions);
}

template <typename Op, typename... E>
constexpr auto Expression<Op, E...>::derivative() const {
  return std::apply(
      [](const auto &...e) { return Op::derivative(e...); },
      inner_expressions);
}

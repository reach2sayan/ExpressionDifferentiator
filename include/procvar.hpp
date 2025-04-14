//
// Created by sayan on 4/13/25.
//

#ifndef PROCVAR_HPP
#define PROCVAR_HPP

#include "expressions.hpp"
#include "operations.hpp"
#include "values.hpp"
#include <utility>
#include <variant>

template <typename T> using var_type = std::variant<Variable<T>, Constant<T>>;

template <typename T> class ProcVar {
  var_type<T> value;
  template <typename U>
  ProcVar(var_type<U> &&v) : value{std::forward<decltype(v)>(v)} {}
  T get_value() const {
    return std::visit([](auto &&v) -> T { return v; }, value);
  }
  friend auto operator+(ProcVar a, const ProcVar &b) {
    auto expr = Sum<T>(a, b);
    return expr;
  }

public:
  ProcVar(Variable<T> v) : ProcVar{var_type<T>{v}} {}
  ProcVar(Constant<T> v) : ProcVar{var_type<T>{v}} {}
  constexpr operator T() const { return get_value(); }
  constexpr void set_value(T v);
  constexpr auto derivative() const {
    return std::visit([](auto &&v) -> T { return v.derivative(); }, value);
  }
};

template <typename T> constexpr void ProcVar<T>::set_value(T v) {
  if (std::holds_alternative<Variable<T>>(value)) {
    std::get<Variable<T>>(value).set(v);
  } else {
    std::unreachable();
  }
}

#endif // PROCVAR_HPP

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
struct VariableTag {};
struct ConstantTag {};

template <typename T> class ProcVar {
  var_type<T> value;

  template <typename U>
  constexpr ProcVar(U &&v) : value{var_type<T>{std::forward<U>(v)}} {}

  constexpr T get_value() const {
    return std::visit([](auto &&v) -> T { return v; }, value);
  }

  constexpr void set_value(T v);

  template <typename Expression1, typename Expression2>
  friend constexpr auto operator+(const Expression1 &a, const Expression2 &b) {
    return Sum<T>(a, b);
  }
  template <typename Expression1, typename Expression2>
  friend constexpr auto operator*(const Expression1 &a, const Expression2 &b) {
    return Multiply<T>(a, b);
  }
  template <typename Expression1, typename Expression2>
  friend constexpr auto operator-(const Expression1 &a, const Expression2 &b) {
    return Sum<T>(a, Multiply<T>(Constant(-1), b));
  }
  template <typename Expression1, typename Expression2>
  friend constexpr auto operator^(const Expression1 &a, const Expression2 &b) {
    return Exp<T>(a, b);
  }

  friend std::ostream &operator<<(std::ostream &out, const ProcVar<T> &c) {
    if (std::holds_alternative<Variable<T>>(c.value)) {
      out << std::get<Variable<T>>(c.value);
    } else if (std::holds_alternative<Constant<T>>(c.value)) {
      out << std::get<Constant<T>>(c.value);
    } else {
      throw std::bad_variant_access{};
    }
    return out;
  }

public:
  constexpr ProcVar(T v, VariableTag) : ProcVar{Variable<T>{std::move(v)}} {}
  constexpr ProcVar(T v, ConstantTag) : ProcVar{Constant<T>{std::move(v)}} {}

  constexpr operator T() const { return get_value(); }
  constexpr ProcVar &operator=(T v) {
    set_value(std::move(v));
    return *this;
  }
  constexpr auto derivative() const {
    return std::visit([](auto &&v) -> T { return v.derivative(); }, value);
  }
};

template <typename T> constexpr void ProcVar<T>::set_value(T v) {
  if (std::holds_alternative<Variable<T>>(value)) {
    std::get<Variable<T>>(value).set(std::move(v));
  } else {
    throw std::bad_variant_access{};
  }
}


#endif // PROCVAR_HPP

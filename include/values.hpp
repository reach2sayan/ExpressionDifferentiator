//
// Created by sayan on 4/13/25.
//

#pragma once
#include <ostream>

template <typename T> class Variable;
template <typename U> class ProcVar;
template <typename T> class Constant {
  const T value;
  const bool fixed;
  friend std::ostream &operator<<(std::ostream &out, const Constant<T> &c) {
    return out << c.value;
  }

public:
  constexpr explicit Constant(T value) : value(value), fixed(true) {}
  constexpr operator T() const { return value; }
  constexpr auto eval() const { return value; }
  constexpr auto derivative() const { return Constant{T{}}; }
};

template <typename T> class Variable {
  T value;
  const bool fixed;
  friend std::ostream &operator<<(std::ostream &out, const Variable<T> &c) {
    return out << 'v';
  }

public:
  constexpr explicit Variable(T value) : value(value), fixed(false) {}
  constexpr T eval() const { return value; }
  constexpr operator T() const { return value; }
  template <typename U> constexpr void set(U &&value) {
    value = std::forward<U>(value);
  }

  constexpr T derivative() const {
    auto ret = T{};
    return Constant{++ret};
  }
};

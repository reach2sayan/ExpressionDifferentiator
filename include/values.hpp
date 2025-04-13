//
// Created by sayan on 4/13/25.
//

#ifndef VALUES_HPP
#define VALUES_HPP
#include <ostream>

template <typename T> class Constant {
  T value;
public:
  constexpr Constant(T value) : value(value) {}
  constexpr operator T() const { return value; }
  constexpr auto eval() const { return value; }
  constexpr auto derivative() const { return Constant{T{}}; }
  friend std::ostream& operator<<(std::ostream& out, const Constant<T>& c) {
    return out << c.value;
  }
};

template <typename T> class Variable {
  T value;
public:
  constexpr Variable(T value) : value(value) {}
  constexpr operator T() const { return value; }
  constexpr T eval() const { return value; }
  constexpr T derivative() const { auto ret = T{}; return Constant{++ret}; }
  friend std::ostream& operator<<(std::ostream& out, const Variable<T>& c) {
    return out << 'v';
  }
};

#endif // VALUES_HPP

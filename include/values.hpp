//
// Created by sayan on 4/13/25.
//

#pragma once
#include <array>
#include <ostream>
#include <random>
#include <set>
#include <cstdlib>
#include <string_view>
#include <type_traits>

#define VALUE_TYPE_MISMATCH_ASSERT(T, U)                                       \
  static_assert(                                                               \
      std::is_same_v<typename T::value_type, typename U::value_type> ||        \
      std::is_convertible_v<typename T::value_type,typename U::value_type> ||  \
      std::is_convertible_v<typename U::value_type,typename T::value_type>,    \
      "Both expressions must have the same value type");

constexpr std::string_view letters = "abcdefghijklmnopqrstuvwxyz";
class character_generator {
  size_t c = 0;
public:
  constexpr auto operator()() {
    return letters[++c%26];
  }
} cgenerator;

struct Operators {
  template <typename Expression1, typename Expression2>
  friend constexpr auto operator+(const Expression1 &a, const Expression2 &b);

  template <typename Expression1, typename Expression2>
  friend constexpr auto operator*(const Expression1 &a, const Expression2 &b);

  template <typename Expression1, typename Expression2>
  friend constexpr auto operator-(const Expression1 &a, const Expression2 &b);

  template <typename Expression1, typename Expression2>
  friend constexpr auto operator^(const Expression1 &a, const Expression2 &b);
};

template <typename> class Variable;
template <typename> class ProcVar;

template <typename T> class Constant : public Operators {
  const T value;
  const bool fixed;
  friend std::ostream &operator<<(std::ostream &out, const Constant<T> &c) {
    return out << std::to_string(c.value) << std::string_view{"_c"};
  }

public:
  using value_type = T;
  constexpr static size_t var_count = 0;
  constexpr explicit Constant(T value) : value(value), fixed(true) {}
  constexpr operator T() const { return value; }
  constexpr auto eval() const { return value; }
  constexpr auto derivative() const { return Constant{T{}}; }
};

template <typename T> class Variable : public Operators {
  T value;
  const bool fixed;
  char symbol;
  friend std::ostream &operator<<(std::ostream &out, const Variable<T> &c) {
    return out << std::to_string(c.value) << "_" << c.symbol;
  }

public:
  using value_type = T;
  constexpr static size_t var_count = 1;
  constexpr explicit Variable(T value) : value(value), fixed(false), symbol(cgenerator()) {}
  constexpr T eval() const { return value; }
  constexpr operator T() const { return value; }
  template <typename U> constexpr void set(U &&value) {
    value = std::forward<U>(value);
  }
  constexpr decltype(auto) operator=(T v) {
    value = std::move(v);
    return *this;
  }
  constexpr auto derivative() const {
    auto ret = T{};
    return Constant{++ret};
  }
};

template <typename Expression1, typename Expression2>
constexpr auto operator+(const Expression1 &a, const Expression2 &b) {
  VALUE_TYPE_MISMATCH_ASSERT(Expression1, Expression2);
  using value_type = typename Expression1::value_type;
  return Sum<value_type>(a, b);
}
template <typename Expression1, typename Expression2>
constexpr auto operator*(const Expression1 &a, const Expression2 &b) {
  VALUE_TYPE_MISMATCH_ASSERT(Expression1, Expression2);
  using value_type = typename Expression1::value_type;
  return Multiply<value_type>(a, b);
}

template <typename Expression1, typename Expression2>
constexpr auto operator-(const Expression1 &a, const Expression2 &b) {
  VALUE_TYPE_MISMATCH_ASSERT(Expression1, Expression2);
  using value_type = typename Expression1::value_type;
  return Sum<value_type>(a, Multiply<value_type>(Constant(-1), b));
}
template <typename T, typename Expression1, typename Expression2>
constexpr auto operator^(const Expression1 &a, const Expression2 &b) {
  return Exp<T>(a, b);
}


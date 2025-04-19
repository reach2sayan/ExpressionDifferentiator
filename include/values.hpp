//
// Created by sayan on 4/13/25.
//

#pragma once
template <typename T> struct DivideOp;

#define VALUE_TYPE_MISMATCH_ASSERT(T, U)                                       \
  static_assert(                                                               \
      std::is_same_v<typename T::value_type, typename U::value_type> ||        \
          std::is_convertible_v<typename T::value_type,                        \
                                typename U::value_type> ||                     \
          std::is_convertible_v<typename U::value_type,                        \
                                typename T::value_type>,                       \
      "Both expressions must have the same value type");

constexpr std::string_view letters =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

class character_generator {
  mutable size_t c = 0;

public:
  constexpr char operator()() const { return letters[++c % 52]; }
};
constexpr static character_generator cgenerator{};

struct Operators {
  template <typename LHS, typename RHS>
  friend constexpr auto operator+(const LHS &a, const RHS &b);

  template <typename LHS, typename RHS>
  friend constexpr auto operator*(const LHS &a, const RHS &b);

  template <typename LHS, typename RHS>
  friend constexpr auto operator-(const LHS &a, const RHS &b);

  template <typename LHS, typename RHS>
  friend constexpr auto operator^(const LHS &a, const RHS &b);

  template <typename LHS, typename RHS>
  friend constexpr Expression<DivideOp<typename LHS::value_type>, LHS, RHS>
  operator/(const LHS &a, const RHS &b);
};

template <typename T> class Constant : public Operators {
  const T value;
  friend std::ostream &operator<<(std::ostream &out, const Constant<T> &c) {
    return out << std::to_string(c.value) << std::string_view{"_c"};
  }

public:
  using value_type = T;
  constexpr static size_t var_count = 0;
  constexpr explicit Constant(T value) : value(value) {}
  constexpr operator T() const { return value; }
  constexpr auto eval() const { return value; }
  constexpr auto derivative() const { return Constant{T{}}; }
};

template <typename T> class Variable : public Operators {
  T value;
  friend std::ostream &operator<<(std::ostream &out, const Variable<T> &c) {
    return out << std::to_string(c.value) << "_" << c.symbol;
  }
  static inline size_t static_counter = 0;

public:
  using value_type = T;
  const char symbol;
  constexpr static size_t var_count = 1;

  constexpr explicit Variable(T value) : value(value), symbol(cgenerator()) {}
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

template <typename LHS, typename RHS>
constexpr auto operator+(const LHS &a, const RHS &b) {
  VALUE_TYPE_MISMATCH_ASSERT(LHS, RHS);
  using value_type = typename LHS::value_type;
  return Sum<value_type>(a, b);
}
template <typename LHS, typename RHS>
constexpr auto operator*(const LHS &a, const RHS &b) {
  VALUE_TYPE_MISMATCH_ASSERT(LHS, RHS);
  using value_type = typename LHS::value_type;
  return Multiply<value_type>(a, b);
}

template <typename LHS, typename RHS>
constexpr auto operator-(const LHS &a, const RHS &b) {
  VALUE_TYPE_MISMATCH_ASSERT(LHS, RHS);
  using value_type = typename LHS::value_type;
  return Sum<value_type>(a, Multiply<value_type>(Constant(-1), b));
}

template <typename LHS, typename RHS>
constexpr Expression<DivideOp<typename LHS::value_type>, LHS, RHS>
operator/(const LHS &a, const RHS &b) {
  VALUE_TYPE_MISMATCH_ASSERT(LHS, RHS);
  using value_type = typename LHS::value_type;
  return Divide<value_type>(a, b);
}

template <typename T, typename LHS, typename RHS>
constexpr auto operator^(const LHS &a, const RHS &b) {
  VALUE_TYPE_MISMATCH_ASSERT(LHS, RHS);
  using value_type = typename LHS::value_type;
  return Exp<value_type>(a, b);
}

#define PV(x) Variable(x)
#define PC(x) Constant(x)

#define DEFINE_CONST_UDL(type, suffix)                                         \
  constexpr Constant<type> operator"" _##suffix(unsigned long long val) {      \
    return Constant<type>{static_cast<type>(val)};                             \
  }                                                                            \
  constexpr Constant<type> operator"" _##suffix(long double val) {             \
    return Constant<type>{static_cast<type>(val)};                             \
  }

#define DEFINE_VAR_UDL(type, suffix)                                           \
  constexpr Variable<type> operator"" _##suffix(unsigned long long val) {      \
    return Variable<type>{static_cast<type>(val)};                             \
  }                                                                            \
  constexpr Variable<type> operator"" _##suffix(long double val) {             \
    return Variable<type>{static_cast<type>(val)};                             \
  }

DEFINE_CONST_UDL(int, ci)
DEFINE_CONST_UDL(double, cd)

DEFINE_VAR_UDL(int, vi)
DEFINE_VAR_UDL(double, vd)
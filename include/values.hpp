//
// Created by sayan on 4/13/25.
//

#pragma once
#include <string>
#include <concepts>

template <typename T> struct DivideOp;

constexpr bool PRINT_VARIABLE_VALUE = false;
constexpr bool PRINT_VARIABLE_LABEL = true;
constexpr bool PRINT_CONSTANT_VALUE = true;
constexpr bool PRINT_CONSTANT_LABEL = false;

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

template <char C, typename Tuple, std::size_t I = 0>
constexpr std::size_t index_of_char_in_tuple() {
  if constexpr (I >= std::tuple_size_v<std::decay_t<Tuple>>) {
    static_assert(I < std::tuple_size_v<Tuple>, "Character not found in tuple");
    return -1;
  } else {
    using Elem = std::tuple_element_t<I, std::decay_t<Tuple>>;
    if constexpr (Elem::value == C) {
      return I;
    } else {
      return index_of_char_in_tuple<C, Tuple, I + 1>();
    }
  }
}

struct IOperators {
  template <typename LHS, typename RHS>
  friend constexpr auto operator+(const LHS &a, const RHS &b);

  template <typename LHS, typename RHS>
  friend constexpr auto operator*(const LHS &a, const RHS &b);

  template <typename LHS, typename RHS>
  friend constexpr auto operator-(const LHS &a, const RHS &b);

  template <typename LHS, typename RHS>
  friend constexpr Expression<DivideOp<typename LHS::value_type>, LHS, RHS>
  operator/(const LHS &a, const RHS &b);

  template <typename Expr>
  friend constexpr MonoExpression<SineOp<typename Expr::value_type>, Expr>
  sin(const Expr &a) {
    using value_type = typename Expr::value_type;
    return Sine<value_type>(a);
  }

  template <typename Expr>
  friend constexpr MonoExpression<CosineOp<typename Expr::value_type>, Expr>
  cos(const Expr &a) {
    using value_type = typename Expr::value_type;
    return Cosine<value_type>(a);
  }

  template <typename Expr>
  friend constexpr MonoExpression<ExpOp<typename Expr::value_type>, Expr>
  exp(const Expr &a) {
    using value_type = typename Expr::value_type;
    return Exp<value_type>(a);
  }
};

template <typename T> class Constant : public IOperators {
  const T value;
  friend std::ostream &operator<<(std::ostream &out, const Constant<T> &c) {
    if (PRINT_CONSTANT_VALUE)
      out << std::to_string(c.value);
    if (PRINT_CONSTANT_LABEL)
      out << std::string_view{"_c"};
    return out;
  }
  constexpr auto eval() const { return value; }

public:
  using value_type = T;
  constexpr explicit Constant(T value) : value(value) {}
  constexpr auto get() const { return value; }
  constexpr operator T() const { return value; }
  constexpr auto derivative() const { return Constant{T{}}; }
  constexpr void update(...) const {
    // No update needed for constant
  }
};

template <typename T, char symbol> class Variable : public IOperators {
  T value;
  friend std::ostream &operator<<(std::ostream &out,
                                  const Variable<T, symbol> &c) {
    if (PRINT_VARIABLE_VALUE) {
      out << std::to_string(c.value) << "_";
    }
    if (PRINT_VARIABLE_LABEL) {
      out << symbol;
    }
    return out;
  }
  static constexpr inline size_t static_counter = 0;
  constexpr T eval() const { return value; }

public:
  using value_type = T;
  constexpr explicit Variable(T value) : value(value) {}
  constexpr operator T() const { return value; }
  constexpr auto get() const { return value; }
  template <typename U> constexpr decltype(auto) operator=(U &&v);
  constexpr void update(const auto &symbols, const auto &updates);
  constexpr auto derivative() const;
};

template <typename T, char symbol>
template <typename U>
constexpr decltype(auto) Variable<T, symbol>::operator=(U &&v) {
  if constexpr (std::is_same_v<decltype(value),
                               std::reference_wrapper<std::decay_t<U>>>) {
    value.get() = std::forward<U>(v);
  } else {
    value = std::forward<U>(v);
  }
  return *this;
}

template <typename T, char symbol>
constexpr void Variable<T, symbol>::update(const auto &symbols,
                                           const auto &updates) {
  constexpr auto index = index_of_char_in_tuple<symbol, decltype(symbols)>();
  operator=(updates[index]);
}

template <typename T, char symbol>
constexpr auto Variable<T, symbol>::derivative() const {
  auto ret = T{};
  return Constant{++ret};
}

template<typename T>
concept ExpressionConcept = is_expression_type<std::remove_cvref_t<T>>::value;

template <ExpressionConcept LHS, ExpressionConcept RHS>
constexpr auto operator+(const LHS &a, const RHS &b) {
  VALUE_TYPE_MISMATCH_ASSERT(LHS, RHS);
  using value_type = typename LHS::value_type;
  return Sum<value_type>(a, b);
}
template <ExpressionConcept LHS, ExpressionConcept RHS>
constexpr auto operator*(const LHS &a, const RHS &b) {
  VALUE_TYPE_MISMATCH_ASSERT(LHS, RHS);
  using value_type = typename LHS::value_type;
  return Multiply<value_type>(a, b);
}

template <ExpressionConcept LHS, ExpressionConcept RHS>
constexpr auto operator-(const LHS &a, const RHS &b) {
  VALUE_TYPE_MISMATCH_ASSERT(LHS, RHS);
  using value_type = typename LHS::value_type;
  return Minus<value_type>(a, b);
}

template <ExpressionConcept LHS, ExpressionConcept RHS>
constexpr Expression<DivideOp<typename LHS::value_type>, LHS, RHS>
operator/(const LHS &a, const RHS &b) {
  VALUE_TYPE_MISMATCH_ASSERT(LHS, RHS);
  using value_type = typename LHS::value_type;
  return Divide<value_type>(a, b);
}

#define PV(x, label) Variable<decltype(x), label>(x)
#define PC(x) Constant(x)

#define DEFINE_CONST_UDL(type, suffix)                                         \
  constexpr Constant<type> operator"" _##suffix(unsigned long long val) {      \
    return Constant<type>{static_cast<type>(std::move(val))};                  \
  }                                                                            \
  constexpr Constant<type> operator"" _##suffix(long double val) {             \
    return Constant<type>{static_cast<type>(std::move(val))};                  \
  }

#define DEFINE_VAR_UDL(type, suffix, label)                                    \
  constexpr auto operator"" _##suffix(unsigned long long val) {                \
    return Variable<type, label>{static_cast<type>(std::move(val))};           \
  }                                                                            \
  constexpr auto operator"" _##suffix(long double val) {                       \
    return Variable<type, label>{static_cast<type>(std::move(val))};           \
  }

DEFINE_CONST_UDL(int, ci)
DEFINE_CONST_UDL(double, cd)
DEFINE_VAR_UDL(int, vi, 'c')
DEFINE_VAR_UDL(double, vd, 'v')
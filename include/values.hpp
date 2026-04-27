#pragma once
#include "expressions.hpp"
#include "operations.hpp"
#include <boost/mp11.hpp>
#include <concepts>
#include <format>
#include <string_view>

constexpr bool PRINT_VARIABLE_VALUE = false;
constexpr bool PRINT_VARIABLE_LABEL = true;
constexpr bool PRINT_CONSTANT_VALUE = true;
constexpr bool PRINT_CONSTANT_LABEL = false;

// Concept replacing the VALUE_TYPE_MISMATCH_ASSERT macro: participates in
// overload resolution rather than firing inside the body.
template <typename LHS, typename RHS>
concept CompatibleValueTypes =
    std::is_same_v<typename LHS::value_type, typename RHS::value_type> ||
    std::is_convertible_v<typename LHS::value_type, typename RHS::value_type> ||
    std::is_convertible_v<typename RHS::value_type, typename LHS::value_type>;

constexpr std::string_view letters =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

class character_generator {
  mutable size_t c = 0;

public:
  constexpr char operator()() const { return letters[++c % 52]; }
};
constexpr static character_generator cgenerator{};

// Find the 0-based index of integral_constant<char,C> in an mp_list.
template <char C, typename SymList>
consteval std::size_t index_of_char_in_hana() {
  return boost::mp11::mp_find<SymList,
                              std::integral_constant<char, C>>::value;
}

struct IOperators {

  template <ExpressionConcept LHS, ExpressionConcept RHS>
    requires CompatibleValueTypes<LHS, RHS>
  friend constexpr auto operator+(const LHS &a, const RHS &b) {
    using value_type = typename LHS::value_type;
    return Expression<SumOp<value_type>, LHS, RHS>{std::move(a), std::move(b)};
  }

  template <ExpressionConcept LHS, ExpressionConcept RHS>
    requires CompatibleValueTypes<LHS, RHS>
  friend constexpr auto operator*(const LHS &a, const RHS &b) {
    using value_type = typename LHS::value_type;
    return Expression<MultiplyOp<value_type>, LHS, RHS>(std::move(a),
                                                        std::move(b));
  }

  template <ExpressionConcept LHS, ExpressionConcept RHS>
    requires CompatibleValueTypes<LHS, RHS>
  friend constexpr auto operator-(const LHS &a, const RHS &b) {
    using value_type = typename LHS::value_type;
    auto neg = MonoExpression<NegateOp<value_type>, RHS>(std::move(b));
    return Expression<SumOp<value_type>, LHS, decltype(neg)>{std::move(a),
                                                             std::move(neg)};
  }

  template <ExpressionConcept LHS, ExpressionConcept RHS>
    requires CompatibleValueTypes<LHS, RHS>
  friend constexpr auto operator/(const LHS &a, const RHS &b) {
    using value_type = typename LHS::value_type;
    return Expression<DivideOp<value_type>, LHS, RHS>{std::move(a),
                                                      std::move(b)};
  }

  template <ExpressionConcept Expr> friend constexpr auto sin(const Expr &a) {
    using value_type = typename Expr::value_type;
    return MonoExpression<SineOp<value_type>, Expr>{std::move(a)};
  }

  template <ExpressionConcept Expr> friend constexpr auto cos(const Expr &a) {
    using value_type = typename Expr::value_type;
    return MonoExpression<CosineOp<value_type>, Expr>{std::move(a)};
  }

  template <ExpressionConcept Expr> friend constexpr auto exp(const Expr &a) {
    using value_type = typename Expr::value_type;
    return MonoExpression<ExpOp<value_type>, Expr>{std::move(a)};
  }

  // Scalar-on-left overloads: wrap the scalar as Constant<VT> and delegate.
  template <Numeric S, ExpressionConcept RHS>
  friend constexpr auto operator+(S s, const RHS &b) {
    using VT = typename RHS::value_type;
    return Constant<VT>{static_cast<VT>(s)} + b;
  }
  template <Numeric S, ExpressionConcept RHS>
  friend constexpr auto operator*(S s, const RHS &b) {
    using VT = typename RHS::value_type;
    return Constant<VT>{static_cast<VT>(s)} * b;
  }
  template <Numeric S, ExpressionConcept RHS>
  friend constexpr auto operator-(S s, const RHS &b) {
    using VT = typename RHS::value_type;
    return Constant<VT>{static_cast<VT>(s)} - b;
  }
  template <Numeric S, ExpressionConcept RHS>
  friend constexpr auto operator/(S s, const RHS &b) {
    using VT = typename RHS::value_type;
    return Constant<VT>{static_cast<VT>(s)} / b;
  }

  // Scalar-on-right overloads.
  template <ExpressionConcept LHS, Numeric S>
  friend constexpr auto operator+(const LHS &a, S s) {
    using VT = typename LHS::value_type;
    return a + Constant<VT>{static_cast<VT>(s)};
  }
  template <ExpressionConcept LHS, Numeric S>
  friend constexpr auto operator*(const LHS &a, S s) {
    using VT = typename LHS::value_type;
    return a * Constant<VT>{static_cast<VT>(s)};
  }
  template <ExpressionConcept LHS, Numeric S>
  friend constexpr auto operator-(const LHS &a, S s) {
    using VT = typename LHS::value_type;
    return a - Constant<VT>{static_cast<VT>(s)};
  }
  template <ExpressionConcept LHS, Numeric S>
  friend constexpr auto operator/(const LHS &a, S s) {
    using VT = typename LHS::value_type;
    return a / Constant<VT>{static_cast<VT>(s)};
  }
};

template <Numeric T> class Constant : public IOperators {
  const T value;
  friend std::ostream &operator<<(std::ostream &out, const Constant<T> &c) {
    if constexpr (PRINT_CONSTANT_VALUE)
      out << std::format("{}", c.value);
    if constexpr (PRINT_CONSTANT_LABEL)
      out << "_c";
    return out;
  }
  [[nodiscard]] constexpr auto eval() const { return value; }

public:
  using value_type = T;
  constexpr explicit Constant(T value) : value(value) {}
  [[nodiscard]] constexpr auto get() const { return value; }
  constexpr operator T() const { return value; }
  [[nodiscard]] constexpr auto derivative() const { return Constant{T{}}; }
  constexpr void update(...) const {}
  constexpr void backward(const auto &, T, auto &) const {} // constant: no variable to accumulate to

  template <std::size_t I>
  [[nodiscard]] constexpr auto get() const {
    static_assert(I < 2);
    if constexpr (requires { std::tuple_size<T>::value; })
      return eval().template get<I>();
    else if constexpr (I == 0)
      return eval();
    else
      return static_cast<T>(derivative());
  }
};

template <Numeric T, char symbol> class Variable : public IOperators {
  T value;
  friend std::ostream &operator<<(std::ostream &out,
                                  const Variable<T, symbol> &c) {
    if constexpr (PRINT_VARIABLE_VALUE)
      out << std::format("{}_", c.value);
    if constexpr (PRINT_VARIABLE_LABEL)
      out << symbol;
    return out;
  }
  static constexpr inline size_t static_counter = 0;
  [[nodiscard]] constexpr T eval() const { return value; }

public:
  using value_type = T;
  constexpr explicit Variable(T value) : value(value) {}
  constexpr operator T() const { return value; }
  [[nodiscard]] constexpr auto get() const { return value; }
  template <typename U> constexpr decltype(auto) operator=(U &&v);
  constexpr void update(const auto &symbols, const auto &updates);
  [[nodiscard]] constexpr auto derivative() const;
  constexpr void backward(const auto &syms, T adj, auto &grads) const;

  template <std::size_t I>
  [[nodiscard]] constexpr auto get() const {
    static_assert(I < 2);
    if constexpr (requires { std::tuple_size<T>::value; })
      return eval().template get<I>();
    else if constexpr (I == 0)
      return eval();
    else
      return static_cast<T>(derivative());
  }
};

template <Numeric T, char symbol>
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

template <Numeric T, char symbol>
constexpr void Variable<T, symbol>::update(const auto &symbols,
                                           const auto &updates) {
  using Syms = std::decay_t<decltype(symbols)>;
  constexpr auto index = index_of_char_in_hana<symbol, Syms>();
  operator=(updates[index]);
}

template <Numeric T, char symbol>
constexpr auto Variable<T, symbol>::derivative() const {
  auto ret = T{};
  return Constant{++ret};
}

template <Numeric T, char symbol>
constexpr void Variable<T, symbol>::backward(const auto &syms, T adj,
                                             auto &grads) const {
  using Syms = std::decay_t<decltype(syms)>;
  constexpr auto idx = index_of_char_in_hana<symbol, Syms>();
  grads[idx] += adj;
}

#define PDV(x, label) Variable<Dual<decltype(x)>, label>(Dual<decltype(x)>{x, 0})
#define PV(x, label) Variable<decltype(x), label>(x)
#define PC(x) Constant(x)

#define DEFINE_CONST_UDL(type, suffix)                                         \
  consteval Constant<type> operator"" _##suffix(unsigned long long val) {      \
    return Constant<type>{static_cast<type>(val)};                             \
  }                                                                            \
  consteval Constant<type> operator"" _##suffix(long double val) {             \
    return Constant<type>{static_cast<type>(val)};                             \
  }

#define DEFINE_VAR_UDL(type, suffix, label)                                    \
  consteval auto operator"" _##suffix(unsigned long long val) {                \
    return Variable<type, label>{static_cast<type>(val)};                      \
  }                                                                            \
  consteval auto operator"" _##suffix(long double val) {                       \
    return Variable<type, label>{static_cast<type>(val)};                      \
  }

DEFINE_CONST_UDL(int, ci)
DEFINE_CONST_UDL(double, cd)
DEFINE_VAR_UDL(int, vi, 'c')
DEFINE_VAR_UDL(double, vd, 'v')

template <Numeric T>
struct std::tuple_size<Constant<T>> : std::integral_constant<std::size_t, 2> {};

template <std::size_t I, Numeric T>
struct std::tuple_element<I, Constant<T>> {
  using type = typename expression_element<T, I>::type;
};

template <Numeric T, char C>
struct std::tuple_size<Variable<T, C>> : std::integral_constant<std::size_t, 2> {};

template <std::size_t I, Numeric T, char C>
struct std::tuple_element<I, Variable<T, C>> {
  using type = typename expression_element<T, I>::type;
};

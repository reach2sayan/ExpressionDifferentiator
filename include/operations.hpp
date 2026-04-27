#pragma once

#include "expressions.hpp"
#include <cmath>
#include <functional>
#include <utility>

enum class OpType : short {
  Unary = 0,
  Binary = 1,
};
using ExpressionType = OpType;

template <typename F, typename T>
concept cunary_op = std::regular_invocable<F, const T &> &&
                    std::same_as<std::invoke_result_t<F, const T &>, T>;

template <typename F, typename T>
concept cbinary_op =
    std::regular_invocable<F, const T &, const T &> &&
    std::same_as<std::invoke_result_t<F, const T &, const T &>, T>;

template <typename T, OpType type> struct Op {
  using value_type = T;
  constexpr static OpType op_type = type;
};

template <typename T, typename func, char symbol>
  requires cunary_op<func, T>
struct UnaryOp : Op<T, OpType::Unary> {
  using value_type = Op<T, OpType::Unary>::value_type;
  static void print(std::ostream &out, const SymbolicExpr auto &lhs) {
    out << symbol << lhs;
  }
  [[nodiscard]] constexpr static auto
  eval(const std::convertible_to<T> auto &lhs) {
    return std::invoke(func{}, lhs);
  }
};

template <typename T, typename func, char symbol>
  requires cbinary_op<func, T>
struct BinaryOp : Op<T, OpType::Binary> {
  using value_type = Op<T, OpType::Binary>::value_type;
  static void print(std::ostream &out, const SymbolicExpr auto &lhs,
                    const SymbolicExpr auto &rhs) {
    out << lhs << symbol << rhs;
  }
  [[nodiscard]] constexpr static auto
  eval(const std::convertible_to<T> auto &lhs,
       const std::convertible_to<T> auto &rhs) {
    return std::invoke(func{}, lhs, rhs);
  }
};

template <typename T> struct SumOp : BinaryOp<T, std::plus<T>, '+'> {
  [[nodiscard]] constexpr static auto
  derivative(const std::convertible_to<T> auto &lhs,
             const std::convertible_to<T> auto &rhs) {
    return lhs.derivative() + rhs.derivative();
  }
};

template <typename T> struct MultiplyOp : BinaryOp<T, std::multiplies<T>, '*'> {
  [[nodiscard]] constexpr static auto
  derivative(const std::convertible_to<T> auto &lhs,
             const std::convertible_to<T> auto &rhs) {
    auto lmul = lhs.derivative() * rhs; // f'(x)g(x)
    auto rmul = lhs * rhs.derivative(); // f(x)g'(x)
    return std::move(lmul) + std::move(rmul);
  }
};

template <typename T> struct NegateOp : UnaryOp<T, std::negate<T>, '-'> {
  [[nodiscard]] constexpr static auto
  derivative(const std::convertible_to<T> auto &lhs) {
    auto d = lhs.derivative();
    return MonoExpression<NegateOp<T>, decltype(d)>{std::move(d)};
  }
};

template <typename T> struct DivideOp : BinaryOp<T, std::divides<T>, '/'> {
  [[nodiscard]] constexpr static auto
  derivative(const std::convertible_to<T> auto &lhs,
             const std::convertible_to<T> auto &rhs) {
    auto num_l = lhs.derivative() * rhs;                  // f'(x)g(x)
    auto num_r = lhs * rhs.derivative();                  // f(x)g'(x)
    auto numerator = std::move(num_l) - std::move(num_r); // f'g - fg'
    auto denominator = rhs * rhs;                         // g(x)^2
    return std::move(numerator) / std::move(denominator);
  }
};

template <typename T>
struct SineOp
    : UnaryOp<T, decltype([](const T &a) -> T { return std::sin(a); }), '$'> {
  template <typename Expr>
  [[nodiscard]] constexpr static auto derivative(const Expr &lhs);
};

template <typename T>
struct CosineOp
    : UnaryOp<T, decltype([](const T &a) -> T { return std::cos(a); }), '['> {
  template <typename Expr>
  [[nodiscard]] constexpr static auto derivative(const Expr &lhs);
};

template <typename T>
template <typename Expr>
constexpr auto CosineOp<T>::derivative(const Expr &lhs) {
  return Negate<T>(sin(lhs)) * lhs.derivative();
}

template <typename T>
template <typename Expr>
constexpr auto SineOp<T>::derivative(const Expr &expr) {
  return cos(expr) * expr.derivative();
}

template <typename T>
struct ExpOp
    : UnaryOp<T, decltype([](const T &a) -> T { return std::exp(a); }), 'e'> {
  template <typename Expr>
  [[nodiscard]] constexpr static auto derivative(const Expr &lhs) {
    return MonoExpression<ExpOp<T>, Expr>{lhs} * lhs.derivative();
  }
};

template <typename T, typename Expr>
[[nodiscard]] constexpr inline auto Negate(Expr expr) {
  return MonoExpression<NegateOp<T>, Expr>{std::move(expr)};
}

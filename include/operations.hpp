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
  static void print(std::ostream &out, const ExpressionConcept auto &lhs) {
    out << symbol << lhs;
  }
  [[nodiscard]] constexpr static auto eval(const ExpressionConcept auto &lhs) {
    return std::invoke(func{}, lhs);
  }
};

template <typename T, typename func, char symbol>
  requires cbinary_op<func, T>
struct BinaryOp : Op<T, OpType::Binary> {
  using value_type = Op<T, OpType::Binary>::value_type;
  static void print(std::ostream &out, const ExpressionConcept auto &lhs,
                    const ExpressionConcept auto &rhs) {
    out << lhs << symbol << rhs;
  }
  [[nodiscard]] constexpr static auto eval(const ExpressionConcept auto &lhs,
                                           const ExpressionConcept auto &rhs) {
    return std::invoke(func{}, lhs, rhs);
  }
};

template <typename T> struct SumOp : BinaryOp<T, std::plus<T>, '+'> {
  [[nodiscard]] constexpr static auto
  derivative(const ExpressionConcept auto &lhs,
             const ExpressionConcept auto &rhs) {
    return lhs.derivative() + rhs.derivative();
  }
  // ā += w̄,  b̄ += w̄
  constexpr static void backward(const ExpressionConcept auto &lhs,
                                 const ExpressionConcept auto &rhs, T adj,
                                 const auto &syms, auto &grads) {
    lhs.backward(syms, adj, grads);
    rhs.backward(syms, adj, grads);
  }
};

template <Numeric T> struct MultiplyOp : BinaryOp<T, std::multiplies<T>, '*'> {
  [[nodiscard]] constexpr static auto
  derivative(const ExpressionConcept auto &lhs,
             const ExpressionConcept auto &rhs) {
    auto lmul = lhs.derivative() * rhs; // f'(x)g(x)
    auto rmul = lhs * rhs.derivative(); // f(x)g'(x)
    return std::move(lmul) + std::move(rmul);
  }
  // ā += w̄·b,  b̄ += w̄·a
  constexpr static void backward(const ExpressionConcept auto &lhs,
                                 const ExpressionConcept auto &rhs, T adj,
                                 const auto &syms, auto &grads) {
    lhs.backward(syms, adj * static_cast<T>(rhs), grads);
    rhs.backward(syms, adj * static_cast<T>(lhs), grads);
  }
};

template <Numeric T> struct NegateOp : UnaryOp<T, std::negate<T>, '-'> {
  [[nodiscard]] constexpr static auto
  derivative(const ExpressionConcept auto &lhs) {
    auto d = lhs.derivative();
    return MonoExpression<NegateOp<T>, decltype(d)>{std::move(d)};
  }
  // ā += -w̄
  constexpr static void backward(const ExpressionConcept auto &expr, T adj,
                                 const auto &syms, auto &grads) {
    expr.backward(syms, -adj, grads);
  }
};

template <Numeric T> struct DivideOp : BinaryOp<T, std::divides<T>, '/'> {
  [[nodiscard]] constexpr static auto
  derivative(const ExpressionConcept auto &lhs,
             const ExpressionConcept auto &rhs) {
    auto num_l = lhs.derivative() * rhs;                  // f'(x)g(x)
    auto num_r = lhs * rhs.derivative();                  // f(x)g'(x)
    auto numerator = std::move(num_l) - std::move(num_r); // f'g - fg'
    auto denominator = rhs * rhs;                         // g(x)^2
    return std::move(numerator) / std::move(denominator);
  }
  // ā += w̄/b,  b̄ += -w̄·a/b²
  constexpr static void backward(const ExpressionConcept auto &lhs,
                                 const ExpressionConcept auto &rhs, T adj,
                                 const auto &syms, auto &grads) {
    const T b = static_cast<T>(rhs);
    lhs.backward(syms, adj / b, grads);
    rhs.backward(syms, -adj * static_cast<T>(lhs) / (b * b), grads);
  }
};

namespace detail {
template <Numeric T> struct sine_impl {
  T operator()(const T &a) const {
    using std::sin;
    return sin(a);
  }
};
template <Numeric T> struct cosine_impl {
  T operator()(const T &a) const {
    using std::cos;
    return cos(a);
  }
};
} // namespace detail

template <Numeric T> struct SineOp : UnaryOp<T, detail::sine_impl<T>, '$'> {
  [[nodiscard]] constexpr static auto
  derivative(const ExpressionConcept auto &lhs);
  // ā += w̄·cos(a)
  constexpr static void backward(const ExpressionConcept auto &expr, T adj,
                                 const auto &syms, auto &grads) {
    using std::cos;
    expr.backward(syms, adj * cos(static_cast<T>(expr)), grads);
  }
};

template <Numeric T> struct CosineOp : UnaryOp<T, detail::cosine_impl<T>, '['> {
  [[nodiscard]] constexpr static auto
  derivative(const ExpressionConcept auto &lhs);
  // ā += -w̄·sin(a)
  constexpr static void backward(const ExpressionConcept auto &expr, T adj,
                                 const auto &syms, auto &grads) {
    using std::sin;
    expr.backward(syms, -adj * sin(static_cast<T>(expr)), grads);
  }
};

template <Numeric T>
constexpr auto CosineOp<T>::derivative(const ExpressionConcept auto &lhs) {
  return Negate<T>(sin(lhs)) * lhs.derivative();
}

template <Numeric T>
constexpr auto SineOp<T>::derivative(const ExpressionConcept auto &expr) {
  return cos(expr) * expr.derivative();
}

namespace detail {
template <Numeric T> struct exp_impl {
  T operator()(const T &a) const {
    using std::exp;
    return exp(a);
  }
};
} // namespace detail

template <Numeric T> struct ExpOp : UnaryOp<T, detail::exp_impl<T>, 'e'> {
  [[nodiscard]] constexpr static auto
  derivative(const ExpressionConcept auto &lhs) {
    return MonoExpression<ExpOp<T>, std::decay_t<decltype(lhs)>>{lhs} *
           lhs.derivative();
  }
  // ā += w̄·exp(a)
  constexpr static void backward(const ExpressionConcept auto &expr, T adj,
                                 const auto &syms, auto &grads) {
    using std::exp;
    expr.backward(syms, adj * exp(static_cast<T>(expr)), grads);
  }
};

template <Numeric T>
[[nodiscard]] constexpr inline auto Negate(ExpressionConcept auto expr) {
  return MonoExpression<NegateOp<T>, decltype(expr)>{std::move(expr)};
}
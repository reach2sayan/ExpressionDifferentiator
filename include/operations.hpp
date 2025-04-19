//
// Created by sayan on 4/13/25.
//

#pragma once

#include "expressions.hpp"

#include <cmath>
#include <functional>
#include <utility>

template <typename Op, typename LHS, typename RHS> class Expression;
template <typename> class Constant;

enum class OpType : short {
  Unary = 0,
  Binary = 1,
};
using ExpressionType = OpType;

template <typename T> using unary_op_func = T(const T &);
template <typename T> using binary_op_func = T(const T &, const T &);

template <typename T, OpType type> struct Op {
  using value_type = T;
  constexpr static OpType op_type = type;
};

template <typename T, unary_op_func<T> func, char symbol>
struct UnaryOp : Op<T, OpType::Unary> {
  using value_type = Op<T, OpType::Unary>::value_type;
  template <typename LHS> static void print(std::ostream &out, const LHS &lhs) {
    out << symbol << lhs;
  }

  template <typename LHS> constexpr static auto eval(const LHS &lhs) {
    return std::invoke(func, lhs);
  }
};

template <typename T, binary_op_func<T> func, char symbol>
struct BinaryOp : Op<T, OpType::Binary> {
  using value_type = Op<T, OpType::Binary>::value_type;
  template <typename LHS, typename RHS>
  static void print(std::ostream &out, const RHS &lhs, const LHS &rhs) {
    out << lhs << symbol << rhs;
  }
  template <typename LHS, typename RHS>
  constexpr static auto eval(const LHS &lhs, const RHS &rhs) {
    return std::invoke(func, lhs, rhs);
  }
};

template <typename Op> [[deprecated]] struct Derivative {
  constexpr static size_t count = [](OpType op) {
    return std::to_underlying(op) + 1;
  }(Op::op_type);
};

template <typename T>
struct ExpOp
    : BinaryOp<T, [](const T &a, const T &b) -> T { return std::pow(a, b); },
               '^'> {
  template <typename LHS, typename RHS>
  constexpr static auto derivative(const LHS &lhs, const RHS &rhs);
};

static_assert(ExpOp<int>::op_type == OpType::Binary);
static_assert(Derivative<ExpOp<int>>::count == 2);

template <typename T>
struct SumOp
    : BinaryOp<T, [](const T &a, const T &b) -> T { return a + b; }, '+'> {
  template <typename LHS, typename RHS>
  constexpr static auto derivative(const LHS &lhs, const RHS &rhs);
};

template <typename T, typename LHS, typename RHS>
constexpr inline auto Sum(LHS lhs, RHS rhs) {
  return Expression<SumOp<T>, LHS, RHS>{std::move(lhs), std::move(rhs)};
}

template <typename T>
template <typename LHS, typename RHS>
constexpr auto ExpOp<T>::derivative(const LHS &lhs, const RHS &rhs) {
  throw std::runtime_error{"Not implemented"};
  return;
}

template <typename T>
template <typename LHS, typename RHS>
constexpr auto SumOp<T>::derivative(const LHS &lhs, const RHS &rhs) {
  return Sum<T>(lhs.derivative(), rhs.derivative());
}

template <typename T>
struct MultiplyOp
    : BinaryOp<T, [](const T &a, const T &b) -> T { return a * b; }, '*'> {
  template <typename LHS, typename RHS>
  constexpr static auto derivative(const LHS &lhs, const RHS &rhs);
};

template <typename T>
template <typename LHS, typename RHS>
constexpr auto MultiplyOp<T>::derivative(const LHS &lhs, const RHS &rhs) {
  auto lmul = Multiply<T>(lhs.derivative(), rhs); // f'(x)g(x)
  auto rmul = Multiply<T>(lhs, rhs.derivative()); // f(x)g'(x)
  return Sum<T>(std::move(lmul), std::move(rmul));
}

template <typename T>
struct NegateOp : UnaryOp<T,
                          [](const T &a) -> T {
                            T v{};
                            --v;
                            return std::move(v) * a;
                          },
                          '-'> {
  template <typename LHS> constexpr static auto derivative(const LHS &lhs);
};

template <typename T>
template <typename LHS>
constexpr auto NegateOp<T>::derivative(const LHS &lhs) {
  auto d = lhs.derivative();
  return MonoExpression<NegateOp<T>, decltype(d)>{std::move(d)};
}

template <typename T>
struct DivideOp
    : BinaryOp<T, [](const T &a, const T &b) -> T { return a / b; }, '/'> {
  template <typename LHS, typename RHS>
  constexpr static auto derivative(const LHS &lhs, const RHS &rhs);
};

template <typename T, typename LHS, typename RHS>
constexpr inline auto Multiply(LHS lhs, RHS rhs) {
  return Expression<MultiplyOp<T>, LHS, RHS>(std::move(lhs), std::move(rhs));
}

template <typename T>
template <typename LHS, typename RHS>
constexpr auto DivideOp<T>::derivative(const LHS &lhs, const RHS &rhs) {
  auto num_l = Multiply<T>(lhs.derivative(), rhs); // f'(x)g(x)
  auto num_r = Multiply<T>(lhs, rhs.derivative()); // f(x) * g'(x)
  auto numerator =
      Minus<T>(std::move(num_l), std::move(num_r)); // f'(x)g(x) - f(x)g'(x)
  auto denominator = Multiply<T>(rhs, rhs);         // g(x)^2
  return Divide<T>(std::move(numerator), std::move(denominator));
}

template <typename T, typename Expr> constexpr inline auto Negate(Expr expr) {
  return MonoExpression<NegateOp<T>, Expr>{std::move(expr)};
}

template <typename T, typename LHS, typename RHS>
constexpr inline auto Divide(LHS lhs, RHS rhs) {
  return Expression<DivideOp<T>, LHS, RHS>{std::move(lhs), std::move(rhs)};
}

template <typename T, typename LHS, typename RHS>
constexpr inline auto Minus(LHS lhs, RHS rhs) {
  auto neg = MonoExpression<NegateOp<T>, RHS>(std::move(rhs));
  return Expression<SumOp<T>, LHS, decltype(neg)>{std::move(lhs),
                                                  std::move(neg)};
}

template <typename T, typename LHS, typename RHS>
constexpr inline auto Exp(LHS lhs, RHS rhs) {
  return Expression<ExpOp<T>, LHS, RHS>(std::move(lhs), std::move(rhs));
}
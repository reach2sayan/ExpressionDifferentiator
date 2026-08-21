#pragma once

#include "expr/unary_math.hpp" // DDX_UNARY_MATH_TABLE

#include <cstdint>
#include <string_view>

// The runtime mirror of the compile-time operation set.  Rows are
// (factory spelling, enumerator, label) -- the same shape as
// DDX_UNARY_MATH_TABLE, so the eighteen transcendentals come straight from
// expr/unary_math.hpp and the two sets cannot drift.
namespace ddx::rt {

#define DDX_RT_LEAF_TABLE(X)                                                   \
  X(constant, Const, "const")                                                  \
  X(var, Var, "var")

#define DDX_RT_UNARY_TABLE(X)                                                  \
  X(neg, Neg, "-")                                                             \
  X(abs, Abs, "abs")

#define DDX_RT_BINARY_TABLE(X)                                                 \
  X(add, Add, "+")                                                             \
  X(mul, Mul, "*")                                                             \
  X(div, Div, "/")                                                             \
  X(pow, Pow, "pow")                                                           \
  X(atan2, Atan2, "atan2")                                                     \
  X(hypot, Hypot, "hypot")                                                     \
  X(max, Max, "max")                                                           \
  X(min, Min, "min")

#define DDX_RT_OP_TABLE(X)                                                     \
  DDX_RT_LEAF_TABLE(X)                                                         \
  DDX_RT_UNARY_TABLE(X)                                                        \
  DDX_UNARY_MATH_TABLE(X)                                                      \
  DDX_RT_BINARY_TABLE(X)

enum class OpCode : std::uint8_t {
#define DDX_RT_ENUMERATOR(fn, Op, label) Op,
  DDX_RT_OP_TABLE(DDX_RT_ENUMERATOR)
#undef DDX_RT_ENUMERATOR
};

inline constexpr std::size_t op_count = [] {
  std::size_t n = 0;
#define DDX_RT_COUNT(fn, Op, label) ++n;
  DDX_RT_OP_TABLE(DDX_RT_COUNT)
#undef DDX_RT_COUNT
  return n;
}();

[[nodiscard]] constexpr std::string_view label_of(OpCode op) noexcept {
  switch (op) {
#define DDX_RT_LABEL(fn, Op, label)                                            \
  case OpCode::Op:                                                             \
    return label;
    DDX_RT_OP_TABLE(DDX_RT_LABEL)
#undef DDX_RT_LABEL
  }
  return "?";
}

[[nodiscard]] constexpr std::uint8_t arity_of(OpCode op) noexcept {
  switch (op) {
#define DDX_RT_ARITY(fn, Op, label)                                            \
  case OpCode::Op:                                                             \
    return 0;
    DDX_RT_LEAF_TABLE(DDX_RT_ARITY)
#undef DDX_RT_ARITY
#define DDX_RT_ARITY(fn, Op, label)                                            \
  case OpCode::Op:                                                             \
    return 1;
    DDX_RT_UNARY_TABLE(DDX_RT_ARITY)
    DDX_UNARY_MATH_TABLE(DDX_RT_ARITY)
#undef DDX_RT_ARITY
#define DDX_RT_ARITY(fn, Op, label)                                            \
  case OpCode::Op:                                                             \
    return 2;
    DDX_RT_BINARY_TABLE(DDX_RT_ARITY)
#undef DDX_RT_ARITY
  }
  return 0;
}

// Operands may be swapped only where the op says so; canonical ordering in the
// builder uses this to make a*b and b*a intern to one node.
[[nodiscard]] constexpr bool is_commutative(OpCode op) noexcept {
  return op == OpCode::Add || op == OpCode::Mul || op == OpCode::Max ||
         op == OpCode::Min || op == OpCode::Hypot;
}

[[nodiscard]] constexpr bool is_leaf(OpCode op) noexcept {
  return arity_of(op) == 0;
}

} // namespace ddx::rt

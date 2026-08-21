#pragma once

#include "expr/operations.hpp" // pow_impl, atan2_impl, hypot_impl, max_impl, min_impl, abs_impl
#include "expr/unary_math.hpp" // the eighteen descriptors
#include "rt/opcode.hpp"

namespace ddx::rt {

// Dispatch an OpCode onto ddx's own functors.  Templated on the scalar so the
// interpreter, constant folding and the graph builder all share one definition:
// at T = double it computes, at T = Expr it builds nodes.
template <impl::Numeric T>
[[nodiscard]] constexpr T apply1(OpCode op, const T &u) noexcept {
  switch (op) {
  case OpCode::Neg:
    return -u;
  case OpCode::Abs:
    return T{impl::detail::abs_impl{}(u)};
#define DDX_RT_APPLY1(fn, Op, label)                                           \
  case OpCode::Op:                                                             \
    return T{impl::detail::Op##Fn<T>{}(u)};
    DDX_UNARY_MATH_TABLE(DDX_RT_APPLY1)
#undef DDX_RT_APPLY1
  default:
    return T{};
  }
}

template <impl::Numeric T>
[[nodiscard]] constexpr T apply2(OpCode op, const T &l, const T &r) noexcept {
  switch (op) {
  case OpCode::Add:
    return l + r;
  case OpCode::Mul:
    return l * r;
  case OpCode::Div:
    return l / r;
  case OpCode::Pow:
    return T{impl::detail::pow_impl{}(l, r)};
  case OpCode::Atan2:
    return T{impl::detail::atan2_impl{}(l, r)};
  case OpCode::Hypot:
    return T{impl::detail::hypot_impl{}(l, r)};
  case OpCode::Max:
    return T{impl::detail::max_impl{}(l, r)};
  case OpCode::Min:
    return T{impl::detail::min_impl{}(l, r)};
  default:
    return T{};
  }
}

} // namespace ddx::rt

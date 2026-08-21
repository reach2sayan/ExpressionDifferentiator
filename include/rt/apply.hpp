#pragma once

#include "expr/operations.hpp" // pow_impl, atan2_impl, hypot_impl, max_impl, min_impl, abs_impl
#include "expr/unary_math.hpp" // the eighteen descriptors
#include "rt/opcode.hpp"

#include <functional> // std::plus and friends, for the table's eval column

namespace ddx::rt {

// Dispatch an OpCode onto ddx's own functors.  Templated on the scalar so the
// interpreter, constant folding and the graph builder all share one definition:
// at T = double it computes, at T = Expr it builds nodes.  Overloaded on arity
// rather than numbered, because the argument list already says which is which.
template <impl::Numeric T>
[[nodiscard]] constexpr T apply(OpCode op, const T &u) noexcept {
  switch (op) {
#define DDX_RT_APPLY(fn, Op, label, functor, ...)                              \
  case OpCode::Op:                                                             \
    return T{functor{}(u)};
    DDX_RT_UNARY_TABLE(DDX_RT_APPLY)
#undef DDX_RT_APPLY
#define DDX_RT_APPLY(fn, Op, label)                                            \
  case OpCode::Op:                                                             \
    return T{impl::detail::Op##Fn<T>{}(u)};
    DDX_UNARY_MATH_TABLE(DDX_RT_APPLY)
#undef DDX_RT_APPLY
  default:
    return T{};
  }
}

template <impl::Numeric T>
[[nodiscard]] constexpr T apply(OpCode op, const T &l, const T &r) noexcept {
  switch (op) {
#define DDX_RT_APPLY(fn, Op, label, functor, ...)                              \
  case OpCode::Op:                                                             \
    return T{functor{}(l, r)};
    DDX_RT_BINARY_TABLE(DDX_RT_APPLY)
#undef DDX_RT_APPLY
  default:
    return T{};
  }
}

} // namespace ddx::rt

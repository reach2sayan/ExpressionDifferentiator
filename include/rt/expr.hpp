#pragma once

#include "expr/expressions.hpp" // CArithmetic
#include "rt/builder.hpp"
#include "rt/opcode.hpp"

#include <string_view>
#include <type_traits>
#include <utility>

namespace ddx::rt {

// A handle onto one node, plus the arithmetic surface that builds more.  A null
// builder means the value is a literal that has not been given to a graph yet,
// which is what lets Expr{1} exist: CFieldLike requires
// constructible_from<int>, and the derivative rules in expr/unary_math.hpp
// manufacture exactly 0 and 1.
class Expr {
public:
  constexpr Expr() = default;
  // By value, not a forwarding reference: Expr satisfies Numeric itself, and a
  // Numeric&& would out-match the copy constructor for a non-const lvalue.
  // Taking V by value leaves both an exact match, and the non-template wins.
  constexpr Expr(impl::Numeric auto v) noexcept : lit_(static_cast<double>(v)) {}
  constexpr Expr(Builder &b, NodeId id) noexcept : builder_(&b), id_(id) {}

  [[nodiscard]] constexpr bool pending() const noexcept {
    return builder_ == nullptr;
  }
  [[nodiscard]] constexpr double literal() const noexcept { return lit_; }
  [[nodiscard]] constexpr Builder *builder() const noexcept { return builder_; }

  // Give the node an identity in `b`, materialising a pending literal.
  [[nodiscard]] constexpr NodeId id(Builder &b) const {
    return builder_ ? id_ : b.constant(lit_);
  }

private:
  Builder *builder_ = nullptr;
  NodeId id_ = no_node;
  double lit_ = 0.0;
};

namespace detail {
[[nodiscard]] inline constexpr Builder *pick(const Expr &l,
                                             const Expr &r) noexcept {
  return l.builder() ? l.builder() : r.builder();
}
} // namespace detail

// Two pending literals fold without ever reaching a graph, so constants the
// derivative rules produce never become nodes.
[[nodiscard]] inline constexpr Expr make(OpCode op, const Expr &u) {
  Builder *const b = u.builder();
  return (b == nullptr) ? Expr{apply(op, u.literal())}
                        : Expr{*b, b->make(op, u.id(*b))};
}

[[nodiscard]] inline constexpr Expr make(OpCode op, const Expr &l,
                                         const Expr &r) {
  Builder *const b = detail::pick(l, r);
  return (b == nullptr) ? Expr{apply(op, l.literal(), r.literal())}
                        : Expr{*b, b->make(op, l.id(*b), r.id(*b))};
}

[[nodiscard]] inline constexpr Expr operator+(const Expr &l, const Expr &r) {
  return make(OpCode::Add, l, r);
}
[[nodiscard]] inline constexpr Expr operator*(const Expr &l, const Expr &r) {
  return make(OpCode::Mul, l, r);
}
[[nodiscard]] inline constexpr Expr operator/(const Expr &l, const Expr &r) {
  return make(OpCode::Div, l, r);
}
[[nodiscard]] inline constexpr Expr operator-(const Expr &u) {
  return make(OpCode::Neg, u);
}
[[nodiscard]] inline constexpr Expr operator-(const Expr &l, const Expr &r) {
  return l + (-r);
}

#define DDX_RT_UNFN(fn, Op, label, ...)                                        \
  [[nodiscard]] inline constexpr Expr fn(const Expr &u) {                      \
    return make(OpCode::Op, u);                                                \
  }
DDX_UNARY_MATH_TABLE(DDX_RT_UNFN)
DDX_RT_UNARY_TABLE(DDX_RT_UNFN)
#undef DDX_RT_UNFN

#define DDX_RT_BINFN(fn, Op, label, ...)                                       \
  [[nodiscard]] inline constexpr Expr fn(const Expr &l, const Expr &r) {       \
    return make(OpCode::Op, l, r);                                             \
  }
DDX_RT_BINARY_TABLE(DDX_RT_BINFN)
#undef DDX_RT_BINFN

// A graph plus the handles onto it.  Holding the builder by value keeps the
// lifetime obvious: an Expr never outlives the Graph it names.
[[nodiscard]] inline constexpr Expr var(Builder &b, std::string_view name) {
  return Expr{b, b.variable(name)};
}
[[nodiscard]] inline constexpr Expr lit(Builder &b, double v) {
  return Expr{b, b.constant(v)};
}

} // namespace ddx::rt

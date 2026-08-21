#pragma once

#include "rt/builder.hpp"
#include "rt/opcode.hpp"

#include <string_view>
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
  constexpr Expr(int v) noexcept : lit_(static_cast<double>(v)) {}
  constexpr Expr(double v) noexcept : lit_(v) {}
  Expr(Builder &b, NodeId id) noexcept : builder_(&b), id_(id) {}

  [[nodiscard]] constexpr bool pending() const noexcept {
    return builder_ == nullptr;
  }
  [[nodiscard]] constexpr double literal() const noexcept { return lit_; }
  [[nodiscard]] constexpr Builder *builder() const noexcept { return builder_; }

  // Give the node an identity in `b`, materialising a pending literal.
  [[nodiscard]] NodeId id(Builder &b) const {
    return builder_ ? id_ : b.constant(lit_);
  }

private:
  Builder *builder_ = nullptr;
  NodeId id_ = no_node;
  double lit_ = 0.0;
};

namespace detail {
[[nodiscard]] inline Builder *pick(const Expr &l, const Expr &r) noexcept {
  return l.builder() ? l.builder() : r.builder();
}
} // namespace detail

// Two pending literals fold without ever reaching a graph, so constants the
// derivative rules produce never become nodes.
[[nodiscard]] inline Expr make(OpCode op, const Expr &u) {
  Builder *b = u.builder();
  if (b == nullptr) {
    return Expr{apply1(op, u.literal())};
  }
  return Expr{*b, b->make(op, u.id(*b))};
}

[[nodiscard]] inline Expr make(OpCode op, const Expr &l, const Expr &r) {
  Builder *b = detail::pick(l, r);
  if (b == nullptr) {
    return Expr{apply2(op, l.literal(), r.literal())};
  }
  return Expr{*b, b->make(op, l.id(*b), r.id(*b))};
}

[[nodiscard]] inline Expr operator+(const Expr &l, const Expr &r) {
  return make(OpCode::Add, l, r);
}
[[nodiscard]] inline Expr operator*(const Expr &l, const Expr &r) {
  return make(OpCode::Mul, l, r);
}
[[nodiscard]] inline Expr operator/(const Expr &l, const Expr &r) {
  return make(OpCode::Div, l, r);
}
[[nodiscard]] inline Expr operator-(const Expr &u) {
  return make(OpCode::Neg, u);
}
[[nodiscard]] inline Expr operator-(const Expr &l, const Expr &r) {
  return l + (-r);
}

#define DDX_RT_UNFN(fn, Op, label)                                             \
  [[nodiscard]] inline Expr fn(const Expr &u) { return make(OpCode::Op, u); }
DDX_UNARY_MATH_TABLE(DDX_RT_UNFN)
DDX_RT_UNARY_TABLE(DDX_RT_UNFN)
#undef DDX_RT_UNFN

#define DDX_RT_BINFN(fn, Op, label)                                            \
  [[nodiscard]] inline Expr fn(const Expr &l, const Expr &r) {                 \
    return make(OpCode::Op, l, r);                                             \
  }
DDX_RT_BINARY_TABLE(DDX_RT_BINFN)
#undef DDX_RT_BINFN

// A graph plus the handles onto it.  Holding the builder by value keeps the
// lifetime obvious: an Expr never outlives the Graph it names.
[[nodiscard]] inline Expr var(Builder &b, std::string_view name) {
  return Expr{b, b.variable(name)};
}
[[nodiscard]] inline Expr lit(Builder &b, double v) {
  return Expr{b, b.constant(v)};
}

} // namespace ddx::rt

#pragma once

#include "rt/apply.hpp"
#include "rt/builder.hpp"
#include "rt/opcode.hpp"

#include <string_view>
#include <utility> // std::move

namespace ddx::rt {

// A handle onto one node, plus the arithmetic surface that builds more.  A null
// builder means the value is a literal that has not been given to a graph yet,
// which is what lets RTExpression{1} exist: CFieldLike requires
// constructible_from<int>, and the derivative rules in expr/unary_math.hpp
// manufacture exactly 0 and 1.
//
// T defaults to double but is whatever the graph is over.  Numeric admits
// matrices and quaternions, and the builder asks ddx which of the rewrites are
// still true for them rather than assuming.
template <impl::Numeric T = double> class RTExpression {
public:
  using value_type = T;
  using builder_type = Builder<T>;

  constexpr RTExpression() = default;

  // By value, not a forwarding reference: RTExpression is itself Numeric, and a
  // Numeric&& would out-match the copy constructor for a non-const lvalue.
  // Taking V by value leaves both an exact match, and the non-template wins.
  template <impl::Numeric V>
  constexpr RTExpression(V v) : lit_(static_cast<T>(std::move(v))) {}

  constexpr RTExpression(Builder<T> &b, NodeId id) noexcept
      : builder_(&b), id_(id) {}

  [[nodiscard]] constexpr bool pending() const noexcept {
    return builder_ == nullptr;
  }
  [[nodiscard]] constexpr const T &literal() const noexcept { return lit_; }
  [[nodiscard]] constexpr Builder<T> *builder() const noexcept {
    return builder_;
  }

  // Give the node an identity in `b`, materialising a pending literal.
  [[nodiscard]] constexpr NodeId id(Builder<T> &b) const {
    return builder_ ? id_ : b.constant(lit_);
  }

  // Two pending literals fold without ever reaching a graph, so the constants
  // the derivative rules produce never become nodes.
  [[nodiscard]] static constexpr RTExpression form(OpCode op,
                                                   const RTExpression &u) {
    Builder<T> *const b = u.builder();
    return b ? RTExpression{*b, b->make(op, u.id(*b))}
             : RTExpression{apply<T>(op, u.literal())};
  }

  [[nodiscard]] static constexpr RTExpression
  form(OpCode op, const RTExpression &l, const RTExpression &r) {
    Builder<T> *const b = l.builder() ? l.builder() : r.builder();
    return b ? RTExpression{*b, b->make(op, l.id(*b), r.id(*b))}
             : RTExpression{apply<T>(op, l.literal(), r.literal())};
  }

  // Hidden friends, not free templates: for a given RTExpression<T> these are
  // ordinary functions, so a bare number converts on either side and `x * 2`
  // and `2 * x` both work.  A free template would deduce T from neither.
  friend constexpr RTExpression operator+(const RTExpression &l,
                                          const RTExpression &r) {
    return form(OpCode::Add, l, r);
  }
  friend constexpr RTExpression operator*(const RTExpression &l,
                                          const RTExpression &r) {
    return form(OpCode::Mul, l, r);
  }
  friend constexpr RTExpression operator/(const RTExpression &l,
                                          const RTExpression &r) {
    return form(OpCode::Div, l, r);
  }
  friend constexpr RTExpression operator-(const RTExpression &u) {
    return form(OpCode::Neg, u);
  }
  friend constexpr RTExpression operator-(const RTExpression &l,
                                          const RTExpression &r) {
    return l + (-r);
  }

#define DDX_RT_UNFN(fn, Op, label, ...)                                        \
  friend constexpr RTExpression fn(const RTExpression &u) {                    \
    return form(OpCode::Op, u);                                                \
  }
  DDX_UNARY_MATH_TABLE(DDX_RT_UNFN)
  DDX_RT_UNARY_TABLE(DDX_RT_UNFN)
#undef DDX_RT_UNFN

#define DDX_RT_BINFN(fn, Op, label, ...)                                       \
  friend constexpr RTExpression fn(const RTExpression &l,                      \
                                   const RTExpression &r) {                    \
    return form(OpCode::Op, l, r);                                             \
  }
  DDX_RT_BINARY_TABLE(DDX_RT_BINFN)
#undef DDX_RT_BINFN

private:
  Builder<T> *builder_ = nullptr;
  NodeId id_ = no_node;
  T lit_{};
};

template <impl::Numeric T>
[[nodiscard]] constexpr RTExpression<T> make(OpCode op,
                                             const RTExpression<T> &u) {
  return RTExpression<T>::form(op, u);
}
template <impl::Numeric T>
[[nodiscard]] constexpr RTExpression<T>
make(OpCode op, const RTExpression<T> &l, const RTExpression<T> &r) {
  return RTExpression<T>::form(op, l, r);
}

template <impl::Numeric T>
[[nodiscard]] constexpr RTExpression<T> var(Builder<T> &b,
                                            std::string_view name) {
  return RTExpression<T>{b, b.variable(name)};
}
template <impl::Numeric T>
[[nodiscard]] constexpr RTExpression<T> lit(Builder<T> &b, const T &v) {
  return RTExpression<T>{b, b.constant(v)};
}

} // namespace ddx::rt

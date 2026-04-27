#pragma once

#include <concepts>
#include <ostream>
#include <tuple>
#include <type_traits>

// ===========================================================================
// Concepts
// ===========================================================================

/// Numeric: any scalar type that supports arithmetic operations.
template <typename T>
concept Numeric = std::is_arithmetic_v<T> || requires(T a, T b) {
  T{};
  { a + b } -> std::convertible_to<T>;
  { a - b } -> std::convertible_to<T>;
  { a * b } -> std::convertible_to<T>;
  { a / b } -> std::convertible_to<T>;
  { -a } -> std::convertible_to<T>;
};

/// AnOp: any operation struct that carries a Numeric value_type.
template <typename O>
concept AnOp =
    requires { typename O::value_type; } && Numeric<typename O::value_type>;

/// SymbolicExpr: structural concept satisfied by any expression-tree node.
template <typename E>
concept SymbolicExpr = requires(const E e) {
  typename E::value_type;
  { static_cast<typename E::value_type>(e) };
  { e.derivative() };
};

// ===========================================================================
// Forward declarations (Op constrained; LHS/RHS/Exp left open so raw scalars
// can still be passed directly to factory functions in existing tests).
// ===========================================================================
template <AnOp Op, typename LHS, typename RHS> class Expression;
template <AnOp Op, typename Exp> class MonoExpression;
template <Numeric T> class Constant;
template <Numeric T, char> class Variable;

// ===========================================================================
// Tag trait: true for any first-class expression node.
// ===========================================================================
template <typename T> struct is_expression_type : std::false_type {};
template <Numeric T> struct is_expression_type<Constant<T>> : std::true_type {};

template <Numeric T, char C>
struct is_expression_type<Variable<T, C>> : std::true_type {};

template <AnOp Op, typename LHS, typename RHS>
struct is_expression_type<Expression<Op, LHS, RHS>> : std::true_type {};

template <AnOp Op, typename Exp>
struct is_expression_type<MonoExpression<Op, Exp>> : std::true_type {};

/// Tag-based concept — complements the structural SymbolicExpr above.
template <typename T>
concept ExpressionConcept = is_expression_type<std::remove_cvref_t<T>>::value;

// ===========================================================================
// BaseExpression: injects value_type from the Op into derived classes.
// ===========================================================================
template <AnOp Op> struct BaseExpression {
  using value_type = typename Op::value_type;
};

// ===========================================================================
// MonoExpression — unary expression node.
// ===========================================================================
template <AnOp Op, typename Exp>
class MonoExpression : public BaseExpression<Op> {
  Exp expression;
  friend std::ostream &operator<<(std::ostream &out, const MonoExpression &e) {
    out << '(' << e.expression << ')';
    return out;
  }

public:
  [[nodiscard]] constexpr const auto &expressions() const { return expression; }
  using lhs_type = Exp;
  using value_type = typename BaseExpression<Op>::value_type;
  constexpr MonoExpression(Exp expr) : expression{std::move(expr)} {}
  [[nodiscard]] constexpr auto derivative() const {
    return Op::derivative(expression);
  }
  constexpr operator value_type() const { return eval(); }
  [[nodiscard]] constexpr auto eval() const { return Op::eval(expression); }
  constexpr void update(const auto &symbols, const auto &updates) {
    expression.update(symbols, updates);
  }
};

// ===========================================================================
// Expression — binary expression node.
// ===========================================================================
template <AnOp Op, typename LHS, typename RHS>
class Expression : public BaseExpression<Op> {
  std::pair<LHS, RHS> inner_expressions;
  friend std::ostream &operator<<(std::ostream &out, const Expression &e) {
    out << '(';
    std::apply([&out](const auto &...e) { Op::print(out, e...); },
               e.inner_expressions);
    out << ')';
    return out;
  }

public:
  using op_type = Op;
  using lhs_type = LHS;
  using rhs_type = RHS;
  using value_type = typename BaseExpression<Op>::value_type;
  [[nodiscard]] constexpr const auto &expressions() const {
    return inner_expressions;
  }
  constexpr Expression(LHS lhs, RHS rhs)
      : inner_expressions({std::move(lhs), std::move(rhs)}) {}
  [[nodiscard]] constexpr auto eval() const {
    return std::apply([](const auto &...e) { return Op::eval(e...); },
                      inner_expressions);
  }
  constexpr operator value_type() const { return eval(); }
  [[nodiscard]] constexpr auto derivative() const {
    return std::apply([](const auto &...e) { return Op::derivative(e...); },
                      inner_expressions);
  }
  constexpr void update(const auto &symbols, const auto &updates) {
    std::apply([&](auto &...e) { (e.update(symbols, updates), ...); },
               inner_expressions);
  }
};

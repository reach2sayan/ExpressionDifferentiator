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
// EvalResult<T> — a pre-computed value that satisfies ExpressionConcept so
// it can be passed to Op::eval without those impls needing modification.
// eval_seeded uses this to inject seeded values into one recursive pass
// without calling update() + eval() separately.
// ===========================================================================
template <Numeric T> struct EvalResult {
  using value_type = T;
  T value;
  [[nodiscard]] constexpr T eval() const { return value; }
  constexpr operator T() const { return value; }
};

template <Numeric T>
struct is_expression_type<EvalResult<T>> : std::true_type {};

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

  template <std::size_t I> [[nodiscard]] constexpr auto get() const {
    static_assert(I < 2);
    if constexpr (requires { std::tuple_size<value_type>::value; })
      return eval().template get<I>(); // Dual path
    else if constexpr (I == 0)
      return eval(); // value
    else
      return static_cast<value_type>(derivative()); // derivative
  }

  [[nodiscard]] constexpr auto derivative() const {
    return Op::derivative(expression);
  }
  constexpr operator value_type() const { return eval(); }
  [[nodiscard]] constexpr auto eval() const { return Op::eval(expression); }

  template <typename Syms, std::size_t N>
  [[nodiscard]] constexpr auto
  eval_seeded(const std::array<value_type, N> &vals) const {
    return Op::eval(
        EvalResult<value_type>{expression.template eval_seeded<Syms>(vals)});
  }

  constexpr void update(const auto &symbols, const auto &updates) {
    expression.update(symbols, updates);
  }
  constexpr void backward(const auto &syms, value_type adj, auto &grads) const {
    Op::backward(expression, adj, syms, grads);
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

  template <std::size_t I> [[nodiscard]] constexpr auto get() const {
    static_assert(I < 2);
    if constexpr (requires { std::tuple_size<value_type>::value; })
      return eval().template get<I>(); // Dual path
    else if constexpr (I == 0)
      return eval(); // value
    else
      return static_cast<value_type>(derivative()); // derivative
  }

  [[nodiscard]] constexpr auto eval() const {
    return std::apply([](const auto &...e) { return Op::eval(e...); },
                      inner_expressions);
  }
  constexpr operator value_type() const { return eval(); }
  [[nodiscard]] constexpr auto derivative() const {
    return std::apply([](const auto &...e) { return Op::derivative(e...); },
                      inner_expressions);
  }

  template <typename Syms, std::size_t N>
  [[nodiscard]] constexpr auto
  eval_seeded(const std::array<value_type, N> &vals) const {
    return Op::eval(
        EvalResult<value_type>{
            inner_expressions.first.template eval_seeded<Syms>(vals)},
        EvalResult<value_type>{
            inner_expressions.second.template eval_seeded<Syms>(vals)});
  }

  constexpr void update(const auto &symbols, const auto &updates) {
    std::apply([&](auto &...e) { (e.update(symbols, updates), ...); },
               inner_expressions);
  }
  constexpr void backward(const auto &syms, value_type adj, auto &grads) const {
    std::apply([&](const auto &...e) { Op::backward(e..., adj, syms, grads); },
               inner_expressions);
  }
};

// ===========================================================================
// Structured-binding support: size=2 for all expression types.
// Element type delegates to value_type when it's tuple-like (e.g. Dual<T>),
// otherwise both elements are value_type ({eval, derivative}).
// ===========================================================================

// ===========================================================================
// Helper: selects the structured-binding element type for an expression.
//   - When value_type is tuple-like (e.g. Dual<T>): delegate to it.
//   - Otherwise: both elements are value_type (eval / derivative.eval).
// ===========================================================================
template <typename V, std::size_t I, typename = void>
struct expression_element {
  using type = V;
};

template <typename V, std::size_t I>
struct expression_element<
    V, I, std::void_t<typename std::tuple_element<I, V>::type>> {
  using type = std::tuple_element_t<I, V>;
};

namespace std {
template <AnOp Op, typename LHS, typename RHS>
struct tuple_size<Expression<Op, LHS, RHS>> : integral_constant<size_t, 2> {};

template <size_t I, AnOp Op, typename LHS, typename RHS>
struct tuple_element<I, Expression<Op, LHS, RHS>> {
  using type =
      typename expression_element<typename Expression<Op, LHS, RHS>::value_type,
                                  I>::type;
};

template <AnOp Op, typename Exp>
struct tuple_size<MonoExpression<Op, Exp>> : integral_constant<size_t, 2> {};

template <size_t I, AnOp Op, typename Exp>
struct tuple_element<I, MonoExpression<Op, Exp>> {
  using type =
      typename expression_element<typename MonoExpression<Op, Exp>::value_type,
                                  I>::type;
};
} // namespace std

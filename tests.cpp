#include "equation.hpp"
#include "operations.hpp"
#include "traits.hpp"
#include "values.hpp"

#include <boost/mp11.hpp>
#include <gtest/gtest.h>
#include <numbers>
#include <ranges>

// ===========================================================================
// Concept satisfaction (static_assert — compile-time contract tests)
// ===========================================================================

TEST(ConceptTest, NumericSatisfied) {
  static_assert(Numeric<int>);
  static_assert(Numeric<double>);
  static_assert(Numeric<float>);
  static_assert(Numeric<long double>);
  static_assert(!Numeric<std::string>);
}

TEST(ConceptTest, SymbolicExprSatisfied) {
  static_assert(SymbolicExpr<Constant<double>>);
  static_assert(SymbolicExpr<Variable<double, 'x'>>);
  using SumExpr = decltype(std::declval<Variable<double, 'x'>>() +
                           std::declval<Constant<double>>());
  static_assert(SymbolicExpr<SumExpr>);
}

TEST(ConceptTest, ExpressionConceptSatisfied) {
  static_assert(ExpressionConcept<Constant<int>>);
  static_assert(ExpressionConcept<Variable<double, 'x'>>);
  static_assert(!ExpressionConcept<int>);
  static_assert(!ExpressionConcept<double>);
}

TEST(ConceptTest, AnOpSatisfied) {
  static_assert(AnOp<SumOp<double>>);
  static_assert(AnOp<MultiplyOp<float>>);
  static_assert(AnOp<SineOp<double>>);
  static_assert(AnOp<CosineOp<double>>);
  static_assert(AnOp<ExpOp<double>>);
  static_assert(AnOp<NegateOp<int>>);
  static_assert(AnOp<DivideOp<double>>);
}

// ===========================================================================
// Symbol extraction
// ===========================================================================

TEST(SymbolTest, SingleVariable) {
  using E = Variable<double, 'x'>;
  using Syms = extract_symbols_from_expr<E>::type;
  static_assert(boost::mp11::mp_size<Syms>::value == 1);
  static_assert(std::is_same_v<boost::mp11::mp_at_c<Syms, 0>,
                               std::integral_constant<char, 'x'>>);
}

TEST(SymbolTest, TwoVariables) {
  using E = decltype(std::declval<Variable<double, 'x'>>() *
                     std::declval<Variable<double, 'y'>>());
  using Syms = extract_symbols_from_expr<E>::type;
  static_assert(boost::mp11::mp_size<Syms>::value == 2);
  // Symbols are sorted by char value: 'x' < 'y'
  static_assert(std::is_same_v<boost::mp11::mp_at_c<Syms, 0>,
                               std::integral_constant<char, 'x'>>);
  static_assert(std::is_same_v<boost::mp11::mp_at_c<Syms, 1>,
                               std::integral_constant<char, 'y'>>);
}

TEST(SymbolTest, DuplicateSymbolsDeduplicated) {
  // x * x has only one distinct symbol
  using E = decltype(std::declval<Variable<double, 'x'>>() *
                     std::declval<Variable<double, 'x'>>());
  using Syms = extract_symbols_from_expr<E>::type;
  static_assert(boost::mp11::mp_size<Syms>::value == 1);
}

TEST(SymbolTest, ThreeVariables) {
  auto x = PV(1.0, 'x');
  auto y = PV(2.0, 'y');
  auto z = PV(3.0, 'z');
  auto expr = x + y + z;
  using Syms = extract_symbols_from_expr<decltype(expr)>::type;
  static_assert(boost::mp11::mp_size<Syms>::value == 3);
}

// ===========================================================================
// Expression / operator tests
// ===========================================================================

TEST(ExpressionTest, StaticTests) {
  static_assert(
      std::is_same_v<
          as_const_expression<
              Expression<MultiplyOp<int>, Variable<int, 'x'>, Constant<int>>>,
          Expression<MultiplyOp<int>, Constant<int>, Constant<int>>>);

  static_assert(
      std::is_same_v<
          as_const_expression<Expression<MultiplyOp<int>, Variable<int, 'x'>,
                                         Variable<int, 'y'>>>,
          Expression<MultiplyOp<int>, Constant<int>, Constant<int>>>);

  auto x = 4_vi;
  auto y = 2_vi;
  auto c = 2_ci;
  auto res = x * y + c;
  auto res2 = make_const_variable<'c'>(res);
  ASSERT_EQ(res2, res);
}

TEST(ExpressionTest, SumTest) {
  double a = 1, b = 2, c = 3;
  auto sum_exp = a +  b + c;
  ASSERT_EQ(sum_exp, 6);
}

TEST(ExpressionTest, MultiplyTest) {
  auto a = 1_ci;
  auto b = 2_vi;
  auto c = 3_ci;
  auto sum_exp = a * b * c;
  auto d = sum_exp.derivative();
  ASSERT_EQ(sum_exp, 6);
  ASSERT_EQ(d, 3);
}

TEST(ExpressionTest, SubtractTest) {
  auto a = 1_ci;
  auto b = 2_vi;
  auto minus = a - b;
  auto d = minus.derivative();
  ASSERT_EQ(minus, -1);
  ASSERT_EQ(d, -1);
}

TEST(ExpressionTest, DivideTest) {
  auto a = 4.0_vd;
  auto b = 2.0_cd;
  auto divide = a / b;
  auto d = divide.derivative();
  ASSERT_EQ(divide, 2.0);
  ASSERT_EQ(d, 0.5);
}

TEST(ExpressionTest, ExpTest) {
  constexpr auto exp_exp = exp(2.0_cd);
  ASSERT_EQ(exp_exp, std::exp(2.0));
}

TEST(ExpressionTest, ExpSum) {
  constexpr auto target = exp(1_cd + 2_cd);
  ASSERT_EQ(target, std::exp(3.0));
}

TEST(ExpressionTest, ExpDerivative) {
  for (auto i : std::ranges::iota_view{1, 1000}) {
    auto target = exp(Variable<double, 'x'>{i * 1.0});
    ASSERT_EQ(target.derivative(), target);
  }
}

TEST(ExpressionTest, Combination) {
  double target = exp(1_cd + 2_cd + 3_cd) +  1_cd;
  ASSERT_EQ(target, std::exp(6.0) + 1.0);
}

TEST(ExpressionTest, ConstantDerivative) {
  auto target = Constant<int>(1);
  ASSERT_EQ(target.derivative(), 0);
  ASSERT_EQ(target, 1);
}

TEST(ExpressionTest, VariableDerivative) {
  auto x = Variable<int, 'x'>{5};
  ASSERT_EQ(x.derivative(), 1);
  ASSERT_EQ(x, 5);
}

TEST(ExpressionTest, DerivativeTest) {
  auto x = 4_vi;
  auto expr = x * 2_ci;
  auto derv = expr.derivative();
  ASSERT_EQ(expr, 8);
  ASSERT_EQ(derv, 2);
}

// ===========================================================================
// Algebraic derivative rules
// ===========================================================================

TEST(DerivativeRuleTest, SumRule) {
  // d/dx [f + g] = f' + g'  =>  d/dx [3x + 5] = 3
  auto x = PV(7,'x');
  auto expr = 3_ci * x + 5_ci;
  ASSERT_EQ(expr.derivative(), 3);
}

TEST(DerivativeRuleTest, ProductRule) {
  // d/dx [x * x] = 2x  at x=4 => 8
  auto x = Variable<int, 'x'>{4};
  auto expr = x * x;
  ASSERT_EQ(expr.derivative(), 8);
}

TEST(DerivativeRuleTest, QuotientRule) {
  // d/dx [x / c] = 1/c  at x=6, c=3 => 1/3
  auto x = Variable<double, 'x'>{6.0};
  auto c = Constant<double>{3.0};
  auto expr = x / c;
  ASSERT_DOUBLE_EQ(expr.derivative(), 1.0 / 3.0);
}

TEST(DerivativeRuleTest, ChainRule_ExpOfLinear) {
  // d/dx [e^(2x)] = 2*e^(2x)  at x=1
  auto x = Variable<double, 'x'>{1.0};
  auto inner = PC(2.0) * x;
  auto expr = exp(inner);
  ASSERT_DOUBLE_EQ(expr.derivative().eval(), 2.0 * std::exp(2.0));
}

TEST(DerivativeRuleTest, ChainRule_SinOfLinear) {
  // d/dx [sin(3x)] = 3*cos(3x)  at x=0
  auto x = Variable<double, 'x'>{0.0};
  auto inner = PC(3.0) * x;
  auto expr = sin(inner);
  ASSERT_DOUBLE_EQ(expr.derivative().eval(), 3.0 * std::cos(0.0));
}

// ===========================================================================
// Variable tests
// ===========================================================================

TEST(VariableTest, GetValue) {
  auto a = 2_vi;
  ASSERT_EQ(a, 2);
}

TEST(VariableTest, Assign) {
  Variable<int, 'a'> a{4};
  a = 2;
  ASSERT_EQ(a, 2);
}

TEST(VariableTest, UdlCompAndAssign) {
  Variable<int, 'a'> a{4};
  auto b = 4_vi;
  ASSERT_EQ(a, b);
}

// ===========================================================================
// Trig tests
// ===========================================================================

TEST(TrigTest, SinTest) {
  auto b = sin(PC(0.5));
  ASSERT_EQ(b, std::sin(0.5));
}

TEST(TrigTest, CosTest) {
  auto b = cos(PV(0.45, 'x'));
  ASSERT_EQ(b, std::cos(0.45));
  ASSERT_DOUBLE_EQ(b.derivative().eval(), -std::sin(0.45));
}

TEST(TrigTest, SinDerivative) {
  auto x = PV(0.7, 'x');
  auto s = sin(x);
  ASSERT_DOUBLE_EQ(s.derivative().eval(), std::cos(0.7));
}

TEST(TrigTest, SinCosIdentity) {
  // sin^2(x) + cos^2(x) == 1
  for (double v : {0.0, 0.5, 1.0, std::numbers::pi / 4}) {
    auto x = Variable<double, 'x'>{v};
    auto s = sin(x);
    auto c = cos(x);
    double lhs = static_cast<double>(s * s) + static_cast<double>(c * c);
    ASSERT_NEAR(lhs, 1.0, 1e-12);
  }
}

TEST(TrigTest, CosDerivativeIsNegSin) {
  // d/dx cos(x) = -sin(x)  at several points
  for (double v : {0.0, 0.3, 1.0, 2.0}) {
    auto x = Variable<double, 'x'>{v};
    ASSERT_DOUBLE_EQ(cos(x).derivative().eval(), -std::sin(v));
  }
}

TEST(TrigTest, ExpDerivativeIsItself) {
  // d/dx e^x = e^x
  for (double v : {-1.0, 0.0, 0.5, 1.5}) {
    auto x = Variable<double, 'x'>{v};
    ASSERT_DOUBLE_EQ(exp(x).derivative().eval(), std::exp(v));
  }
}

// ===========================================================================
// Equation (partial differentiation) tests
// ===========================================================================

TEST(EquationTest, SingleVariable) {
  auto x = PV(4, 'x');
  auto expr = x * 2_ci;
  auto eq = Equation(expr);
  ASSERT_EQ(eq[IDX(1)], 2);
}

TEST(EquationTest, TwoVariables) {
  auto x = PV(4, 'x');
  auto y = PV(2, 'y');
  auto expr = x * y;
  auto eq = Equation(expr);
  ASSERT_EQ(eq[IDX(1)], 2); // df/dx = y = 2
  ASSERT_EQ(eq[IDX(2)], 4); // df/dy = x = 4
}

TEST(EquationTest, LinearCombination) {
  auto x = PV(4, 'x');
  auto y = PV(2, 'y');
  auto expr = PC(1) * x + PC(2) * y;
  auto eq = Equation(expr);
  ASSERT_EQ(eq[IDX(1)], 1);
  ASSERT_EQ(eq[IDX(2)], 2);
}

TEST(EquationTest, DifferenceOfSquares) {
  constexpr auto x = PV(4, 'x');
  constexpr auto y = PV(2, 'y');
  constexpr auto expr = (x + y) * (x - y);
  constexpr auto eq = Equation(expr);
  auto [d1, d2] = eq.eval_derivatives();
  ASSERT_EQ(expr, 12); // (4+2)*(4-2) = 12
  ASSERT_EQ(d1, 8);    // 2x = 8
  ASSERT_EQ(d2, -4);   // -2y = -4
}

TEST(EquationTest, ExpEquation) {
  auto x = PV(1.0, 'x');
  auto expr = exp(x);
  auto eq = Equation(expr);
  ASSERT_DOUBLE_EQ(eq.eval(), std::exp(1.0));
  ASSERT_DOUBLE_EQ(eq[IDX(1)].eval(), std::exp(1.0)); // d(e^x)/dx = e^x
}

TEST(EquationTest, TrigEquation) {
  auto x = PV(0.5, 'x');
  auto expr = sin(x) * cos(x);
  auto eq = Equation(expr);
  // d/dx [sin(x)cos(x)] = cos^2(x) - sin^2(x) = cos(2x)
  ASSERT_DOUBLE_EQ(eq[IDX(1)].eval(), std::cos(2 * 0.5));
}

TEST(EquationTest, IdxEquivalence) {
  // idx<N>() and IDX(N) must produce the same index object
  auto x = PV(3, 'x');
  auto y = PV(5, 'y');
  auto eq = Equation(x * y);
  ASSERT_EQ(eq[idx<1>()], eq[IDX(1)]);
  ASSERT_EQ(eq[idx<2>()], eq[IDX(2)]);
}

TEST(EquationTest, ThreeVariablePartials) {
  // f(x,y,z) = x*y + y*z  at (2,3,4)
  // df/dx = y = 3, df/dy = x + z = 6, df/dz = y = 3
  auto x = PV(2, 'x');
  auto y = PV(3, 'y');
  auto z = PV(4, 'z');
  auto expr = x * y + y * z;
  auto eq = Equation(expr);
  auto [dx, dy, dz] = eq.eval_derivatives();
  ASSERT_EQ(dx, 3);
  ASSERT_EQ(dy, 6);
  ASSERT_EQ(dz, 3);
}

TEST(EquationTest, UpdateAndReevaluate) {
  // f(x) = x^2,  df/dx = 2x.
  // Both copies of x in the derivative are live variables, so update
  // propagates.
  auto x = Variable<int, 'x'>{3};
  auto expr = x * x;
  auto eq = Equation(expr);

  ASSERT_EQ(eq.eval(), 9);
  ASSERT_EQ(eq[IDX(1)].eval(), 6); // 2*3 = 6

  using Syms = Equation<decltype(expr)>::symbols;
  eq.update(Syms{}, std::array{5});
  ASSERT_EQ(eq.eval(), 25);
  ASSERT_EQ(eq[IDX(1)].eval(), 10); // 2*5 = 10
}

TEST(EquationTest, MixedTrigExpEquation) {
  // f(x) = e^x * sin(x)  at x=1
  // f'(x) = e^x*(sin(x) + cos(x))
  auto x = PV(1.0, 'x');
  auto expr = exp(x) * sin(x);
  auto eq = Equation(expr);
  double expected = std::exp(1.0) * (std::sin(1.0) + std::cos(1.0));
  ASSERT_DOUBLE_EQ(eq[IDX(1)].eval(), expected);
}

TEST(EquationTest, NumberOfDerivatives) {
  auto x = PV(1, 'x');
  auto y = PV(2, 'y');
  static_assert(Equation<decltype(x * y)>::number_of_derivatives == 2);
  static_assert(Equation<decltype(x * x)>::number_of_derivatives == 1);
}

// ===========================================================================
// VectorEquation — f: ℝⁿ → ℝᵐ  (Jacobian tests)
// ===========================================================================

TEST(VectorEquationTest, Dimensions) {
  auto x = PV(1, 'x');
  auto y = PV(2, 'y');
  // f: ℝ² → ℝ²
  using VE = VectorEquation<decltype(x + y), decltype(x * y)>;
  static_assert(VE::output_dim == 2);
  static_assert(VE::input_dim  == 2);
}

TEST(VectorEquationTest, Eval) {
  // f(x,y) = (x + y,  x * y)  at (3, 4)  =>  (7, 12)
  auto x = PV(3, 'x');
  auto y = PV(4, 'y');
  auto ve = VectorEquation(x + y, x * y);
  auto v = ve.eval();
  ASSERT_EQ(v[0], 7);
  ASSERT_EQ(v[1], 12);
}

TEST(VectorEquationTest, JacobianLinear) {
  // f(x,y) = (x + y,  x * y)  at (3, 4)
  // J = [[1, 1],
  //      [y, x]] = [[1, 1], [4, 3]]
  auto x = PV(3, 'x');
  auto y = PV(4, 'y');
  auto ve = VectorEquation(x + y, x * y);
  auto J = ve.eval_jacobian();
  ASSERT_EQ(J[0][0], 1);  // ∂(x+y)/∂x
  ASSERT_EQ(J[0][1], 1);  // ∂(x+y)/∂y
  ASSERT_EQ(J[1][0], 4);  // ∂(x*y)/∂x = y = 4
  ASSERT_EQ(J[1][1], 3);  // ∂(x*y)/∂y = x = 3
}

TEST(VectorEquationTest, JacobianWithTrig) {
  // f(x,y) = (x*y,  sin(x) + y*y)  at (2.0, 3.0)
  // J = [[y,      x   ],
  //      [cos(x), 2y  ]]
  auto x = PV(2.0, 'x');
  auto y = PV(3.0, 'y');
  auto ve = VectorEquation(x * y, sin(x) + y * y);
  auto J = ve.eval_jacobian();
  ASSERT_DOUBLE_EQ(J[0][0], 3.0);          // ∂(x*y)/∂x = y
  ASSERT_DOUBLE_EQ(J[0][1], 2.0);          // ∂(x*y)/∂y = x
  ASSERT_DOUBLE_EQ(J[1][0], std::cos(2.0)); // ∂(sin(x)+y²)/∂x
  ASSERT_DOUBLE_EQ(J[1][1], 6.0);           // ∂(sin(x)+y²)/∂y = 2y
}

TEST(VectorEquationTest, SingleComponentIsGradient) {
  // VectorEquation with one component is just the gradient of a scalar.
  auto x = PV(2.0, 'x');
  auto y = PV(3.0, 'y');
  auto ve = VectorEquation(x * y);           // f: ℝ² → ℝ¹
  auto eq = Equation(x * y);
  auto J  = ve.eval_jacobian();
  auto g  = eq.eval_derivatives();
  static_assert(decltype(ve)::output_dim == 1);
  static_assert(decltype(ve)::input_dim  == 2);
  ASSERT_DOUBLE_EQ(J[0][0], g[0]);          // ∂(x*y)/∂x
  ASSERT_DOUBLE_EQ(J[0][1], g[1]);          // ∂(x*y)/∂y
}

TEST(VectorEquationTest, SymbolUnionAcrossComponents) {
  // f0 depends only on x,  f1 depends only on y.
  // Jacobian should be 2×2 with zeros off the diagonal.
  auto x = PV(4.0, 'x');
  auto y = PV(3.0, 'y');
  auto ve = VectorEquation(x * x, y * y);   // (x², y²)
  static_assert(decltype(ve)::input_dim == 2);
  auto J = ve.eval_jacobian();
  ASSERT_DOUBLE_EQ(J[0][0], 8.0);   // ∂(x²)/∂x = 2x = 8
  ASSERT_DOUBLE_EQ(J[0][1], 0.0);   // ∂(x²)/∂y = 0
  ASSERT_DOUBLE_EQ(J[1][0], 0.0);   // ∂(y²)/∂x = 0
  ASSERT_DOUBLE_EQ(J[1][1], 6.0);   // ∂(y²)/∂y = 2y = 6
}

TEST(VectorEquationTest, ThreeOutputs) {
  // f(x,y) = (x², x*y, y²)  — Jacobian is 3×2
  auto x = PV(2.0, 'x');
  auto y = PV(5.0, 'y');
  auto ve = VectorEquation(x * x, x * y, y * y);
  static_assert(decltype(ve)::output_dim == 3);
  static_assert(decltype(ve)::input_dim  == 2);
  auto J = ve.eval_jacobian();
  ASSERT_DOUBLE_EQ(J[0][0], 4.0);   // 2x
  ASSERT_DOUBLE_EQ(J[0][1], 0.0);   // 0
  ASSERT_DOUBLE_EQ(J[1][0], 5.0);   // y
  ASSERT_DOUBLE_EQ(J[1][1], 2.0);   // x
  ASSERT_DOUBLE_EQ(J[2][0], 0.0);   // 0
  ASSERT_DOUBLE_EQ(J[2][1], 10.0);  // 2y
}

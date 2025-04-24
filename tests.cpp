//
// Created by sayan on 4/13/25.
//

#include "derivative.hpp"
#include "equation.hpp"
#include "operations.hpp"
#include "procvar.hpp"
#include "soequations.hpp"
#include "traits.hpp"
#include "values.hpp"

#include <gtest/gtest.h>
#include <ranges>

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
  auto sum_exp = Sum<int>(a, Sum<int>(b, c));
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
  constexpr auto exp_exp = Exp<double>(2.0);
  ASSERT_EQ(exp_exp, std::exp(2.0));
}

TEST(ExpressionTest, ExpSum) {
  constexpr auto target = Exp<double>(Sum<int>(1, 2));
  ASSERT_EQ(target, std::exp(3.0));
}

TEST(ExpressionTest, ExpDerivative) {
  for (auto i : std::ranges::iota_view{1,1000}) {
    auto target = Exp<double>(Variable<double,'x'>{i * 1.0});
    auto res = target.derivative();
    ASSERT_EQ(target.derivative(), target);
  }
}

TEST(ExpressionTest, Combination) {
  double target = Sum<double>(Exp<double>(Sum<int>(1, Sum<int>(2, 3))), 1);
  ASSERT_EQ(target, std::exp(6.0)+1.0);
}

TEST(ExpressionTest, ConstantTest) {
  auto target = Constant<int>(1);
  auto derv = target.derivative();
  ASSERT_EQ(derv, 0);
  ASSERT_EQ(target, 1);
}

TEST(ExpressionTest, VariableTest) {
  auto target = Constant<int>(1);
  auto derv = target.derivative();
  ASSERT_EQ(derv, 0);
  ASSERT_EQ(target, 1);
}

TEST(ExpressionTest, DerivativeTest) {
  auto x = 4_vi;
  auto expr = x * 2_ci;
  auto target = 8_ci;
  auto derv = expr.derivative();
  ASSERT_EQ(expr, target);
  ASSERT_EQ(derv, 2);
}

TEST(ProcVarTest, GetValue) {
  auto a = 2_vi;
  ASSERT_EQ(a, 2);
}

TEST(ProcVarTest, SpecifyValue) {
  Variable<int, 'a'> a{4};
  a = 2;
  ASSERT_NE(a, 4);
}

TEST(ProcVarTest, UdlCompAndAssign) {
  Variable<int, 'a'> a{4};
  auto b = 4_vi;
  ASSERT_EQ(a, b);
}

TEST(ProcVarTest, FixedToSpecifyValue) {
  Variable<int, 'a'> a{4};
  a = 2;
  ASSERT_EQ(a, 2);
}

TEST(ProcVarTest, Assign) {
  ProcessVar<double> pv(42.0);
  auto x = pv.as_variable<'x'>();
  ASSERT_EQ(x.get().get(), 42.0);
  pv.set_value(3.0);
  ASSERT_EQ(x.get().get(), 3.0);
  x = 33.0;
  auto k = pv.as_const();
  // k = 12.0; // shouldn't compile
  ASSERT_EQ(pv.get_value(), 33.0);
}

TEST(TrigTest, SinTest) {
  auto b = sin(PC(0.5));
  ASSERT_EQ(b, std::sin(0.5));
}

TEST(TrigTest, CosTest) {
  auto b = cos(PV(0.45, 'x'));
  ASSERT_EQ(b, std::cos(0.45));
  ASSERT_EQ(b.derivative().eval(), -std::sin(0.45));
}

TEST(EquationTest, DerivativeStatic) {
  constexpr auto a = 1_ci;
  constexpr Variable<int, 'x'> b{2};
  constexpr Variable<int, 'y'> c{3};
  constexpr auto sum_exp = a * b * c;
  Equation eq{sum_exp};
  // Derivative d{eq.get_expression()};
}

TEST(EquationTest, DerivativeTest1) {
  auto x = PV(4, 'x');
  auto y = PV(2, 'y');
  auto expr = x * y;
  auto eq = Equation(expr);
  // auto derivs = eq.get_derivatives();
  // auto dcount = std::tuple_size_v<decltype(derivs)>;
  // ASSERT_EQ(dcount, 2);

  auto d1 = eq[IDX(1)];
  auto d2 = eq[IDX(2)];

  ASSERT_EQ(expr, 8);
  ASSERT_EQ(d1, 2);
  ASSERT_EQ(d2, 4);
}

TEST(EquationTest, DerivativeTest2) {
  auto x = PV(4, 'x');
  auto y = PV(2, 'y');
  auto c1 = PC(1);
  auto c2 = PC(2);
  auto expr = c1 * x + c2 * y;
  auto eq = Equation(expr);

  ASSERT_EQ(eq[IDX(1)], 1);
  ASSERT_EQ(eq[IDX(2)], 2);
}

TEST(EquationTest, DerivativeTest3) {
  constexpr auto x = PV(4, 'x');           // x = 4
  constexpr auto y = PV(2, 'y');           // y = 2
  constexpr auto expr = (x + y) * (x - y); // (x + y) * (x - y)
  constexpr auto eq = Equation(expr);
  auto [d1, d2] = eq.eval_derivatives();
  ASSERT_EQ(expr, 12); // (4 + 2) * (4 - 2) = 6 * 2 = 12
  ASSERT_EQ(d1, 8);    // derivative w.r.t x: 2x = 8
  ASSERT_EQ(d2, -4);   // derivative w.r.t y: -2y = -4
}

TEST(EquationTest, SampleTest) {
  constexpr auto x1 = PV(1.0, 'x'); // x = 1
  constexpr auto x2 = PV(1.0, 'y'); // y = 2
  constexpr auto x3 = PV(1.0, 'z'); // z = 3
  constexpr auto c11 = 3.0_cd;
  constexpr auto c12 = PC(-1.0);
  constexpr auto c13 = PC(-3/2);
  constexpr auto c21 = 4.0_cd;
  constexpr auto c22 = PC(-625.0);
  constexpr auto c23 = 2.0_cd;
  constexpr auto c24 = PC(-1.0);
  constexpr auto c31 = 20.0_cd;
  constexpr auto c32 = 1.0_cd;
  constexpr auto c33 = 9.0_cd;

  constexpr auto expr1 = c11 * x1 + c12 * cos(x2 * x3) - c13;
  constexpr auto expr2 = c21 * x1 * x1 + c22 * x2*x2 + c23 * x3 + c24;
  constexpr auto expr3 = c31*x3 + exp(c24*x1*x2) + c33;

  constexpr auto soe = make_system_of_equations(expr1, expr2, expr3);
  bool isq = soe.is_square;

  auto res = soe.eval();
  std::cout << "Result:\n";
  for (auto r : res) {
    std::cout << r << ", ";
  }
  std::cout << "Jacobian:\n";
  for (auto r : soe.jacobian()) {
    std::cout << r << ", ";
  }
}
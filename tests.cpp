//
// Created by sayan on 4/13/25.
//

#include "derivative.hpp"
#include "equation.hpp"
#include "operations.hpp"
#include "procvar.hpp"
#include "traits.hpp"
#include "values.hpp"
#include <gtest/gtest.h>


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
  auto exp_exp = Exp<int>(2, 4);
  ASSERT_EQ(exp_exp, 16);
}

TEST(ExpressionTest, ExpSum) {
  auto target = Exp<int>(Sum<int>(1, 2), 2);
  ASSERT_EQ(target, 9);
}

TEST(ExpressionTest, Combination) {
  auto target = Sum<int>(Exp<int>(2, Sum<int>(1, Sum<int>(2, 3))), 1);
  ASSERT_EQ(target, 65);
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

TEST(TrigTest, SinTest) {
  auto b = sin(PC(0.5));
  std::cout << b.eval() << std::endl;
  ASSERT_EQ(b, std::sin(0.5));
}

TEST(TrigTest, CosTest) {
  auto b = cos(PV(0.45,'x'));
  ASSERT_EQ(b, std::cos(0.45));
  ASSERT_EQ(b.derivative().eval(), -std::sin(0.45));
}


TEST(EquationTest, DerivativeStatic) {
  constexpr auto a = 1_ci;
  constexpr Variable<int, 'x'> b{2};
  constexpr Variable<int, 'y'> c{3};
  constexpr auto sum_exp = a * b * c;
  Equation eq{sum_exp};
  //Derivative d{eq.get_expression()};
}

TEST(EquationTest, DerivativeTest1) {
  auto x = PV(4,'x');
  auto y = PV(2,'y');
  auto expr = x * y;
  auto eq = Equation(expr);
  auto derivs = eq.get_derivatives();
  auto dcount = std::tuple_size_v<decltype(derivs)>;
  ASSERT_EQ(dcount, 2);

  auto d1 = std::get<0>(derivs);
  auto d2 = std::get<1>(derivs);

  ASSERT_EQ(expr, 8);
  ASSERT_EQ(d1, 2);
  ASSERT_EQ(d2, 4);
}

TEST(EquationTest, DerivativeTest2) {
  auto x = PV(4,'x');
  auto y = PV(2,'y');
  auto c1 = PC(1);
  auto c2 = PC(2);
  auto expr = c1*x + c2*y;
  auto eq = Equation(expr);
  auto derivs = eq.get_derivatives();
  auto dcount = std::tuple_size_v<decltype(derivs)>;
  ASSERT_EQ(dcount, 2);
  ASSERT_EQ(std::get<0>(derivs),1);
  ASSERT_EQ(std::get<1>(derivs),2);
}

TEST(EquationTest, DerivativeTest3) {
  constexpr auto x = PV(4,'x');  // x = 4
  constexpr auto y = PV(2,'y');  // y = 2
  constexpr auto expr = (x + y) * (x - y);  // (x + y) * (x - y)
  constexpr auto eq = Equation(expr);
  constexpr auto derivs = eq.get_derivatives();

  constexpr auto d1 = std::get<0>(derivs);  // derivative w.r.t x
  constexpr auto d2 = std::get<1>(derivs);  // derivative w.r.t y
  static_assert(expr == 12);
  static_assert(d1 == 8);
  static_assert(d2 == -4  );
  ASSERT_EQ(expr, 12);  // (4 + 2) * (4 - 2) = 6 * 2 = 12
  ASSERT_EQ(d1, 8);    // derivative w.r.t x: 2x = 8
  ASSERT_EQ(d2, -4);   // derivative w.r.t y: -2y = -4
}
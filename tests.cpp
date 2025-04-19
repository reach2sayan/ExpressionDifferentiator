//
// Created by sayan on 4/13/25.
//

#include "matrix.hpp"
#include "operations.hpp"
#include "procvar.hpp"
#include "traits.hpp"
#include "values.hpp"
#include <gtest/gtest.h>

std::array<int, 16> data1 = {1, 2,  3,  4,  5,  6,  7,  8,
                             9, 10, 11, 12, 13, 14, 15, 16};
std::array<int, 16> data2 = {1, 2,  3,  4,  5,  6,  7,  8,
                             9, 10, 11, 12, 13, 14, 15, 16};
auto manual_add(auto a, auto b) {
  std::array<int, 16> result;
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      result[i * 16 + j] = a[i * 16 + j] + b[i * 16 + j];
    }
  }
  return result;
}

TEST(ExpressionTest, StaticTests) {
  auto a = PV(2);
  auto b = PC(3);
  auto oter = 4.0_vd; // PV(4.0);
  auto tmp = a + b + oter;
  static_assert(tmp.var_count == 2);
  auto tmp2 = a * b;
  static_assert(std::is_same_v<
                as_const_expression<
                    Expression<MultiplyOp<int>, Variable<int>, Constant<int>>>,
                Expression<MultiplyOp<int>, Constant<int>, Constant<int>>>);

  static_assert(std::is_same_v<
                as_const_expression<
                    Expression<MultiplyOp<int>, Variable<int>, Variable<int>>>,
                Expression<MultiplyOp<int>, Constant<int>, Constant<int>>>);

  auto syms = collect_symbols(tmp);
  EXPECT_EQ(syms[0], 'b');
  EXPECT_EQ(syms[1], 'c');
  // static_assert(syms == std::array{a.symbol, oter.symbol});
}

TEST(ExpressionTest, SumTest) {
  double a = 1, b = 2, c = 3;
  auto sum_exp = Sum<int>(a, Sum<int>(b, c));
  EXPECT_EQ(sum_exp, 6);
}

TEST(ExpressionTest, MultiplyTest) {
  auto a = 1_ci;
  auto b = 2_vi;
  auto c = 3_ci;
  auto sum_exp = a * b * c;
  auto d = sum_exp.derivative();
  EXPECT_EQ(sum_exp, 6);
  EXPECT_EQ(d, 3);
}

TEST(ExpressionTest, SubtractTest) {
  auto a = 1_ci;
  auto b = 2_vi;
  auto c = 3_ci;
  auto minus = a - b;
  auto d = minus.derivative();
  EXPECT_EQ(minus, -1);
  EXPECT_EQ(d, -1);
}

TEST(ExpressionTest, DivideTest) {
  auto a = 4.0_vd;
  auto b = 2.0_cd;
  auto divide = a / b;
  auto d = divide.derivative();
  EXPECT_EQ(divide, 2.0);
  EXPECT_EQ(d, 0.5);
}

TEST(ExpressionTest, ExpTest) {
  auto exp_exp = Exp<int>(2, 4);
  EXPECT_EQ(exp_exp, 16);
}

TEST(ExpressionTest, ExpSum) {
  auto target = Exp<int>(Sum<int>(1, 2), 2);
  EXPECT_EQ(target, 9);
}

TEST(ExpressionTest, Combination) {
  auto target = Sum<int>(Exp<int>(2, Sum<int>(1, Sum<int>(2, 3))), 1);
  EXPECT_EQ(target, 65);
}

TEST(ExpressionTest, ConstantTest) {
  auto target = Constant<int>(1);
  auto derv = target.derivative();
  EXPECT_EQ(derv, 0);
  EXPECT_EQ(target, 1);
}

TEST(ExpressionTest, VariableTest) {
  auto target = Constant<int>(1);
  auto derv = target.derivative();
  EXPECT_EQ(derv, 0);
  EXPECT_EQ(target, 1);
}

TEST(ExpressionTest, DerivativeTest) {
  auto x = 4_vi;
  auto expr = x * 2_ci;
  auto target = 8_ci;
  auto derv = expr.derivative();
  EXPECT_EQ(expr, target);
  EXPECT_EQ(derv, 2);
}

TEST(ProcVarTest, GetValue) {
  auto a = 2_vi;
  EXPECT_EQ(a, 2);
}

TEST(ProcVarTest, SpecifyValue) {
  Variable<int> a{4};
  a = 2;
  EXPECT_NE(a, 4);
}

TEST(ProcVarTest, UdlCompAndAssign) {
  Variable<int> a{4};
  auto b = 4_vi;
  EXPECT_EQ(a, b);
}

TEST(ProcVarTest, FixedToSpecifyValue) {
  Variable<int> a{4};
  a = 2;
  EXPECT_EQ(a, 2);
}
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
  auto oter = PV(4.0);
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
  for (auto &s : syms) {
    std::cout << s << std::endl;
  }
  // static_assert(syms == std::array{a.symbol, oter.symbol});
}

TEST(ExpressionTest, SumTest) {
  double a = 1, b = 2, c = 3;
  auto sum_exp = Sum<int>(a, Sum<int>(b, c));
  EXPECT_EQ(sum_exp, 6);
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
  Variable x{4};
  auto expr = Multiply<int>(Variable<int>(x), Constant<int>(2));
  auto target = Constant<int>(8);
  auto derv = expr.derivative();
  EXPECT_EQ(expr, target);
  EXPECT_EQ(derv, 2);
}

TEST(ProcVarTest, GetValue) {
  Variable<int> a{2};
  EXPECT_EQ(a, 2);
}

TEST(ProcVarTest, SpecifyValue) {
  Variable<int> a{4};
  a = 2;
  EXPECT_EQ(a, 2);
}

TEST(ProcVarTest, FixedToSpecifyValue) {
  Variable<int> a{4};
  a = 2;
  EXPECT_EQ(a, 2);
}
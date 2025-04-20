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

template <typename TExpression>
using derivatives_type = decltype(make_derivatives(std::declval<TExpression>()));

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
  auto c = 3_ci;
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

TEST(EquationTest, DerivativeStatic) {
  constexpr auto a = 1_ci;
  constexpr Variable<int, 'x'> b{2};
  constexpr Variable<int, 'y'> c{3};
  constexpr auto sum_exp = a * b * c;
  Equation eq{sum_exp};
  //Derivative d{eq.get_expression()};
}

TEST(EquationTest, SetUpBasic) {
  constexpr auto a = 1_ci;
  constexpr Variable<int, 'x'> b{2};
  constexpr Variable<int, 'y'> c{3};
  constexpr auto sum_exp = a * b * c;
  Equation eq{sum_exp};
  //constexpr auto labels = collect_variable_labels(sum_exp);
  //constexpr auto label_tuple = std::tuple_cat(labels);
  //constexpr auto v = make_derivatives(sum_exp);
  auto v = extract_symbols_from_expr<decltype(sum_exp)>::type{};
  //static_assert(std::is_same<decltype(v),int>::value);
  //using Result = extract_charlist<decltype(sum_exp)>::type;

  //auto v = std::tuple{make_all_constant_except<'x'>(sum_exp), make_all_constant_except<'y'>(sum_exp)};

  //static_assert(std::is_same_v<decltype(label_tuple), std::tuple<char,char>>,"failed");
  auto v3 = std::tuple{make_all_constant_except<'x'>(sum_exp), make_all_constant_except<'y'>(sum_exp)};

}

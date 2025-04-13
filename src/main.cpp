#include <iostream>

#include "matrix.hpp"
#include "operations.hpp"
#include "values.hpp"

std::array<int, 16> data1 = {1, 2,  3,  4,  5,  6,  7,  8,
                             9, 10, 11, 12, 13, 14, 15, 16};
std::array<int, 16> data2 = {1, 2,  3,  4,  5,  6,  7,  8,
                             9, 10, 11, 12, 13, 14, 15, 16};

int main() {
  matrix<int, 4, 4> m1(data1);
  matrix<int, 4, 4> m2(data2);
  auto target = Sum<int>(Constant<int>{3}, Constant<int>{1});
  auto target2 = target.derivative();
  std::cout << target.eval() << std::endl;
  std::cout << target2 << std::endl;

  Variable x{4};
  auto expr = Sum<int>(Constant<int>(3), Exp<int,Variable<int>,Constant<int>>(x, Constant(2)));
  std::cout << expr << std::endl;
  std::cout << expr.eval() << std::endl;
  auto expr2 = Multiply<int>(Variable<int>(x), Constant<int>(2));
  std::cout << expr2 << std::endl;
  std::cout << expr2.eval() << std::endl;
  std::cout << expr2.derivative() << std::endl;
  std::cout << expr2.derivative().eval() << std::endl;
  //auto derv = target.derivative();
}
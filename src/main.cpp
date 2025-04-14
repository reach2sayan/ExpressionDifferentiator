#include <iostream>

#include "matrix.hpp"
#include "procvar.hpp"

std::array<int, 16> data1 = {1, 2,  3,  4,  5,  6,  7,  8,
                             9, 10, 11, 12, 13, 14, 15, 16};
std::array<int, 16> data2 = {1, 2,  3,  4,  5,  6,  7,  8,
                             9, 10, 11, 12, 13, 14, 15, 16};

int main() {
  matrix<int, 4, 4> m1(data1);
  matrix<int, 4, 4> m2(data2);
  auto target = Sum<int>(Constant{3}, Constant{1});
  auto target2 = target.derivative();
  std::cout << target.eval() << std::endl;
  std::cout << target2 << std::endl;

  Variable x{4};
  Variable y{4};
  auto expr = Sum<int>(Sum<int>(Variable{y},
    Multiply<int>(Variable{x}, Constant{2})), Constant{2});
  std::cout << expr << std::endl;
  auto derv = expr.derivative();
  std::cout << derv << std::endl;
  std::cout << derv.eval() << std::endl;
  ProcVar<int> a{Variable<int>{2}};
  ProcVar<int> b{Constant<int>{3}};
  auto tmp = a + b;
  auto tmp2 = a * b;

  std::cout << tmp << std::endl;
  std::cout << tmp.derivative() << std::endl;
  std::cout << tmp.eval() << std::endl;

  std::cout << tmp2 << std::endl;
  std::cout << tmp2.derivative() << std::endl;
  std::cout << tmp2.eval() << std::endl;
}
#include <iostream>

#include "matrix.hpp"
#include "procvar.hpp"
#include "equation.hpp"

std::array<int, 16> data1 = {1, 2,  3,  4,  5,  6,  7,  8,
                             9, 10, 11, 12, 13, 14, 15, 16};
std::array<int, 16> data2 = {1, 2,  3,  4,  5,  6,  7,  8,
                             9, 10, 11, 12, 13, 14, 15, 16};

#define PV(x) ProcVar(x, VariableTag{})
#define PC(x) ProcVar(x, ConstantTag{})

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
  auto a = PV(2);
  auto b = PC(3);
  auto oter = PV(4.0);
  ProcVar<int> c{5, ConstantTag{}};
  auto tmp = a + b + c;
  auto tmp2 = a * b;

  std::cout << tmp << std::endl;
  std::cout << tmp.derivative() << std::endl;
  std::cout << tmp.eval() << std::endl;

  std::cout << tmp2 << std::endl;
  std::cout << tmp2.derivative() << std::endl;
  std::cout << tmp2.eval() << std::endl;

  Equation e(tmp);
  std::cout << (int)e << std::endl;
}
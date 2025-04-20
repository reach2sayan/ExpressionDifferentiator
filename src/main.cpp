
#define REMOVE_MATRIX 1
#if !defined(REMOVE_MATRIX)
#include "matrix.hpp"
#endif

template <typename... T> struct TD;

#include "equation.hpp"
#include "procvar.hpp"
#include "traits.hpp"
#include "values.hpp"
#include <iostream>
#include <string>

#define PRINT_TUP(tup) print_tup(tup)



int main() {
  auto target = Sum<int>(Constant{3}, Constant{1});
  auto target2 = target.derivative();
  // std::cout << target.eval() << std::endl;
  // std::cout << target2 << std::endl;

  Variable<int, 'x'> x{4};
  Variable<int, 'y'> y{4};
  auto expr =
      Sum<int>(Sum<int>(Variable{y}, Multiply<int>(Variable{x}, Constant{2})),
               Constant{2});
  // std::cout << expr << std::endl;
  // auto derv = expr.derivative();
  // std::cout << derv << std::endl;
  // std::cout << derv.eval() << std::endl;
  auto a = PV(2, 'a');
  auto b = PC(3);
  auto oter = PV(4, 'o');
  auto tmp = a + b + oter;
  std::cout << "a + b + oter \n= " << tmp << "\n= " << tmp.eval() << "\n";
  auto d = extract_symbols_from_expr<decltype(tmp)>::type{};
  auto k = make_derivatives(d, tmp);

  Equation e(tmp);
  std::cout << e;
  //auto derivs = e.get_derivatives();
  // TD<decltype(k), decltype(d), decltype(tmp)> td;
  /*
  auto tmp2 = a * b;
  auto tmp3 = a / b;
  std::cout << tmp3 << std::endl;
  std::cout << tmp3.derivative() << std::endl;
  std::cout << tmp3.eval() << std::endl;
  ProcVar<int> fc;
  std::cout << tmp << std::endl;
  std::cout << tmp.derivative() << std::endl;
  std::cout << tmp.eval() << std::endl;

  std::cout << tmp2 << std::endl;
  std::cout << tmp2.derivative() << std::endl;
  std::cout << tmp3.eval() << std::endl;

  Equation e(tmp);
  std::cout << (int)e << std::endl;
  */
}

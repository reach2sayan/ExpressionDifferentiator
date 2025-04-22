
#define REMOVE_MATRIX 1
#if !defined(REMOVE_MATRIX)
#include "matrix.hpp"
#endif

template <typename... T> struct TD;

#include "equation.hpp"
#include "procvar.hpp"
#include "soequations.hpp"
#include "traits.hpp"
#include "values.hpp"
#include <iostream>
#include <string>

#define PRINT_TUP(tup) print_tup(tup)

int main() {
  /*
  auto target = Sum<int>(Constant{3}, Constant{1});
  auto target2 = target.derivative();
  std::cout << target.eval() << std::endl;
  std::cout << target2 << std::endl;

  Variable<int, 'x'> x{4};
  Variable<int, 'y'> y{4};
  auto expr =
      Sum<int>(Sum<int>(Variable{y}, Multiply<int>(Variable{x}, Constant{2})),
               Constant{2});
  std::cout << expr << std::endl;
  auto derv = expr.derivative();
  std::cout << derv << std::endl;
  std::cout << derv.eval() << std::endl;
  auto a = PV(2, 'a');
  auto b = PV(3,'b');
  auto oter = PV(4, 'o');
  auto tmp = a + b + oter;
  std::cout << "a + b + oter \n= " << tmp << "\n= " << tmp.eval() << "\n";
  auto d = extract_symbols_from_expr<decltype(tmp)>::type{};
  auto k = make_derivatives(d, tmp);

  Equation e(tmp);
  std::cout << e;
  Equation e3(a-b*oter);
  std::cout <<"----------------------\n";
  Equation e2(a * b * oter);
  std::cout << e2 << std::endl;
  std::cout << e2.eval() << std::endl;
  for (auto c: e2.eval_derivatives())
    std::cout << c << ", ";
  std::cout <<"----------------------\n";
  SystemOfEquations soe(e, e2,e3);
  auto res = soe.eval();
  std::cout << "System of equations\n";
  for (auto &r : res) {
    std::cout << r << ", ";
  }
  constexpr auto sq = soe.is_square;
  auto jac = soe.jacobian();
  std::cout << "Jacobian\n";
  for (auto r : jac) {
    for (auto c : r) {
      std::cout << c << ", ";
    }
    std::cout << "\n";
  }

  constexpr auto x1 = PV(4,'y');  // x = 4
  constexpr auto y2 = PV(2,'x');  // y = 2
  constexpr auto expr1 = x1 + y2 + x1 * y2;  // (x + y) * (x - y)
  constexpr auto eq = Equation(expr1);

  decltype(eq)::symbolslist sl;
  decltype(e2)::symbolslist s2;
  */
  constexpr auto a = PV(2, 'x');
  constexpr auto b = PV(3, 'y');
  constexpr auto expr2 = a + b;
  constexpr auto x1 = PV(4, 'y');           // x = 4
  constexpr auto y2 = PV(2, 'x');           // y = 2
  constexpr auto expr1 = x1 + y2 + x1 * y2; // (x + y) * (x - y)
  auto soee = make_system_of_equations(expr1, expr2);
  std::cout << soee << "\n";
  for (auto r: soee.eval()) {
    std::cout << r << ", ";
  }
  std::cout << "\n";
  auto d = soee.jacobian();
  for (auto r : d) {
    for (auto c : r) {
      std::cout << c << ", ";
    }
    std::cout << "\n";
  }


  constexpr bool squa = soee.is_square;
  // TD<tuple_union_t<decltype(sl),decltype(s2)>> _;
}

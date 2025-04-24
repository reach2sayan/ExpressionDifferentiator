
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
  auto a = PV(2, 'x');
  auto b = PV(3, 'y');
  auto expr2 = a + b;
  auto x1 = PV(4, 'y');                        // x = 4
  auto y2 = PV(2, 'x');                        // y = 2
  auto expr1 = x1 + y2 + PC(3) * x1 * y2 * y2; // (x + y) * (x - y)
  auto soee = make_system_of_equations(expr1, expr2);
  auto soee2 = make_system_of_equations(expr1);
  std::cout << soee << "\n";
  auto result = soee.eval();
  for (auto r : result) {
    std::cout << r << ", ";
  }
  std::cout << "\n";
  auto d = soee.jacobian();
  for (auto r : d) {
    std::cout << r << ", ";
  }
  std::cout << "\nchanging\n";
  constexpr std::array<int, 2> arr = {10, 11};
  soee.update(arr);
  auto expr3 = expr1 + a;

  auto result2 = soee.eval();
  for (auto r : result2) {
    std::cout << r << ", ";
  }
  std::cout << "\n";
  auto d2 = soee.jacobian();
  for (auto r : d2) {
    std::cout << r << ", ";
  }
}

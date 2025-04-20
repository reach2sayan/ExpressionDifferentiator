
#define REMOVE_MATRIX 1
#if !defined(REMOVE_MATRIX)
#include "matrix.hpp"
#endif


template<typename... T> struct TD;

#include <iostream>
#include "equation.hpp"
#include "procvar.hpp"
#include "traits.hpp"
#include "values.hpp"
#include <string>

template<class TupType, size_t... I>
void print_tup(const TupType& _tup, std::index_sequence<I...>)
{
  std::cout << "(";
  (..., (std::cout << (I == 0? "" : ", ") << std::get<I>(_tup)));
  std::cout << ")\n";
}

template<class... T>
void print_tup (const std::tuple<T...>& _tup)
{
  print_tup(_tup, std::make_index_sequence<sizeof...(T)>());
}


int main() {
  auto target = Sum<int>(Constant{3}, Constant{1});
  auto target2 = target.derivative();
  //std::cout << target.eval() << std::endl;
  //std::cout << target2 << std::endl;

  Variable<int,'x'> x{4};
  Variable<int,'y'> y{4};
  auto expr =
      Sum<int>(Sum<int>(Variable{y}, Multiply<int>(Variable{x}, Constant{2})),
               Constant{2});
  //std::cout << expr << std::endl;
  //auto derv = expr.derivative();
  //std::cout << derv << std::endl;
  //std::cout << derv.eval() << std::endl;
  auto a = PVl(2,'a');
  auto b = PC(3);
  auto oter = PVl(4,'o');
  auto tmp = a + b + oter;
  std::cout << "a + b + oter \n= " << tmp << "\n= " << tmp.eval() << "\n";
  auto d = extract_symbols_from_expr<decltype(tmp)>::type{};
  auto k = make_derivatives(d,tmp);
  print_tup(k);

  Equation e(tmp);
  auto derivs = e.get_derivatives();
  //TD<decltype(k), decltype(d), decltype(tmp)> td;
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

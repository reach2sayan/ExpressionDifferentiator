//
// Created by sayan on 4/22/25.
//

#pragma once

#include "soequations.hpp"
#include <experimental/mdspan>

constexpr double EPSILON = 1e-10;
constexpr size_t MAX_ITERATIONS = 1000;

template <typename... TEquations> class NewtonRaphson {
  SystemOfEquations<TEquations...> soe;

  using value_type = SystemOfEquations<TEquations...>::value_type;
  constexpr static size_t number_of_equations =
      SystemOfEquations<TEquations...>::number_of_equations;

public:
  NewtonRaphson(SystemOfEquations<TEquations...> _soe) : soe{std::move(_soe)} {}
  constexpr auto get_value() const { return soe.eval(); }
  constexpr auto get_jacobian() const { return soe.jacobian(); }
  void solve();
};
template <typename... TEquations> void NewtonRaphson<TEquations...>::solve() {
  auto jac = soe.jacobian();
}

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
  const double epsilon = EPSILON;
  const size_t max_iterations = MAX_ITERATIONS;

public:
  constexpr auto invert_jacobian() const;
  NewtonRaphson(SystemOfEquations<TEquations...> _soe) : soe{std::move(_soe)} {}
  constexpr void
  update(const std::array<value_type, number_of_equations> &updates) {
    soe.update(updates);
  }
  constexpr auto get_value() const { return soe.eval(); }
  constexpr auto get_jacobian() const { return soe.jacobian(); }
  void solve();
};

template <typename... TEquations>
constexpr auto NewtonRaphson<TEquations...>::invert_jacobian() const {
  return soe.jacobian();
}

//
// Created by sayan on 4/21/25.
//

#pragma once

#include "equation.hpp"

template <typename... TEquations>
class SystemOfEquations : public TupleSupport {
private:
  std::tuple<TEquations...> equations;

public:
  constexpr static size_t number_of_equations = sizeof...(TEquations);
  constexpr explicit SystemOfEquations(TEquations... eqns)
      : equations{std::move(eqns)...} {}

  constexpr auto &get_equations() { return equations; }
  constexpr const auto &get_equations() const { return equations; }
};

namespace std {
template <typename... TEquations>
struct tuple_size<SystemOfEquations<TEquations...>>
    : std::integral_constant<std::size_t, sizeof...(TEquations)> {};

template <std::size_t N, typename... TEquations>
struct tuple_element<N, SystemOfEquations<TEquations...>> {
  using type = std::tuple_element_t<N, std::tuple<TEquations...>>;
};
} // namespace std

// Provide get<N> overloads in the same namespace as MyWrapper
template <std::size_t N, typename... TEquations>
decltype(auto) get(SystemOfEquations<TEquations...> &w) {
  return std::get<N>(w.equations);
}
template <std::size_t N, typename... TEquations>
decltype(auto) get(const SystemOfEquations<TEquations...> &w) {
  return std::get<N>(w.equations);
}

template <std::size_t N, typename... TEquations>
decltype(auto) get(SystemOfEquations<TEquations...> &&w) {
  return std::get<N>(std::move(w.equations));
}
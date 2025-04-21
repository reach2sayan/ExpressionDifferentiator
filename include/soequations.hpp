//
// Created by sayan on 4/21/25.
//

#pragma once

#include "equation.hpp"

template <typename... TEquations>
class SystemOfEquations : public TupleSupport {
private:
  std::tuple<TEquations...> equations;

  template <std::size_t N, typename... Us>
  friend decltype(auto) get(SystemOfEquations<Us...> &);

  template <std::size_t N, typename... Us>
  friend decltype(auto) get(const SystemOfEquations<Us...> &);

  template <std::size_t N, typename... Us>
  friend decltype(auto) get(SystemOfEquations<Us...> &&);

public:
  constexpr static size_t number_of_equations = sizeof...(TEquations);
  static constexpr bool is_square =
      (... && (std::tuple_size_v<typename TEquations::derivatives_t> ==
               number_of_equations));
  constexpr explicit SystemOfEquations(TEquations... eqns)
      : equations{std::move(eqns)...} {}

  constexpr auto &get_equations() { return equations; }
  constexpr const auto &get_equations() const { return equations; }

  auto eval() const;
};

template <typename... TEquations>
auto SystemOfEquations<TEquations...>::eval() const {
  auto make_array_helper = []<typename Tuple, std::size_t... Is>(
                               const Tuple &tup, std::index_sequence<Is...>) {
    return std::array{std::get<Is>(tup).eval()...};
  };
  return make_array_helper(equations,
                           std::make_index_sequence<sizeof...(TEquations)>{});
}

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
//
// Created by sayan on 4/21/25.
//

#pragma once

#include "equation.hpp"

template <typename... TEquations>
class SystemOfEquations : public TupleSupport {
private:
  std::tuple<TEquations...> equations;
  static_assert(all_tuple_type_same<TEquations...>);

  template <std::size_t N, typename... Us>
  friend decltype(auto) get(SystemOfEquations<Us...> &);

  template <std::size_t N, typename... Us>
  friend decltype(auto) get(const SystemOfEquations<Us...> &);

  template <std::size_t N, typename... Us>
  friend decltype(auto) get(SystemOfEquations<Us...> &&);

public:
  using value_type = typename std::tuple_element_t<0, std::tuple<TEquations...>>::value_type;
  constexpr static size_t number_of_equations = sizeof...(TEquations);
  static constexpr bool is_square =
      (... && (std::tuple_size_v<typename TEquations::derivatives_t> ==
               number_of_equations));
  constexpr explicit SystemOfEquations(TEquations... eqns)
      : equations{std::move(eqns)...} {}

  constexpr auto &get_equations() { return equations; }
  constexpr const auto &get_equations() const { return equations; }

  auto eval() const;
  auto get_jacobian() -> std::enable_if_t < is_square, std::array<value_type,number_of_equations*number_of_equations>> const {
    return {};
  }
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
//
// Created by sayan on 4/21/25.
//

#pragma once

#include "equation.hpp"

template <typename... TEquations> class SystemOfEquations {
private:
  std::tuple<TEquations...> equations;
  static_assert(all_tuple_type_same<TEquations...>);

  template <std::size_t N, typename... Us>
  friend decltype(auto) get(SystemOfEquations<Us...> &);

  template <std::size_t N, typename... Us>
  friend decltype(auto) get(const SystemOfEquations<Us...> &);

  template <std::size_t N, typename... Us>
  friend decltype(auto) get(SystemOfEquations<Us...> &&);

  constexpr auto &get_equations() { return equations; }
  constexpr const auto &get_equations() const { return equations; }
  friend std::ostream &operator<<(std::ostream &out,
                                  const SystemOfEquations &e) {
    print_tup(out, e.equations);
    return out;
  }

public:
  using value_type =
      typename std::tuple_element_t<0, std::tuple<TEquations...>>::value_type;
  constexpr static size_t number_of_equations = sizeof...(TEquations);
  static constexpr bool is_square =
      (... && (std::tuple_size_v<typename TEquations::derivatives_t> ==
               number_of_equations));
  constexpr explicit SystemOfEquations(TEquations... eqns)
      : equations{std::move(eqns)...} {}

  auto eval() const;
  auto jacobian() const -> std::enable_if_t<
      is_square, std::array<std::array<value_type, number_of_equations>,
                            number_of_equations>>;
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
template <typename... TEquations>
auto SystemOfEquations<TEquations...>::jacobian() const
    -> std::enable_if_t<is_square,
                        std::array<std::array<value_type, number_of_equations>,
                                   number_of_equations>> {
  auto make_array_helper = []<typename Tuple, std::size_t... Is>(
                               const Tuple &tup, std::index_sequence<Is...>) {
    return std::array{std::get<Is>(tup).eval_derivatives()...};
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

template <typename TExpression, typename Tuple>
constexpr auto fold_tuple(TExpression expression,
                          const Tuple &missing_symbols) {

  auto fold_tuple_impl =
      []<typename TTExpression, typename TTuple, std::size_t... Is>(
          TTExpression exp, TTuple missing_symbols,
          std::index_sequence<Is...>) {
        using value_type = TTExpression::value_type;
        return (exp + ... +
                Variable<value_type, std::tuple_element_t<Is, TTuple>::value>{
                    value_type{}});
      };
  return Equation{
      fold_tuple_impl(std::move(expression), std::move(missing_symbols),
                      std::make_index_sequence<std::tuple_size_v<Tuple>>{})};
}

template <typename... TExpressions>
constexpr auto make_system_of_equations(TExpressions... exprs) {

  using combined_symbols_list_t =
      tuple_union_t<typename Equation<TExpressions>::symbolslist...>;
  constexpr combined_symbols_list_t combined_symbols_list{};

  auto fill_expression_with_missing_symbols =
      [&combined_symbols_list]<typename TExpr>(TExpr expr) {
        using current_symbols_list_t = typename Equation<TExpr>::symbolslist;
        using missing_symbols_list_t =
            tuple_difference_t<combined_symbols_list_t, current_symbols_list_t>;

        return fold_tuple(std::move(expr), missing_symbols_list_t{});
      };

  return SystemOfEquations(fill_expression_with_missing_symbols(exprs)...);
}
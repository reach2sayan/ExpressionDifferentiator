#pragma once

#include "dual.hpp"
#include "expressions.hpp"
#include "traits.hpp"
#include <array>
#include <boost/mp11.hpp>

template <ExpressionConcept Expr>
[[nodiscard]] constexpr auto reverse_mode_gradient(const Expr &expr) {
  using Syms = typename extract_symbols_from_expr<std::remove_cvref_t<Expr>>::type;
  using T = typename Expr::value_type;
  constexpr auto N = boost::mp11::mp_size<Syms>::value;
  std::array<T, N> grads{};
  expr.backward(Syms{}, T{1}, grads);
  return grads;
}

template <ExpressionConcept Expr>
[[nodiscard]] constexpr auto forward_mode_gradient(
    Expr &expr,
    std::array<dual_scalar_t<typename std::remove_cvref_t<Expr>::value_type>,
               boost::mp11::mp_size<typename extract_symbols_from_expr<
                   std::remove_cvref_t<Expr>>::type>::value>
        values)
  requires is_dual_v<typename std::remove_cvref_t<Expr>::value_type>
{
  using expr_type = std::remove_cvref_t<Expr>;
  using value_type = typename expr_type::value_type;
  using scalar_type = dual_scalar_t<value_type>;
  using symbols = typename extract_symbols_from_expr<expr_type>::type;
  constexpr std::size_t n = boost::mp11::mp_size<symbols>::value;

  std::array<scalar_type, n> gradients{};
  std::array<value_type, n> seeded{};

  for (std::size_t j = 0; j < n; ++j) {
    for (std::size_t i = 0; i < n; ++i)
      seeded[i] = value_type{values[i], i == j ? scalar_type{1} : scalar_type{}};
    expr.update(symbols{}, seeded);
    gradients[j] = expr.eval().template get<1>();
  }

  for (std::size_t i = 0; i < n; ++i)
    seeded[i] = value_type{values[i], scalar_type{}};
  expr.update(symbols{}, seeded);

  return gradients;
}

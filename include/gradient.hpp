#pragma once

#include "expressions.hpp"
#include "traits.hpp"
#include <array>
#include <boost/mp11.hpp>

template <ExpressionConcept Expr>
[[nodiscard]] constexpr auto gradient(const Expr &expr) {
  using Syms = typename extract_symbols_from_expr<std::remove_cvref_t<Expr>>::type;
  using T = typename Expr::value_type;
  constexpr auto N = boost::mp11::mp_size<Syms>::value;
  std::array<T, N> grads{};
  expr.backward(Syms{}, T{1}, grads);
  return grads;
}

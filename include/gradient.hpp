#pragma once

#include "dual.hpp"
#include "expressions.hpp"
#include "traits.hpp"
#include <array>
#include <boost/mp11/algorithm.hpp>

namespace mp = boost::mp11;

template <ExpressionConcept Expr, typename T = typename Expr::value_type>
  requires (!is_dual_v<T>)
[[nodiscard]] constexpr auto reverse_mode_gradient(const Expr &expr) {
  using Syms =
      typename extract_symbols_from_expr<std::remove_cvref_t<Expr>>::type;
  constexpr auto N = mp::mp_size<Syms>::value;
  std::array<T, N> grads{};
  expr.backward(Syms{}, T{1}, grads);
  return grads;
}

template <ExpressionConcept Expr, typename T = typename Expr::value_type>
  requires is_dual_v<T>
[[nodiscard]] constexpr auto reverse_mode_gradient(const Expr &expr) {
  using scalar_t = dual_scalar_t<T>;
  using Syms =
      typename extract_symbols_from_expr<std::remove_cvref_t<Expr>>::type;
  constexpr auto N = mp::mp_size<Syms>::value;
  std::array<T, N> grads{};
  expr.backward(Syms{}, T{1}, grads);
  std::array<scalar_t, N> result{};
  for (std::size_t i = 0; i < N; ++i) {
    result[i] = grads[i].template get<0>();
  }

  return result;
}

// Stateless forward-mode gradient: no update() calls, no mutation of expr.
// Each of the N passes does one eval_seeded() traversal instead of the old
// update()+eval() double-traversal, plus a final restore traversal.
// Cost: N traversals (was 2N+1).
template <
    ExpressionConcept Expr,
    typename TArr =
        dual_scalar_t<typename std::remove_cvref_t<Expr>::value_type>,
    std::size_t N = mp::mp_size<typename extract_symbols_from_expr<
        std::remove_cvref_t<Expr>>::type>::value>
[[nodiscard]] constexpr auto forward_mode_gradient(const Expr &expr,
                                                   std::array<TArr, N> values)
  requires is_dual_v<typename std::remove_cvref_t<Expr>::value_type>
{
  using expr_type = std::remove_cvref_t<Expr>;
  using value_type = typename expr_type::value_type;
  using scalar_type = dual_scalar_t<value_type>;
  using symbols = typename extract_symbols_from_expr<expr_type>::type;
  constexpr std::size_t n = mp::mp_size<symbols>::value;

  // For each pass J, build a fresh independent seed array with only variable J
  // seeded to 1. Independent arrays have no aliasing between passes so the
  // compiler can pipeline or constant-fold all N evaluations in parallel.
  std::array<scalar_type, n> gradients{};
  static_for<n>([&]<std::size_t J>() {
    std::array<value_type, n> s{};
    static_for<n>([&]<std::size_t I>() {
      s[I] = value_type{values[I], I == J ? scalar_type{1} : scalar_type{}};
    });
    gradients[J] = expr.template eval_seeded<symbols>(s).template get<1>();
  });

  return gradients;
}

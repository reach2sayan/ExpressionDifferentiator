#pragma once

#include "dual.hpp"
#include "expressions.hpp"
#include "traits.hpp"
#include <array>
#include <boost/mp11/algorithm.hpp>
#include <ranges>

namespace diff {

namespace mp = boost::mp11;

enum class DiffMode { Symbolic, Forward, Reverse };

// ===========================================================================
// detail — implementation functions; use the public wrappers below.
// ===========================================================================
namespace detail {

template <ExpressionConcept Expr, typename T = typename Expr::value_type>
  requires(!is_dual_v<T>)
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

// Stateless forward-mode gradient: N eval_seeded passes.
template <ExpressionConcept Expr,
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

// Forward-over-reverse Hessian. Requires value_type = Dual<S>.
// Mutates expr (seeds dual tangents) and restores on exit.
template <ExpressionConcept Expr,
          typename T = typename std::remove_cvref_t<Expr>::value_type,
          typename S = dual_scalar_t<T>,
          std::size_t N = mp::mp_size<typename extract_symbols_from_expr<
              std::remove_cvref_t<Expr>>::type>::value>
  requires is_dual_v<T>
[[nodiscard]] auto reverse_mode_hessian(Expr &expr, std::array<S, N> values) {
  using symbols = typename extract_symbols_from_expr<std::remove_cvref_t<Expr>>::type;

  std::array<std::array<S, N>, N> H{};
  std::array<T, N> seeds{};

  for (std::size_t j = 0; j < N; ++j) {
    for (std::size_t i = 0; i < N; ++i) {
      seeds[i] = T{values[i], i == j ? S{1} : S{}};
    }
    expr.update(symbols{}, seeds);
    std::array<T, N> grads{};
    expr.backward(symbols{}, T{1}, grads);
    for (std::size_t i = 0; i < N; ++i) {
      H[i][j] = grads[i].template get<1>();
    }
  }

  for (std::size_t i = 0; i < N; ++i) {
    seeds[i] = T{values[i], S{}};
  }
  expr.update(symbols{}, seeds);

  return H;
}

// No-values overload: reads current variable values via collect().
template <ExpressionConcept Expr,
          typename T = typename std::remove_cvref_t<Expr>::value_type,
          typename S = dual_scalar_t<T>,
          std::size_t N = mp::mp_size<typename extract_symbols_from_expr<
              std::remove_cvref_t<Expr>>::type>::value>
  requires is_dual_v<T>
[[nodiscard]] auto reverse_mode_hessian(Expr &expr) {
  using symbols = typename extract_symbols_from_expr<std::remove_cvref_t<Expr>>::type;
  std::array<T, N> current{};
  expr.collect(symbols{}, current);
  std::array<S, N> values{};
  for (std::size_t i = 0; i < N; ++i) {
    values[i] = current[i].template get<0>();
  }
  return reverse_mode_hessian(expr, values);
}

// Stateless forward-over-forward Hessian. Requires value_type = Dual<Dual<S>>.
template <ExpressionConcept Expr,
          typename T = typename std::remove_cvref_t<Expr>::value_type,
          typename D = dual_scalar_t<T>,
          typename S = dual_scalar_t<D>,
          std::size_t N = mp::mp_size<typename extract_symbols_from_expr<
              std::remove_cvref_t<Expr>>::type>::value>
  requires is_dual_v<T> && is_dual_v<D>
[[nodiscard]] constexpr auto forward_mode_hessian(const Expr &expr,
                                                  std::array<S, N> values) {
  using symbols = typename extract_symbols_from_expr<std::remove_cvref_t<Expr>>::type;

  std::array<std::array<S, N>, N> H{};
  static_for<N>([&]<std::size_t I>() {
    static_for<N>([&]<std::size_t J>() {
      std::array<T, N> seeds{};
      static_for<N>([&]<std::size_t K>() {
        seeds[K] = T{D{values[K], K == J ? S{1} : S{}},
                     D{K == I ? S{1} : S{}, S{}}};
      });
      H[I][J] =
          expr.template eval_seeded<symbols>(seeds).template get<1>().template get<1>();
    });
  });

  return H;
}

// No-values overload: reads current variable values via collect().
template <ExpressionConcept Expr,
          typename T = typename std::remove_cvref_t<Expr>::value_type,
          typename D = dual_scalar_t<T>,
          typename S = dual_scalar_t<D>,
          std::size_t N = mp::mp_size<typename extract_symbols_from_expr<
              std::remove_cvref_t<Expr>>::type>::value>
  requires is_dual_v<T> && is_dual_v<D>
[[nodiscard]] constexpr auto forward_mode_hessian(const Expr &expr) {
  using symbols = typename extract_symbols_from_expr<std::remove_cvref_t<Expr>>::type;
  std::array<T, N> current{};
  expr.collect(symbols{}, current);
  std::array<S, N> values{};
  for (std::size_t i = 0; i < N; ++i) {
    values[i] = current[i].template get<0>().template get<0>();
  }
  return forward_mode_hessian(expr, values);
}

} // namespace detail

// ===========================================================================
// Public API — select mode with DiffMode::Forward or DiffMode::Reverse.
// ===========================================================================

// gradient<DiffMode::Reverse>(expr) — one backward pass, no mutation.
template <DiffMode Mode, ExpressionConcept Expr>
  requires(Mode == DiffMode::Reverse)
[[nodiscard]] constexpr auto gradient(const Expr &expr) {
  return detail::reverse_mode_gradient(expr);
}

// gradient<DiffMode::Forward>(expr, values) — N seeded forward passes.
// Requires Dual<T> variables; values holds the base scalar evaluation point.
template <DiffMode Mode, ExpressionConcept Expr,
          typename TArr =
              dual_scalar_t<typename std::remove_cvref_t<Expr>::value_type>,
          std::size_t N = mp::mp_size<typename extract_symbols_from_expr<
              std::remove_cvref_t<Expr>>::type>::value>
  requires(Mode == DiffMode::Forward &&
           is_dual_v<typename std::remove_cvref_t<Expr>::value_type>)
[[nodiscard]] constexpr auto gradient(const Expr &expr,
                                      std::array<TArr, N> values) {
  return detail::forward_mode_gradient(expr, values);
}

// hessian<DiffMode::Reverse>(expr, values) — forward-over-reverse,
// N backward passes. Requires Dual<S> variables; mutates and restores expr.
template <DiffMode Mode, ExpressionConcept Expr,
          typename T = typename std::remove_cvref_t<Expr>::value_type,
          typename S = dual_scalar_t<T>,
          std::size_t N = mp::mp_size<typename extract_symbols_from_expr<
              std::remove_cvref_t<Expr>>::type>::value>
  requires(Mode == DiffMode::Reverse && is_dual_v<T>)
[[nodiscard]] auto hessian(Expr &expr, std::array<S, N> values) {
  return detail::reverse_mode_hessian(expr, values);
}

// hessian<DiffMode::Reverse>(expr) — reads current variable values.
template <DiffMode Mode, ExpressionConcept Expr,
          typename T = typename std::remove_cvref_t<Expr>::value_type,
          typename S = dual_scalar_t<T>,
          std::size_t N = mp::mp_size<typename extract_symbols_from_expr<
              std::remove_cvref_t<Expr>>::type>::value>
  requires(Mode == DiffMode::Reverse && is_dual_v<T>)
[[nodiscard]] auto hessian(Expr &expr) {
  return detail::reverse_mode_hessian(expr);
}

// hessian<DiffMode::Forward>(expr, values) — forward-over-forward,
// N² seeded passes. Requires Dual<Dual<S>> variables; fully stateless.
template <DiffMode Mode, ExpressionConcept Expr,
          typename T = typename std::remove_cvref_t<Expr>::value_type,
          typename D = dual_scalar_t<T>,
          typename S = dual_scalar_t<D>,
          std::size_t N = mp::mp_size<typename extract_symbols_from_expr<
              std::remove_cvref_t<Expr>>::type>::value>
  requires(Mode == DiffMode::Forward && is_dual_v<T> && is_dual_v<D>)
[[nodiscard]] constexpr auto hessian(const Expr &expr, std::array<S, N> values) {
  return detail::forward_mode_hessian(expr, values);
}

// hessian<DiffMode::Forward>(expr) — reads current variable values.
template <DiffMode Mode, ExpressionConcept Expr,
          typename T = typename std::remove_cvref_t<Expr>::value_type,
          typename D = dual_scalar_t<T>,
          typename S = dual_scalar_t<D>,
          std::size_t N = mp::mp_size<typename extract_symbols_from_expr<
              std::remove_cvref_t<Expr>>::type>::value>
  requires(Mode == DiffMode::Forward && is_dual_v<T> && is_dual_v<D>)
[[nodiscard]] constexpr auto hessian(const Expr &expr) {
  return detail::forward_mode_hessian(expr);
}

} // namespace diff

#define reverse_mode_grad diff::gradient<diff::DiffMode::Reverse>

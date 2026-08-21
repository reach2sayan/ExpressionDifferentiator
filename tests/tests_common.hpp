#pragma once
#include "ddx.hpp"

#include "dual/dual.hpp"
#include "dual/taylor_dual.hpp"
#include "expr/bound.hpp"
#include "drivers/numeric.hpp"
#include "drivers/symbolic.hpp"
#include "drivers/seeded_energy.hpp"
#include "drivers/hessian.hpp"
#include "expr/equation.hpp"
#include "expr/format.hpp"
#include "expr/operations.hpp"
#include "expr/traits.hpp"
#include "expr/values.hpp"
#include "md/layouts.hpp"
#include "md/tensor.hpp"
#include "util/scope_guard.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <gtest/gtest.h>
#include <tuple>
#include <numbers>
#include <random>
#include <ranges>
#include <set>
#include <sstream>
#include <stdexcept>

using namespace ddx::impl;
using namespace ddx::literals; // "x"_s

// Test-side conveniences over the drivers' two second-order result shapes.
// Both name the same members, so the only thing that differs is whether the
// buffers are owned (unique_ptr, runtime arity) or inline (array, static N).
template <typename T>
concept CHessianResult = requires(const T &H) {
  { H.value } -> std::convertible_to<double>;
  H.gradient;
  H.hessian;
  { T::arity } -> std::convertible_to<std::size_t>;
} || requires(const T &H) {
  { H.value } -> std::convertible_to<double>;
  { H.arity } -> std::convertible_to<std::size_t>;
};

[[nodiscard]] constexpr const double *raw(const double *p) noexcept { return p; }
template <std::ranges::contiguous_range R>
[[nodiscard]] constexpr const double *raw(const R &r) noexcept {
  return std::ranges::data(r);
}
[[nodiscard]] inline const double *raw(const std::unique_ptr<double[]> &p) noexcept {
  return p.get();
}

template <CHessianResult T>
[[nodiscard]] constexpr const double *grad_ptr(const T &H) noexcept {
  return raw(H.gradient);
}

template <CHessianResult T>
[[nodiscard]] constexpr const double *hess_ptr(const T &H) noexcept {
  return raw(H.hessian);
}

template <CHessianResult T>
[[nodiscard]] constexpr std::size_t hess_n(const T &H) noexcept {
  return H.arity;
}

template <CHessianResult T>
[[nodiscard]] constexpr double hess_at(const T &H, std::size_t i,
                                       std::size_t j) noexcept {
  return hess_ptr(H)[i * hess_n(H) + j]; // row-major, as the drivers document
}

template <CHessianResult T>
[[nodiscard]] constexpr double grad_at(const T &H, std::size_t i) noexcept {
  return grad_ptr(H)[i];
}

template <CHessianResult T>
[[nodiscard]] constexpr double val_of(const T &H) noexcept {
  return H.value;
}


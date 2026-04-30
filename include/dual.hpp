#pragma once

#include "expressions.hpp"
#include <cmath>
#include <ostream>
#include <tuple>
#include <utility>

template <typename T> class Dual {
private:
  T val{};
  T deriv{};

public:
  constexpr Dual() = default;
  constexpr explicit Dual(T v, T d = T{}) : val(v), deriv(d) {}

  constexpr Dual operator+(const Dual &o) const {
    return Dual{val + o.val, deriv + o.deriv};
  }
  constexpr Dual operator-(const Dual &o) const {
    return Dual{val - o.val, deriv - o.deriv};
  }
  constexpr Dual operator*(const Dual &o) const {
    return Dual{val * o.val, deriv * o.val + val * o.deriv};
  }
  constexpr Dual operator/(const Dual &o) const {
    return Dual{val / o.val, (deriv * o.val - val * o.deriv) / (o.val * o.val)};
  }
  constexpr Dual &operator+=(const Dual &o) { val += o.val; deriv += o.deriv; return *this; }
  constexpr Dual &operator-=(const Dual &o) { val -= o.val; deriv -= o.deriv; return *this; }
  constexpr Dual &operator*=(const Dual &o) { *this = *this * o; return *this; }
  constexpr Dual &operator/=(const Dual &o) { *this = *this / o; return *this; }
  constexpr Dual operator-() const { return Dual{-val, -deriv}; }
  constexpr Dual &operator++() {
    ++val;
    return *this;
  }

  friend std::ostream &operator<<(std::ostream &out, const Dual &d) {
    return out << d.val << "+" << d.deriv << "e";
  }

  template <std::size_t Index> [[nodiscard]] constexpr auto get() const {
    static_assert(Index < 2, "Dual index out of bounds");
    if constexpr (Index == 0)
      return val;
    else
      return deriv;
  }

  // Hidden friends — found by ADL, have access to private members.
  [[nodiscard]] friend constexpr Dual sin(const Dual &d) {
    using std::sin, std::cos;
    return Dual{sin(d.val), cos(d.val) * d.deriv};
  }
  [[nodiscard]] friend constexpr Dual cos(const Dual &d) {
    using std::sin, std::cos;
    return Dual{cos(d.val), -sin(d.val) * d.deriv};
  }
  [[nodiscard]] friend constexpr Dual exp(const Dual &d) {
    using std::exp;
    const T e = exp(d.val);
    return Dual{e, e * d.deriv};
  }
};

static_assert(Numeric<Dual<double>>);
static_assert(Numeric<Dual<float>>);

namespace {
template <typename T> T dual_scalar_impl(T &&);
template <typename T> T dual_scalar_impl(Dual<T> &&);
template <typename T> consteval bool is_dual_impl(std::type_identity<T>) {
  return false;
}
template <typename T> consteval bool is_dual_impl(std::type_identity<Dual<T>>) {
  return true;
}
} // namespace

template <typename T>
inline constexpr bool is_dual_v = is_dual_impl(std::type_identity<T>{});

template <typename T>
using dual_scalar_t = decltype(dual_scalar_impl(std::declval<T>()));

template <typename T>
struct std::tuple_size<Dual<T>> : std::integral_constant<std::size_t, 2> {};
template <typename T, std::size_t N> struct std::tuple_element<N, Dual<T>> {
  using type = T;
};

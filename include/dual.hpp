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

template <typename T> struct is_dual : std::false_type {};
template <typename T> struct is_dual<Dual<T>> : std::true_type {};
template <typename T> inline constexpr bool is_dual_v = is_dual<T>::value;

template <typename T> struct dual_scalar {
  using type = T;
};
template <typename T> struct dual_scalar<Dual<T>> {
  using type = T;
};
template <typename T> using dual_scalar_t = typename dual_scalar<T>::type;

template <typename T>
struct std::tuple_size<Dual<T>> : std::integral_constant<std::size_t, 2> {};
template <typename T, std::size_t N> struct std::tuple_element<N, Dual<T>> {
  using type = T;
};


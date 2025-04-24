//
// Created by sayan on 4/19/25.
//

#pragma once
template <typename> class Derivative {
  [[deprecated]];
};
#define DEPRECATED_DERIVATIVE true
#if !DEPRECATED_DERIVATIVE
template <typename TExpression>
constexpr auto collect_variable_labels(const TExpression &expression) {
  std::array<char, N> result{};
  std::size_t index = 0;
  make_labels_array(expression, result, index);
  return result;
}

template<typename TExpression>
constexpr static auto
make_derivatives(const TExpression &expression) {
  auto labels = collect_variable_labels(expression);
  auto label_tuple = std::tuple_cat(labels);
  return label_tuple;
}

template <typename TExpression, typename... TDerivatives> class Derivative {
  std::tuple<TDerivatives...> derivatives;

public:
  constexpr Derivative(const TExpression &expression)
      : derivatives{make_derivatives(expression)} {}
};

template <typename TExpression>
Derivative(TExpression)
    -> Derivative<std::decay_t<TExpression>,
                  decltype(TExpression::template make_derivatives(
                      std::declval<const TExpression&>()))>;

#endif
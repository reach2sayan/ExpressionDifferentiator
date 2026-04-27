#include "dual.hpp"
#include "equation.hpp"
#include "gradient.hpp"
#include "values.hpp"

#include <benchmark/benchmark.h>

#include <array>
#include <numbers>

template <ExpressionConcept Expr>
[[nodiscard]] auto forward_gradient(
    Expr &expr, std::array<dual_scalar_t<typename Expr::value_type>,
                           boost::mp11::mp_size<
                               typename extract_symbols_from_expr<Expr>::type>::value>
                   values) {
  using value_type = typename Expr::value_type;
  using scalar_type = dual_scalar_t<value_type>;
  using symbols = typename extract_symbols_from_expr<Expr>::type;
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

template <typename Eq>
static void run_symbolic(benchmark::State &state, Eq &eq) {
  for (auto _ : state) {
    auto gradients = eq.eval_derivatives();
    benchmark::DoNotOptimize(gradients);
    benchmark::ClobberMemory();
  }
}

template <ExpressionConcept Expr>
static void run_reverse(benchmark::State &state, const Expr &expr) {
  for (auto _ : state) {
    auto gradients = gradient(expr);
    benchmark::DoNotOptimize(gradients);
    benchmark::ClobberMemory();
  }
}

template <ExpressionConcept Expr, std::size_t N>
static void run_forward(benchmark::State &state, Expr &expr,
                        const std::array<dual_scalar_t<typename Expr::value_type>, N> &values) {
  for (auto _ : state) {
    auto gradients = forward_gradient(expr, values);
    benchmark::DoNotOptimize(gradients);
    benchmark::ClobberMemory();
  }
}

static void BM_Symbolic_F1_Univariate(benchmark::State &state) {
  auto x = PV(1.25, 'x');
  auto eq = Equation(exp(x) * sin(x) + x * x * x + 2.0 * x);
  run_symbolic(state, eq);
}
BENCHMARK(BM_Symbolic_F1_Univariate);

static void BM_Forward_F1_Univariate(benchmark::State &state) {
  using D = Dual<double>;
  Variable<D, 'x'> x{D{1.25}};
  auto expr = exp(x) * sin(x) + x * x * x + 2.0 * x;
  run_forward(state, expr, std::array{1.25});
}
BENCHMARK(BM_Forward_F1_Univariate);

static void BM_Reverse_F1_Univariate(benchmark::State &state) {
  auto x = PV(1.25, 'x');
  auto expr = exp(x) * sin(x) + x * x * x + 2.0 * x;
  run_reverse(state, expr);
}
BENCHMARK(BM_Reverse_F1_Univariate);

static void BM_Symbolic_F2_Bivariate(benchmark::State &state) {
  auto x = PV(1.3, 'x');
  auto y = PV(0.7, 'y');
  auto eq = Equation(x * y + sin(x) + y * y + exp(x + y));
  run_symbolic(state, eq);
}
BENCHMARK(BM_Symbolic_F2_Bivariate);

static void BM_Forward_F2_Bivariate(benchmark::State &state) {
  using D = Dual<double>;
  Variable<D, 'x'> x{D{1.3}};
  Variable<D, 'y'> y{D{0.7}};
  auto expr = x * y + sin(x) + y * y + exp(x + y);
  run_forward(state, expr, std::array{1.3, 0.7});
}
BENCHMARK(BM_Forward_F2_Bivariate);

static void BM_Reverse_F2_Bivariate(benchmark::State &state) {
  auto x = PV(1.3, 'x');
  auto y = PV(0.7, 'y');
  auto expr = x * y + sin(x) + y * y + exp(x + y);
  run_reverse(state, expr);
}
BENCHMARK(BM_Reverse_F2_Bivariate);

static void BM_Symbolic_F3_Trivariate(benchmark::State &state) {
  auto x = PV(0.9, 'x');
  auto y = PV(1.1, 'y');
  auto z = PV(0.4, 'z');
  auto eq = Equation(exp(x * y) + sin(z) * x + y * z + x * x * z);
  run_symbolic(state, eq);
}
BENCHMARK(BM_Symbolic_F3_Trivariate);

static void BM_Forward_F3_Trivariate(benchmark::State &state) {
  using D = Dual<double>;
  Variable<D, 'x'> x{D{0.9}};
  Variable<D, 'y'> y{D{1.1}};
  Variable<D, 'z'> z{D{0.4}};
  auto expr = exp(x * y) + sin(z) * x + y * z + x * x * z;
  run_forward(state, expr, std::array{0.9, 1.1, 0.4});
}
BENCHMARK(BM_Forward_F3_Trivariate);

static void BM_Reverse_F3_Trivariate(benchmark::State &state) {
  auto x = PV(0.9, 'x');
  auto y = PV(1.1, 'y');
  auto z = PV(0.4, 'z');
  auto expr = exp(x * y) + sin(z) * x + y * z + x * x * z;
  run_reverse(state, expr);
}
BENCHMARK(BM_Reverse_F3_Trivariate);

static void BM_Symbolic_F4_FourVariables(benchmark::State &state) {
  auto x = PV(1.0, 'x');
  auto y = PV(0.5, 'y');
  auto z = PV(1.7, 'z');
  auto w = PV(std::numbers::pi_v<double> / 6.0, 'w');
  auto eq = Equation((x + y) * (z - w) + exp(x * z) + sin(y * w) + x * y * z * w);
  run_symbolic(state, eq);
}
BENCHMARK(BM_Symbolic_F4_FourVariables);

static void BM_Forward_F4_FourVariables(benchmark::State &state) {
  using D = Dual<double>;
  Variable<D, 'x'> x{D{1.0}};
  Variable<D, 'y'> y{D{0.5}};
  Variable<D, 'z'> z{D{1.7}};
  Variable<D, 'w'> w{D{std::numbers::pi_v<double> / 6.0}};
  auto expr = (x + y) * (z - w) + exp(x * z) + sin(y * w) + x * y * z * w;
  run_forward(state, expr, std::array{1.0, 0.5, 1.7, std::numbers::pi_v<double> / 6.0});
}
BENCHMARK(BM_Forward_F4_FourVariables);

static void BM_Reverse_F4_FourVariables(benchmark::State &state) {
  auto x = PV(1.0, 'x');
  auto y = PV(0.5, 'y');
  auto z = PV(1.7, 'z');
  auto w = PV(std::numbers::pi_v<double> / 6.0, 'w');
  auto expr = (x + y) * (z - w) + exp(x * z) + sin(y * w) + x * y * z * w;
  run_reverse(state, expr);
}
BENCHMARK(BM_Reverse_F4_FourVariables);

BENCHMARK_MAIN();

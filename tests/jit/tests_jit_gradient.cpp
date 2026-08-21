#include "ddx.hpp"
#include "jit/kernel.hpp"
#include "rt/bridge.hpp"
#include "rt/derivative.hpp"
#include "rt/graph.hpp"
#include "rt/interpret.hpp"

#include <gtest/gtest.h>

#include <vector>

// ===========================================================================
// JIT'd gradients
//
// A kernel computes exactly the outputs its graph was frozen with, so freezing
// {value, partials...} is all it takes to get the gradient columns; the checks
// here are that those columns land in the right buffers and hold what the
// interpreter says they should.
// ===========================================================================

namespace {
using ddx::rt::Builder;
using ddx::rt::Graph;

ddx::jit::Compiler &compiler() {
  static ddx::jit::Compiler c;
  return c;
}

void expect_gradient_matches_interpreter(auto build, std::size_t nvars,
                                         std::size_t n = 32) {
  Builder b;
  std::vector<ddx::rt::Expr> vars;
  static constexpr const char *names[] = {"x", "y", "z"};
  for (std::size_t i = 0; i < nvars; ++i) {
    vars.push_back(var(b, names[i]));
  }
  const auto root = build(b, vars);
  // The sweep is repeated for the reference values; the builder runs its own.
  const auto reference_gradient = ddx::rt::gradient(b, root.id(b));
  const auto kernel = compiler().compile(
      ddx::rt::GraphBuilder{b}.value(root).gradient().build());
  ASSERT_EQ(kernel.outputs(), nvars + 1);

  std::vector<std::vector<double>> columns(nvars, std::vector<double>(n));
  for (std::size_t j = 0; j < nvars; ++j) {
    for (std::size_t i = 0; i < n; ++i) {
      columns[j][i] = 0.2 + 0.5 * static_cast<double>((i + 2 * j) % 7) / 7.0;
    }
  }
  std::vector<const double *> xs(nvars);
  for (std::size_t j = 0; j < nvars; ++j) {
    xs[j] = columns[j].data();
  }

  std::vector<double> value(n);
  std::vector<std::vector<double>> grad(nvars, std::vector<double>(n));
  std::vector<double *> gp(nvars);
  for (std::size_t j = 0; j < nvars; ++j) {
    gp[j] = grad[j].data();
  }
  kernel(xs, value.data(), gp.data(), n);

  for (std::size_t i = 0; i < n; ++i) {
    std::vector<double> point(nvars);
    for (std::size_t j = 0; j < nvars; ++j) {
      point[j] = columns[j][i];
    }
    const auto reference = ddx::rt::evaluate_all(b, point);
    EXPECT_NEAR(value[i], reference[reference_gradient.value], 1e-12);
    for (std::size_t j = 0; j < nvars; ++j) {
      const double want = reference[reference_gradient.partial[j]];
      EXPECT_NEAR(grad[j][i], want, 1e-12 * std::max(1.0, std::abs(want)))
          << "d/d" << names[j] << " at point " << i;
    }
  }
}

TEST(JitGradient, MatchesTheInterpreter) {
  expect_gradient_matches_interpreter(
      [](Builder &, auto &v) { return exp(v[0]) * sin(v[1]); }, 2);
  expect_gradient_matches_interpreter(
      [](Builder &, auto &v) { return v[0] / v[1]; }, 2);
  expect_gradient_matches_interpreter(
      [](Builder &, auto &v) { return pow(v[0], v[1]); }, 2);
  expect_gradient_matches_interpreter(
      [](Builder &, auto &v) { return log(v[0]) * sqrt(v[1]) + tanh(v[2]); },
      3);
  expect_gradient_matches_interpreter(
      [](Builder &, auto &v) { return erf(v[0]) + cbrt(v[1]); }, 2);
}

TEST(JitGradient, MatchesDdxThroughTheBridge) {
  constexpr auto x = ddx::var<"x">;
  constexpr auto y = ddx::var<"y">;
  const auto f = exp(x) * sin(y) + x * y;

  Builder b;
  const auto root = ddx::rt::to_graph(b, f);
  const auto kernel = compiler().compile(
      ddx::rt::GraphBuilder{b}.value(root).gradient().build());

  const std::array cx{1.0, 0.25, 2.5};
  const std::array cy{2.0, 1.75, 0.5};
  const std::array<const double *, 2> xs{cx.data(), cy.data()};
  std::array<double, 3> value{};
  std::array<double, 3> dx{};
  std::array<double, 3> dy{};
  std::array<double *, 2> gp{dx.data(), dy.data()};
  kernel(xs, value.data(), gp.data(), value.size());

  for (std::size_t i = 0; i < value.size(); ++i) {
    const auto expected = ddx::Equation{f}.gradient(cx[i], cy[i]);
    EXPECT_NEAR(value[i], ddx::Equation{f}.evaluate(cx[i], cy[i]), 1e-12);
    EXPECT_NEAR(dx[i], expected[0], 1e-12);
    EXPECT_NEAR(dy[i], expected[1], 1e-12);
  }
}

} // namespace

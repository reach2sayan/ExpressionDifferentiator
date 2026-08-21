#include "ddx.hpp"
#include "jit/kernel.hpp"
#include "rt/bridge.hpp"
#include "rt/graph.hpp"
#include "rt/interpret.hpp"

#include <gtest/gtest.h>

#include <numeric>
#include <vector>

// ===========================================================================
// JIT'd values (src/jit/codegen.cpp)
//
// The interpreter in rt/interpret.hpp is the reference: it and the compiled
// kernel walk the same graph, so any disagreement is a codegen bug rather than
// a question about what the expression means.  Batch shape throughout, because
// that is the shape the kernel exists for.
// ===========================================================================

namespace {
using ddx::rt::Builder;
using ddx::rt::Graph;

ddx::jit::Compiler &compiler() {
  static ddx::jit::Compiler c;
  return c;
}

// Compile the value of `build` and compare a whole batch against the
// interpreter, point by point.
void expect_matches_interpreter(auto build, std::size_t nvars,
                                std::size_t n = 64) {
  Builder b;
  std::vector<ddx::rt::Expr> vars;
  static constexpr const char *names[] = {"x", "y", "z", "w"};
  for (std::size_t i = 0; i < nvars; ++i) {
    vars.push_back(var(b, names[i]));
  }
  const auto root = build(b, vars);
  const auto graph = Graph::freeze(b, std::array{root.id(b)});
  const auto kernel = compiler().compile(graph);
  ASSERT_TRUE(static_cast<bool>(kernel));
  ASSERT_EQ(kernel.arity(), nvars);

  // Spread the sample points over a range every op in the suite accepts.
  std::vector<std::vector<double>> columns(nvars, std::vector<double>(n));
  for (std::size_t j = 0; j < nvars; ++j) {
    for (std::size_t i = 0; i < n; ++i) {
      columns[j][i] = 0.15 + 0.6 * static_cast<double>((i + 3 * j) % 11) / 11.0;
    }
  }
  std::vector<const double *> xs(nvars);
  for (std::size_t j = 0; j < nvars; ++j) {
    xs[j] = columns[j].data();
  }

  std::vector<double> got(n, std::numeric_limits<double>::quiet_NaN());
  kernel(xs, got.data(), nullptr, n);

  for (std::size_t i = 0; i < n; ++i) {
    std::vector<double> point(nvars);
    for (std::size_t j = 0; j < nvars; ++j) {
      point[j] = columns[j][i];
    }
    const double want = ddx::rt::evaluate(b, root.id(b), point);
    ASSERT_NEAR(got[i], want, 1e-12 * std::max(1.0, std::abs(want)))
        << "at point " << i;
  }
}

TEST(JitValue, Arithmetic) {
  expect_matches_interpreter([](Builder &, auto &v) { return v[0] + v[1]; }, 2);
  expect_matches_interpreter([](Builder &, auto &v) { return v[0] - v[1]; }, 2);
  expect_matches_interpreter([](Builder &, auto &v) { return v[0] * v[1]; }, 2);
  expect_matches_interpreter([](Builder &, auto &v) { return v[0] / v[1]; }, 2);
  expect_matches_interpreter([](Builder &, auto &v) { return -v[0] * v[1]; },
                             2);
}

// Split across tests so a failure names the op that broke.
TEST(JitValue, IntrinsicBackedFunctions) {
  expect_matches_interpreter(
      [](Builder &, auto &v) { return sin(v[0]) + cos(v[1]); }, 2);
  expect_matches_interpreter(
      [](Builder &, auto &v) { return tan(v[0]) * exp(v[1]); }, 2);
  expect_matches_interpreter(
      [](Builder &, auto &v) { return log(v[0]) - log10(v[1]); }, 2);
  expect_matches_interpreter(
      [](Builder &, auto &v) { return sqrt(v[0]) + asin(v[1]); }, 2);
  expect_matches_interpreter(
      [](Builder &, auto &v) { return acos(v[0]) + atan(v[1]); }, 2);
  expect_matches_interpreter(
      [](Builder &, auto &v) { return sinh(v[0]) + cosh(v[1]); }, 2);
  expect_matches_interpreter(
      [](Builder &, auto &v) { return tanh(v[0]) * abs(v[1]); }, 2);
  expect_matches_interpreter([](Builder &, auto &v) { return pow(v[0], v[1]); },
                             2);
  expect_matches_interpreter(
      [](Builder &, auto &v) { return atan2(v[0], v[1]); }, 2);
  expect_matches_interpreter([](Builder &, auto &v) { return max(v[0], v[1]); },
                             2);
  expect_matches_interpreter([](Builder &, auto &v) { return min(v[0], v[1]); },
                             2);
}

// These six have no LLVM intrinsic and go out as libm calls, so they exercise
// a different path through the emitter.
TEST(JitValue, LibmBackedFunctions) {
  expect_matches_interpreter([](Builder &, auto &v) { return cbrt(v[0]); }, 1);
  expect_matches_interpreter([](Builder &, auto &v) { return asinh(v[0]); }, 1);
  expect_matches_interpreter([](Builder &, auto &v) { return atanh(v[0]); }, 1);
  expect_matches_interpreter([](Builder &, auto &v) { return erf(v[0]); }, 1);
  expect_matches_interpreter(
      [](Builder &, auto &v) { return hypot(v[0], v[1]); }, 2);
  expect_matches_interpreter(
      [](Builder &, auto &v) { return acosh(v[0] + 1.0); }, 1);
}

TEST(JitValue, SharedAndNested) {
  expect_matches_interpreter(
      [](Builder &, auto &v) {
        return (v[0] * v[1]) * (v[0] * v[1]) + sin(v[0] * v[1]);
      },
      2);
  expect_matches_interpreter(
      [](Builder &, auto &v) { return sin(cos(exp(v[0] * v[1]))); }, 2);
  expect_matches_interpreter(
      [](Builder &, auto &v) {
        auto t = v[0];
        for (int i = 0; i < 6; ++i) {
          t = sin(t * v[1]) + exp(t / 3.0);
        }
        return t;
      },
      2);
  expect_matches_interpreter(
      [](Builder &, auto &v) {
        return exp(v[0] * v[1]) + log(v[2]) / tanh(v[3]);
      },
      4);
}

TEST(JitValue, AgreesWithDdxThroughTheBridge) {
  constexpr auto x = ddx::var<"x">;
  constexpr auto y = ddx::var<"y">;
  Builder b;
  const auto root = ddx::rt::to_graph(b, exp(x) * sin(y));
  const auto graph = Graph::freeze(b, std::array{root.id(b)});
  const auto kernel = compiler().compile(graph);

  const std::array cx{1.0, 0.25, 2.5};
  const std::array cy{2.0, 1.75, 0.5};
  const std::array<const double *, 2> xs{cx.data(), cy.data()};
  std::array<double, 3> got{};
  kernel(xs, got.data(), nullptr, got.size());

  for (std::size_t i = 0; i < got.size(); ++i) {
    EXPECT_NEAR(got[i], ddx::Equation{exp(x) * sin(y)}.evaluate(cx[i], cy[i]),
                1e-12);
  }
}

TEST(JitValue, EmptyBatchWritesNothing) {
  Builder b;
  const auto x = var(b, "x");
  const auto graph = Graph::freeze(b, std::array{sin(x).id(b)});
  const auto kernel = compiler().compile(graph);

  const double *none = nullptr;
  const std::array<const double *, 1> xs{none};
  double sentinel = 42.0;
  kernel(xs, &sentinel, nullptr, 0);
  EXPECT_DOUBLE_EQ(sentinel, 42.0);
}

TEST(JitValue, ConstantFoldsToAStore) {
  Builder b;
  const auto x = var(b, "x");
  const auto f = x * ddx::rt::Expr{0} + ddx::rt::Expr{7};
  const auto graph = Graph::freeze(b, std::array{f.id(b)});
  const auto kernel = compiler().compile(graph);

  const std::array cx{1.0, 2.0};
  const std::array<const double *, 1> xs{cx.data()};
  std::array<double, 2> got{};
  kernel(xs, got.data(), nullptr, got.size());
  EXPECT_DOUBLE_EQ(got[0], 7.0);
  EXPECT_DOUBLE_EQ(got[1], 7.0);
}

TEST(JitValue, SeparateCompilesCoexist) {
  Builder b1;
  const auto x1 = var(b1, "x");
  const auto k1 =
      compiler().compile(Graph::freeze(b1, std::array{sin(x1).id(b1)}));

  Builder b2;
  const auto x2 = var(b2, "x");
  const auto k2 =
      compiler().compile(Graph::freeze(b2, std::array{cos(x2).id(b2)}));

  const std::array c{0.5};
  const std::array<const double *, 1> xs{c.data()};
  double a = 0, bb = 0;
  k1(xs, &a, nullptr, 1);
  k2(xs, &bb, nullptr, 1);
  EXPECT_NEAR(a, std::sin(0.5), 1e-15);
  EXPECT_NEAR(bb, std::cos(0.5), 1e-15);
}

} // namespace

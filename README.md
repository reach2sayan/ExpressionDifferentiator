[![CMake](https://github.com/reach2sayan/ExpressionSolver/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/reach2sayan/ExpressionSolver/actions/workflows/cmake-multi-platform.yml)
[![C++](https://img.shields.io/badge/C++-%2300599C.svg?logo=c%2B%2B&logoColor=white)](#)

# Expression Differentiator

A header-only C++20 library for symbolic expressions, symbolic differentiation, forward-mode automatic differentiation with dual numbers, reverse-mode gradients, and Jacobian computation.

## What it does

- Build typed expression trees from `Variable`, `Constant`, and operator nodes.
- Compute exact symbolic derivatives with product, quotient, and chain rules.
- Wrap scalar expressions in `Equation` to evaluate all partial derivatives.
- Wrap multiple outputs in `VectorEquation` to evaluate Jacobians.
- Use `Dual<T>` for forward-mode automatic differentiation.
- Use `reverse_mode_gradient(expr)` for reverse-mode gradients of scalar expressions.
- Use `VectorEquation::eval_jacobian_reverse()` for reverse-mode Jacobians of vector-valued functions.
- Evaluate many constant-only expressions at compile time.

## Requirements

- C++20 compiler
- CMake 3.20+
- Boost headers with `boost::mp11`

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

On Windows, the GitHub workflow builds with MSVC and vcpkg-provided Boost headers. The local CMake setup also falls back to fetching `boost/mp11` when `Boost::headers` is not already available.

## Benchmarks

Google Benchmark support is built by default through the `benchmarks` target.

```sh
cmake -S . -B build
cmake --build build --config Release --target benchmarks
./build/Release/benchmarks --benchmark_min_time=0.05s
```

You can also export machine-readable JSON with the custom CMake target:

```sh
cmake --build build --config Release --target benchmark_json
```

This writes `benchmark-results/benchmarks.json` inside the build directory.

PowerShell users should quote regex filters:

```powershell
.\build-win\Release\benchmarks.exe "--benchmark_filter=F1|F2|F3|F4" --benchmark_min_time=0.05s
```

A manual GitHub Actions workflow is available at
[.github/workflows/benchmark-manual.yml](C:/Users/sayan.samanta/source/repos/Expression_Differentiator/.github/workflows/benchmark-manual.yml).
It builds the benchmark target on Ubuntu and Windows, exports JSON, captures console output, and uploads the results as workflow artifacts.

The benchmark suite compares three ways of computing a full gradient for the same scalar function:

- symbolic partials via `Equation(...).eval_derivatives()`
- forward mode via `Dual<T>` with one seeded pass per input
- reverse mode via `reverse_mode_gradient(expr)`

The current suite uses four functions:

- `F1(x) = exp(x) * sin(x) + x^3 + 2x`
- `F2(x, y) = xy + sin(x) + y^2 + exp(x + y)`
- `F3(x, y, z) = exp(xy) + x sin(z) + yz + x^2 z`
- `F4(x, y, z, w) = (x + y)(z - w) + exp(xz) + sin(yw) + xyzw`

Current Release snapshot on this Windows machine:

| Function | Symbolic | Forward | Reverse |
|---|---:|---:|---:|
| `F1` | 12.6 ns | 10.3 ns | 9.63 ns |
| `F2` | 12.6 ns | 14.6 ns | 6.00 ns |
| `F3` | 22.0 ns | 24.0 ns | 7.50 ns |
| `F4` | 23.0 ns | 39.2 ns | 5.44 ns |

In this snapshot, reverse mode is fastest on all four benchmarked functions. Treat these numbers as machine- and compiler-dependent measurements rather than fixed library-wide conclusions.

### Vector Jacobian benchmark slice

There is also a vector-valued benchmark for a 2-output, 4-input function:

- symbolic Jacobian via `VectorEquation::eval_jacobian()`
- forward Jacobian via `VectorEquation::eval_jacobian_forward(...)`
- reverse Jacobian via `VectorEquation::eval_jacobian_reverse()`

```powershell
.\build-win\Release\benchmarks.exe "--benchmark_filter=.*Vector.*" --benchmark_min_time=0.05s
```

Current Release snapshot on this Windows machine:

| Benchmark | Time |
|---|---:|
| symbolic vector Jacobian | 27.9 ns |
| forward vector Jacobian | 140 ns |
| reverse vector Jacobian | 6.25 ns |

### Memory-oriented benchmark slice

There is also a small memory-focused benchmark slice that looks at object footprint and batched evaluation throughput for the 4-variable function `F4`.

```powershell
.\build-win\Release\benchmarks.exe "--benchmark_filter=.*(Footprint|Batched).*" --benchmark_min_time=0.05s
```

Current snapshot on this machine:

- symbolic expression object: `96 B`
- reverse expression object: `96 B`
- forward expression object: `192 B`
- symbolic `Equation` object: `1152 B`
- dual value: `16 B`

For a batched working set of `F4` objects, the current throughput numbers suggest:

- reverse mode has the best per-item throughput
- symbolic `Equation` objects are much larger
- forward mode pays both a larger object cost than plain symbolic expressions and a multi-pass gradient cost

Latest `F4` batched throughput snapshot:

- symbolic: `169.9M/s` at `256`, `139.8M/s` at `1024`, `118.6M/s` at `4096`
- reverse: `656.5M/s` at `256`, `317.8M/s` at `1024`, `261.0M/s` at `4096`
- forward: `25.5M/s` at `256`, `26.1M/s` at `1024`, `25.5M/s` at `4096`

This is not a direct hardware cache-miss measurement, but it is a practical proxy for locality and working-set pressure.

### Hardware counter measurements

If you want real cache-related counters, run the benchmark binary under `perf stat` on Linux:

```sh
perf stat -e cache-references,cache-misses,cycles,instructions \
  ./build/benchmarks --benchmark_filter='.*(Footprint|Batched).*' --benchmark_min_time=0.1s
```

For a more memory-focused view, you can also try:

```sh
perf stat -e LLC-loads,LLC-load-misses,L1-dcache-loads,L1-dcache-load-misses \
  ./build/benchmarks --benchmark_filter='.*Batched.*' --benchmark_min_time=0.1s
```

That gives you direct miss/load counters to compare symbolic, forward, and reverse mode under the same workload.

## Main types

### Primitives

- `Constant<T>` stores a fixed numeric value.
- `Variable<T, 'x'>` stores a named symbolic variable.

### Expression nodes

- `Expression<Op, LHS, RHS>` is a binary node.
- `MonoExpression<Op, Expr>` is a unary node.

### Operations

- Arithmetic: `+`, `-`, `*`, `/`
- Unary math: `sin`, `cos`, `exp`

### Higher-level helpers

- `Equation(expr)` precomputes symbolic partial derivatives for a scalar expression.
- `VectorEquation(exprs...)` represents a vector-valued function and its Jacobian.
- `Dual<T>` enables forward-mode AD.
- `reverse_mode_gradient(expr)` computes reverse-mode gradients for scalar expressions.
- `VectorEquation::eval_jacobian_reverse()` computes reverse-mode Jacobians for vector-valued expressions.

## Convenience helpers

| Syntax | Meaning |
|---|---|
| `PC(v)` | `Constant<decltype(v)>{v}` |
| `PV(v, 'x')` | `Variable<decltype(v), 'x'>{v}` |
| `3_ci` | `Constant<int>{3}` |
| `1.5_cd` | `Constant<double>{1.5}` |
| `4_vi` | `Variable<int, 'c'>{4}` |
| `2.0_vd` | `Variable<double, 'v'>{2.0}` |
| `idx<N>()` | Compile-time derivative index for `Equation` |
| `IDX(N)` | Legacy index macro |

## Examples

### Symbolic differentiation

```cpp
auto x = PV(4, 'x');
auto y = PV(2, 'y');
auto expr = x * y + PC(3) * x * y * y;

std::cout << expr.eval() << "\n";            // 56
std::cout << expr.derivative().eval() << "\n";
```

### Scalar partial derivatives

```cpp
auto x = PV(4, 'x');
auto y = PV(2, 'y');
auto eq = Equation(x * y);

std::cout << eq.eval() << "\n";              // 8
std::cout << eq[idx<1>()].eval() << "\n";    // df/dx = y = 2
std::cout << eq[idx<2>()].eval() << "\n";    // df/dy = x = 4
```

### Update variables in-place

```cpp
auto x = Variable<int, 'x'>{3};
auto eq = Equation(x * x);

using Syms = Equation<decltype(x * x)>::symbols;
eq.update(Syms{}, std::array{5});

std::cout << eq.eval() << "\n";              // 25
std::cout << eq[idx<1>()].eval() << "\n";    // 10
```

### Vector Jacobian from symbolic derivatives

```cpp
auto x = PV(3.0, 'x');
auto y = PV(4.0, 'y');
auto ve = VectorEquation(x + y, x * y);

auto values = ve.eval();         // {7.0, 12.0}
auto J = ve.eval_jacobian();     // {{1, 1}, {4, 3}}
```

### Forward-mode AD with dual numbers

```cpp
using D = Dual<double>;

Variable<D, 'x'> x{D{3.0, 1.0}};
auto [f, df] = (x * x + x).eval();

// f = 12, df/dx = 7
```

### Reverse-mode gradient

```cpp
auto x = PV(1.0, 'x');
auto y = PV(2.0, 'y');

auto g = reverse_mode_gradient(exp(x) * sin(y));
// g[0] = d/dx, g[1] = d/dy
```

### Forward-mode Jacobian for vector functions

```cpp
using D = Dual<double>;

Variable<D, 'x'> x{D{2.0}};
Variable<D, 'y'> y{D{3.0}};
auto ve = VectorEquation(x * y, sin(x) + y * y);

auto J = ve.eval_jacobian_forward({2.0, 3.0});
```

## Notes

- Symbol order is derived from variable labels and sorted at compile time.
- `Equation` and `VectorEquation` work with symbolic expressions and cache derivative expressions.
- `eval_jacobian_forward(...)` is only available when the expression value type is `Dual<T>`.
- The test suite covers symbolic differentiation, trig/exp rules, dual-number forward mode, reverse-mode gradients, and Jacobian agreement between methods.

## Contributing

This is a personal learning project, but suggestions and pull requests are welcome.

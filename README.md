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
- Use `gradient(expr)` for reverse-mode gradients of scalar expressions.
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

PowerShell users should quote regex filters:

```powershell
.\build-win\Release\benchmarks.exe "--benchmark_filter=F1|F2|F3|F4" --benchmark_min_time=0.05s
```

The benchmark suite compares three ways of computing a full gradient for the same scalar function:

- symbolic partials via `Equation(...).eval_derivatives()`
- forward mode via `Dual<T>` with one seeded pass per input
- reverse mode via `gradient(expr)`

The current suite uses four functions:

- `F1(x) = exp(x) * sin(x) + x^3 + 2x`
- `F2(x, y) = xy + sin(x) + y^2 + exp(x + y)`
- `F3(x, y, z) = exp(xy) + x sin(z) + yz + x^2 z`
- `F4(x, y, z, w) = (x + y)(z - w) + exp(xz) + sin(yw) + xyzw`

Current Release snapshot on this Windows machine:

| Function | Symbolic | Forward | Reverse |
|---|---:|---:|---:|
| `F1` | 14.0 ns | 9.38 ns | 6.23 ns |
| `F2` | 11.7 ns | 17.4 ns | 6.25 ns |
| `F3` | 23.4 ns | 20.9 ns | 6.23 ns |
| `F4` | 23.4 ns | 35.0 ns | 5.45 ns |

In this snapshot, reverse mode is fastest on all four benchmarked functions. Treat these numbers as machine- and compiler-dependent measurements rather than fixed library-wide conclusions.

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
- `gradient(expr)` computes reverse-mode gradients for scalar expressions.

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

auto g = gradient(exp(x) * sin(y));
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

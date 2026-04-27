[![CMake](https://github.com/reach2sayan/ExpressionSolver/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/reach2sayan/ExpressionSolver/actions/workflows/cmake-multi-platform.yml) [![C++](https://img.shields.io/badge/C++-%2300599C.svg?logo=c%2B%2B&logoColor=white)](#)

# Expression Differentiator

A header-only C++26 template library for symbolic mathematical expressions with automatic differentiation, partial derivatives, and Jacobian computation — all resolved at compile time.

## Features

- **Symbolic Expressions**: Build expression trees from `Variable`, `Constant`, and operator types
- **Automatic Differentiation**: Exact symbolic derivatives via chain, product, and quotient rules
- **Partial Derivatives**: `Equation` wraps a scalar expression and exposes all partial derivatives
- **Jacobian Matrices**: `VectorEquation` maps ℝⁿ → ℝᵐ and computes the full J[i][j] = ∂fᵢ/∂xⱼ
- **Transcendental Functions**: `sin`, `cos`, `exp` with correct derivative rules
- **Compile-time Evaluation**: Expressions over `Constant` values are `constexpr`
- **Type-safe**: Works with any numeric type that provides `+`, `-`, `*`, `/`  
  (or specializes `std::plus<>`, `std::minus<>`, `std::multiplies<>`, `std::divides<>`)

## Requirements

- C++26 compiler — Clang 17+ recommended (GCC 13 has an ICE on deeply nested Hana instantiations; GCC 14+ may work)
- [Boost](https://www.boost.org/) ≥ 1.74 (headers only; used for `boost::hana`)
- CMake ≥ 3.20

## Building

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

`CMakeLists.txt` automatically selects `clang++` when available.

## Core Components

### Primitives

| Type | Description |
|------|-------------|
| `Constant<T>` | A fixed numeric value; `derivative()` returns `Constant{0}` |
| `Variable<T, char>` | A named symbolic variable; `derivative()` returns `Constant{1}` |

### Expression nodes

| Type | Description |
|------|-------------|
| `Expression<Op, LHS, RHS>` | Binary expression node (addition, multiplication, division) |
| `MonoExpression<Op, Expr>` | Unary expression node (negation, sin, cos, exp) |

### Operations

| Type | Description |
|------|-------------|
| `SumOp<T>` | Addition — also used internally to implement subtraction (`a - b` ≡ `a + (-b)`) |
| `MultiplyOp<T>` | Multiplication |
| `DivideOp<T>` | Division |
| `NegateOp<T>` | Unary negation |
| `SineOp<T>` | sin |
| `CosineOp<T>` | cos |
| `ExpOp<T>` | eˣ |

Operator overloads (`+`, `-`, `*`, `/`, `sin()`, `cos()`, `exp()`) accept any `Expression`, `Variable`, or `Constant`, so you rarely touch these types directly.

### Equations

| Type | Description |
|------|-------------|
| `Equation<Expr>` | Wraps a scalar expression. Exposes `.eval()`, `.eval_derivatives()` (returns `std::array`), and `operator[idx<N>()]` for individual partials |
| `VectorEquation<Exprs...>` | Wraps multiple scalar expressions (ℝⁿ → ℝᵐ). Exposes `.eval()` and `.eval_jacobian()` |

## Convenience Macros and UDLs

| Syntax | Expands to |
|--------|------------|
| `PC(v)` | `Constant<decltype(v)>{v}` |
| `PV(v, 'x')` | `Variable<decltype(v), 'x'>{v}` |
| `3_ci` | `Constant<int>{3}` |
| `1.5_cd` | `Constant<double>{1.5}` |
| `4_vi` | `Variable<int, 'c'>{4}` |
| `2.0_vd` | `Variable<double, 'v'>{2.0}` |

Use `idx<N>()` (or the legacy `IDX(N)`) to index into an `Equation`'s derivative list.

## Usage Examples

### Scalar expression with partial derivatives

```cpp
auto x = PV(4, 'x');   // Variable<int, 'x'>{4}
auto y = PV(2, 'y');   // Variable<int, 'y'>{2}
auto expr = x * y + PC(3) * x * y * y;   // f(x,y) = xy + 3xy²

std::cout << expr.eval();   // 4*2 + 3*4*4 = 56

auto eq = Equation(expr);
auto [df_dx, df_dy] = eq.eval_derivatives();
// df/dx = y + 3y²  = 14
// df/dy = x + 6xy  = 52
```

### Indexing individual partials

```cpp
auto x = PV(4, 'x');
auto y = PV(2, 'y');
auto eq = Equation(x * y);

eq[idx<1>()].eval();   // ∂(x*y)/∂x = y = 2
eq[idx<2>()].eval();   // ∂(x*y)/∂y = x = 4
```

### Transcendental functions

```cpp
auto x = PV(1.0, 'x');
auto g = sin(x) * cos(x);
g.eval();                  // sin(1)*cos(1) ≈ 0.455
g.derivative().eval();     // cos²(x) − sin²(x) = cos(2) ≈ −0.416
```

### Jacobian of a vector-valued function

```cpp
auto x = PV(3.0, 'x');
auto y = PV(4.0, 'y');
// f: ℝ² → ℝ²,  f(x,y) = (x + y,  x * y)
auto ve = VectorEquation(x + y, x * y);

auto fval = ve.eval();         // {7.0, 12.0}
auto J    = ve.eval_jacobian();
// J = [[∂(x+y)/∂x, ∂(x+y)/∂y],   [[1, 1],
//      [∂(x*y)/∂x, ∂(x*y)/∂y]] =   [4, 3]]
```

### Rectangular Jacobian (ℝ² → ℝ³)

```cpp
auto x = PV(1.0, 'x');
auto y = PV(2.0, 'y');
auto ve = VectorEquation(x * x, sin(x) * y, x + y * y);
// output_dim == 3, input_dim == 2
auto J = ve.eval_jacobian();   // 3×2 std::array<std::array<double,2>,3>
```

### Updating variable values

```cpp
auto x = Variable<int, 'x'>{3};
auto eq = Equation(x * x);

eq.eval();            // 9
eq[idx<1>()].eval();  // 6  (= 2x)

using Syms = Equation<decltype(x * x)>::symbols;
eq.update(Syms{}, std::array{5});

eq.eval();            // 25
eq[idx<1>()].eval();  // 10  (= 2x)
```

## Contributing

This is a personal project, but contributions are welcome! I want to learn, so please comment. I don't promise to implement all suggestions, but I will think them through.
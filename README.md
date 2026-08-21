[![CMake](https://github.com/reach2sayan/ddx/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/reach2sayan/ddx/actions/workflows/cmake-multi-platform.yml)
[![C++](https://img.shields.io/badge/C++-%2300599C.svg?logo=c%2B%2B&logoColor=white)](#)

# ddx

A header-only C++23 library for symbolic expression trees and automatic
differentiation: symbolic derivatives, forward mode (dual numbers), reverse mode
(adjoint sweeps), Hessians, higher-order derivative tensors, and Taylor-mode
univariate derivatives — all usable at run time or inside `constexpr`.

```cpp
#include "ddx.hpp"
using namespace ddx;

constexpr auto x = var<"x">;
constexpr auto y = var<"y">;

auto f  = exp(x) * sin(y);
auto v  = Equation{f}.evaluate(1.0, 2.0);   // f(1, 2)
auto g  = Equation{f}.gradient(1.0, 2.0);   // {∂f/∂x, ∂f/∂y}
```

**Contents**

1. [Requirements and setup](#requirements-and-setup)
2. [Building expressions](#building-expressions)
3. [Supplying a point](#supplying-a-point)
4. [Derivatives: the `Equation` API](#derivatives-the-equation-api)
5. [Return types](#return-types)
6. [Getting the most speed out of it](#getting-the-most-speed-out-of-it)
7. [Compile-time use](#compile-time-use)
8. [Runtime expressions](#runtime-expressions)
9. [Thread safety](#thread-safety)
10. [Building the project](#building-the-project)
11. [Cheat sheet](#cheat-sheet)
12. [Diagnostics](#diagnostics)
13. [Further reading](#further-reading)

---

## Requirements and setup

- A C++23 compiler with a C++23 standard library: **GCC 14+**, or **Clang 17+**
  over libstdc++ 14+ / libc++ 17+. MSVC (VS 2022, `/std:c++latest`) is supported.
- **CMake 3.20+** if you build through CMake.

C++23 is a hard requirement — the library uses `constexpr std::bitset` inside
`consteval` functions and the multidimensional subscript `t[i, j, k]`.

The library is header-only and has no third-party dependencies. A reference
implementation of `std::mdspan` is vendored under `include/md/third_party/` and
is used automatically when your toolchain has no complete `<mdspan>`.

### Using it

With CMake, link the interface target — it puts `include/` on your include path:

```cmake
add_subdirectory(ddx)
target_link_libraries(my_app PRIVATE ddx::ddx)   # or: ddx
```

Then include the public header:

```cpp
#include "ddx.hpp"    // the whole public surface

using namespace ddx;  // assumed by every example below
```

### What `namespace ddx` contains

`ddx.hpp` puts fifteen names in `namespace ddx`, and that is the whole surface:

| Name | Purpose |
|---|---|
| `Equation` | the derivative API — every derivative entry point is a member |
| `var<"x">`, `sym<"x">`, `idx<N>()` | name a symbol; index into an `Equation` |
| `var_of<"x">(v)`, `dual_var_of<"x">(v)` | name a symbol, taking its scalar type from `v` |
| `constant(3.0)` | a value stored in the tree |
| `literals` — `"x"_s`, `_cd`, `_ci`, `_vd`, `_vi` | the user-defined literals, as a namespace |
| `named<"x">(v)`, `NamedValue` | one keyword argument of a point, or one map entry |
| `map(…)`, `Map` | a compile-time map of those entries |
| `DiffMode` | `Symbolic` vs `Reverse` |
| `dual`, `dual2nd` | the symbol value type `Equation::hessian` needs |

The operators (`+`, `*`, …), the math functions (`sin`, `exp`, …), `operator<<`
and the `std::formatter` specialisations are deliberately *not* on that list.
They are found by argument-dependent lookup, so they work on an expression
without their names being visible. `using namespace ddx;` therefore brings in
fifteen names, not a hundred and a half.

Every user-defined literal — `"x"_s` and the `_cd`, `_ci`, `_vd`, `_vi` suffixes
— lives in `ddx::literals`, so they arrive only where you ask for them:

```cpp
using namespace ddx::literals;    // "x"_s, 2.0_cd, 4_vi, …
```

The public API is macro-free, so nothing of the library sits at global scope.
Everything that deduces a type from a value — `constant(v)`, `var_of<"x">(v)`,
`dual_var_of<"x">(v)` — is an ordinary function template, and obeys namespaces
like the rest.

`ddx::rt` and `ddx::jit` are deliberately not among them: they are opt-in, they
carry dependencies, and `ddx.hpp` does not reach for either. See
[Runtime expressions](#runtime-expressions).

---

## Building expressions

### Symbols

A variable is a **name**, spelled `var<"x">`:

```cpp
constexpr auto x = var<"x">;              // a double-valued symbol named "x"
constexpr auto n = var<"n", int>;         // another scalar type
auto xd = var<"x", dual>;                 // dual-valued symbol (needed for hessian())
```

Equivalent spellings, including the ones that deduce the scalar type from an
exemplar value rather than naming it:

| Syntax | Means |
|---|---|
| `var<"x">` | a symbol named `"x"`, valued `double` |
| `var<"x", T>` | a symbol named `"x"`, valued `T` |
| `var_of<"x">(v)` | a symbol named `"x"`, valued `decltype(v)` — `v` supplies only the *type* |
| `dual_var_of<"x">(v)` | the same, dual-valued: `var<"x", dual>` when `v` is a `double` |
| `2.0_vd` | a `double` symbol named `"v"` (needs `ddx::literals`) |
| `4_vi` | an `int` symbol named `"c"` (needs `ddx::literals`) |

### Constants

| Syntax | Means |
|---|---|
| `constant(3.0)` | a `double` constant stored in the tree |
| `1.5_cd` | the same, as a literal (needs `ddx::literals`) |
| `3_ci` | an `int` constant (needs `ddx::literals`) |

A bare scalar mixed with an expression is promoted automatically, so `x * 2.0`
and `2.0 * x + 1.0` need no wrapping. Use `constant` only when you want the
constant spelled explicitly.

### Operators and functions

| Kind | Available |
|---|---|
| Arithmetic | `+`  `-`  `*`  `/`  unary `-` |
| Unary functions | `sin` `cos` `tan` `exp` `log` `log10` `sqrt` `cbrt` `abs` `asin` `acos` `atan` `sinh` `cosh` `tanh` `asinh` `acosh` `atanh` `erf` |
| Binary functions | `pow` `atan2` `hypot` `max` `min` |

```cpp
auto f = (x + y) * (x - y) + exp(x * y) + sin(y) * x * y;
auto g = pow(x, 2.0) + hypot(x, y) - log(max(x, y));
```

Trees are simplified as they are built: `x + 0`, `x * 1`, `x * 0`, `x / 1`,
`(a / x) * x`, `-(-x)`, `pow(x, 0)`, `pow(x, 1)` and literal folding are applied
by the operators themselves, so a derivative comes out as `y` rather than
`1 * y + x * 0`, and `d(x log x)/dx` as `1 + log(x)` rather than
`log(x) + x * (1 / x)`.

### Symbols carry no value

A symbol holds nothing, and neither does any node built from it. An expression
over symbols is an *empty type*, whatever its depth and however often a symbol
repeats:

```cpp
auto f = (x + y) * (x - y) + exp(x * y) + sin(y) * x * y;
static_assert(std::is_empty_v<decltype(f)>);   // sizeof(f) == 1
```

The point is supplied where you ask for a value or a derivative, never where the
tree is built. One value slot exists per *symbol*, not per leaf occurrence, and
an expression is free to copy, store, and pass by value.

### Printing

Expressions and `Equation`s are formattable and streamable:

```cpp
std::format("{}", x * y + sin(x));      // "x * y + sin(x)"
std::format("{}", x / (y / x));         // "x / (y / x)"  — parentheses as needed
std::format("{::.3f}", 2.0 * x);        // "2.000 * x"    — spec applies to every number
std::cout << (x - y * x) << '\n';       // "x - y * x"
```

For an `Equation` the format prints each function followed by its gradient row.
A symbol held constant for the purpose of one partial derivative prints with a
`_c` suffix:

```
f0: x * y
  grad: y_c, x_c
```

---

## Supplying a point

Every entry point that needs numbers accepts the point in three interchangeable
spellings:

```cpp
auto f = x * y + constant(3.0) * x;                 // f(x, y) = xy + 3x

f.eval(4.0, 2.0);                                   // positional, canonical order
f.eval(named<"y">(2.0), named<"x">(4.0));           // by name, order-independent
f.eval(std::array{4.0, 2.0});                       // any input range
```

All of these give `20`. The same spellings work for `Equation::evaluate`,
`gradient`, `jacobian`, `hessian`, `derivative_tensor` — one normaliser serves
them all.

### Canonical order

**Canonical order is alphabetical by symbol name, not the order you wrote them
in.** For `f(w, x, y, z)` the positional form expects `w` first — and for
`f(x, y)` above, `x` then `y`.

The positional and range forms check the count at compile time, so a missing or
extra value is a compile error. The named form binds by name and is immune to
ordering entirely — prefer them when the symbol set is large. A
non-sized range that runs short throws `std::out_of_range` (a compile error
during constant evaluation).

### A compile-time map

The same keyword arguments also stand on their own, as `map`:

```cpp
constexpr auto m = map(named<"n">(3), named<"x">(1.5));

static_assert(m.get<"n">() == 3);          // int
static_assert(m["x"_s] == 1.5);            // double
static_assert(m.contains<"n">());
static_assert(m.size == 2);
```

The keys live in the type, so a lookup is a member access the compiler has
already resolved — there is no search, no hashing, and nothing to keep in step
at run time. The values do *not* have to share a type: each slot keeps exactly
what it was given, which is what separates this from the point of an
expression.

**The entries, in every spelling.** A map is an aggregate over its entries, so
braces work as well as the `map(…)` call, and an entry can be written as a
keyword argument, as a label/value pair, or as a variable/value pair:

```cpp
constexpr auto x = var<"x">;
constexpr auto n = var<"n", int>;

Map{named<"n">(3), named<"x">(1.5)};                  // keyword argument
Map{NamedValue{"n"_s, 3}, NamedValue{x, 1.5}};        // label pair, variable pair
map(named(n, 3), named(x, 1.5));                      // named(), keyed by a tag
Map<NamedValue<"n", int>, NamedValue<"x", double>>{{3}, {1.5}};   // type spelled out
```

All four are the same type. What no spelling can do is `{"n", 3}` with a bare
string: a braced list has no type for the compiler to read `"n"` out of, and the
key has to reach the *type* — so the label comes in as `"n"_s`, as `var<"n">`,
or as the `named<"n">` template argument.

**Everything else it answers.** Writing a slot is in place; adding or removing
one gives a new map, since the keys are part of the type:

```cpp
auto m2 = m;
m2.set<"x">(2.5);                          // in place — same type
m2["x"_s] = 2.5;                           // the same write

constexpr auto m3 = m.insert(named<"y">('c'));   // 3 entries, m untouched
constexpr auto m4 = m3.erase<"n">();             // 2 entries, order preserved
constexpr auto m5 = m.erase<"n">().insert(named<"n">(2.5f));   // "n" is now float

m.for_each([](auto key, const auto &v) { /* key.name is "n", then "x" */ });
m.keys();                                  // {"n", "x"} — entry order, not sorted
```

`m == m2` compares keys in order and then values; a map whose keys are permuted
is a different type and does not compare at all. Asking for a key that is not
there is a compile error (`Map: key not present`), as is a duplicate key.

A map is a container, not a point: `f.eval(…)` takes the `named<"x">(v)`
arguments themselves, not a map of them.

## Derivatives: the `Equation` API

`Equation` is the derivative API. Wrap one expression for a scalar function
f: ℝⁿ → ℝ, or several for a vector function f: ℝⁿ → ℝᵐ:

```cpp
auto eq  = Equation{x * y};                    // scalar
auto sys = Equation(x + y, x * y, sin(x));     // three outputs
```

`n` is the number of distinct symbols across all outputs, in canonical order;
`m` is the number of outputs. Both are available as
`decltype(eq)::input_dim` / `::output_dim`.

### Scalar functions

```cpp
constexpr auto x = var<"x">;
constexpr auto y = var<"y">;
auto eq = Equation{exp(x) * sin(y)};
const std::array pt{1.0, 2.0};

eq.evaluate(pt);                          // f(x, y)
eq.gradient(pt);                          // reverse mode — one backward sweep
eq.gradient<DiffMode::Symbolic>(pt);      // evaluate the stored partial trees
eq.derivative_tensor<1>(pt);              // forward mode gradient
eq.derivative_tensor<2>(pt);              // forward-over-forward Hessian
```

Every one of them also takes positional or named arguments:

```cpp
eq.gradient(1.0, 2.0);
eq.gradient(named<"y">(2.0), named<"x">(1.0));
```

#### Hessians

There are two Hessians, and they differ in what the symbols must be:

```cpp
// forward-over-forward: plain scalar symbols
auto H1 = Equation{x * y}.derivative_tensor<2>(std::array{2.0, 3.0});

// forward-over-reverse: dual-valued symbols
auto a = var<"x", dual>;
auto b = var<"y", dual>;
auto H2 = Equation{a * b}.hessian(std::array{2.0, 3.0});
```

`hessian()` needs `dual`-valued symbols because it seeds tangents into the
tree; `derivative_tensor<K>` builds its nested duals internally and works on
plain scalar symbols. Either way the *point* is given in the base scalar type
(`double` above), never in the dual type.

#### Higher-order and univariate derivatives

`derivative_tensor<K>` generalises to any order: it returns a rank-K symmetric
tensor of all K-th order partials.

```cpp
auto T3 = Equation{x * y + sin(x)}.derivative_tensor<3>(std::array{1.0, 2.0});
T3[0, 0, 1];   // ∂³f/∂x²∂y
```

For a **single-variable** function, `univariate_derivative<K>` is far cheaper —
it runs one Taylor-mode sweep in O(K²) instead of building a rank-K tensor:

```cpp
double d4 = Equation{sin(x)}.univariate_derivative<4>(1.0);   // f⁗(1.0)
```

#### The symbolic partials

An `Equation` can hand back the derivative *expressions* themselves. Slot 0 is
the function; slot k > 0 is the partial with respect to the k-th symbol in
canonical order:

```cpp
auto eq = Equation(x * y);

eq[idx<0>()];              // x * y
eq[idx<1>()].eval(2.0);    // ∂f/∂x = y  → 2
eq[idx<2>()].eval(4.0);    // ∂f/∂y = x  → 4
eq.get<1>();               // same as eq[idx<1>()]
```

Because the partials are simplified, each names only the symbols it actually
depends on and takes just those values — `∂(xy)/∂x` is `y`, a one-symbol
expression. The symbols that were held constant to form a partial print with a
`_c` suffix (`y_c`); they still read their value from the point as usual, their
derivative is simply zero. Like any expression the partials are empty types, so
holding them is free.

### Vector functions

```cpp
auto sys = Equation(x * y, sin(x) + y * y);
const std::array pt{1.0, 2.0};

sys.evaluate(pt);                          // std::array<double, 2>
sys.jacobian(pt);                          // reverse mode, J[i][j] = ∂fᵢ/∂xⱼ
sys.jacobian<DiffMode::Symbolic>(pt);      // symbolic
sys.derivative_tensor<1>(pt);              // forward-mode Jacobian
sys.derivative_tensor<2>(pt);              // per-output Hessians, H[k][i][j]
```

The reverse-mode per-output Hessian is available too, again on `dual`-valued
symbols:

```cpp
auto a = var<"x", dual>;
auto b = var<"y", dual>;
auto H = Equation(a * b, a * a).hessian(std::array{2.0, 3.0});
// H[0][i][j] = ∂²(xy)/∂xᵢ∂xⱼ,  H[1][i][j] = ∂²(x²)/∂xᵢ∂xⱼ
```

### Choosing a mode

| Call | Mode | Symbols | Cost |
|---|---|---|---|
| `gradient(pt)` | reverse | plain or dual | one backward sweep |
| `gradient<DiffMode::Symbolic>(pt)` | symbolic | plain or dual | evaluates n partial trees |
| `derivative_tensor<1>(pt)` | forward | plain | n forward sweeps |
| `hessian(pt)` | forward-over-reverse | **`dual` required** | one sweep per *colour* — see below |
| `derivative_tensor<2>(pt)` | forward-over-forward | plain | one sweep per index pair |
| `derivative_tensor<K>(pt)` | forward | plain | one sweep per distinct K-index |
| `univariate_derivative<K>(x0)` | Taylor | plain, n = 1 | one sweep, O(K²) |

Which to reach for is in [Getting the most speed out of
it](#getting-the-most-speed-out-of-it); the short version is reverse for a
gradient of many variables, and symbolic when you want to *see* the derivative
rather than just evaluate it.

There is no `DiffMode::Forward` — forward mode is reached through
`derivative_tensor<K>` and `univariate_derivative<K>`.

### One-shot forward tangent

To get a value and one directional derivative in a single pass, without an
`Equation`:

```cpp
auto t = (x * y).eval_with_tangent<"x">(4.0, 2.0);
t.value();   // 8
t.deriv();   // ∂(xy)/∂x = 2
```

It returns a `dual`, seeded on the named symbol. Arguments are positional, in
canonical order.

---

## Return types

Everything below is an owning value; the library keeps no reference to it.

| Call | Returns |
|---|---|
| `eval` / `evaluate` (m = 1) | the scalar type `T` |
| `evaluate` (m > 1) | `std::array<T, m>` |
| `gradient` | `std::array<S, n>` |
| `jacobian` | rank-2 tensor, `m × n` |
| `hessian` / `derivative_tensor<2>` (m = 1) | rank-2 tensor, `n × n` |
| `hessian` / `derivative_tensor<2>` (m > 1) | rank-3 tensor, `m × n × n` |
| `derivative_tensor<K>` (m = 1) | rank-K tensor, `n^K` |
| `derivative_tensor<K>` (m > 1) | rank-(K+1) tensor, `m × n^K` |
| `univariate_derivative<K>` | the scalar type `S` |

`S` is the base scalar type: for a `dual`-valued expression it is
`double`. A single-output system carries no leading output axis — a scalar
function's Hessian is an `n × n` matrix, not a `1 × n × n` stack.

### Tensors

The tensor type accepts two equivalent index spellings:

```cpp
auto H = Equation{x * y}.derivative_tensor<2>(std::array{2.0, 3.0});

H[0, 1];        // the mdspan spelling
H[0][1];        // the nested spelling — same element
H.extent(0);    // n
decltype(H)::rank();
H.data();       // contiguous storage (packed: symmetric entries are stored once)
```

Symmetric derivative tensors use a packed layout, so `data()` holds fewer
elements than `n^K`; index it through `[i, j]` / `[i][j]` rather than assuming a
dense stride.

---

## Getting the most speed out of it

**Pick the call that matches the shape of the problem.** This is worth more than
everything else on this page put together.

| You want | Call | What it costs |
|---|---|---|
| a gradient, many variables | `gradient(pt)` | one backward sweep, independent of n |
| a gradient, two or three variables | `gradient<DiffMode::Symbolic>(pt)` | n folded partial trees — often quicker at that size |
| a Hessian | `hessian(pt)`, symbols declared `dual` | one sweep per colour of the sparsity pattern |
| a Hessian without dual-valued symbols | `derivative_tensor<2>(pt)` | one sweep per index pair — O(n²) |
| the K-th derivative of one variable | `univariate_derivative<K>(x0)` | one Taylor sweep, O(K²) |
| all K-th partials of n variables | `derivative_tensor<K>(pt)` | one sweep per distinct K-index |

Reverse is the default for a reason, but it is not a clean win at every size: on
a three-symbol system here, `jacobian<DiffMode::Symbolic>` runs in 7.7 ns against
12.5 ns for `jacobian<DiffMode::Reverse>`, because at that width the folded
partial trees are cheaper than a sweep. Reverse pulls away as n grows. If n is
small and the call is hot, measure both — it is a one-word change.

**`hessian()` reads your problem's sparsity off the type, for free.** Two
variables that never appear in the same second-derivative term can be seeded in
the *same* backward sweep, so the cost is one sweep per *colour* of the coupling
pattern rather than one per variable — and the pattern is computed from the
expression type at compile time, so there is nothing to switch on and nothing to
pay at run time. On an 8-variable chain (each variable coupled only to its
neighbours, plus one long-range term) that is 5 sweeps instead of 8, measured
here at **1.7–1.9x** faster end to end. A dense problem colours in n and costs
exactly what it always did. The wider and more structured the problem, the more
this wins — so prefer `hessian()` over `derivative_tensor<2>()` whenever you can
declare the symbols `dual`.

**Prefer `univariate_derivative<K>` whenever the function really has one
variable.** `derivative_tensor<K>` builds a rank-K tensor to hold a single
number; the Taylor sweep is O(K²) and allocates nothing.

**Do not worry about rebuilding the tree.** Expressions and `Equation`s are
empty types — `sizeof(Equation{f})` is 1 — and the whole structure lives in the
type, so `Equation{f}.gradient(pt)` inside a loop constructs nothing at run
time. Hoisting it into a variable is a readability choice, not a speed one.

**Let the tree be simplified for you.** Algebraic folding happens as the
expression is built, so `∂(xy)/∂x` really is the single node `y`, and the
partials each name only the symbols they still depend on. Nothing you can write
by hand beats it, and hand-expanding an expression usually makes it worse.

**Give the point in the base scalar type.** A `dual`-valued expression still
takes a point of `double`; the seeding happens inside. Positional and named
arguments cost exactly the same — the reordering is resolved at compile time.

**Compile with the flags the project already sets.** `-ffp-contract=fast` and
`-fno-math-errno` (`DDX_FP_FLAGS=ON`, the default) are both worth having.
`-ffast-math` is **not**: it was measured at 19% *slower* here, and it changes
derivative values. `-march=native` (`ENABLE_NATIVE_ARCH=ON`) is on by default.

**Know where the time actually goes.** For a gradient of anything with `exp`,
`log`, `sin` or `pow` in it, the libm call dominates — around three quarters of
the total. Reducing the number of transcendental calls in the expression is the
optimisation with the most left in it; shaving arithmetic nodes around them is
not.

**Many points at once is a different question.** Everything above is one point per
call. If what you have is thousands of them, the libm call that dominates can be
vectorised, which the compile-time path cannot do for you — see
[Runtime expressions](#runtime-expressions) for the batch kernel and what it
measures at.

**Move it to compile time if the point is known.** Every entry point is
`constexpr`; see the next section.

---

## Compile-time use

Evaluation *and* differentiation are `constexpr`. A gradient, a Hessian or a
Taylor-mode derivative can be computed during constant evaluation and baked into
the binary:

```cpp
constexpr auto x = var<"x">;
constexpr auto y = var<"y">;

constexpr auto g  = Equation{x * y}.gradient(std::array{3.0, 4.0});
constexpr auto gf = Equation{x * y}.derivative_tensor<1>(std::array{3.0, 4.0});
constexpr auto H  = Equation{x * y}.derivative_tensor<2>(std::array{3.0, 4.0});
constexpr auto d2 = Equation{x * x * x}.univariate_derivative<2>(2.0);

static_assert(g[0] == 4.0 && gf[0] == 4.0 && H[0][1] == 1.0 && d2 == 12.0);
```

These are `constexpr`, not `consteval`: the same call serves a `static_assert`
and a value read from a file at run time.

---

## Runtime expressions

Everything above needs the expression written in source, because the tree lives in
the type. An expression assembled at run time — terms looped over from a data file,
a coupling read from a configuration — has no type to live in, so it lives in a
graph instead.

`ddx::rt` is that graph and `ddx::jit` compiles it to native code. Both are opt-in;
the core stays header-only and dependency-free either way.

```cpp
#include "rt/derivative.hpp"
using namespace ddx::rt;

Builder b;
auto x = var(b, "x");
auto y = var(b, "y");
auto f = exp(x) * sin(y);        // the same operators, resolved at run time

auto g = gradient(b, f.id(b));   // ∂f/∂x and ∂f/∂y, as more graph nodes
                                 // (or let GraphBuilder do it, below)
```

`Expr` is a handle onto one node. The operators and the eighteen unary functions are
the ones you already have, and the derivative rules are literally the same rules:
`unary_math.hpp` writes them against `Numeric`, so instantiating them at `Expr`
builds nodes instead of computing numbers. Adding a row to `DDX_UNARY_MATH_TABLE`
gives the runtime path the function and its derivative at once.

"Runtime" names when the *structure* is decided, not when the arithmetic runs. Every
one of these calls is `constexpr`, so a graph assembled from values a constant
expression can see is itself a constant expression:

```cpp
consteval double slope() {
  Builder b;
  const auto x = var(b, "x");
  const auto g = gradient(b, (x * x).id(b));
  return evaluate_all(b, std::array{3.0})[g.partial[0]];
}
static_assert(slope() == 6.0);
```

Freezing is where that stops: `Graph` and the JIT are run-time only.

### The graph folds as it is built

`Builder` interns: forming a subexpression that already exists returns the existing
node, so `exp(x) * sin(y)` written twice is one subgraph and structural identity is a
`std::uint32_t` compare. The rewrites of [`simplify.hpp`](include/expr/simplify.hpp)
run on the way in, exactly as the operator factories run them at compile time — `x+0`,
`x*1`, `x*0`, `x/1`, `-(-x)`, `(n/d)*d → n`, and constant folding. Literals fold before
they ever reach a graph, so `Expr{2} * Expr{3} + Expr{1}` costs no nodes at all.

`gradient` is one reverse sweep over the whole graph, accumulating *nodes* rather than
values — the structural analogue of the sweep in
[`drivers/symbolic.hpp`](include/drivers/symbolic.hpp). One sweep produces every
partial, and they share every subexpression they can. Partials come back in
`Builder::symbols()` order, which is the order the symbols were first seen.

### Freezing and compiling

Freezing gives the static graph: a `Builder` can still be added to, a `Graph` cannot.
Whatever you freeze with is exactly what the kernel computes.

```cpp
#include "jit/kernel.hpp"

const auto graph = GraphBuilder{b}.value(f).gradient().build();

ddx::jit::Compiler compiler;
const auto kernel = compiler.compile(graph);
```

`GraphBuilder` names the outputs one step at a time — `value` is the function and
what `gradient` differentiates, `output` adds a column of anything else — and `build`
is the only thing that produces a `Graph`. `.value(f).build()` alone gives a kernel
that computes just the value.

The kernel takes columns, not points:

```cpp
const std::array<const double *, 2> xs{x_column, y_column};
const std::array<double *, 1> values{f_column};
const std::array<double *, 2> partials{dx_column, dy_column};
kernel(xs, values, partials, {}, n);
```

`xs[j]` is the column for symbol `j` and `partials[j]` the column for the partial in
it, each of length `n`. The four blocks are the symbols, the values, the Jacobian and
the Hessian; a block the graph was not frozen with is `{}` here rather than a null
pointer whose length the kernel would have to infer, and it is never read.

A `Kernel` is a function pointer plus a share of the `Compiler` that emitted it, so it
may outlive that `Compiler` and still be called; a copy costs one atomic increment and
a call costs nothing. The flip side is that one surviving `Kernel` holds the whole
LLJIT — the target machine, the symbol generators, every module compiled through it —
so dropping a `Compiler` reclaims that memory only once the last kernel from it is
gone.

For a graph without the JIT there is `evaluate(b, root, point)`, a plain walk in node
order. It is the reference the compiled kernels are tested against, and it is what you
get when `DDX_BUILD_JIT` is off.

A kernel's IR prints like anything else in the library — `ddx::jit::Ir` has a
`std::formatter` and an `operator<<`, next to the one for expressions in
[`format.hpp`](include/expr/format.hpp):

```cpp
std::cout << ddx::jit::Ir{compiler, graph};   // or std::format("{}", ...)
```

### Crossing over from a compile-time expression

`to_graph` lowers a typed tree into a graph, which is useful when the shape is known
but the *use* is a batch, and is how the two paths are tested against each other:

```cpp
constexpr auto x = ddx::var<"x">;
Builder b;
auto root = to_graph(b, exp(x) * sin(ddx::var<"y">));
```

### What this is worth

Per point the JIT does not beat the compile-time path and is not meant to: there the
expression is already inlined straight-line code, and a JIT'd call has the same libm
calls plus an indirect call. The batch kernel wins by vectorising, and the thing worth
vectorising is the libm call that
[dominates a gradient](#getting-the-most-speed-out-of-it).

Gradient of `exp(x) * sin(y)`, one machine, `-march=native`, medians of five runs:

| n | `Equation::gradient` | batch kernel | |
|---|---|---|---|
| 1 | 13.2 ns | 16.3 ns | compile-time path 1.2x |
| 1 000 | 13.5 µs | 3.44 µs | **kernel 3.9x** |
| 1 000 000 | 13.6 ms | 4.53 ms | **kernel 3.0x** |

Compiling a gradient kernel costs about 9.3 ms, paid once.

That advantage is specific to transcendentals. A polynomial gradient runs *slower*
through the kernel — around 1.4x at a thousand points and 1.3x at a million — and not
because it failed to vectorise: it vectorises four wide as well. It writes three columns
an iteration where the loop writes two, and at that size the work is memory-bound, so
the extra column is most of the difference. Cheap arithmetic in bulk is a memory
problem, and a better code generator does not help with those. Those two figures are
also the least stable in the suite, at around 10% run-to-run against 2-3% elsewhere,
for the same reason.

The real reason to reach for any of this is the first paragraph: an expression that is
not known until the program runs. The speed is why the batch shape is the one on offer.

### Vector math library

The loop vectoriser only vectorises `sin`, `exp` and the rest if it is told there are
vector forms to call. `Options::veclib` defaults to `Auto`, which uses glibc's libmvec
on x86-64 Linux; glibc 2.35 and later covers every function in
`DDX_UNARY_MATH_TABLE` plus `pow`, `atan2` and `hypot`. `VecLib::None` turns it off,
which is the escape hatch if a mapping ever names a symbol the host's glibc lacks —
the loop still compiles and still gives the same answers, it simply stays scalar
through those calls.

Nothing enables reassociation. `-ffast-math` is no more welcome here than anywhere
else in the project, for the same reason: it changes derivative values.

### Options follow how the project was built

`Options` starts from the build rather than from a fixed constant, so a kernel agrees
with the compile-time path by default:

| Field | Default |
|---|---|
| `contract` | `DDX_FP_FLAGS` — a kernel that contracts where `Equation` does not would give different derivative values for the same expression |
| `opt_level` | `1` for a `Debug` build, `3` for `Release`, `2` otherwise |

`ddx::jit::default_opt_level` and `default_contract` name those, and both fields are
still ordinary members you can set per compile.

The optimisation level is never `0`. No predefined macro carries the level — GCC and
Clang define only `__OPTIMIZE__`, and only as a yes/no — so CMake is the one place that
knows it, and a debug build is mapped to `1` rather than `0` deliberately: `-O0` turns
the loop vectoriser off, and vectorising is the only reason a batch kernel is worth
compiling. Debugging your own program should make the JIT cheaper to invoke, not
pointless.

---

## Thread safety

Nothing in the library starts a thread, and nothing in it takes a lock. That is a
deliberate reading of where the parallelism in this problem actually is: one point per
call is nanoseconds of work, which no thread pool can pay for, and a batch of thousands
is the caller's loop and the caller's pool. What the library owes that caller is a
statement of what may be shared. Here it is.

**The compile-time path is pure.** An `Equation` is an empty type and every entry point
on it reads only its argument, so `evaluate`, `gradient`, `hessian`,
`derivative_tensor<K>` and `univariate_derivative<K>` may be called from any number of
threads on the same expression. There is nothing to synchronise because there is
nothing shared to begin with.

**A runtime model belongs to one thread while it is being built.** `equation()` makes
its arena current through a thread-local, so two threads assembling models at the same
time never see each other's; that is what the arena is thread-local *for*. A `Builder`
is not itself synchronised, so one `Builder` is one thread's until it is frozen.

**A frozen `Graph` is immutable.** `freeze` copies out of the builder and a `Graph` has
no mutating operation, so it may be read, printed, and compiled from any number of
threads at once.

**One `Compiler` may compile from several threads.** LLJIT synchronises its own tables
and the name each module is added under is handed out atomically, so concurrent
`compile()` calls on one `Compiler` are well defined. They are not, however, fast:
compiling is a single-threaded LLVM pipeline of a few milliseconds, and several at once
contend inside LLVM. One `Compiler` per thread avoids that, at the cost of one LLJIT
and one set of host-symbol generators each.

**A `Kernel` is reentrant, and the batch is where parallelism belongs.** The emitted
code reads no global and writes no static: every intermediate is a register, and the
only memory it touches is the columns handed to it. So a batch splits by slicing the
columns, with no synchronisation of any kind:

```cpp
const std::size_t chunk = (n + threads - 1) / threads;
std::vector<std::jthread> pool;
for (std::size_t t = 0; t < n; t += chunk) {
  const std::size_t m = std::min(chunk, n - t);
  pool.emplace_back([&, t, m] {
    const std::array<const double *, 2> xs{x.data() + t, y.data() + t};
    const std::array<double *, 1> values{f.data() + t};
    const std::array<double *, 2> partials{dx.data() + t, dy.data() + t};
    kernel(xs, values, partials, {}, m);
  });
}
```

Slice on the vector width if the difference matters; the tail of a chunk is a scalar
remainder, and one per thread rather than one per batch is the only cost of splitting.

**Lifetime needs no rule.** A `Kernel` shares ownership of the JIT its code lives in,
so a `Compiler` that goes out of scope while threads are still calling kernels frees
nothing they are using — the last `Kernel` to go frees the code. There is no pool to
join before the `Compiler` leaves scope, and no way to spell the bug where a thread
calls into an unmapped page.

---

## Building the project

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Or through the presets, which need CMake 3.21+ (the library itself still only needs
3.20):

| Preset | Build type | Runtime graph | JIT |
|---|---|---|---|
| `debug` | Debug | — | — |
| `release` | Release | — | — |
| `relwithdebinfo` | RelWithDebInfo | — | — |
| `debug_with_rt` | Debug | yes, interpreted | — |
| `release_with_rt` | Release | yes, interpreted | — |
| `debug_with_jit` | Debug | yes | kernels at `-O1` |
| `release_with_jit` | Release | yes | kernels at `-O3` |

The `_with_rt` pair is the runtime graph on the interpreter. It needs only the
header-only Boost the build fetches, so it configures where the JIT presets do
not — they are gated on Linux and on an LLVM install. It is also the path a
graph over anything but `double` takes anyway: the JIT emits machine types, so
`Equation` over a dual, a Taylor dual or a matrix interprets even in a JIT
build.

```sh
cmake --preset release_with_jit
cmake --build --preset release_with_jit
ctest --preset release_with_jit
```

The two JIT presets pin `LLVM_DIR` to the Debian/Ubuntu `llvm-20` layout, because an
unhinted `find_package(LLVM)` takes whichever it finds first — often too old. Override
it on the command line, which wins over the preset:

```sh
cmake --preset release_with_jit -DLLVM_DIR=/opt/llvm-19/lib/cmake/llvm
```

or keep your own layout in an untracked `CMakeUserPresets.json`.

Benchmarks:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target benchmarks
./build/benchmarks
```

See [BENCHMARKS.md](benchmarks/BENCHMARKS.md) for the suite description and
results, and `src/main.cpp` for a runnable tour of every entry point.

[Runtime expressions](#runtime-expressions) are off by default, because they are the
one part of the library with third-party dependencies:

```sh
cmake -S . -B build -DDDX_BUILD_JIT=ON -DLLVM_DIR=/usr/lib/llvm-20/lib/cmake/llvm
cmake --build build --target tests_rt tests_jit
```

`ddx::rt` needs Boost.Graph and Boost.DynamicBitset, fetched at a pinned version during
configuration. Both are header-only, so no compiled Boost library is linked and a
system Boost is not consulted. `ddx::jit` adds LLVM 18–20 — the ORC API is not stable
across releases, so the range is checked rather than assumed. Neither is reachable
from `ddx::ddx`, which remains header-only and standard-library-only.

### CMake options

| Option | Default | Meaning |
|---|---|---|
| `DDX_BUILD_BENCHMARKS` | `ON` | build the Google Benchmark targets |
| `ENABLE_NATIVE_ARCH` | `ON` | `-march=native` (falls back to `x86-64-v3`) |
| `DDX_FP_FLAGS` | `ON` | `-ffp-contract=fast -fno-math-errno` |
| `DDX_MDSPAN_MODE` | `auto` | `auto` / `std` / `vendored` — which `mdspan` to bind to |
| `DDX_DEDUCING_THIS` | `auto` | `auto` / `on` / `off` — accessor spelling (P0847) |
| `DDX_BUILD_RT` | `OFF` | the runtime expression graph — fetches header-only Boost.Graph and Boost.DynamicBitset |
| `DDX_BUILD_JIT` | `OFF` | the LLVM JIT backend — implies `DDX_BUILD_RT` |

`-ffast-math` is not used and is not recommended: it changes derivative values.

---

## Cheat sheet

```cpp
// symbols and expressions
constexpr auto x = var<"x">;  constexpr auto y = var<"y">;
auto f = exp(x) * sin(y) + pow(x, 2.0);

// values
f.eval(1.0, 2.0);                              // positional (alphabetical order!)
f.eval(named<"y">(2.0), named<"x">(1.0));      // by name

// a compile-time map of the same keyword arguments
constexpr auto m = map(named<"n">(3), named<"x">(1.5));
static_assert(m.get<"n">() == 3 && m["x"_s] == 1.5);
m.insert(named<"y">('c'));                     // new key, new type

// scalar derivatives
auto eq = Equation{f};
eq.evaluate(1.0, 2.0);
eq.gradient(1.0, 2.0);                         // reverse mode
eq.gradient<DiffMode::Symbolic>(1.0, 2.0);
eq.derivative_tensor<1>(1.0, 2.0);             // forward gradient
eq.derivative_tensor<2>(1.0, 2.0);             // forward Hessian
eq[idx<1>()];                                  // ∂f/∂x as an expression

// dual-valued symbols → reverse-mode Hessian
auto a = var<"x", dual>;  auto b = var<"y", dual>;
Equation{exp(a) * sin(b)}.hessian(1.0, 2.0);

// univariate higher order
Equation{sin(x)}.univariate_derivative<4>(1.0);

// vector systems
auto sys = Equation(x * y, sin(x) + y * y);
sys.jacobian(1.0, 2.0);
sys.derivative_tensor<2>(1.0, 2.0);            // per-output Hessians

// single expression → Equation, implicitly
Equation eq2 = x * y;                          // no braces needed
```

---

## Diagnostics

Common compile-time messages and what they mean:

| Message | Cause |
|---|---|
| `eval: supply exactly one value per symbol, in canonical order` | wrong number of positional values — canonical order is alphabetical |
| `eval: no value supplied for this symbol` | the point does not cover every symbol |
| no matching call to `hessian` | the symbols are not `dual`-valued — use `var<"x", dual>`, or use `derivative_tensor<2>` |
| `Map: key not present (see keys())` | the map has no such key — `keys()` lists the ones it has |
| `map: duplicate key` / `Map: duplicate key` | two entries name the same key |

At run time the library throws only where a wrong point would otherwise pass
silently, and always `std::out_of_range`: an input range that supplies fewer
values than the expression has symbols.
Nothing on the evaluation path is `noexcept` for that reason — silently
differentiating at the wrong point is worse than an exception.

---

## Further reading

Worked walkthroughs of the algorithms, each doing one derivative by hand and
then pointing at the code that does it:

| Document | Covers |
|---|---|
| [docs/reverse_mode_by_example.md](docs/reverse_mode_by_example.md) | one gradient in both modes over the same graph, then the Jacobian and the Hessian |
| [docs/ad_jacobian.md](docs/ad_jacobian.md) | the node protocol and Jacobian computation in full ([PDF](docs/ad_jacobian.pdf)) |
| [docs/taylor_dual_by_example.md](docs/taylor_dual_by_example.md) | `TaylorDual<S, N>` — jet arithmetic and the recurrences, at `N = 3` |
| [docs/hyperdual_nth_order_by_example.md](docs/hyperdual_nth_order_by_example.md) | the nested dual's `2ⁿ` component lattice, and why the top slot is the `n`th derivative |
| [docs/forward_higher_order_by_example.md](docs/forward_higher_order_by_example.md) | the same higher-order derivative down both routes, compared |

[NOTES.md](NOTES.md) has the design decisions that are not obvious from the
headers. [REFERENCES.md](REFERENCES.md) is the literature: Part I is the
mathematics — dual numbers, Taylor arithmetic, the complexity results behind the
mode choices — and Part II is expression-tree optimisation.

---

## License

See [LICENSE.txt](LICENSE.txt). Suggestions and pull requests are welcome.

[![CMake](https://github.com/reach2sayan/ExpressionSolver/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/reach2sayan/ExpressionSolver/actions/workflows/cmake-multi-platform.yml) 

# Expression Differentiator

A C++23 template library for symbolic mathematical
expressions, derivatives, and equation systems
with compile-time evaluation capabilities.

## Overview

ExpressionSolver is a modern C++ library that
enables symbolic representation, manipulation, and
evaluation of mathematical expressions. It
supports automatic differentiation, equation
systems, and provides a compile-time capable
expression evaluation engine.

## Features

- **Expression Representation**: Create and
  manipulate complex mathematical expressions
- **Automatic Differentiation**: Compute
  derivatives symbolically
- **Equation Systems**: Work with systems of
  equations
- **Compile-time Evaluation**: Evaluate
  expressions at compile time when possible
- **Type-safe Operations**: All operations are
  type-safe and work with various numeric types. 

  For user defined types, they would have to 
  overload the operators `+`, `-`, `*`, `/` or 
  specialize `std::plus<>{}`, `std::minus<>{}}`,
  `std::multiplies<>{}`, `std::divides<>{}` for the types

## Requirements

- C++23 compatible compiler

## Core Components

### Expressions

The library represents expressions as template
classes that model the expression tree:

- : Represents binary operations
  `Expression<Op, LHS, RHS>`
- : Represents unary operations
  `MonoExpression<Op, Exp>`
- : Represents constant values `Constant<T>`
- `Variable<T, char>`: Represents variables with
  symbolic identifiers

### Operations

Mathematical operations are implemented as
operator types:

- : Addition `SumOp<T>`
- : Multiplication `MultiplyOp<T>`
- : Division `DivideOp<T>`
- : Subtraction `Op<T>`
- : Negation `NegateOp<T>`
- : Sine function `SineOp<T>`
- : Cosine function `CosineOp<T>`
- : Exponential function `ExpOp<T>`

A constant value can be created using the `PC(value)` macro.
There are also handy udl such as `7_ci` for constant integer
and `1.618_cd` for constant double.

There are also operator overloads for operations `+`, `-`, `*`, `/`,
`sin()`,`cos()`, `exp()` which take `Expression` objects or `Variable`
objects or `Constant` objects as arguments. This makes usage rather convenient
as can be seen in the [usage examples](#Usage Examples) section.

### Equations and Systems

- `Equation<TExpression>`: Wraps an expression
  with its derivatives
- : Manages systems of equations with Jacobian
  computation `SystemOfEquations<TEquations...>`

Note.  A jacobian would be available iff the system of equation is square

### Process Variable

- `ProcessVar<T>`: Represents a variable that can
  be used in expressions and updated. This is used 
  to provide reference semantics to variables in your 
  application, allowing you to update the variable values 
in the expression implicitly. 

 - User can create a `Variable<T>` or a `Constant<T>` from a `ProcessVar<T>` using the
function `as_variable<char>()` or `as_constant<T>()` respectively. Any
updates to the `ProcessVar<T>` will be reflected in the `Variable<T>` or `Constant<T>`
and vice versa.

## Usage Examples

### Creating Expressions

``` cpp
// Define process variables
auto x = PV(2, 'x');  // Variable x with initial value 2
auto y = PV(3, 'y');  // Variable y with initial value 3

// Create expressions
auto expr = x + y + 3_ci * x * y;  // x + y + 3*x*y
```

### Working with Equations

``` cpp
// Create an equation from an expression
auto eq = Equation(expr);

// Access the equation and its derivatives
auto value = eq.eval();            // Evaluate the equation
auto derivs = eq.eval_derivatives(); // Get all derivatives
```

### Systems of Equations

``` cpp
// Create expressions
auto expr1 = x + y + 3_ci * x * y * y;
auto expr2 = x + y;

// Create a system of equations
auto system = make_system_of_equations(expr1, expr2);

// Evaluate the system
auto result = system.eval();

// Compute the Jacobian matrix
auto jacobian = system.jacobian();

// Update variable values
std::array<int, 2> newValues = {42, 1729};
system.update(newValues);
```

### Process Variables

``` cpp
ProcessVar<double> pv(3.14);
auto x = pv.as_variable<'x'>();

x = 6.203; // updates pv
pv.set_value(8.314); // updates x
```

## Contributing

This is a personal project, but contributions are welcome! I want 
to learn, so please comment. I don't promise to implement all 
suggestions, but I will surely think them through and through.

<sub>I know the tests are far from complete. I will be working on them I promise.</sub>

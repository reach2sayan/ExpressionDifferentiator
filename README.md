[![CMake](https://github.com/reach2sayan/ExpressionSolver/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/reach2sayan/ExpressionSolver/actions/workflows/cmake-multi-platform.yml) [![C++](https://img.shields.io/badge/C++-%2300599C.svg?logo=c%2B%2B&logoColor=white)](#)

# Expression Differentiator

Expression Differentiator is a lightweight C++ library for symbolic differentiation and evaluation of mathematical expressions. It allows you to define expressions using variables and constants, evaluate them numerically, and compute derivatives automatically.

The library supports scalar and vector-valued functions, making it useful for mathematical modeling, optimization, and educational purposes.

## Features
* Symbolic expression construction using C++ operators
* Automatic differentiation (first-order derivatives)
* Numerical evaluation of expressions
* Support for:
	* Polynomial expressions
	* Multivariable functions
	* Trigonometric functions (sin, cos, etc.)
	* Vector-valued functions (ℝⁿ → ℝᵐ)
* Clean and composable expression system

## Project Structure
```
SymDiff/
├── include/        # Core headers (expressions, operations, values, etc.)
├── src/            # Example / main program
├── tests.cpp       # Unit tests
├── CMakeLists.txt  # Build configuration
└── build/          # Build artifacts (generated)
```

##  Usage Examples
### Scalar Function with Multiple Variables
```cpp
// f(x, y) = x*y + 3*x*y^2
auto x = PV(4, 'x');
auto y = PV(2, 'y');
auto expr = x * y + PC(3) * x * y * y;

std::cout << expr << "\n"; // symbolic form
std::cout << expr.eval() << "\n"; // numerical evaluation
```
### Compute Partial Derivatives
```cpp
auto eq = Equation(expr);
auto [dx, dy] = eq.eval_derivatives();

std::cout << "df/dx = " << dx << "\n";
std::cout << "df/dy = " << dy << "\n";
```
### Trigonometric Differentiation
```cpp
auto x = PV(1.0, 'x');
auto expr = sin(x) * cos(x);

std::cout << expr.eval() << "\n"; // value
std::cout << expr.derivative().eval() << "\n"; // derivative
```
### Vector-Valued Functions
```cpp

// f(x, y) = (x + y, x * y)
auto x = PV(3.0, 'x');
auto y = PV(4.0, 'y');
auto f = VectorEquation(x + y, x * y);
auto result = f.eval();
std::cout << f;
```
##  Core Concepts
### ```PV``` — Parameter Variable
Represents a variable with a value and identifier.
```cpp
auto x = PV(2.0, 'x');
```
### ```PC``` — Parameter Constant
Represents a constant value.
```cpp
auto c = PC(3.0);
```
### Expressions
Built using operator overloading:
```cpp
auto expr = x * y + 2 * x;
```

### Differentiation
```cpp
auto d = expr.derivative();
```

### Evaluation
```cpp
double value = expr.eval();
```

## Contributing

This is a personal project, but contributions are welcome! I want 
to learn, so please comment. I don't promise to implement all 
suggestions, but I will surely think them through and through.

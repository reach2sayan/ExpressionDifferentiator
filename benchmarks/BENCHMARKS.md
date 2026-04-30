# Benchmarks

## Running the suite

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target benchmarks
./build/benchmarks
```

Export machine-readable JSON:

```sh
cmake --build build --target benchmark_json
# writes build/benchmark-results/benchmarks.json
```

Filter to a subset:

```sh
./build/benchmarks --benchmark_filter='.*F4.*'
```

PowerShell:

```powershell
.\build-win\Release\benchmarks.exe "--benchmark_filter=F1|F2|F3|F4" --benchmark_min_time=0.05s
```

A manual GitHub Actions workflow at
[.github/workflows/benchmark-manual.yml](../.github/workflows/benchmark-manual.yml)
builds on Ubuntu and Windows, exports JSON, and uploads results as artifacts.

---

## Scalar gradient suite

Compares three ways to compute a full gradient for the same scalar function:

- **Symbolic** — `Equation(...).eval_derivatives()`
- **Forward** — `Dual<T>` with one seeded pass per input variable
- **Reverse** — `reverse_mode_gradient(expr)`

Functions benchmarked:

| Name | Expression |
|------|-----------|
| `F1` | `exp(x)*sin(x) + x³ + 2x` |
| `F2` | `xy + sin(x) + y² + exp(x+y)` |
| `F3` | `exp(xy) + x·sin(z) + yz + x²z` |
| `F4` | `(x+y)(z-w) + exp(xz) + sin(yw) + xyzw` |

### Windows / MSVC snapshot

| Function | Symbolic | Forward | Reverse |
|----------|--------:|--------:|--------:|
| `F1`     | 12.6 ns | 10.3 ns |  9.63 ns |
| `F2`     | 12.6 ns | 14.6 ns |  6.00 ns |
| `F3`     | 22.0 ns | 24.0 ns |  7.50 ns |
| `F4`     | 23.0 ns | 39.2 ns |  5.44 ns |

### Linux / GCC snapshot

| Function | Symbolic | Forward | Reverse |
|----------|--------:|--------:|--------:|
| `F1`     | 0.458 ns | 0.346 ns | 0.224 ns |
| `F2`     | 0.223 ns | 0.224 ns | 0.225 ns |
| `F3`     | 0.227 ns | 0.227 ns | 0.224 ns |
| `F4`     | 0.447 ns | 0.451 ns | 0.450 ns |

On Linux, all three modes are fully inlined to near-zero measured time — the benchmark loop overhead dominates. The Windows numbers reflect a less aggressive inliner and show the real relative cost.

---

## Vector Jacobian suite

Compares three Jacobian paths for a 2-output, 4-input function:

- **Symbolic** — `VectorEquation::eval_jacobian()`
- **Forward** — `VectorEquation::eval_jacobian_forward(...)`
- **Reverse** — `VectorEquation::eval_jacobian_reverse()`

```sh
./build/benchmarks --benchmark_filter='.*Vector.*'
```

### Windows snapshot

| Method | Time |
|--------|-----:|
| Symbolic | 27.9 ns |
| Forward  | 140 ns  |
| Reverse  | 6.25 ns |

### Linux snapshot

| Method | Time |
|--------|-----:|
| Symbolic | 0.894 ns |
| Forward  | 0.898 ns |
| Reverse  | ~0.9 ns  |

Same inlining caveat as above.

---

## Parallel Jacobian experiment (reverted)

`eval_jacobian_reverse` was briefly changed to launch one `std::async` task per output row, since the per-row `backward()` passes write to independent `J[i]` slices with no shared state. The sweep below measured the breakeven (same expression replicated across rows):

```sh
./build/benchmarks --benchmark_filter='.*(Parallel).*'
```

| output_dim | Symbolic (serial) | Async parallel |
|:----------:|------------------:|---------------:|
| 2          | 0.900 ns          | 44,372 ns      |
| 4          | 1.79 ns           | 81,571 ns      |
| 6          | 325 ns            | 153,560 ns     |
| 5 (trig/exp mix) | 225 ns      | 125,471 ns     |

`std::async` thread spawn costs ~40–50 µs. Each `backward()` pass costs nanoseconds. The parallel version is 50,000× slower at 2 rows and still ~470× slower at 6 rows. The change was reverted. `eval_jacobian_reverse` remains `constexpr` and serial.

The benchmarks for the parallel sweep (`BM_Reverse_Parallel_*`, `BM_Symbolic_Parallel_*`) are kept in the suite as a regression guard and to document this boundary.

---

## Object footprint and batched throughput

```sh
./build/benchmarks --benchmark_filter='.*(Footprint|Batched).*'
```

### Object sizes (Linux)

| Object | Size |
|--------|-----:|
| Scalar expression (F4) | 136 B |
| `Equation` (F4, with cached partials) | 1,544 B |
| Forward-mode expression (`Dual<double>`) | 232 B |
| `Dual<double>` value | 16 B |

### Batched throughput — F4 (Linux)

| Mode | 256 items | 1024 items | 4096 items |
|------|----------:|-----------:|-----------:|
| Symbolic (`Equation`) | 14.4 M/s | 12.3 M/s | 11.8 M/s |
| Reverse  (expression) | 130.6 M/s | 125.3 M/s | 105.3 M/s |
| Forward  (dual expr)  | 13.7 M/s  | 12.9 M/s  | 12.5 M/s  |

Reverse mode has the best per-item throughput — smaller objects (136 B vs 1,544 B for `Equation`) and a single tree traversal. Forward mode pays both a larger object footprint and a multi-pass cost (one pass per input variable).

### Hardware counters (Linux only)

```sh
perf stat -e cache-references,cache-misses,cycles,instructions \
  ./build/benchmarks --benchmark_filter='.*(Footprint|Batched).*' --benchmark_min_time=0.1s
```

For L1/LLC breakdown:

```sh
perf stat -e LLC-loads,LLC-load-misses,L1-dcache-loads,L1-dcache-load-misses \
  ./build/benchmarks --benchmark_filter='.*Batched.*' --benchmark_min_time=0.1s
```

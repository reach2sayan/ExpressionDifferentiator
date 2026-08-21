#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <string>

// The JIT's public surface.  No LLVM header appears here, and none may: this
// lives under include/, and Compiler holds its LLVM state behind a pointer for
// exactly that reason.  rt::Graph is only named, so nwgraph stays out too.
namespace ddx::rt {
class Graph;
}

namespace ddx::jit {

// Which vector math library the loop vectoriser may call.  Auto probes for
// glibc's libmvec and falls back to None, which still compiles -- the loop
// simply stays scalar wherever a transcendental appears.
enum class VecLib : std::uint8_t { None, Auto, Libmvec };

struct Options {
  unsigned opt_level = 2;
  VecLib veclib = VecLib::Auto;
  // Matches the -ffp-contract=fast the project builds with.  Nothing here
  // enables reassociation: that would change derivative values.
  bool contract = true;
};

// One compiled graph.  Cheap to copy -- it is a function pointer and a shape --
// but it does not own the code, so it must not outlive the Compiler.
class Kernel {
public:
  using function_type = void (*)(const double *const *, double *,
                                 double *const *, std::size_t);

  Kernel() = default;
  Kernel(function_type fn, std::size_t arity, std::size_t outputs) noexcept
      : fn_(fn), arity_(arity), outputs_(outputs) {}

  // xs[j] is the column for symbol j, g[j] the column for the partial in that
  // symbol, all of length n.  A kernel computes exactly the outputs its graph
  // was frozen with, so g is read only when outputs() > 1 -- freeze with the
  // value alone and it is never touched.
  void operator()(std::span<const double *const> xs, double *f,
                  double *const *g, std::size_t n) const {
    fn_(xs.data(), f, g, n);
  }

  [[nodiscard]] explicit operator bool() const noexcept {
    return fn_ != nullptr;
  }
  [[nodiscard]] std::size_t arity() const noexcept { return arity_; }
  [[nodiscard]] std::size_t outputs() const noexcept { return outputs_; }

private:
  function_type fn_ = nullptr;
  std::size_t arity_ = 0;
  std::size_t outputs_ = 0;
};

class Compiler {
public:
  Compiler();
  ~Compiler();
  Compiler(Compiler &&) noexcept;
  Compiler &operator=(Compiler &&) noexcept;
  Compiler(const Compiler &) = delete;
  Compiler &operator=(const Compiler &) = delete;

  [[nodiscard]] Kernel compile(const rt::Graph &g, const Options &opt = {});

  // The optimised IR, for tests that assert on what the vectoriser did.
  [[nodiscard]] std::string dump_ir(const rt::Graph &g,
                                    const Options &opt = {}) const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace ddx::jit

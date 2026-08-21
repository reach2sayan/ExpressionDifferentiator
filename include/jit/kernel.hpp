#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <string>

// The JIT's public surface.
namespace ddx::rt {
class Graph;
}

namespace ddx::jit {

// Which vector math library the loop vectoriser may call.
enum class VecLib : std::uint8_t { None, Auto, Libmvec };

#ifndef DDX_JIT_DEFAULT_OPT
#define DDX_JIT_DEFAULT_OPT 2
#endif
#ifndef DDX_JIT_DEFAULT_CONTRACT
#define DDX_JIT_DEFAULT_CONTRACT 1
#endif

inline constexpr unsigned default_opt_level = DDX_JIT_DEFAULT_OPT;
inline constexpr bool default_contract = DDX_JIT_DEFAULT_CONTRACT != 0;

struct Options {
  // Never 0: that disables the loop vectoriser
  unsigned opt_level = default_opt_level;
  VecLib veclib = VecLib::Auto;
  bool contract = default_contract;  // Follows DDX_FP_FLAGS
};

// One compiled graph.  Cheap to copy
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

private:
  friend class Ir;
  [[nodiscard]] std::string render_ir(const rt::Graph &g,
                                      const Options &opt) const;

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// The optimised IR a graph would compile to
class Ir {
public:
  Ir(const Compiler &c, const rt::Graph &g, Options opt = {}) noexcept
      : compiler_(&c), graph_(&g), options_(opt) {}
  [[nodiscard]] std::string str() const {
    return compiler_->render_ir(*graph_, options_);
  }

private:
  const Compiler *compiler_;
  const rt::Graph *graph_;
  Options options_;
};

} // namespace ddx::jit

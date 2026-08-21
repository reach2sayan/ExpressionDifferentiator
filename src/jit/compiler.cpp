#include "codegen.hpp"

#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>

#include <atomic>
#include <stdexcept>
#include <string>
#include <utility>

namespace ddx::jit {
namespace {

// llvm::InitializeNativeTarget is global state, and a process may hold more
// than one Compiler.
void init_native_target_once() {
  // A function-local static is initialised once and thread-safely, which is the
  // whole of what call_once was here for.
  [[maybe_unused]] static const bool ready = [] {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    return true;
  }();
}

[[noreturn]] void fail(const llvm::Twine &what, llvm::Error e) {
  throw std::runtime_error((what + ": " + llvm::toString(std::move(e))).str());
}

// llvm::Expected or the error as an exception.  Every step of bringing up the
// JIT returns one, and unwrapping each with its own if/else nests the whole
// constructor five deep for no gain.
template <typename T>
[[nodiscard]] T must(llvm::Expected<T> e, const llvm::Twine &what) {
  if (!e) {
    fail(what, e.takeError());
  }
  return std::move(*e);
}

llvm::TargetLibraryInfoImpl target_library_info(const llvm::Triple &triple,
                                                const Options &opt) {
  llvm::TargetLibraryInfoImpl tlii(triple);
  const bool want = opt.veclib == VecLib::Libmvec ||
                    (opt.veclib == VecLib::Auto && triple.isOSLinux() &&
                     triple.getArch() == llvm::Triple::x86_64);
  if (want) {
    // Without this the loop vectoriser has no vector form for sin/exp/... and
    // bails out of any loop containing one -- which is most of them here.
    tlii.addVectorizableFunctionsFromVecLib(
        llvm::TargetLibraryInfoImpl::LIBMVEC_X86, triple);
  }
  return tlii;
}

void optimize(llvm::Module &m, llvm::TargetMachine &tm, const Options &opt) {
  const llvm::Triple triple(m.getTargetTriple());
  llvm::TargetLibraryInfoImpl tlii = target_library_info(triple, opt);

  llvm::LoopAnalysisManager lam;
  llvm::FunctionAnalysisManager fam;
  llvm::CGSCCAnalysisManager cgam;
  llvm::ModuleAnalysisManager mam;

  llvm::PassBuilder pb(&tm);
  // Before registerFunctionAnalyses, which would otherwise install the default
  // llvm::TargetLibraryAnalysis and with it an empty vector-function table.
  fam.registerPass([&] { return llvm::TargetLibraryAnalysis(tlii); });
  pb.registerModuleAnalyses(mam);
  pb.registerCGSCCAnalyses(cgam);
  pb.registerFunctionAnalyses(fam);
  pb.registerLoopAnalyses(lam);
  pb.crossRegisterProxies(lam, fam, cgam, mam);

  llvm::OptimizationLevel level = llvm::OptimizationLevel::O2;
  switch (opt.opt_level) {
  case 0:
    level = llvm::OptimizationLevel::O0;
    break;
  case 1:
    level = llvm::OptimizationLevel::O1;
    break;
  case 3:
    level = llvm::OptimizationLevel::O3;
    break;
  default:
    break;
  }
  llvm::ModulePassManager mpm = level == llvm::OptimizationLevel::O0
                                    ? pb.buildO0DefaultPipeline(level)
                                    : pb.buildPerModuleDefaultPipeline(level);
  mpm.run(m, mam);
}

} // namespace

struct Compiler::Impl {
  std::unique_ptr<llvm::orc::LLJIT> jit;
  std::unique_ptr<llvm::TargetMachine> tm;
  // Two threads compiling through one Compiler would otherwise race here and,
  // worse, hand the JIT two modules under one name.  LLJIT itself is
  // internally synchronised, so this counter is the only shared mutable state.
  std::atomic<unsigned> counter{0};

  Impl() {
    init_native_target_once();

    auto jtmb = must(llvm::orc::JITTargetMachineBuilder::detectHost(),
                     "detecting the host");
    // Parity with the project's -march=native.
    jtmb.setCPU(llvm::sys::getHostCPUName().str());
    for (const auto &[feature, enabled] : llvm::sys::getHostCPUFeatures()) {
      jtmb.getFeatures().AddFeature(feature, enabled);
    }

    tm = must(jtmb.createTargetMachine(), "creating a target machine");
    jit = must(llvm::orc::LLJITBuilder()
                   .setJITTargetMachineBuilder(std::move(jtmb))
                   .create(),
               "creating the JIT");

    llvm::orc::JITDylib &jd = jit->getMainJITDylib();
    const char prefix = jit->getDataLayout().getGlobalPrefix();

    jd.addGenerator(
        must(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
                 prefix),
             "opening the process symbols"));

    // GetForCurrentProcess only sees what is already loaded, and libmvec is not
    // linked into a program that never called it; without this the vector
    // symbols the vectoriser emits have nowhere to resolve.
    if (auto vec = llvm::orc::DynamicLibrarySearchGenerator::Load(
            "libmvec.so.1", prefix)) {
      jd.addGenerator(std::move(*vec));
    } else {
      llvm::consumeError(vec.takeError());
    }
  }

  std::unique_ptr<llvm::Module> build(llvm::LLVMContext &ctx,
                                      const rt::Graph<double> &g,
                                      const Options &opt,
                                      const std::string &name) const {
    auto m = detail::emit_module(ctx, g, opt, name);
    if (!m) {
      throw std::runtime_error(
          "ddx::jit: the emitted module failed verification");
    }
    m->setDataLayout(jit->getDataLayout());
    m->setTargetTriple(tm->getTargetTriple().str());
    optimize(*m, *tm, opt);
    return m;
  }
};

Compiler::Compiler() : impl_(std::make_shared<Impl>()) {}
Compiler::~Compiler() = default;
Compiler::Compiler(Compiler &&) noexcept = default;
Compiler &Compiler::operator=(Compiler &&) noexcept = default;

Kernel Compiler::compile(const rt::Graph<double> &g, const Options &opt) {
  const std::string name = "ddx_kernel_" + std::to_string(impl_->counter++);
  auto ctx = std::make_unique<llvm::LLVMContext>();
  auto m = impl_->build(*ctx, g, opt, name);

  if (auto e = impl_->jit->addIRModule(
          llvm::orc::ThreadSafeModule(std::move(m), std::move(ctx)))) {
    fail("adding the module", std::move(e));
  }
  const auto sym = must(impl_->jit->lookup(name), "looking up " + name);

  const auto &layout = g.layout();
  // The Impl is what owns the code, so the Kernel holds a share of it: a
  // Compiler that goes out of scope no longer takes live kernels with it.
  return Kernel{sym.toPtr<Kernel::function_type>(),
                g.symbols().size(),
                layout.values,
                layout.jacobian,
                layout.hessian,
                impl_};
}

std::string Compiler::render_ir(const rt::Graph<double> &g,
                                const Options &opt) const {
  auto ctx = std::make_unique<llvm::LLVMContext>();
  auto m = impl_->build(*ctx, g, opt, "ddx_kernel_dump");
  std::string out;
  llvm::raw_string_ostream os(out);
  m->print(os, nullptr);
  return out;
}

} // namespace ddx::jit

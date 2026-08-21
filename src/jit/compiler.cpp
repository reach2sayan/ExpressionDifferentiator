#include "codegen.hpp"

#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>

#include <mutex>
#include <stdexcept>
#include <string>

using namespace llvm;

namespace ddx::jit {
namespace {

// InitializeNativeTarget is global state, and a process may hold more than one
// Compiler.
void init_native_target_once() {
  static std::once_flag flag;
  std::call_once(flag, [] {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
  });
}

[[noreturn]] void fail(const Twine &what, Error e) {
  throw std::runtime_error((what + ": " + toString(std::move(e))).str());
}

TargetLibraryInfoImpl target_library_info(const Triple &triple,
                                          const Options &opt) {
  TargetLibraryInfoImpl tlii(triple);
  const bool want = opt.veclib == VecLib::Libmvec ||
                    (opt.veclib == VecLib::Auto && triple.isOSLinux() &&
                     triple.getArch() == Triple::x86_64);
  if (want) {
    // Without this the loop vectoriser has no vector form for sin/exp/... and
    // bails out of any loop containing one -- which is most of them here.
    tlii.addVectorizableFunctionsFromVecLib(TargetLibraryInfoImpl::LIBMVEC_X86,
                                            triple);
  }
  return tlii;
}

void optimize(Module &m, TargetMachine &tm, const Options &opt) {
  const Triple triple(m.getTargetTriple());
  TargetLibraryInfoImpl tlii = target_library_info(triple, opt);

  LoopAnalysisManager lam;
  FunctionAnalysisManager fam;
  CGSCCAnalysisManager cgam;
  ModuleAnalysisManager mam;

  PassBuilder pb(&tm);
  // Before registerFunctionAnalyses, which would otherwise install the default
  // TargetLibraryAnalysis and with it an empty vector-function table.
  fam.registerPass([&] { return TargetLibraryAnalysis(tlii); });
  pb.registerModuleAnalyses(mam);
  pb.registerCGSCCAnalyses(cgam);
  pb.registerFunctionAnalyses(fam);
  pb.registerLoopAnalyses(lam);
  pb.crossRegisterProxies(lam, fam, cgam, mam);

  OptimizationLevel level = OptimizationLevel::O2;
  switch (opt.opt_level) {
  case 0:
    level = OptimizationLevel::O0;
    break;
  case 1:
    level = OptimizationLevel::O1;
    break;
  case 3:
    level = OptimizationLevel::O3;
    break;
  default:
    break;
  }
  ModulePassManager mpm = level == OptimizationLevel::O0
                              ? pb.buildO0DefaultPipeline(level)
                              : pb.buildPerModuleDefaultPipeline(level);
  mpm.run(m, mam);
}

} // namespace

struct Compiler::Impl {
  std::unique_ptr<orc::LLJIT> jit;
  std::unique_ptr<TargetMachine> tm;
  unsigned counter = 0;

  Impl() {
    init_native_target_once();

    auto jtmb = orc::JITTargetMachineBuilder::detectHost();
    if (!jtmb) {
      fail("detecting the host", jtmb.takeError());
    }
    // Parity with the project's -march=native.
    jtmb->setCPU(sys::getHostCPUName().str());
    for (const auto &[feature, enabled] : sys::getHostCPUFeatures()) {
      jtmb->getFeatures().AddFeature(feature, enabled);
    }

    auto machine = jtmb->createTargetMachine();
    if (!machine) {
      fail("creating a target machine", machine.takeError());
    }
    tm = std::move(*machine);

    auto built = orc::LLJITBuilder()
                     .setJITTargetMachineBuilder(std::move(*jtmb))
                     .create();
    if (!built) {
      fail("creating the JIT", built.takeError());
    }
    jit = std::move(*built);

    orc::JITDylib &jd = jit->getMainJITDylib();
    const char prefix = jit->getDataLayout().getGlobalPrefix();
    auto process =
        orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(prefix);
    if (!process) {
      fail("opening the process symbols", process.takeError());
    }
    jd.addGenerator(std::move(*process));
    // GetForCurrentProcess only sees what is already loaded, and libmvec is not
    // linked into a program that never called it; without this the vector
    // symbols the vectoriser emits have nowhere to resolve.
    if (auto vec =
            orc::DynamicLibrarySearchGenerator::Load("libmvec.so.1", prefix)) {
      jd.addGenerator(std::move(*vec));
    } else {
      consumeError(vec.takeError());
    }
  }

  std::unique_ptr<Module> build(LLVMContext &ctx, const rt::Graph &g,
                                const Options &opt, const std::string &name) {
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

Compiler::Compiler() : impl_(std::make_unique<Impl>()) {}
Compiler::~Compiler() = default;
Compiler::Compiler(Compiler &&) noexcept = default;
Compiler &Compiler::operator=(Compiler &&) noexcept = default;

Kernel Compiler::compile(const rt::Graph &g, const Options &opt) {
  const std::string name = "ddx_kernel_" + std::to_string(impl_->counter++);
  auto ctx = std::make_unique<LLVMContext>();
  auto m = impl_->build(*ctx, g, opt, name);

  if (auto e = impl_->jit->addIRModule(
          orc::ThreadSafeModule(std::move(m), std::move(ctx)))) {
    fail("adding the module", std::move(e));
  }
  auto sym = impl_->jit->lookup(name);
  if (!sym) {
    fail("looking up " + name, sym.takeError());
  }

  const std::size_t outputs = g.outputs().size();
  return Kernel{sym->toPtr<Kernel::function_type>(), g.symbols().size(),
                outputs};
}

std::string Compiler::dump_ir(const rt::Graph &g, const Options &opt) const {
  auto ctx = std::make_unique<LLVMContext>();
  auto m = impl_->build(*ctx, g, opt, "ddx_kernel_dump");
  std::string out;
  raw_string_ostream os(out);
  m->print(os, nullptr);
  return out;
}

} // namespace ddx::jit

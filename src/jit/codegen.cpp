#include "codegen.hpp"

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Verifier.h>

#include <array>
#include <ranges>
#include <span>
#include <string_view>
#include <vector>

namespace ddx::jit::detail {
namespace {

// Which LLVM intrinsic, if any, covers an op.  LLVM 20 has f64 intrinsics for
// most of the set; cbrt, asinh, acosh, atanh, erf and hypot have none and go
// out as libm calls instead -- under label_of(op), which is already the libm
// spelling for every one of them.
llvm::Intrinsic::ID intrinsic_for(rt::OpCode op) {
  switch (op) {
  case rt::OpCode::Abs:
    return llvm::Intrinsic::fabs;
  case rt::OpCode::Max:
    return llvm::Intrinsic::maxnum;
  case rt::OpCode::Min:
    return llvm::Intrinsic::minnum;
  case rt::OpCode::Pow:
    return llvm::Intrinsic::pow;
  case rt::OpCode::Atan2:
    return llvm::Intrinsic::atan2;
  case rt::OpCode::SineOp:
    return llvm::Intrinsic::sin;
  case rt::OpCode::CosineOp:
    return llvm::Intrinsic::cos;
  case rt::OpCode::TanOp:
    return llvm::Intrinsic::tan;
  case rt::OpCode::AsinOp:
    return llvm::Intrinsic::asin;
  case rt::OpCode::AcosOp:
    return llvm::Intrinsic::acos;
  case rt::OpCode::AtanOp:
    return llvm::Intrinsic::atan;
  case rt::OpCode::SinhOp:
    return llvm::Intrinsic::sinh;
  case rt::OpCode::CoshOp:
    return llvm::Intrinsic::cosh;
  case rt::OpCode::TanhOp:
    return llvm::Intrinsic::tanh;
  case rt::OpCode::ExpOp:
    return llvm::Intrinsic::exp;
  case rt::OpCode::LogOp:
    return llvm::Intrinsic::log;
  case rt::OpCode::Log10Op:
    return llvm::Intrinsic::log10;
  case rt::OpCode::SqrtOp:
    return llvm::Intrinsic::sqrt;
  default:
    return llvm::Intrinsic::not_intrinsic;
  }
}

// A libm declaration the optimiser is allowed to move.  memory(none) is the IR
// spelling of -fno-math-errno: without it every call is assumed to write errno,
// which blocks hoisting and vectorisation outright.
llvm::Function *libm_decl(llvm::Module &m, std::string_view name,
                          unsigned args) {
  llvm::Type *f64 = llvm::Type::getDoubleTy(m.getContext());
  llvm::SmallVector<llvm::Type *, 2> params(args, f64);
  llvm::FunctionCallee c =
      m.getOrInsertFunction(name, llvm::FunctionType::get(f64, params, false));
  auto *fn = llvm::cast<llvm::Function>(c.getCallee());
  fn->setMemoryEffects(llvm::MemoryEffects::none());
  fn->setDoesNotThrow();
  fn->setWillReturn();
  fn->setDoesNotFreeMemory();
  return fn;
}

class Emitter {
public:
  Emitter(llvm::Module &m, llvm::IRBuilder<> &b) : m_(m), b_(b) {}

  llvm::Value *unary(rt::OpCode op, llvm::Value *u) const {
    return (op == rt::OpCode::Neg) ? b_.CreateFNeg(u)
           : (intrinsic_for(op) != llvm::Intrinsic::not_intrinsic)
               ? b_.CreateCall(llvm::Intrinsic::getOrInsertDeclaration(
                                   &m_, intrinsic_for(op), {b_.getDoubleTy()}),
                               {u})
               : b_.CreateCall(libm_decl(m_, rt::label_of(op), 1), {u});
  }

  llvm::Value *binary(rt::OpCode op, llvm::Value *l, llvm::Value *r) const {
    switch (op) {
    case rt::OpCode::Add:
      return b_.CreateFAdd(l, r);
    case rt::OpCode::Mul:
      return b_.CreateFMul(l, r);
    case rt::OpCode::Div:
      return b_.CreateFDiv(l, r);
    default:
      break;
    }
    return intrinsic_for(op) != llvm::Intrinsic::not_intrinsic
               ? b_.CreateCall(llvm::Intrinsic::getOrInsertDeclaration(
                                   &m_, intrinsic_for(op), {b_.getDoubleTy()}),
                               {l, r})
               : b_.CreateCall(libm_decl(m_, rt::label_of(op), 2), {l, r});
  }

private:
  llvm::Module &m_;
  llvm::IRBuilder<> &b_;
};

// The kernel's signature, and the promises about its pointers that decide
// whether the loop can be vectorised at all.
llvm::Function *declare_kernel(llvm::Module &m, llvm::StringRef name) {
  llvm::LLVMContext &ctx = m.getContext();
  llvm::Type *const i64 = llvm::Type::getInt64Ty(ctx);
  llvm::PointerType *const ptr = llvm::PointerType::getUnqual(ctx);
  auto *const fty = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx),
                                            {ptr, ptr, ptr, i64}, false);
  auto *const fn =
      llvm::Function::Create(fty, llvm::Function::ExternalLinkage, name, m);

  static constexpr std::array names{"xs", "f", "g", "n"};
  for (const auto [i, arg_name] : std::views::enumerate(names)) {
    fn->getArg(static_cast<unsigned>(i))->setName(arg_name);
  }
  // The columns never alias the outputs; saying so is what lets the vectoriser
  // skip the runtime overlap check it would otherwise need.
  for (const unsigned i : {0u, 1u, 2u}) {
    fn->addParamAttr(i, llvm::Attribute::NoAlias);
    fn->addParamAttr(i, llvm::Attribute::NoCapture);
  }
  fn->addParamAttr(0, llvm::Attribute::ReadOnly);
  return fn;
}

llvm::FastMathFlags flags_for(const Options &opt) {
  // Contraction only, matching the project's -ffp-contract=fast.  Nothing here
  // enables reassociation: an elementwise map has no reduction to reassociate,
  // and it would change derivative values.
  llvm::FastMathFlags fmf;
  if (opt.contract) {
    fmf.setAllowContract();
  }
  return fmf;
}

// The pointers every iteration would otherwise reload.  Loading them once in
// the entry block is what makes them loop-invariant.
struct Columns {
  std::vector<llvm::Value *> inputs;
  std::vector<llvm::Value *> partial_outputs;
};

Columns hoist_columns(llvm::IRBuilder<> &b, llvm::Function &fn,
                      const rt::Graph<double> &g) {
  llvm::PointerType *const ptr = llvm::PointerType::getUnqual(fn.getContext());
  llvm::Argument *const xs = fn.getArg(0);
  llvm::Argument *const out_g = fn.getArg(2);

  const auto load_slot = [&](llvm::Argument *base, std::size_t j,
                             const char *stem) {
    llvm::Value *const slot = b.CreateConstInBoundsGEP1_64(ptr, base, j);
    return b.CreateLoad(ptr, slot, stem + std::to_string(j));
  };

  Columns cols;
  cols.inputs.reserve(g.symbols().size());
  for (std::size_t j = 0; j < g.symbols().size(); ++j) {
    cols.inputs.push_back(load_slot(xs, j, "col"));
  }
  // outputs()[0] is the value and goes to `f`; the rest are partials.
  const std::size_t partials = g.outputs().size() - 1;
  cols.partial_outputs.reserve(partials);
  for (std::size_t j = 0; j < partials; ++j) {
    cols.partial_outputs.push_back(load_slot(out_g, j, "g"));
  }
  return cols;
}

// One value per node, in id order.  Ids are topological, so a single pass needs
// no recursion and no worklist: every operand is already a llvm::Value when
// read.
std::vector<llvm::Value *> emit_nodes(const Emitter &emit, llvm::IRBuilder<> &b,
                                      const rt::Graph<double> &g,
                                      const Columns &cols, llvm::Value *index) {
  llvm::Type *const f64 = b.getDoubleTy();
  std::vector<llvm::Value *> value(g.size(), nullptr);
  for (const rt::NodeId v : g.live_nodes()) {
    const auto &p = g[v];
    const auto operands = g.operands(v);
    switch (rt::arity_of(p.op)) {
    case 0:
      value[v] =
          p.op == rt::OpCode::Const
              ? llvm::cast<llvm::Value>(llvm::ConstantFP::get(f64, p.value))
              : b.CreateLoad(
                    f64, b.CreateInBoundsGEP(f64, cols.inputs[p.slot], index));
      break;
    case 1:
      value[v] = emit.unary(p.op, value[operands[0]]);
      break;
    default:
      value[v] = emit.binary(p.op, value[operands[0]], value[operands[1]]);
      break;
    }
  }
  return value;
}

void emit_stores(llvm::IRBuilder<> &b, llvm::Function &fn,
                 const rt::Graph<double> &g, const Columns &cols,
                 std::span<llvm::Value *const> value, llvm::Value *index) {
  llvm::Type *const f64 = b.getDoubleTy();
  const auto outputs = g.outputs();
  const auto store = [&](llvm::Value *column, rt::NodeId node) {
    b.CreateStore(value[node], b.CreateInBoundsGEP(f64, column, index));
  };
  store(fn.getArg(1), outputs[0]);
  for (const auto [j, column] : std::views::enumerate(cols.partial_outputs)) {
    store(column, outputs[static_cast<std::size_t>(j) + 1]);
  }
}

} // namespace

std::unique_ptr<llvm::Module> emit_module(llvm::LLVMContext &ctx,
                                          const rt::Graph<double> &g,
                                          const Options &opt,
                                          llvm::StringRef name) {
  auto m = std::make_unique<llvm::Module>("ddx.jit", ctx);
  llvm::Function *const fn = declare_kernel(*m, name);
  llvm::Type *const i64 = llvm::Type::getInt64Ty(ctx);
  llvm::Argument *const count = fn->getArg(3);

  auto *const entry = llvm::BasicBlock::Create(ctx, "entry", fn);
  auto *const loop = llvm::BasicBlock::Create(ctx, "loop", fn);
  auto *const exit = llvm::BasicBlock::Create(ctx, "exit", fn);
  const llvm::FastMathFlags fmf = flags_for(opt);

  llvm::IRBuilder<> b(entry);
  b.setFastMathFlags(fmf);
  const Columns cols = hoist_columns(b, *fn, g);
  b.CreateCondBr(b.CreateICmpEQ(count, llvm::ConstantInt::get(i64, 0)), exit,
                 loop);

  b.SetInsertPoint(loop);
  b.setFastMathFlags(fmf);
  llvm::PHINode *const index = b.CreatePHI(i64, 2, "i");
  index->addIncoming(llvm::ConstantInt::get(i64, 0), entry);

  const Emitter emit(*m, b);
  const std::vector<llvm::Value *> value = emit_nodes(emit, b, g, cols, index);
  emit_stores(b, *fn, g, cols, value, index);

  llvm::Value *const next =
      b.CreateAdd(index, llvm::ConstantInt::get(i64, 1), "i.next");
  index->addIncoming(next, loop);
  b.CreateCondBr(b.CreateICmpEQ(next, count), exit, loop);

  b.SetInsertPoint(exit);
  b.CreateRetVoid();

  return llvm::verifyFunction(*fn, &llvm::errs()) ? nullptr : std::move(m);
}

} // namespace ddx::jit::detail

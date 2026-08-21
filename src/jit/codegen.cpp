#include "codegen.hpp"

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Verifier.h>

#include <vector>

using namespace llvm;

namespace ddx::jit::detail {
namespace {

// Which LLVM intrinsic, if any, covers an op.  LLVM 20 has f64 intrinsics for
// most of the set; cbrt, asinh, acosh, atanh, erf and hypot have none and go
// out as libm calls instead.
Intrinsic::ID intrinsic_for(rt::OpCode op) {
  switch (op) {
  case rt::OpCode::Abs:
    return Intrinsic::fabs;
  case rt::OpCode::Max:
    return Intrinsic::maxnum;
  case rt::OpCode::Min:
    return Intrinsic::minnum;
  case rt::OpCode::Pow:
    return Intrinsic::pow;
  case rt::OpCode::Atan2:
    return Intrinsic::atan2;
  case rt::OpCode::SineOp:
    return Intrinsic::sin;
  case rt::OpCode::CosineOp:
    return Intrinsic::cos;
  case rt::OpCode::TanOp:
    return Intrinsic::tan;
  case rt::OpCode::AsinOp:
    return Intrinsic::asin;
  case rt::OpCode::AcosOp:
    return Intrinsic::acos;
  case rt::OpCode::AtanOp:
    return Intrinsic::atan;
  case rt::OpCode::SinhOp:
    return Intrinsic::sinh;
  case rt::OpCode::CoshOp:
    return Intrinsic::cosh;
  case rt::OpCode::TanhOp:
    return Intrinsic::tanh;
  case rt::OpCode::ExpOp:
    return Intrinsic::exp;
  case rt::OpCode::LogOp:
    return Intrinsic::log;
  case rt::OpCode::Log10Op:
    return Intrinsic::log10;
  case rt::OpCode::SqrtOp:
    return Intrinsic::sqrt;
  default:
    return Intrinsic::not_intrinsic;
  }
}

const char *libm_for(rt::OpCode op) {
  switch (op) {
  case rt::OpCode::CbrtOp:
    return "cbrt";
  case rt::OpCode::AsinhOp:
    return "asinh";
  case rt::OpCode::AcoshOp:
    return "acosh";
  case rt::OpCode::AtanhOp:
    return "atanh";
  case rt::OpCode::ErfOp:
    return "erf";
  case rt::OpCode::Hypot:
    return "hypot";
  default:
    return nullptr;
  }
}

// A libm declaration the optimiser is allowed to move.  memory(none) is the IR
// spelling of -fno-math-errno: without it every call is assumed to write errno,
// which blocks hoisting and vectorisation outright.
Function *libm_decl(Module &m, const char *name, unsigned args) {
  Type *f64 = Type::getDoubleTy(m.getContext());
  SmallVector<Type *, 2> params(args, f64);
  FunctionCallee c =
      m.getOrInsertFunction(name, FunctionType::get(f64, params, false));
  auto *fn = cast<Function>(c.getCallee());
  fn->setMemoryEffects(MemoryEffects::none());
  fn->setDoesNotThrow();
  fn->setWillReturn();
  fn->setDoesNotFreeMemory();
  return fn;
}

class Emitter {
public:
  Emitter(Module &m, IRBuilder<> &b, const rt::Graph &g)
      : m_(m), b_(b), g_(g) {}

  Value *unary(rt::OpCode op, Value *u) {
    if (op == rt::OpCode::Neg) {
      return b_.CreateFNeg(u);
    }
    if (const Intrinsic::ID id = intrinsic_for(op);
        id != Intrinsic::not_intrinsic) {
      return b_.CreateCall(
          Intrinsic::getOrInsertDeclaration(&m_, id, {b_.getDoubleTy()}), {u});
    }
    return b_.CreateCall(libm_decl(m_, libm_for(op), 1), {u});
  }

  Value *binary(rt::OpCode op, Value *l, Value *r) {
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
    if (const Intrinsic::ID id = intrinsic_for(op);
        id != Intrinsic::not_intrinsic) {
      return b_.CreateCall(
          Intrinsic::getOrInsertDeclaration(&m_, id, {b_.getDoubleTy()}),
          {l, r});
    }
    return b_.CreateCall(libm_decl(m_, libm_for(op), 2), {l, r});
  }

private:
  Module &m_;
  IRBuilder<> &b_;
  const rt::Graph &g_;
};

} // namespace

std::unique_ptr<Module> emit_module(LLVMContext &ctx, const rt::Graph &g,
                                    const Options &opt, StringRef name) {
  auto m = std::make_unique<Module>("ddx.jit", ctx);
  Type *f64 = Type::getDoubleTy(ctx);
  Type *i64 = Type::getInt64Ty(ctx);
  PointerType *ptr = PointerType::getUnqual(ctx);

  auto *fty =
      FunctionType::get(Type::getVoidTy(ctx), {ptr, ptr, ptr, i64}, false);
  auto *fn = Function::Create(fty, Function::ExternalLinkage, name, *m);

  Argument *xs = fn->getArg(0);
  Argument *out_f = fn->getArg(1);
  Argument *out_g = fn->getArg(2);
  Argument *count = fn->getArg(3);
  xs->setName("xs");
  out_f->setName("f");
  out_g->setName("g");
  count->setName("n");
  // The columns never alias the outputs; saying so is what lets the vectoriser
  // skip a runtime overlap check.
  for (unsigned i = 0; i < 3; ++i) {
    fn->addParamAttr(i, Attribute::NoAlias);
    fn->addParamAttr(i, Attribute::NoCapture);
  }
  fn->addParamAttr(0, Attribute::ReadOnly);

  auto *entry = BasicBlock::Create(ctx, "entry", fn);
  auto *loop = BasicBlock::Create(ctx, "loop", fn);
  auto *exit = BasicBlock::Create(ctx, "exit", fn);

  IRBuilder<> b(entry);
  FastMathFlags fmf;
  if (opt.contract) {
    fmf.setAllowContract();
  }
  b.setFastMathFlags(fmf);

  // Hoist the column and output pointers: they are loop-invariant, and leaving
  // them inside would make every iteration reload them.
  const std::size_t nvars = g.symbols().size();
  std::vector<Value *> columns(nvars);
  for (std::size_t j = 0; j < nvars; ++j) {
    Value *slot = b.CreateConstInBoundsGEP1_64(ptr, xs, j);
    columns[j] = b.CreateLoad(ptr, slot, "col" + std::to_string(j));
  }
  const auto outputs = g.outputs();
  const std::size_t npartials = outputs.size() > 1 ? outputs.size() - 1 : 0;
  std::vector<Value *> partial_out(npartials);
  const bool want_g = npartials > 0;
  if (want_g) {
    for (std::size_t j = 0; j < npartials; ++j) {
      Value *slot = b.CreateConstInBoundsGEP1_64(ptr, out_g, j);
      partial_out[j] = b.CreateLoad(ptr, slot, "g" + std::to_string(j));
    }
  }

  b.CreateCondBr(b.CreateICmpEQ(count, ConstantInt::get(i64, 0)), exit, loop);

  b.SetInsertPoint(loop);
  b.setFastMathFlags(fmf);
  PHINode *iv = b.CreatePHI(i64, 2, "i");
  iv->addIncoming(ConstantInt::get(i64, 0), entry);

  // Ids are topological, so one pass in id order needs no recursion and no
  // worklist: every operand is already a Value by the time it is read.
  Emitter emit(*m, b, g);
  std::vector<Value *> value(g.size(), nullptr);
  for (rt::NodeId v = 0; v < g.size(); ++v) {
    if (!g.live(v)) {
      continue;
    }
    const auto &p = g[v];
    switch (rt::arity_of(p.op)) {
    case 0:
      value[v] = p.op == rt::OpCode::Const
                     ? cast<Value>(ConstantFP::get(f64, p.value))
                     : b.CreateLoad(
                           f64, b.CreateInBoundsGEP(f64, columns[p.slot], iv));
      break;
    case 1: {
      const auto ops = g.operands(v);
      value[v] = emit.unary(p.op, value[ops[0]]);
      break;
    }
    default: {
      const auto ops = g.operands(v);
      value[v] = emit.binary(p.op, value[ops[0]], value[ops[1]]);
      break;
    }
    }
  }

  b.CreateStore(value[outputs[0]], b.CreateInBoundsGEP(f64, out_f, iv));
  for (std::size_t j = 0; j < npartials; ++j) {
    b.CreateStore(value[outputs[j + 1]],
                  b.CreateInBoundsGEP(f64, partial_out[j], iv));
  }

  Value *next = b.CreateAdd(iv, ConstantInt::get(i64, 1), "i.next");
  iv->addIncoming(next, loop);
  b.CreateCondBr(b.CreateICmpEQ(next, count), exit, loop);

  b.SetInsertPoint(exit);
  b.CreateRetVoid();

  if (verifyFunction(*fn, &errs())) {
    return nullptr;
  }
  return m;
}

} // namespace ddx::jit::detail

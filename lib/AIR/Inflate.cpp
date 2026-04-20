#include "AIR.h"
#include "mlir/Conversion/SCFToControlFlow/SCFToControlFlow.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlowOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/AsmState.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassManager.h"
#include "swift/AIR/AIROps.h"
#include "swift/AST/ASTContext.h"
#include "swift/AST/ASTWalker.h"

#include "swift/AST/Decl.h"
#include "swift/AST/FileSystem.h"
#include "swift/AST/Module.h"
#include "swift/Frontend/Frontend.h"

#include "AIRGenModule.h"

using namespace mlir;
using namespace mlir::air;

namespace {

/// Walks expression trees to assign display names to AIRSpliceExpr nodes
/// using the MLIR AsmState for consistent SSA naming.
class SpliceNameAssigner : public swift::ASTWalker {
  AsmState &State;

public:
  SpliceNameAssigner(AsmState &state) : State(state) {}

  PostWalkResult<Expr *> walkToExprPost(Expr *E) override {
    if (auto *splice = dyn_cast<AIRSpliceExpr>(E)) {
      auto val = Value::getFromOpaquePointer(splice->getOpaqueAIROp());
      llvm::SmallString<8> buf;
      llvm::raw_svector_ostream os(buf);
      val.printAsOperand(os, State);
      splice->setDisplayName(buf);
    }
    return Action::Continue(E);
  }
};

/// Before printing, assign MLIR SSA names to all AIRSpliceExpr nodes
/// so the ASTDumper can render them as %N.
static void assignSpliceDisplayNames(ModuleOp module,
                                     const OpPrintingFlags &flags) {
  AsmState asmState(module.getOperation(), flags);

  module->walk([&](Operation *op) {
    ASTNodeAttr attr;
    if (auto e = dyn_cast<ASTExprOp>(op))
      attr = e.getNodeAttr();
    else if (auto s = dyn_cast<ASTStmtOp>(op))
      attr = s.getNodeAttr();
    else
      return;

    auto ast = swift::ASTNode::getFromOpaqueValue(attr.getOpaquePointer());
    if (Expr *expr = ast.dyn_cast<Expr *>()) {
      SpliceNameAssigner assigner(asmState);
      expr->walk(assigner);
    }
  });
}

} // end anonymous namespace

namespace swift {

/// \returns true if there was an error diagnostic emitted
bool performAirInflation(CompilerInstance &CI, ModuleDecl *M,
                         std::optional<StringRef> OutputFile) {
  MLIRContext context;
  context.loadDialect<air::AIRDialect, mlir::scf::SCFDialect,
                      mlir::cf::ControlFlowDialect>();

  AIRGenModule AGM(context, ModuleOp::create(AIRLoc(M, &context)));
  AGM.emitModule(M);
  AGM.performDIExpansion();

  // Lower scf.if → cf.cond_br / cf.br.
  context.disableMultithreading();
  PassManager pm(&context);
  pm.addPass(createSCFToControlFlowPass());
  (void)pm.run(AGM.getModule());

  if (OutputFile) {
    withOutputPath(M->getASTContext().Diags, CI.getOutputBackend(), *OutputFile,
       [&](raw_ostream &out) {
         OpPrintingFlags flags;
         flags.assumeVerified();
         assignSpliceDisplayNames(AGM.getModule(), flags);
         AGM.getModule()->print(out, flags);
         return false;
       });
  }

  return false;
}

}

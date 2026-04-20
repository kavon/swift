#include "AIR.h"
#include "mlir/Conversion/SCFToControlFlow/SCFToControlFlow.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlowOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassManager.h"
#include "swift/AIR/AIROps.h"
#include "swift/AST/ASTContext.h"

#include "swift/AST/Decl.h"
#include "swift/AST/FileSystem.h"
#include "swift/AST/Module.h"
#include "swift/Frontend/Frontend.h"

#include "AIRGenModule.h"

using namespace mlir;
using namespace mlir::air;

namespace swift {

/// \returns true if there was an error diagnostic emitted
bool performAirInflation(CompilerInstance &CI, ModuleDecl *M,
                         std::optional<StringRef> OutputFile) {
  MLIRContext context;
  context.loadDialect<air::AIRDialect, mlir::scf::SCFDialect,
                      mlir::cf::ControlFlowDialect>();

  AIRGenModule AGM(context, ModuleOp::create(AIRLoc(M, &context)));
  AGM.emitModule(M);

  // Run the pass pipeline: DI expansion then SCF→CF lowering.
  context.disableMultithreading();
  PassManager pm(&context);
  // TODO: fix func.return to match function result types, then re-enable.
  pm.enableVerifier(false);

  // TODO: wire this to a command-line flag (e.g., -air-print-pipeline).
  OpPrintingFlags printFlags;
  printFlags.assumeVerified();
  pm.enableIRPrinting(
      [](Pass *, Operation *) { return true; },
      [](Pass *, Operation *) { return true; },
      /*printModuleScope=*/true,
      /*printAfterOnlyOnChange=*/true,
      /*printAfterOnlyOnFailure=*/false,
      llvm::errs(),
      printFlags);

  pm.addPass(createDIExpansionPass());
  pm.addPass(createSCFToControlFlowPass());
  (void)pm.run(AGM.getModule());

  if (OutputFile) {
    withOutputPath(M->getASTContext().Diags, CI.getOutputBackend(), *OutputFile,
       [&](raw_ostream &out) {
         OpPrintingFlags flags;
         flags.assumeVerified();
         assignSpliceDisplayNames(AGM.getModule());
         AGM.getModule()->print(out, flags);
         return false;
       });
  }

  return false;
}

}

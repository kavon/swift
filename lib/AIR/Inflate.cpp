#include "AIR.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Pass/PassManager.h"
#include "swift/AIR/AIROps.h"
#include "swift/AST/ASTContext.h"

#include "swift/AST/Decl.h"
#include "swift/AST/FileSystem.h"
#include "swift/AST/Module.h"
#include "swift/Frontend/Frontend.h"

#include "AIRGenModule.h"

using namespace mlir;

namespace swift {

/// \returns true if there was an error diagnostic emitted
bool performAirInflation(CompilerInstance &CI, ModuleDecl *M,
                         std::optional<StringRef> OutputFile) {
  MLIRContext context;
  context.loadDialect<air::AIRDialect>();

  AIRGenModule AGM(context, ModuleOp::create(AIRLoc(M, &context)));
  AGM.emitModule(M);
  AGM.performDIExpansion();

  if (OutputFile) {
    withOutputPath(M->getASTContext().Diags, CI.getOutputBackend(), *OutputFile,
       [&](raw_ostream &out) {
         OpPrintingFlags flags;
         flags.assumeVerified(); // Avoids double-quoted ops.
         AGM.getModule()->print(out, flags);
         return false; // failed
       });
  }

  // Cannot enable multi-threading if we're printing.
  context.disableMultithreading();

  PassManager pm(&context);

  // FIXME: output to the actual file.
  // raw_ostream &out = llvm::errs();
  // pm.enableIRPrinting([](Pass *, Operation *) { return true; },
  //                     [](Pass *, Operation *) { return true; }, true, true,
  //                     true, out, OpPrintingFlags());

  (void)pm.run(AGM.getModule());

  return false;
}

}

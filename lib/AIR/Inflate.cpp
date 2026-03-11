#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/PassManager.h"

using namespace mlir;

void airSmokeTest() {
  MLIRContext context;

  OpBuilder builder(&context);
  auto module = ModuleOp::create(builder.getUnknownLoc());

  PassManager pm(&context);
  (void)pm.run(module);

  module->erase();
}

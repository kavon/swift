//
// Created by Kavon Farvardin on 3/18/26.
//

#ifndef SWIFT_AIRGEN_MODULE_H
#define SWIFT_AIRGEN_MODULE_H

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "swift/AST/Decl.h"
#include "swift/AST/Module.h"

using namespace swift;

class AIRGenModule {
  mlir::MLIRContext &context;
  mlir::OpBuilder builder;
  mlir::ModuleOp module;
public:
  AIRGenModule(mlir::MLIRContext &ctx, mlir::ModuleOp mod)
    : context(ctx), builder(&ctx), module(mod) {}

  mlir::OpBuilder &getBuilder() { return builder; }
  mlir::ModuleOp getModule() { return module; }
  mlir::MLIRContext &getContext() { return context; }

  void emitModule(ModuleDecl *M);
  void emitDecl(Decl *D);
  void emitFunction(FuncDecl *FD);
};

#endif // SWIFT_AIRGEN_MODULE_H

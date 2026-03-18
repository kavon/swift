
#include "AIRGenModule.h"
#include "ASTVisitor.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "swift/AST/FileUnit.h"
#include "swift/AST/SourceFile.h"

using namespace swift;
using namespace mlir;

namespace {

/// Visitor for top-level declarations.
class DeclEmitter : public air::ASTVisitor<DeclEmitter> {
  AIRGenModule &AGM;
public:
  DeclEmitter(AIRGenModule &agm) : AGM(agm) {}

  void visitFuncDecl(FuncDecl *FD) {
    AGM.emitFunction(FD);
  }

  // Other decl kinds as needed...
};

} // end anonymous namespace



void AIRGenModule::emitFunction(FuncDecl *FD) {
  auto loc = builder.getUnknownLoc(); // TODO: convert Swift SourceLoc -> mlir::Location

  // Build the MLIR function type from the Swift AST type.
  // For now, a placeholder with no args/results:
  auto funcType = builder.getFunctionType({}, {});

  // Create the function and insert into the module.
  auto funcOp = func::FuncOp::create(loc, FD->getBaseIdentifier().str(), funcType);
  module.push_back(funcOp);

  // Add an entry block and point the builder into it.
  Block *entryBlock = funcOp.addEntryBlock();
  builder.setInsertionPointToEnd(entryBlock);

  // TODO: visit the function body (FD->getBody()) to emit statements/exprs.

  // Every block needs a terminator.
  builder.create<func::ReturnOp>(loc);
}

void AIRGenModule::emitDecl(Decl *D) {
  if (auto *fn = dyn_cast<FuncDecl>(D)) {
    emitFunction(fn);
  }
}


void AIRGenModule::emitModule(ModuleDecl *M) {
  getModule().setName(M->getName().str());

  for (const FileUnit *file : M->getFiles()) {
    if (auto *sf = dyn_cast<SourceFile>(file)) {
      for (Decl *decl : sf->getTopLevelDecls()) {
        emitDecl(decl);
      }
    }
    // TODO: See ASTLoweringRequest for more things to emit
    // if (auto *synth = file->getSynthesizedFile()) {
    //   synth->getTopLevelDecls(Results);
    // }
  }
}

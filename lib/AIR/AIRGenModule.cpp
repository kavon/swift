
#include "AIRGenModule.h"
#include "ASTVisitor.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "swift/AIR/AIROps.h"
#include "swift/AST/FileUnit.h"
#include "swift/AST/ParameterList.h"
#include "swift/AST/SourceFile.h"
#include "swift/AST/Stmt.h"

using namespace swift;
using namespace mlir;
using namespace mlir::air;

namespace {

/// Create an ASTNodeAttr wrapping an ASTNode.
ASTNodeAttr getASTNodeAttr(OpBuilder &builder, swift::ASTNode node) {
  return ASTNodeAttr::get(builder.getContext(), node.getOpaqueValue());
}

/// Emits AIR ops for expressions. For now, all expressions are opaque.
class ExprEmitter : public swift::air::ExprVisitor<ExprEmitter, Value> {
  AIRGenModule &AGM;
  MLIRContext *Ctx;
public:
  ExprEmitter(AIRGenModule &agm) : AGM(agm), Ctx(&agm.getContext()) {}

  Value visitExpr(Expr *E) {
    auto &builder = AGM.getBuilder();
    auto loc = builder.getUnknownLoc();
    auto attr = getASTNodeAttr(builder, E);
    auto resultTy = ASTType::get(Ctx, E->getType()->getCanonicalType());
    return builder.create<ASTExprOp>(loc, resultTy, attr);
  }
};

/// Emits AIR ops for statements.
class StmtEmitter
    : public swift::air::ASTVisitor<StmtEmitter, /*ExprRetTy=*/Value> {
  AIRGenModule &AGM;
  ExprEmitter exprEmitter;

  OpBuilder &getBuilder() { return AGM.getBuilder(); }
  Location getLoc() { return getBuilder().getUnknownLoc(); }

public:
  StmtEmitter(AIRGenModule &agm) : AGM(agm), exprEmitter(agm) {}

  /// A BraceStmt is an air.begin_scope
  void visitBraceStmt(BraceStmt *BS) {
    auto &builder = getBuilder();
    auto loc = getLoc();

    auto scopeOp = builder.create<BeginScopeOp>(loc);
    auto *body = new Block();
    scopeOp.getBody().push_back(body);
    builder.setInsertionPointToEnd(body);

    emitBraceStmtBody(BS);

    // Ensure the block has a terminator.
    BeginScopeOp::ensureTerminator(scopeOp.getBody(), builder, loc);
    builder.setInsertionPointAfter(scopeOp);
  }

  /// Emit an IfStmt as an air.if with then/else regions.
  void visitIfStmt(IfStmt *IS) {
    auto &builder = getBuilder();
    auto loc = getLoc();

    // Emit the condition. For now, take the first boolean condition element
    // and emit it as an embedded expression.
    Value cond;
    auto condElements = IS->getCond();
    if (!condElements.empty() &&
        condElements.front().getKind() ==
            StmtConditionElement::CK_Boolean) {
      cond = exprEmitter.visit(condElements.front().getBoolean());
    } else {
      // For pattern-binding or availability conditions, emit the whole
      // condition list as an opaque embedded expression from the first element.
      auto *firstExpr = condElements.empty()
                            ? nullptr
                            : condElements.front().getBoolean();
      if (firstExpr)
        cond = exprEmitter.visit(firstExpr);
      else
        cond = exprEmitter.visitExpr(nullptr); // placeholder
    }

    auto ifOp = builder.create<IfOp>(loc, cond);

    // Emit the 'then' region.
    {
      auto *thenBlock = new Block();
      ifOp.getThenRegion().push_back(thenBlock);
      builder.setInsertionPointToEnd(thenBlock);

      if (auto *thenBody = IS->getThenStmt()) {
        emitBraceStmtBody(thenBody);
      }
      IfOp::ensureTerminator(ifOp.getThenRegion(), builder, loc);
    }

    // Emit the 'else' region if present.
    if (auto *elseStmt = IS->getElseStmt()) {
      auto *elseBlock = new Block();
      ifOp.getElseRegion().push_back(elseBlock);
      builder.setInsertionPointToEnd(elseBlock);

      if (auto *elseBrace = dyn_cast<BraceStmt>(elseStmt)) {
        emitBraceStmtBody(elseBrace);
      } else {
        // else-if chain: the else is another IfStmt.
        this->visit(elseStmt);
      }
      IfOp::ensureTerminator(ifOp.getElseRegion(), builder, loc);
    }

    builder.setInsertionPointAfter(ifOp);
  }

  /// Emit a ReturnStmt as an embedded_stmt (opaque for now).
  void visitReturnStmt(ReturnStmt *RS) {
    emitOpaqueStmt(RS);
  }

  /// Fallback: emit any unhandled statement as an opaque air.embedded_stmt.
  void visitStmt(Stmt *S) {
    emitOpaqueStmt(S);
  }

  /// Emit an expression, returning its SSA value.
  Value visitExpr(Expr *E) {
    return exprEmitter.visit(E);
  }

  /// Emit a declaration inside a brace stmt body.
  void visitDecl(Decl *D) {
    auto &builder = getBuilder();
    auto loc = getLoc();
    auto attr = getASTNodeAttr(builder, D);
    builder.create<ASTStmtOp>(loc, attr);
  }

private:
  /// Emit the elements of a BraceStmt directly into the current insertion
  /// point (without creating a new air.scope).
  void emitBraceStmtBody(BraceStmt *BS) {
    for (auto elem : BS->getElements()) {
      if (auto *expr = elem.dyn_cast<Expr *>()) {
        exprEmitter.visit(expr);
      } else if (auto *stmt = elem.dyn_cast<Stmt *>()) {
        this->visit(stmt);
      } else if (auto *decl = elem.dyn_cast<Decl *>()) {
        visitDecl(decl);
      }
    }
  }

  void emitOpaqueStmt(Stmt *S) {
    auto &builder = getBuilder();
    auto loc = getLoc();
    auto attr = getASTNodeAttr(builder, swift::ASTNode(S));
    builder.create<ASTStmtOp>(loc, attr);
  }
};

/// Visitor for top-level declarations.
class DeclEmitter : public swift::air::ASTVisitor<DeclEmitter> {
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
  auto *ctx = &getContext();

  // Build the MLIR function type from the Swift AST type.
  SmallVector<mlir::Type> paramTypes;
  for (ParamDecl *pd : FD->getParameters()->getArray()) {
    auto ty = ASTType::get(ctx, pd->getInterfaceType()->getCanonicalType());
    paramTypes.push_back(ty);
  }
  auto resultTy =
    ASTType::get(ctx, FD->getResultInterfaceType()->getCanonicalType());
  auto funcType = builder.getFunctionType(paramTypes, {resultTy});


  // Create the function and insert into the module.
  auto funcOp = func::FuncOp::create(loc, FD->getBaseIdentifier().str(), funcType);
  module.push_back(funcOp);

  // Add an entry block and point the builder into it.
  Block *entryBlock = funcOp.addEntryBlock();
  builder.setInsertionPointToEnd(entryBlock);

  // Emit the function body.
  if (auto *body = FD->getBody()) {
    StmtEmitter stmtEmitter(*this);
    stmtEmitter.visitBraceStmt(body);
  }

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


// MARK: DI Expansion

namespace {

ASTNode getAST(ASTNodeAttr attr) {
    return swift::ASTNode::getFromOpaqueValue(attr.getOpaquePointer());
}

/// Emits AIR ops for expressions. For now, all expressions are opaque.
class DIExpansion : public ASTVisitor<DIExpansion> {
  AIRGenModule &AGM;
  MLIRContext *Ctx;
public:
  DIExpansion(AIRGenModule &agm) : AGM(agm), Ctx(&agm.getContext()) {}

  void visitDecl(Decl *D) {
    // SKIPPED!
  }

  void visitExpr(Expr *E) {
    // SKIPPED!
  }

  // TODO: This should do lookup and return the DeclareOp it refers to.
  // That should be the return of this entire visitor: the variables mentioned.
  void visitDeclRefExpr(DeclRefExpr *DR) {

  }

  void visitVarDecl(VarDecl *VD) {
    auto &builder = AGM.getBuilder();
    auto loc = builder.getUnknownLoc();
    DI_DeclareOp::create(builder, loc);

    if (VD->hasInitialValue()) {
      DI_InitOp::create(builder, loc);
    }
  }

  void visitAssignExpr(AssignExpr *E) {
    auto &builder = AGM.getBuilder();
    auto loc = builder.getUnknownLoc();

    DI_InitOp::create(builder, loc);
  }

  void visitConsumeExpr(ConsumeExpr *E) {
    auto &builder = AGM.getBuilder();
    auto loc = builder.getUnknownLoc();

    // TODO: visit the subexpr first, to get back any variables referenced by it
    // then mark them all consumed.

    DI_ConsumeOp::create(builder, loc);
  }
};

}

// TODO: this should live separately as a pass.
void AIRGenModule::performDIExpansion() {
  DIExpansion expander(*this);

  module->walk([&](Operation *op) {
    auto expand = [&](ASTNodeAttr attr) {
      builder.setInsertionPointAfter(op);
      auto ast = getAST(attr);
      if (Decl *decl = dyn_cast<Decl *>(ast)) {
        expander.visit(decl);
      } else if (Expr *expr = dyn_cast<Expr *>(ast)) {
        expander.visit(expr);
      } else if (Stmt *stmt = dyn_cast<Stmt *>(ast)) {
        llvm_unreachable("AIRGenModule should have lowered Stmt's earlier!");
      }
    };

    if (ASTStmtOp stmt = dyn_cast<ASTStmtOp>(op)) {
      expand(stmt.getNode());
    } else if (ASTExprOp expr = dyn_cast<ASTExprOp>(op)) {
      expand(expr.getNode());
    }
  });
}

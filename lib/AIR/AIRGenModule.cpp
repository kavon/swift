
#include "AIRGenModule.h"
#include "ASTVisitor.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "swift/AIR/AIROps.h"
#include "swift/AST/ASTWalker.h"
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

  void visitBraceStmt(BraceStmt *BS) {
    emitBraceStmtBody(BS);
  }

  /// Emit an IfStmt as scf.if with then/else regions.
  void visitIfStmt(IfStmt *IS) {
    auto &builder = getBuilder();
    auto loc = getLoc();

    // Emit the condition as !air.ast_type<Bool>.
    Value cond;
    auto condElements = IS->getCond();
    if (!condElements.empty() &&
        condElements.front().getKind() ==
            StmtConditionElement::CK_Boolean) {
      cond = exprEmitter.visit(condElements.front().getBoolean());
    } else {
      auto *firstExpr = condElements.empty()
                            ? nullptr
                            : condElements.front().getBoolean();
      if (firstExpr)
        cond = exprEmitter.visit(firstExpr);
      else
        cond = exprEmitter.visitExpr(nullptr);
    }

    // Bridge !air.ast_type<Bool> → i1 for scf.if.
    auto i1Cond = builder.create<UnrealizedConversionCastOp>(
        loc, builder.getI1Type(), cond).getResult(0);

    bool hasElse = (IS->getElseStmt() != nullptr);
    auto ifOp = builder.create<scf::IfOp>(loc, i1Cond, hasElse);

    // Emit into the 'then' region (block + yield already created by scf.if).
    builder.setInsertionPointToStart(&ifOp.getThenRegion().front());
    if (auto *thenBody = IS->getThenStmt())
      emitBraceStmtBody(thenBody);

    // Emit into the 'else' region if present.
    if (hasElse) {
      builder.setInsertionPointToStart(&ifOp.getElseRegion().front());
      if (auto *elseBrace = dyn_cast<BraceStmt>(IS->getElseStmt()))
        emitBraceStmtBody(elseBrace);
      else
        this->visit(IS->getElseStmt());
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

using VarRefs = SmallVector<mlir::Value, 2>;

class DIExpansion
    : public ASTVisitor<DIExpansion, /*ExprRetTy=*/VarRefs> {
  AIRGenModule &AGM;
  MLIRContext *Ctx;
  llvm::DenseMap<VarDecl *, mlir::Value> VarLocs;

public:
  DIExpansion(AIRGenModule &agm) : AGM(agm), Ctx(&agm.getContext()) {}

  void visitDecl(Decl *D) {}

  VarRefs visitExpr(Expr *E) { return {}; }

  VarRefs visitLoadExpr(LoadExpr *E) {
    return visit(E->getSubExpr());
  }

  VarRefs visitDeclRefExpr(DeclRefExpr *DR) {
    if (auto *VD = dyn_cast<VarDecl>(DR->getDecl())) {
      if (mlir::Value val = lookupVarDecl(VD))
        return {val};
    }
    return {};
  }

  mlir::Value lookupVarDecl(VarDecl *VD) {
    auto it = VarLocs.find(VD);
    if (it != VarLocs.end())
      return it->second;
    return nullptr;
  }

  void visitVarDecl(VarDecl *VD) {
    auto &builder = AGM.getBuilder();
    auto loc = builder.getUnknownLoc();
    auto varRefTy = VarRefType::get(Ctx);
    auto nameAttr = builder.getStringAttr(VD->getBaseName().userFacingName());
    auto declOp = DI_DeclareOp::create(builder, loc, varRefTy, nameAttr);
    VarLocs[VD] = declOp.getResult();

    if (VD->hasInitialValue()) {
      DI_InitOp::create(builder, loc, mlir::ValueRange{declOp.getResult()});
    }
  }

  VarRefs visitAssignExpr(AssignExpr *E) {
    auto &builder = AGM.getBuilder();
    auto loc = builder.getUnknownLoc();

    VarRefs srcRefs = visit(E->getSrc());
    if (!srcRefs.empty())
      DI_ConsumeOp::create(builder, loc, srcRefs);

    VarRefs destRefs = visit(E->getDest());
    if (!destRefs.empty())
      DI_InitOp::create(builder, loc, destRefs);

    return destRefs;
  }

  VarRefs visitConsumeExpr(ConsumeExpr *E) {
    auto &builder = AGM.getBuilder();
    auto loc = builder.getUnknownLoc();

    VarRefs refs = visit(E->getSubExpr());
    if (!refs.empty())
      DI_ConsumeOp::create(builder, loc, refs);
    return {};
  }

  VarRefs visitApplyExpr(ApplyExpr *E) {
    auto &builder = AGM.getBuilder();
    auto loc = builder.getUnknownLoc();

    VarRefs allRefs;
    if (auto *args = E->getArgs()) {
      for (auto arg : *args) {
        VarRefs argRefs = visit(arg.getExpr());
        allRefs.append(argRefs.begin(), argRefs.end());
      }
    }
    if (!allRefs.empty())
      DI_UseOp::create(builder, loc, allRefs);
    return {};
  }
};

/// Walks an expression tree and wraps DeclRefExprs that reference
/// known variables with AIRSpliceExpr nodes.
class SpliceInserter : public swift::ASTWalker {
  DIExpansion &Expander;

public:
  SpliceInserter(DIExpansion &expander) : Expander(expander) {}

  PostWalkResult<Expr *> walkToExprPost(Expr *E) override {
    auto *DR = dyn_cast<DeclRefExpr>(E);
    if (!DR)
      return Action::Continue(E);

    auto *VD = dyn_cast<VarDecl>(DR->getDecl());
    if (!VD)
      return Action::Continue(E);

    mlir::Value val = Expander.lookupVarDecl(VD);
    if (!val)
      return Action::Continue(E);

    auto &ctx = VD->getASTContext();
    auto *splice = new (ctx) AIRSpliceExpr(DR, val.getAsOpaquePointer());
    return Action::Continue(splice);
  }
};

} // end anonymous namespace

// TODO: this should live separately as a pass.
void AIRGenModule::performDIExpansion() {
  DIExpansion expander(*this);
  SpliceInserter splicer(expander);

  module->walk([&](Operation *op) {
    auto expand = [&](ASTNodeAttr attr) {
      builder.setInsertionPointAfter(op);
      auto ast = getAST(attr);
      if (Decl *decl = dyn_cast<Decl *>(ast)) {
        expander.visit(decl);
      } else if (Expr *expr = dyn_cast<Expr *>(ast)) {
        expander.visit(expr);
        expr->walk(splicer);
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

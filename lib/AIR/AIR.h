#ifndef SWIFT_AIR_H
#define SWIFT_AIR_H

#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/Location.h"
#include "swift/AST/Expr.h"
#include "swift/Basic/SourceLoc.h"

/// Create an mlir::Location holding a swift::SourceLoc
class AIRLoc {
  mlir::Location loc;

public:
  using OpaqueSourceLoc = const char *_Nullable;

  template <typename HasLoc>
  AIRLoc(HasLoc *ASTNode, mlir::MLIRContext *ctx)
      : AIRLoc(ASTNode->getLoc(), ctx) {}

  AIRLoc(swift::SourceLoc srcLoc, mlir::MLIRContext *ctx)
      : loc(mlir::OpaqueLoc::get<OpaqueSourceLoc>(srcLoc.getPointer(), ctx)) {}

  /// Implicit conversion to mlir::Location so you can pass AIRLoc
  /// anywhere an mlir::Location is expected.
  /*implicit*/ operator mlir::Location() const { return loc; }

  /// Returns an invalid SourceLoc if it wasn't an AIRLoc.
  static swift::SourceLoc getASTLoc(mlir::Location loc) {
    if (auto opaque = mlir::dyn_cast<mlir::OpaqueLoc>(loc))
      return swift::SourceLoc::getFromPointer(
          mlir::OpaqueLoc::getUnderlyingLocation<OpaqueSourceLoc>(opaque));

    return swift::SourceLoc();
  }
};

#endif // SWIFT_AIR_H

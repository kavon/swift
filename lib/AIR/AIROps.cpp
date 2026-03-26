//===--- AIROps.cpp - AIR MLIR Dialect implementation ----------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2024 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "swift/AIR/AIROps.h"
#include "swift/AST/ASTNode.h"
#include "swift/AST/Type.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/OpImplementation.h"
#include "llvm/ADT/TypeSwitch.h"

using namespace mlir;
using namespace mlir::air;

//===----------------------------------------------------------------------===//
// AIR Dialect
//===----------------------------------------------------------------------===//

#include "swift/AIR/AIROpsDialect.cpp.inc"

void AIRDialect::initialize() {
  addOperations<
#define GET_OP_LIST
#include "swift/AIR/AIROps.cpp.inc"
      >();
  addAttributes<
#define GET_ATTRDEF_LIST
#include "swift/AIR/AIROpsAttrDefs.cpp.inc"
      >();
  addTypes<
#define GET_TYPEDEF_LIST
#include "swift/AIR/AIROpsTypes.cpp.inc"
      >();
}

//===----------------------------------------------------------------------===//
// ASTNodeAttr custom print/parse
//===----------------------------------------------------------------------===//

void ASTNodeAttr::print(AsmPrinter &p) const {
  auto node = swift::ASTNode::getFromOpaqueValue(getOpaquePointer());
  p << "(\n";
  node.dump(p.getStream(), /*Indent=*/6);
  p << ")";
}

Attribute ASTNodeAttr::parse(AsmParser &parser, Type type) {
  return {}; // not parseable
}

//===----------------------------------------------------------------------===//
// IfOp
//===----------------------------------------------------------------------===//

void IfOp::print(OpAsmPrinter &p) {
  p << " " << getCondition() << " ";
  p.printRegion(getThenRegion(), /*printEntryBlockArgs=*/false);
  auto &elseRegion = getElseRegion();
  if (!elseRegion.empty()) {
    p << " else ";
    p.printRegion(elseRegion, /*printEntryBlockArgs=*/false);
  }
  p.printOptionalAttrDict((*this)->getAttrs());
}

ParseResult IfOp::parse(OpAsmParser &parser, OperationState &result) {
  return failure();
}

//===----------------------------------------------------------------------===//
// ASTExprOp
//===----------------------------------------------------------------------===//

static void printASTNode(OpAsmPrinter &p, void *opaquePtr) {
  auto node = swift::ASTNode::getFromOpaqueValue(opaquePtr);

  // Dump the AST to a string with no base indentation.
  llvm::SmallString<256> buf;
  llvm::raw_svector_ostream sstr(buf);
  node.dump(sstr, /*Indent=*/0);

  llvm::StringRef text(buf);
  text = text.rtrim('\n');

  p.increaseIndent();
  p.printNewline();
  p << " ⊢ ";
  p.increaseIndent();
  // Strip the new-lines so we can use printNewline instead, which will
  // respect the indentation level of the printer.
  llvm::SmallVector<llvm::StringRef, 16> lines;
  text.split(lines, '\n', /*MaxSplit=*/-1, /*KeepEmpty=*/false);
  for (auto line : lines) {
    p.getStream() << line;
    p.printNewline();
  }
  p.decreaseIndent();
  p.decreaseIndent();
}


void ASTExprOp::print(OpAsmPrinter &p) {
  p << " : ";
  p.printType(getResult().getType());
  printASTNode(p, getNodeAttr().getOpaquePointer());
}

ParseResult ASTExprOp::parse(OpAsmParser &parser, OperationState &result) {
  return {}; // not parseable
}

//===----------------------------------------------------------------------===//
// ASTStmtOp
//===----------------------------------------------------------------------===//

void ASTStmtOp::print(OpAsmPrinter &p) {
  printASTNode(p, getNodeAttr().getOpaquePointer());
}

ParseResult ASTStmtOp::parse(OpAsmParser &parser, OperationState &result) {
  return {}; // not parseable
}


//===----------------------------------------------------------------------===//
// TableGen'd op/type/attr definitions
//===----------------------------------------------------------------------===//

#define GET_OP_CLASSES
#include "swift/AIR/AIROps.cpp.inc"

#define GET_TYPEDEF_CLASSES
#include "swift/AIR/AIROpsTypes.cpp.inc"

//===----------------------------------------------------------------------===//
// ASTType
//===----------------------------------------------------------------------===//

void ASTType::print(mlir::AsmPrinter &p) const {
  p << "<";
  getAstType().print(p.getStream());
  p << ">";
}

mlir::Type ASTType::parse(mlir::AsmParser &p) {
  p.emitError(p.getCurrentLocation(), "parsing is not supported");
  return {};
}

#define GET_ATTRDEF_CLASSES
#include "swift/AIR/AIROpsAttrDefs.cpp.inc"

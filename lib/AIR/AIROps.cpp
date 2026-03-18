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
#include "mlir/IR/DialectImplementation.h"
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
  p << "<\n";
  node.dump(p.getStream(), /*Indent=*/2);
  p << ">";
}

Attribute ASTNodeAttr::parse(AsmParser &parser, Type type) {
  return {}; // not parseable — contains session-local pointers
}

//===----------------------------------------------------------------------===//
// IfOp custom print/parse
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
  OpAsmParser::UnresolvedOperand condOperand;
  Type condType;

  if (parser.parseOperand(condOperand))
    return failure();

  // Resolve the condition operand as !air.ast_value.
  condType = ASTValueType::get(parser.getContext());
  if (parser.resolveOperand(condOperand, condType, result.operands))
    return failure();

  // Parse the 'then' region.
  auto *thenRegion = result.addRegion();
  if (parser.parseRegion(*thenRegion, /*arguments=*/{}, /*argTypes=*/{}))
    return failure();
  IfOp::ensureTerminator(*thenRegion, parser.getBuilder(),
                         result.location);

  // Parse the optional 'else' region.
  auto *elseRegion = result.addRegion();
  if (!parser.parseOptionalKeyword("else")) {
    if (parser.parseRegion(*elseRegion, /*arguments=*/{}, /*argTypes=*/{}))
      return failure();
    IfOp::ensureTerminator(*elseRegion, parser.getBuilder(),
                           result.location);
  }

  if (parser.parseOptionalAttrDict(result.attributes))
    return failure();

  return success();
}

//===----------------------------------------------------------------------===//
// TableGen'd op/type/attr definitions
//===----------------------------------------------------------------------===//

#define GET_OP_CLASSES
#include "swift/AIR/AIROps.cpp.inc"

#define GET_TYPEDEF_CLASSES
#include "swift/AIR/AIROpsTypes.cpp.inc"

#define GET_ATTRDEF_CLASSES
#include "swift/AIR/AIROpsAttrDefs.cpp.inc"

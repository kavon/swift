//===--- AIROps.td - AIR dialect definition ----------------*- tablegen -*-===//
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

#ifndef SWIFT_AIR_OPS_TD
#define SWIFT_AIR_OPS_TD

include "mlir/IR/OpBase.td"
include "mlir/IR/AttrTypeBase.td"
include "mlir/Interfaces/SideEffectInterfaces.td"

//===----------------------------------------------------------------------===//
// AIR Dialect
//===----------------------------------------------------------------------===//

def AIR_Dialect : Dialect {
  let name = "air";
  let cppNamespace = "::mlir::air";
  let useDefaultTypePrinterParser = 1;
  let useDefaultAttributePrinterParser = 1;
}

//===----------------------------------------------------------------------===//
// Base class for AIR ops
//===----------------------------------------------------------------------===//

class AIR_Op<string mnemonic, list<Trait> traits = []>
    : Op<AIR_Dialect, mnemonic, traits>;

//===----------------------------------------------------------------------===//
// AIR Attributes
//===----------------------------------------------------------------------===//

/// Stores a void* compatible with ASTNode::getFromOpaqueValue().
/// Custom printer calls ASTNode::dump() for rich textual output.
/// Not parseable (session-local pointers).
def AIR_ASTNodeAttr : AttrDef<AIR_Dialect, "ASTNode"> {
  let mnemonic = "ast_node";
  let parameters = (ins "void *":$opaquePointer);
  let hasCustomAssemblyFormat = 1;
}

//===----------------------------------------------------------------------===//
// AIR Types
//===----------------------------------------------------------------------===//

/// Opaque type for results of air.embedded_expr. No parameters.
def AIR_ASTValueType : TypeDef<AIR_Dialect, "ASTValue"> {
  let mnemonic = "ast_value";
}

//===----------------------------------------------------------------------===//
// AIR Operations
//===----------------------------------------------------------------------===//

def AIR_ScopeOp : AIR_Op<"scope", [
    SingleBlockImplicitTerminator<"mlir::air::YieldOp">]> {
  let summary = "Lexical scope (BraceStmt)";
  let regions = (region SizedRegion<1>:$body);
  let assemblyFormat = "$body attr-dict";
}

def AIR_IfOp : AIR_Op<"if", [
    SingleBlockImplicitTerminator<"mlir::air::YieldOp">,
    NoRegionArguments]> {
  let summary = "Conditional (if/else)";
  let arguments = (ins AIR_ASTValueType:$condition);
  let regions = (region SizedRegion<1>:$thenRegion,
                        MaxSizedRegion<1>:$elseRegion);
  let hasCustomAssemblyFormat = 1;
}

def AIR_YieldOp : AIR_Op<"yield", [Pure, Terminator]> {
  let summary = "Region terminator";
  let assemblyFormat = "attr-dict";
}

def AIR_EmbeddedExprOp : AIR_Op<"embedded_expr", [Pure]> {
  let summary = "Unevaluated Expr* from the Swift AST";
  let arguments = (ins AIR_ASTNodeAttr:$node);
  let results = (outs AIR_ASTValueType:$result);
  let assemblyFormat = "$node attr-dict `:` type($result)";
}

def AIR_EmbeddedStmtOp : AIR_Op<"embedded_stmt", []> {
  let summary = "Unevaluated Stmt* from the Swift AST";
  let arguments = (ins AIR_ASTNodeAttr:$node);
  let assemblyFormat = "$node attr-dict";
}

#endif // SWIFT_AIR_OPS_TD

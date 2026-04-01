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

  let dependentDialects = [
    "func::FuncDialect"
  ];
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

def AIR_ASTType : TypeDef<AIR_Dialect, "AST"> {
  let mnemonic = "ast_type";
  let summary = "A wrapper for swift::CanType";
  let parameters = (ins "swift::CanType":$astType);

  let hasCustomAssemblyFormat = 1;
}

//===----------------------------------------------------------------------===//
// AIR Operations
//===----------------------------------------------------------------------===//

def AIR_BeginScopeOp : AIR_Op<"begin_scope", [
    SingleBlockImplicitTerminator<"mlir::air::EndScopeOp">]> {
  let summary = "Begins a lexical scope";
  let regions = (region SizedRegion<1>:$body);
  let assemblyFormat = "$body attr-dict";
}

def AIR_EndScopeOp : AIR_Op<"end_scope", [Pure, Terminator]> {
  let summary = "Ends a lexical scope";
  let assemblyFormat = "attr-dict";
}

def AIR_ASTExprOp : AIR_Op<"ast_expr", [Pure]> {
  let summary = "Unevaluated Expr* from the AST";
  let arguments = (ins AIR_ASTNodeAttr:$node);
  let results = (outs AIR_ASTType:$result);
  let hasCustomAssemblyFormat = 1;
}

def AIR_ASTStmtOp : AIR_Op<"ast_stmt", []> {
  let summary = "Unevaluated Stmt* from the AST";
  let arguments = (ins AIR_ASTNodeAttr:$node);
  let hasCustomAssemblyFormat = 1;
}

def AIR_IfOp : AIR_Op<"if", [
    SingleBlockImplicitTerminator<"mlir::air::EndScopeOp">,
    NoRegionArguments]> {
  let summary = "Conditional (if/else)";
  let arguments = (ins AIR_ASTType:$condition);
  let regions = (region SizedRegion<1>:$thenRegion,
                        MaxSizedRegion<1>:$elseRegion);
  let hasCustomAssemblyFormat = 1;
}


// Abstract Variable Use instructions

def AIR_DI_DeclareOp : AIR_Op<"di.declare", [Pure]> {
  let summary = "Declares an uninitialized Swift value.";
}

def AIR_DI_InitOp : AIR_Op<"di.init", [Pure]> {
  let summary = "Denotes a program-point that initializes a Swift value.";
}

def AIR_DI_UseOp : AIR_Op<"di.use", [Pure]> {
  let summary = "Denotes a program-point that requires that a Swift value is initialized.";
}

def AIR_DI_ConsumeOp : AIR_Op<"di.consume", [Pure]> {
  let summary = "Denotes a program-point that consumes Swift value, leaving it uninitialized.";
}

#endif // SWIFT_AIR_OPS_TD

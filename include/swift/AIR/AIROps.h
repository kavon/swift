//===--- AIROps.h - AIR MLIR Dialect ---------------------------*- C++ -*-===//
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

#ifndef SWIFT_AIR_AIROPS_H
#define SWIFT_AIR_AIROPS_H

#include "swift/AST/Type.h"
#include "mlir/Bytecode/BytecodeOpInterface.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Dialect.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

//===----------------------------------------------------------------------===//
// AIR Dialect
//===----------------------------------------------------------------------===//

#include "swift/AIR/AIROpsDialect.h.inc"

//===----------------------------------------------------------------------===//
// AIR Attributes
//===----------------------------------------------------------------------===//

#define GET_ATTRDEF_CLASSES
#include "swift/AIR/AIROpsAttrDefs.h.inc"

//===----------------------------------------------------------------------===//
// AIR Types
//===----------------------------------------------------------------------===//

#define GET_TYPEDEF_CLASSES
#include "swift/AIR/AIROpsTypes.h.inc"

//===----------------------------------------------------------------------===//
// AIR Operations
//===----------------------------------------------------------------------===//

#define GET_OP_CLASSES
#include "swift/AIR/AIROps.h.inc"

#endif // SWIFT_AIR_AIROPS_H

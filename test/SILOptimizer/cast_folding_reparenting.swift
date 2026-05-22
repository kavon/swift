// RUN: %target-swift-frontend %s -emit-sil -enable-experimental-feature Reparenting -target %target-cpu-apple-macosx12 | %FileCheck %s --check-prefix=CHECK-OLD
// RUN: %target-swift-frontend %s -emit-sil -enable-experimental-feature Reparenting -target %target-cpu-apple-macosx14 | %FileCheck %s --check-prefix=CHECK-NEW

// Verify that a `is`-cast to a `@reparentable` parent protocol is NOT
// constant-folded when the deployment target is older than the
// `@reparented` extension that introduced the inheritance relationship.

// REQUIRES: OS=macosx
// REQUIRES: swift_feature_Reparenting

@available(macOS 12, *)
@reparentable public protocol RP {}

public protocol Child: RP {}

@available(macOS 14, *)
extension Child: @reparented RP {}

// At a deployment target older than the reparenting extension, the cast
// must remain — the runtime may not have the inheritance yet. Once the
// deployment target reaches the extension's availability, the cast can be
// folded as before.

@available(macOS 12, *)
public func isRPGeneric<T: Child>(_ p: T) -> Bool {
  return p is RP
}

// CHECK-OLD-LABEL: sil [available 12] @$s24cast_folding_reparenting11isRPGenericySbxAA5ChildRzlF
// CHECK-OLD: checked_cast_addr_br {{.*}} to any RP

// CHECK-NEW-LABEL: sil [available 12] @$s24cast_folding_reparenting11isRPGenericySbxAA5ChildRzlF
// CHECK-NEW-NOT: checked_cast_addr_br

public protocol Grandchild: Child {}

@available(macOS 12, *)
public func isRPGrandchild<T: Grandchild>(_ p: T) -> Bool {
  return p is RP
}

// The reparented edge is between Child and RP. Grandchild reaches RP only
// through Child, so a `Grandchild` value's conformance to RP also depends
// on the unavailable extension at the older deployment target.
// CHECK-OLD-LABEL: sil [available 12] @$s24cast_folding_reparenting14isRPGrandchildySbxAA10GrandchildRzlF
// CHECK-OLD: checked_cast_addr_br {{.*}} to any RP

// CHECK-NEW-LABEL: sil [available 12] @$s24cast_folding_reparenting14isRPGrandchildySbxAA10GrandchildRzlF
// CHECK-NEW-NOT: checked_cast_addr_br

// A concrete class conforming to Child has its own conformance to RP only
// because of the reparented extension on Child, so the same gating applies.
public final class MyClass: Child {}

@available(macOS 12, *)
public func isRPClass(_ p: MyClass) -> Bool {
  return p is RP
}

// CHECK-OLD-LABEL: sil [available 12] @$s24cast_folding_reparenting9isRPClassySbAA7MyClassCF
// CHECK-OLD: checked_cast_addr_br {{.*}} to any RP

// CHECK-NEW-LABEL: sil [available 12] @$s24cast_folding_reparenting9isRPClassySbAA7MyClassCF
// CHECK-NEW-NOT: checked_cast_addr_br

// A struct that picks up its conformance to Child via an extension still
// inherits the reparented availability constraint.
public struct MyStruct {}
extension MyStruct: Child {}

@available(macOS 12, *)
public func isRPStruct(_ p: MyStruct) -> Bool {
  return p is RP
}

// CHECK-OLD-LABEL: sil [available 12] @$s24cast_folding_reparenting10isRPStructySbAA8MyStructVF
// CHECK-OLD: checked_cast_addr_br {{.*}} to any RP

// CHECK-NEW-LABEL: sil [available 12] @$s24cast_folding_reparenting10isRPStructySbAA8MyStructVF
// CHECK-NEW-NOT: checked_cast_addr_br

// A class that lists RP directly in its inheritance clause has a non-
// reparented path to RP, so the cast can fold even at the older
// deployment target.
public final class DirectClass: Child, RP {}

@available(macOS 12, *)
public func isRPDirect(_ p: DirectClass) -> Bool {
  return p is RP
}

// CHECK-OLD-LABEL: sil [available 12] @$s24cast_folding_reparenting10isRPDirectySbAA11DirectClassCF
// CHECK-OLD-NOT: checked_cast_addr_br

// CHECK-NEW-LABEL: sil [available 12] @$s24cast_folding_reparenting10isRPDirectySbAA11DirectClassCF
// CHECK-NEW-NOT: checked_cast_addr_br

// Existential `any Child` source: existential-to-existential casts are
// already classified as MaySucceed regardless of reparenting.
@available(macOS 12, *)
public func isRPExistential(_ p: any Child) -> Bool {
  return p is RP
}

// CHECK-OLD-LABEL: sil [available 12] @$s24cast_folding_reparenting15isRPExistentialySbAA5Child_pF
// CHECK-OLD: checked_cast_addr_br {{.*}} to any RP

// CHECK-NEW-LABEL: sil [available 12] @$s24cast_folding_reparenting15isRPExistentialySbAA5Child_pF
// CHECK-NEW: checked_cast_addr_br {{.*}} to any RP

// Unconstrained generic source: nothing is statically known about T's
// conformances, so the cast must run dynamically regardless of the
// deployment target.
@available(macOS 12, *)
public func isRPUnconstrained<T>(_ p: T) -> Bool {
  return p is RP
}

// CHECK-OLD-LABEL: sil [available 12] @$s24cast_folding_reparenting17isRPUnconstrainedySbxlF
// CHECK-OLD: checked_cast_addr_br {{.*}} to any RP

// CHECK-NEW-LABEL: sil [available 12] @$s24cast_folding_reparenting17isRPUnconstrainedySbxlF
// CHECK-NEW: checked_cast_addr_br {{.*}} to any RP

// `Any` source: same as the unconstrained generic — the runtime decides.
@available(macOS 12, *)
public func isRPAny(_ p: Any) -> Bool {
  return p is RP
}

// CHECK-OLD-LABEL: sil [available 12] @$s24cast_folding_reparenting7isRPAnyySbypF
// CHECK-OLD: checked_cast_addr_br {{.*}} to any RP

// CHECK-NEW-LABEL: sil [available 12] @$s24cast_folding_reparenting7isRPAnyySbypF
// CHECK-NEW: checked_cast_addr_br {{.*}} to any RP

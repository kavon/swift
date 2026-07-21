// RUN: %target-swift-emit-silgen -enable-testing %s | %FileCheck %s

@_alwaysEmitIntoClient
internal func topLevelFunction() -> Int { 0 }
// CHECK-LABEL: sil non_abi [serialized] [export_implementation] [ossa] @$s32always_emit_into_client_testable16topLevelFunctionSiyF

internal struct InternalContext {
  @_alwaysEmitIntoClient
  func method() -> Int { 0 }
  // CHECK-LABEL: sil [export_implementation] [ossa] @$s32always_emit_into_client_testable15InternalContextV6methodSiyF

  @_alwaysEmitIntoClient
  private func privateMethod() -> Int { 0 }
  // CHECK-LABEL: sil [export_implementation] [ossa] @$s32always_emit_into_client_testable15InternalContextV13privateMethod{{.*}}LLSiyF
}

public struct PublicContext {
  @_alwaysEmitIntoClient
  public func method() -> Int { 0 }
  // CHECK-LABEL: sil non_abi [serialized] [export_implementation] [ossa] @$s32always_emit_into_client_testable13PublicContextV6methodSiyF
}

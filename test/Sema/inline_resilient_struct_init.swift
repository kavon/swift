// RUN: %target-typecheck-verify-swift -enable-library-evolution -enable-experimental-feature InlineAlways

// REQUIRES: swift_feature_InlineAlways

// rdar://174331202

public struct Resilient {
  public var i: Int

  @inline(always)
  public init(inlineAlways: Int) { // expected-error {{initializer for resilient struct 'Resilient' is '@inlinable' and must delegate (with 'self.init') or assign to 'self'}}
    self.i = inlineAlways
  }

  @inlinable
  public init(inlinable: Int) { // expected-error {{initializer for resilient struct 'Resilient' is '@inlinable' and must delegate (with 'self.init') or assign to 'self'}}
    self.i = inlinable
  }

  @_transparent
  public init(transparent: Int) { // expected-error {{initializer for resilient struct 'Resilient' is '@_transparent' and must delegate (with 'self.init') or assign to 'self'}}
    self.i = transparent
  }

  @_alwaysEmitIntoClient
  public init(aeic: Int) { // expected-error {{initializer for resilient struct 'Resilient' is '@_alwaysEmitIntoClient' and must delegate (with 'self.init') or assign to 'self'}}
    self.i = aeic
  }

  public init(normal: Int) {
    self.i = normal
  }

  @inlinable
  public init(delegating: Bool) {
    self.init(normal: 0)
  }

  @inlinable
  public init(selfAssign: Bool) {
    self = Resilient(normal: 0)
  }
}

public struct EmptyResilient {
  @inlinable
  public init() {} // expected-error {{initializer for resilient struct 'EmptyResilient' is '@inlinable' and must delegate (with 'self.init') or assign to 'self'}}
}

@frozen
public struct Frozen {
  public var i: Int

  @inlinable
  public init(i: Int) {
    self.i = i
  }

  @inline(always)
  public init(inlineAlways: Int) {
    self.i = inlineAlways
  }
}

public enum ResilientEnum {
  case a, b

  @inlinable
  public init(flag: Bool) {
    self = flag ? .a : .b
  }

  @inline(always)
  public init(otherFlag: Bool) {
    self = otherFlag ? .b : .a
  }
}

struct Internal {
  var i: Int

  @inline(always)
  init(i: Int) {
    self.i = i
  }
}

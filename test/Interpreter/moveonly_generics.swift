// RUN: %target-run-simple-swift(-Xfrontend -sil-verify-all -enable-experimental-feature NoncopyableGenerics)

// FIXME: rdar://117924596 (NoncopyableGenerics end-to-end tests fail under -O)
// XXX: %target-run-simple-swift(-O -Xfrontend -sil-verify-all -enable-experimental-feature NoncopyableGenerics)

// REQUIRES: executable_test
// REQUIRES: asserts

// FIXME: rdar://119005327 (fix issues building stdlib with NoncopyableGenerics enabled)
// REQUIRES: OS=macosx

// Needed due to limitations of autoclosures and noncopyable types.
func eagerAssert(_ b: Bool, _ line: Int = #line) {
  guard b else { fatalError("assertion failure on line \(line)") }
}

///---------------------
/// MARK: a Swift x Haskell limited-edition stdlib
/// ---------------------



/// MARK: Ord aka 'Comparable'
protocol Ord: ~Copyable {
  func compare(_ other: borrowing Self) -> Ordering
}

enum Ordering {
  case LT
  case EQ
  case GT

  static func compare<T: Comparable>(_ a: borrowing T, _ b: borrowing T) -> Ordering {
    if (a < b) { return .LT }
    if (a > b) { return .GT }
    eagerAssert(a == b)
    return .EQ
  }
}

extension Ord {
  static func <(_ a: borrowing Self, _ b: borrowing Self) -> Bool {
    return a.compare(b) == .LT
  }
  static func <=(_ a: borrowing Self, _ b: borrowing Self) -> Bool {
    return !(a > b)
  }
  static func >(_ a: borrowing Self, _ b: borrowing Self) -> Bool {
    return a.compare(b) == .GT
  }
  static func >=(_ a: borrowing Self, _ b: borrowing Self) -> Bool {
    return !(a < b)
  }
  static func ==(_ a: borrowing Self, _ b: borrowing Self) -> Bool {
    a.compare(b) == .EQ
  }
  static func !=(_ a: borrowing Self, _ b: borrowing Self) -> Bool {
    !(a == b)
  }
}

/// MARK: Maybe aka 'Optional'
enum Maybe<Val: ~Copyable>: ~Copyable {
  case just(Val)
  case none

  // NOTE: a 'consuming' version of the unfortunately 'borrowing' map from the stdlib.
  consuming func consumingMap<U: ~Copyable>(_ fn: (consuming Val) throws -> U) rethrows -> Maybe<U> {
    // rdar://117638878 (MoveOnlyAddressChecker crashes with generic associated value in enum)
    fatalError("need to fix rdar://117638878")
//     return switch consume self {
//       case let .just(val): .just(try fn(val))
//       case .none: .none
//     }
  }
}

// NOTE: temporary
extension Maybe: Copyable where Val: Copyable {}


/// MARK: beginning of tests

struct FileDescriptor: ~Copyable, Ord {
  let id: Int

  func compare(_ other: borrowing FileDescriptor) -> Ordering {
    return .compare(id, other.id)
  }

  deinit { print("calling deinit!") }
}

class Shared<T: ~Copyable> {
  let value: T
  init(_ val: consuming T) { self.value = val }
  func borrow<V>(_ f: (borrowing T) throws -> V) rethrows -> V {
    return try f(value)
  }
}

defer { testOrd() }
func testOrd() {
  let stderr = Shared(FileDescriptor(id: 1))
  let stdout = Shared(FileDescriptor(id: 0))
  stdout.borrow { out in
    stderr.borrow { err in
      eagerAssert(err > out)
      eagerAssert(out < err)
      eagerAssert(out != err)
    }
  }
}

defer { testMaybe() }
func testMaybe() {
  let mayb = Maybe.just(FileDescriptor(id: 0));
  switch consume mayb {
    case let .just(fd): eagerAssert(fd.id == 0)
    case .none: eagerAssert(false)
  }
}
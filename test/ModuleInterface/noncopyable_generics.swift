// RUN: %empty-directory(%t)

// RUN: %target-swift-frontend -swift-version 5 -enable-library-evolution -emit-module \
// RUN:     -enable-experimental-feature NoncopyableGenerics \
// RUN:     -enable-experimental-feature NonescapableTypes \
// RUN:     -o %t/NoncopyableGenerics_Misc.swiftmodule \
// RUN:     -emit-module-interface-path %t/NoncopyableGenerics_Misc.swiftinterface \
// RUN:     %S/Inputs/NoncopyableGenerics_Misc.swift

// RUN: %target-swift-frontend -swift-version 5 -enable-library-evolution -emit-module \
// RUN:     -enable-experimental-feature NoncopyableGenerics \
// RUN:     -enable-experimental-feature NonescapableTypes \
// RUN:     -o %t/Swiftskell.swiftmodule \
// RUN:     -emit-module-interface-path %t/Swiftskell.swiftinterface \
// RUN:     %S/Inputs/Swiftskell.swift

// Check the interfaces

// RUN: %FileCheck %s --check-prefix=CHECK-MISC < %t/NoncopyableGenerics_Misc.swiftinterface
// RUN: %FileCheck %s < %t/Swiftskell.swiftinterface

// See if we can compile a module through just the interface and typecheck using it.

// RUN: %target-swift-frontend -compile-module-from-interface \
// RUN:    -enable-experimental-feature NoncopyableGenerics \
// RUN:     -enable-experimental-feature NonescapableTypes \
// RUN:    %t/NoncopyableGenerics_Misc.swiftinterface -o %t/NoncopyableGenerics_Misc.swiftmodule

// RUN: %target-swift-frontend -compile-module-from-interface \
// RUN:    -enable-experimental-feature NoncopyableGenerics \
// RUN:     -enable-experimental-feature NonescapableTypes \
// RUN:    %t/Swiftskell.swiftinterface -o %t/Swiftskell.swiftmodule

// RUN: %target-swift-frontend -typecheck -I %t %s \
// RUN:    -enable-experimental-feature NoncopyableGenerics \
// RUN:    -enable-experimental-feature NonescapableTypes

// REQUIRES: asserts

import NoncopyableGenerics_Misc

// CHECK-MISC: public struct _Toys {
// CHECK-MISC: public struct rdar118697289_S1<Element> {
// CHECK-MISC: public struct rdar118697289_S2<Element> {
// CHECK-MISC: public static func allCopyable1<T>(_ a: T, _ b: T) -> T

// CHECK-MISC: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-MISC-NEXT: public static func allCopyable2<T>(_ s: T) where T : NoncopyableGenerics_Misc._NoCopyP

// CHECK-MISC: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-MISC-NEXT: public static func oneCopyable1<T, V>(_ s: T, _ v: borrowing V) where T : {{.*}}._NoCopyP, V : ~Copyable

// CHECK-MISC: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-MISC-NEXT: public static func oneCopyable2<T, V>(_ s: borrowing T, _ v: V) where T : {{.*}}._NoCopyP, T : ~Copyable

// CHECK-MISC: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-MISC-NEXT: public static func oneCopyable3<T, V>(_ s: borrowing T, _ v: V) where T : {{.*}}._NoCopyP, T : ~Copyable

// CHECK-MISC: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-MISC-NEXT: public static func basic_some(_ s: some _NoCopyP)

// CHECK-MISC: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-MISC-NEXT: public static func basic_some_nc(_ s: borrowing some _NoCopyP & ~Copyable)

// CHECK-MISC: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-MISC-NEXT: public static func oneEscapable<T, V>(_ s: T, _ v: V) where T : NoncopyableGenerics_Misc._NoEscapableP, T : ~Escapable

// CHECK-MISC: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-MISC-NEXT: public static func canEscapeButConforms<T>(_ t: T) where T : {{.*}}._NoEscapableP

// CHECK-MISC: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-MISC-NEXT: public static func opaqueNonEscapable(_ s: some _NoEscapableP & ~Escapable)

// CHECK-MISC: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-MISC-NEXT: public static func opaqueEscapable(_ s: some _NoEscapableP)

// CHECK-MISC: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-MISC-NEXT: public struct ExplicitHello<T> : ~Copyable where T : ~Copyable {

// CHECK-MISC: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-MISC-NEXT: extension {{.*}}.ExplicitHello : Swift.Copyable where T : Swift.Copyable {

// CHECK-MISC: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-MISC-NEXT: public struct Hello<T> where T : ~Copyable, T : ~Escapable {

// CHECK-MISC: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-MISC-NEXT: public protocol TestAssocTypes {
// CHECK-MISC-NEXT:   associatedtype A : {{.*}}._NoCopyP, ~Copyable

// CHECK-MISC: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-MISC-NEXT: public typealias SomeAlias<G> = {{.*}}.Hello<G>

// CHECK-MISC: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-MISC-NEXT: public typealias AliasWithInverse<G> = {{.*}}.Hello<G> where G : ~Copyable, G : ~Escapable

// CHECK-MISC: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-MISC-NEXT: public struct RudePointer<T> : Swift.Copyable where T : ~Copyable {

// CHECK-MISC: noInversesSTART
// CHECK-MISC-NOT: ~
// CHECK-MISC: noInversesEND

///////////////////////////////////////////////////////////////////////
// Synthesized conditional conformances are next

// CHECK-MISC: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-MISC-NEXT: extension {{.*}}.Hello : Swift.Copyable where T : Swift.Copyable {

////////////////////////////////////////////////////////////////////////
//    At the end, ensure there are no synthesized Copyable extensions

// CHECK-MISC-NOT: extension {{.*}}_Toys : Swift.Copyable

import Swiftskell

// CHECK: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-NEXT: public protocol Show : ~Copyable {

// CHECK: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-NEXT: public func print(_ s: borrowing some Show & ~Copyable)

// CHECK: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-NEXT: public protocol Eq : ~Copyable {

// CHECK: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-NEXT: extension Swiftskell.Eq {

// CHECK: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-NEXT: extension Swiftskell.Eq where Self : Swift.Equatable {

// CHECK: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-NEXT: public protocol Generator : ~Copyable {
// CHECK-NEXT:   associatedtype Element : ~Copyable

// CHECK: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-NEXT: public struct Vector<T> where T : ~Copyable {

// CHECK: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-NEXT: public enum Maybe<Value> where Value : ~Copyable {

// CHECK: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-NEXT: extension Swiftskell.Maybe : Swiftskell.Show {

// CHECK: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-NEXT: extension Swiftskell.Maybe : Swiftskell.Eq where Value : Swiftskell.Eq {

// CHECK: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-NEXT: public func maybe<A, B>(_ defaultVal: B, _ fn: (consuming A) -> B) -> (consuming Swiftskell.Maybe<A>) -> B where A : ~Copyable

// CHECK: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-NEXT: @inlinable public func fromMaybe<A>(_ defaultVal: A) -> (Swiftskell.Maybe<A>) -> A {

// CHECK: #if compiler(>=5.3) && $NoncopyableGenerics
// CHECK-NEXT: public func isJust<A>(_ m: borrowing Swiftskell.Maybe<A>) -> Swift.Bool where A : ~Copyable


struct FileDescriptor: ~Copyable, Eq, Show {
  let id: Int

  func show() -> String {
    return "FileDescriptor(id:\(id))"
  }

  static func ==(_ a: borrowing Self, _ b: borrowing Self) -> Bool {
    return a.id == b.id
  }
}

// RUN: %target-swift-frontend \
// RUN:   %s -module-name test \
// RUN:   -sil-verify-all -verify \
// RUN:   -emit-sil | %FileCheck %s

func exit() -> Never { fatalError() }

struct Compressor: ~Copyable {
  // CHECK-LABEL: sil hidden @$s4test10CompressorV17compressNextChunkyyF : $@convention(method) (@inout Compressor) -> () {
  // CHECK-NOT: destroy
  // CHECK: }
  mutating func compressNextChunk() {
    exit()
  }

  // CHECK-LABEL: sil hidden @$s4test10CompressorV18compressAndConsumeyyF : $@convention(method) (@owned Compressor) -> () {
  // CHECK-NOT: destroy
  // CHECK: }
  consuming func compressAndConsume() { exit() }

  // CHECK-LABEL: sil hidden @$s4test10CompressorV17compressAndBorrowyyF : $@convention(method) (@guaranteed Compressor) -> () {
  // CHECK-NOT: destroy
  // CHECK: }
  borrowing func compressAndBorrow() { exit() }

  // CHECK-LABEL: sil hidden @$s4test10CompressorV9maybeExityySbF : $@convention(method) (Bool, @inout Compressor) -> () {
  // CHECK-NOT: destroy
  // CHECK: }
  mutating func maybeExit(_ b: Bool) {
    if b {
      exit()
    }
  }

  deinit { print("hi") }
}

// CHECK-LABEL: sil hidden @$s4test9takeInoutyyAA10CompressorVz_ADztF : $@convention(thin) (@inout Compressor, @inout Compressor) -> () {
// CHECK-NOT: destroy
// CHECK: }
func takeInout(_ comp1: inout Compressor,
               _ comp2: inout Compressor) { exit() }

// CHECK-LABEL: sil hidden @$s4test13takeConsumingyyAA10CompressorVnF : $@convention(thin) (@owned Compressor) -> () {
// CHECK-NOT: destroy
// CHECK: }
func takeConsuming(_ comp: consuming Compressor) { exit() }

// CHECK-LABEL: sil hidden @$s4test13takeBorrowingyyAA10CompressorVF : $@convention(thin) (@guaranteed Compressor) -> () {
// CHECK-NOT: destroy
// CHECK: }
func takeBorrowing(_ comp: borrowing Compressor) { exit() }

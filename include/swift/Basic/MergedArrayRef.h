//===- swift/Basic/MergedArrayRef.h - A merger of two arrays ----*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2023 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file defines the MergedArrayRef, a data structure that represents a
// merged view of two sorted ArrayRef's, without moving or copying the elements
// of the original arrays. The merger happens via a user-provided comparison
// function. Each individual array must already be sorted by that same
// comparison function.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_INCLUDE_SWIFT_BASIC_MERGEDARRAYREF_H
#define SWIFT_INCLUDE_SWIFT_BASIC_MERGEDARRAYREF_H

#include <assert.h>
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include <functional>

namespace swift {

template<typename Element, typename LessOrEqFunctor = std::less_equal<>>
struct MergedArrayRef {
private:
  // The core part of the merged array representation. We store just one bit
  // that indicates whether element i resides in the "left" array (bit = 1)
  // or the "right" array (bit = 0).
  llvm::SmallBitVector pickLeft;

  /// Used to select metadata about one of the "sides" of this merged array.
  enum Side {
    RIGHT = 0,
    LEFT = 1
  };

  // The individual sub-arrays themselves, indexed via the Side enum.
  llvm::ArrayRef<Element> arr[2];

  template<typename From>
  static llvm::ArrayRef<Element> upcast(llvm::ArrayRef<From> from) {
    return llvm::ArrayRef<Element>(from.data(), from.size());
  }

public:
  /// MARK: initialization

  /// the empty array
  MergedArrayRef() {}

  /// Creates a merged array using the given two arrays and a <= function.
  MergedArrayRef(llvm::ArrayRef<Element> left, llvm::ArrayRef<Element> right,
                 LessOrEqFunctor lessOrEq = LessOrEqFunctor{}) {
    init(left, right, lessOrEq);
  }

  /// Creates a merged array using the given two arrays and a <= function.
  template<typename Ty1 = Element, typename Ty2 = Element>
  MergedArrayRef(llvm::ArrayRef<Ty1> left, llvm::ArrayRef<Ty2> right,
                 LessOrEqFunctor lessOrEq = LessOrEqFunctor{}) {
    init(upcast(left), upcast(right), lessOrEq);
  }

  /// Changes the "left" array. Invalidates all existing iterators.
  template<typename Ty = Element>
  void setLeft(llvm::ArrayRef<Ty> left,
               LessOrEqFunctor lessOrEq = LessOrEqFunctor{}) {
    init(upcast(left), arr[RIGHT], lessOrEq);
  }

  /// Changes the "right" array. Invalidates all existing iterators.
  template<typename Ty = Element>
  void setRight(llvm::ArrayRef<Ty> right,
                LessOrEqFunctor lessOrEq = LessOrEqFunctor{}) {
    init(arr[LEFT], upcast(right), lessOrEq);
  }

  /// Removes all elements from this merged array.
  /// Does not mutate the contents of the underlying subarrays.
  void clear() {
    arr[LEFT] = llvm::ArrayRef<Element>();
    arr[RIGHT] = llvm::ArrayRef<Element>();
    pickLeft.clear();
    assert(size() == 0);
  }

  /// MARK: iteration

  struct const_iterator {
  private:
    friend MergedArrayRef;
    MergedArrayRef<Element> const *merged;

    // FIXME: we can't keep a const pointer to the merged array ref, because
    //  people might be throwing away or reconstructing this thing on-the-fly
    //  and perhaps even comparing iterators from different instances of the
    //  same fundamental MergedArrayRef. For example, this needs to work:
    //    std::for_each(getMergedArray().begin(), getMergedArray().end(), ...)
    //  where we have getMergedArray() { return MergedArrayRef(arr1, arr2); }

    // These cursors remember how far we've advanced into the
    // left or right array, respectively. Indexed via the Side enum.
    size_t cursor[2];

    // The current index into the merged array, i.e., the bit-vector.
    inline size_t currentIndex() const { return cursor[LEFT] + cursor[RIGHT]; }

    // Modify this iterator so it represents the next element, or if there is
    // no other element, to the "end" representation of this iterator.
    void advance() {
      const auto current = currentIndex();

      // are we at the end?
      if (current == merged->pickLeft.size())
        return;

      // otherwise, advance.
      auto side = merged->pickLeft[current] ? LEFT : RIGHT;
      cursor[side]++;
    }

    /// Determines whether the current iterator is pointing to a specific
    /// index of a side of the merged arrays. An iterator is only ever looking
    /// at one side at a time.
    bool lookingAt(Side side, size_t index) const {
      // not looking at the same side.
      if (merged->pickLeft[currentIndex()] != side)
        return false;

      // must be looking at same index on the side
      return cursor[side] == index;
    }

    const_iterator(MergedArrayRef<Element> const *mergedArray,
                   size_t leftStart,
                   size_t rightStart) : merged(mergedArray) {
      assert(leftStart + rightStart <= merged->pickLeft.size());
      cursor[LEFT] = leftStart;
      cursor[RIGHT] = rightStart;
    }

    static const_iterator makeBegin(MergedArrayRef<Element> const *merged) {
      return const_iterator(merged, 0, 0);
    }

    static const_iterator makeEnd(MergedArrayRef<Element> const *merged) {
      return const_iterator(merged,
                            merged->arr[LEFT].size(),
                            merged->arr[RIGHT].size());
    }

    static const_iterator makeRBegin(MergedArrayRef<Element> const *merged) {
      if (merged->empty())
        llvm_unreachable("back() of empty array.");

      auto iter = makeEnd(merged);
      assert(iter.currentIndex() == merged->pickLeft.size());

      // this is basically one round of a future impl of operator-- to make this
      // a bidirectional iterator.
      auto current = iter.currentIndex();
      auto side = merged->pickLeft[current - 1] ? LEFT : RIGHT;
      iter.cursor[side]--;

      return iter;
    }

  public:
    using difference_type = std::ptrdiff_t;
    using value_type = Element;
    using pointer = Element *;
    using reference = Element &;
    using iterator_category = std::forward_iterator_tag;

    // Prefix increment operator.
    const_iterator &operator++() {
      advance();
      return *this;
    }

    // Postfix increment operator.
    const_iterator operator++(int) {
      const_iterator copy = *this;
      ++*this;
      return copy;
    }

    Element const &operator*() const {
      auto side = merged->pickLeft[currentIndex()] ? LEFT : RIGHT;
      auto index = cursor[side];
      return merged->arr[side][index];
    }

    bool operator==(const const_iterator &other) const {
      assert(same(merged->arr[LEFT], other.merged->arr[LEFT])
                 && same(merged->arr[RIGHT], other.merged->arr[RIGHT])
                 && "comparing iterators from different merged arrays");
      return cursor[LEFT] == other.cursor[LEFT]
          && cursor[RIGHT] == other.cursor[RIGHT];
    }
    bool operator!=(const const_iterator &other) const {
      return !(*this == other);
    }
  };

  const_iterator begin() const { return const_iterator::makeBegin(this); }
  const_iterator end() const { return const_iterator::makeEnd(this); }

  Element const &front() const {
    assert(!empty());
    return *(begin());
  }

  Element const &back() const {
    assert(!empty());
    return *(const_iterator::makeRBegin(this));
  }

  /// The number of elements in this merged array.
  size_t size() const { return pickLeft.size(); }

  /// Is this array empty?
  bool empty() const { return pickLeft.empty(); }

  /// MARK: indexing

  /// Time Complexity: O(size())
  Element const &operator[](size_t i) const {
    return *(at(i));
  }

  /// Returns an iterator at the given index of this merged array.
  ///
  /// Time Complexity: O(size())
  const_iterator at(size_t index) const {
    // NOTE: This could become more efficient if you have a O(1) std::popcount.
    // First, you apply a mask to select only bits 0..<index of \c pickLeft.
    // Then you set the iterator's leftIdx to the std::popcount of those bits
    // and its rightIdx to `index - popcount` (maybe -1 ?). This gets tricky
    // to implement for larger bit vectors, etc, so I didn't pursue.
    if (index >= size())
      llvm_unreachable("index out of bounds");

    auto iter = begin();
    for (size_t i = 0; i < index; ++i)
      ++iter;
    return iter;
  }

  /// Returns an iterator into this merged array that lies within the given
  /// \c subArray at that subArray's \c index. For example,
  ///
  ///   let left = {1, 3}
  ///   let right = {2}
  ///   let merged = MergedArrayRef(left, right, <)
  ///   let iter = merged.within(right, 0)
  ///   assert(*iter == 2)
  ///   assert(*(iter+1) == 3)
  ///
  /// here we ask for an iterator within the `right` ArrayRef, and provide index
  /// 0 to select the first element of `right`. The returned iterator is
  /// pointing at the two in the merged array {1, 2, 3}.
  ///
  /// Time Complexity: O(size())
  ///
  /// \param subArray An ArrayRef equal to either the left or right array
  /// represented by this MergedArrayRef.
  /// \param index An index of \c subArray.
  /// \return an iterator into this MergedArrayRef.
  const_iterator within(llvm::ArrayRef<Element> subArray, size_t index) const {
    // hard error if you provide an unknown array.
    if (!same(subArray, arr[LEFT]) && !same(subArray, arr[RIGHT]))
      llvm_unreachable("cannot index into unknown array!");

    const Side side = subArray == arr[LEFT] ? LEFT : RIGHT;
    if (index >= arr[side].size())
      llvm_unreachable("index out of bounds");

    const auto iterEnd = end();
    for (auto iter = begin(); iter != iterEnd; ++iter) {
      if (iter.lookingAt(side, index))
        return iter;
    }

    llvm_unreachable("did not find element?");
  }

  bool operator==(const MergedArrayRef &other) const {
    return pickLeft == other.pickLeft && arr[LEFT] == other.arr[LEFT]
        && arr[RIGHT] == other.arr[RIGHT];
  }
  bool operator!=(const MergedArrayRef &other) const {
    return !(*this == other);
  }

private:
  /// MARK: implementation details

  /// Takes two arrays individually sorted by the \c lessOrEq functor to
  /// initialize this merged array.
  void init(llvm::ArrayRef<Element> left, llvm::ArrayRef<Element> right,
            LessOrEqFunctor lessOrEq) {
    assert(isSorted(left, lessOrEq));
    assert(isSorted(right, lessOrEq));

    pickLeft.clear();
    pickLeft.resize(left.size() + right.size(), /*chooseLeft=*/false);
    arr[LEFT] = left;
    arr[RIGHT] = right;

    // Simulate the merger and store the results in whichArray
    size_t leftIdx = 0, rightIdx = 0, mergeIdx = 0;

    while (leftIdx < arr[LEFT].size() && rightIdx < arr[RIGHT].size()) {
      // each bit represents whether we chose the left element during this
      // comparison.
      bool chooseLeft = lessOrEq(arr[LEFT][leftIdx], arr[RIGHT][rightIdx]);

      pickLeft[mergeIdx++] = chooseLeft;

      if (chooseLeft)
        ++leftIdx;
      else
        ++rightIdx;
    }

    // Handle the remaining tail of the "left" array. Since the BitVector starts
    // with all bits cleared to zero, we only need to set the tail of the vector
    // if there's remaining "left" elements.
    if (leftIdx < arr[LEFT].size()) {
      // ensure the same number of elements that remain in the left array
      // matches the number of bits remaining in the bit-vector.
      assert(arr[LEFT].size() - leftIdx == pickLeft.size() - mergeIdx);

      pickLeft.set(mergeIdx, pickLeft.size());
    }
  }

  /// Checks if the two ArrayRefs are pointing to the same data.
  static bool same(llvm::ArrayRef<Element> a, llvm::ArrayRef<Element> b) {
    return a.data() == b.data() && a.size() == b.size();
  }

#ifndef NDEBUG
  /// Checks whether the array is sorted by the given lessOrEq function.
  template<typename Ty>
  static bool isSorted(llvm::ArrayRef<Ty> arr, LessOrEqFunctor lessOrEq) {
    size_t elm = 1;
    while (elm < arr.size()) {
      if (!lessOrEq(arr[elm - 1], arr[elm]))
        return false;
      ++elm;
    }
    return true;
  }
#endif
};

} // namespace swift

#endif //SWIFT_INCLUDE_SWIFT_BASIC_MERGEDARRAYREF_H

//===--- MergedArrayRefTest.cpp ----------------------------------------------===//
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

#include "swift/Basic/MergedArrayRef.h"
#include "gtest/gtest.h"
#include <algorithm>

using namespace swift;

template<typename Element>
void expectEqualIter(MergedArrayRef<Element> const &merged,
                     std::initializer_list<Element> expected) {
  auto expectI = expected.begin(), expectE = expected.end();
  for (Element value : merged) {
    EXPECT_FALSE(expectI == expectE);
    EXPECT_EQ(*expectI, value);
    ++expectI;
  }
  EXPECT_TRUE(expectI == expectE);
}

template<typename Element>
void expectEqualIter(MergedArrayRef<Element> const &one,
                     MergedArrayRef<Element> const &two) {
  auto twoI = two.begin(), twoE = two.end();
  for (Element value : one) {
    EXPECT_FALSE(twoI == twoE);
    EXPECT_EQ(*twoI, value);
    ++twoI;
  }
  EXPECT_TRUE(twoI == twoE);
}

template<typename Element>
void expectEqualIter(MergedArrayRef<Element> const &one,
                     llvm::ArrayRef<Element> const &two) {
  auto twoI = two.begin(), twoE = two.end();
  for (Element value : one) {
    EXPECT_FALSE(twoI == twoE);
    EXPECT_EQ(*twoI, value);
    ++twoI;
  }
  EXPECT_TRUE(twoI == twoE);
}

TEST(MergedArrayRef, merging_tests) {
  MergedArrayRef<int> empty;
  MergedArrayRef<int> merged;
  EXPECT_EQ(empty, merged);
  expectEqualIter(empty, merged);

  {
    // simple merging scenarios
    auto left = {1, 3, 4};
    auto right = {2};
    merged = MergedArrayRef<int>(left, right);
    expectEqualIter(merged, {1, 2, 3, 4});

    // check that I can be devious and mutate an array element.
    auto &elm = const_cast<int &>(merged[1]);
    EXPECT_EQ(*right.begin(), 2);
    elm = 1337;
    EXPECT_EQ(*right.begin(), 1337);
    expectEqualIter(merged, {1, 1337, 3, 4});

    llvm::ArrayRef<int> newRight = {-2, -1, 0, 1, 2, 3, 4, 5};
    merged.setRight(newRight);
    expectEqualIter(merged, {-2, -1, 0, 1, 1, 2, 3, 3, 4, 4, 5});
    expectEqualIter(merged, MergedArrayRef<int>(newRight, left));
  }

  {
    // ensure we can handle duplicates and merging of the same array into itself
    llvm::ArrayRef<int> dupy = {1, 1, 2, 2};
    merged.setLeft(dupy);
    merged.setRight(dupy);
    expectEqualIter(merged, {1, 1, 1, 1, 2, 2, 2, 2});

    // ensure we reach the empty element by setting left and right to empty
    merged.setLeft({});
    expectEqualIter(merged, dupy);

    merged.setRight({});
    expectEqualIter(merged, {});
    EXPECT_EQ(merged, MergedArrayRef<int>());
  }

  {
    // ensure that equality means element-wise equality testing and not pointers
    auto one = {1, 10, 100};
    auto another = {1, 10, 100};
    auto fives = {5, 50};
    MergedArrayRef<int> first(one, fives);
    MergedArrayRef<int> second(another, fives);
    expectEqualIter(first, second);
    EXPECT_EQ(first, second);

    // do basic size, clear, and subscript testing
    EXPECT_EQ(first.size(), 5);
    EXPECT_EQ(first[0], 1);
    EXPECT_EQ(first[1], 5);
    EXPECT_EQ(first[2], 10);
    EXPECT_EQ(first[3], 50);
    EXPECT_EQ(first[4], 100);

    first.clear();
    EXPECT_EQ(first.size(), 0);
    EXPECT_NE(first, second);

    first.setRight({1337});
    EXPECT_EQ(first.size(), 1);
    EXPECT_EQ(first[0], 1337);
  }
}

TEST(MergedArrayRef, within_tests) {
  llvm::ArrayRef<int> evens = {22, 44};
  llvm::ArrayRef<int> odds = {11, 33};
  MergedArrayRef<int> merged(evens, odds);

  { // get an iterator starting within the first element of the evens array
    auto first = merged.within(odds, 0);
    EXPECT_EQ(first, merged.begin());
    EXPECT_EQ(*first, 11);
    EXPECT_EQ(*(++first), 22);

    auto iter = merged.within(evens, 0);
    EXPECT_EQ(*iter, 22);
    EXPECT_EQ(iter, merged.at(1));
    EXPECT_EQ(iter, first);

    EXPECT_EQ(*(++iter), 33);
    EXPECT_EQ(iter, merged.at(2));
    EXPECT_EQ(iter, merged.within(odds, 1));

    EXPECT_EQ(*(++iter), 44);
    EXPECT_EQ(iter, merged.at(3));
    EXPECT_EQ(iter, merged.within(evens, 1));

    EXPECT_EQ(++iter, merged.end());
  }

  { // check that within adapts upon changing a sub-array
    merged.setRight({});
    auto iter = merged.within(evens, 0);
    EXPECT_EQ(*iter, 22);
    EXPECT_EQ(iter, merged.at(0));
    EXPECT_EQ(iter, merged.begin());

    EXPECT_EQ(*(++iter), 44);
    EXPECT_EQ(iter, merged.at(1));
    EXPECT_NE(iter, merged.end());

    EXPECT_EQ(++iter, merged.end());
  }
}

TEST(MergedArrayRef, advanced_tests) {
  llvm::ArrayRef<int> left = {2, 3, 7};
  llvm::ArrayRef<int> right = {1, 3, 4, 5, 6};
  MergedArrayRef<int> merged(left, right);

  EXPECT_FALSE(merged.empty());
  EXPECT_EQ(merged.back(), 7);
  EXPECT_EQ(merged.front(), 1);

  EXPECT_TRUE(std::any_of(merged.begin(), merged.end(), [](auto const &elm) {
    return elm == 3;
  }));

  EXPECT_TRUE(std::all_of(merged.begin(), merged.end(), [](auto const &elm) {
    return elm < 10;
  }));

  {
    auto firstDupe = std::adjacent_find(merged.begin(), merged.end());
    EXPECT_EQ(*firstDupe, 3);
    EXPECT_EQ(*(++firstDupe), 3);
    auto nextDupe = std::adjacent_find(firstDupe, merged.end());
    EXPECT_EQ(nextDupe, merged.end());
  }

  {
    auto iter = merged.begin();
    std::for_each(merged.begin(), merged.end(), [&](const auto &elm) {
      EXPECT_EQ(*iter, elm);
      iter++;
    });
  }
}

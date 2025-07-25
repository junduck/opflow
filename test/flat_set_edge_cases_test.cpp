#include <algorithm>
#include <gtest/gtest.h>
#include <iterator>
#include <vector>

#include "opflow/impl/flat_set.hpp"

using namespace opflow::impl;

class FlatSetEdgeCasesTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

// Test critical edge cases that could expose bugs
TEST_F(FlatSetEdgeCasesTest, FindReturnsCorrectIteratorWhenNotFound) {
  flat_set<int> fs;
  fs.insert(10);
  fs.insert(20);
  fs.insert(30);

  auto it = fs.find(99); // Non-existent element

  // The current implementation incorrectly returns an iterator past the end
  // but not equal to end() when element is not found
  EXPECT_EQ(it, fs.end()) << "find() should return end() when element not found";
}

TEST_F(FlatSetEdgeCasesTest, ConstFindReturnsCorrectIteratorWhenNotFound) {
  flat_set<int> fs;
  fs.insert(10);
  fs.insert(20);
  fs.insert(30);

  const auto &cfs = fs;
  auto cit = cfs.find(99); // Non-existent element

  EXPECT_EQ(cit, cfs.end()) << "const find() should return end() when element not found";
}

TEST_F(FlatSetEdgeCasesTest, EmplaceLogicWithComplexDuplicatePattern) {
  flat_set<int> fs;

  // Test the emplace logic carefully
  size_t idx1 = fs.emplace(10);
  EXPECT_EQ(idx1, 0);
  EXPECT_EQ(fs.size(), 1);

  // Add different element
  size_t idx2 = fs.emplace(20);
  EXPECT_EQ(idx2, 1);
  EXPECT_EQ(fs.size(), 2);

  // Try to add duplicate of first element
  size_t idx3 = fs.emplace(10);
  EXPECT_EQ(idx3, 0);      // Should return index of existing element
  EXPECT_EQ(fs.size(), 2); // Size should not change

  // Verify the container state
  EXPECT_EQ(fs[0], 10);
  EXPECT_EQ(fs[1], 20);
}

TEST_F(FlatSetEdgeCasesTest, EmptyContainerEdgeCases) {
  flat_set<int> fs;

  // Test all operations on empty container
  EXPECT_TRUE(fs.empty());
  EXPECT_EQ(fs.size(), 0);
  EXPECT_EQ(fs.begin(), fs.end());
  EXPECT_EQ(fs.cbegin(), fs.cend());
  EXPECT_EQ(fs.rbegin(), fs.rend());
  EXPECT_EQ(fs.crbegin(), fs.crend());

  // Test find on empty container
  auto it = fs.find(42);
  EXPECT_EQ(it, fs.end());

  // Test contains on empty container
  EXPECT_FALSE(fs.contains(42));

  // Test erase on empty container
  auto erase_it = fs.erase(42);
  EXPECT_EQ(erase_it, fs.end());
  EXPECT_TRUE(fs.empty());
}

TEST_F(FlatSetEdgeCasesTest, SingleElementEdgeCases) {
  flat_set<int> fs;
  fs.insert(42);

  // Test iterator relationships
  EXPECT_NE(fs.begin(), fs.end());
  EXPECT_EQ(fs.begin() + 1, fs.end());
  EXPECT_EQ(fs.end() - 1, fs.begin());

  // Test reverse iterators
  EXPECT_NE(fs.rbegin(), fs.rend());
  EXPECT_EQ(fs.rbegin() + 1, fs.rend());

  // Test that begin points to the element
  EXPECT_EQ(*fs.begin(), 42);
  EXPECT_EQ(*fs.rbegin(), 42);
}

TEST_F(FlatSetEdgeCasesTest, IteratorArithmetic) {
  flat_set<int> fs;
  for (int i = 0; i < 5; ++i) {
    fs.insert(i * 10);
  }

  auto it = fs.begin();
  EXPECT_EQ(*it, 0);

  ++it;
  EXPECT_EQ(*it, 10);

  it += 2;
  EXPECT_EQ(*it, 30);

  it -= 1;
  EXPECT_EQ(*it, 20);

  auto it2 = it + 2;
  EXPECT_EQ(*it2, 40);

  auto it3 = it2 - 1;
  EXPECT_EQ(*it3, 30);
}

TEST_F(FlatSetEdgeCasesTest, EraseAtBoundaries) {
  flat_set<int> fs;
  fs.insert(10);
  fs.insert(20);
  fs.insert(30);
  fs.insert(40);
  fs.insert(50);

  // Erase first element
  auto it1 = fs.erase(fs.begin());
  EXPECT_EQ(fs.size(), 4);
  EXPECT_EQ(*it1, 20); // Should point to new first element
  EXPECT_EQ(fs[0], 20);

  // Erase last element
  auto last_it = fs.end() - 1;
  auto it2 = fs.erase(last_it);
  EXPECT_EQ(fs.size(), 3);
  EXPECT_EQ(it2, fs.end()); // Should return end()

  // Verify remaining elements
  EXPECT_EQ(fs[0], 20);
  EXPECT_EQ(fs[1], 30);
  EXPECT_EQ(fs[2], 40);
}

TEST_F(FlatSetEdgeCasesTest, ExtractAfterModifications) {
  flat_set<int> fs;
  fs.insert(1);
  fs.insert(2);
  fs.insert(3);
  fs.insert(2); // duplicate
  fs.erase(1);

  auto container = fs.extract();
  EXPECT_EQ(container.size(), 2);
  EXPECT_EQ(container[0], 2);
  EXPECT_EQ(container[1], 3);

  // Original should be empty
  EXPECT_TRUE(fs.empty());
}

TEST_F(FlatSetEdgeCasesTest, SwapWithDifferentSizes) {
  flat_set<int> fs1;
  fs1.insert(100);

  flat_set<int> fs2;
  for (int i = 0; i < 10; ++i) {
    fs2.insert(i);
  }

  swap(fs1, fs2);

  EXPECT_EQ(fs1.size(), 10);
  EXPECT_EQ(fs2.size(), 1);
  EXPECT_EQ(fs2[0], 100);

  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(fs1[static_cast<size_t>(i)], i);
  }
}

TEST_F(FlatSetEdgeCasesTest, SwapWithEmptySet) {
  flat_set<int> fs1;
  fs1.insert(1);
  fs1.insert(2);
  fs1.insert(3);

  flat_set<int> fs2; // empty

  swap(fs1, fs2);

  EXPECT_TRUE(fs1.empty());
  EXPECT_EQ(fs2.size(), 3);
  EXPECT_EQ(fs2[0], 1);
  EXPECT_EQ(fs2[1], 2);
  EXPECT_EQ(fs2[2], 3);
}

TEST_F(FlatSetEdgeCasesTest, ComparisonWithEmptySets) {
  flat_set<int> fs1;
  flat_set<int> fs2;

  EXPECT_EQ(fs1, fs2);
  EXPECT_FALSE(fs1 != fs2);
  EXPECT_FALSE(fs1 < fs2);
  EXPECT_FALSE(fs1 > fs2);
  EXPECT_TRUE(fs1 <= fs2);
  EXPECT_TRUE(fs1 >= fs2);
}

TEST_F(FlatSetEdgeCasesTest, ComparisonEmptyVsNonEmpty) {
  flat_set<int> empty_fs;
  flat_set<int> non_empty_fs;
  non_empty_fs.insert(1);

  EXPECT_NE(empty_fs, non_empty_fs);
  EXPECT_LT(empty_fs, non_empty_fs);
  EXPECT_GT(non_empty_fs, empty_fs);
}

// Test memory and performance edge cases
TEST_F(FlatSetEdgeCasesTest, ManyDuplicateInsertions) {
  flat_set<int> fs;

  // Insert the same element many times
  for (int i = 0; i < 1000; ++i) {
    size_t idx = fs.insert(42);
    EXPECT_EQ(idx, 0);       // Should always return 0
    EXPECT_EQ(fs.size(), 1); // Size should remain 1
  }

  EXPECT_EQ(fs[0], 42);
}

TEST_F(FlatSetEdgeCasesTest, AlternatingInsertErase) {
  flat_set<int> fs;

  // Pattern: insert, erase, insert, erase
  for (int i = 0; i < 100; ++i) {
    fs.insert(i);
    if (i > 0) {
      fs.erase(i - 1);
    }
  }

  // Should only have the last element
  EXPECT_EQ(fs.size(), 1);
  EXPECT_EQ(fs[0], 99);
}

TEST_F(FlatSetEdgeCasesTest, StressTestWithRandomOperations) {
  flat_set<int> fs;
  std::vector<int> reference;

  // Simulate a series of operations
  for (int i = 0; i < 50; ++i) {
    fs.insert(i % 10); // Will create some duplicates

    // Build reference without duplicates, preserving order
    if (std::find(reference.begin(), reference.end(), i % 10) == reference.end()) {
      reference.push_back(i % 10);
    }
  }

  // Verify size matches reference
  EXPECT_EQ(fs.size(), reference.size());

  // Verify all elements match
  for (size_t i = 0; i < reference.size(); ++i) {
    EXPECT_EQ(fs[i], reference[i]);
  }
}

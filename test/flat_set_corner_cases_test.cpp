#include <deque>
#include <gtest/gtest.h>
#include <vector>

#include "opflow/impl/flat_set.hpp"

using namespace opflow::impl;

class FlatSetCornerCasesTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

// This test verifies the subtle emplace behavior more thoroughly
TEST_F(FlatSetCornerCasesTest, EmplaceWithIdenticalObjectsButDifferentInstances) {
  flat_set<std::string> fs;

  std::string str1 = "test";
  std::string str2 = "test"; // Same content, different object

  size_t idx1 = fs.emplace(str1);
  EXPECT_EQ(idx1, 0);
  EXPECT_EQ(fs.size(), 1);

  size_t idx2 = fs.emplace(str2); // Should be treated as duplicate
  EXPECT_EQ(idx2, 0);             // Should return index of existing
  EXPECT_EQ(fs.size(), 1);        // Size should not change
}

// Test the exact logic of the emplace method
TEST_F(FlatSetCornerCasesTest, EmplaceInternalLogic) {
  flat_set<int> fs;

  // First insertion
  size_t idx1 = fs.emplace(100);
  EXPECT_EQ(idx1, 0);
  EXPECT_EQ(fs.size(), 1);
  EXPECT_EQ(fs[0], 100);

  // Second insertion (different value)
  size_t idx2 = fs.emplace(200);
  EXPECT_EQ(idx2, 1);
  EXPECT_EQ(fs.size(), 2);
  EXPECT_EQ(fs[0], 100);
  EXPECT_EQ(fs[1], 200);

  // Third insertion (duplicate of first)
  size_t idx3 = fs.emplace(100);
  EXPECT_EQ(idx3, 0);      // Should return index of existing element
  EXPECT_EQ(fs.size(), 2); // Size should remain the same
  EXPECT_EQ(fs[0], 100);
  EXPECT_EQ(fs[1], 200);

  // Fourth insertion (duplicate of second)
  size_t idx4 = fs.emplace(200);
  EXPECT_EQ(idx4, 1);      // Should return index of existing element
  EXPECT_EQ(fs.size(), 2); // Size should remain the same
  EXPECT_EQ(fs[0], 100);
  EXPECT_EQ(fs[1], 200);
}

// Test that order is preserved properly even with duplicates
TEST_F(FlatSetCornerCasesTest, OrderPreservationWithDuplicates) {
  flat_set<int> fs;

  // Insert in specific order
  fs.insert(50);
  fs.insert(10);
  fs.insert(30);
  fs.insert(10); // duplicate
  fs.insert(20);
  fs.insert(50); // duplicate

  // Order should be: 50, 10, 30, 20 (duplicates ignored)
  EXPECT_EQ(fs.size(), 4);
  EXPECT_EQ(fs[0], 50);
  EXPECT_EQ(fs[1], 10);
  EXPECT_EQ(fs[2], 30);
  EXPECT_EQ(fs[3], 20);
}

// Test iterator stability and correctness
TEST_F(FlatSetCornerCasesTest, IteratorConsistency) {
  flat_set<int> fs;
  fs.insert(1);
  fs.insert(2);
  fs.insert(3);

  auto it1 = fs.find(2);
  auto it2 = fs.begin() + 1;

  EXPECT_EQ(it1, it2);
  EXPECT_EQ(*it1, *it2);
  EXPECT_EQ(*it1, 2);
}

// Test that erasing updates subsequent iterators correctly
TEST_F(FlatSetCornerCasesTest, EraseAndIteratorUpdates) {
  flat_set<int> fs;
  fs.insert(10);
  fs.insert(20);
  fs.insert(30);
  fs.insert(40);

  // Get iterator to element 30
  auto it_30 = fs.find(30);
  EXPECT_EQ(*it_30, 30);

  // Erase element 20 (before 30)
  fs.erase(20);

  // Now element 30 should be at a different position
  auto new_it_30 = fs.find(30);
  EXPECT_EQ(*new_it_30, 30);
  EXPECT_EQ(new_it_30, fs.begin() + 1); // Should now be at index 1
}

// Test container extraction preserves order
TEST_F(FlatSetCornerCasesTest, ExtractPreservesOrder) {
  flat_set<int> fs;

  // Insert in non-sorted order
  std::vector<int> insertion_order = {5, 1, 8, 3, 9, 2};
  for (int val : insertion_order) {
    fs.insert(val);
  }

  auto extracted = fs.extract();

  // Should preserve insertion order
  EXPECT_EQ(extracted.size(), insertion_order.size());
  for (size_t i = 0; i < insertion_order.size(); ++i) {
    EXPECT_EQ(extracted[i], insertion_order[i]);
  }
}

// Test that the container type parameter works correctly
TEST_F(FlatSetCornerCasesTest, CustomContainerType) {
  // Test with deque instead of vector
  using custom_flat_set = flat_set<int, std::deque<int>>;
  custom_flat_set fs;

  fs.insert(10);
  fs.insert(20);
  fs.insert(10); // duplicate

  EXPECT_EQ(fs.size(), 2);
  EXPECT_EQ(fs[0], 10);
  EXPECT_EQ(fs[1], 20);

  auto extracted = fs.extract();
  EXPECT_EQ(extracted.size(), 2);
}

// Test move semantics work correctly
TEST_F(FlatSetCornerCasesTest, MoveSemantics) {
  flat_set<std::string> fs;

  std::string movable = "will_be_moved";
  std::string copy_of_movable = movable;

  fs.insert(std::move(movable));
  EXPECT_EQ(fs[0], copy_of_movable);
  // movable might be empty after move (implementation-dependent)
}

// Test perfect forwarding in emplace
TEST_F(FlatSetCornerCasesTest, EmplacePerfectForwarding) {
  flat_set<std::string> fs;

  // Test emplace with string literal
  fs.emplace("literal");
  EXPECT_EQ(fs[0], "literal");

  // Test emplace with temporary string
  fs.emplace(std::string("temporary"));
  EXPECT_EQ(fs[1], "temporary");

  // Test emplace with lvalue
  std::string lvalue = "lvalue";
  fs.emplace(lvalue);
  EXPECT_EQ(fs[2], "lvalue");
}

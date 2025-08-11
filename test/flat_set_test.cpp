#include <algorithm>
#include <gtest/gtest.h>
#include <iterator>
#include <string>
#include <vector>

#include "opflow/detail/flat_set.hpp"

using namespace opflow::detail;

class FlatSetTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

// Test basic construction and capacity
TEST_F(FlatSetTest, DefaultConstruction) {
  flat_set<int> fs;
  EXPECT_TRUE(fs.empty());
  EXPECT_EQ(fs.size(), 0);
  EXPECT_GT(fs.max_size(), 0);
}

TEST_F(FlatSetTest, EmptySetProperties) {
  flat_set<int> fs;
  EXPECT_EQ(fs.begin(), fs.end());
  EXPECT_EQ(fs.cbegin(), fs.cend());
  EXPECT_EQ(fs.rbegin(), fs.rend());
  EXPECT_EQ(fs.crbegin(), fs.crend());
}

// Test insertion and emplace
TEST_F(FlatSetTest, BasicInsertion) {
  flat_set<int> fs;

  // Test first insertion
  size_t idx1 = fs.insert(42);
  EXPECT_EQ(idx1, 0);
  EXPECT_EQ(fs.size(), 1);
  EXPECT_FALSE(fs.empty());
  EXPECT_EQ(fs[0], 42);
}

TEST_F(FlatSetTest, MultipleInsertions) {
  flat_set<int> fs;

  size_t idx1 = fs.insert(10);
  size_t idx2 = fs.insert(20);
  size_t idx3 = fs.insert(30);

  EXPECT_EQ(idx1, 0);
  EXPECT_EQ(idx2, 1);
  EXPECT_EQ(idx3, 2);
  EXPECT_EQ(fs.size(), 3);

  EXPECT_EQ(fs[0], 10);
  EXPECT_EQ(fs[1], 20);
  EXPECT_EQ(fs[2], 30);
}

TEST_F(FlatSetTest, DuplicateInsertion) {
  flat_set<int> fs;

  size_t idx1 = fs.insert(42);
  size_t idx2 = fs.insert(42); // Duplicate

  EXPECT_EQ(idx1, 0);
  EXPECT_EQ(idx2, 0);      // Should return index of existing element
  EXPECT_EQ(fs.size(), 1); // Size should not increase
  EXPECT_EQ(fs[0], 42);
}

TEST_F(FlatSetTest, DuplicateInsertionWithOtherElements) {
  flat_set<int> fs;

  fs.insert(10);
  fs.insert(20);
  fs.insert(30);

  size_t idx = fs.insert(20); // Duplicate of middle element
  EXPECT_EQ(idx, 1);          // Should return index of existing element
  EXPECT_EQ(fs.size(), 3);    // Size should not change

  // Verify order preserved
  EXPECT_EQ(fs[0], 10);
  EXPECT_EQ(fs[1], 20);
  EXPECT_EQ(fs[2], 30);
}

TEST_F(FlatSetTest, EmplaceForwarding) {
  flat_set<std::string> fs;

  size_t idx = fs.emplace("hello");
  EXPECT_EQ(idx, 0);
  EXPECT_EQ(fs[0], "hello");

  // Test duplicate emplace
  size_t idx2 = fs.emplace("hello");
  EXPECT_EQ(idx2, 0);
  EXPECT_EQ(fs.size(), 1);
}

// Test move semantics
TEST_F(FlatSetTest, MoveInsertion) {
  flat_set<std::string> fs;

  std::string str = "movable";
  size_t idx = fs.insert(std::move(str));

  EXPECT_EQ(idx, 0);
  EXPECT_EQ(fs[0], "movable");
  // str should be moved from (implementation dependent)
}

// Test indexing operator
TEST_F(FlatSetTest, IndexOperator) {
  flat_set<int> fs;
  fs.insert(100);
  fs.insert(200);

  EXPECT_EQ(fs[0], 100);
  EXPECT_EQ(fs[1], 200);

  const auto &cfs = fs;
  EXPECT_EQ(cfs[0], 100);
  EXPECT_EQ(cfs[1], 200);
}

#ifdef _DEBUG
TEST_F(FlatSetTest, IndexOperatorOutOfBounds) {
  flat_set<int> fs;
  fs.insert(42);

  // This should trigger assertion in debug builds
  EXPECT_DEATH(fs[1], "Index out of bounds");
}
#endif

// Test iterators
TEST_F(FlatSetTest, BasicIteration) {
  flat_set<int> fs;
  fs.insert(1);
  fs.insert(2);
  fs.insert(3);

  std::vector<int> values;
  for (auto it = fs.begin(); it != fs.end(); ++it) {
    values.push_back(*it);
  }

  EXPECT_EQ(values, std::vector<int>({1, 2, 3}));
}

TEST_F(FlatSetTest, ConstIteration) {
  flat_set<int> fs;
  fs.insert(1);
  fs.insert(2);
  fs.insert(3);

  const auto &cfs = fs;
  std::vector<int> values;
  for (auto it = cfs.begin(); it != cfs.end(); ++it) {
    values.push_back(*it);
  }

  EXPECT_EQ(values, std::vector<int>({1, 2, 3}));
}

TEST_F(FlatSetTest, ReverseIteration) {
  flat_set<int> fs;
  fs.insert(1);
  fs.insert(2);
  fs.insert(3);

  std::vector<int> values;
  for (auto it = fs.rbegin(); it != fs.rend(); ++it) {
    values.push_back(*it);
  }

  EXPECT_EQ(values, std::vector<int>({3, 2, 1}));
}

TEST_F(FlatSetTest, RangeBasedFor) {
  flat_set<int> fs;
  fs.insert(10);
  fs.insert(20);
  fs.insert(30);

  std::vector<int> values;
  for (const auto &val : fs) {
    values.push_back(val);
  }

  EXPECT_EQ(values, std::vector<int>({10, 20, 30}));
}

// Test erase operations
TEST_F(FlatSetTest, EraseByValue) {
  flat_set<int> fs;
  fs.insert(10);
  fs.insert(20);
  fs.insert(30);

  auto it = fs.erase(20);
  EXPECT_EQ(fs.size(), 2);
  EXPECT_EQ(fs[0], 10);
  EXPECT_EQ(fs[1], 30);

  // Check returned iterator points to element that took erased element's place
  EXPECT_EQ(*it, 30);
}

TEST_F(FlatSetTest, EraseNonExistentValue) {
  flat_set<int> fs;
  fs.insert(10);
  fs.insert(20);
  fs.insert(30);

  auto it = fs.erase(99);  // Non-existent value
  EXPECT_EQ(fs.size(), 3); // Size unchanged
  EXPECT_EQ(it, fs.end()); // Should return end iterator
}

TEST_F(FlatSetTest, EraseByIterator) {
  flat_set<int> fs;
  fs.insert(10);
  fs.insert(20);
  fs.insert(30);

  auto it = fs.begin();
  ++it; // Point to second element (20)

  auto result_it = fs.erase(it);
  EXPECT_EQ(fs.size(), 2);
  EXPECT_EQ(fs[0], 10);
  EXPECT_EQ(fs[1], 30);

  // Returned iterator should point to element that took erased position
  EXPECT_EQ(*result_it, 30);
}

TEST_F(FlatSetTest, EraseLastElement) {
  flat_set<int> fs;
  fs.insert(10);
  fs.insert(20);
  fs.insert(30);

  auto last_it = fs.end();
  --last_it; // Point to last element

  auto result_it = fs.erase(last_it);
  EXPECT_EQ(fs.size(), 2);
  EXPECT_EQ(result_it, fs.end()); // Should return end()
}

TEST_F(FlatSetTest, EraseOnlyElement) {
  flat_set<int> fs;
  fs.insert(42);

  auto it = fs.erase(42);
  EXPECT_TRUE(fs.empty());
  EXPECT_EQ(it, fs.end());
}

TEST_F(FlatSetTest, EraseInvalidIterator) {
  flat_set<int> fs;
  fs.insert(10);

  auto end_it = fs.end();
  auto result = fs.erase(end_it);
  EXPECT_EQ(result, fs.end());
  EXPECT_EQ(fs.size(), 1); // Should be unchanged
}

// Test clear
TEST_F(FlatSetTest, Clear) {
  flat_set<int> fs;
  fs.insert(1);
  fs.insert(2);
  fs.insert(3);

  EXPECT_FALSE(fs.empty());

  fs.clear();
  EXPECT_TRUE(fs.empty());
  EXPECT_EQ(fs.size(), 0);
  EXPECT_EQ(fs.begin(), fs.end());
}

// Test find operations
TEST_F(FlatSetTest, FindExistingElement) {
  flat_set<int> fs;
  fs.insert(10);
  fs.insert(20);
  fs.insert(30);

  auto it = fs.find(20);
  EXPECT_NE(it, fs.end());
  EXPECT_EQ(*it, 20);

  // Test const version
  const auto &cfs = fs;
  auto cit = cfs.find(20);
  EXPECT_NE(cit, cfs.end());
  EXPECT_EQ(*cit, 20);
}

TEST_F(FlatSetTest, FindNonExistentElement) {
  flat_set<int> fs;
  fs.insert(10);
  fs.insert(20);
  fs.insert(30);

  auto it = fs.find(99);
  EXPECT_EQ(it, fs.end());

  // Test const version
  const auto &cfs = fs;
  auto cit = cfs.find(99);
  EXPECT_EQ(cit, cfs.end());
}

TEST_F(FlatSetTest, FindInEmptySet) {
  flat_set<int> fs;

  auto it = fs.find(42);
  EXPECT_EQ(it, fs.end());
}

// Test contains
TEST_F(FlatSetTest, Contains) {
  flat_set<int> fs;
  fs.insert(10);
  fs.insert(20);
  fs.insert(30);

  EXPECT_TRUE(fs.contains(10));
  EXPECT_TRUE(fs.contains(20));
  EXPECT_TRUE(fs.contains(30));
  EXPECT_FALSE(fs.contains(99));
}

TEST_F(FlatSetTest, ContainsEmptySet) {
  flat_set<int> fs;
  EXPECT_FALSE(fs.contains(42));
}

// Test extract
TEST_F(FlatSetTest, Extract) {
  flat_set<int> fs;
  fs.insert(1);
  fs.insert(2);
  fs.insert(3);

  auto container = fs.extract();
  EXPECT_EQ(container.size(), 3);
  EXPECT_EQ(container[0], 1);
  EXPECT_EQ(container[1], 2);
  EXPECT_EQ(container[2], 3);

  // Original set should be empty after extract
  EXPECT_TRUE(fs.empty());
}

// Test swap
TEST_F(FlatSetTest, Swap) {
  flat_set<int> fs1;
  fs1.insert(1);
  fs1.insert(2);

  flat_set<int> fs2;
  fs2.insert(10);
  fs2.insert(20);
  fs2.insert(30);

  swap(fs1, fs2);

  EXPECT_EQ(fs1.size(), 3);
  EXPECT_EQ(fs1[0], 10);
  EXPECT_EQ(fs1[1], 20);
  EXPECT_EQ(fs1[2], 30);

  EXPECT_EQ(fs2.size(), 2);
  EXPECT_EQ(fs2[0], 1);
  EXPECT_EQ(fs2[1], 2);
}

// Test comparison operators
TEST_F(FlatSetTest, ComparisonOperators) {
  flat_set<int> fs1;
  fs1.insert(1);
  fs1.insert(2);
  fs1.insert(3);

  flat_set<int> fs2;
  fs2.insert(1);
  fs2.insert(2);
  fs2.insert(3);

  flat_set<int> fs3;
  fs3.insert(1);
  fs3.insert(2);
  fs3.insert(4);

  EXPECT_EQ(fs1, fs2);
  EXPECT_NE(fs1, fs3);
  EXPECT_LT(fs1, fs3); // 3 < 4 in lexicographic order
}

// Test with custom container
TEST_F(FlatSetTest, CustomContainer) {
  using custom_set = flat_set<int, std::vector<int>>;
  custom_set fs;

  fs.insert(42);
  EXPECT_EQ(fs.size(), 1);
  EXPECT_EQ(fs[0], 42);
}

// Test with different types
TEST_F(FlatSetTest, StringType) {
  flat_set<std::string> fs;

  fs.insert("hello");
  fs.insert("world");
  fs.insert("hello"); // duplicate

  EXPECT_EQ(fs.size(), 2);
  EXPECT_EQ(fs[0], "hello");
  EXPECT_EQ(fs[1], "world");
}

// Test edge cases
TEST_F(FlatSetTest, LargeNumberOfElements) {
  flat_set<int> fs;

  // Insert many elements
  for (int i = 0; i < 1000; ++i) {
    size_t idx = fs.insert(i);
    EXPECT_EQ(idx, static_cast<size_t>(i));
  }

  EXPECT_EQ(fs.size(), 1000);

  // Verify all elements are present
  for (int i = 0; i < 1000; ++i) {
    EXPECT_EQ(fs[static_cast<size_t>(i)], i);
    EXPECT_TRUE(fs.contains(i));
  }
}

TEST_F(FlatSetTest, InterleavedInsertionsAndDuplicates) {
  flat_set<int> fs;

  // Pattern: insert new, insert duplicate, insert new
  fs.insert(1);
  fs.insert(2);
  fs.insert(1); // duplicate
  fs.insert(3);
  fs.insert(2); // duplicate
  fs.insert(4);

  EXPECT_EQ(fs.size(), 4);
  EXPECT_EQ(fs[0], 1);
  EXPECT_EQ(fs[1], 2);
  EXPECT_EQ(fs[2], 3);
  EXPECT_EQ(fs[3], 4);
}

// Test iterator stability after modifications
TEST_F(FlatSetTest, IteratorStabilityAfterErase) {
  flat_set<int> fs;
  fs.insert(10);
  fs.insert(20);
  fs.insert(30);
  fs.insert(40);

  auto it = fs.find(30);
  EXPECT_EQ(*it, 30);

  // Erase element before the iterator
  fs.erase(10);

  // Iterator should still be valid but point to different logical position
  // Note: This depends on implementation details
}

// Test preservation of insertion order
TEST_F(FlatSetTest, InsertionOrderPreservation) {
  flat_set<int> fs;

  // Insert in non-sorted order
  fs.insert(50);
  fs.insert(10);
  fs.insert(30);
  fs.insert(20);
  fs.insert(40);

  // Should preserve insertion order
  EXPECT_EQ(fs[0], 50);
  EXPECT_EQ(fs[1], 10);
  EXPECT_EQ(fs[2], 30);
  EXPECT_EQ(fs[3], 20);
  EXPECT_EQ(fs[4], 40);
}

// Test algorithmic compatibility
TEST_F(FlatSetTest, STLAlgorithmCompatibility) {
  flat_set<int> fs;
  fs.insert(3);
  fs.insert(1);
  fs.insert(4);
  fs.insert(1); // duplicate
  fs.insert(5);

  // Test std::find
  auto it = std::find(fs.begin(), fs.end(), 4);
  EXPECT_NE(it, fs.end());
  EXPECT_EQ(*it, 4);

  // Test std::count
  auto count = std::count(fs.begin(), fs.end(), 1);
  EXPECT_EQ(count, 1); // Should only appear once despite duplicate insertion

  // Test std::for_each
  int sum = 0;
  std::for_each(fs.begin(), fs.end(), [&sum](int val) { sum += val; });
  EXPECT_EQ(sum, 3 + 1 + 4 + 5); // 13
}

#include "opflow/detail/column_store.hpp"
#include <gtest/gtest.h>
#include <vector>

using namespace opflow::detail;

class ColumnStoreTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

// Test basic construction and properties
TEST_F(ColumnStoreTest, BasicConstruction) {
  column_store<int> store(4); // 4 columns

  EXPECT_EQ(store.ncol(), 4);
  EXPECT_EQ(store.nrow(), 0);
  EXPECT_EQ(store.size(), 0);
  EXPECT_TRUE(store.empty());
  EXPECT_EQ(store.column_capacity(), 0);
}

// Test construction with initial capacity
TEST_F(ColumnStoreTest, ConstructionWithCapacity) {
  column_store<int> store(3, 5); // 3 columns, capacity 5

  EXPECT_EQ(store.ncol(), 3);
  EXPECT_EQ(store.nrow(), 0);
  EXPECT_EQ(store.size(), 0);
  EXPECT_TRUE(store.empty());
  EXPECT_EQ(store.column_capacity(), 5);
}

// Test append functionality
TEST_F(ColumnStoreTest, AppendRows) {
  column_store<int> store(3);

  std::vector<int> row1 = {10, 20, 30};
  std::vector<int> row2 = {40, 50, 60};

  store.append(row1);
  EXPECT_EQ(store.nrow(), 1);
  EXPECT_EQ(store.size(), 3);
  EXPECT_FALSE(store.empty());

  store.append(row2);
  EXPECT_EQ(store.nrow(), 2);
  EXPECT_EQ(store.size(), 6);
}

// Test column access
TEST_F(ColumnStoreTest, ColumnAccess) {
  column_store<int> store(3);

  std::vector<int> row1 = {10, 20, 30};
  std::vector<int> row2 = {40, 50, 60};
  std::vector<int> row3 = {70, 80, 90};

  store.append(row1);
  store.append(row2);
  store.append(row3);

  // Test column 0
  auto col0 = store[0];
  EXPECT_EQ(col0.size(), 3);
  EXPECT_EQ(col0[0], 10);
  EXPECT_EQ(col0[1], 40);
  EXPECT_EQ(col0[2], 70);

  // Test column 1
  auto col1 = store[1];
  EXPECT_EQ(col1.size(), 3);
  EXPECT_EQ(col1[0], 20);
  EXPECT_EQ(col1[1], 50);
  EXPECT_EQ(col1[2], 80);

  // Test column 2
  auto col2 = store[2];
  EXPECT_EQ(col2.size(), 3);
  EXPECT_EQ(col2[0], 30);
  EXPECT_EQ(col2[1], 60);
  EXPECT_EQ(col2[2], 90);
}

// Test const column access
TEST_F(ColumnStoreTest, ConstColumnAccess) {
  column_store<int> store(2);

  std::vector<int> row1 = {10, 20};
  std::vector<int> row2 = {30, 40};

  store.append(row1);
  store.append(row2);

  const auto &const_store = store;
  auto col0 = const_store[0];
  auto col1 = const_store[1];

  EXPECT_EQ(col0[0], 10);
  EXPECT_EQ(col0[1], 30);
  EXPECT_EQ(col1[0], 20);
  EXPECT_EQ(col1[1], 40);
}

// Test element access
TEST_F(ColumnStoreTest, ElementAccess) {
  column_store<int> store(3);

  std::vector<int> row1 = {10, 20, 30};
  std::vector<int> row2 = {40, 50, 60};

  store.append(row1);
  store.append(row2);

  // Test at() method
  EXPECT_EQ(store.at(0, 0), 10);
  EXPECT_EQ(store.at(1, 0), 20);
  EXPECT_EQ(store.at(2, 0), 30);
  EXPECT_EQ(store.at(0, 1), 40);
  EXPECT_EQ(store.at(1, 1), 50);
  EXPECT_EQ(store.at(2, 1), 60);

  // Test modification
  store.at(1, 0) = 999;
  EXPECT_EQ(store.at(1, 0), 999);
  EXPECT_EQ(store[1][0], 999);
}

// Test reserve functionality
TEST_F(ColumnStoreTest, Reserve) {
  column_store<int> store(3);

  store.reserve(5);
  EXPECT_EQ(store.column_capacity(), 5);
  EXPECT_EQ(store.nrow(), 0);

  // Should be no-op if reserving smaller capacity
  store.reserve(3);
  EXPECT_EQ(store.column_capacity(), 5);
}

// Test clear functionality
TEST_F(ColumnStoreTest, Clear) {
  column_store<int> store(2);

  std::vector<int> row1 = {10, 20};
  std::vector<int> row2 = {30, 40};

  store.append(row1);
  store.append(row2);

  EXPECT_EQ(store.nrow(), 2);
  EXPECT_FALSE(store.empty());

  store.clear();

  EXPECT_EQ(store.nrow(), 0);
  EXPECT_TRUE(store.empty());
  EXPECT_EQ(store.size(), 0);

  // Capacity should be preserved
  EXPECT_GT(store.column_capacity(), 0);
}

// Test automatic capacity growth
TEST_F(ColumnStoreTest, AutomaticCapacityGrowth) {
  column_store<int> store(2);

  // Add enough rows to trigger capacity growth
  for (size_t i = 0; i < 10; ++i) {
    std::vector<int> row = {static_cast<int>(i * 10), static_cast<int>(i * 10 + 1)};
    store.append(row);
  }

  EXPECT_EQ(store.nrow(), 10);
  EXPECT_GE(store.column_capacity(), 10);

  // Verify all data is correct
  for (size_t i = 0; i < 10; ++i) {
    EXPECT_EQ(store.at(0, i), static_cast<int>(i * 10));
    EXPECT_EQ(store.at(1, i), static_cast<int>(i * 10 + 1));
  }
}

// Test with different data types
TEST_F(ColumnStoreTest, DifferentDataTypes) {
  // Test with double
  {
    column_store<double> store(2);
    std::vector<double> row = {3.14, 2.71};
    store.append(row);

    EXPECT_DOUBLE_EQ(store.at(0, 0), 3.14);
    EXPECT_DOUBLE_EQ(store.at(1, 0), 2.71);
  }

  // Test with struct
  struct Point {
    int x, y;
    bool operator==(const Point &other) const { return x == other.x && y == other.y; }
  };

  {
    column_store<Point> store(2);
    std::vector<Point> row = {{10, 20}, {30, 40}};
    store.append(row);

    EXPECT_EQ(store.at(0, 0).x, 10);
    EXPECT_EQ(store.at(0, 0).y, 20);
    EXPECT_EQ(store.at(1, 0).x, 30);
    EXPECT_EQ(store.at(1, 0).y, 40);
  }
}

// Test edge cases
TEST_F(ColumnStoreTest, EdgeCases) {
  // Single column
  {
    column_store<int> store(1);
    std::vector<int> row = {42};
    store.append(row);

    EXPECT_EQ(store.ncol(), 1);
    EXPECT_EQ(store.nrow(), 1);
    EXPECT_EQ(store.at(0, 0), 42);
  }
}

// Test with custom allocator
TEST_F(ColumnStoreTest, CustomAllocator) {
  using custom_alloc = std::allocator<int>;
  column_store<int, custom_alloc> store(3, 0, custom_alloc{});

  std::vector<int> row = {10, 20, 30};
  store.append(row);

  EXPECT_EQ(store.nrow(), 1);
  EXPECT_EQ(store.at(0, 0), 10);
  EXPECT_EQ(store.at(1, 0), 20);
  EXPECT_EQ(store.at(2, 0), 30);
}

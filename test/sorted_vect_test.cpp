#include <algorithm>
#include <gtest/gtest.h>
#include <random>
#include <vector>

#include "opflow/impl/sorted_vect.hpp"

using namespace opflow::impl;

class SortedVectTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

// Test basic push operations for int
TEST_F(SortedVectTest, PushIntValues) {
  sorted_vect<int> sv;

  // Push values in random order
  std::vector<int> values = {5, 2, 8, 1, 9, 3, 7, 4, 6};
  for (int val : values) {
    sv.push(val);
  }

  // Verify container is sorted
  EXPECT_TRUE(std::is_sorted(sv.begin(), sv.end()));
  EXPECT_EQ(sv.size(), values.size());

  // Verify all values are present
  std::vector<int> expected = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_EQ(std::vector<int>(sv.begin(), sv.end()), expected);
}

// Test push operations with move semantics
TEST_F(SortedVectTest, PushMoveSemantics) {
  sorted_vect<int> sv;

  int val1 = 10;
  int val2 = 5;

  sv.push(std::move(val1));
  sv.push(std::move(val2));

  EXPECT_TRUE(std::is_sorted(sv.begin(), sv.end()));
  EXPECT_EQ(sv.size(), 2);
  EXPECT_EQ(sv[0], 5);
  EXPECT_EQ(sv[1], 10);
}

// Test with double values
TEST_F(SortedVectTest, PushDoubleValues) {
  sorted_vect<double> sv;

  std::vector<double> values = {3.14, 1.41, 2.71, 0.57, 4.67};
  for (double val : values) {
    sv.push(val);
  }

  EXPECT_TRUE(std::is_sorted(sv.begin(), sv.end()));
  EXPECT_EQ(sv.size(), values.size());
}

// Test rank function with existing values
TEST_F(SortedVectTest, RankExistingValues) {
  sorted_vect<int> sv;
  std::vector<int> values = {10, 20, 30, 40, 50};

  for (int val : values) {
    sv.push(val);
  }

  EXPECT_EQ(sv.rank(10), 0);
  EXPECT_EQ(sv.rank(20), 1);
  EXPECT_EQ(sv.rank(30), 2);
  EXPECT_EQ(sv.rank(40), 3);
  EXPECT_EQ(sv.rank(50), 4);
}

// Test rank function with non-existing values
TEST_F(SortedVectTest, RankNonExistingValues) {
  sorted_vect<int> sv;
  std::vector<int> values = {10, 30, 50, 70, 90};

  for (int val : values) {
    sv.push(val);
  }

  // Non-existing values should return position where they would be inserted
  EXPECT_EQ(sv.rank(5), 5);   // Beyond end (not found)
  EXPECT_EQ(sv.rank(15), 5);  // Beyond end (not found)
  EXPECT_EQ(sv.rank(25), 5);  // Beyond end (not found)
  EXPECT_EQ(sv.rank(100), 5); // Beyond end (not found)
}

// Test erase existing values
TEST_F(SortedVectTest, EraseExistingValues) {
  sorted_vect<int> sv;
  std::vector<int> values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

  for (int val : values) {
    sv.push(val);
  }

  // Erase some values
  sv.erase(5);
  sv.erase(1);
  sv.erase(10);

  EXPECT_TRUE(std::is_sorted(sv.begin(), sv.end()));
  EXPECT_EQ(sv.size(), 7);

  std::vector<int> expected = {2, 3, 4, 6, 7, 8, 9};
  EXPECT_EQ(std::vector<int>(sv.begin(), sv.end()), expected);
}

// Test erase non-existing values (should be no-op)
TEST_F(SortedVectTest, EraseNonExistingValues) {
  sorted_vect<int> sv;
  std::vector<int> values = {2, 4, 6, 8, 10};

  for (int val : values) {
    sv.push(val);
  }

  size_t original_size = sv.size();

  // Try to erase non-existing values
  sv.erase(1);
  sv.erase(3);
  sv.erase(5);
  sv.erase(11);

  EXPECT_TRUE(std::is_sorted(sv.begin(), sv.end()));
  EXPECT_EQ(sv.size(), original_size);

  std::vector<int> expected = {2, 4, 6, 8, 10};
  EXPECT_EQ(std::vector<int>(sv.begin(), sv.end()), expected);
}

// Test erase_rank function
TEST_F(SortedVectTest, EraseByRank) {
  sorted_vect<int> sv;
  std::vector<int> values = {10, 20, 30, 40, 50};

  for (int val : values) {
    sv.push(val);
  }

  // Erase element at rank 2 (value 30)
  sv.erase_rank(2);

  EXPECT_TRUE(std::is_sorted(sv.begin(), sv.end()));
  EXPECT_EQ(sv.size(), 4);

  std::vector<int> expected = {10, 20, 40, 50};
  EXPECT_EQ(std::vector<int>(sv.begin(), sv.end()), expected);

  // Erase first element
  sv.erase_rank(0);
  expected = {20, 40, 50};
  EXPECT_EQ(std::vector<int>(sv.begin(), sv.end()), expected);

  // Erase last element
  sv.erase_rank(sv.size() - 1);
  expected = {20, 40};
  EXPECT_EQ(std::vector<int>(sv.begin(), sv.end()), expected);
}

// Test erase_rank with invalid indices
TEST_F(SortedVectTest, EraseByRankInvalid) {
  sorted_vect<int> sv;
  sv.push(100);
  sv.push(200);

  size_t original_size = sv.size();

  // Try to erase out-of-bounds index
  sv.erase_rank(10);

  EXPECT_EQ(sv.size(), original_size);
  EXPECT_TRUE(std::is_sorted(sv.begin(), sv.end()));
}

// Test behavior with duplicate values
TEST_F(SortedVectTest, DuplicateValues) {
  sorted_vect<int> sv;

  sv.push(5);
  sv.push(3);
  sv.push(5);
  sv.push(1);
  sv.push(3);
  sv.push(5);

  EXPECT_TRUE(std::is_sorted(sv.begin(), sv.end()));
  EXPECT_EQ(sv.size(), 6);

  std::vector<int> expected = {1, 3, 3, 5, 5, 5};
  EXPECT_EQ(std::vector<int>(sv.begin(), sv.end()), expected);

  // Erase one instance of 5
  sv.erase(5);
  expected = {1, 3, 3, 5, 5};
  EXPECT_EQ(std::vector<int>(sv.begin(), sv.end()), expected);
}

// Test with larger dataset to trigger binary search threshold
TEST_F(SortedVectTest, LargeDatasetBinarySearch) {
  sorted_vect<int, 50> sv; // Lower threshold for testing

  // Generate random values
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(1, 1000);

  std::vector<int> values;
  for (int i = 0; i < 200; ++i) {
    int val = dis(gen);
    values.push_back(val);
    sv.push(val);
  }

  EXPECT_TRUE(std::is_sorted(sv.begin(), sv.end()));
  EXPECT_EQ(sv.size(), values.size());

  // Test some rank operations
  if (!sv.empty()) {
    EXPECT_EQ(sv.rank(sv[0]), 0);
    if (sv.size() > 1) {
      EXPECT_EQ(sv.rank(sv[sv.size() / 2]), sv.size() / 2);
    }
  }
}

// Test empty container
TEST_F(SortedVectTest, EmptyContainer) {
  sorted_vect<int> sv;

  EXPECT_TRUE(sv.empty());
  EXPECT_EQ(sv.size(), 0);
  EXPECT_TRUE(std::is_sorted(sv.begin(), sv.end()));

  // Rank of any value in empty container should be 0
  EXPECT_EQ(sv.rank(42), 0);

  // Erasing from empty container should be safe
  sv.erase(42);
  EXPECT_TRUE(sv.empty());

  // Erase by rank should be safe
  sv.erase_rank(0);
  EXPECT_TRUE(sv.empty());
}

// Test with char type
TEST_F(SortedVectTest, CharType) {
  sorted_vect<char> sv;

  std::string input = "hello";
  for (char c : input) {
    sv.push(c);
  }

  EXPECT_TRUE(std::is_sorted(sv.begin(), sv.end()));

  std::vector<char> expected = {'e', 'h', 'l', 'l', 'o'};
  EXPECT_EQ(std::vector<char>(sv.begin(), sv.end()), expected);
}

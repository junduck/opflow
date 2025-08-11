#include <algorithm>
#include <gtest/gtest.h>
#include <numeric>
#include <vector>

#include "opflow/detail/ringbuf_vect.hpp"

using namespace opflow::detail;

class RingbufVectTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

// Test basic construction and properties
TEST_F(RingbufVectTest, DefaultConstruction) {
  ringbuf_vect<int> rb;
  EXPECT_TRUE(rb.empty());
  EXPECT_EQ(rb.size(), 0);
}

TEST_F(RingbufVectTest, ConstructionWithCapacity) {
  ringbuf_vect<int> rb(16);
  EXPECT_TRUE(rb.empty());
  EXPECT_EQ(rb.size(), 0);
}

TEST_F(RingbufVectTest, ConstructionWithZeroCapacity) {
  ringbuf_vect<int> rb(0);
  EXPECT_TRUE(rb.empty());
  EXPECT_EQ(rb.size(), 0);
}

// Test basic push and access operations
TEST_F(RingbufVectTest, BasicPushAndAccess) {
  ringbuf_vect<int> rb;

  rb.push(42);
  EXPECT_FALSE(rb.empty());
  EXPECT_EQ(rb.size(), 1);
  EXPECT_EQ(rb.front(), 42);
  EXPECT_EQ(rb.back(), 42);
  EXPECT_EQ(rb[0], 42);
}

TEST_F(RingbufVectTest, MultiplePushes) {
  ringbuf_vect<int> rb;

  for (int i = 1; i <= 5; ++i) {
    rb.push(i * 10);
  }

  EXPECT_EQ(rb.size(), 5);
  EXPECT_EQ(rb.front(), 10);
  EXPECT_EQ(rb.back(), 50);

  for (size_t i = 0; i < 5; ++i) {
    EXPECT_EQ(rb[i], static_cast<int>((i + 1) * 10));
  }
}

// Test pop operations
TEST_F(RingbufVectTest, PopFront) {
  ringbuf_vect<int> rb;

  for (int i = 1; i <= 5; ++i) {
    rb.push(i * 10);
  }

  rb.pop();
  EXPECT_EQ(rb.size(), 4);
  EXPECT_EQ(rb.front(), 20);
  EXPECT_EQ(rb.back(), 50);

  rb.pop();
  EXPECT_EQ(rb.size(), 3);
  EXPECT_EQ(rb.front(), 30);

  // Test remaining elements are still accessible
  EXPECT_EQ(rb[0], 30);
  EXPECT_EQ(rb[1], 40);
  EXPECT_EQ(rb[2], 50);
}

TEST_F(RingbufVectTest, PopFromEmptyBuffer) {
  ringbuf_vect<int> rb;

  rb.pop(); // Should not crash
  EXPECT_TRUE(rb.empty());
  EXPECT_EQ(rb.size(), 0);
}

// Test capacity expansion and wrapping
TEST_F(RingbufVectTest, CapacityExpansion) {
  ringbuf_vect<int> rb(4);

  // Fill beyond initial capacity
  for (int i = 1; i <= 10; ++i) {
    rb.push(i);
  }

  EXPECT_EQ(rb.size(), 10);
  for (size_t i = 0; i < 10; ++i) {
    EXPECT_EQ(rb[i], static_cast<int>(i + 1));
  }
}

TEST_F(RingbufVectTest, WrapAroundBehavior) {
  ringbuf_vect<int> rb(8);

  // Fill buffer
  for (int i = 1; i <= 8; ++i) {
    rb.push(i);
  }

  // Pop some elements to create wrap-around scenario
  rb.pop();
  rb.pop();
  rb.pop();

  // Add more elements (should wrap around)
  rb.push(100);
  rb.push(200);
  rb.push(300);

  EXPECT_EQ(rb.size(), 8);
  EXPECT_EQ(rb[0], 4);   // First remaining element
  EXPECT_EQ(rb[7], 300); // Last added element
}

// Test iterators
TEST_F(RingbufVectTest, ForwardIterators) {
  ringbuf_vect<int> rb;

  for (int i = 1; i <= 5; ++i) {
    rb.push(i * 10);
  }

  std::vector<int> values;
  for (auto it = rb.begin(); it != rb.end(); ++it) {
    values.push_back(*it);
  }

  std::vector<int> expected = {10, 20, 30, 40, 50};
  EXPECT_EQ(values, expected);
}

TEST_F(RingbufVectTest, ConstIterators) {
  ringbuf_vect<int> rb;

  for (int i = 1; i <= 3; ++i) {
    rb.push(i * 100);
  }

  const auto &const_rb = rb;
  std::vector<int> values;
  for (auto it = const_rb.cbegin(); it != const_rb.cend(); ++it) {
    values.push_back(*it);
  }

  std::vector<int> expected = {100, 200, 300};
  EXPECT_EQ(values, expected);
}

TEST_F(RingbufVectTest, ReverseIterators) {
  ringbuf_vect<int> rb;

  for (int i = 1; i <= 4; ++i) {
    rb.push(i);
  }

  std::vector<int> values;
  for (auto it = rb.rbegin(); it != rb.rend(); ++it) {
    values.push_back(*it);
  }

  std::vector<int> expected = {4, 3, 2, 1};
  EXPECT_EQ(values, expected);
}

TEST_F(RingbufVectTest, RangeBasedForLoop) {
  ringbuf_vect<int> rb;

  for (int i = 10; i <= 30; i += 10) {
    rb.push(i);
  }

  std::vector<int> values;
  for (const auto &val : rb) {
    values.push_back(val);
  }

  std::vector<int> expected = {10, 20, 30};
  EXPECT_EQ(values, expected);
}

// Test iterator arithmetic
TEST_F(RingbufVectTest, IteratorArithmetic) {
  ringbuf_vect<int> rb;

  for (int i = 0; i < 6; ++i) {
    rb.push(i * 5);
  }

  auto it = rb.begin();
  EXPECT_EQ(*it, 0);

  it += 3;
  EXPECT_EQ(*it, 15);

  it -= 1;
  EXPECT_EQ(*it, 10);

  auto it2 = it + 2;
  EXPECT_EQ(*it2, 20);

  auto diff = it2 - it;
  EXPECT_EQ(diff, 2);

  EXPECT_EQ(it[1], 15);
}

// Test with wrapped around data
TEST_F(RingbufVectTest, IteratorsAfterWrapAround) {
  ringbuf_vect<int> rb(4);

  // Fill the buffer
  for (int i = 1; i <= 4; ++i) {
    rb.push(i);
  }

  // Create wrap-around
  rb.pop();
  rb.pop();
  rb.push(5);
  rb.push(6);

  std::vector<int> values;
  std::copy(rb.begin(), rb.end(), std::back_inserter(values));

  std::vector<int> expected = {3, 4, 5, 6};
  EXPECT_EQ(values, expected);
}

// Test clear operation
TEST_F(RingbufVectTest, Clear) {
  ringbuf_vect<int> rb;

  for (int i = 1; i <= 5; ++i) {
    rb.push(i);
  }

  EXPECT_EQ(rb.size(), 5);
  EXPECT_FALSE(rb.empty());

  rb.clear();

  EXPECT_EQ(rb.size(), 0);
  EXPECT_TRUE(rb.empty());

  // Should be able to use after clear
  rb.push(99);
  EXPECT_EQ(rb.size(), 1);
  EXPECT_EQ(rb.front(), 99);
}

// Test reserve operation
TEST_F(RingbufVectTest, Reserve) {
  ringbuf_vect<int> rb(2);

  rb.push(1);
  rb.push(2);

  rb.reserve(16);

  // Data should still be accessible
  EXPECT_EQ(rb.size(), 2);
  EXPECT_EQ(rb[0], 1);
  EXPECT_EQ(rb[1], 2);

  // Should be able to add more elements without expansion
  for (int i = 3; i <= 10; ++i) {
    rb.push(i);
  }

  EXPECT_EQ(rb.size(), 10);
  for (size_t i = 0; i < 10; ++i) {
    EXPECT_EQ(rb[i], static_cast<int>(i + 1));
  }
}

// Test copy constructor and assignment
TEST_F(RingbufVectTest, CopyConstructor) {
  ringbuf_vect<int> rb1;

  for (int i = 1; i <= 5; ++i) {
    rb1.push(i * 2);
  }

  ringbuf_vect<int> rb2(rb1);

  EXPECT_EQ(rb1.size(), rb2.size());
  for (size_t i = 0; i < rb1.size(); ++i) {
    EXPECT_EQ(rb1[i], rb2[i]);
  }

  // Modify original, copy should be unchanged
  rb1.push(99);
  EXPECT_EQ(rb1.size(), 6);
  EXPECT_EQ(rb2.size(), 5);
}

TEST_F(RingbufVectTest, CopyAssignment) {
  ringbuf_vect<int> rb1;
  ringbuf_vect<int> rb2;

  for (int i = 1; i <= 3; ++i) {
    rb1.push(i * 10);
  }

  rb2.push(999); // Different initial state

  rb2 = rb1;

  EXPECT_EQ(rb1.size(), rb2.size());
  for (size_t i = 0; i < rb1.size(); ++i) {
    EXPECT_EQ(rb1[i], rb2[i]);
  }
}

// Test move constructor and assignment
TEST_F(RingbufVectTest, MoveConstructor) {
  ringbuf_vect<int> rb1;

  for (int i = 1; i <= 4; ++i) {
    rb1.push(i * 3);
  }

  ringbuf_vect<int> rb2(std::move(rb1));

  EXPECT_EQ(rb2.size(), 4);
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(rb2[i], static_cast<int>((i + 1) * 3));
  }
}

TEST_F(RingbufVectTest, MoveAssignment) {
  ringbuf_vect<int> rb1;
  ringbuf_vect<int> rb2;

  for (int i = 1; i <= 3; ++i) {
    rb1.push(i * 7);
  }

  rb2 = std::move(rb1);

  EXPECT_EQ(rb2.size(), 3);
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(rb2[i], static_cast<int>((i + 1) * 7));
  }
}

// Test with different data types
TEST_F(RingbufVectTest, FloatingPointType) {
  ringbuf_vect<double> rb;

  rb.push(3.14);
  rb.push(2.71);
  rb.push(1.41);

  EXPECT_EQ(rb.size(), 3);
  EXPECT_DOUBLE_EQ(rb[0], 3.14);
  EXPECT_DOUBLE_EQ(rb[1], 2.71);
  EXPECT_DOUBLE_EQ(rb[2], 1.41);
}

// Stress test with large number of operations
TEST_F(RingbufVectTest, StressTest) {
  ringbuf_vect<int> rb(8);

  // Add many elements
  for (int i = 0; i < 1000; ++i) {
    rb.push(i);
  }

  EXPECT_EQ(rb.size(), 1000);

  // Remove half
  for (int i = 0; i < 500; ++i) {
    rb.pop();
  }

  EXPECT_EQ(rb.size(), 500);
  EXPECT_EQ(rb.front(), 500);
  EXPECT_EQ(rb.back(), 999);

  // Add more elements
  for (int i = 1000; i < 1200; ++i) {
    rb.push(i);
  }

  EXPECT_EQ(rb.size(), 700);
  EXPECT_EQ(rb.front(), 500);
  EXPECT_EQ(rb.back(), 1199);
}

// Test algorithm compatibility
TEST_F(RingbufVectTest, AlgorithmCompatibility) {
  ringbuf_vect<int> rb;

  for (int i = 1; i <= 10; ++i) {
    rb.push(i);
  }

  // Test std::find
  auto it = std::find(rb.begin(), rb.end(), 5);
  EXPECT_NE(it, rb.end());
  EXPECT_EQ(*it, 5);

  // Test std::accumulate
  int sum = std::accumulate(rb.begin(), rb.end(), 0);
  EXPECT_EQ(sum, 55); // 1+2+...+10 = 55

  // Test std::count_if
  auto even_count = std::count_if(rb.begin(), rb.end(), [](int x) { return x % 2 == 0; });
  EXPECT_EQ(even_count, 5); // 2, 4, 6, 8, 10
}

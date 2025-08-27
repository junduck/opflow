#include <gtest/gtest.h>

#include "opflow/common.hpp"
#include "opflow/detail/vector_store.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <memory_resource>
#include <numeric>
#include <type_traits>

using namespace opflow::detail;
using namespace opflow;

class VectorStoreTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

// Test basic construction and properties
TEST_F(VectorStoreTest, BasicConstruction) {
  vector_store<int> store(4, 3); // 4 elements per group, 3 groups

  EXPECT_EQ(store.group_size(), 4);
  EXPECT_EQ(store.num_groups(), 3);
  EXPECT_EQ(store.size(), 12); // 4 * 3

  // Stride should be aligned to cacheline
  size_t expected_stride = aligned_size(4 * sizeof(int), cacheline_size);
  EXPECT_EQ(store.group_stride(), expected_stride);
}

// Test construction with custom allocator
TEST_F(VectorStoreTest, CustomAllocatorConstruction) {
  using custom_alloc = cacheline_aligned_alloc<std::byte>;
  vector_store<double, custom_alloc> store(2, 5, custom_alloc{});

  EXPECT_EQ(store.group_size(), 2);
  EXPECT_EQ(store.num_groups(), 5);
  EXPECT_EQ(store.size(), 10);
}

// Test PMR allocator construction
TEST_F(VectorStoreTest, PMRAllocatorConstruction) {
  std::pmr::monotonic_buffer_resource buffer(1024);
  std::pmr::polymorphic_allocator<std::byte> alloc(&buffer);

  vector_store<float, std::pmr::polymorphic_allocator<std::byte>> store(3, 4, alloc);

  EXPECT_EQ(store.group_size(), 3);
  EXPECT_EQ(store.num_groups(), 4);
  EXPECT_EQ(store.size(), 12);
}

// Test alignment properties
TEST_F(VectorStoreTest, CachelineAlignment) {
  vector_store<int> store(5, 4);

  // Each group should start at cacheline aligned address
  for (size_t grp = 0; grp < store.num_groups(); ++grp) {
    auto span = store[grp];
    auto addr = reinterpret_cast<uintptr_t>(span.data());

    // First group should be aligned
    if (grp == 0) {
      EXPECT_EQ(addr % cacheline_size, 0) << "Group " << grp << " not aligned";
    }

    // All groups should be separated by stride
    if (grp > 0) {
      auto prev_span = store[grp - 1];
      auto prev_addr = reinterpret_cast<uintptr_t>(prev_span.data());
      EXPECT_EQ(addr - prev_addr, store.group_stride())
          << "Incorrect stride between groups " << (grp - 1) << " and " << grp;
    }
  }
}

// Test data access and modification
TEST_F(VectorStoreTest, DataAccess) {
  vector_store<int> store(3, 2);

  // Initialize with test data
  for (size_t grp = 0; grp < store.num_groups(); ++grp) {
    auto span = store[grp];
    for (size_t i = 0; i < span.size(); ++i) {
      span[i] = static_cast<int>(grp * 100 + i);
    }
  }

  // Verify data through get() method
  for (size_t grp = 0; grp < store.num_groups(); ++grp) {
    auto span = store.get(grp);
    for (size_t i = 0; i < span.size(); ++i) {
      EXPECT_EQ(span[i], static_cast<int>(grp * 100 + i));
    }
  }

  // Verify data through operator[]
  for (size_t grp = 0; grp < store.num_groups(); ++grp) {
    auto span = store[grp];
    for (size_t i = 0; i < span.size(); ++i) {
      EXPECT_EQ(span[i], static_cast<int>(grp * 100 + i));
    }
  }
}

// Test const access
TEST_F(VectorStoreTest, ConstAccess) {
  vector_store<int> store(2, 3);

  // Initialize data
  store[0][0] = 42;
  store[1][1] = 84;
  store[2][0] = 126;

  // Test const access
  const auto &const_store = store;
  EXPECT_EQ(const_store[0][0], 42);
  EXPECT_EQ(const_store[1][1], 84);
  EXPECT_EQ(const_store[2][0], 126);

  EXPECT_EQ(const_store.get(0)[0], 42);
  EXPECT_EQ(const_store.get(1)[1], 84);
  EXPECT_EQ(const_store.get(2)[0], 126);
}

// Test with different data types
TEST_F(VectorStoreTest, DifferentDataTypes) {
  // Test with char
  {
    vector_store<char> store(8, 2);
    store[0][0] = 'A';
    store[1][7] = 'Z';
    EXPECT_EQ(store[0][0], 'A');
    EXPECT_EQ(store[1][7], 'Z');
  }

  // Test with double
  {
    vector_store<double> store(4, 3);
    store[1][2] = 3.14159;
    EXPECT_DOUBLE_EQ(store[1][2], 3.14159);
  }

  // Test with struct
  struct Point {
    int x, y;
  };
  static_assert(std::is_trivial_v<Point>);
  {
    vector_store<Point> store(2, 2);
    store[0][0] = {10, 20};
    store[1][1] = {30, 40};
    EXPECT_EQ(store[0][0].x, 10);
    EXPECT_EQ(store[0][0].y, 20);
    EXPECT_EQ(store[1][1].x, 30);
    EXPECT_EQ(store[1][1].y, 40);
  }
}

// Test edge cases
TEST_F(VectorStoreTest, EdgeCases) {
  // Single element per group
  {
    vector_store<int> store(1, 5);
    EXPECT_EQ(store.group_size(), 1);
    EXPECT_EQ(store.num_groups(), 5);
    EXPECT_EQ(store.size(), 5);

    for (size_t i = 0; i < 5; ++i) {
      store[i][0] = static_cast<int>(i * 10);
    }

    for (size_t i = 0; i < 5; ++i) {
      EXPECT_EQ(store[i][0], static_cast<int>(i * 10));
    }
  }

  // Single group
  {
    vector_store<double> store(10, 1);
    EXPECT_EQ(store.group_size(), 10);
    EXPECT_EQ(store.num_groups(), 1);
    EXPECT_EQ(store.size(), 10);

    auto span = store[0];
    for (size_t i = 0; i < span.size(); ++i) {
      span[i] = i * 0.5;
    }

    for (size_t i = 0; i < span.size(); ++i) {
      EXPECT_DOUBLE_EQ(store[0][i], i * 0.5);
    }
  }

  // Large elements that span multiple cachelines
  {
    struct LargeStruct {
      std::array<int, cacheline_size / sizeof(int) + 1> data;
    };
    static_assert(std::is_trivial_v<LargeStruct>);

    vector_store<LargeStruct> store(2, 3);
    EXPECT_EQ(store.group_size(), 2);
    EXPECT_EQ(store.num_groups(), 3);

    // Verify we can access all elements
    store[0][0].data[0] = 100;
    store[2][1].data.back() = 200;
    EXPECT_EQ(store[0][0].data[0], 100);
    EXPECT_EQ(store[2][1].data.back(), 200);

    auto addr2 = reinterpret_cast<uintptr_t>(store[2].data());
    EXPECT_EQ(addr2 % cacheline_size, 0) << "Group 2 not aligned";
  }
}

// Test memory layout and no false sharing
TEST_F(VectorStoreTest, MemoryLayout) {
  vector_store<int> store(4, 3);

  // Groups should be in separate cachelines
  auto span0 = store[0];
  auto span1 = store[1];
  auto span2 = store[2];

  auto addr0 = reinterpret_cast<uintptr_t>(span0.data());
  auto addr1 = reinterpret_cast<uintptr_t>(span1.data());
  auto addr2 = reinterpret_cast<uintptr_t>(span2.data());

  // Groups should be separated by at least cacheline_size
  EXPECT_GE(addr1 - addr0, cacheline_size);
  EXPECT_GE(addr2 - addr1, cacheline_size);

  // First group should be cacheline aligned
  EXPECT_EQ(addr0 % cacheline_size, 0);
}

// Test initialization
TEST_F(VectorStoreTest, Initialization) {
  // Elements should be value-initialized (zero for arithmetic types)
  vector_store<int> store(5, 3);

  for (size_t grp = 0; grp < store.num_groups(); ++grp) {
    auto span = store[grp];
    for (size_t i = 0; i < span.size(); ++i) {
      EXPECT_EQ(span[i], 0) << "Element [" << grp << "][" << i << "] not zero-initialized";
    }
  }

  // Test with floating point
  vector_store<double> store_double(3, 2);
  for (size_t grp = 0; grp < store_double.num_groups(); ++grp) {
    auto span = store_double[grp];
    for (size_t i = 0; i < span.size(); ++i) {
      EXPECT_DOUBLE_EQ(span[i], 0.0) << "Element [" << grp << "][" << i << "] not zero-initialized";
    }
  }
}

// Test span properties
TEST_F(VectorStoreTest, SpanProperties) {
  vector_store<int> store(6, 4);

  for (size_t grp = 0; grp < store.num_groups(); ++grp) {
    auto span = store[grp];

    EXPECT_EQ(span.size(), 6);
    EXPECT_FALSE(span.empty());
    EXPECT_EQ(span.size_bytes(), 6 * sizeof(int));

    // Test iterator functionality
    std::fill(span.begin(), span.end(), static_cast<int>(grp + 1));
    EXPECT_TRUE(std::all_of(span.begin(), span.end(), [grp](int val) { return val == static_cast<int>(grp + 1); }));
  }
}

// Test copy semantics (should be default copyable for trivial types)
TEST_F(VectorStoreTest, CopySemantics) {
  vector_store<int> store1(3, 2);

  // Initialize with test data
  store1[0][0] = 10;
  store1[0][1] = 20;
  store1[0][2] = 30;
  store1[1][0] = 40;
  store1[1][1] = 50;
  store1[1][2] = 60;

  // Copy construction
  vector_store<int> store2 = store1;

  EXPECT_EQ(store2.group_size(), store1.group_size());
  EXPECT_EQ(store2.num_groups(), store1.num_groups());
  EXPECT_EQ(store2.size(), store1.size());

  // Verify data was copied
  for (size_t grp = 0; grp < store1.num_groups(); ++grp) {
    auto span1 = store1[grp];
    auto span2 = store2[grp];
    for (size_t i = 0; i < span1.size(); ++i) {
      EXPECT_EQ(span1[i], span2[i]);
    }
  }

  // Modify copy and ensure original is unchanged
  store2[0][0] = 999;
  EXPECT_EQ(store1[0][0], 10);
  EXPECT_EQ(store2[0][0], 999);
}

// Test move semantics
TEST_F(VectorStoreTest, MoveSemantics) {
  vector_store<int> store1(4, 3);
  store1[1][2] = 42;

  auto original_size = store1.size();
  auto original_group_size = store1.group_size();
  auto original_num_groups = store1.num_groups();

  // Move construction
  vector_store<int> store2 = std::move(store1);

  EXPECT_EQ(store2.size(), original_size);
  EXPECT_EQ(store2.group_size(), original_group_size);
  EXPECT_EQ(store2.num_groups(), original_num_groups);
  EXPECT_EQ(store2[1][2], 42);
}

// Performance/stress test with larger data
TEST_F(VectorStoreTest, StressTest) {
  constexpr size_t num_groups = 100;
  constexpr size_t group_size = 50;

  vector_store<uint64_t> store(group_size, num_groups);

  // Fill with pattern
  for (size_t grp = 0; grp < store.num_groups(); ++grp) {
    auto span = store[grp];
    for (size_t i = 0; i < span.size(); ++i) {
      span[i] = grp * 1000000 + i;
    }
  }

  // Verify pattern
  for (size_t grp = 0; grp < store.num_groups(); ++grp) {
    auto span = store[grp];
    for (size_t i = 0; i < span.size(); ++i) {
      EXPECT_EQ(span[i], grp * 1000000 + i);
    }
  }

  EXPECT_EQ(store.size(), num_groups * group_size);
}

// Test allocator propagation
TEST_F(VectorStoreTest, AllocatorPropagation) {
  // Test that custom allocators are properly used
  std::array<std::byte, 4096> buffer;
  std::pmr::monotonic_buffer_resource resource(buffer.data(), buffer.size());
  std::pmr::polymorphic_allocator<std::byte> alloc(&resource);

  // This should use our custom allocator
  vector_store<int, std::pmr::polymorphic_allocator<std::byte>> store(10, 5, alloc);

  EXPECT_EQ(store.group_size(), 10);
  EXPECT_EQ(store.num_groups(), 5);

  // Fill with data to ensure allocation worked
  for (size_t grp = 0; grp < store.num_groups(); ++grp) {
    auto span = store[grp];
    std::iota(span.begin(), span.end(), static_cast<int>(grp * 100));
  }

  // Verify data
  for (size_t grp = 0; grp < store.num_groups(); ++grp) {
    auto span = store[grp];
    for (size_t i = 0; i < span.size(); ++i) {
      EXPECT_EQ(span[i], static_cast<int>(grp * 100 + i));
    }
  }
}

// Test ensure_group_capacity method
TEST_F(VectorStoreTest, EnsureGroupCapacity) {
  vector_store<int> store(3, 4); // 3 elements per group, 4 groups

  // Initialize with test data
  for (size_t grp = 0; grp < store.num_groups(); ++grp) {
    auto span = store[grp];
    for (size_t i = 0; i < span.size(); ++i) {
      span[i] = static_cast<int>(grp * 10 + i);
    }
  }

  // Ensure capacity smaller than current - should be no-op
  store.ensure_group_capacity(2);
  EXPECT_EQ(store.group_size(), 3);
  EXPECT_EQ(store.num_groups(), 4);

  // Verify data is unchanged
  for (size_t grp = 0; grp < store.num_groups(); ++grp) {
    auto span = store[grp];
    for (size_t i = 0; i < span.size(); ++i) {
      EXPECT_EQ(span[i], static_cast<int>(grp * 10 + i));
    }
  }

  // Ensure capacity equal to current - should be no-op
  store.ensure_group_capacity(3);
  EXPECT_EQ(store.group_size(), 3);
  EXPECT_EQ(store.num_groups(), 4);

  // Verify data is unchanged
  for (size_t grp = 0; grp < store.num_groups(); ++grp) {
    auto span = store[grp];
    for (size_t i = 0; i < span.size(); ++i) {
      EXPECT_EQ(span[i], static_cast<int>(grp * 10 + i));
    }
  }

  // Expand capacity to larger size
  store.ensure_group_capacity(6);
  EXPECT_EQ(store.group_size(), 6);
  EXPECT_EQ(store.num_groups(), 4);

  // Verify original data is preserved
  for (size_t grp = 0; grp < store.num_groups(); ++grp) {
    auto span = store[grp];
    for (size_t i = 0; i < 3; ++i) { // Original data
      EXPECT_EQ(span[i], static_cast<int>(grp * 10 + i));
    }
    for (size_t i = 3; i < 6; ++i) { // New elements should be zero-initialized
      EXPECT_EQ(span[i], 0);
    }
  }

  // Test we can use the new capacity
  for (size_t grp = 0; grp < store.num_groups(); ++grp) {
    auto span = store[grp];
    span[4] = static_cast<int>(grp * 100 + 4);
    span[5] = static_cast<int>(grp * 100 + 5);
  }

  // Verify new data
  for (size_t grp = 0; grp < store.num_groups(); ++grp) {
    auto span = store[grp];
    EXPECT_EQ(span[4], static_cast<int>(grp * 100 + 4));
    EXPECT_EQ(span[5], static_cast<int>(grp * 100 + 5));
  }
}

// Test ensure_group_capacity with alignment considerations
TEST_F(VectorStoreTest, EnsureGroupCapacityAlignment) {
  vector_store<int> store(2, 3);

  // Fill with test data
  store[0][0] = 10;
  store[0][1] = 20;
  store[1][0] = 30;
  store[1][1] = 40;
  store[2][0] = 50;
  store[2][1] = 60;

  // Expand to a size that might have the same stride (depending on cacheline size)
  store.ensure_group_capacity(3);

  // Verify alignment is maintained
  for (size_t grp = 0; grp < store.num_groups(); ++grp) {
    auto span = store[grp];
    auto addr = reinterpret_cast<uintptr_t>(span.data());
    if (grp == 0) {
      EXPECT_EQ(addr % cacheline_size, 0) << "Group " << grp << " not aligned after expansion";
    }
  }

  // Verify data preservation
  EXPECT_EQ(store[0][0], 10);
  EXPECT_EQ(store[0][1], 20);
  EXPECT_EQ(store[1][0], 30);
  EXPECT_EQ(store[1][1], 40);
  EXPECT_EQ(store[2][0], 50);
  EXPECT_EQ(store[2][1], 60);

  // New elements should be zero-initialized
  EXPECT_EQ(store[0][2], 0);
  EXPECT_EQ(store[1][2], 0);
  EXPECT_EQ(store[2][2], 0);
}

// Test ensure_group_capacity with significant size increase
TEST_F(VectorStoreTest, EnsureGroupCapacityLargeIncrease) {
  vector_store<double> store(2, 2);

  // Initialize with known values
  store[0][0] = 1.1;
  store[0][1] = 2.2;
  store[1][0] = 3.3;
  store[1][1] = 4.4;

  // Significantly increase capacity
  store.ensure_group_capacity(10);

  EXPECT_EQ(store.group_size(), 10);
  EXPECT_EQ(store.num_groups(), 2);

  // Verify original data
  EXPECT_DOUBLE_EQ(store[0][0], 1.1);
  EXPECT_DOUBLE_EQ(store[0][1], 2.2);
  EXPECT_DOUBLE_EQ(store[1][0], 3.3);
  EXPECT_DOUBLE_EQ(store[1][1], 4.4);

  // Verify new elements are zero-initialized
  for (size_t grp = 0; grp < store.num_groups(); ++grp) {
    auto span = store[grp];
    for (size_t i = 2; i < 10; ++i) {
      EXPECT_DOUBLE_EQ(span[i], 0.0) << "New element [" << grp << "][" << i << "] not zero-initialized";
    }
  }

  // Test using the full new capacity
  for (size_t grp = 0; grp < store.num_groups(); ++grp) {
    auto span = store[grp];
    for (size_t i = 2; i < 10; ++i) {
      span[i] = grp * 10.0 + i;
    }
  }

  // Verify we can access all elements
  for (size_t grp = 0; grp < store.num_groups(); ++grp) {
    auto span = store[grp];
    for (size_t i = 2; i < 10; ++i) {
      EXPECT_DOUBLE_EQ(span[i], grp * 10.0 + i);
    }
  }
}

// Test ensure_group_capacity with different data types
TEST_F(VectorStoreTest, EnsureGroupCapacityDifferentTypes) {
  // Test with struct
  struct Point {
    int x, y;
  };
  static_assert(std::is_trivial_v<Point>);

  vector_store<Point> store(2, 2);
  store[0][0] = {10, 20};
  store[0][1] = {30, 40};
  store[1][0] = {50, 60};
  store[1][1] = {70, 80};

  store.ensure_group_capacity(4);

  EXPECT_EQ(store.group_size(), 4);
  EXPECT_EQ(store.num_groups(), 2);

  // Verify original data
  EXPECT_EQ(store[0][0].x, 10);
  EXPECT_EQ(store[0][0].y, 20);
  EXPECT_EQ(store[0][1].x, 30);
  EXPECT_EQ(store[0][1].y, 40);
  EXPECT_EQ(store[1][0].x, 50);
  EXPECT_EQ(store[1][0].y, 60);
  EXPECT_EQ(store[1][1].x, 70);
  EXPECT_EQ(store[1][1].y, 80);

  // Verify new elements are zero-initialized
  EXPECT_EQ(store[0][2].x, 0);
  EXPECT_EQ(store[0][2].y, 0);
  EXPECT_EQ(store[0][3].x, 0);
  EXPECT_EQ(store[0][3].y, 0);
  EXPECT_EQ(store[1][2].x, 0);
  EXPECT_EQ(store[1][2].y, 0);
  EXPECT_EQ(store[1][3].x, 0);
  EXPECT_EQ(store[1][3].y, 0);
}

// Test ensure_group_capacity preserves memory layout properties
TEST_F(VectorStoreTest, EnsureGroupCapacityMemoryLayout) {
  vector_store<int> store(3, 3);

  // Fill with data
  for (size_t grp = 0; grp < store.num_groups(); ++grp) {
    auto span = store[grp];
    for (size_t i = 0; i < span.size(); ++i) {
      span[i] = static_cast<int>(grp * 100 + i);
    }
  }

  store.ensure_group_capacity(8);

  // Verify groups are still properly aligned and separated
  auto span0 = store[0];
  auto span1 = store[1];
  auto span2 = store[2];

  auto addr0 = reinterpret_cast<uintptr_t>(span0.data());
  auto addr1 = reinterpret_cast<uintptr_t>(span1.data());
  auto addr2 = reinterpret_cast<uintptr_t>(span2.data());

  // First group should be cacheline aligned
  EXPECT_EQ(addr0 % cacheline_size, 0);

  // Groups should be separated by at least cacheline_size
  EXPECT_GE(addr1 - addr0, cacheline_size);
  EXPECT_GE(addr2 - addr1, cacheline_size);

  // Verify stride is consistent
  EXPECT_EQ(addr1 - addr0, store.group_stride());
  EXPECT_EQ(addr2 - addr1, store.group_stride());

  // Verify data integrity
  for (size_t grp = 0; grp < store.num_groups(); ++grp) {
    auto span = store[grp];
    for (size_t i = 0; i < 3; ++i) { // Original data
      EXPECT_EQ(span[i], static_cast<int>(grp * 100 + i));
    }
  }
}

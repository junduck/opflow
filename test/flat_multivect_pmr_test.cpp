#include <gtest/gtest.h>
#include <memory_resource>
#include <vector>

#include "opflow/detail/flat_multivect.hpp"

using namespace opflow::detail;

TEST(FlatMultiVectPmrTest, ConstructFromStdAlloc) {
  // Create a normal allocator-backed flat_multivect
  flat_multivect<int> std_fmv;
  std::vector<int> a = {1, 2, 3};
  std::vector<int> b = {4, 5};
  std::vector<int> c = {6};
  std_fmv.push_back(a);
  std_fmv.push_back(b);
  std_fmv.push_back(c);

  // Use an upstream buffer for pmr
  std::pmr::monotonic_buffer_resource pool;

  // Construct a pmr-backed flat_multivect from the std-backed one
  flat_multivect<int, std::pmr::polymorphic_allocator<int>> pmr_fmv(std_fmv, &pool);

  // Verify sizes and contents
  EXPECT_EQ(pmr_fmv.size(), std_fmv.size());
  EXPECT_EQ(pmr_fmv.total_size(), std_fmv.total_size());

  for (size_t i = 0; i < pmr_fmv.size(); ++i) {
    auto s1 = std_fmv[i];
    auto s2 = pmr_fmv[i];
    EXPECT_EQ(s1.size(), s2.size());
    EXPECT_TRUE(std::equal(s1.begin(), s1.end(), s2.begin()));
  }
}

// A small counting memory_resource that delegates to an upstream resource
// and counts allocate/deallocate calls.
struct CountingResource : std::pmr::memory_resource {
  size_t alloc_calls = 0;
  size_t dealloc_calls = 0;
  std::pmr::memory_resource *upstream;

  explicit CountingResource(std::pmr::memory_resource *up = std::pmr::new_delete_resource()) noexcept : upstream(up) {}

private:
  void *do_allocate(size_t bytes, size_t alignment) override {
    ++alloc_calls;
    return upstream->allocate(bytes, alignment);
  }
  void do_deallocate(void *p, size_t bytes, size_t alignment) override {
    ++dealloc_calls;
    upstream->deallocate(p, bytes, alignment);
  }
  bool do_is_equal(const std::pmr::memory_resource &other) const noexcept override { return this == &other; }
};

TEST(FlatMultiVectPmrTest, ConstructFromPmrAllocToStd) {
  // Build a pmr-backed flat_multivect and then construct a std-backed one from it.
  std::pmr::monotonic_buffer_resource pool;
  std::pmr::polymorphic_allocator<int> pa(&pool);

  flat_multivect<int, std::pmr::polymorphic_allocator<int>> pmr_fmv(pa);
  pmr_fmv.push_back(std::vector<int>{7, 8, 9});
  pmr_fmv.push_back(std::vector<int>{10});

  // Construct default-allocator instance from pmr-backed one
  flat_multivect<int> std_fmv(pmr_fmv);

  EXPECT_EQ(std_fmv.size(), pmr_fmv.size());
  EXPECT_EQ(std_fmv.total_size(), pmr_fmv.total_size());
  for (size_t i = 0; i < std_fmv.size(); ++i) {
    auto s1 = std_fmv[i];
    auto s2 = pmr_fmv[i];
    EXPECT_EQ(s1.size(), s2.size());
    EXPECT_TRUE(std::equal(s1.begin(), s1.end(), s2.begin()));
  }
}

TEST(FlatMultiVectPmrTest, AllocationCounting) {
  // Create a small std-backed flat_multivect
  flat_multivect<int> std_fmv;
  std_fmv.push_back(std::vector<int>{1, 2, 3});
  std_fmv.push_back(std::vector<int>{4, 5});

  // Counting resource that wraps new_delete to actually perform allocations
  CountingResource counting_res(std::pmr::new_delete_resource());
  std::pmr::polymorphic_allocator<int> pa(&counting_res);

  size_t before = counting_res.alloc_calls;
  // Construct a pmr-backed flat_multivect from the std-backed one
  flat_multivect<int, std::pmr::polymorphic_allocator<int>> pmr_fmv(std_fmv, pa);

  // We expect the pmr-backed construction to allocate via the counting resource
  EXPECT_GT(counting_res.alloc_calls, before);
}

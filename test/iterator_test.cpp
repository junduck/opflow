#include <algorithm>
#include <gtest/gtest.h>
#include <iterator>
#include <memory>
#include <numeric>
#include <string>
#include <type_traits>
#include <vector>

#include "opflow/impl/iterator.hpp"

using namespace opflow::impl;

// Test container that returns references
class RefContainer {
private:
  std::vector<int> data_;

public:
  using iterator = iterator_t<RefContainer, false>;
  using const_iterator = iterator_t<RefContainer, true>;

  RefContainer(std::initializer_list<int> init) : data_(init) {}

  // Constructor for range
  template <typename Iterator>
  RefContainer(Iterator first, Iterator last) : data_(first, last) {}

  int &operator[](std::size_t index) { return data_[index]; }
  const int &operator[](std::size_t index) const { return data_[index]; }

  std::size_t size() const { return data_.size(); }

  iterator begin() { return iterator(this, 0); }
  iterator end() { return iterator(this, size()); }
  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, size()); }
  const_iterator cbegin() const { return const_iterator(this, 0); }
  const_iterator cend() const { return const_iterator(this, size()); }
};

// Test container that returns values (copies)
class ValueContainer {
private:
  std::vector<std::string> data_;

public:
  using iterator = iterator_t<ValueContainer, false>;
  using const_iterator = iterator_t<ValueContainer, true>;

  ValueContainer(std::initializer_list<std::string> init) : data_(init) {}

  std::string operator[](std::size_t index) { return data_[index]; }
  std::string operator[](std::size_t index) const { return data_[index]; }

  std::size_t size() const { return data_.size(); }

  iterator begin() { return iterator(this, 0); }
  iterator end() { return iterator(this, size()); }
  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, size()); }
};

// Test container with proxy objects (similar to std::vector<bool>)
class ProxyContainer {
private:
  std::vector<bool> data_;

public:
  using iterator = iterator_t<ProxyContainer, false>;
  using const_iterator = iterator_t<ProxyContainer, true>;

  ProxyContainer(std::initializer_list<bool> init) : data_(init) {}

  auto operator[](std::size_t index) -> decltype(data_[index]) { return data_[index]; }
  auto operator[](std::size_t index) const -> decltype(data_[index]) { return data_[index]; }

  std::size_t size() const { return data_.size(); }

  iterator begin() { return iterator(this, 0); }
  iterator end() { return iterator(this, size()); }
  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, size()); }
};

// Test container with struct objects to verify operator* and operator->
struct TestStruct {
  int value;
  std::string name;

  TestStruct(int v, const std::string &n) : value(v), name(n) {}

  bool operator==(const TestStruct &other) const { return value == other.value && name == other.name; }
};

class StructContainer {
private:
  std::vector<TestStruct> data_;

public:
  using iterator = iterator_t<StructContainer, false>;
  using const_iterator = iterator_t<StructContainer, true>;

  StructContainer(std::initializer_list<TestStruct> init) : data_(init) {}

  TestStruct &operator[](std::size_t index) { return data_[index]; }
  const TestStruct &operator[](std::size_t index) const { return data_[index]; }

  std::size_t size() const { return data_.size(); }

  iterator begin() { return iterator(this, 0); }
  iterator end() { return iterator(this, size()); }
  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, size()); }
  const_iterator cbegin() const { return const_iterator(this, 0); }
  const_iterator cend() const { return const_iterator(this, size()); }
};

class IteratorTest : public ::testing::Test {
protected:
  RefContainer ref_container{1, 2, 3, 4, 5};
  ValueContainer value_container{"a", "b", "c", "d", "e"};
  ProxyContainer proxy_container{true, false, true, false, true};
  StructContainer struct_container{{1, "first"}, {2, "second"}, {3, "third"}, {4, "fourth"}, {5, "fifth"}};
};

// Test basic iterator properties
TEST_F(IteratorTest, BasicIteratorTraits) {
  using Iterator = RefContainer::iterator;
  using ConstIterator = RefContainer::const_iterator;

  // Check iterator traits
  EXPECT_TRUE((std::is_same_v<Iterator::iterator_category, std::random_access_iterator_tag>));
  EXPECT_TRUE((std::is_same_v<Iterator::value_type, int>));
  EXPECT_TRUE((std::is_same_v<Iterator::reference, int &>));
  EXPECT_TRUE((std::is_same_v<Iterator::pointer, int *>));
  EXPECT_TRUE((std::is_same_v<Iterator::difference_type, std::ptrdiff_t>));

  // Check const iterator traits
  EXPECT_TRUE((std::is_same_v<ConstIterator::iterator_category, std::random_access_iterator_tag>));
  EXPECT_TRUE((std::is_same_v<ConstIterator::value_type, int>));
  EXPECT_TRUE((std::is_same_v<ConstIterator::reference, const int &>));
  EXPECT_TRUE((std::is_same_v<ConstIterator::pointer, const int *>));

  // Check struct iterator traits to ensure operator-> works correctly
  using StructIterator = StructContainer::iterator;
  using ConstStructIterator = StructContainer::const_iterator;

  EXPECT_TRUE((std::is_same_v<StructIterator::iterator_category, std::random_access_iterator_tag>));
  EXPECT_TRUE((std::is_same_v<StructIterator::value_type, TestStruct>));
  EXPECT_TRUE((std::is_same_v<StructIterator::reference, TestStruct &>));
  EXPECT_TRUE((std::is_same_v<StructIterator::pointer, TestStruct *>));

  EXPECT_TRUE((std::is_same_v<ConstStructIterator::value_type, TestStruct>));
  EXPECT_TRUE((std::is_same_v<ConstStructIterator::reference, const TestStruct &>));
  EXPECT_TRUE((std::is_same_v<ConstStructIterator::pointer, const TestStruct *>));
}

// Test iterator construction and conversion
TEST_F(IteratorTest, ConstructionAndConversion) {
  auto it = ref_container.begin();
  auto cit = ref_container.cbegin();

  // Test default construction
  RefContainer::iterator default_it;
  RefContainer::const_iterator default_cit;

  // Test copy construction
  auto it_copy = it;
  auto cit_copy = cit;
  EXPECT_EQ(it, it_copy);
  EXPECT_EQ(cit, cit_copy);

  // Test non-const to const conversion
  RefContainer::const_iterator converted(it);
  EXPECT_EQ(*converted, *it);

  // Test that const to non-const conversion is not allowed (should not compile)
  // RefContainer::iterator invalid(cit); // This should not compile
}

// Test dereferencing and member access
TEST_F(IteratorTest, DereferenceAndMemberAccess) {
  auto it = ref_container.begin();
  EXPECT_EQ(*it, 1);

  // Test that value iterator works (returns copies)
  auto value_it = value_container.begin();
  EXPECT_EQ(*value_it, "a");

  // Test modification through reference
  *it = 42;
  EXPECT_EQ(*it, 42);
  EXPECT_EQ(ref_container[0], 42);
  *it = 1; // restore
}

// Test increment and decrement operators
TEST_F(IteratorTest, IncrementDecrement) {
  auto it = ref_container.begin();

  // Test pre-increment
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 2);

  // Test post-increment
  auto old_it = it++;
  EXPECT_EQ(*old_it, 2);
  EXPECT_EQ(*it, 3);

  // Test pre-decrement
  --it;
  EXPECT_EQ(*it, 2);

  // Test post-decrement
  old_it = it--;
  EXPECT_EQ(*old_it, 2);
  EXPECT_EQ(*it, 1);
}

// Test arithmetic operators
TEST_F(IteratorTest, ArithmeticOperators) {
  auto it = ref_container.begin();

  // Test operator+=
  it += 2;
  EXPECT_EQ(*it, 3);

  // Test operator-=
  it -= 1;
  EXPECT_EQ(*it, 2);

  // Test operator+
  auto it2 = it + 2;
  EXPECT_EQ(*it2, 4);
  EXPECT_EQ(*it, 2); // original unchanged

  // Test operator-
  auto it3 = it2 - 1;
  EXPECT_EQ(*it3, 3);

  // Test n + iterator
  auto it4 = 3 + ref_container.begin();
  EXPECT_EQ(*it4, 4);

  // Test iterator difference
  auto diff = it2 - it;
  EXPECT_EQ(diff, 2);
}

// Test subscript operator
TEST_F(IteratorTest, SubscriptOperator) {
  auto it = ref_container.begin();

  EXPECT_EQ(it[0], 1);
  EXPECT_EQ(it[1], 2);
  EXPECT_EQ(it[2], 3);
  EXPECT_EQ(it[3], 4);
  EXPECT_EQ(it[4], 5);

  // Test with offset iterator
  auto it2 = it + 2;
  EXPECT_EQ(it2[0], 3);
  EXPECT_EQ(it2[1], 4);
  EXPECT_EQ(it2[-1], 2);
  EXPECT_EQ(it2[-2], 1);
}

// Test comparison operators
TEST_F(IteratorTest, ComparisonOperators) {
  auto it1 = ref_container.begin();
  auto it2 = ref_container.begin() + 2;
  auto it3 = ref_container.begin() + 2;

  // Test equality
  EXPECT_EQ(it2, it3);
  EXPECT_NE(it1, it2);

  // Test ordering
  EXPECT_LT(it1, it2);
  EXPECT_LE(it1, it2);
  EXPECT_LE(it2, it3);
  EXPECT_GT(it2, it1);
  EXPECT_GE(it2, it1);
  EXPECT_GE(it3, it2);
}

// Test with different container types
TEST_F(IteratorTest, ValueContainerSupport) {
  auto it = value_container.begin();

  EXPECT_EQ(*it, "a");
  ++it;
  EXPECT_EQ(*it, "b");

  auto it2 = it + 2;
  EXPECT_EQ(*it2, "d");

  auto diff = it2 - it;
  EXPECT_EQ(diff, 2);
}

TEST_F(IteratorTest, ProxyContainerSupport) {
  auto it = proxy_container.begin();

  EXPECT_EQ(*it, true); // index 0: true
  ++it;
  EXPECT_EQ(*it, false); // index 1: false

  auto it2 = it + 2; // index 3: false (not true!)
  EXPECT_EQ(*it2, false);

  // Test assignment through proxy
  *it = true;
  EXPECT_EQ(*it, true);
}

// Test iterator with struct objects to verify operator* and operator->
TEST_F(IteratorTest, StructContainerSupport) {
  auto it = struct_container.begin();

  // Test operator* (dereference)
  TestStruct &first_struct = *it;
  EXPECT_EQ(first_struct.value, 1);
  EXPECT_EQ(first_struct.name, "first");

  // Test operator-> (member access)
  EXPECT_EQ(it->value, 1);
  EXPECT_EQ(it->name, "first");

  // Test modification through operator*
  (*it).value = 42;
  EXPECT_EQ(it->value, 42);
  EXPECT_EQ(struct_container[0].value, 42);
  (*it).value = 1; // restore

  // Test modification through operator->
  it->name = "modified";
  EXPECT_EQ(it->name, "modified");
  EXPECT_EQ(struct_container[0].name, "modified");
  it->name = "first"; // restore

  // Test with iterator arithmetic
  ++it;
  EXPECT_EQ(it->value, 2);
  EXPECT_EQ(it->name, "second");

  auto it2 = it + 2;
  EXPECT_EQ(it2->value, 4);
  EXPECT_EQ(it2->name, "fourth");

  // Test subscript with arrow operator
  auto it3 = struct_container.begin();
  EXPECT_EQ(it3[0].value, 1);
  EXPECT_EQ(it3[0].name, "first");
  EXPECT_EQ(it3[2].value, 3);
  EXPECT_EQ(it3[2].name, "third");
}

// Test const iterator with struct objects
TEST_F(IteratorTest, ConstStructIterator) {
  const StructContainer &const_struct_ref = struct_container;
  auto cit = const_struct_ref.begin();

  // Test const operator* and operator->
  EXPECT_EQ(cit->value, 1);
  EXPECT_EQ(cit->name, "first");

  const TestStruct &const_struct = *cit;
  EXPECT_EQ(const_struct.value, 1);
  EXPECT_EQ(const_struct.name, "first");

  // Test that we can't modify through const iterator
  // cit->value = 42; // This should not compile
  // (*cit).value = 42; // This should not compile

  ++cit;
  EXPECT_EQ(cit->value, 2);
  EXPECT_EQ(cit->name, "second");
}

// Test that operator-> returns proper pointer for address operations
TEST_F(IteratorTest, PointerSemantics) {
  auto it = struct_container.begin();

  // Test that operator-> returns actual pointer
  TestStruct *ptr1 = &(*it);
  TestStruct *ptr2 = it.operator->();
  EXPECT_EQ(ptr1, ptr2);

  // Test pointer arithmetic equivalence
  EXPECT_EQ(&it[0], ptr1);
  EXPECT_EQ(&it[1], &struct_container[1]);

  // Test that we can take address of members
  int *value_ptr = &(it->value);
  std::string *name_ptr = &(it->name);

  EXPECT_EQ(*value_ptr, 1);
  EXPECT_EQ(*name_ptr, "first");

  // Modify through pointer and verify
  *value_ptr = 99;
  EXPECT_EQ(it->value, 99);
  EXPECT_EQ(struct_container[0].value, 99);
  *value_ptr = 1; // restore
}

// Test iterator with standard algorithms
TEST_F(IteratorTest, StandardAlgorithmCompatibility) {
  // Test with std::distance
  auto distance = std::distance(ref_container.begin(), ref_container.end());
  EXPECT_EQ(distance, 5);

  // Test with std::advance
  auto it = ref_container.begin();
  std::advance(it, 3);
  EXPECT_EQ(*it, 4);

  // Test with std::find
  auto found = std::find(ref_container.begin(), ref_container.end(), 3);
  EXPECT_NE(found, ref_container.end());
  EXPECT_EQ(*found, 3);

  // Test with std::sort (modify container first)
  RefContainer sort_container{5, 1, 4, 2, 3};
  std::sort(sort_container.begin(), sort_container.end());
  EXPECT_EQ(sort_container[0], 1);
  EXPECT_EQ(sort_container[1], 2);
  EXPECT_EQ(sort_container[2], 3);
  EXPECT_EQ(sort_container[3], 4);
  EXPECT_EQ(sort_container[4], 5);

  // Test algorithms with struct container
  auto struct_found =
      std::find_if(struct_container.begin(), struct_container.end(), [](const TestStruct &s) { return s.value == 3; });
  EXPECT_NE(struct_found, struct_container.end());
  EXPECT_EQ(struct_found->value, 3);
  EXPECT_EQ(struct_found->name, "third");

  // Test std::transform with struct container
  std::vector<int> values;
  std::transform(struct_container.begin(), struct_container.end(), std::back_inserter(values),
                 [](const TestStruct &s) { return s.value; });
  EXPECT_EQ(values.size(), 5);
  EXPECT_EQ(values[0], 1);
  EXPECT_EQ(values[4], 5);
}

// Test edge cases and error conditions
TEST_F(IteratorTest, EdgeCases) {
  RefContainer empty_container{};

  // Test empty container
  EXPECT_EQ(empty_container.begin(), empty_container.end());

  // Test iterator arithmetic at boundaries
  auto it = ref_container.begin();
  auto end_it = it + 5;
  EXPECT_EQ(end_it, ref_container.end());

  auto back_it = end_it - 5;
  EXPECT_EQ(back_it, ref_container.begin());
}

// Test const correctness
TEST_F(IteratorTest, ConstCorrectness) {
  const RefContainer &const_ref = ref_container;

  auto cit = const_ref.begin();
  EXPECT_EQ(*cit, 1);

  // Test that we can't modify through const iterator
  // *cit = 42; // This should not compile

  // Test iterator comparison between const and non-const
  auto it = ref_container.begin();
  EXPECT_EQ(it, cit);
  EXPECT_EQ(cit, it);
}

// Test iterator invalidation scenarios
TEST_F(IteratorTest, IteratorFromDifferentContainers) {
  RefContainer other_container{10, 20, 30};

  auto it1 = ref_container.begin();
  auto it2 = other_container.begin();

  // Test that iterators from different containers are different
  EXPECT_NE(it1, it2); // They should not be equal

  // These assertions should trigger in debug mode with our assert checks
  EXPECT_DEATH(it1 - it2, "different containers");
  // Note: Commented out death tests as they may not be enabled in all builds
}

// Performance test - ensure iterator operations are efficient
TEST_F(IteratorTest, PerformanceCharacteristics) {
  // Create a container with many elements
  std::vector<int> large_data(10000);
  std::iota(large_data.begin(), large_data.end(), 0);

  RefContainer large_ref_container(large_data.begin(), large_data.end());

  auto start = large_ref_container.begin();
  auto end = large_ref_container.end();

  // Random access should be O(1)
  auto middle = start + 5000;
  EXPECT_EQ(*middle, 5000);

  // Distance calculation should be O(1)
  auto dist = end - start;
  EXPECT_EQ(dist, 10000);
}

// Test iterator with move-only types
class MoveOnlyContainer {
private:
  std::vector<std::unique_ptr<int>> data_;

public:
  using iterator = iterator_t<MoveOnlyContainer, false>;
  using const_iterator = iterator_t<MoveOnlyContainer, true>;

  MoveOnlyContainer() {
    for (int i = 1; i <= 5; ++i) {
      data_.push_back(std::make_unique<int>(i));
    }
  }

  std::unique_ptr<int> &operator[](std::size_t index) { return data_[index]; }
  const std::unique_ptr<int> &operator[](std::size_t index) const { return data_[index]; }

  std::size_t size() const { return data_.size(); }

  iterator begin() { return iterator(this, 0); }
  iterator end() { return iterator(this, size()); }
  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, size()); }
};

TEST_F(IteratorTest, MoveOnlyTypes) {
  MoveOnlyContainer move_container;

  auto it = move_container.begin();
  EXPECT_EQ(**it, 1);

  ++it;
  EXPECT_EQ(**it, 2);

  auto it2 = it + 2;
  EXPECT_EQ(**it2, 4);
}

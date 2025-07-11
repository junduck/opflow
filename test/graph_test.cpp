#include "opflow/graph.hpp"
#include "gtest/gtest.h"
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>

using namespace opflow;

class TopologicalSorterTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(TopologicalSorterTest, EmptyGraph) {
  TopologicalSorter<int> sorter;
  EXPECT_TRUE(sorter.empty());
  EXPECT_EQ(sorter.size(), 0);

  auto result = sorter.static_order();
  EXPECT_TRUE(result.empty());
}

TEST_F(TopologicalSorterTest, SingleNode) {
  TopologicalSorter<int> sorter;
  sorter.add(42);

  EXPECT_FALSE(sorter.empty());
  EXPECT_EQ(sorter.size(), 1);
  EXPECT_TRUE(sorter.contains(42));

  auto result = sorter.static_order();
  EXPECT_EQ(result.size(), 1);
  EXPECT_EQ(result[0], 42);
}

TEST_F(TopologicalSorterTest, LinearChain) {
  TopologicalSorter<int> sorter;
  // Create chain: 1 -> 2 -> 3 -> 4
  sorter.add(1);
  sorter.add(2, {1});
  sorter.add(3, {2});
  sorter.add(4, {3});

  auto result = sorter.static_order();
  EXPECT_EQ(result.size(), 4);
  EXPECT_EQ(result[0], 1);
  EXPECT_EQ(result[1], 2);
  EXPECT_EQ(result[2], 3);
  EXPECT_EQ(result[3], 4);
}

TEST_F(TopologicalSorterTest, DiamondDependency) {
  TopologicalSorter<char> sorter;
  // Create diamond: A -> B,C -> D
  sorter.add('A');
  sorter.add('B', {'A'});
  sorter.add('C', {'A'});
  sorter.add('D', {'B', 'C'});

  auto result = sorter.static_order();
  EXPECT_EQ(result.size(), 4);
  EXPECT_EQ(result[0], 'A');
  EXPECT_EQ(result[3], 'D');

  // B and C can be in any order, but both must come after A and before D
  EXPECT_TRUE((result[1] == 'B' && result[2] == 'C') || (result[1] == 'C' && result[2] == 'B'));
}

TEST_F(TopologicalSorterTest, CycleDetection) {
  TopologicalSorter<int> sorter;
  // Create cycle: 1 -> 2 -> 3 -> 1
  sorter.add(1, {3});
  sorter.add(2, {1});
  sorter.add(3, {2});

  EXPECT_THROW(sorter.static_order(), CycleError);
}

TEST_F(TopologicalSorterTest, SelfCycle) {
  TopologicalSorter<int> sorter;
  // Self-referencing node
  sorter.add(1, {1});

  EXPECT_THROW(sorter.static_order(), CycleError);
}

TEST_F(TopologicalSorterTest, StringNodes) {
  TopologicalSorter<std::string> sorter;
  // Package dependencies example
  sorter.add("app", {"logging", "database"});
  sorter.add("logging", {"utils"});
  sorter.add("database", {"utils"});
  sorter.add("utils");

  auto result = sorter.static_order();
  EXPECT_EQ(result.size(), 4);
  EXPECT_EQ(result[0], "utils");
  EXPECT_EQ(result[3], "app");

  // logging and database can be in any order
  EXPECT_TRUE((result[1] == "logging" && result[2] == "database") ||
              (result[1] == "database" && result[2] == "logging"));
}

TEST_F(TopologicalSorterTest, PrepareAndIterate) {
  TopologicalSorter<int> sorter;
  sorter.add(1);
  sorter.add(2, {1});
  sorter.add(3, {1});
  sorter.add(4, {2, 3});

  sorter.prepare();
  EXPECT_FALSE(sorter.done());

  // First iteration - only node 1 should be ready
  auto ready = sorter.get_ready();
  EXPECT_EQ(ready.size(), 1);
  EXPECT_EQ(ready[0], 1);
  EXPECT_FALSE(sorter.done());

  sorter.mark_done({1});

  // Second iteration - nodes 2 and 3 should be ready
  ready = sorter.get_ready();
  EXPECT_EQ(ready.size(), 2);
  std::set<int> ready_set(ready.begin(), ready.end());
  EXPECT_TRUE(ready_set.count(2) && ready_set.count(3));
  EXPECT_FALSE(sorter.done());

  sorter.mark_done({2, 3});

  // Third iteration - node 4 should be ready
  ready = sorter.get_ready();
  EXPECT_EQ(ready.size(), 1);
  EXPECT_EQ(ready[0], 4);
  EXPECT_FALSE(sorter.done());

  sorter.mark_done({4});
  EXPECT_TRUE(sorter.done());
}

TEST_F(TopologicalSorterTest, GetReadyWithLimit) {
  TopologicalSorter<int> sorter;
  sorter.add(1);
  sorter.add(2);
  sorter.add(3);
  sorter.add(4, {1, 2, 3});

  sorter.prepare();

  // Get only 2 nodes at a time
  auto ready = sorter.get_ready(2);
  EXPECT_EQ(ready.size(), 2);

  // Get the remaining node
  ready = sorter.get_ready(2);
  EXPECT_EQ(ready.size(), 1);
}

TEST_F(TopologicalSorterTest, InvalidOperations) {
  TopologicalSorter<int> sorter;
  sorter.add(1);

  // Should throw when calling done() before prepare()
  EXPECT_THROW(sorter.done(), std::runtime_error);

  // Should throw when calling get_ready() before prepare()
  EXPECT_THROW(sorter.get_ready(), std::runtime_error);

  // Should throw when calling mark_done() before prepare()
  EXPECT_THROW(sorter.mark_done({1}), std::runtime_error);

  sorter.prepare();

  // Should throw when calling prepare() again
  EXPECT_THROW(sorter.prepare(), std::runtime_error);

  // Should throw when calling static_order() after prepare()
  EXPECT_THROW(sorter.static_order(), std::runtime_error);
}

TEST_F(TopologicalSorterTest, ConvenienceFunction) {
  std::unordered_map<std::string, std::unordered_set<std::string>> graph = {
      {"compile", {"source"}}, {"link", {"compile"}}, {"test", {"link"}}, {"source", {}}};

  auto result = topological_sort(graph);
  EXPECT_EQ(result.size(), 4);
  EXPECT_EQ(result[0], "source");
  EXPECT_EQ(result[1], "compile");
  EXPECT_EQ(result[2], "link");
  EXPECT_EQ(result[3], "test");
}

TEST_F(TopologicalSorterTest, NodeQueries) {
  TopologicalSorter<int> sorter;
  sorter.add(1);
  sorter.add(2, {1});
  sorter.add(3, {1, 2});

  using NodeSet = TopologicalSorter<int>::NodeSet;
  // Test dependencies
  auto deps_it = sorter.dependencies(3);
  NodeSet deps(deps_it.begin(), deps_it.end());
  EXPECT_EQ(deps.size(), 2);
  EXPECT_TRUE(deps.count(1) && deps.count(2));

  // Test successors
  auto succ_it = sorter.successors(1);
  NodeSet succ(succ_it.begin(), succ_it.end());
  EXPECT_EQ(succ.size(), 2);
  EXPECT_TRUE(succ.count(2) && succ.count(3));

  // Test non-existent node
  auto no_deps_it = sorter.dependencies(999);
  EXPECT_EQ(std::ranges::size(no_deps_it), 0);

  auto nodes = sorter.nodes();
  EXPECT_EQ(nodes.size(), 3);
  std::set<int> node_set(nodes.begin(), nodes.end());
  EXPECT_TRUE(node_set.count(1) && node_set.count(2) && node_set.count(3));
}

TEST_F(TopologicalSorterTest, ClearGraph) {
  TopologicalSorter<int> sorter;
  sorter.add(1);
  sorter.add(2, {1});

  EXPECT_FALSE(sorter.empty());
  EXPECT_EQ(sorter.size(), 2);

  sorter.clear();

  EXPECT_TRUE(sorter.empty());
  EXPECT_EQ(sorter.size(), 0);

  // Should be able to use after clearing
  sorter.add(3);
  auto result = sorter.static_order();
  EXPECT_EQ(result.size(), 1);
  EXPECT_EQ(result[0], 3);
}

TEST_F(TopologicalSorterTest, ComplexGraph) {
  TopologicalSorter<int> sorter;

  // Create a more complex dependency graph
  sorter.add(1);         // No dependencies
  sorter.add(2);         // No dependencies
  sorter.add(3, {1});    // Depends on 1
  sorter.add(4, {1});    // Depends on 1
  sorter.add(5, {2});    // Depends on 2
  sorter.add(6, {3, 4}); // Depends on 3 and 4
  sorter.add(7, {5});    // Depends on 5
  sorter.add(8, {6, 7}); // Depends on 6 and 7

  auto result = sorter.static_order();
  EXPECT_EQ(result.size(), 8);

  // Verify partial ordering constraints
  auto pos = [&result](int node) { return std::find(result.begin(), result.end(), node) - result.begin(); };

  // Check all dependency constraints are satisfied
  EXPECT_LT(pos(1), pos(3));
  EXPECT_LT(pos(1), pos(4));
  EXPECT_LT(pos(2), pos(5));
  EXPECT_LT(pos(3), pos(6));
  EXPECT_LT(pos(4), pos(6));
  EXPECT_LT(pos(5), pos(7));
  EXPECT_LT(pos(6), pos(8));
  EXPECT_LT(pos(7), pos(8));
}

#include "gtest/gtest.h"
#include <algorithm>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "opflow/topo.hpp"

using namespace opflow;

// Test fixture for topological_sorter tests
class TopologicalSorterTest : public ::testing::Test {
protected:
  topological_sorter<int> int_sorter;
  topological_sorter<std::string> string_sorter;

  void SetUp() override {
    // Fresh sorters for each test
    int_sorter.clear();
    string_sorter.clear();
  }
};

// Helper function to check if a vector contains all expected elements
template <typename T>
bool contains_all(const std::vector<T> &vec, const std::vector<T> &expected) {
  std::set<T> vec_set(vec.begin(), vec.end());
  std::set<T> expected_set(expected.begin(), expected.end());
  return vec_set == expected_set;
}

// Enhanced helper function to validate topological order
template <typename T>
bool is_valid_topological_order(const std::vector<T> &order, const topological_sorter<T> &sorter) {
  if (order.empty()) {
    return sorter.empty(); // An empty order is valid if the sorter is empty
  }

  // Check if all nodes from sorter are in the order
  std::unordered_set<T> order_set(order.begin(), order.end());
  auto nodes_view = sorter.nodes();
  std::unordered_set<T> sorter_nodes(nodes_view.begin(), nodes_view.end());

  if (order_set != sorter_nodes) {
    return false; // Different sets of nodes
  }

  // Check for duplicates in order
  if (order_set.size() != order.size()) {
    return false; // Duplicates in order
  }

  // Build position map
  std::unordered_map<T, size_t> position;
  for (size_t i = 0; i < order.size(); ++i) {
    position[order[i]] = i;
  }

  // Validate topological constraint: dependencies come before dependents
  for (const auto &node : order) {
    for (const auto &dep : sorter.pred_of(node)) {
      if (position.find(dep) == position.end() || position[dep] >= position[node]) {
        return false; // Dependency missing or appears after dependent
      }
    }
  }
  return true;
}

// Helper to check if a graph has cycles by attempting to sort
template <typename T>
bool has_cycle(const topological_sorter<T> &sorter) {
  return sorter.sort().empty() && !sorter.empty();
}

// Test graph builder for programmatic test generation
template <typename T>
class TestGraphBuilder {
private:
  topological_sorter<T> sorter_;
  std::vector<T> nodes_;
  bool has_expected_cycle_ = false;

public:
  TestGraphBuilder() = default;

  // Add a chain of dependencies: last -> ... -> second -> first
  TestGraphBuilder &add_chain(const std::vector<T> &chain) {
    if (chain.empty())
      return *this;

    for (size_t i = 0; i < chain.size(); ++i) {
      nodes_.push_back(chain[i]);
      if (i == 0) {
        sorter_.add_vertex(chain[i]);
      } else {
        sorter_.add_vertex(chain[i], std::vector<T>{chain[i - 1]});
      }
    }
    return *this;
  }

  // Add a diamond pattern: top depends on left,right; left,right depend on bottom
  TestGraphBuilder &add_diamond(const T &top, const T &left, const T &right, const T &bottom) {
    nodes_.insert(nodes_.end(), {top, left, right, bottom});
    sorter_.add_vertex(bottom);
    sorter_.add_vertex(left, std::vector<T>{bottom});
    sorter_.add_vertex(right, std::vector<T>{bottom});
    sorter_.add_vertex(top, std::vector<T>{left, right});
    return *this;
  }

  // Add a star pattern: center depends on all points
  TestGraphBuilder &add_star(const T &center, const std::vector<T> &points) {
    nodes_.push_back(center);
    nodes_.insert(nodes_.end(), points.begin(), points.end());

    for (const auto &point : points) {
      sorter_.add_vertex(point);
    }
    sorter_.add_vertex(center, points);
    return *this;
  }

  // Add a cycle (marks as expected to have cycle)
  TestGraphBuilder &add_cycle(const std::vector<T> &cycle_nodes) {
    if (cycle_nodes.size() < 2)
      return *this;

    has_expected_cycle_ = true;
    nodes_.insert(nodes_.end(), cycle_nodes.begin(), cycle_nodes.end());

    for (size_t i = 0; i < cycle_nodes.size(); ++i) {
      size_t next = (i + 1) % cycle_nodes.size();
      sorter_.add_vertex(cycle_nodes[i], std::vector<T>{cycle_nodes[next]});
    }
    return *this;
  }

  // Add isolated nodes (no dependencies)
  TestGraphBuilder &add_isolated(const std::vector<T> &isolated) {
    nodes_.insert(nodes_.end(), isolated.begin(), isolated.end());
    for (const auto &node : isolated) {
      sorter_.add_vertex(node);
    }
    return *this;
  }

  // Get the built sorter
  const topological_sorter<T> &get_sorter() const { return sorter_; }

  // Get all nodes that were added
  const std::vector<T> &get_nodes() const { return nodes_; }

  // Whether this graph is expected to have a cycle
  bool expects_cycle() const { return has_expected_cycle_; }

  // Validate the built graph
  bool validate() const {
    auto result = sorter_.sort();

    if (expects_cycle()) {
      return result.empty(); // Should fail to sort due to cycle
    } else {
      return is_valid_topological_order(result, sorter_) && contains_all(result, nodes_);
    }
  }
};

// Basic functionality tests
TEST_F(TopologicalSorterTest, DefaultConstructor) {
  EXPECT_TRUE(int_sorter.empty());
  EXPECT_EQ(int_sorter.size(), 0);
  EXPECT_TRUE(int_sorter.sort().empty());
}

TEST_F(TopologicalSorterTest, AddSingleVertex) {
  int_sorter.add_vertex(1);

  EXPECT_FALSE(int_sorter.empty());
  EXPECT_EQ(int_sorter.size(), 1);
  EXPECT_TRUE(int_sorter.contains(1));

  auto result = int_sorter.sort();
  EXPECT_EQ(result.size(), 1);
  EXPECT_EQ(result[0], 1);
}

TEST_F(TopologicalSorterTest, AddVertexWithDependencies) {
  std::vector<int> deps = {2, 3};
  int_sorter.add_vertex(1, deps);

  EXPECT_EQ(int_sorter.size(), 3);
  EXPECT_TRUE(int_sorter.contains(1));
  EXPECT_TRUE(int_sorter.contains(2));
  EXPECT_TRUE(int_sorter.contains(3));

  const auto &node_deps = int_sorter.pred_of(1);
  EXPECT_EQ(node_deps.size(), 2);
  EXPECT_EQ(std::ranges::count(node_deps, 2), 1);
  EXPECT_EQ(std::ranges::count(node_deps, 3), 1);
}

TEST_F(TopologicalSorterTest, AddVertexWithEmptyDependencies) {
  std::vector<int> empty_deps;
  int_sorter.add_vertex(1, empty_deps);

  EXPECT_EQ(int_sorter.size(), 1);
  EXPECT_TRUE(int_sorter.contains(1));
  EXPECT_TRUE(int_sorter.pred_of(1).empty());
}

// Edge management tests
TEST_F(TopologicalSorterTest, AddEdges) {
  int_sorter.add_vertex(1);
  int_sorter.add_vertex(2);
  int_sorter.add_vertex(3);

  std::vector<int> deps = {2, 3};
  int_sorter.add_vertex(1, deps);

  const auto &node_deps = int_sorter.pred_of(1);
  EXPECT_EQ(node_deps.size(), 2);
  EXPECT_EQ(std::ranges::count(node_deps, 2), 1);
  EXPECT_EQ(std::ranges::count(node_deps, 3), 1);

  const auto &dependents_2 = int_sorter.succ_of(2);
  EXPECT_EQ(std::ranges::count(dependents_2, 1), 1);

  const auto &dependents_3 = int_sorter.succ_of(3);
  EXPECT_EQ(std::ranges::count(dependents_3, 1), 1);
}

TEST_F(TopologicalSorterTest, AddEdgeToNonExistentNode) {
  std::vector<int> deps = {2, 3};
  int_sorter.add_vertex(1, deps); // Node 1 doesn't exist yet

  EXPECT_EQ(int_sorter.size(), 3);
  EXPECT_TRUE(int_sorter.contains(1));
  EXPECT_TRUE(int_sorter.contains(2));
  EXPECT_TRUE(int_sorter.contains(3));
}

TEST_F(TopologicalSorterTest, RemoveEdges) {
  std::vector<int> deps = {2, 3, 4};
  int_sorter.add_vertex(1, deps);

  std::vector<int> to_remove = {2, 3};
  int_sorter.rm_edge(1, to_remove);

  const auto &node_deps = int_sorter.pred_of(1);
  EXPECT_EQ(node_deps.size(), 1);
  EXPECT_EQ(std::ranges::count(node_deps, 4), 1);
  EXPECT_EQ(std::ranges::count(node_deps, 2), 0);
  EXPECT_EQ(std::ranges::count(node_deps, 3), 0);

  // Check reverse dependencies
  EXPECT_TRUE(int_sorter.succ_of(2).empty());
  EXPECT_TRUE(int_sorter.succ_of(3).empty());
  EXPECT_EQ(std::ranges::count(int_sorter.succ_of(4), 1), 1);
}

TEST_F(TopologicalSorterTest, RemoveEdgeFromNonExistentNode) {
  std::vector<int> deps = {2, 3};
  int_sorter.rm_edge(999, deps); // Should not crash
  EXPECT_TRUE(int_sorter.empty());
}

// Vertex removal tests
TEST_F(TopologicalSorterTest, RemoveVertex) {
  // Create a small graph: 1 -> 2 -> 3
  int_sorter.add_vertex(3);
  int_sorter.add_vertex(2, std::vector<int>{3});
  int_sorter.add_vertex(1, std::vector<int>{2});

  EXPECT_EQ(int_sorter.size(), 3);

  // Remove vertex 2
  int_sorter.rm_vertex(2);

  EXPECT_EQ(int_sorter.size(), 2);
  EXPECT_FALSE(int_sorter.contains(2));
  EXPECT_TRUE(int_sorter.contains(1));
  EXPECT_TRUE(int_sorter.contains(3));

  // Check that dependencies are cleaned up
  EXPECT_TRUE(int_sorter.pred_of(1).empty());
  EXPECT_TRUE(int_sorter.succ_of(3).empty());
}

TEST_F(TopologicalSorterTest, RemoveNonExistentVertex) {
  int_sorter.add_vertex(1);
  int_sorter.rm_vertex(999); // Should not crash

  EXPECT_EQ(int_sorter.size(), 1);
  EXPECT_TRUE(int_sorter.contains(1));
}

// Topological sorting tests
TEST_F(TopologicalSorterTest, SortEmptyGraph) {
  auto result = int_sorter.sort();
  EXPECT_TRUE(result.empty());
}

TEST_F(TopologicalSorterTest, SortSingleNode) {
  int_sorter.add_vertex(42);
  auto result = int_sorter.sort();

  EXPECT_EQ(result.size(), 1);
  EXPECT_EQ(result[0], 42);
}

TEST_F(TopologicalSorterTest, SortLinearChain) {
  // Create chain: 1 -> 2 -> 3 -> 4
  int_sorter.add_vertex(4);
  int_sorter.add_vertex(3, std::vector<int>{4});
  int_sorter.add_vertex(2, std::vector<int>{3});
  int_sorter.add_vertex(1, std::vector<int>{2});

  auto result = int_sorter.sort();

  EXPECT_EQ(result.size(), 4);
  EXPECT_TRUE(is_valid_topological_order(result, int_sorter));
  EXPECT_TRUE(contains_all(result, {1, 2, 3, 4}));

  // In this specific case, we can check exact order
  auto it1 = std::find(result.begin(), result.end(), 1);
  auto it2 = std::find(result.begin(), result.end(), 2);
  auto it3 = std::find(result.begin(), result.end(), 3);
  auto it4 = std::find(result.begin(), result.end(), 4);

  EXPECT_TRUE(it4 < it3);
  EXPECT_TRUE(it3 < it2);
  EXPECT_TRUE(it2 < it1);
}

TEST_F(TopologicalSorterTest, SortDiamondDependency) {
  // Create diamond: 1 depends on 2,3; 2,3 depend on 4
  int_sorter.add_vertex(4);
  int_sorter.add_vertex(3, std::vector<int>{4});
  int_sorter.add_vertex(2, std::vector<int>{4});
  int_sorter.add_vertex(1, std::vector<int>{2, 3});

  auto result = int_sorter.sort();

  EXPECT_EQ(result.size(), 4);
  EXPECT_TRUE(is_valid_topological_order(result, int_sorter));
  EXPECT_TRUE(contains_all(result, {1, 2, 3, 4}));
}

TEST_F(TopologicalSorterTest, SortComplexGraph) {
  // More complex dependency graph
  int_sorter.add_vertex(7);
  int_sorter.add_vertex(6);
  int_sorter.add_vertex(5, std::vector<int>{6});
  int_sorter.add_vertex(4, std::vector<int>{6});
  int_sorter.add_vertex(3, std::vector<int>{4, 5});
  int_sorter.add_vertex(2, std::vector<int>{5, 7});
  int_sorter.add_vertex(1, std::vector<int>{2, 3});

  auto result = int_sorter.sort();

  EXPECT_EQ(result.size(), 7);
  EXPECT_TRUE(is_valid_topological_order(result, int_sorter));
  EXPECT_TRUE(contains_all(result, {1, 2, 3, 4, 5, 6, 7}));
}

TEST_F(TopologicalSorterTest, SortWithCycle) {
  // Create a cycle: 1 -> 2 -> 3 -> 1
  int_sorter.add_vertex(1, std::vector<int>{3});
  int_sorter.add_vertex(2, std::vector<int>{1});
  int_sorter.add_vertex(3, std::vector<int>{2});

  auto result = int_sorter.sort();

  // Should return empty vector when cycle is detected
  EXPECT_TRUE(result.empty());
}

TEST_F(TopologicalSorterTest, SortWithSelfLoop) {
  // Create self-loop: 1 -> 1
  int_sorter.add_vertex(1, std::vector<int>{1});

  auto result = int_sorter.sort();
  EXPECT_TRUE(result.empty());
}

// Query methods tests
TEST_F(TopologicalSorterTest, Dependencies) {
  std::vector<int> deps = {2, 3, 4};
  int_sorter.add_vertex(1, deps);

  const auto &dependencies = int_sorter.pred_of(1);
  EXPECT_EQ(dependencies.size(), 3);
  EXPECT_EQ(std::ranges::count(dependencies, 2), 1);
  EXPECT_EQ(std::ranges::count(dependencies, 3), 1);
  EXPECT_EQ(std::ranges::count(dependencies, 4), 1);

  // Non-existent node should return empty set
  const auto &empty_deps = int_sorter.pred_of(999);
  EXPECT_TRUE(empty_deps.empty());
}

TEST_F(TopologicalSorterTest, Dependents) {
  int_sorter.add_vertex(1, std::vector<int>{2});
  int_sorter.add_vertex(3, std::vector<int>{2});
  int_sorter.add_vertex(4, std::vector<int>{2});

  const auto &dependents = int_sorter.succ_of(2);
  EXPECT_EQ(dependents.size(), 3);
  EXPECT_EQ(std::ranges::count(dependents, 1), 1);
  EXPECT_EQ(std::ranges::count(dependents, 3), 1);
  EXPECT_EQ(std::ranges::count(dependents, 4), 1);

  // Non-existent node should return empty set
  const auto &empty_deps = int_sorter.succ_of(999);
  EXPECT_TRUE(empty_deps.empty());
}

TEST_F(TopologicalSorterTest, Contains) {
  int_sorter.add_vertex(1);
  int_sorter.add_vertex(2, std::vector<int>{3});

  EXPECT_TRUE(int_sorter.contains(1));
  EXPECT_TRUE(int_sorter.contains(2));
  EXPECT_TRUE(int_sorter.contains(3)); // Added as dependency
  EXPECT_FALSE(int_sorter.contains(999));
}

TEST_F(TopologicalSorterTest, Nodes) {
  int_sorter.add_vertex(1);
  int_sorter.add_vertex(2);
  int_sorter.add_vertex(3);

  auto nodes_view = int_sorter.nodes();
  std::vector<int> nodes(nodes_view.begin(), nodes_view.end());

  EXPECT_EQ(nodes.size(), 3);
  EXPECT_TRUE(contains_all(nodes, {1, 2, 3}));
}

// Utility methods tests
TEST_F(TopologicalSorterTest, SizeAndEmpty) {
  EXPECT_TRUE(int_sorter.empty());
  EXPECT_EQ(int_sorter.size(), 0);

  int_sorter.add_vertex(1);
  EXPECT_FALSE(int_sorter.empty());
  EXPECT_EQ(int_sorter.size(), 1);

  int_sorter.add_vertex(2, std::vector<int>{3, 4});
  EXPECT_EQ(int_sorter.size(), 4); // 1, 2, 3, 4

  int_sorter.clear();
  EXPECT_TRUE(int_sorter.empty());
  EXPECT_EQ(int_sorter.size(), 0);
}

TEST_F(TopologicalSorterTest, Clear) {
  int_sorter.add_vertex(1, std::vector<int>{2, 3});
  int_sorter.add_vertex(4, std::vector<int>{5});

  EXPECT_EQ(int_sorter.size(), 5);

  int_sorter.clear();

  EXPECT_TRUE(int_sorter.empty());
  EXPECT_EQ(int_sorter.size(), 0);
  EXPECT_FALSE(int_sorter.contains(1));
  EXPECT_FALSE(int_sorter.contains(2));
  EXPECT_TRUE(int_sorter.sort().empty());
}

// String-based tests to verify template functionality
TEST_F(TopologicalSorterTest, StringNodes) {
  string_sorter.add_vertex("main", std::vector<std::string>{"lib1", "lib2"});
  string_sorter.add_vertex("lib1", std::vector<std::string>{"core"});
  string_sorter.add_vertex("lib2", std::vector<std::string>{"core"});
  string_sorter.add_vertex("core");

  auto result = string_sorter.sort();

  EXPECT_EQ(result.size(), 4);
  EXPECT_TRUE(is_valid_topological_order(result, string_sorter));
  EXPECT_TRUE(contains_all(result, {"main", "lib1", "lib2", "core"}));
}

// Edge case tests
TEST_F(TopologicalSorterTest, DuplicateDependencies) {
  // Adding the same dependency multiple times should not create duplicates
  std::vector<int> deps = {2, 2, 3, 3, 2};
  int_sorter.add_vertex(1, deps);

  const auto &dependencies = int_sorter.pred_of(1);
  EXPECT_EQ(dependencies.size(), 2); // Should only have 2 and 3
  EXPECT_EQ(std::ranges::count(dependencies, 2), 1);
  EXPECT_EQ(std::ranges::count(dependencies, 3), 1);
}

TEST_F(TopologicalSorterTest, AddExistingVertex) {
  int_sorter.add_vertex(1);
  int_sorter.add_vertex(1, std::vector<int>{2}); // Add again with dependencies

  // Should now have dependency
  const auto &deps = int_sorter.pred_of(1);
  EXPECT_EQ(std::ranges::count(deps, 2), 1);
  EXPECT_EQ(int_sorter.size(), 2);
}

TEST_F(TopologicalSorterTest, LargeGraph) {
  // Test with a larger number of nodes
  const int num_nodes = 1000;

  // Create a linear chain
  for (int i = 1; i < num_nodes; ++i) {
    int_sorter.add_vertex(i, std::vector<int>{i + 1});
  }
  int_sorter.add_vertex(num_nodes);

  auto result = int_sorter.sort();

  EXPECT_EQ(result.size(), num_nodes);
  EXPECT_TRUE(is_valid_topological_order(result, int_sorter));
}

// Programmatic graph building tests
TEST_F(TopologicalSorterTest, ProgrammaticChainBuilder) {
  TestGraphBuilder<int> builder;

  // Build various chain lengths
  std::vector<std::vector<int>> chains = {
      {1},              // Single node
      {2, 3},           // Two nodes
      {4, 5, 6},        // Three nodes
      {7, 8, 9, 10, 11} // Five nodes
  };

  for (const auto &chain : chains) {
    builder.add_chain(chain);
  }

  EXPECT_TRUE(builder.validate());

  const auto &sorter = builder.get_sorter();
  auto result = sorter.sort();
  EXPECT_TRUE(is_valid_topological_order(result, sorter));
  EXPECT_EQ(result.size(), builder.get_nodes().size());
}

TEST_F(TopologicalSorterTest, ProgrammaticDiamondBuilder) {
  TestGraphBuilder<int> builder;

  // Add multiple diamond patterns
  builder
      .add_diamond(1, 2, 3, 4)     // Diamond 1
      .add_diamond(5, 6, 7, 8)     // Diamond 2
      .add_diamond(9, 10, 11, 12); // Diamond 3

  EXPECT_TRUE(builder.validate());

  const auto &sorter = builder.get_sorter();
  auto result = sorter.sort();
  EXPECT_TRUE(is_valid_topological_order(result, sorter));
  EXPECT_EQ(result.size(), 12);
}

TEST_F(TopologicalSorterTest, ProgrammaticStarBuilder) {
  TestGraphBuilder<int> builder;

  // Add star patterns of different sizes
  builder
      .add_star(1, {2, 3, 4})        // 3-point star
      .add_star(5, {6, 7, 8, 9, 10}) // 5-point star
      .add_star(11, {12});           // 1-point star (just dependency)

  EXPECT_TRUE(builder.validate());

  const auto &sorter = builder.get_sorter();
  auto result = sorter.sort();
  EXPECT_TRUE(is_valid_topological_order(result, sorter));
}

TEST_F(TopologicalSorterTest, ProgrammaticCycleBuilder) {
  TestGraphBuilder<int> builder;

  // Add a cycle - should fail validation
  builder.add_cycle({1, 2, 3});

  EXPECT_TRUE(builder.expects_cycle());
  EXPECT_TRUE(builder.validate()); // Should validate as expected cycle

  const auto &sorter = builder.get_sorter();
  EXPECT_TRUE(has_cycle(sorter));
}

TEST_F(TopologicalSorterTest, ProgrammaticMixedBuilder) {
  TestGraphBuilder<int> builder;

  // Mix different patterns
  builder.add_chain({1, 2, 3})
      .add_diamond(10, 11, 12, 3) // Connect to existing chain
      .add_star(20, {21, 22, 23})
      .add_isolated({30, 31, 32});

  EXPECT_TRUE(builder.validate());

  const auto &sorter = builder.get_sorter();
  auto result = sorter.sort();
  EXPECT_TRUE(is_valid_topological_order(result, sorter));
}

// Edge case tests with enhanced coverage
TEST_F(TopologicalSorterTest, EdgeCase_EmptyDependencyLists) {
  // Test multiple ways of adding nodes with empty dependencies
  int_sorter.add_vertex(1, std::vector<int>{});
  int_sorter.add_vertex(2);
  int_sorter.add_vertex(3, std::vector<int>{});

  EXPECT_EQ(int_sorter.size(), 3);
  auto result = int_sorter.sort();
  EXPECT_TRUE(is_valid_topological_order(result, int_sorter));
}

TEST_F(TopologicalSorterTest, EdgeCase_SelfDependencyVariations) {
  // Test different ways of creating self-dependencies
  int_sorter.add_vertex(1, std::vector<int>{1}); // Direct self-dependency
  EXPECT_TRUE(has_cycle(int_sorter));

  int_sorter.clear();
  int_sorter.add_vertex(2);
  int_sorter.add_vertex(2, std::vector<int>{2}); // Add self-dependency later
  EXPECT_TRUE(has_cycle(int_sorter));
}

TEST_F(TopologicalSorterTest, EdgeCase_ComplexCycles) {
  // Test various cycle lengths
  for (int cycle_len = 2; cycle_len <= 5; ++cycle_len) {
    int_sorter.clear();

    // Create cycle: 1->2->3->...->cycle_len->1
    for (int i = 1; i <= cycle_len; ++i) {
      int next = (i == cycle_len) ? 1 : i + 1;
      int_sorter.add_vertex(i, std::vector<int>{next});
    }

    EXPECT_TRUE(has_cycle(int_sorter)) << "Failed for cycle length: " << cycle_len;
  }
}

TEST_F(TopologicalSorterTest, EdgeCase_PartialCycles) {
  // Graph with some nodes in cycle, others not
  int_sorter.add_vertex(1, std::vector<int>{2}); // 1->2
  int_sorter.add_vertex(2, std::vector<int>{3}); // 2->3
  int_sorter.add_vertex(3, std::vector<int>{2}); // 3->2 (creates cycle 2->3->2)
  int_sorter.add_vertex(4, std::vector<int>{1}); // 4->1 (not in cycle)
  int_sorter.add_vertex(5);                      // 5 isolated

  EXPECT_TRUE(has_cycle(int_sorter));
  auto result = int_sorter.sort();
  EXPECT_TRUE(result.empty());
}

TEST_F(TopologicalSorterTest, EdgeCase_DuplicateOperations) {
  // Test robustness against duplicate operations
  int_sorter.add_vertex(1);
  int_sorter.add_vertex(1); // Duplicate add
  int_sorter.add_vertex(1, std::vector<int>{2});
  int_sorter.add_vertex(1, std::vector<int>{2, 2, 2}); // Duplicate deps

  EXPECT_EQ(int_sorter.size(), 2);
  const auto &deps = int_sorter.pred_of(1);
  EXPECT_EQ(deps.size(), 1);
  EXPECT_EQ(std::ranges::count(deps, 2), 1);
}

TEST_F(TopologicalSorterTest, EdgeCase_RemovalPatterns) {
  // Test various removal patterns
  int_sorter.add_vertex(1, std::vector<int>{2, 3, 4});
  int_sorter.add_vertex(5, std::vector<int>{1, 2});

  // Remove middle node
  int_sorter.rm_vertex(2);
  EXPECT_FALSE(int_sorter.contains(2));
  EXPECT_EQ(std::ranges::count(int_sorter.pred_of(1), 2), 0);
  EXPECT_EQ(std::ranges::count(int_sorter.pred_of(5), 2), 0);

  // Remove node with no dependencies but has dependents
  int_sorter.rm_vertex(3);
  EXPECT_EQ(std::ranges::count(int_sorter.pred_of(1), 3), 0);

  auto result = int_sorter.sort();
  EXPECT_TRUE(is_valid_topological_order(result, int_sorter));
}

TEST_F(TopologicalSorterTest, EdgeCase_LargeStarPattern) {
  // Single node depending on many others
  const int num_deps = 100;
  std::vector<int> deps;
  for (int i = 2; i <= num_deps + 1; ++i) {
    deps.push_back(i);
  }

  int_sorter.add_vertex(1, deps);

  auto result = int_sorter.sort();
  EXPECT_TRUE(is_valid_topological_order(result, int_sorter));
  EXPECT_EQ(result.size(), num_deps + 1);

  // Verify node 1 comes last
  EXPECT_EQ(result.back(), 1);
}

TEST_F(TopologicalSorterTest, EdgeCase_LargeFanOut) {
  // Many nodes depending on single node
  const int num_dependents = 100;

  int_sorter.add_vertex(1); // Root node
  for (int i = 2; i <= num_dependents + 1; ++i) {
    int_sorter.add_vertex(i, std::vector<int>{1});
  }

  auto result = int_sorter.sort();
  EXPECT_TRUE(is_valid_topological_order(result, int_sorter));
  EXPECT_EQ(result.size(), num_dependents + 1);

  // Verify node 1 comes first
  EXPECT_EQ(result.front(), 1);
}

TEST_F(TopologicalSorterTest, EdgeCase_DeepChain) {
  // Very deep dependency chain
  const int chain_length = 500;

  for (int i = 1; i < chain_length; ++i) {
    int_sorter.add_vertex(i, std::vector<int>{i + 1});
  }
  int_sorter.add_vertex(chain_length);

  auto result = int_sorter.sort();
  EXPECT_TRUE(is_valid_topological_order(result, int_sorter));
  EXPECT_EQ(result.size(), chain_length);

  // Verify order is correct
  EXPECT_EQ(result.front(), chain_length);
  EXPECT_EQ(result.back(), 1);
}

TEST_F(TopologicalSorterTest, EdgeCase_AlternatingAddRemove) {
  // Test interleaved add/remove operations
  int_sorter.add_vertex(1, std::vector<int>{2, 3});
  int_sorter.add_vertex(4, std::vector<int>{1});

  EXPECT_EQ(int_sorter.size(), 4);

  int_sorter.rm_vertex(2);
  EXPECT_EQ(int_sorter.size(), 3);

  int_sorter.add_vertex(5, std::vector<int>{3, 4});
  EXPECT_EQ(int_sorter.size(), 4);

  int_sorter.rm_vertex(1);
  EXPECT_EQ(int_sorter.size(), 3);

  auto result = int_sorter.sort();
  EXPECT_TRUE(is_valid_topological_order(result, int_sorter));
}

TEST_F(TopologicalSorterTest, EdgeCase_StringComplexGraph) {
  // Complex graph with string nodes to test template robustness
  string_sorter.add_vertex("app", std::vector<std::string>{"ui", "core"});
  string_sorter.add_vertex("ui", std::vector<std::string>{"widgets", "platform"});
  string_sorter.add_vertex("core", std::vector<std::string>{"utils", "platform"});
  string_sorter.add_vertex("widgets", std::vector<std::string>{"platform"});
  string_sorter.add_vertex("utils", std::vector<std::string>{"platform"});
  string_sorter.add_vertex("platform");

  auto result = string_sorter.sort();
  EXPECT_TRUE(is_valid_topological_order(result, string_sorter));
  EXPECT_EQ(result.size(), 6);

  // Platform should be first, app should be last
  EXPECT_EQ(result.front(), "platform");
  EXPECT_EQ(result.back(), "app");
}

TEST_F(TopologicalSorterTest, StressTest_RandomGraphs) {
  // Generate and test multiple random-ish graphs
  const int num_tests = 10;
  const int max_nodes = 20;

  for (int test = 0; test < num_tests; ++test) {
    int_sorter.clear();

    // Create a guaranteed acyclic graph by building in layers
    for (int layer = 0; layer < 4; ++layer) {
      for (int node = layer * max_nodes / 4; node < (layer + 1) * max_nodes / 4; ++node) {
        std::vector<int> deps;

        // Add dependencies from previous layers only
        for (int dep_layer = 0; dep_layer < layer; ++dep_layer) {
          for (int dep = dep_layer * max_nodes / 4; dep < (dep_layer + 1) * max_nodes / 4; ++dep) {
            if ((dep + node + test) % 3 == 0) { // Pseudo-random selection
              deps.push_back(dep);
            }
          }
        }

        if (deps.empty()) {
          int_sorter.add_vertex(node);
        } else {
          int_sorter.add_vertex(node, deps);
        }
      }
    }

    auto result = int_sorter.sort();
    EXPECT_TRUE(is_valid_topological_order(result, int_sorter)) << "Failed on stress test iteration: " << test;
  }
}

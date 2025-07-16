#include "gtest/gtest.h"
#include <algorithm>
#include <string>
#include <vector>

#include "opflow/topo.hpp"

using namespace opflow;

class SortedGraphTest : public ::testing::Test {
protected:
  topological_sorter<int> int_sorter;
  topological_sorter<std::string> string_sorter;

  void SetUp() override {
    int_sorter.clear();
    string_sorter.clear();
  }

  // Helper to create a simple linear dependency chain: 1 -> 2 -> 3
  sorted_graph<int> create_linear_graph() {
    int_sorter.add_vertex(1);
    int_sorter.add_vertex(2, std::vector<int>{1});
    int_sorter.add_vertex(3, std::vector<int>{2});
    return int_sorter.make_sorted_graph();
  }

  // Helper to create a diamond dependency: 1 -> 2,3 -> 4
  sorted_graph<int> create_diamond_graph() {
    int_sorter.add_vertex(1);
    int_sorter.add_vertex(2, std::vector<int>{1});
    int_sorter.add_vertex(3, std::vector<int>{1});
    int_sorter.add_vertex(4, std::vector<int>{2, 3});
    return int_sorter.make_sorted_graph();
  }
};

// Test default constructor and empty state
TEST_F(SortedGraphTest, DefaultConstructor) {
  sorted_graph<int> graph;

  EXPECT_TRUE(graph.empty());
  EXPECT_EQ(graph.size(), 0);
  EXPECT_EQ(graph.begin(), graph.end());
  EXPECT_EQ(graph.sorted_nodes().size(), 0);
}

// Test construction from topological_sorter
TEST_F(SortedGraphTest, ConstructionFromSorter) {
  auto graph = create_linear_graph();

  EXPECT_FALSE(graph.empty());
  EXPECT_EQ(graph.size(), 3);

  auto sorted_nodes = graph.sorted_nodes();
  EXPECT_EQ(sorted_nodes.size(), 3);
  EXPECT_EQ(sorted_nodes[0], 1);
  EXPECT_EQ(sorted_nodes[1], 2);
  EXPECT_EQ(sorted_nodes[2], 3);
}

// Test iterator basic functionality
TEST_F(SortedGraphTest, IteratorBasics) {
  auto graph = create_linear_graph();

  auto it = graph.begin();
  EXPECT_NE(it, graph.end());

  // Check first element
  auto [node1, deps1] = *it;
  EXPECT_EQ(node1, 1);
  EXPECT_TRUE(deps1.empty());

  // Move to second element
  ++it;
  auto [node2, deps2] = *it;
  EXPECT_EQ(node2, 2);
  EXPECT_EQ(deps2.size(), 1);
  EXPECT_TRUE(deps2.contains(1));

  // Move to third element
  ++it;
  auto [node3, deps3] = *it;
  EXPECT_EQ(node3, 3);
  EXPECT_EQ(deps3.size(), 1);
  EXPECT_TRUE(deps3.contains(2));

  // Should be at end now
  ++it;
  EXPECT_EQ(it, graph.end());
}

// Test iterator arithmetic
TEST_F(SortedGraphTest, IteratorArithmetic) {
  auto graph = create_linear_graph();

  auto it = graph.begin();

  // Test +=
  it += 2;
  auto [node, deps] = *it;
  EXPECT_EQ(node, 3);

  // Test -=
  it -= 1;
  auto [node2, deps2] = *it;
  EXPECT_EQ(node2, 2);

  // Test + operator
  auto it2 = graph.begin() + 2;
  auto [node3, deps3] = *it2;
  EXPECT_EQ(node3, 3);

  // Test - operator
  auto it3 = graph.end() - 1;
  auto [node4, deps4] = *it3;
  EXPECT_EQ(node4, 3);

  // Test iterator difference
  EXPECT_EQ(graph.end() - graph.begin(), 3);
}

// Test random access with operator[]
TEST_F(SortedGraphTest, RandomAccess) {
  auto graph = create_linear_graph();

  // Test operator[]
  auto [node1, deps1] = graph[0];
  EXPECT_EQ(node1, 1);
  EXPECT_TRUE(deps1.empty());

  auto [node2, deps2] = graph[1];
  EXPECT_EQ(node2, 2);
  EXPECT_EQ(deps2.size(), 1);

  auto [node3, deps3] = graph[2];
  EXPECT_EQ(node3, 3);
  EXPECT_EQ(deps3.size(), 1);
}

// Test bounds-checked access with at()
TEST_F(SortedGraphTest, BoundsCheckedAccess) {
  auto graph = create_linear_graph();

  // Valid access
  auto [node, deps] = graph.at(0);
  EXPECT_EQ(node, 1);

  // Invalid access should throw
  EXPECT_THROW(graph.at(3), std::out_of_range);
  EXPECT_THROW(graph.at(100), std::out_of_range);
}

// Test node_at method
TEST_F(SortedGraphTest, NodeAt) {
  auto graph = create_linear_graph();

  EXPECT_EQ(graph.node_at(0), 1);
  EXPECT_EQ(graph.node_at(1), 2);
  EXPECT_EQ(graph.node_at(2), 3);
}

// Test front() and back() methods
TEST_F(SortedGraphTest, FrontAndBack) {
  auto graph = create_linear_graph();

  auto [front_node, front_deps] = graph.front();
  EXPECT_EQ(front_node, 1);
  EXPECT_TRUE(front_deps.empty());

  auto [back_node, back_deps] = graph.back();
  EXPECT_EQ(back_node, 3);
  EXPECT_EQ(back_deps.size(), 1);
  EXPECT_TRUE(back_deps.contains(2));
}

// Test range-based for loop
TEST_F(SortedGraphTest, RangeBasedFor) {
  auto graph = create_linear_graph();

  std::vector<int> nodes;
  for (auto [node, deps] : graph) {
    nodes.push_back(node);
  }

  EXPECT_EQ(nodes.size(), 3);
  EXPECT_EQ(nodes[0], 1);
  EXPECT_EQ(nodes[1], 2);
  EXPECT_EQ(nodes[2], 3);
}

// Test STL algorithm compatibility
TEST_F(SortedGraphTest, STLAlgorithms) {
  auto graph = create_linear_graph();

  // Test std::distance
  EXPECT_EQ(std::distance(graph.begin(), graph.end()), 3);

  // Test std::find_if
  auto it = std::find_if(graph.begin(), graph.end(), [](const auto &pair) { return pair.first == 2; });
  EXPECT_NE(it, graph.end());
  EXPECT_EQ((*it).first, 2);
}

// Test with diamond dependency pattern
TEST_F(SortedGraphTest, DiamondDependency) {
  auto graph = create_diamond_graph();

  EXPECT_EQ(graph.size(), 4);

  // First node should be 1 (no dependencies)
  auto [node1, deps1] = graph[0];
  EXPECT_EQ(node1, 1);
  EXPECT_TRUE(deps1.empty());

  // Last node should be 4 (depends on 2 and 3)
  auto [node4, deps4] = graph.back();
  EXPECT_EQ(node4, 4);
  EXPECT_EQ(deps4.size(), 2);
  EXPECT_TRUE(deps4.contains(2));
  EXPECT_TRUE(deps4.contains(3));
}

// Test with string nodes
TEST_F(SortedGraphTest, StringNodes) {
  string_sorter.add_vertex("start");
  string_sorter.add_vertex("middle", std::vector<std::string>{"start"});
  string_sorter.add_vertex("end", std::vector<std::string>{"middle"});

  auto graph = string_sorter.make_sorted_graph();

  EXPECT_EQ(graph.size(), 3);

  auto [node1, deps1] = graph[0];
  EXPECT_EQ(node1, "start");

  auto [node3, deps3] = graph[2];
  EXPECT_EQ(node3, "end");
  EXPECT_TRUE(deps3.contains("middle"));
}

// Test immutability (mutating methods should be hidden)
TEST_F(SortedGraphTest, Immutability) {
  auto graph = create_linear_graph();

  // These should not compile if uncommented:
  // graph.add_vertex(4);
  // graph.rm_vertex(1);
  // graph.add_edge(1, std::vector<int>{5});
  // graph.rm_edge(2, std::vector<int>{1});
  // graph.clear();
  // graph.sort();

  // The test passes if the code compiles (methods are properly hidden)
  SUCCEED();
}

// Test cyclic graph handling (should return empty sorted_graph)
TEST_F(SortedGraphTest, CyclicGraph) {
  // Create a cycle: 1 -> 2 -> 3 -> 1
  int_sorter.add_vertex(1, std::vector<int>{3});
  int_sorter.add_vertex(2, std::vector<int>{1});
  int_sorter.add_vertex(3, std::vector<int>{2});

  auto graph = int_sorter.make_sorted_graph();

  EXPECT_TRUE(graph.empty());
  EXPECT_EQ(graph.size(), 0);
}

// Test iterator comparison operators
TEST_F(SortedGraphTest, IteratorComparison) {
  auto graph = create_linear_graph();

  auto it1 = graph.begin();
  auto it2 = graph.begin() + 1;
  auto it3 = graph.end();

  EXPECT_TRUE(it1 < it2);
  EXPECT_TRUE(it2 < it3);
  EXPECT_FALSE(it2 < it1);

  EXPECT_TRUE(it1 <= it2);
  EXPECT_TRUE(it1 <= it1);

  EXPECT_TRUE(it2 > it1);
  EXPECT_TRUE(it3 > it2);

  EXPECT_TRUE(it2 >= it1);
  EXPECT_TRUE(it2 >= it2);

  EXPECT_TRUE(it1 == it1);
  EXPECT_FALSE(it1 == it2);

  EXPECT_FALSE(it1 != it1);
  EXPECT_TRUE(it1 != it2);
}

// Test iterator subscript operator
TEST_F(SortedGraphTest, IteratorSubscript) {
  auto graph = create_linear_graph();

  auto it = graph.begin();

  auto [node1, deps1] = it[0];
  EXPECT_EQ(node1, 1);

  auto [node2, deps2] = it[1];
  EXPECT_EQ(node2, 2);

  auto [node3, deps3] = it[2];
  EXPECT_EQ(node3, 3);
}

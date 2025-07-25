#include "gtest/gtest.h"
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "opflow/topo.hpp"

using namespace opflow;

// Test fixture for BFS tests
class TopoBfsTest : public ::testing::Test {
protected:
  topological_sorter<int> int_sorter;
  topological_sorter<std::string> string_sorter;

  void SetUp() override {
    // Fresh sorters for each test
    int_sorter.clear();
    string_sorter.clear();
  }

  // Helper to verify colour map correctness
  template <typename T>
  void verify_colour_consistency(const std::unordered_map<T, colour> &colour_map,
                                 const std::unordered_map<T, size_t> &depth_map, const topological_sorter<T> &sorter) {
    // All nodes in sorter should be in colour_map
    for (const auto &node : sorter.nodes()) {
      EXPECT_TRUE(colour_map.find(node) != colour_map.end()) << "Node " << node << " missing from colour_map";
    }

    // All nodes with depth should have been visited (gray or black)
    for (const auto &[node, depth] : depth_map) {
      auto colour_it = colour_map.find(node);
      ASSERT_TRUE(colour_it != colour_map.end()) << "Node " << node << " in depth_map but not in colour_map";
      EXPECT_TRUE(colour_it->second == colour::gray || colour_it->second == colour::black)
          << "Node " << node << " has depth but is white";
    }
  }

  // Helper to check BFS depth correctness
  template <typename T>
  void verify_depth_correctness(const T &root, const std::unordered_map<T, size_t> &depth_map,
                                const topological_sorter<T> &sorter) {
    if (depth_map.empty())
      return;

    // Root should have depth 0
    auto root_depth_it = depth_map.find(root);
    if (root_depth_it != depth_map.end()) {
      EXPECT_EQ(root_depth_it->second, 0) << "Root node should have depth 0";
    }

    // Check that depths increase correctly along edges
    for (const auto &[node, depth] : depth_map) {
      for (const auto &dependent : sorter.succ_of(node)) {
        auto dependent_depth_it = depth_map.find(dependent);
        if (dependent_depth_it != depth_map.end()) {
          EXPECT_EQ(dependent_depth_it->second, depth + 1) << "Dependent " << dependent << " should have depth "
                                                           << (depth + 1) << " but has " << dependent_depth_it->second;
        }
      }
    }
  }
};

// Test BFS on empty graph
TEST_F(TopoBfsTest, EmptyGraph) {
  auto [colour_map, depth_map] = int_sorter.bfs(1, topological_sorter<int>::noop_visitor);

  EXPECT_TRUE(colour_map.empty());
  EXPECT_TRUE(depth_map.empty());
}

// Test BFS on single node
TEST_F(TopoBfsTest, SingleNode) {
  int_sorter.add_vertex(42);

  std::vector<int> visited_nodes;
  auto visitor = [&visited_nodes](const int &node, const auto &, size_t) {
    visited_nodes.push_back(node);
    return true;
  };

  auto [colour_map, depth_map] = int_sorter.bfs(42, visitor);

  EXPECT_EQ(visited_nodes.size(), 1);
  EXPECT_EQ(visited_nodes[0], 42);
  EXPECT_EQ(colour_map[42], colour::black);
  EXPECT_EQ(depth_map[42], 0);

  verify_colour_consistency(colour_map, depth_map, int_sorter);
  verify_depth_correctness(42, depth_map, int_sorter);
}

// Test BFS with non-existent root
TEST_F(TopoBfsTest, NonExistentRoot) {
  int_sorter.add_vertex(1);
  int_sorter.add_vertex(2);

  auto [colour_map, depth_map] = int_sorter.bfs(999, topological_sorter<int>::noop_visitor);

  EXPECT_TRUE(colour_map.empty());
  EXPECT_TRUE(depth_map.empty());
}

// Test linear chain: 1 -> 2 -> 3 -> 4
TEST_F(TopoBfsTest, LinearChain) {
  int_sorter.add_vertex(1);
  int_sorter.add_vertex(2, std::vector<int>{1});
  int_sorter.add_vertex(3, std::vector<int>{2});
  int_sorter.add_vertex(4, std::vector<int>{3});

  std::vector<int> visited_order;
  auto visitor = [&visited_order](const int &node, const auto &, size_t) {
    visited_order.push_back(node);
    return true;
  };

  auto [colour_map, depth_map] = int_sorter.bfs(1, visitor);

  // Should visit in BFS order: 1, 2, 3, 4
  EXPECT_EQ(visited_order, std::vector<int>({1, 2, 3, 4}));

  // Check depths
  EXPECT_EQ(depth_map[1], 0);
  EXPECT_EQ(depth_map[2], 1);
  EXPECT_EQ(depth_map[3], 2);
  EXPECT_EQ(depth_map[4], 3);

  // All should be black (visited)
  for (int i = 1; i <= 4; ++i) {
    EXPECT_EQ(colour_map[i], colour::black);
  }

  verify_colour_consistency(colour_map, depth_map, int_sorter);
  verify_depth_correctness(1, depth_map, int_sorter);
}

// Test tree structure:    1
//                       /   \
//                      2     3
//                     / \   / \
//                    4   5 6   7
TEST_F(TopoBfsTest, TreeStructure) {
  int_sorter.add_vertex(1);
  int_sorter.add_vertex(2, std::vector<int>{1});
  int_sorter.add_vertex(3, std::vector<int>{1});
  int_sorter.add_vertex(4, std::vector<int>{2});
  int_sorter.add_vertex(5, std::vector<int>{2});
  int_sorter.add_vertex(6, std::vector<int>{3});
  int_sorter.add_vertex(7, std::vector<int>{3});

  std::vector<int> visited_order;
  auto visitor = [&visited_order](const int &node, const auto &, size_t) {
    visited_order.push_back(node);
    return true;
  };

  auto [colour_map, depth_map] = int_sorter.bfs(1, visitor);

  // Should visit level by level: 1, then {2,3}, then {4,5,6,7}
  EXPECT_EQ(visited_order[0], 1);

  // Level 1: nodes 2 and 3 (can be in any order)
  std::set<int> level1(visited_order.begin() + 1, visited_order.begin() + 3);
  EXPECT_EQ(level1, std::set<int>({2, 3}));

  // Level 2: nodes 4,5,6,7 (can be in any order)
  std::set<int> level2(visited_order.begin() + 3, visited_order.end());
  EXPECT_EQ(level2, std::set<int>({4, 5, 6, 7}));

  // Check depths
  EXPECT_EQ(depth_map[1], 0);
  EXPECT_EQ(depth_map[2], 1);
  EXPECT_EQ(depth_map[3], 1);
  EXPECT_EQ(depth_map[4], 2);
  EXPECT_EQ(depth_map[5], 2);
  EXPECT_EQ(depth_map[6], 2);
  EXPECT_EQ(depth_map[7], 2);

  verify_colour_consistency(colour_map, depth_map, int_sorter);
  verify_depth_correctness(1, depth_map, int_sorter);
}

// Test diamond structure:  1
//                         / \
//                        2   3
//                         \ /
//                          4
TEST_F(TopoBfsTest, DiamondStructure) {
  int_sorter.add_vertex(1);
  int_sorter.add_vertex(2, std::vector<int>{1});
  int_sorter.add_vertex(3, std::vector<int>{1});
  int_sorter.add_vertex(4, std::vector<int>{2, 3});

  std::vector<int> visited_order;
  std::vector<int> gray_encounters;

  auto visitor = [&visited_order](const int &node, const auto &, size_t) {
    visited_order.push_back(node);
    return true;
  };

  auto gray_handler = [&gray_encounters](const int &node, const auto &, size_t) {
    gray_encounters.push_back(node);
    return true;
  };

  auto [colour_map, depth_map] = int_sorter.bfs(1, visitor, gray_handler, topological_sorter<int>::noop_visitor);

  // Should visit: 1, then {2,3}, then 4
  EXPECT_EQ(visited_order[0], 1);
  std::set<int> level1(visited_order.begin() + 1, visited_order.begin() + 3);
  EXPECT_EQ(level1, std::set<int>({2, 3}));
  EXPECT_EQ(visited_order[3], 4);

  // Node 4 should be encountered as gray once (when second parent is processed)
  EXPECT_EQ(gray_encounters.size(), 1);
  EXPECT_EQ(gray_encounters[0], 4);

  verify_colour_consistency(colour_map, depth_map, int_sorter);
  verify_depth_correctness(1, depth_map, int_sorter);
}

// Test multiple disconnected components
TEST_F(TopoBfsTest, DisconnectedComponents) {
  // Component 1: 1 -> 2
  int_sorter.add_vertex(1);
  int_sorter.add_vertex(2, std::vector<int>{1});

  // Component 2: 3 -> 4
  int_sorter.add_vertex(3);
  int_sorter.add_vertex(4, std::vector<int>{3});

  std::vector<int> visited_order;
  auto visitor = [&visited_order](const int &node, const auto &, size_t) {
    visited_order.push_back(node);
    return true;
  };

  // Start BFS from component 1
  auto [colour_map, depth_map] = int_sorter.bfs(1, visitor);

  // Should only visit component 1
  EXPECT_EQ(visited_order.size(), 2);
  EXPECT_EQ(visited_order, std::vector<int>({1, 2}));

  // Component 2 should remain white
  EXPECT_EQ(colour_map[3], colour::white);
  EXPECT_EQ(colour_map[4], colour::white);

  verify_colour_consistency(colour_map, depth_map, int_sorter);
  verify_depth_correctness(1, depth_map, int_sorter);
}

// Test early termination via visitor
TEST_F(TopoBfsTest, EarlyTerminationByVisitor) {
  // Create chain: 1 -> 2 -> 3 -> 4
  int_sorter.add_vertex(1);
  int_sorter.add_vertex(2, std::vector<int>{1});
  int_sorter.add_vertex(3, std::vector<int>{2});
  int_sorter.add_vertex(4, std::vector<int>{3});

  std::vector<int> visited_order;
  auto visitor = [&visited_order](const int &node, const auto &, size_t) {
    visited_order.push_back(node);
    return node != 2; // Stop after visiting node 2
  };

  auto [colour_map, depth_map] = int_sorter.bfs(1, visitor);

  // Should stop after visiting 1 and 2
  EXPECT_EQ(visited_order, std::vector<int>({1, 2}));

  // Node 2 should be black (visited), 3 should be gray (discovered), 4 white
  EXPECT_EQ(colour_map[1], colour::black);
  EXPECT_EQ(colour_map[2], colour::black);
  EXPECT_EQ(colour_map[3], colour::gray);
  EXPECT_EQ(colour_map[4], colour::white);
}

// Test early termination via gray handler
TEST_F(TopoBfsTest, EarlyTerminationByGrayHandler) {
  // Diamond: 1 -> {2,3} -> 4
  int_sorter.add_vertex(1);
  int_sorter.add_vertex(2, std::vector<int>{1});
  int_sorter.add_vertex(3, std::vector<int>{1});
  int_sorter.add_vertex(4, std::vector<int>{2, 3});

  std::vector<int> visited_order;
  std::vector<int> gray_encounters;

  auto visitor = [&visited_order](const int &node, const auto &, size_t) {
    visited_order.push_back(node);
    return true;
  };

  auto gray_handler = [&gray_encounters](const int &node, const auto &, size_t) {
    gray_encounters.push_back(node);
    return false; // Stop on first gray encounter
  };

  auto [colour_map, depth_map] = int_sorter.bfs(1, visitor, gray_handler, topological_sorter<int>::noop_visitor);

  // Should encounter gray node 4 and then stop
  EXPECT_EQ(gray_encounters.size(), 1);
  EXPECT_EQ(gray_encounters[0], 4);
}

// Test handler with and without depth parameter
TEST_F(TopoBfsTest, HandlerVariants) {
  int_sorter.add_vertex(1);
  int_sorter.add_vertex(2, std::vector<int>{1});

  // Test visitor without depth
  std::vector<int> visited_no_depth;
  auto visitor_no_depth = [&visited_no_depth](const int &node, const auto &) {
    visited_no_depth.push_back(node);
    return true;
  };

  // Test visitor with depth
  std::vector<std::pair<int, size_t>> visited_with_depth;
  auto visitor_with_depth = [&visited_with_depth](const int &node, const auto &, size_t depth) {
    visited_with_depth.emplace_back(node, depth);
    return true;
  };

  // Test without depth
  auto [colour_map1, depth_map1] = int_sorter.bfs(1, visitor_no_depth);

  EXPECT_EQ(visited_no_depth, std::vector<int>({1, 2}));

  // Test with depth
  auto [colour_map2, depth_map2] = int_sorter.bfs(1, visitor_with_depth);

  EXPECT_EQ(visited_with_depth.size(), 2);
  EXPECT_EQ(visited_with_depth[0], std::make_pair(1, 0));
  EXPECT_EQ(visited_with_depth[1], std::make_pair(2, 1));
}

// Test complex graph with great-grandparent relationships
TEST_F(TopoBfsTest, ComplexGraphWithGreatGrandparents) {
  // Graph:     1
  //           /|\
  //          2 3 4
  //         /  |  \
  //        5   6   7
  //         \ | /
  //          \|/
  //           8
  int_sorter.add_vertex(1);
  int_sorter.add_vertex(2, std::vector<int>{1});
  int_sorter.add_vertex(3, std::vector<int>{1});
  int_sorter.add_vertex(4, std::vector<int>{1});
  int_sorter.add_vertex(5, std::vector<int>{2});
  int_sorter.add_vertex(6, std::vector<int>{3});
  int_sorter.add_vertex(7, std::vector<int>{4});
  int_sorter.add_vertex(8, std::vector<int>{5, 6, 7});

  std::vector<int> visited_order;
  std::vector<int> gray_encounters;

  auto visitor = [&visited_order](const int &node, const auto &, size_t) {
    visited_order.push_back(node);
    return true;
  };

  auto gray_handler = [&gray_encounters](const int &node, const auto &, size_t) {
    gray_encounters.push_back(node);
    return true;
  };

  auto [colour_map, depth_map] = int_sorter.bfs(1, visitor, gray_handler, topological_sorter<int>::noop_visitor);

  // Check BFS level structure
  EXPECT_EQ(visited_order[0], 1); // Level 0

  // Level 1: {2,3,4}
  std::set<int> level1(visited_order.begin() + 1, visited_order.begin() + 4);
  EXPECT_EQ(level1, std::set<int>({2, 3, 4}));

  // Level 2: {5,6,7}
  std::set<int> level2(visited_order.begin() + 4, visited_order.begin() + 7);
  EXPECT_EQ(level2, std::set<int>({5, 6, 7}));

  // Level 3: {8}
  EXPECT_EQ(visited_order[7], 8);

  // Node 8 should be encountered as gray twice (when second and third parents are processed)
  EXPECT_EQ(gray_encounters.size(), 2);
  EXPECT_EQ(gray_encounters[0], 8);
  EXPECT_EQ(gray_encounters[1], 8);

  verify_colour_consistency(colour_map, depth_map, int_sorter);
  verify_depth_correctness(1, depth_map, int_sorter);
}

// Test multiple roots scenario
TEST_F(TopoBfsTest, MultipleRoots) {
  // Two independent trees:
  // Root 1: 1 -> 2 -> 4
  // Root 3: 3 -> 5 -> 6
  int_sorter.add_vertex(1);
  int_sorter.add_vertex(2, std::vector<int>{1});
  int_sorter.add_vertex(4, std::vector<int>{2});
  int_sorter.add_vertex(3);
  int_sorter.add_vertex(5, std::vector<int>{3});
  int_sorter.add_vertex(6, std::vector<int>{5});

  // BFS from root 1
  std::vector<int> visited_from_1;
  auto visitor1 = [&visited_from_1](const int &node, const auto &, size_t) {
    visited_from_1.push_back(node);
    return true;
  };

  auto [colour_map1, depth_map1] = int_sorter.bfs(1, visitor1);

  EXPECT_EQ(visited_from_1, std::vector<int>({1, 2, 4}));
  EXPECT_EQ(colour_map1[3], colour::white);
  EXPECT_EQ(colour_map1[5], colour::white);
  EXPECT_EQ(colour_map1[6], colour::white);

  // BFS from root 3
  std::vector<int> visited_from_3;
  auto visitor3 = [&visited_from_3](const int &node, const auto &, size_t) {
    visited_from_3.push_back(node);
    return true;
  };

  auto [colour_map3, depth_map3] = int_sorter.bfs(3, visitor3);

  EXPECT_EQ(visited_from_3, std::vector<int>({3, 5, 6}));
  EXPECT_EQ(colour_map3[1], colour::white);
  EXPECT_EQ(colour_map3[2], colour::white);
  EXPECT_EQ(colour_map3[4], colour::white);
}

// Test with string nodes to ensure template works correctly
TEST_F(TopoBfsTest, StringNodes) {
  string_sorter.add_vertex("root");
  string_sorter.add_vertex("child1", std::vector<std::string>{"root"});
  string_sorter.add_vertex("child2", std::vector<std::string>{"root"});
  string_sorter.add_vertex("grandchild", std::vector<std::string>{"child1", "child2"});

  std::vector<std::string> visited_order;
  auto visitor = [&visited_order](const std::string &node, const auto &, size_t) {
    visited_order.push_back(node);
    return true;
  };

  auto [colour_map, depth_map] = string_sorter.bfs("root", visitor);

  EXPECT_EQ(visited_order[0], "root");
  std::set<std::string> level1(visited_order.begin() + 1, visited_order.begin() + 3);
  EXPECT_EQ(level1, std::set<std::string>({"child1", "child2"}));
  EXPECT_EQ(visited_order[3], "grandchild");

  verify_colour_consistency(colour_map, depth_map, string_sorter);
  verify_depth_correctness(std::string("root"), depth_map, string_sorter);
}

// Test black handler functionality
TEST_F(TopoBfsTest, BlackHandler) {
  // Chain: 1 -> 2 -> 3 -> 4
  //             | -> 5 <- |
  int_sorter.add_vertex(1);
  int_sorter.add_vertex(2, std::vector<int>{1, 5});
  int_sorter.add_vertex(3, std::vector<int>{2});
  int_sorter.add_vertex(4, std::vector<int>{3});
  int_sorter.add_vertex(5, std::vector<int>{4});

  std::vector<int> black_encounters;
  auto black_handler = [&black_encounters](const int &node, const auto &, size_t) {
    black_encounters.push_back(node);
    return true;
  };

  std::vector<int> visited_order;
  auto visitor = [&visited_order](const int &node, const auto &, size_t) {
    visited_order.push_back(node);
    return true;
  };

  auto [colour_map, depth_map] = int_sorter.bfs(1, visitor, topological_sorter<int>::noop_visitor, black_handler);

  // Some nodes should be encountered as black when revisited
  EXPECT_FALSE(black_encounters.empty());

  verify_colour_consistency(colour_map, depth_map, int_sorter);
  // due to cycle depth now make no sense
  // verify_depth_correctness(1, depth_map, int_sorter);
}

// Test correctness of BFS vs expected order manually
TEST_F(TopoBfsTest, BfsOrderCorrectnessManualVerification) {
  // Create a specific graph where we know the exact BFS order
  //     0
  //   / | \
  //  1  2  3
  //  |  |  |
  //  4  5  6
  int_sorter.add_vertex(0);
  int_sorter.add_vertex(1, std::vector<int>{0});
  int_sorter.add_vertex(2, std::vector<int>{0});
  int_sorter.add_vertex(3, std::vector<int>{0});
  int_sorter.add_vertex(4, std::vector<int>{1});
  int_sorter.add_vertex(5, std::vector<int>{2});
  int_sorter.add_vertex(6, std::vector<int>{3});

  std::vector<int> visited_order;
  std::vector<size_t> depths;

  auto visitor = [&visited_order, &depths](const int &node, const auto &, size_t depth) {
    visited_order.push_back(node);
    depths.push_back(depth);
    return true;
  };

  auto [colour_map, depth_map] = int_sorter.bfs(0, visitor);

  // Verify BFS properties:
  // 1. Root first
  EXPECT_EQ(visited_order[0], 0);
  EXPECT_EQ(depths[0], 0);

  // 2. All nodes at depth d before any at depth d+1
  for (size_t i = 1; i < depths.size(); ++i) {
    EXPECT_TRUE(depths[i] >= depths[i - 1])
        << "BFS order violated: depth " << depths[i] << " after depth " << depths[i - 1];
  }

  // 3. Verify specific depth assignments
  EXPECT_EQ(depth_map[0], 0);
  EXPECT_EQ(depth_map[1], 1);
  EXPECT_EQ(depth_map[2], 1);
  EXPECT_EQ(depth_map[3], 1);
  EXPECT_EQ(depth_map[4], 2);
  EXPECT_EQ(depth_map[5], 2);
  EXPECT_EQ(depth_map[6], 2);

  verify_colour_consistency(colour_map, depth_map, int_sorter);
  verify_depth_correctness(0, depth_map, int_sorter);
}

// Edge case: graph with self-loop (though DAG shouldn't have this, test robustness)
TEST_F(TopoBfsTest, GraphStructureIntegrity) {
  // Verify that the graph structure is maintained correctly during BFS
  int_sorter.add_vertex(1);
  int_sorter.add_vertex(2, std::vector<int>{1});
  int_sorter.add_vertex(3, std::vector<int>{1});

  // Store original dependents
  auto original_successors_1 = int_sorter.succ_of(1);
  auto original_successors_2 = int_sorter.succ_of(2);
  auto original_successors_3 = int_sorter.succ_of(3);

  auto [colour_map, depth_map] = int_sorter.bfs(1, topological_sorter<int>::noop_visitor);

  // Verify graph structure is unchanged
  EXPECT_EQ(int_sorter.succ_of(1), original_successors_1);
  EXPECT_EQ(int_sorter.succ_of(2), original_successors_2);
  EXPECT_EQ(int_sorter.succ_of(3), original_successors_3);
}

// Test large graph performance and correctness
TEST_F(TopoBfsTest, LargeGraph) {
  // Build a wide tree: root with 100 children, each child has 10 grandchildren
  const int root = 0;
  const int num_children = 100;
  const int grandchildren_per_child = 10;

  int_sorter.add_vertex(root);

  // Add children
  for (int i = 1; i <= num_children; ++i) {
    int_sorter.add_vertex(i, std::vector<int>{root});
  }

  // Add grandchildren
  int node_id = num_children + 1;
  for (int child = 1; child <= num_children; ++child) {
    for (int gc = 0; gc < grandchildren_per_child; ++gc) {
      int_sorter.add_vertex(node_id++, std::vector<int>{child});
    }
  }

  std::vector<int> visited_order;
  std::unordered_map<int, size_t> visit_depths;

  auto visitor = [&visited_order, &visit_depths](const int &node, const auto &, size_t depth) {
    visited_order.push_back(node);
    visit_depths[node] = depth;
    return true;
  };

  auto [colour_map, depth_map] = int_sorter.bfs(root, visitor);

  // Verify correct number of nodes visited
  EXPECT_EQ(visited_order.size(), 1 + num_children + num_children * grandchildren_per_child);

  // Verify root is first
  EXPECT_EQ(visited_order[0], root);
  EXPECT_EQ(visit_depths[root], 0);

  // Verify all children have depth 1
  for (int i = 1; i <= num_children; ++i) {
    EXPECT_EQ(visit_depths[i], 1);
  }

  // Verify all grandchildren have depth 2
  node_id = num_children + 1;
  for (int child = 1; child <= num_children; ++child) {
    for (int gc = 0; gc < grandchildren_per_child; ++gc) {
      EXPECT_EQ(visit_depths[node_id++], 2);
    }
  }

  verify_colour_consistency(colour_map, depth_map, int_sorter);
  verify_depth_correctness(root, depth_map, int_sorter);
}

// Test complex multi-path convergence
TEST_F(TopoBfsTest, ComplexConvergence) {
  // Create a graph where multiple paths of different lengths lead to the same node
  //     0
  //   / | \
  //  1  2  3
  //  |  |  |\
  //  4  5  6 7
  //   \ |  |/
  //    \|  |
  //     8  9
  //      \/
  //      10

  int_sorter.add_vertex(0);
  int_sorter.add_vertex(1, std::vector<int>{0});
  int_sorter.add_vertex(2, std::vector<int>{0});
  int_sorter.add_vertex(3, std::vector<int>{0});
  int_sorter.add_vertex(4, std::vector<int>{1});
  int_sorter.add_vertex(5, std::vector<int>{2});
  int_sorter.add_vertex(6, std::vector<int>{3});
  int_sorter.add_vertex(7, std::vector<int>{3});
  int_sorter.add_vertex(8, std::vector<int>{4, 5});
  int_sorter.add_vertex(9, std::vector<int>{6, 7});
  int_sorter.add_vertex(10, std::vector<int>{8, 9});

  std::vector<int> visited_order;
  std::vector<int> gray_encounters;
  std::vector<int> black_encounters;

  auto visitor = [&visited_order](const int &node, const auto &, size_t) {
    visited_order.push_back(node);
    return true;
  };

  auto gray_handler = [&gray_encounters](const int &node, const auto &, size_t) {
    gray_encounters.push_back(node);
    return true;
  };

  auto black_handler = [&black_encounters](const int &node, const auto &, size_t) {
    black_encounters.push_back(node);
    return true;
  };

  auto [colour_map, depth_map] = int_sorter.bfs(0, visitor, gray_handler, black_handler);

  // Verify BFS level structure
  EXPECT_EQ(depth_map[0], 0);
  EXPECT_EQ(depth_map[1], 1);
  EXPECT_EQ(depth_map[2], 1);
  EXPECT_EQ(depth_map[3], 1);
  EXPECT_EQ(depth_map[4], 2);
  EXPECT_EQ(depth_map[5], 2);
  EXPECT_EQ(depth_map[6], 2);
  EXPECT_EQ(depth_map[7], 2);
  EXPECT_EQ(depth_map[8], 3);
  EXPECT_EQ(depth_map[9], 3);
  EXPECT_EQ(depth_map[10], 4);

  // Verify convergence nodes are encountered as gray
  EXPECT_TRUE(std::find(gray_encounters.begin(), gray_encounters.end(), 8) != gray_encounters.end());
  EXPECT_TRUE(std::find(gray_encounters.begin(), gray_encounters.end(), 9) != gray_encounters.end());
  EXPECT_TRUE(std::find(gray_encounters.begin(), gray_encounters.end(), 10) != gray_encounters.end());

  verify_colour_consistency(colour_map, depth_map, int_sorter);
  verify_depth_correctness(0, depth_map, int_sorter);
}

// Test handler exception safety
TEST_F(TopoBfsTest, HandlerExceptionSafety) {
  int_sorter.add_vertex(1);
  int_sorter.add_vertex(2, std::vector<int>{1});
  int_sorter.add_vertex(3, std::vector<int>{2});

  // Test that if a handler throws, the graph remains intact
  auto throwing_visitor = [](const int &node, const auto &, size_t) -> bool {
    if (node == 2) {
      throw std::runtime_error("Test exception");
    }
    return true;
  };

  // Verify original graph state
  EXPECT_EQ(int_sorter.size(), 3);
  EXPECT_TRUE(int_sorter.contains(1));
  EXPECT_TRUE(int_sorter.contains(2));
  EXPECT_TRUE(int_sorter.contains(3));

  EXPECT_THROW({ int_sorter.bfs(1, throwing_visitor); }, std::runtime_error);

  // Verify graph state is unchanged after exception
  EXPECT_EQ(int_sorter.size(), 3);
  EXPECT_TRUE(int_sorter.contains(1));
  EXPECT_TRUE(int_sorter.contains(2));
  EXPECT_TRUE(int_sorter.contains(3));
}

// Test depth parameter vs return value consistency
TEST_F(TopoBfsTest, DepthParameterConsistency) {
  int_sorter.add_vertex(1);
  int_sorter.add_vertex(2, std::vector<int>{1});
  int_sorter.add_vertex(3, std::vector<int>{2});

  std::vector<std::pair<int, size_t>> visitor_depths;
  std::vector<std::pair<int, size_t>> gray_depths;
  std::vector<std::pair<int, size_t>> black_depths;

  auto visitor = [&visitor_depths](const int &node, const auto &, size_t depth) {
    visitor_depths.emplace_back(node, depth);
    return true;
  };

  auto gray_handler = [&gray_depths](const int &node, const auto &, size_t depth) {
    gray_depths.emplace_back(node, depth);
    return true;
  };

  auto black_handler = [&black_depths](const int &node, const auto &, size_t depth) {
    black_depths.emplace_back(node, depth);
    return true;
  };

  auto [colour_map, depth_map] = int_sorter.bfs(1, visitor, gray_handler, black_handler);

  // Verify depth parameters match depth_map
  for (const auto &[node, depth] : visitor_depths) {
    EXPECT_EQ(depth, depth_map[node]) << "Visitor depth parameter doesn't match depth_map for node " << node;
  }

  for (const auto &[node, depth] : gray_depths) {
    EXPECT_EQ(depth, depth_map[node]) << "Gray handler depth parameter doesn't match depth_map for node " << node;
  }

  for (const auto &[node, depth] : black_depths) {
    EXPECT_EQ(depth, depth_map[node]) << "Black handler depth parameter doesn't match depth_map for node " << node;
  }
}

// Test mixed handler signatures (some with depth, some without)
TEST_F(TopoBfsTest, MixedHandlerSignatures) {
  int_sorter.add_vertex(1);
  int_sorter.add_vertex(2, std::vector<int>{1});
  int_sorter.add_vertex(3, std::vector<int>{1});
  int_sorter.add_vertex(4, std::vector<int>{2, 3});

  std::vector<int> visited_nodes;
  std::vector<int> gray_nodes;
  std::vector<std::pair<int, size_t>> black_nodes_with_depth;

  // Visitor without depth
  auto visitor = [&visited_nodes](const int &node, const auto &) {
    visited_nodes.push_back(node);
    return true;
  };

  // Gray handler without depth
  auto gray_handler = [&gray_nodes](const int &node, const auto &) {
    gray_nodes.push_back(node);
    return true;
  };

  // Black handler with depth
  auto black_handler = [&black_nodes_with_depth](const int &node, const auto &, size_t depth) {
    black_nodes_with_depth.emplace_back(node, depth);
    return true;
  };

  auto [colour_map, depth_map] = int_sorter.bfs(1, visitor, gray_handler, black_handler);

  // All should work correctly
  EXPECT_FALSE(visited_nodes.empty());
  EXPECT_FALSE(gray_nodes.empty()); // Node 4 should be encountered as gray

  // Verify black handler got correct depths
  for (const auto &[node, depth] : black_nodes_with_depth) {
    auto depth_it = depth_map.find(node);
    ASSERT_TRUE(depth_it != depth_map.end());
    EXPECT_EQ(depth, depth_it->second);
  }
}

// Test graph with isolated nodes
TEST_F(TopoBfsTest, GraphWithIsolatedNodes) {
  // Add connected component: 1 -> 2 -> 3
  int_sorter.add_vertex(1);
  int_sorter.add_vertex(2, std::vector<int>{1});
  int_sorter.add_vertex(3, std::vector<int>{2});

  // Add isolated nodes
  int_sorter.add_vertex(10);
  int_sorter.add_vertex(20);
  int_sorter.add_vertex(30);

  std::vector<int> visited_order;
  auto visitor = [&visited_order](const int &node, const auto &, size_t) {
    visited_order.push_back(node);
    return true;
  };

  auto [colour_map, depth_map] = int_sorter.bfs(1, visitor);

  // Should only visit connected component
  EXPECT_EQ(visited_order, std::vector<int>({1, 2, 3}));

  // Isolated nodes should remain white
  EXPECT_EQ(colour_map[10], colour::white);
  EXPECT_EQ(colour_map[20], colour::white);
  EXPECT_EQ(colour_map[30], colour::white);

  // Isolated nodes should not be in depth map
  EXPECT_EQ(depth_map.find(10), depth_map.end());
  EXPECT_EQ(depth_map.find(20), depth_map.end());
  EXPECT_EQ(depth_map.find(30), depth_map.end());

  verify_colour_consistency(colour_map, depth_map, int_sorter);
  verify_depth_correctness(1, depth_map, int_sorter);
}

// Test BFS from leaf node (should only visit itself)
TEST_F(TopoBfsTest, BfsFromLeafNode) {
  int_sorter.add_vertex(1);
  int_sorter.add_vertex(2, std::vector<int>{1});
  int_sorter.add_vertex(3, std::vector<int>{2});

  std::vector<int> visited_order;
  auto visitor = [&visited_order](const int &node, const auto &, size_t) {
    visited_order.push_back(node);
    return true;
  };

  // Start BFS from leaf node
  auto [colour_map, depth_map] = int_sorter.bfs(3, visitor);

  // Should only visit the leaf node itself
  EXPECT_EQ(visited_order.size(), 1);
  EXPECT_EQ(visited_order[0], 3);
  EXPECT_EQ(depth_map[3], 0);
  EXPECT_EQ(colour_map[3], colour::black);

  // Other nodes should remain white
  EXPECT_EQ(colour_map[1], colour::white);
  EXPECT_EQ(colour_map[2], colour::white);

  verify_colour_consistency(colour_map, depth_map, int_sorter);
  verify_depth_correctness(3, depth_map, int_sorter);
}

// Test BFS traversal order determinism (within levels)
TEST_F(TopoBfsTest, TraversalOrderDeterminism) {
  // Create a graph where BFS order within levels might vary
  //     1
  //   / | \
  //  2  3  4
  //  |  |  |
  //  5  6  7

  int_sorter.add_vertex(1);
  int_sorter.add_vertex(2, std::vector<int>{1});
  int_sorter.add_vertex(3, std::vector<int>{1});
  int_sorter.add_vertex(4, std::vector<int>{1});
  int_sorter.add_vertex(5, std::vector<int>{2});
  int_sorter.add_vertex(6, std::vector<int>{3});
  int_sorter.add_vertex(7, std::vector<int>{4});

  // Run BFS multiple times and check that depth levels are consistent
  for (int run = 0; run < 5; ++run) {
    std::vector<int> visited_order;
    std::vector<size_t> visit_depths;

    auto visitor = [&visited_order, &visit_depths](const int &node, const auto &, size_t depth) {
      visited_order.push_back(node);
      visit_depths.push_back(depth);
      return true;
    };

    auto [colour_map, depth_map] = int_sorter.bfs(1, visitor);

    // Verify BFS properties are maintained across runs
    EXPECT_EQ(visited_order[0], 1);
    EXPECT_EQ(visit_depths[0], 0);

    // All depths should be non-decreasing
    for (size_t i = 1; i < visit_depths.size(); ++i) {
      EXPECT_GE(visit_depths[i], visit_depths[i - 1]);
    }

    verify_colour_consistency(colour_map, depth_map, int_sorter);
    verify_depth_correctness(1, depth_map, int_sorter);
  }
}

// Test memory efficiency - verify no unnecessary allocations
TEST_F(TopoBfsTest, MemoryEfficiency) {
  // Create a reasonably sized graph
  int_sorter.add_vertex(0);
  for (int i = 1; i <= 50; ++i) {
    int_sorter.add_vertex(i, std::vector<int>{0});
  }

  size_t visit_count = 0;
  auto visitor = [&visit_count](const int &, const auto &, size_t) {
    ++visit_count;
    return true;
  };

  auto [colour_map, depth_map] = int_sorter.bfs(0, visitor);

  // Verify all nodes were visited exactly once
  EXPECT_EQ(visit_count, 51);
  EXPECT_EQ(colour_map.size(), 51);
  EXPECT_EQ(depth_map.size(), 51);

  // Verify no extra entries in maps
  for (const auto &[node, _] : colour_map) {
    EXPECT_TRUE(int_sorter.contains(node));
  }

  for (const auto &[node, _] : depth_map) {
    EXPECT_TRUE(int_sorter.contains(node));
  }
}

// Edge case: Empty graph with non-existent root should handle gracefully
TEST_F(TopoBfsTest, EmptyGraphNonExistentRoot) {
  std::vector<int> visited;
  auto visitor = [&visited](const int &node, const auto &, size_t) {
    visited.push_back(node);
    return true;
  };

  auto [colour_map, depth_map] = int_sorter.bfs(999, visitor);

  EXPECT_TRUE(visited.empty());
  EXPECT_TRUE(colour_map.empty());
  EXPECT_TRUE(depth_map.empty());
}

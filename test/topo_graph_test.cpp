#include <gtest/gtest.h>
#include <string>
#include <unordered_set>
#include <vector>

#include "opflow/graph.hpp"
#include "opflow/topo_graph.hpp"

using namespace opflow;

// Test fixture for topo_graph tests
class TopoGraphTest : public ::testing::Test {
protected:
  void SetUp() override {}

  // Helper function to create a simple linear chain: A -> B -> C
  graph<int> create_linear_chain() {
    graph<int> g;
    g.add_vertex(1);      // A (no dependencies)
    g.add_vertex(2, {1}); // B depends on A
    g.add_vertex(3, {2}); // C depends on B
    return g;
  }

  // Helper function to create a diamond pattern: A -> B, C; B,C -> D
  graph<int> create_diamond() {
    graph<int> g;
    g.add_vertex(1);         // A (no dependencies)
    g.add_vertex(2, {1});    // B depends on A
    g.add_vertex(3, {1});    // C depends on A
    g.add_vertex(4, {2, 3}); // D depends on B and C
    return g;
  }

  // Helper function to create a complex graph
  graph<int> create_complex_graph() {
    graph<int> g;
    g.add_vertex(1);         // Root
    g.add_vertex(2);         // Another root
    g.add_vertex(3, {1});    // depends on 1
    g.add_vertex(4, {1, 2}); // depends on 1, 2
    g.add_vertex(5, {3});    // depends on 3
    g.add_vertex(6, {3, 4}); // depends on 3, 4
    g.add_vertex(7, {5, 6}); // depends on 5, 6
    return g;
  }

  // Helper function to create a graph with cycle
  graph<int> create_cyclic_graph() {
    graph<int> g;
    g.add_vertex(1, {{3, 0}}); // 1 depends on 3
    g.add_vertex(2, {{1, 0}}); // 2 depends on 1
    g.add_vertex(3, {{2, 0}}); // 3 depends on 2 (creates cycle)
    return g;
  }

  // Helper function to create a self-loop graph
  graph<int> create_self_loop_graph() {
    graph<int> g;
    g.add_vertex(1, {1}); // Self dependency
    return g;
  }

  // Helper function to validate topological order
  template <typename T>
  bool is_valid_topological_order(const topo_graph<T> &tg, const graph<T> &original) {
    // Check that all dependencies come before dependents
    for (size_t i = 0; i < tg.size(); ++i) {
      const T &current = tg[i];
      auto preds = tg.preds(i);

      // All predecessor indices should be less than i
      for (size_t pred_id : preds) {
        if (pred_id >= i) {
          return false;
        }
      }

      // Verify that the predecessors in topo_graph match original graph
      auto original_preds = original.pred_of(current);
      if (preds.size() != original_preds.size()) {
        return false;
      }

      std::unordered_set<T> original_pred_set(original_preds.begin(), original_preds.end());
      for (size_t pred_id : preds) {
        if (original_pred_set.find(tg[pred_id]) == original_pred_set.end()) {
          return false;
        }
      }
    }
    return true;
  }

  // Helper to check if all nodes from original graph are present
  template <typename T>
  bool contains_all_nodes(const topo_graph<T> &tg, const graph<T> &original) {
    if (tg.size() != original.size()) {
      return false;
    }

    std::unordered_set<T> tg_nodes;
    for (size_t i = 0; i < tg.size(); ++i) {
      tg_nodes.insert(tg[i]);
    }

    for (const auto &[node, _] : original.get_pred()) {
      if (tg_nodes.find(node) == tg_nodes.end()) {
        return false;
      }
    }

    return true;
  }
};

// Test fixture for string-based graphs
class TopoGraphStringTest : public ::testing::Test {
protected:
  // Helper to create a string graph
  graph<std::string> create_string_graph() {
    graph<std::string> g;
    g.add_vertex("root");
    g.add_vertex("child1", std::vector<std::string>{"root"});
    g.add_vertex("child2", std::vector<std::string>{"root"});
    g.add_vertex("grandchild", std::vector<std::string>{"child1", "child2"});
    return g;
  }
};

// Basic construction tests
TEST_F(TopoGraphTest, ConstructFromEmptyGraph) {
  graph<int> empty_graph;
  topo_graph<int> tg(empty_graph);

  EXPECT_TRUE(tg.empty());
  EXPECT_EQ(tg.size(), 0);
}

TEST_F(TopoGraphTest, ConstructFromSingleNode) {
  graph<int> g;
  g.add_vertex(42);
  topo_graph<int> tg(g);

  EXPECT_FALSE(tg.empty());
  EXPECT_EQ(tg.size(), 1);
  EXPECT_EQ(tg[0], 42);
  EXPECT_TRUE(tg.contains_id(0));
  EXPECT_TRUE(tg.contains_node(42));
  EXPECT_FALSE(tg.contains_node(1));
  EXPECT_FALSE(tg.contains_node(43));

  auto preds = tg.preds(0);
  EXPECT_EQ(preds.size(), 0);
}

TEST_F(TopoGraphTest, ConstructFromLinearChain) {
  auto g = create_linear_chain();
  topo_graph<int> tg(g);

  EXPECT_EQ(tg.size(), 3);
  EXPECT_TRUE(contains_all_nodes(tg, g));
  EXPECT_TRUE(is_valid_topological_order(tg, g));

  // In a linear chain A->B->C, the order should be A, B, C
  // But since nodes are integers, we need to check the actual ordering
  EXPECT_EQ(tg[0], 1); // First node should be 1 (no dependencies)
  EXPECT_EQ(tg[1], 2); // Second node should be 2 (depends on 1)
  EXPECT_EQ(tg[2], 3); // Third node should be 3 (depends on 2)

  // Check predecessors
  EXPECT_EQ(tg.preds(0).size(), 0); // Node 1 has no predecessors
  EXPECT_EQ(tg.preds(1).size(), 1); // Node 2 has 1 predecessor
  EXPECT_EQ(tg.preds(1)[0], 0);     // Node 2's predecessor is at index 0
  EXPECT_EQ(tg.preds(2).size(), 1); // Node 3 has 1 predecessor
  EXPECT_EQ(tg.preds(2)[0], 1);     // Node 3's predecessor is at index 1
}

TEST_F(TopoGraphTest, ConstructFromDiamondPattern) {
  auto g = create_diamond();
  topo_graph<int> tg(g);

  EXPECT_EQ(tg.size(), 4);
  EXPECT_TRUE(contains_all_nodes(tg, g));
  EXPECT_TRUE(is_valid_topological_order(tg, g));

  // First node should be 1 (no dependencies)
  EXPECT_EQ(tg[0], 1);
  EXPECT_EQ(tg.preds(0).size(), 0);

  // Node 4 should be last (depends on 2 and 3)
  EXPECT_EQ(tg[3], 4);
  EXPECT_EQ(tg.preds(3).size(), 2);

  // The middle two nodes should be 2 and 3 in some order
  std::unordered_set<int> middle_nodes{tg[1], tg[2]};
  EXPECT_TRUE(middle_nodes.count(2) && middle_nodes.count(3));
}

TEST_F(TopoGraphTest, ConstructFromComplexGraph) {
  auto g = create_complex_graph();
  topo_graph<int> tg(g);

  EXPECT_EQ(tg.size(), 7);
  EXPECT_TRUE(contains_all_nodes(tg, g));
  EXPECT_TRUE(is_valid_topological_order(tg, g));

  // Roots (1, 2) should come first
  std::unordered_set<int> first_two{tg[0], tg[1]};
  EXPECT_TRUE(first_two.count(1) && first_two.count(2));

  // Node 7 should be last
  EXPECT_EQ(tg[6], 7);
}

// Cycle detection tests
TEST_F(TopoGraphTest, ThrowsOnCyclicGraph) {
  auto g = create_cyclic_graph();
  EXPECT_THROW(topo_graph<int> tg(g), std::runtime_error);
}

TEST_F(TopoGraphTest, ThrowsOnSelfLoop) {
  auto g = create_self_loop_graph();
  EXPECT_THROW(topo_graph<int> tg(g), std::runtime_error);
}

// Edge cases
TEST_F(TopoGraphTest, MultipleRoots) {
  graph<int> g;
  g.add_vertex(1);                            // Root 1
  g.add_vertex(2);                            // Root 2
  g.add_vertex(3);                            // Root 3
  g.add_vertex(4, std::vector<int>{1, 2, 3}); // Depends on all roots

  topo_graph<int> tg(g);

  EXPECT_EQ(tg.size(), 4);
  EXPECT_TRUE(is_valid_topological_order(tg, g));

  // Node 4 should be last
  EXPECT_EQ(tg[3], 4);
  EXPECT_EQ(tg.preds(3).size(), 3);

  // First three should be the roots in some order
  std::unordered_set<int> roots{tg[0], tg[1], tg[2]};
  EXPECT_TRUE(roots.count(1) && roots.count(2) && roots.count(3));
}

TEST_F(TopoGraphTest, DisconnectedComponents) {
  graph<int> g;
  // Component 1: 1 -> 2
  g.add_vertex(1);
  g.add_vertex(2, std::vector<int>{1});

  // Component 2: 3 -> 4
  g.add_vertex(3);
  g.add_vertex(4, std::vector<int>{3});

  topo_graph<int> tg(g);

  EXPECT_EQ(tg.size(), 4);
  EXPECT_TRUE(contains_all_nodes(tg, g));
  EXPECT_TRUE(is_valid_topological_order(tg, g));
}

TEST_F(TopoGraphTest, NodesWithNoPredecessors) {
  graph<int> g;
  g.add_vertex(1);
  g.add_vertex(2);
  g.add_vertex(3);

  topo_graph<int> tg(g);

  EXPECT_EQ(tg.size(), 3);
  EXPECT_TRUE(contains_all_nodes(tg, g));

  // All nodes should have no predecessors
  for (size_t i = 0; i < tg.size(); ++i) {
    EXPECT_EQ(tg.preds(i).size(), 0);
  }
}

// String-based tests
TEST_F(TopoGraphStringTest, StringNodes) {
  auto g = create_string_graph();
  topo_graph<std::string> tg(g);

  EXPECT_EQ(tg.size(), 4);
  EXPECT_TRUE(tg.contains_node("root"));
  EXPECT_TRUE(tg.contains_node("child1"));
  EXPECT_TRUE(tg.contains_node("child2"));
  EXPECT_TRUE(tg.contains_node("grandchild"));
  EXPECT_FALSE(tg.contains_node("nonexistent"));

  // Root should be first
  EXPECT_EQ(tg[0], "root");

  // Grandchild should be last
  EXPECT_EQ(tg[3], "grandchild");
}

// Accessor tests
TEST_F(TopoGraphTest, ContainsByIndex) {
  auto g = create_linear_chain();
  topo_graph<int> tg(g);

  EXPECT_TRUE(tg.contains_id(0));
  EXPECT_TRUE(tg.contains_id(1));
  EXPECT_TRUE(tg.contains_id(2));
  EXPECT_FALSE(tg.contains_id(3));
  EXPECT_FALSE(tg.contains_id(static_cast<size_t>(-1)));
}

TEST_F(TopoGraphTest, ContainsByValue) {
  auto g = create_linear_chain();
  topo_graph<int> tg(g);

  EXPECT_TRUE(tg.contains_node(1));
  EXPECT_TRUE(tg.contains_node(2));
  EXPECT_TRUE(tg.contains_node(3));
  EXPECT_FALSE(tg.contains_node(4));
  EXPECT_FALSE(tg.contains_node(0));
}

TEST_F(TopoGraphTest, IndexOperator) {
  auto g = create_linear_chain();
  topo_graph<int> tg(g);

  // Test that we can access all elements
  for (size_t i = 0; i < tg.size(); ++i) {
    EXPECT_NO_THROW(tg[i]);
  }

  // Accessing out of bounds should be undefined behavior
  // We don't test this as it's implementation-defined
}

TEST_F(TopoGraphTest, PredsMethod) {
  auto g = create_diamond();
  topo_graph<int> tg(g);

  // Test that preds returns spans with correct sizes
  for (size_t i = 0; i < tg.size(); ++i) {
    auto preds = tg.preds(i);
    EXPECT_NO_THROW(preds.size());

    // All predecessor IDs should be valid
    for (size_t pred_id : preds) {
      EXPECT_LT(pred_id, tg.size());
      EXPECT_LT(pred_id, i); // Topological order constraint
    }
  }
}

// Memory and performance tests
TEST_F(TopoGraphTest, LargeGraph) {
  graph<int> g;

  // Create a large linear chain
  constexpr int N = 1000;
  g.add_vertex(0);
  for (int i = 1; i < N; ++i) {
    g.add_vertex(i, std::vector<int>{i - 1});
  }

  topo_graph<int> tg(g);

  EXPECT_EQ(tg.size(), N);
  EXPECT_TRUE(is_valid_topological_order(tg, g));

  // Check first and last elements
  EXPECT_EQ(tg[0], 0);
  EXPECT_EQ(tg[N - 1], N - 1);

  // Check that each node has exactly one predecessor (except the first)
  EXPECT_EQ(tg.preds(0).size(), 0);
  for (size_t i = 1; i < static_cast<size_t>(N); ++i) {
    EXPECT_EQ(tg.preds(i).size(), 1);
    EXPECT_EQ(tg.preds(i)[0], i - 1);
  }
}

TEST_F(TopoGraphTest, WideGraph) {
  graph<int> g;

  // Create a graph with one root and many children
  constexpr int N = 100;
  g.add_vertex(0); // Root
  for (int i = 1; i <= N; ++i) {
    g.add_vertex(i, std::vector<int>{0});
  }

  topo_graph<int> tg(g);

  EXPECT_EQ(tg.size(), N + 1);
  EXPECT_TRUE(is_valid_topological_order(tg, g));

  // Root should be first
  EXPECT_EQ(tg[0], 0);
  EXPECT_EQ(tg.preds(0).size(), 0);

  // All other nodes should have root as predecessor
  for (int i = 1; i <= N; ++i) {
    // Find the index of node i in the topological order
    size_t idx = static_cast<size_t>(-1);
    for (size_t j = 0; j < tg.size(); ++j) {
      if (tg[j] == i) {
        idx = j;
        break;
      }
    }
    EXPECT_NE(idx, static_cast<size_t>(-1)) << "Node " << i << " not found in topo_graph";
    EXPECT_EQ(tg.preds(idx).size(), 1);
    EXPECT_EQ(tg.preds(idx)[0], 0);
  }
}

// Template deduction guide test
TEST_F(TopoGraphTest, DeductionGuide) {
  auto g = create_linear_chain();
  auto tg = topo_graph(g); // Should deduce topo_graph<int>

  EXPECT_EQ(tg.size(), 3);
  static_assert(std::is_same_v<decltype(tg), topo_graph<int>>);
}

// Iteration tests (if iterators are added in the future)
TEST_F(TopoGraphTest, RangeBasedForLoop) {
  auto g = create_linear_chain();
  topo_graph<int> tg(g);

  std::vector<int> collected;
  for (size_t i = 0; i < tg.size(); ++i) {
    collected.push_back(tg[i]);
  }

  EXPECT_EQ(collected.size(), 3);
  EXPECT_EQ(collected[0], 1);
  EXPECT_EQ(collected[1], 2);
  EXPECT_EQ(collected[2], 3);
}

// Edge case: Empty predecessor lists in complex scenarios
TEST_F(TopoGraphTest, MixedEmptyAndNonEmptyPreds) {
  graph<int> g;
  g.add_vertex(1);                         // No predecessors
  g.add_vertex(2, std::vector<int>{});     // Explicitly empty predecessors
  g.add_vertex(3, std::vector<int>{1, 2}); // Multiple predecessors

  topo_graph<int> tg(g);

  EXPECT_EQ(tg.size(), 3);
  EXPECT_TRUE(is_valid_topological_order(tg, g));

  // Node 3 should be last
  EXPECT_EQ(tg[2], 3);
  EXPECT_EQ(tg.preds(2).size(), 2);
}

// Stress test for complex dependencies
TEST_F(TopoGraphTest, ComplexDependencyPatterns) {
  graph<int> g;

  // Create a more complex dependency pattern
  g.add_vertex(1); // Root
  g.add_vertex(2, std::vector<int>{1});
  g.add_vertex(3, std::vector<int>{1});
  g.add_vertex(4, std::vector<int>{2, 3});
  g.add_vertex(5, std::vector<int>{2});
  g.add_vertex(6, std::vector<int>{3});
  g.add_vertex(7, std::vector<int>{4, 5, 6});

  topo_graph<int> tg(g);

  EXPECT_EQ(tg.size(), 7);
  EXPECT_TRUE(is_valid_topological_order(tg, g));

  // Root should be first
  EXPECT_EQ(tg[0], 1);

  // Node 7 should be last
  EXPECT_EQ(tg[6], 7);
  EXPECT_EQ(tg.preds(6).size(), 3);

  auto leaf = tg.leaf_ids();
  EXPECT_EQ(leaf.size(), 1);
  EXPECT_EQ(leaf[0], 6); // Node 7 is the only leaf
}

// Test with duplicate node values (should not happen in practice, but test robustness)
TEST_F(TopoGraphTest, NodeValueUniqueness) {
  auto g = create_linear_chain();
  topo_graph<int> tg(g);

  std::unordered_set<int> seen_values;
  for (size_t i = 0; i < tg.size(); ++i) {
    int value = tg[i];
    EXPECT_TRUE(seen_values.insert(value).second) << "Duplicate value: " << value;
  }
}

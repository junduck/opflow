#include <gtest/gtest.h>

#include "opflow/graph_node.hpp"

#include <algorithm>
#include <memory>

using namespace opflow;

struct dummy_node {
  using data_type = double;

  dummy_node() = default;
  dummy_node(int val) : value(val) {}
  dummy_node(std::string name) : name(std::move(name)) {}
  dummy_node(std::string name, int val) : name(std::move(name)), value(val) {}

  void clone_at(void *) const {};
  size_t clone_size() const noexcept { return sizeof(dummy_node); };
  size_t clone_align() const noexcept { return alignof(dummy_node); }
  dummy_node const *observer() const noexcept { return this; }

  std::string name;
  int value = 0;

  bool operator==(dummy_node const &other) const { return name == other.name && value == other.value; }
};

static_assert(dag_node<dummy_node>);
static_assert(dag_node_ptr<std::shared_ptr<dummy_node>>);
static_assert(dag_node_ptr<dummy_node const *>);

class GraphNodeTest : public ::testing::Test {
protected:
  void SetUp() override { g.clear(); }

  graph_node<dummy_node> g;

  // Helper function to create a dummy node
  auto make_node(std::string name = "", int value = 0) { return std::make_shared<dummy_node>(std::move(name), value); }

  // Helper function to verify node exists and has expected content
  void verify_node(std::shared_ptr<dummy_node> const &node, std::string const &expected_name, int expected_value = 0) {
    ASSERT_TRUE(node != nullptr);
    EXPECT_EQ(node->name, expected_name);
    EXPECT_EQ(node->value, expected_value);
  }
};

// Basic functionality tests
TEST_F(GraphNodeTest, BasicConstruction) {
  EXPECT_TRUE(g.empty());
  EXPECT_EQ(g.size(), 0);
}

TEST_F(GraphNodeTest, AddSingleNode) {
  auto nodeA = make_node("A");
  g.add(nodeA);

  EXPECT_FALSE(g.empty());
  EXPECT_EQ(g.size(), 1);
  EXPECT_TRUE(g.contains(nodeA));
  EXPECT_TRUE(g.is_root(nodeA));
  EXPECT_TRUE(g.is_leaf(nodeA));
}

TEST_F(GraphNodeTest, AddNodeWithSinglePredecessor) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");

  g.add(nodeB, nodeA);

  EXPECT_EQ(g.size(), 2);
  EXPECT_TRUE(g.contains(nodeA));
  EXPECT_TRUE(g.contains(nodeB));

  // Check adjacency
  auto const &pred_B = g.pred_of(nodeB);
  EXPECT_EQ(pred_B.size(), 1);
  EXPECT_TRUE(pred_B.count(nodeA));

  auto const &succ_A = g.succ_of(nodeA);
  EXPECT_EQ(succ_A.size(), 1);
  EXPECT_TRUE(succ_A.count(nodeB));

  // Check arguments
  auto const &args_B = g.args_of(nodeB);
  EXPECT_EQ(args_B.size(), 1);
  EXPECT_EQ(args_B[0].node, nodeA);
  EXPECT_EQ(args_B[0].port, 0);

  // Check root/leaf status
  EXPECT_TRUE(g.is_root(nodeA));
  EXPECT_FALSE(g.is_root(nodeB));
  EXPECT_FALSE(g.is_leaf(nodeA));
  EXPECT_TRUE(g.is_leaf(nodeB));
}

TEST_F(GraphNodeTest, AddNodeWithMultiplePredecessors) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");
  auto nodeC = make_node("C");

  g.add(nodeC, {nodeA, nodeB});

  EXPECT_EQ(g.size(), 3);

  auto const &pred_C = g.pred_of(nodeC);
  EXPECT_EQ(pred_C.size(), 2);
  EXPECT_TRUE(pred_C.count(nodeA));
  EXPECT_TRUE(pred_C.count(nodeB));

  auto const &args_C = g.args_of(nodeC);
  EXPECT_EQ(args_C.size(), 2);
  // Arguments should preserve order
  EXPECT_EQ(args_C[0].node, nodeA);
  EXPECT_EQ(args_C[0].port, 0);
  EXPECT_EQ(args_C[1].node, nodeB);
  EXPECT_EQ(args_C[1].port, 0);
}

TEST_F(GraphNodeTest, AddNodeWithPortSpecification) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");
  auto nodeC = make_node("C");

  g.add(nodeC, {nodeA | 0, nodeB | 1});

  auto const &args_C = g.args_of(nodeC);
  EXPECT_EQ(args_C.size(), 2);
  EXPECT_EQ(args_C[0].node, nodeA);
  EXPECT_EQ(args_C[0].port, 0);
  EXPECT_EQ(args_C[1].node, nodeB);
  EXPECT_EQ(args_C[1].port, 1);
}

TEST_F(GraphNodeTest, AddNodeWithMakeEdge) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");
  auto nodeC = make_node("C");

  g.add(nodeC, {make_edge(nodeA, 2), make_edge(nodeB, 3)});

  auto const &args_C = g.args_of(nodeC);
  EXPECT_EQ(args_C.size(), 2);
  EXPECT_EQ(args_C[0].node, nodeA);
  EXPECT_EQ(args_C[0].port, 2);
  EXPECT_EQ(args_C[1].node, nodeB);
  EXPECT_EQ(args_C[1].port, 3);
}

// In-place construction tests
TEST_F(GraphNodeTest, InPlaceConstructionWithPredecessors) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");

  auto nodeC = g.add<dummy_node>({nodeA, nodeB}, "C", 42);

  EXPECT_EQ(g.size(), 3);
  verify_node(nodeC, "C", 42);

  auto const &pred_C = g.pred_of(nodeC);
  EXPECT_EQ(pred_C.size(), 2);
  EXPECT_TRUE(pred_C.count(nodeA));
  EXPECT_TRUE(pred_C.count(nodeB));
}

TEST_F(GraphNodeTest, InPlaceConstructionWithInitializerList) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");

  auto nodeC = g.add<dummy_node>({nodeA, nodeB}, "C", 99);

  verify_node(nodeC, "C", 99);
  EXPECT_EQ(g.size(), 3);
}

TEST_F(GraphNodeTest, InPlaceConstructionWithSinglePredecessor) {
  auto nodeA = make_node("A");

  auto nodeB = g.add<dummy_node>(nodeA, "B", 123);

  verify_node(nodeB, "B", 123);
  EXPECT_EQ(g.size(), 2);

  auto const &pred_B = g.pred_of(nodeB);
  EXPECT_EQ(pred_B.size(), 1);
  EXPECT_TRUE(pred_B.count(nodeA));
}

TEST_F(GraphNodeTest, InPlaceConstructionWithEdgeType) {
  auto nodeA = make_node("A");

  auto nodeB = g.add<dummy_node>(nodeA | 5, "B", 456);

  verify_node(nodeB, "B", 456);

  auto const &args_B = g.args_of(nodeB);
  EXPECT_EQ(args_B.size(), 1);
  EXPECT_EQ(args_B[0].node, nodeA);
  EXPECT_EQ(args_B[0].port, 5);
}

TEST_F(GraphNodeTest, RootNodeConstruction) {
  auto nodeA = g.root<dummy_node>("A", 777);

  verify_node(nodeA, "A", 777);
  EXPECT_EQ(g.size(), 1);
  EXPECT_TRUE(g.is_root(nodeA));
  EXPECT_TRUE(g.is_leaf(nodeA));
}

// Edge case tests
TEST_F(GraphNodeTest, SelfLoops) {
  auto nodeA = make_node("A");
  g.add(nodeA, nodeA); // Self-loop

  EXPECT_EQ(g.size(), 1);

  auto const &pred_A = g.pred_of(nodeA);
  EXPECT_EQ(pred_A.size(), 1);
  EXPECT_TRUE(pred_A.count(nodeA));

  auto const &succ_A = g.succ_of(nodeA);
  EXPECT_EQ(succ_A.size(), 1);
  EXPECT_TRUE(succ_A.count(nodeA));

  EXPECT_FALSE(g.is_root(nodeA)); // Has predecessor (itself)
  EXPECT_FALSE(g.is_leaf(nodeA)); // Has successor (itself)
}

TEST_F(GraphNodeTest, DuplicateEdges) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");

  // Add multiple edges from B to A with different ports
  g.add(nodeB, {nodeA | 0, nodeA | 1, nodeA | 0});

  auto const &pred_B = g.pred_of(nodeB);
  EXPECT_EQ(pred_B.size(), 1); // Only one unique predecessor
  EXPECT_TRUE(pred_B.count(nodeA));

  auto const &args_B = g.args_of(nodeB);
  EXPECT_EQ(args_B.size(), 3); // But three arguments (duplicates allowed)
  EXPECT_EQ(args_B[0].node, nodeA);
  EXPECT_EQ(args_B[0].port, 0);
  EXPECT_EQ(args_B[1].node, nodeA);
  EXPECT_EQ(args_B[1].port, 1);
  EXPECT_EQ(args_B[2].node, nodeA);
  EXPECT_EQ(args_B[2].port, 0);
}

// Removal tests
TEST_F(GraphNodeTest, RemoveNode) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");
  auto nodeC = make_node("C");
  auto nodeD = make_node("D");

  g.add(nodeC, {nodeA, nodeB});
  g.add(nodeD, nodeC);

  EXPECT_EQ(g.size(), 4);

  g.rm(nodeC);

  EXPECT_EQ(g.size(), 3);
  EXPECT_FALSE(g.contains(nodeC));
  EXPECT_TRUE(g.contains(nodeA));
  EXPECT_TRUE(g.contains(nodeB));
  EXPECT_TRUE(g.contains(nodeD));

  // D should have no predecessors now
  auto const &pred_D = g.pred_of(nodeD);
  EXPECT_TRUE(pred_D.empty());

  // A and B should have no successors
  auto const &succ_A = g.succ_of(nodeA);
  auto const &succ_B = g.succ_of(nodeB);
  EXPECT_TRUE(succ_A.empty());
  EXPECT_TRUE(succ_B.empty());
}

TEST_F(GraphNodeTest, RemoveNonExistentNode) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");

  g.add(nodeA);
  g.rm(nodeB); // Remove non-existent node

  EXPECT_EQ(g.size(), 1);
  EXPECT_TRUE(g.contains(nodeA));
}

// Edge removal tests
TEST_F(GraphNodeTest, RemoveEdge) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");

  g.add(nodeB, {nodeA | 0, nodeA | 1});

  g.rm_edge(nodeB, nodeA | 1);

  auto const &args_B = g.args_of(nodeB);
  EXPECT_EQ(args_B.size(), 1);
  EXPECT_EQ(args_B[0].node, nodeA);
  EXPECT_EQ(args_B[0].port, 0);

  // A should still be a predecessor since there's still A:0
  auto const &pred_B = g.pred_of(nodeB);
  EXPECT_EQ(pred_B.size(), 1);
  EXPECT_TRUE(pred_B.count(nodeA));
}

TEST_F(GraphNodeTest, RemoveAllEdgesToSamePredecessor) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");

  g.add(nodeB, {nodeA | 0, nodeA | 1});

  // Remove all edges to A
  g.rm_edge(nodeB, nodeA | 0);
  g.rm_edge(nodeB, nodeA | 1);

  auto const &args_B = g.args_of(nodeB);
  EXPECT_TRUE(args_B.empty());

  // A should no longer be a predecessor
  auto const &pred_B = g.pred_of(nodeB);
  EXPECT_TRUE(pred_B.empty());

  // B should no longer be a successor of A
  auto const &succ_A = g.succ_of(nodeA);
  EXPECT_TRUE(succ_A.empty());
}

TEST_F(GraphNodeTest, RemoveEdgeFromNonExistentNode) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");

  g.add(nodeA);
  g.rm_edge(nodeB, nodeA); // Remove edge from non-existent node

  EXPECT_EQ(g.size(), 1); // Should not affect the graph
}

TEST_F(GraphNodeTest, RemoveNonExistentEdge) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");
  auto nodeC = make_node("C");

  g.add(nodeB, nodeA);
  g.rm_edge(nodeB, nodeC); // Remove non-existent edge

  auto const &pred_B = g.pred_of(nodeB);
  EXPECT_EQ(pred_B.size(), 1);
  EXPECT_TRUE(pred_B.count(nodeA));
}

// Replace node tests
TEST_F(GraphNodeTest, ReplaceNode) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");
  auto nodeC = make_node("C");
  auto nodeD = make_node("D");
  auto nodeE = make_node("E");
  auto nodeX = make_node("X");

  g.add(nodeC, {nodeA, nodeB});
  g.add(nodeD, nodeC);
  g.add(nodeE, nodeC);

  g.replace(nodeX, nodeC);

  EXPECT_FALSE(g.contains(nodeC));
  EXPECT_TRUE(g.contains(nodeX));

  // X should have A and B as predecessors
  auto const &pred_X = g.pred_of(nodeX);
  EXPECT_EQ(pred_X.size(), 2);
  EXPECT_TRUE(pred_X.count(nodeA));
  EXPECT_TRUE(pred_X.count(nodeB));

  // D and E should have X as predecessor
  auto const &pred_D = g.pred_of(nodeD);
  auto const &pred_E = g.pred_of(nodeE);
  EXPECT_EQ(pred_D.size(), 1);
  EXPECT_EQ(pred_E.size(), 1);
  EXPECT_TRUE(pred_D.count(nodeX));
  EXPECT_TRUE(pred_E.count(nodeX));

  // Arguments should be updated
  auto const &args_D = g.args_of(nodeD);
  auto const &args_E = g.args_of(nodeE);
  EXPECT_EQ(args_D[0].node, nodeX);
  EXPECT_EQ(args_E[0].node, nodeX);
}

TEST_F(GraphNodeTest, ReplaceNonExistentNode) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");
  auto nodeC = make_node("C");

  g.add(nodeA);
  g.replace(nodeC, nodeB); // Replace non-existent node

  EXPECT_EQ(g.size(), 1);
  EXPECT_TRUE(g.contains(nodeA));
  EXPECT_FALSE(g.contains(nodeB));
  EXPECT_FALSE(g.contains(nodeC));
}

TEST_F(GraphNodeTest, ReplaceWithExistingNode) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");

  g.add(nodeA);
  g.add(nodeB);
  g.replace(nodeB, nodeA); // Replace with existing node

  EXPECT_EQ(g.size(), 2);
  EXPECT_TRUE(g.contains(nodeA));
  EXPECT_TRUE(g.contains(nodeB));
}

TEST_F(GraphNodeTest, ReplaceNodeWithItself) {
  auto nodeA = make_node("A");

  g.add(nodeA);
  g.replace(nodeA, nodeA); // Replace with itself

  EXPECT_EQ(g.size(), 1);
  EXPECT_TRUE(g.contains(nodeA));
}

// Replace edge tests
TEST_F(GraphNodeTest, ReplaceEdge) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");
  auto nodeC = make_node("C");
  auto nodeX = make_node("X");

  g.add(nodeC, {nodeA | 0, nodeB | 1});

  g.replace(nodeC, nodeA | 0, nodeX | 2);

  auto const &args_C = g.args_of(nodeC);
  EXPECT_EQ(args_C.size(), 2);
  EXPECT_EQ(args_C[0].node, nodeX);
  EXPECT_EQ(args_C[0].port, 2);
  EXPECT_EQ(args_C[1].node, nodeB);
  EXPECT_EQ(args_C[1].port, 1);

  // Check adjacency updates
  auto const &pred_C = g.pred_of(nodeC);
  EXPECT_EQ(pred_C.size(), 2);
  EXPECT_TRUE(pred_C.count(nodeX));
  EXPECT_TRUE(pred_C.count(nodeB));
  EXPECT_FALSE(pred_C.count(nodeA));
}

TEST_F(GraphNodeTest, ReplaceEdgeWithItself) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");

  g.add(nodeB, nodeA | 0);

  g.replace(nodeB, nodeA | 0, nodeA | 0); // Replace with itself

  auto const &args_B = g.args_of(nodeB);
  EXPECT_EQ(args_B.size(), 1);
  EXPECT_EQ(args_B[0].node, nodeA);
  EXPECT_EQ(args_B[0].port, 0);
}

TEST_F(GraphNodeTest, ReplaceNonExistentEdge) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");
  auto nodeC = make_node("C");
  auto nodeD = make_node("D");

  g.add(nodeB, nodeA);
  g.replace(nodeB, nodeC | 0, nodeD | 1); // Replace non-existent edge

  auto const &pred_B = g.pred_of(nodeB);
  EXPECT_EQ(pred_B.size(), 1);
  EXPECT_TRUE(pred_B.count(nodeA));
}

// Graph merging tests
TEST_F(GraphNodeTest, MergeDisjointGraphs) {
  graph_node<dummy_node> g2;

  auto nodeA = make_node("A");
  auto nodeB = make_node("B");
  auto nodeC = make_node("C");
  auto nodeD = make_node("D");

  g.add(nodeA);
  g.add(nodeB, nodeA);

  g2.add(nodeC);
  g2.add(nodeD, nodeC);

  g.merge(g2);

  EXPECT_EQ(g.size(), 4);
  EXPECT_TRUE(g.contains(nodeA));
  EXPECT_TRUE(g.contains(nodeB));
  EXPECT_TRUE(g.contains(nodeC));
  EXPECT_TRUE(g.contains(nodeD));

  // Check that relationships are preserved
  auto const &pred_B = g.pred_of(nodeB);
  auto const &pred_D = g.pred_of(nodeD);
  EXPECT_TRUE(pred_B.count(nodeA));
  EXPECT_TRUE(pred_D.count(nodeC));
}

TEST_F(GraphNodeTest, MergeOperatorPlus) {
  graph_node<dummy_node> g2;

  auto nodeA = make_node("A");
  auto nodeB = make_node("B");

  g.add(nodeA);
  g2.add(nodeB);

  auto g3 = g + g2;

  EXPECT_EQ(g3.size(), 2);
  EXPECT_TRUE(g3.contains(nodeA));
  EXPECT_TRUE(g3.contains(nodeB));

  // Original graphs should be unchanged
  EXPECT_EQ(g.size(), 1);
  EXPECT_EQ(g2.size(), 1);
}

TEST_F(GraphNodeTest, MergeOperatorPlusEqual) {
  graph_node<dummy_node> g2;

  auto nodeA = make_node("A");
  auto nodeB = make_node("B");

  g.add(nodeA);
  g2.add(nodeB);

  g += g2;

  EXPECT_EQ(g.size(), 2);
  EXPECT_TRUE(g.contains(nodeA));
  EXPECT_TRUE(g.contains(nodeB));
}

// Complex graph tests
TEST_F(GraphNodeTest, ComplexDAG) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");
  auto nodeC = make_node("C");
  auto nodeD = make_node("D");
  auto nodeE = make_node("E");

  g.add(nodeA);
  g.add(nodeB);
  g.add(nodeC, {nodeA, nodeB});
  g.add(nodeD, nodeA);
  g.add(nodeE, {nodeC, nodeD});

  // Check roots and leaves
  auto roots = g.get_roots();
  auto leaves = g.get_leaves();

  EXPECT_EQ(roots.size(), 2);
  EXPECT_TRUE(std::find(roots.begin(), roots.end(), nodeA) != roots.end());
  EXPECT_TRUE(std::find(roots.begin(), roots.end(), nodeB) != roots.end());

  EXPECT_EQ(leaves.size(), 1);
  EXPECT_TRUE(std::find(leaves.begin(), leaves.end(), nodeE) != leaves.end());
}

TEST_F(GraphNodeTest, ClearGraph) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");

  g.add(nodeA);
  g.add(nodeB, nodeA);

  EXPECT_FALSE(g.empty());

  g.clear();

  EXPECT_TRUE(g.empty());
  EXPECT_EQ(g.size(), 0);
  EXPECT_FALSE(g.contains(nodeA));
  EXPECT_FALSE(g.contains(nodeB));
}

// Additional edge case tests
TEST_F(GraphNodeTest, SelfLoopWithMultiplePorts) {
  auto nodeA = make_node("A");

  // Node depends on itself through multiple ports
  g.add(nodeA, {nodeA | 0, nodeA | 1, nodeA | 2});

  auto const &pred_A = g.pred_of(nodeA);
  auto const &args_A = g.args_of(nodeA);

  EXPECT_EQ(pred_A.size(), 1);
  EXPECT_TRUE(pred_A.count(nodeA));
  EXPECT_EQ(args_A.size(), 3);

  // Check all three self-references with different ports
  EXPECT_EQ(args_A[0].node, nodeA);
  EXPECT_EQ(args_A[0].port, 0);
  EXPECT_EQ(args_A[1].node, nodeA);
  EXPECT_EQ(args_A[1].port, 1);
  EXPECT_EQ(args_A[2].node, nodeA);
  EXPECT_EQ(args_A[2].port, 2);
}

TEST_F(GraphNodeTest, LargePortNumbers) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");

  g.add(nodeB, nodeA | 4294967295u); // Max uint32_t

  auto const &args = g.args_of(nodeB);
  EXPECT_EQ(args.size(), 1);
  EXPECT_EQ(args[0].port, 4294967295u);
}

TEST_F(GraphNodeTest, ManyDuplicateEdgesWithDifferentPorts) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");

  // Add many edges from B to A with sequential ports
  std::vector<graph_node<dummy_node>::edge_type> edges;
  for (uint32_t i = 0; i < 100; ++i) {
    edges.emplace_back(nodeA, i);
  }
  g.add(nodeB, edges);

  auto const &pred_B = g.pred_of(nodeB);
  auto const &args_B = g.args_of(nodeB);

  EXPECT_EQ(pred_B.size(), 1); // Only one unique predecessor
  EXPECT_TRUE(pred_B.count(nodeA));
  EXPECT_EQ(args_B.size(), 100); // But 100 arguments

  // Check that ports are in order
  for (uint32_t i = 0; i < 100; ++i) {
    EXPECT_EQ(args_B[i].node, nodeA);
    EXPECT_EQ(args_B[i].port, i);
  }
}

TEST_F(GraphNodeTest, RemoveSpecificDuplicateEdge) {
  auto nodeA = make_node("A");
  auto nodeC = make_node("C");

  g.add(nodeC, {nodeA | 1, nodeA | 2, nodeA | 1, nodeA | 3});

  // Remove all A:1 edges (should remove 2 edges)
  g.rm_edge(nodeC, nodeA | 1);

  auto const &args_C = g.args_of(nodeC);
  EXPECT_EQ(args_C.size(), 2); // Should have A:2 and A:3 left
  EXPECT_EQ(args_C[0].node, nodeA);
  EXPECT_EQ(args_C[0].port, 2);
  EXPECT_EQ(args_C[1].node, nodeA);
  EXPECT_EQ(args_C[1].port, 3);

  // A should still be a predecessor
  auto const &pred_C = g.pred_of(nodeC);
  EXPECT_EQ(pred_C.size(), 1);
  EXPECT_TRUE(pred_C.count(nodeA));
}

TEST_F(GraphNodeTest, ConsistencyAfterOperations) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");
  auto nodeC = make_node("C");
  auto nodeD = make_node("D");

  g.add(nodeC, {nodeA, nodeB});
  g.add(nodeD, nodeC);

  // Remove C and check consistency
  g.rm(nodeC);

  // Verify no dangling references
  for (auto const &[node, preds] : g.get_pred()) {
    for (auto const &pred : preds) {
      // Each predecessor should have this node in its successor list
      auto const &succs = g.succ_of(pred);
      EXPECT_TRUE(succs.count(node)) << "Dangling predecessor reference found";
    }
  }

  for (auto const &[node, succs] : g.get_succ()) {
    for (auto const &succ : succs) {
      // Each successor should have this node in its predecessor list
      auto const &preds = g.pred_of(succ);
      EXPECT_TRUE(preds.count(node)) << "Dangling successor reference found";
    }
  }

  for (auto const &[node, args] : g.get_args()) {
    for (auto const &arg : args) {
      // Each argument node should be in the predecessor set
      auto const &preds = g.pred_of(node);
      EXPECT_TRUE(preds.count(arg.node)) << "Argument node not in predecessor set";
    }
  }
}

TEST_F(GraphNodeTest, ArgumentOrderPreservation) {
  std::vector<std::shared_ptr<dummy_node>> nodes;
  for (char c : {'Z', 'Y', 'X', 'W', 'V'}) {
    nodes.push_back(make_node(std::string(1, c)));
  }

  auto target = make_node("target");
  g.add(target, nodes);

  auto const &args = g.args_of(target);
  EXPECT_EQ(args.size(), 5);

  for (size_t i = 0; i < nodes.size(); ++i) {
    EXPECT_EQ(args[i].node, nodes[i]) << "Argument order not preserved at index " << i;
    EXPECT_EQ(args[i].port, 0);
  }
}

TEST_F(GraphNodeTest, CyclicGraphDetection) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");
  auto nodeC = make_node("C");

  // Create a cycle: A -> B -> C -> A
  g.add(nodeB, nodeA);
  g.add(nodeC, nodeB);
  g.add(nodeA, nodeC); // This creates a cycle

  EXPECT_EQ(g.size(), 3);

  // Each node should have exactly one predecessor and one successor
  for (const auto &node : {nodeA, nodeB, nodeC}) {
    EXPECT_EQ(g.pred_of(node).size(), 1);
    EXPECT_EQ(g.succ_of(node).size(), 1);
  }

  // Verify the cycle
  EXPECT_TRUE(g.pred_of(nodeA).count(nodeC));
  EXPECT_TRUE(g.pred_of(nodeB).count(nodeA));
  EXPECT_TRUE(g.pred_of(nodeC).count(nodeB));

  EXPECT_TRUE(g.succ_of(nodeA).count(nodeB));
  EXPECT_TRUE(g.succ_of(nodeB).count(nodeC));
  EXPECT_TRUE(g.succ_of(nodeC).count(nodeA));
}

TEST_F(GraphNodeTest, TemplateBasedInPlaceConstruction) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");

  // Test template-based construction using the data_type
  auto nodeC = g.add<dummy_node>({nodeA, nodeB}, "C", 42);

  EXPECT_EQ(g.size(), 3);
  verify_node(nodeC, "C", 42);

  auto const &pred_C = g.pred_of(nodeC);
  EXPECT_EQ(pred_C.size(), 2);
  EXPECT_TRUE(pred_C.count(nodeA));
  EXPECT_TRUE(pred_C.count(nodeB));
}

TEST_F(GraphNodeTest, LargeGraph) {
  // Create a larger graph to test performance and correctness
  const size_t N = 100;
  std::vector<std::shared_ptr<dummy_node>> nodes;

  // Create nodes
  for (size_t i = 0; i < N; ++i) {
    nodes.push_back(make_node(std::to_string(i), static_cast<int>(i)));
  }

  // Create chain: 0 -> 1 -> 2 -> ... -> N-1
  g.add(nodes[0]);
  for (size_t i = 1; i < N; ++i) {
    g.add(nodes[i], nodes[i - 1]);
  }

  EXPECT_EQ(g.size(), N);

  // Check chain structure
  EXPECT_TRUE(g.is_root(nodes[0]));
  EXPECT_TRUE(g.is_leaf(nodes[N - 1]));

  for (size_t i = 1; i < N - 1; ++i) {
    EXPECT_FALSE(g.is_root(nodes[i]));
    EXPECT_FALSE(g.is_leaf(nodes[i]));
  }

  // Check that each node has exactly one predecessor (except root)
  for (size_t i = 1; i < N; ++i) {
    auto const &preds = g.pred_of(nodes[i]);
    EXPECT_EQ(preds.size(), 1);
    EXPECT_TRUE(preds.count(nodes[i - 1]));
  }
}

// Additional comprehensive edge case tests
TEST_F(GraphNodeTest, EmptyPredecessorLists) {
  auto nodeA = make_node("A");

  // Test with empty vector
  std::vector<std::shared_ptr<dummy_node>> empty_preds;
  g.add(nodeA, empty_preds);

  EXPECT_EQ(g.size(), 1);
  EXPECT_TRUE(g.is_root(nodeA));
  EXPECT_TRUE(g.is_leaf(nodeA));
}

TEST_F(GraphNodeTest, EmptyEdgeList) {
  auto nodeA = make_node("A");

  // Test with empty edge vector
  std::vector<graph_node<dummy_node>::edge_type> empty_edges;
  g.add(nodeA, empty_edges);

  EXPECT_EQ(g.size(), 1);
  EXPECT_TRUE(g.is_root(nodeA));
  EXPECT_TRUE(g.is_leaf(nodeA));
}

TEST_F(GraphNodeTest, ZeroPortEdges) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");

  g.add(nodeB, nodeA | 0);

  auto const &args = g.args_of(nodeB);
  EXPECT_EQ(args.size(), 1);
  EXPECT_EQ(args[0].port, 0);
}

TEST_F(GraphNodeTest, AddNodeTwiceWithDifferentPredecessors) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");
  auto nodeC = make_node("C");

  // Add nodeC first with nodeA as predecessor
  g.add(nodeC, nodeA);
  EXPECT_EQ(g.pred_of(nodeC).size(), 1);
  EXPECT_TRUE(g.pred_of(nodeC).count(nodeA));

  // Add nodeC again with nodeB as predecessor (should add to existing predecessors)
  g.add(nodeC, nodeB);
  EXPECT_EQ(g.pred_of(nodeC).size(), 2);
  EXPECT_TRUE(g.pred_of(nodeC).count(nodeA));
  EXPECT_TRUE(g.pred_of(nodeC).count(nodeB));
}

TEST_F(GraphNodeTest, ComplexPortMapping) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");
  auto nodeC = make_node("C");

  // Create complex port mapping
  g.add(nodeC, {nodeA | 100, nodeB | 200, nodeA | 150, nodeB | 250});

  auto const &args = g.args_of(nodeC);
  EXPECT_EQ(args.size(), 4);
  EXPECT_EQ(args[0].node, nodeA);
  EXPECT_EQ(args[0].port, 100);
  EXPECT_EQ(args[1].node, nodeB);
  EXPECT_EQ(args[1].port, 200);
  EXPECT_EQ(args[2].node, nodeA);
  EXPECT_EQ(args[2].port, 150);
  EXPECT_EQ(args[3].node, nodeB);
  EXPECT_EQ(args[3].port, 250);

  // Should have only 2 unique predecessors
  auto const &preds = g.pred_of(nodeC);
  EXPECT_EQ(preds.size(), 2);
  EXPECT_TRUE(preds.count(nodeA));
  EXPECT_TRUE(preds.count(nodeB));
}

TEST_F(GraphNodeTest, ReplaceEdgeComplexCase) {
  auto nodeA = make_node("A");
  auto nodeB = make_node("B");
  auto nodeC = make_node("C");
  auto nodeX = make_node("X");

  // Setup: C depends on A through multiple ports and B once
  g.add(nodeC, {nodeA | 1, nodeA | 2, nodeB | 3, nodeA | 4});

  // Replace one of the A edges
  g.replace(nodeC, nodeA | 2, nodeX | 5);

  auto const &args = g.args_of(nodeC);
  EXPECT_EQ(args.size(), 4);
  EXPECT_EQ(args[0].node, nodeA);
  EXPECT_EQ(args[0].port, 1);
  EXPECT_EQ(args[1].node, nodeX); // Replaced
  EXPECT_EQ(args[1].port, 5);
  EXPECT_EQ(args[2].node, nodeB);
  EXPECT_EQ(args[2].port, 3);
  EXPECT_EQ(args[3].node, nodeA);
  EXPECT_EQ(args[3].port, 4);

  // Predecessors should include A, B, and X
  auto const &preds = g.pred_of(nodeC);
  EXPECT_EQ(preds.size(), 3);
  EXPECT_TRUE(preds.count(nodeA));
  EXPECT_TRUE(preds.count(nodeB));
  EXPECT_TRUE(preds.count(nodeX));
}

TEST_F(GraphNodeTest, NodeIdentityConsistency) {
  auto nodeA = make_node("A", 42);
  auto nodeB = make_node("B", 99);

  g.add(nodeA);
  g.add(nodeB, nodeA);

  // Verify node identity is preserved
  EXPECT_EQ(nodeA->name, "A");
  EXPECT_EQ(nodeA->value, 42);
  EXPECT_EQ(nodeB->name, "B");
  EXPECT_EQ(nodeB->value, 99);

  // Verify the same shared_ptr is used
  auto const &preds = g.pred_of(nodeB);
  auto found_pred = *preds.begin();
  EXPECT_EQ(found_pred.get(), nodeA.get());
  EXPECT_EQ(found_pred->name, "A");
  EXPECT_EQ(found_pred->value, 42);
}

TEST_F(GraphNodeTest, MultipleDisconnectedComponents) {
  // Create two separate DAGs
  auto nodeA1 = make_node("A1");
  auto nodeB1 = make_node("B1");
  auto nodeC1 = make_node("C1");

  auto nodeA2 = make_node("A2");
  auto nodeB2 = make_node("B2");
  auto nodeC2 = make_node("C2");

  // First component: A1 -> B1 -> C1
  g.add(nodeB1, nodeA1);
  g.add(nodeC1, nodeB1);

  // Second component: A2 -> B2 -> C2
  g.add(nodeB2, nodeA2);
  g.add(nodeC2, nodeB2);

  EXPECT_EQ(g.size(), 6);

  // Check roots and leaves
  auto roots = g.get_roots();
  auto leaves = g.get_leaves();

  EXPECT_EQ(roots.size(), 2);
  EXPECT_TRUE(std::find(roots.begin(), roots.end(), nodeA1) != roots.end());
  EXPECT_TRUE(std::find(roots.begin(), roots.end(), nodeA2) != roots.end());

  EXPECT_EQ(leaves.size(), 2);
  EXPECT_TRUE(std::find(leaves.begin(), leaves.end(), nodeC1) != leaves.end());
  EXPECT_TRUE(std::find(leaves.begin(), leaves.end(), nodeC2) != leaves.end());

  // Verify no cross-connections
  EXPECT_FALSE(g.pred_of(nodeB1).count(nodeA2));
  EXPECT_FALSE(g.pred_of(nodeB2).count(nodeA1));
}

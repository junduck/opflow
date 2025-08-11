#include <gtest/gtest.h>

#include "opflow/graph.hpp"

using namespace opflow;
using namespace opflow::literals; // for _p literal and operator|

class GraphTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Reset graph for each test
    g.clear();
  }

  graph<std::string> g;
};

// Basic functionality tests
TEST_F(GraphTest, BasicConstruction) {
  EXPECT_TRUE(g.empty());
  EXPECT_EQ(g.size(), 0);
}

TEST_F(GraphTest, AddSingleVertex) {
  g.add("A");

  EXPECT_FALSE(g.empty());
  EXPECT_EQ(g.size(), 1);
  EXPECT_TRUE(g.contains("A"));
  EXPECT_TRUE(g.is_root("A"));
  EXPECT_TRUE(g.is_leaf("A"));
}

TEST_F(GraphTest, AddVertexWithSinglePredecessor) {
  g.add("B", "A");

  EXPECT_EQ(g.size(), 2);
  EXPECT_TRUE(g.contains("A"));
  EXPECT_TRUE(g.contains("B"));

  // Check adjacency
  auto const &pred_B = g.pred_of("B");
  EXPECT_EQ(pred_B.size(), 1);
  EXPECT_TRUE(pred_B.count("A"));

  auto const &succ_A = g.succ_of("A");
  EXPECT_EQ(succ_A.size(), 1);
  EXPECT_TRUE(succ_A.count("B"));

  // Check arguments
  auto const &args_B = g.args_of("B");
  EXPECT_EQ(args_B.size(), 1);
  EXPECT_EQ(args_B[0].node, "A");
  EXPECT_EQ(args_B[0].port, 0);

  // Check root/leaf status
  EXPECT_TRUE(g.is_root("A"));
  EXPECT_FALSE(g.is_root("B"));
  EXPECT_FALSE(g.is_leaf("A"));
  EXPECT_TRUE(g.is_leaf("B"));
}

TEST_F(GraphTest, AddVertexWithMultiplePredecessors) {
  g.add("C", {"A", "B"});

  EXPECT_EQ(g.size(), 3);

  auto const &pred_C = g.pred_of("C");
  EXPECT_EQ(pred_C.size(), 2);
  EXPECT_TRUE(pred_C.count("A"));
  EXPECT_TRUE(pred_C.count("B"));

  auto const &args_C = g.args_of("C");
  EXPECT_EQ(args_C.size(), 2);
  // Arguments should preserve order
  EXPECT_EQ(args_C[0].node, "A");
  EXPECT_EQ(args_C[0].port, 0);
  EXPECT_EQ(args_C[1].node, "B");
  EXPECT_EQ(args_C[1].port, 0);
}

TEST_F(GraphTest, AddVertexWithPortSpecification) {
  g.add("C", {"A" | 0_p, "B" | 1_p});

  auto const &args_C = g.args_of("C");
  EXPECT_EQ(args_C.size(), 2);
  EXPECT_EQ(args_C[0].node, "A");
  EXPECT_EQ(args_C[0].port, 0);
  EXPECT_EQ(args_C[1].node, "B");
  EXPECT_EQ(args_C[1].port, 1);
}

TEST_F(GraphTest, AddVertexWithMakeArg) {
  g.add("C", {make_node_arg("A", 2), make_node_arg("B", 3_p)});

  auto const &args_C = g.args_of("C");
  EXPECT_EQ(args_C.size(), 2);
  EXPECT_EQ(args_C[0].node, "A");
  EXPECT_EQ(args_C[0].port, 2);
  EXPECT_EQ(args_C[1].node, "B");
  EXPECT_EQ(args_C[1].port, 3);
}

// Edge case tests
TEST_F(GraphTest, DuplicateNodeAddition) {
  g.add("A");
  g.add("A", std::vector<std::string>{"B"}); // Add A again with different predecessors

  EXPECT_EQ(g.size(), 2); // Should have A and B

  // A should now have B as predecessor
  auto const &pred_A = g.pred_of("A");
  EXPECT_EQ(pred_A.size(), 1);
  EXPECT_TRUE(pred_A.count("B"));
}

TEST_F(GraphTest, SelfLoops) {
  g.add("A", "A"); // Self-loop

  EXPECT_EQ(g.size(), 1);

  auto const &pred_A = g.pred_of("A");
  EXPECT_EQ(pred_A.size(), 1);
  EXPECT_TRUE(pred_A.count("A"));

  auto const &succ_A = g.succ_of("A");
  EXPECT_EQ(succ_A.size(), 1);
  EXPECT_TRUE(succ_A.count("A"));

  EXPECT_FALSE(g.is_root("A")); // Has predecessor (itself)
  EXPECT_FALSE(g.is_leaf("A")); // Has successor (itself)
}

TEST_F(GraphTest, DuplicateEdges) {
  // Add multiple edges from B to A with different ports
  g.add("B", {"A" | 0_p, "A" | 1_p, "A" | 0_p});

  auto const &pred_B = g.pred_of("B");
  EXPECT_EQ(pred_B.size(), 1); // Only one unique predecessor
  EXPECT_TRUE(pred_B.count("A"));

  auto const &args_B = g.args_of("B");
  EXPECT_EQ(args_B.size(), 3); // But three arguments (duplicates allowed)
  EXPECT_EQ(args_B[0].node, "A");
  EXPECT_EQ(args_B[0].port, 0);
  EXPECT_EQ(args_B[1].node, "A");
  EXPECT_EQ(args_B[1].port, 1);
  EXPECT_EQ(args_B[2].node, "A");
  EXPECT_EQ(args_B[2].port, 0);
}

// Removal tests
TEST_F(GraphTest, RemoveVertex) {
  g.add("C", {"A", "B"});
  g.add("D", "C");

  EXPECT_EQ(g.size(), 4);

  g.rm("C");

  EXPECT_EQ(g.size(), 3);
  EXPECT_FALSE(g.contains("C"));
  EXPECT_TRUE(g.contains("A"));
  EXPECT_TRUE(g.contains("B"));
  EXPECT_TRUE(g.contains("D"));

  // D should have no predecessors now
  auto const &pred_D = g.pred_of("D");
  EXPECT_TRUE(pred_D.empty());

  // A and B should have no successors
  auto const &succ_A = g.succ_of("A");
  auto const &succ_B = g.succ_of("B");
  EXPECT_TRUE(succ_A.empty());
  EXPECT_TRUE(succ_B.empty());
}

TEST_F(GraphTest, RemoveNonExistentVertex) {
  g.add("A");
  g.rm("B"); // Remove non-existent vertex

  EXPECT_EQ(g.size(), 1);
  EXPECT_TRUE(g.contains("A"));
}

// Edge removal tests
TEST_F(GraphTest, RemoveEdge) {
  g.add("B", {"A" | 0_p, "A" | 1_p});

  g.rm("B", "A" | 1_p);

  auto const &args_B = g.args_of("B");
  EXPECT_EQ(args_B.size(), 1);
  EXPECT_EQ(args_B[0].node, "A");
  EXPECT_EQ(args_B[0].port, 0);

  // A should still be a predecessor since there's still A:0
  auto const &pred_B = g.pred_of("B");
  EXPECT_EQ(pred_B.size(), 1);
  EXPECT_TRUE(pred_B.count("A"));
}

TEST_F(GraphTest, RemoveAllEdgesToSamePredecessor) {
  g.add("B", {"A" | 0_p, "A" | 1_p});

  // Remove all edges to A
  g.rm("B", "A" | 0_p);
  g.rm("B", "A" | 1_p);

  auto const &args_B = g.args_of("B");
  EXPECT_TRUE(args_B.empty());

  // A should no longer be a predecessor
  auto const &pred_B = g.pred_of("B");
  EXPECT_TRUE(pred_B.empty());

  // B should no longer be a successor of A
  auto const &succ_A = g.succ_of("A");
  EXPECT_TRUE(succ_A.empty());
}

TEST_F(GraphTest, RemoveEdgeFromNonExistentNode) {
  g.add("A");
  g.rm("B", "A"); // Remove edge from non-existent node

  EXPECT_EQ(g.size(), 1); // Should not affect the graph
}

TEST_F(GraphTest, RemoveNonExistentEdge) {
  g.add("B", "A");
  g.rm("B", "C"); // Remove non-existent edge

  auto const &pred_B = g.pred_of("B");
  EXPECT_EQ(pred_B.size(), 1);
  EXPECT_TRUE(pred_B.count("A"));
}

// Replace vertex tests
TEST_F(GraphTest, ReplaceVertex) {
  g.add("C", {"A", "B"});
  g.add("D", "C");
  g.add("E", "C");

  g.replace("C", "X");

  EXPECT_FALSE(g.contains("C"));
  EXPECT_TRUE(g.contains("X"));

  // X should have A and B as predecessors
  auto const &pred_X = g.pred_of("X");
  EXPECT_EQ(pred_X.size(), 2);
  EXPECT_TRUE(pred_X.count("A"));
  EXPECT_TRUE(pred_X.count("B"));

  // D and E should have X as predecessor
  auto const &pred_D = g.pred_of("D");
  auto const &pred_E = g.pred_of("E");
  EXPECT_EQ(pred_D.size(), 1);
  EXPECT_EQ(pred_E.size(), 1);
  EXPECT_TRUE(pred_D.count("X"));
  EXPECT_TRUE(pred_E.count("X"));

  // Arguments should be updated
  auto const &args_D = g.args_of("D");
  auto const &args_E = g.args_of("E");
  EXPECT_EQ(args_D[0].node, "X");
  EXPECT_EQ(args_E[0].node, "X");
}

TEST_F(GraphTest, ReplaceNonExistentVertex) {
  g.add("A");
  g.replace("B", "C"); // Replace non-existent vertex

  EXPECT_EQ(g.size(), 1);
  EXPECT_TRUE(g.contains("A"));
  EXPECT_FALSE(g.contains("B"));
  EXPECT_FALSE(g.contains("C"));
}

TEST_F(GraphTest, ReplaceWithExistingVertex) {
  g.add("A");
  g.add("B");
  g.replace("A", "B"); // Replace with existing vertex

  EXPECT_EQ(g.size(), 2);
  EXPECT_TRUE(g.contains("A"));
  EXPECT_TRUE(g.contains("B"));
}

TEST_F(GraphTest, ReplaceVertexWithItself) {
  g.add("A");
  g.replace("A", "A"); // Replace with itself

  EXPECT_EQ(g.size(), 1);
  EXPECT_TRUE(g.contains("A"));
}

// Replace edge tests
TEST_F(GraphTest, ReplaceEdge) {
  g.add("C", {"A" | 0_p, "B" | 1_p});

  g.replace("C", "A" | 0_p, "X" | 2_p);

  auto const &args_C = g.args_of("C");
  EXPECT_EQ(args_C.size(), 2);
  EXPECT_EQ(args_C[0].node, "X");
  EXPECT_EQ(args_C[0].port, 2);
  EXPECT_EQ(args_C[1].node, "B");
  EXPECT_EQ(args_C[1].port, 1);

  // Check adjacency updates
  auto const &pred_C = g.pred_of("C");
  EXPECT_EQ(pred_C.size(), 2);
  EXPECT_TRUE(pred_C.count("X"));
  EXPECT_TRUE(pred_C.count("B"));
  EXPECT_FALSE(pred_C.count("A"));
}

TEST_F(GraphTest, ReplaceEdgeWithItself) {
  g.add("B", "A" | 0_p);

  g.replace("B", "A" | 0_p, "A" | 0_p); // Replace with itself

  auto const &args_B = g.args_of("B");
  EXPECT_EQ(args_B.size(), 1);
  EXPECT_EQ(args_B[0].node, "A");
  EXPECT_EQ(args_B[0].port, 0);
}

TEST_F(GraphTest, ReplaceNonExistentEdge) {
  g.add("B", "A");
  g.replace("B", "C" | 0_p, "D" | 1_p); // Replace non-existent edge

  auto const &pred_B = g.pred_of("B");
  EXPECT_EQ(pred_B.size(), 1);
  EXPECT_TRUE(pred_B.count("A"));
}

TEST_F(GraphTest, ReplaceDuplicateEdges) {
  g.add("C", {"A" | 0_p, "A" | 0_p, "B" | 1_p});

  g.replace("C", "A" | 0_p, "X" | 2_p);

  auto const &args_C = g.args_of("C");
  EXPECT_EQ(args_C.size(), 3);
  // Both A:0 edges should be replaced
  EXPECT_EQ(args_C[0].node, "X");
  EXPECT_EQ(args_C[0].port, 2);
  EXPECT_EQ(args_C[1].node, "X");
  EXPECT_EQ(args_C[1].port, 2);
  EXPECT_EQ(args_C[2].node, "B");
  EXPECT_EQ(args_C[2].port, 1);
}

// Graph merging tests
TEST_F(GraphTest, MergeDisjointGraphs) {
  graph<std::string> g2;

  g.add("A");
  g.add("B", "A");

  g2.add("C");
  g2.add("D", "C");

  g.merge(g2);

  EXPECT_EQ(g.size(), 4);
  EXPECT_TRUE(g.contains("A"));
  EXPECT_TRUE(g.contains("B"));
  EXPECT_TRUE(g.contains("C"));
  EXPECT_TRUE(g.contains("D"));

  // Check that relationships are preserved
  auto const &pred_B = g.pred_of("B");
  auto const &pred_D = g.pred_of("D");
  EXPECT_TRUE(pred_B.count("A"));
  EXPECT_TRUE(pred_D.count("C"));
}

TEST_F(GraphTest, MergeOverlappingGraphs) {
  graph<std::string> g2;

  g.add("A");
  g.add("B", "A");

  g2.add("B", "C"); // B already exists but with different predecessor

  g.merge(g2);

  EXPECT_EQ(g.size(), 3);

  // B should keep its original predecessor A (lhs takes precedence)
  auto const &pred_B = g.pred_of("B");
  EXPECT_EQ(pred_B.size(), 1);
  EXPECT_TRUE(pred_B.count("A"));
  EXPECT_FALSE(pred_B.count("C"));

  // But C should be added as a new node
  EXPECT_TRUE(g.contains("C"));
}

TEST_F(GraphTest, MergeOperatorPlus) {
  graph<std::string> g2;

  g.add("A");
  g2.add("B");

  auto g3 = g + g2;

  EXPECT_EQ(g3.size(), 2);
  EXPECT_TRUE(g3.contains("A"));
  EXPECT_TRUE(g3.contains("B"));

  // Original graphs should be unchanged
  EXPECT_EQ(g.size(), 1);
  EXPECT_EQ(g2.size(), 1);
}

TEST_F(GraphTest, MergeOperatorPlusEqual) {
  graph<std::string> g2;

  g.add("A");
  g2.add("B");

  g += g2;

  EXPECT_EQ(g.size(), 2);
  EXPECT_TRUE(g.contains("A"));
  EXPECT_TRUE(g.contains("B"));
}

// Complex graph tests
TEST_F(GraphTest, ComplexDAG) {
  g.add("A");
  g.add("B");
  g.add("C", {"A", "B"});
  g.add("D", "A");
  g.add("E", {"C", "D"});

  // Check roots and leaves
  auto roots = g.get_roots();
  auto leaves = g.get_leaves();

  EXPECT_EQ(roots.size(), 2);
  EXPECT_TRUE(std::find(roots.begin(), roots.end(), "A") != roots.end());
  EXPECT_TRUE(std::find(roots.begin(), roots.end(), "B") != roots.end());

  EXPECT_EQ(leaves.size(), 1);
  EXPECT_TRUE(std::find(leaves.begin(), leaves.end(), "E") != leaves.end());
}

TEST_F(GraphTest, ClearGraph) {
  g.add("A");
  g.add("B", "A");

  EXPECT_FALSE(g.empty());

  g.clear();

  EXPECT_TRUE(g.empty());
  EXPECT_EQ(g.size(), 0);
  EXPECT_FALSE(g.contains("A"));
  EXPECT_FALSE(g.contains("B"));
}

// Character literal tests
TEST_F(GraphTest, CharArrayLiterals) {
  g.add("node1", {"node2", "node3"});

  EXPECT_EQ(g.size(), 3);
  EXPECT_TRUE(g.contains("node1"));
  EXPECT_TRUE(g.contains("node2"));
  EXPECT_TRUE(g.contains("node3"));
}

TEST_F(GraphTest, CharArrayLiteralsWithPorts) {
  g.add("node1", {"node2" | 1_p, "node3" | 2_p});

  auto const &args = g.args_of("node1");
  EXPECT_EQ(args.size(), 2);
  EXPECT_EQ(args[0].node, "node2");
  EXPECT_EQ(args[0].port, 1);
  EXPECT_EQ(args[1].node, "node3");
  EXPECT_EQ(args[1].port, 2);
}

// Potential bug detection tests
TEST_F(GraphTest, ConsistencyAfterOperations) {
  g.add("C", {"A", "B"});
  g.add("D", "C");

  // Remove C and check consistency
  g.rm("C");

  // Verify no dangling references
  for (auto const &[node, preds] : g.get_pred()) {
    for (auto const &pred : preds) {
      EXPECT_TRUE(g.contains(pred)) << "Dangling predecessor reference";

      auto const &succ = g.succ_of(pred);
      EXPECT_TRUE(succ.count(node)) << "Inconsistent successor mapping";
    }
  }

  for (auto const &[node, succs] : g.get_succ()) {
    for (auto const &succ : succs) {
      EXPECT_TRUE(g.contains(succ)) << "Dangling successor reference";

      auto const &pred = g.pred_of(succ);
      EXPECT_TRUE(pred.count(node)) << "Inconsistent predecessor mapping";
    }
  }

  for (auto const &[node, args] : g.get_args()) {
    for (auto const &arg : args) {
      EXPECT_TRUE(g.contains(arg.node)) << "Dangling argument reference";
    }
  }
}

TEST_F(GraphTest, ArgumentOrderPreservation) {
  std::vector<std::string> nodes = {"Z", "Y", "X", "W", "V"};
  g.add("target", nodes);

  auto const &args = g.args_of("target");
  EXPECT_EQ(args.size(), 5);

  for (size_t i = 0; i < nodes.size(); ++i) {
    EXPECT_EQ(args[i].node, nodes[i]) << "Argument order not preserved at index " << i;
  }
}

TEST_F(GraphTest, PortConsistency) {
  g.add("B", {"A" | 5_p});

  auto const &args = g.args_of("B");
  EXPECT_EQ(args.size(), 1);
  EXPECT_EQ(args[0].port, 5);

  // Remove and re-add with different port
  g.rm("B", "A" | 5_p);
  g.add("B", {"A" | 3_p});

  auto const &new_args = g.args_of("B");
  EXPECT_EQ(new_args.size(), 1);
  EXPECT_EQ(new_args[0].port, 3);
}

// Additional edge case tests to thoroughly test the implementation
TEST_F(GraphTest, EmptyStringNodes) {
  g.add("", "A");
  g.add("B", "");

  EXPECT_EQ(g.size(), 3);
  EXPECT_TRUE(g.contains(""));
  EXPECT_TRUE(g.contains("A"));
  EXPECT_TRUE(g.contains("B"));

  auto const &pred_empty = g.pred_of("");
  auto const &pred_B = g.pred_of("B");
  EXPECT_TRUE(pred_empty.count("A"));
  EXPECT_TRUE(pred_B.count(""));
}

TEST_F(GraphTest, LongPortNumbers) {
  g.add("B", "A" | 4294967295_p); // Max uint32_t

  auto const &args = g.args_of("B");
  EXPECT_EQ(args.size(), 1);
  EXPECT_EQ(args[0].port, 4294967295u);
}

TEST_F(GraphTest, ManyDuplicateEdgesWithDifferentPorts) {
  // Add many edges from B to A with sequential ports
  std::vector<graph<std::string>::node_arg_type> edges;
  for (uint32_t i = 0; i < 100; ++i) {
    edges.emplace_back("A", i);
  }
  g.add("B", edges);

  auto const &pred_B = g.pred_of("B");
  auto const &args_B = g.args_of("B");

  EXPECT_EQ(pred_B.size(), 1); // Only one unique predecessor
  EXPECT_TRUE(pred_B.count("A"));
  EXPECT_EQ(args_B.size(), 100); // But 100 arguments

  // Check that ports are in order
  for (uint32_t i = 0; i < 100; ++i) {
    EXPECT_EQ(args_B[i].node, "A");
    EXPECT_EQ(args_B[i].port, i);
  }
}

TEST_F(GraphTest, RemoveSpecificDuplicateEdge) {
  g.add("C", {"A" | 1_p, "A" | 2_p, "A" | 1_p, "A" | 3_p});

  // Remove all A:1 edges (should remove 2 edges)
  g.rm("C", "A" | 1_p);

  auto const &args_C = g.args_of("C");
  EXPECT_EQ(args_C.size(), 2); // Should have A:2 and A:3 left
  EXPECT_EQ(args_C[0].node, "A");
  EXPECT_EQ(args_C[0].port, 2);
  EXPECT_EQ(args_C[1].node, "A");
  EXPECT_EQ(args_C[1].port, 3);

  // A should still be a predecessor
  auto const &pred_C = g.pred_of("C");
  EXPECT_EQ(pred_C.size(), 1);
  EXPECT_TRUE(pred_C.count("A"));
}

TEST_F(GraphTest, LargeGraph) {
  // Create a larger graph to test performance and correctness
  const int N = 100;

  // Create chain: 0 -> 1 -> 2 -> ... -> N-1
  for (int i = 1; i < N; ++i) {
    g.add(std::to_string(i), std::to_string(i - 1));
  }

  EXPECT_EQ(g.size(), N);

  // Check chain structure
  EXPECT_TRUE(g.is_root("0"));
  EXPECT_TRUE(g.is_leaf(std::to_string(N - 1)));

  for (int i = 1; i < N - 1; ++i) {
    EXPECT_FALSE(g.is_root(std::to_string(i)));
    EXPECT_FALSE(g.is_leaf(std::to_string(i)));
  }

  // Check that each node has exactly one predecessor (except root)
  for (int i = 1; i < N; ++i) {
    auto const &pred = g.pred_of(std::to_string(i));
    EXPECT_EQ(pred.size(), 1);
    EXPECT_TRUE(pred.count(std::to_string(i - 1)));
  }
}

TEST_F(GraphTest, GraphIntegrityAfterManyOperations) {
  // Perform many operations and check graph integrity
  g.add("A");
  g.add("B", "A");
  g.add("C", {"A", "B"});
  g.add("D", "C");

  // Replace some vertices
  g.replace("B", "X");
  g.replace("A", "Y");

  // Remove some edges
  g.rm("C", "X");

  // Add more vertices
  g.add("E", {"C", "D"});

  // Check final structure
  EXPECT_EQ(g.size(), 5); // Y, X, C, D, E
  EXPECT_TRUE(g.contains("Y"));
  EXPECT_TRUE(g.contains("X"));
  EXPECT_TRUE(g.contains("C"));
  EXPECT_TRUE(g.contains("D"));
  EXPECT_TRUE(g.contains("E"));
  EXPECT_FALSE(g.contains("A"));
  EXPECT_FALSE(g.contains("B"));

  // Check that C only has Y as predecessor (X was removed)
  auto const &pred_C = g.pred_of("C");
  EXPECT_EQ(pred_C.size(), 1);
  EXPECT_TRUE(pred_C.count("Y"));
}

TEST_F(GraphTest, StringLiteralVsStringObjectConsistency) {
  // Test that string literals and string objects behave the same
  std::string node_a = "A";
  std::string node_b = "B";

  g.add("C", "A");              // string literal
  g.add("D", node_a);           // string object
  g.add("E", {"A", "B"});       // string literals in initializer list
  g.add("F", {node_a, node_b}); // string objects in initializer list

  EXPECT_EQ(g.size(), 6);

  // All should have the same predecessor
  auto const &pred_C = g.pred_of("C");
  auto const &pred_D = g.pred_of("D");
  EXPECT_EQ(pred_C, pred_D);

  auto const &pred_E = g.pred_of("E");
  auto const &pred_F = g.pred_of("F");
  EXPECT_EQ(pred_E, pred_F);
}

TEST_F(GraphTest, SpecialCharacterNodes) {
  // Test nodes with special characters
  g.add("node@#$%", "node with spaces");
  g.add("node_with_unicode_🔥", "node@#$%");
  g.add("", "node_with_unicode_🔥"); // empty string node

  EXPECT_EQ(g.size(), 4);
  EXPECT_TRUE(g.contains("node@#$%"));
  EXPECT_TRUE(g.contains("node with spaces"));
  EXPECT_TRUE(g.contains("node_with_unicode_🔥"));
  EXPECT_TRUE(g.contains(""));
}

// Advanced edge case tests
TEST_F(GraphTest, CyclicGraphDetection) {
  // Create a cycle: A -> B -> C -> A
  g.add("B", "A");
  g.add("C", "B");
  g.add("A", "C"); // This creates a cycle

  EXPECT_EQ(g.size(), 3);

  // Each node should have exactly one predecessor and one successor
  for (const auto &node : {"A", "B", "C"}) {
    EXPECT_EQ(g.pred_of(node).size(), 1);
    EXPECT_EQ(g.succ_of(node).size(), 1);
    EXPECT_FALSE(g.is_root(node));
    EXPECT_FALSE(g.is_leaf(node));
  }

  // Verify the cycle
  EXPECT_TRUE(g.pred_of("A").count("C"));
  EXPECT_TRUE(g.pred_of("B").count("A"));
  EXPECT_TRUE(g.pred_of("C").count("B"));

  EXPECT_TRUE(g.succ_of("A").count("B"));
  EXPECT_TRUE(g.succ_of("B").count("C"));
  EXPECT_TRUE(g.succ_of("C").count("A"));
}

TEST_F(GraphTest, SelfLoopWithMultiplePorts) {
  // Node depends on itself through multiple ports
  g.add("A", {"A" | 0_p, "A" | 1_p, "A" | 2_p});

  auto const &pred_A = g.pred_of("A");
  auto const &args_A = g.args_of("A");

  EXPECT_EQ(pred_A.size(), 1);
  EXPECT_TRUE(pred_A.count("A"));
  EXPECT_EQ(args_A.size(), 3);

  // Check all three self-references with different ports
  EXPECT_EQ(args_A[0].node, "A");
  EXPECT_EQ(args_A[0].port, 0);
  EXPECT_EQ(args_A[1].node, "A");
  EXPECT_EQ(args_A[1].port, 1);
  EXPECT_EQ(args_A[2].node, "A");
  EXPECT_EQ(args_A[2].port, 2);
}

TEST_F(GraphTest, RemoveVertexInCycle) {
  // Create cycle A -> B -> C -> A, then remove B
  g.add("B", "A");
  g.add("C", "B");
  g.add("A", "C");

  g.rm("B");

  EXPECT_EQ(g.size(), 2);
  EXPECT_FALSE(g.contains("B"));

  // A should still depend on C
  auto const &pred_A = g.pred_of("A");
  EXPECT_EQ(pred_A.size(), 1);
  EXPECT_TRUE(pred_A.count("C"));

  // C should still have A as successor (because A depends on C)
  auto const &succ_C = g.succ_of("C");
  EXPECT_EQ(succ_C.size(), 1);
  EXPECT_TRUE(succ_C.count("A"));
  EXPECT_FALSE(g.is_leaf("C"));

  // C should have no predecessors now (B was removed)
  auto const &pred_C = g.pred_of("C");
  EXPECT_TRUE(pred_C.empty());
  EXPECT_TRUE(g.is_root("C"));
}

TEST_F(GraphTest, ReplaceVertexInCycle) {
  // Create cycle A -> B -> C -> A, then replace B with X
  g.add("B", "A");
  g.add("C", "B");
  g.add("A", "C");

  g.replace("B", "X");

  EXPECT_EQ(g.size(), 3);
  EXPECT_FALSE(g.contains("B"));
  EXPECT_TRUE(g.contains("X"));

  // Check that the cycle is preserved: A -> X -> C -> A
  EXPECT_TRUE(g.pred_of("X").count("A"));
  EXPECT_TRUE(g.pred_of("C").count("X"));
  EXPECT_TRUE(g.pred_of("A").count("C"));

  EXPECT_TRUE(g.succ_of("A").count("X"));
  EXPECT_TRUE(g.succ_of("X").count("C"));
  EXPECT_TRUE(g.succ_of("C").count("A"));
}

TEST_F(GraphTest, MultipleDisconnectedComponents) {
  // Create two disconnected components
  // Component 1: A -> B -> C
  g.add("B", "A");
  g.add("C", "B");

  // Component 2: X -> Y -> Z
  g.add("Y", "X");
  g.add("Z", "Y");

  EXPECT_EQ(g.size(), 6);

  // Check roots and leaves
  auto roots = g.get_roots();
  auto leaves = g.get_leaves();

  EXPECT_EQ(roots.size(), 2);
  EXPECT_TRUE(std::find(roots.begin(), roots.end(), "A") != roots.end());
  EXPECT_TRUE(std::find(roots.begin(), roots.end(), "X") != roots.end());

  EXPECT_EQ(leaves.size(), 2);
  EXPECT_TRUE(std::find(leaves.begin(), leaves.end(), "C") != leaves.end());
  EXPECT_TRUE(std::find(leaves.begin(), leaves.end(), "Z") != leaves.end());

  // Components should be completely disconnected
  EXPECT_TRUE(g.succ_of("C").empty());
  EXPECT_TRUE(g.succ_of("Z").empty());
  EXPECT_TRUE(g.pred_of("A").empty());
  EXPECT_TRUE(g.pred_of("X").empty());
}

TEST_F(GraphTest, ComplexPortConnections) {
  // Create a node that depends on another node through many different ports
  std::vector<graph<std::string>::node_arg_type> edges;
  for (uint32_t i = 0; i < 10; ++i) {
    edges.emplace_back("producer", i);
    edges.emplace_back("producer", i); // Add duplicates
  }
  g.add("consumer", edges);

  auto const &args = g.args_of("consumer");
  EXPECT_EQ(args.size(), 20); // 10 ports * 2 duplicates each

  // All arguments should point to "producer"
  for (const auto &arg : args) {
    EXPECT_EQ(arg.node, "producer");
  }

  // Check that we have the expected port pattern
  for (uint32_t i = 0; i < 10; ++i) {
    EXPECT_EQ(args[i * 2].port, i);
    EXPECT_EQ(args[i * 2 + 1].port, i);
  }
}

TEST_F(GraphTest, EdgeReplacementWithCycles) {
  // Create: A -> B -> C, and replace B->C with B->A (creating a cycle)
  g.add("B", "A");
  g.add("C", "B");

  g.replace("C", make_node_arg("B", 0), make_node_arg("A", 0));

  // Now we should have A -> B and A -> C (no longer B -> C)
  auto const &pred_C = g.pred_of("C");
  EXPECT_EQ(pred_C.size(), 1);
  EXPECT_TRUE(pred_C.count("A"));
  EXPECT_FALSE(pred_C.count("B"));

  auto const &succ_A = g.succ_of("A");
  EXPECT_EQ(succ_A.size(), 2);
  EXPECT_TRUE(succ_A.count("B"));
  EXPECT_TRUE(succ_A.count("C"));

  auto const &succ_B = g.succ_of("B");
  EXPECT_TRUE(succ_B.empty());
}

TEST_F(GraphTest, MergeGraphsWithComplexDependencies) {
  graph<std::string> g2;

  // g: A -> C -> E
  g.add("C", "A");
  g.add("E", "C");

  // g2: B -> C -> D (C exists in both with different predecessors)
  g2.add("C", "B");
  g2.add("D", "C");

  g.merge(g2);

  EXPECT_EQ(g.size(), 5); // A, B, C, D, E

  // C should keep its original predecessor A (lhs precedence)
  auto const &pred_C = g.pred_of("C");
  EXPECT_EQ(pred_C.size(), 1);
  EXPECT_TRUE(pred_C.count("A"));
  EXPECT_FALSE(pred_C.count("B"));

  // But D should be added as a new node depending on C
  auto const &pred_D = g.pred_of("D");
  EXPECT_EQ(pred_D.size(), 1);
  EXPECT_TRUE(pred_D.count("C"));

  // B should exist as an isolated node
  EXPECT_TRUE(g.contains("B"));
  EXPECT_TRUE(g.is_root("B"));
  EXPECT_TRUE(g.is_leaf("B"));
}

TEST_F(GraphTest, StressTestManyEdgeOperations) {
  // Stress test with many edge additions and removals
  const uint32_t N = 50;

  // Add many edges
  std::vector<graph<std::string>::node_arg_type> edges;
  for (uint32_t i = 0; i < N; ++i) {
    edges.emplace_back("source", i);
  }
  g.add("target", edges);

  EXPECT_EQ(g.args_of("target").size(), N);

  // Remove edges in reverse order
  for (uint32_t i = N - 1; i != UINT32_MAX; --i) {
    g.rm("target", make_node_arg("source", i));
    EXPECT_EQ(g.args_of("target").size(), i);
  }

  // target should now have no predecessors
  EXPECT_TRUE(g.pred_of("target").empty());
  EXPECT_TRUE(g.is_root("target"));
}

TEST_F(GraphTest, ReplaceEdgeWithMultipleOccurrences) {
  // Add the same edge multiple times, then replace all occurrences
  g.add("B", {"A" | 1_p, "C" | 2_p, "A" | 1_p, "D" | 3_p, "A" | 1_p});

  auto const &initial_args = g.args_of("B");
  EXPECT_EQ(initial_args.size(), 5);

  // Count occurrences of A:1
  int count = 0;
  for (const auto &arg : initial_args) {
    if (arg.node == "A" && arg.port == 1) {
      count++;
    }
  }
  EXPECT_EQ(count, 3);

  // Replace all A:1 with X:5
  g.replace("B", make_node_arg("A", 1), make_node_arg("X", 5));

  auto const &final_args = g.args_of("B");
  EXPECT_EQ(final_args.size(), 5);

  // Check that all A:1 have been replaced with X:5
  for (const auto &arg : final_args) {
    if (arg.node == "X" && arg.port == 5) {
      count--;
    }
  }
  EXPECT_EQ(count, 0); // All A:1 should have been replaced

  // Verify the final arguments
  EXPECT_EQ(final_args[0].node, "X");
  EXPECT_EQ(final_args[0].port, 5);
  EXPECT_EQ(final_args[1].node, "C");
  EXPECT_EQ(final_args[1].port, 2);
  EXPECT_EQ(final_args[2].node, "X");
  EXPECT_EQ(final_args[2].port, 5);
  EXPECT_EQ(final_args[3].node, "D");
  EXPECT_EQ(final_args[3].port, 3);
  EXPECT_EQ(final_args[4].node, "X");
  EXPECT_EQ(final_args[4].port, 5);
}

TEST_F(GraphTest, GraphCopyAndMove) {
  // Set up initial graph
  g.add("C", {"A", "B"});
  g.add("D", "C");

  // Test copy constructor
  auto g_copy = g;
  EXPECT_EQ(g_copy.size(), g.size());
  EXPECT_TRUE(g_copy.contains("A"));
  EXPECT_TRUE(g_copy.contains("B"));
  EXPECT_TRUE(g_copy.contains("C"));
  EXPECT_TRUE(g_copy.contains("D"));

  // Modify original and ensure copy is unaffected
  g.add("E", "D");
  EXPECT_EQ(g.size(), 5);
  EXPECT_EQ(g_copy.size(), 4);
  EXPECT_FALSE(g_copy.contains("E"));

  // Test copy assignment
  graph<std::string> g_assign;
  g_assign.add("X");
  g_assign = g_copy;
  EXPECT_EQ(g_assign.size(), 4);
  EXPECT_FALSE(g_assign.contains("X"));
  EXPECT_TRUE(g_assign.contains("D"));
}

TEST_F(GraphTest, ArgumentOrderStabilityUnderModification) {
  // Create a node with specific argument order
  std::vector<std::string> original_order = {"Z", "Y", "X", "W", "V", "U"};
  g.add("target", original_order);

  // Add more edges
  g.add("target", {"T", "S"});

  auto args = g.args_of("target");
  EXPECT_EQ(args.size(), 8); // 6 original + 2 new

  // Original order should be preserved for first 6 elements
  for (size_t i = 0; i < original_order.size(); ++i) {
    EXPECT_EQ(args[i].node, original_order[i]);
  }

  // New elements should be appended
  EXPECT_EQ(args[6].node, "T");
  EXPECT_EQ(args[7].node, "S");
}

TEST_F(GraphTest, ErrorConditionsAndBoundaryValues) {
  // Test various edge cases that might cause issues

  // Add vertex with empty predecessor list
  g.add("A", std::vector<std::string>{});
  EXPECT_TRUE(g.is_root("A"));
  EXPECT_TRUE(g.is_leaf("A"));

  // Remove edge that doesn't exist
  g.rm("A", "nonexistent");
  EXPECT_TRUE(g.pred_of("A").empty());

  // Replace edge that doesn't exist
  g.replace("A", make_node_arg("nonexistent", 0), make_node_arg("B", 0));
  EXPECT_TRUE(g.pred_of("A").empty());

  // Replace non-existent node
  g.replace("nonexistent", "B");
  EXPECT_FALSE(g.contains("B"));

  // Operations on non-existent nodes should be safe
  EXPECT_TRUE(g.pred_of("nonexistent").empty());
  EXPECT_TRUE(g.succ_of("nonexistent").empty());
  EXPECT_TRUE(g.args_of("nonexistent").empty());
}

TEST_F(GraphTest, PortRangeAndLimits) {
  // Test with port numbers at the limits of uint32_t
  const uint32_t max_port = std::numeric_limits<uint32_t>::max();
  const uint32_t near_max = max_port - 1;

  g.add("B", {"A" | detail::node_port_t{max_port}});
  g.add("C", {"A" | detail::node_port_t{near_max}});

  auto args_B = g.args_of("B");
  auto args_C = g.args_of("C");

  EXPECT_EQ(args_B[0].port, max_port);
  EXPECT_EQ(args_C[0].port, near_max);

  // Test port 0 explicitly
  g.add("D", {"A" | detail::node_port_t{0}});
  auto args_D = g.args_of("D");
  EXPECT_EQ(args_D[0].port, 0);
}

TEST_F(GraphTest, GraphIntegrityAfterComplexOperations) {
  // Perform a complex sequence of operations and verify graph integrity

  // Build initial graph
  g.add("B", "A");
  g.add("C", {"A", "B"});
  g.add("D", {"B", "C"});
  g.add("E", "D");

  // Perform replacements
  g.replace("B", "B_new");
  g.replace("C", make_node_arg("A", 0), make_node_arg("A_new", 0));

  // Remove and re-add
  g.rm("D");
  g.add("D_new", {"C", "E"});

  // Merge with another graph
  graph<std::string> other;
  other.add("F", "E");
  other.add("G", {"F", "A_new"});
  g.merge(other);

  // Verify final integrity
  EXPECT_GT(g.size(), 0);

  // Check all adjacency relationships are bidirectional
  for (auto const &[node, preds] : g.get_pred()) {
    for (auto const &pred : preds) {
      EXPECT_TRUE(g.contains(pred)) << "Predecessor " << pred << " of " << node << " doesn't exist";
      auto const &pred_succs = g.succ_of(pred);
      EXPECT_TRUE(pred_succs.count(node)) << "Predecessor " << pred << " doesn't list " << node << " as successor";
    }
  }

  for (auto const &[node, succs] : g.get_succ()) {
    for (auto const &succ : succs) {
      EXPECT_TRUE(g.contains(succ)) << "Successor " << succ << " of " << node << " doesn't exist";
      auto const &succ_preds = g.pred_of(succ);
      EXPECT_TRUE(succ_preds.count(node)) << "Successor " << succ << " doesn't list " << node << " as predecessor";
    }
  }

  // Check argument references are valid
  for (auto const &[node, args] : g.get_args()) {
    for (auto const &arg : args) {
      EXPECT_TRUE(g.contains(arg.node)) << "Argument node " << arg.node << " doesn't exist";
    }
  }
}

// Additional corner case tests for maximum coverage
TEST_F(GraphTest, EmptyGraphOperations) {
  // Operations on completely empty graph
  EXPECT_TRUE(g.empty());
  EXPECT_EQ(g.size(), 0);

  // These should not crash and should return empty results
  auto roots = g.get_roots();
  auto leaves = g.get_leaves();

  EXPECT_TRUE(roots.empty());
  EXPECT_TRUE(leaves.empty());

  // Operations on non-existent nodes should be safe
  g.rm("nonexistent");
  g.rm("nonexistent", "also_nonexistent");
  g.replace("nonexistent", "also_nonexistent");
  g.replace("nonexistent", make_node_arg("also", 0), make_node_arg("nonexistent", 1));

  EXPECT_TRUE(g.empty());
}

TEST_F(GraphTest, SingleNodeOperations) {
  g.add("A");

  // Single node should be both root and leaf
  EXPECT_TRUE(g.is_root("A"));
  EXPECT_TRUE(g.is_leaf("A"));

  auto roots = g.get_roots();
  auto leaves = g.get_leaves();

  EXPECT_EQ(roots.size(), 1);
  EXPECT_EQ(leaves.size(), 1);
  EXPECT_EQ(roots[0], "A");
  EXPECT_EQ(leaves[0], "A");

  // Operations on itself
  g.add("A", "A"); // Self dependency
  EXPECT_FALSE(g.is_root("A"));
  EXPECT_FALSE(g.is_leaf("A"));

  g.rm("A", "A"); // Remove self dependency
  EXPECT_TRUE(g.is_root("A"));
  EXPECT_TRUE(g.is_leaf("A"));
}

TEST_F(GraphTest, OperatorOverloadsWithLiterals) {
  // Test all combinations of literal types with operators
  g.add("target", {"str1" | 0_p, std::string("str2") | 1_p});

  // Add char array separately since it needs special handling
  g.add("target", "str3");

  auto const &args = g.args_of("target");
  EXPECT_EQ(args.size(), 3);
  EXPECT_EQ(args[0].node, "str1");
  EXPECT_EQ(args[0].port, 0);
  EXPECT_EQ(args[1].node, "str2");
  EXPECT_EQ(args[1].port, 1);
  EXPECT_EQ(args[2].node, "str3");
  EXPECT_EQ(args[2].port, 0); // Default port
}

TEST_F(GraphTest, InitializerListEdgeCases) {
  // Empty initializer lists
  g.add("A", std::initializer_list<std::string>{});
  g.add("B", std::initializer_list<graph<std::string>::node_arg_type>{});

  EXPECT_TRUE(g.is_root("A"));
  EXPECT_TRUE(g.is_root("B"));
  EXPECT_EQ(g.args_of("A").size(), 0);
  EXPECT_EQ(g.args_of("B").size(), 0);

  // Mixed types in initializer list (should work)
  g.add("C", {"literal", std::string("object")});

  auto const &args_C = g.args_of("C");
  EXPECT_EQ(args_C.size(), 2);
  EXPECT_EQ(args_C[0].node, "literal");
  EXPECT_EQ(args_C[1].node, "object");
}

TEST_F(GraphTest, ReplaceEdgeCleanupVerification) {
  // Test that replace properly cleans up adjacency when no more connections exist
  g.add("B", {"A" | 0_p, "C" | 1_p});

  // Replace A with D
  g.replace("B", make_node_arg("A", 0), make_node_arg("D", 2));

  // A should no longer be a predecessor of B
  EXPECT_FALSE(g.pred_of("B").count("A"));
  // B should no longer be a successor of A
  EXPECT_FALSE(g.succ_of("A").count("B"));

  // But D should now be connected
  EXPECT_TRUE(g.pred_of("B").count("D"));
  EXPECT_TRUE(g.succ_of("D").count("B"));

  // C should still be connected
  EXPECT_TRUE(g.pred_of("B").count("C"));
  EXPECT_TRUE(g.succ_of("C").count("B"));
}

TEST_F(GraphTest, DuplicateEdgeRemovalEdgeCases) {
  // Add many duplicate edges and remove them in various patterns
  g.add("B", {"A" | 1_p, "A" | 2_p, "A" | 1_p, "A" | 3_p, "A" | 1_p});

  EXPECT_EQ(g.args_of("B").size(), 5);
  EXPECT_EQ(g.pred_of("B").size(), 1); // Only one unique predecessor

  // Remove one specific port (should remove all instances)
  g.rm("B", make_node_arg("A", 1));

  auto const &args = g.args_of("B");
  EXPECT_EQ(args.size(), 2); // Should have A:2 and A:3 left

  // Verify remaining edges
  bool found_port_2 = false, found_port_3 = false;
  for (const auto &arg : args) {
    if (arg.port == 2)
      found_port_2 = true;
    if (arg.port == 3)
      found_port_3 = true;
    if (arg.port == 1)
      FAIL() << "Port 1 should have been removed";
  }
  EXPECT_TRUE(found_port_2);
  EXPECT_TRUE(found_port_3);

  // A should still be a predecessor
  EXPECT_TRUE(g.pred_of("B").count("A"));
}

TEST_F(GraphTest, NodeTypeConsistencyWithStringTypes) {
  // Ensure different string types are treated consistently
  std::string str_obj = "object";
  const char *str_ptr = "pointer";
  char str_arr[] = "array";

  g.add("target", str_obj);
  g.add("target", str_ptr);
  g.add("target", str_arr);
  g.add("target", "literal");

  auto const &preds = g.pred_of("target");
  EXPECT_EQ(preds.size(), 4);
  EXPECT_TRUE(preds.count("object"));
  EXPECT_TRUE(preds.count("pointer"));
  EXPECT_TRUE(preds.count("array"));
  EXPECT_TRUE(preds.count("literal"));
}

TEST_F(GraphTest, ExtremePortValues) {
  // Test with extreme port values
  const uint32_t zero_port = 0;
  const uint32_t max_port = std::numeric_limits<uint32_t>::max();
  const uint32_t near_max = max_port - 1;

  g.add("consumer", {make_node_arg("producer", zero_port), make_node_arg("producer", max_port),
                     make_node_arg("producer", near_max)});

  auto const &args = g.args_of("consumer");
  EXPECT_EQ(args.size(), 3);
  EXPECT_EQ(args[0].port, zero_port);
  EXPECT_EQ(args[1].port, max_port);
  EXPECT_EQ(args[2].port, near_max);

  // Test removal with extreme port values
  g.rm("consumer", make_node_arg("producer", max_port));
  auto const &new_args = g.args_of("consumer");
  EXPECT_EQ(new_args.size(), 2);

  // Verify max_port was removed
  for (const auto &arg : new_args) {
    EXPECT_NE(arg.port, max_port);
  }
}

TEST_F(GraphTest, ComplexGraphMergeScenarios) {
  graph<std::string> g1, g2, g3;

  // Create overlapping but different subgraphs
  g1.add("shared", "unique1");
  g1.add("unique_to_g1", "shared");

  g2.add("shared", "unique2"); // Different predecessor, different observation
  g2.add("unique_to_g2", "shared");

  g3.add("shared", "unique3"); // Yet another different predecessor
  g3.add("bridge", {"unique_to_g1", "unique_to_g2"});

  // Merge all into g1
  g1.merge(g2);
  g1.merge(g3);

  // Verify final state
  EXPECT_EQ(g1.size(), 7); // shared, unique1, unique2, unique3, unique_to_g1, unique_to_g2, bridge

  // shared should keep g1's original configuration (lhs precedence)
  auto const &pred_shared = g1.pred_of("shared");
  EXPECT_EQ(pred_shared.size(), 1);
  EXPECT_TRUE(pred_shared.count("unique1"));

  // bridge should connect unique_to_g1 and unique_to_g2
  auto const &pred_bridge = g1.pred_of("bridge");
  EXPECT_EQ(pred_bridge.size(), 2);
  EXPECT_TRUE(pred_bridge.count("unique_to_g1"));
  EXPECT_TRUE(pred_bridge.count("unique_to_g2"));
}

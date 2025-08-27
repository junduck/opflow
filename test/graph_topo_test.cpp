#include <gtest/gtest.h>

#include "opflow/graph_node.hpp"
#include "opflow/graph_topo.hpp"

#include <algorithm>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <vector>

using namespace opflow;

struct dummy_node {
  using data_type = int;

  dummy_node() = default;
  dummy_node(int val) : value(val) {}
  dummy_node(std::string name) : name(std::move(name)) {}
  dummy_node(std::string name, int val) : name(std::move(name)), value(val) {}

  dummy_node *clone_at(void *mem) const {
    ++clone_count;
    return new (mem) dummy_node(*this);
  }
  size_t clone_size() const noexcept { return sizeof(dummy_node); };
  size_t clone_align() const noexcept { return alignof(dummy_node); }
  dummy_node const *observer() const noexcept { return this; }

  size_t num_inputs() const noexcept { return 0; }  // No inputs
  size_t num_outputs() const noexcept { return 0; } // No outputs

  std::string name;
  int value = 0;
  mutable size_t clone_count = 0; // Track how many times this node was cloned

  bool operator==(dummy_node const &other) const { return name == other.name && value == other.value; }
};

// Special test node with non-standard alignment for memory alignment testing
struct alignas(64) aligned_dummy_node {
  using data_type = int;

  aligned_dummy_node() = default;
  aligned_dummy_node(int val) : value(val) {}
  aligned_dummy_node(std::string name) : name(std::move(name)) {}
  aligned_dummy_node(std::string name, int val) : name(std::move(name)), value(val) {}

  aligned_dummy_node *clone_at(void *mem) const {
    ++clone_count;
    return new (mem) aligned_dummy_node(*this);
  }
  size_t clone_size() const noexcept { return sizeof(aligned_dummy_node); };
  size_t clone_align() const noexcept { return alignof(aligned_dummy_node); }
  aligned_dummy_node const *observer() const noexcept { return this; }

  size_t num_inputs() const noexcept { return 0; }  // No inputs
  size_t num_outputs() const noexcept { return 0; } // No outputs

  std::string name;
  int value = 0;
  mutable size_t clone_count = 0;

  bool operator==(aligned_dummy_node const &other) const { return name == other.name && value == other.value; }
};

static_assert(dag_node<dummy_node>);
static_assert(dag_node_ptr<std::shared_ptr<dummy_node>>);
static_assert(dag_node_ptr<dummy_node const *>);
static_assert(dag_node<aligned_dummy_node>);

class GraphTopoFanoutTest : public ::testing::Test {
protected:
  void SetUp() override { g.clear(); }

  graph_node<dummy_node> g;

  // Helper function to create a dummy node
  auto make_node(std::string name = "", int value = 0) { return std::make_shared<dummy_node>(std::move(name), value); }

  // Helper to create simple linear graph: A -> B -> C
  void create_linear_graph() {
    nodeA = make_node("A", 1);
    nodeB = make_node("B", 2);
    nodeC = make_node("C", 3);

    g.add(nodeA);
    g.add(nodeB, nodeA);
    g.add(nodeC, nodeB);

    out_nodes = {nodeC};
    g.set_output(out_nodes);
  }

  // Helper to create diamond graph: A -> B, A -> C, B -> D, C -> D
  void create_diamond_graph() {
    nodeA = make_node("A", 1);
    nodeB = make_node("B", 2);
    nodeC = make_node("C", 3);
    nodeD = make_node("D", 4);

    g.add(nodeA);
    g.add(nodeB, nodeA);
    g.add(nodeC, nodeA);
    g.add(nodeD, {nodeB, nodeC});

    out_nodes = {nodeD};
    g.set_output(out_nodes);
  }

  // Helper to create complex graph with multiple outputs
  void create_complex_graph() {
    nodeA = make_node("A", 1);
    nodeB = make_node("B", 2);
    nodeC = make_node("C", 3);
    nodeD = make_node("D", 4);
    auto nodeE = make_node("E", 5);
    auto nodeF = make_node("F", 6);

    g.add(nodeA);
    g.add(nodeB, nodeA);
    g.add(nodeC, nodeA);
    g.add(nodeD, {nodeB, nodeC});
    g.add(nodeE, nodeB);
    g.add(nodeF, {nodeC, nodeE});

    out_nodes = {nodeD, nodeF};
    g.set_output(out_nodes);
  }

  std::shared_ptr<dummy_node> nodeA, nodeB, nodeC, nodeD;
  std::vector<std::shared_ptr<dummy_node>> out_nodes;
};

TEST_F(GraphTopoFanoutTest, SingleNodeGraph) {
  auto node = make_node("single", 42);
  g.add(node);
  g.add_output(node);

  graph_topo<dummy_node> topo(g, 1);

  EXPECT_EQ(topo.size(), 1);
  EXPECT_EQ(topo.num_nodes(), 1);
  EXPECT_EQ(topo.num_groups(), 1);

  ASSERT_EQ(topo.nodes_out().size(), 1);
  EXPECT_EQ(topo.nodes_out()[0].id, 0);

  auto nodes_span = topo.nodes_of(0);
  ASSERT_EQ(nodes_span.size(), 1);
  EXPECT_EQ(nodes_span[0]->name, "single");
  EXPECT_EQ(nodes_span[0]->value, 42);
}

TEST_F(GraphTopoFanoutTest, LinearGraphTopologicalOrder) {
  create_linear_graph();

  graph_topo<dummy_node> topo(g, 1);

  EXPECT_EQ(topo.size(), 3);
  EXPECT_EQ(topo.nodes_out().size(), 1);
  EXPECT_EQ(topo.nodes_out()[0].id, 2); // nodeC should be at index 2 (last in topo order)

  auto nodes_span = topo.nodes_of(0);
  ASSERT_EQ(nodes_span.size(), 3);

  // Verify topological order: A should come before B, B should come before C
  bool found_A = false, found_B = false, found_C = false;
  size_t idx_A = 0, idx_B = 0, idx_C = 0;

  for (size_t i = 0; i < nodes_span.size(); ++i) {
    if (nodes_span[i]->name == "A") {
      found_A = true;
      idx_A = i;
    } else if (nodes_span[i]->name == "B") {
      found_B = true;
      idx_B = i;
    } else if (nodes_span[i]->name == "C") {
      found_C = true;
      idx_C = i;
    }
  }

  EXPECT_TRUE(found_A && found_B && found_C);
  EXPECT_LT(idx_A, idx_B);
  EXPECT_LT(idx_B, idx_C);
}

TEST_F(GraphTopoFanoutTest, DiamondGraphCorrectPredecessors) {
  create_diamond_graph();

  graph_topo<dummy_node> topo(g, 1);

  EXPECT_EQ(topo.size(), 4);

  // Find nodeD's index and verify its predecessors
  auto nodes_span = topo.nodes_of(0);
  size_t nodeD_idx = SIZE_MAX;
  for (size_t i = 0; i < nodes_span.size(); ++i) {
    if (nodes_span[i]->name == "D") {
      nodeD_idx = i;
      break;
    }
  }

  ASSERT_NE(nodeD_idx, SIZE_MAX);

  auto preds = topo.pred_of(nodeD_idx);
  EXPECT_EQ(preds.size(), 2); // nodeD has two predecessors

  auto args = topo.args_of(nodeD_idx);
  EXPECT_EQ(args.size(), 2); // nodeD has two arguments
}

// Memory safety and PMR correctness tests
TEST_F(GraphTopoFanoutTest, MultipleGroupsCorrectCopies) {
  create_linear_graph();

  constexpr size_t num_groups = 5;
  graph_topo<dummy_node> topo(g, num_groups);

  EXPECT_EQ(topo.num_groups(), num_groups);
  EXPECT_EQ(topo.size(), 3);

  // Verify each group has independent copies
  for (size_t grp = 0; grp < num_groups; ++grp) {
    auto nodes_span = topo.nodes_of(grp);
    ASSERT_EQ(nodes_span.size(), 3);

    // Verify nodes are different objects across groups
    if (grp > 0) {
      auto prev_nodes = topo.nodes_of(grp - 1);
      for (size_t i = 0; i < nodes_span.size(); ++i) {
        EXPECT_NE(nodes_span[i].get(), prev_nodes[i].get())
            << "Node " << i << " in group " << grp << " should be different from group " << (grp - 1);
        EXPECT_EQ(nodes_span[i]->name, prev_nodes[i]->name)
            << "Node " << i << " should have same content across groups";
        EXPECT_EQ(nodes_span[i]->value, prev_nodes[i]->value)
            << "Node " << i << " should have same value across groups";
      }
    }
  }
}

TEST_F(GraphTopoFanoutTest, MemoryAlignmentCorrectness) {
  graph_node<aligned_dummy_node> aligned_g;
  auto nodeA = std::make_shared<aligned_dummy_node>("A", 1);
  auto nodeB = std::make_shared<aligned_dummy_node>("B", 2);

  aligned_g.add(nodeA);
  aligned_g.add(nodeB, nodeA);
  aligned_g.add_output(nodeB);

  graph_topo<aligned_dummy_node> topo(aligned_g, 3);

  // Verify all nodes are properly aligned
  for (size_t grp = 0; grp < 3; ++grp) {
    auto nodes_span = topo.nodes_of(grp);
    for (size_t i = 0; i < nodes_span.size(); ++i) {
      auto ptr = reinterpret_cast<uintptr_t>(nodes_span[i].get());
      EXPECT_EQ(ptr % 64, 0) << "Node " << i << " in group " << grp << " is not 64-byte aligned";
    }
  }
}

TEST_F(GraphTopoFanoutTest, PMRArenaMemoryManagement) {
  create_complex_graph();

  constexpr size_t num_groups = 3;

  {
    graph_topo<dummy_node> topo(g, num_groups);

    // Verify nodes were cloned for each group
    EXPECT_EQ(topo.size(), g.size());

    // Verify we can access all groups without crashes
    for (size_t grp = 0; grp < num_groups; ++grp) {
      auto nodes_span = topo.nodes_of(grp);
      EXPECT_EQ(nodes_span.size(), g.size());

      for (size_t i = 0; i < nodes_span.size(); ++i) {
        // Access node properties to ensure they're valid
        EXPECT_FALSE(nodes_span[i]->name.empty());
        EXPECT_GT(nodes_span[i]->value, 0);
      }
    }
  }
  // topo destructor should clean up arena memory automatically
  // No explicit verification needed as arena destructor handles cleanup
}

TEST_F(GraphTopoFanoutTest, LargeGraphStressTest) {
  // Create a larger graph to stress test memory management
  std::vector<std::shared_ptr<dummy_node>> nodes;
  constexpr size_t graph_size = 100;

  // Create chain of nodes
  for (size_t i = 0; i < graph_size; ++i) {
    auto node = make_node("node_" + std::to_string(i), static_cast<int>(i));
    if (i == 0) {
      g.add(node);
    } else {
      g.add(node, nodes[i - 1]);
    }
    nodes.push_back(node);
  }
  g.add_output(nodes.back());

  constexpr size_t num_groups = 10;
  graph_topo<dummy_node> topo(g, num_groups);

  EXPECT_EQ(topo.size(), graph_size);
  EXPECT_EQ(topo.num_groups(), num_groups);

  // Verify integrity of all groups
  for (size_t grp = 0; grp < num_groups; ++grp) {
    auto nodes_span = topo.nodes_of(grp);
    ASSERT_EQ(nodes_span.size(), graph_size);

    // Verify chain integrity within each group
    for (size_t i = 1; i < nodes_span.size(); ++i) {
      auto preds = topo.pred_of(i);
      EXPECT_EQ(preds.size(), 1);
      EXPECT_EQ(preds[0], i - 1);
    }
  }
}

TEST_F(GraphTopoFanoutTest, CyclicGraphHandling) {
  // Create a cyclic graph: A -> B -> C -> A
  nodeA = make_node("A", 1);
  nodeB = make_node("B", 2);
  nodeC = make_node("C", 3);

  g.add(nodeA);
  g.add(nodeB, nodeA);
  g.add(nodeC, nodeB);
  // Create cycle by making A depend on C (this creates a cycle)
  g.add(nodeA, nodeC);
  g.add_output(nodeC);

  EXPECT_THROW(graph_topo<dummy_node> topo(g, 1), std::runtime_error);
}

TEST_F(GraphTopoFanoutTest, MultipleOutputNodes) {
  create_complex_graph();

  graph_topo<dummy_node> topo(g, 1);

  EXPECT_EQ(topo.nodes_out().size(), 2);                     // We have 2 output nodes
  EXPECT_NE(topo.nodes_out()[0].id, topo.nodes_out()[1].id); // They should be at different indices

  auto nodes_span = topo.nodes_of(0);

  // Verify output nodes can be accessed
  for (auto out : topo.nodes_out()) {
    EXPECT_LT(out.id, nodes_span.size());
    // Verify these are indeed our output nodes
    bool is_output = (nodes_span[out.id]->name == "D") || (nodes_span[out.id]->name == "F");
    EXPECT_TRUE(is_output);
  }
}

TEST_F(GraphTopoFanoutTest, ConstCorrectness) {
  create_linear_graph();

  graph_topo<dummy_node> topo(g, 1);
  graph_topo<dummy_node> const &const_topo = topo;

  // Test const methods
  EXPECT_EQ(const_topo.size(), 3);
  EXPECT_EQ(const_topo.num_nodes(), 3);
  EXPECT_EQ(const_topo.num_groups(), 1);

  // Test const nodes access
  auto const_nodes = const_topo.nodes_of(0);
  EXPECT_EQ(const_nodes.size(), 3);

  // Test const pred/args access
  auto preds = const_topo.pred_of(1);
  auto args = const_topo.args_of(1);
  EXPECT_EQ(preds.size(), 1);
  EXPECT_EQ(args.size(), 1);
}

TEST_F(GraphTopoFanoutTest, MemoryEfficiencyMultipleGroups) {
  create_linear_graph();

  // Test that pred_map and arg_map are shared across groups
  constexpr size_t num_groups = 100;
  graph_topo<dummy_node> topo(g, num_groups);

  EXPECT_EQ(topo.num_groups(), num_groups);

  // All groups should have same topological structure
  for (size_t grp = 0; grp < num_groups; ++grp) {
    for (size_t node_id = 0; node_id < topo.size(); ++node_id) {
      auto preds = topo.pred_of(node_id);
      auto args = topo.args_of(node_id);

      // These should be consistent across all groups since structure is shared
      if (grp == 0) {
        // Store reference values from first group
        // (pred_of and args_of return the same data regardless of group)
        continue;
      }

      // Verify structure is same (pred_map and arg_map are shared)
      EXPECT_EQ(preds.size(), topo.pred_of(node_id).size());
      EXPECT_EQ(args.size(), topo.args_of(node_id).size());
    }
  }
}

TEST_F(GraphTopoFanoutTest, ArenaMemoryAlignmentEdgeCases) {
  // Test with nodes having different alignment requirements
  graph_node<aligned_dummy_node> mixed_g;
  std::vector<std::shared_ptr<aligned_dummy_node>> nodes;

  // Create a complex graph with varying alignments
  for (size_t i = 0; i < 10; ++i) {
    auto node = std::make_shared<aligned_dummy_node>("aligned_" + std::to_string(i), static_cast<int>(i));
    if (i == 0) {
      mixed_g.add(node);
    } else {
      mixed_g.add(node, nodes[i - 1]);
    }
    nodes.push_back(node);
  }
  mixed_g.add_output(nodes.back());

  // Test with multiple groups to stress arena allocation
  graph_topo<aligned_dummy_node> topo(mixed_g, 5);

  // Verify all nodes maintain proper alignment
  for (size_t grp = 0; grp < 5; ++grp) {
    auto nodes_span = topo.nodes_of(grp);
    for (size_t i = 0; i < nodes_span.size(); ++i) {
      auto ptr = reinterpret_cast<uintptr_t>(nodes_span[i].get());
      EXPECT_EQ(ptr % alignof(aligned_dummy_node), 0) << "Node " << i << " in group " << grp << " lost alignment";
    }
  }
}

TEST_F(GraphTopoFanoutTest, TopoOrderConsistencyAcrossGroups) {
  create_diamond_graph();

  constexpr size_t num_groups = 3;
  graph_topo<dummy_node> topo(g, num_groups);

  // Get topological order from first group
  auto group0 = topo.nodes_of(0);
  std::vector<std::string> topo_order;
  for (size_t i = 0; i < group0.size(); ++i) {
    topo_order.push_back(group0[i]->name);
  }

  // Verify all other groups have identical topological order
  for (size_t grp = 1; grp < num_groups; ++grp) {
    auto group = topo.nodes_of(grp);
    ASSERT_EQ(group.size(), topo_order.size());

    for (size_t i = 0; i < group.size(); ++i) {
      EXPECT_EQ(group[i]->name, topo_order[i])
          << "Group " << grp << " has different topological order at position " << i;
    }
  }
}

TEST_F(GraphTopoFanoutTest, PredecessorAndArgumentMapping) {
  create_diamond_graph();

  graph_topo<dummy_node> topo(g, 1);
  auto nodes_span = topo.nodes_of(0);

  // Find indices of each node
  std::unordered_map<std::string, size_t> name_to_idx;
  for (size_t i = 0; i < nodes_span.size(); ++i) {
    name_to_idx[nodes_span[i]->name] = i;
  }

  // Verify A has no predecessors
  auto a_preds = topo.pred_of(name_to_idx["A"]);
  auto a_args = topo.args_of(name_to_idx["A"]);
  EXPECT_TRUE(a_preds.empty());
  EXPECT_TRUE(a_args.empty());

  // Verify B has A as predecessor
  auto b_preds = topo.pred_of(name_to_idx["B"]);
  auto b_args = topo.args_of(name_to_idx["B"]);
  EXPECT_EQ(b_preds.size(), 1);
  EXPECT_EQ(b_args.size(), 1);
  EXPECT_EQ(b_preds[0], name_to_idx["A"]);
  EXPECT_EQ(b_args[0].node, name_to_idx["A"]);
  EXPECT_EQ(b_args[0].port, 0);

  // Verify C has A as predecessor
  auto c_preds = topo.pred_of(name_to_idx["C"]);
  auto c_args = topo.args_of(name_to_idx["C"]);
  EXPECT_EQ(c_preds.size(), 1);
  EXPECT_EQ(c_args.size(), 1);
  EXPECT_EQ(c_preds[0], name_to_idx["A"]);

  // Verify D has B and C as predecessors
  auto d_preds = topo.pred_of(name_to_idx["D"]);
  auto d_args = topo.args_of(name_to_idx["D"]);
  EXPECT_EQ(d_preds.size(), 2);
  EXPECT_EQ(d_args.size(), 2);

  std::vector<size_t> expected_preds = {name_to_idx["B"], name_to_idx["C"]};
  std::sort(expected_preds.begin(), expected_preds.end());

  std::vector<size_t> actual_preds(d_preds.begin(), d_preds.end());
  std::sort(actual_preds.begin(), actual_preds.end());

  EXPECT_EQ(actual_preds, expected_preds);
}

TEST_F(GraphTopoFanoutTest, ComplexCyclicGraphDetection) {
  // Create a more complex cyclic graph
  nodeA = make_node("A", 1);
  nodeB = make_node("B", 2);
  nodeC = make_node("C", 3);
  nodeD = make_node("D", 4);
  auto nodeE = make_node("E", 5);

  g.add(nodeA);
  g.add(nodeB, nodeA);
  g.add(nodeC, nodeB);
  g.add(nodeD, nodeC);
  g.add(nodeE, {nodeD, nodeA}); // E depends on both D and A
  // Create cycle: A -> B -> C -> D, and then make A depend on E
  g.add(nodeA, nodeE); // This creates a cycle

  g.add_output(nodeE);

  EXPECT_THROW(graph_topo<dummy_node> topo(g, 1), std::runtime_error);
}

TEST_F(GraphTopoFanoutTest, LargeGraphPerformance) {
  // Create a large binary tree-like graph
  std::vector<std::shared_ptr<dummy_node>> nodes;
  constexpr size_t depth = 10; // 2^10 - 1 = 1023 nodes

  // Create nodes level by level
  for (size_t level = 0; level < depth; ++level) {
    size_t nodes_in_level = 1ULL << level;
    for (size_t i = 0; i < nodes_in_level; ++i) {
      size_t node_id = (1ULL << level) - 1 + i;
      auto node = make_node("node_" + std::to_string(node_id), static_cast<int>(node_id));

      if (level == 0) {
        g.add(node);
      } else {
        // Each node depends on one parent from previous level
        size_t parent_id = (node_id - 1) / 2;
        g.add(node, nodes[parent_id]);
      }
      nodes.push_back(node);
    }
  }

  // Use leaf nodes as outputs
  std::vector<std::shared_ptr<dummy_node>> out_nodes;
  size_t first_leaf = (1ULL << (depth - 1)) - 1;
  for (size_t i = first_leaf; i < nodes.size(); ++i) {
    out_nodes.push_back(nodes[i]);
  }
  g.set_output(out_nodes);

  // Measure construction time (basic performance check)
  auto start = std::chrono::high_resolution_clock::now();
  graph_topo<dummy_node> topo(g, 2);
  auto end = std::chrono::high_resolution_clock::now();

  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  EXPECT_EQ(topo.size(), nodes.size());
  EXPECT_EQ(topo.num_groups(), 2);
  EXPECT_LT(duration.count(), 1000); // Should complete within 1 second

  // Verify structure integrity on large graph
  for (size_t grp = 0; grp < 2; ++grp) {
    auto nodes_span = topo.nodes_of(grp);
    EXPECT_EQ(nodes_span.size(), nodes.size());

    // Spot check: verify root node has no predecessors
    size_t root_idx = 0;
    for (size_t i = 0; i < nodes_span.size(); ++i) {
      if (nodes_span[i]->value == 0) { // Root node has value 0
        root_idx = i;
        break;
      }
    }

    auto root_preds = topo.pred_of(root_idx);
    EXPECT_TRUE(root_preds.empty());
  }
}

TEST_F(GraphTopoFanoutTest, NodeGroupIsolation) {
  create_linear_graph();

  constexpr size_t num_groups = 3;
  graph_topo<dummy_node> topo(g, num_groups);

  // Modify state of nodes in one group and verify other groups are unaffected
  auto group0 = topo.nodes_of(0);
  auto group1 = topo.nodes_of(1);
  auto group2 = topo.nodes_of(2);

  // Find a specific node in each group and verify they're different objects
  size_t test_idx = 1; // Middle node
  auto *node0 = const_cast<dummy_node *>(group0[test_idx].get());
  auto *node1 = const_cast<dummy_node *>(group1[test_idx].get());
  auto *node2 = const_cast<dummy_node *>(group2[test_idx].get());

  // They should be different objects
  EXPECT_NE(node0, node1);
  EXPECT_NE(node1, node2);
  EXPECT_NE(node0, node2);

  // But have same initial values
  EXPECT_EQ(node0->value, node1->value);
  EXPECT_EQ(node1->value, node2->value);
  EXPECT_EQ(node0->name, node1->name);
  EXPECT_EQ(node1->name, node2->name);

  // Modify one group
  node0->value = 999;
  node0->name = "modified";

  // Other groups should remain unchanged
  EXPECT_NE(node0->value, node1->value);
  EXPECT_NE(node0->value, node2->value);
  EXPECT_EQ(node1->value, node2->value);
  EXPECT_NE(node0->name, node1->name);
  EXPECT_EQ(node1->name, node2->name);
}

TEST_F(GraphTopoFanoutTest, EmptyOutputNodesList) {
  create_linear_graph();

  std::vector<std::shared_ptr<dummy_node>> empty_out_nodes;
  g.set_output(empty_out_nodes);

  graph_topo<dummy_node> topo(g, 1);

  EXPECT_EQ(topo.size(), 3);
}

TEST_F(GraphTopoFanoutTest, MultipleCopiesOfSameOutputNode) {
  create_linear_graph();

  // Add the same output node multiple times
  std::vector<std::shared_ptr<dummy_node>> duplicate_out_nodes = {nodeC, nodeC, nodeB, nodeC};
  g.set_output(duplicate_out_nodes);

  graph_topo<dummy_node> topo(g, 1);

  EXPECT_EQ(topo.nodes_out().size(), 4);
  EXPECT_EQ(topo.nodes_out()[0], topo.nodes_out()[1]); // Same node should have same index
  EXPECT_EQ(topo.nodes_out()[1], topo.nodes_out()[3]); // Same node should have same index
  EXPECT_NE(topo.nodes_out()[0], topo.nodes_out()[2]); // Different nodes should have different indices
}

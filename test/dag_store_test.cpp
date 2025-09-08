#include <gtest/gtest.h>

#include "opflow/detail/dag_store.hpp"
#include "opflow/graph_named.hpp"
#include "opflow/graph_node.hpp"

#include <chrono>
#include <memory>
#include <unordered_map>
#include <vector>

using namespace opflow;
using namespace opflow::detail;

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

  size_t num_inputs() const noexcept { return 0; }  // No inputs
  size_t num_outputs() const noexcept { return 1; } // Prevent incompatible errors

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

  size_t num_inputs() const noexcept { return 0; }  // No inputs
  size_t num_outputs() const noexcept { return 1; } // Prevent incompatible errors

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
    g.add(nodeD, nodeB, nodeC);

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
    g.add(nodeD, nodeB, nodeC);
    g.add(nodeE, nodeB);
    g.add(nodeF, nodeC, nodeE);

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

  dag_store<dummy_node> topo(g, 1);

  EXPECT_EQ(topo.size(), 1);
  EXPECT_EQ(topo.num_nodes(), 1);
  EXPECT_EQ(topo.num_groups(), 1);

  auto nodes_span = topo[0];
  ASSERT_EQ(nodes_span.size(), 1);
  EXPECT_EQ(nodes_span[0]->name, "single");
  EXPECT_EQ(nodes_span[0]->value, 42);

  // Verify output mapping
  EXPECT_EQ(topo.output_offset.size(), 1);
  EXPECT_EQ(topo.output_offset[0].size, 1);
}

TEST_F(GraphTopoFanoutTest, LinearGraphTopologicalOrder) {
  create_linear_graph();

  dag_store<dummy_node> topo(g, 1);

  EXPECT_EQ(topo.size(), 3);
  EXPECT_EQ(topo.output_offset.size(), 1); // One output node

  auto nodes_span = topo[0];
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

  // Verify input/output structure through record offsets
  EXPECT_EQ(topo.record_offset.size(), 3);
  EXPECT_EQ(topo.input_offset.size(), 3);
}

TEST_F(GraphTopoFanoutTest, DiamondGraphCorrectPredecessors) {
  create_diamond_graph();

  dag_store<dummy_node> topo(g, 1);

  EXPECT_EQ(topo.size(), 4);

  // Verify the diamond structure is preserved in topological order
  auto nodes_span = topo[0];

  // Find indices of each node
  std::unordered_map<std::string, size_t> name_to_idx;
  for (size_t i = 0; i < nodes_span.size(); ++i) {
    name_to_idx[nodes_span[i]->name] = i;
  }

  // Verify diamond structure through topological ordering constraints
  // A should come before B and C, B and C should come before D
  EXPECT_LT(name_to_idx["A"], name_to_idx["B"]);
  EXPECT_LT(name_to_idx["A"], name_to_idx["C"]);
  EXPECT_LT(name_to_idx["B"], name_to_idx["D"]);
  EXPECT_LT(name_to_idx["C"], name_to_idx["D"]);

  // Verify input structure through input_offset
  EXPECT_EQ(topo.input_offset.size(), 4);

  // Node D should have 2 inputs (from B and C)
  auto nodeD_inputs = topo.input_offset[name_to_idx["D"]];
  EXPECT_EQ(nodeD_inputs.size(), 2);
}

// Memory safety and PMR correctness tests
TEST_F(GraphTopoFanoutTest, MultipleGroupsCorrectCopies) {
  create_linear_graph();

  constexpr size_t num_groups = 5;
  dag_store<dummy_node> topo(g, num_groups);

  EXPECT_EQ(topo.num_groups(), num_groups);
  EXPECT_EQ(topo.size(), 3);

  // Verify each group has independent copies
  for (size_t grp = 0; grp < num_groups; ++grp) {
    auto nodes_span = topo[grp];
    ASSERT_EQ(nodes_span.size(), 3);

    // Verify nodes are different objects across groups
    if (grp > 0) {
      auto prev_nodes = topo[grp - 1];
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

  dag_store<aligned_dummy_node> topo(aligned_g, 3);

  // Verify all nodes are properly aligned
  for (size_t grp = 0; grp < 3; ++grp) {
    auto nodes_span = topo[grp];
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
    dag_store<dummy_node> topo(g, num_groups);

    // Verify nodes were cloned for each group
    EXPECT_EQ(topo.size(), g.size());

    // Verify we can access all groups without crashes
    for (size_t grp = 0; grp < num_groups; ++grp) {
      auto nodes_span = topo[grp];
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
  dag_store<dummy_node> topo(g, num_groups);

  EXPECT_EQ(topo.size(), graph_size);
  EXPECT_EQ(topo.num_groups(), num_groups);

  // Verify integrity of all groups
  for (size_t grp = 0; grp < num_groups; ++grp) {
    auto nodes_span = topo[grp];
    ASSERT_EQ(nodes_span.size(), graph_size);

    // Verify chain structure through input_offset - each node (except first) should have 1 input
    for (size_t i = 1; i < nodes_span.size(); ++i) {
      auto inputs = topo.input_offset[i];
      EXPECT_EQ(inputs.size(), 1) << "Node " << i << " should have exactly 1 input";
    }

    // First node should have no inputs
    auto first_inputs = topo.input_offset[0];
    EXPECT_EQ(first_inputs.size(), 0) << "First node should have no inputs";
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

  EXPECT_THROW(dag_store<dummy_node> topo(g, 1), std::runtime_error);
}

TEST_F(GraphTopoFanoutTest, MultipleOutputNodes) {
  create_complex_graph();

  dag_store<dummy_node> topo(g, 1);

  EXPECT_EQ(topo.output_offset.size(), 2); // We have 2 output nodes

  auto nodes_span = topo[0];

  // Verify we have the expected nodes
  std::unordered_set<std::string> found_names;
  for (size_t i = 0; i < nodes_span.size(); ++i) {
    found_names.insert(nodes_span[i]->name);
  }

  // Should have all 6 nodes: A, B, C, D, E, F
  EXPECT_EQ(found_names.size(), 6);
  EXPECT_TRUE(found_names.count("A"));
  EXPECT_TRUE(found_names.count("B"));
  EXPECT_TRUE(found_names.count("C"));
  EXPECT_TRUE(found_names.count("D"));
  EXPECT_TRUE(found_names.count("E"));
  EXPECT_TRUE(found_names.count("F"));
}

TEST_F(GraphTopoFanoutTest, ConstCorrectness) {
  create_linear_graph();

  dag_store<dummy_node> topo(g, 1);
  dag_store<dummy_node> const &const_topo = topo;

  // Test const methods
  EXPECT_EQ(const_topo.size(), 3);
  EXPECT_EQ(const_topo.num_nodes(), 3);
  EXPECT_EQ(const_topo.num_groups(), 1);

  // Test const nodes access
  auto const_nodes = const_topo[0];
  EXPECT_EQ(const_nodes.size(), 3);

  // Test const public data access
  EXPECT_EQ(const_topo.record_offset.size(), 3);
  EXPECT_EQ(const_topo.input_offset.size(), 3);
  EXPECT_EQ(const_topo.output_offset.size(), 1);
}

TEST_F(GraphTopoFanoutTest, MemoryEfficiencyMultipleGroups) {
  create_linear_graph();

  // Test that the structure is efficiently shared across groups
  constexpr size_t num_groups = 100;
  dag_store<dummy_node> topo(g, num_groups);

  EXPECT_EQ(topo.num_groups(), num_groups);

  // All groups should have same topological structure
  for (size_t grp = 0; grp < num_groups; ++grp) {
    auto nodes_span = topo[grp];
    EXPECT_EQ(nodes_span.size(), 3);

    // Verify linear chain structure through input offsets
    for (size_t node_id = 0; node_id < topo.size(); ++node_id) {
      auto inputs = topo.input_offset[node_id];

      if (node_id == 0) {
        EXPECT_EQ(inputs.size(), 0); // First node has no inputs
      } else {
        EXPECT_EQ(inputs.size(), 1); // Other nodes have one input
      }
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
  dag_store<aligned_dummy_node> topo(mixed_g, 5);

  // Verify all nodes maintain proper alignment
  for (size_t grp = 0; grp < 5; ++grp) {
    auto nodes_span = topo[grp];
    for (size_t i = 0; i < nodes_span.size(); ++i) {
      auto ptr = reinterpret_cast<uintptr_t>(nodes_span[i].get());
      EXPECT_EQ(ptr % alignof(aligned_dummy_node), 0) << "Node " << i << " in group " << grp << " lost alignment";
    }
  }
}

TEST_F(GraphTopoFanoutTest, TopoOrderConsistencyAcrossGroups) {
  create_diamond_graph();

  constexpr size_t num_groups = 3;
  dag_store<dummy_node> topo(g, num_groups);

  // Get topological order from first group
  auto group0 = topo[0];
  std::vector<std::string> topo_order;
  for (size_t i = 0; i < group0.size(); ++i) {
    topo_order.push_back(group0[i]->name);
  }

  // Verify all other groups have identical topological order
  for (size_t grp = 1; grp < num_groups; ++grp) {
    auto group = topo[grp];
    ASSERT_EQ(group.size(), topo_order.size());

    for (size_t i = 0; i < group.size(); ++i) {
      EXPECT_EQ(group[i]->name, topo_order[i])
          << "Group " << grp << " has different topological order at position " << i;
    }
  }
}

TEST_F(GraphTopoFanoutTest, PredecessorAndArgumentMapping) {
  create_diamond_graph();

  dag_store<dummy_node> topo(g, 1);
  auto nodes_span = topo[0];

  // Find indices of each node
  std::unordered_map<std::string, size_t> name_to_idx;
  for (size_t i = 0; i < nodes_span.size(); ++i) {
    name_to_idx[nodes_span[i]->name] = i;
  }

  // Verify A has no inputs (root node)
  auto a_inputs = topo.input_offset[name_to_idx["A"]];
  EXPECT_TRUE(a_inputs.empty());

  // Verify B has A as input (should have 1 input)
  auto b_inputs = topo.input_offset[name_to_idx["B"]];
  EXPECT_EQ(b_inputs.size(), 1);

  // Verify C has A as input (should have 1 input)
  auto c_inputs = topo.input_offset[name_to_idx["C"]];
  EXPECT_EQ(c_inputs.size(), 1);

  // Verify D has B and C as inputs (should have 2 inputs)
  auto d_inputs = topo.input_offset[name_to_idx["D"]];
  EXPECT_EQ(d_inputs.size(), 2);

  // Verify topological ordering constraints hold
  EXPECT_LT(name_to_idx["A"], name_to_idx["B"]);
  EXPECT_LT(name_to_idx["A"], name_to_idx["C"]);
  EXPECT_LT(name_to_idx["B"], name_to_idx["D"]);
  EXPECT_LT(name_to_idx["C"], name_to_idx["D"]);
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
  g.add(nodeE, nodeD, nodeA); // E depends on both D and A
  // Create cycle: A -> B -> C -> D, and then make A depend on E
  g.add(nodeA, nodeE); // This creates a cycle

  g.add_output(nodeE);

  EXPECT_THROW(dag_store<dummy_node> topo(g, 1), std::runtime_error);
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
  dag_store<dummy_node> topo(g, 2);
  auto end = std::chrono::high_resolution_clock::now();

  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  EXPECT_EQ(topo.size(), nodes.size());
  EXPECT_EQ(topo.num_groups(), 2);
  EXPECT_LT(duration.count(), 1000); // Should complete within 1 second

  // Verify structure integrity on large graph
  for (size_t grp = 0; grp < 2; ++grp) {
    auto nodes_span = topo[grp];
    EXPECT_EQ(nodes_span.size(), nodes.size());

    // Spot check: verify root node has no inputs
    size_t root_idx = 0;
    for (size_t i = 0; i < nodes_span.size(); ++i) {
      if (nodes_span[i]->value == 0) { // Root node has value 0
        root_idx = i;
        break;
      }
    }

    auto root_inputs = topo.input_offset[root_idx];
    EXPECT_TRUE(root_inputs.empty());
  }
}

TEST_F(GraphTopoFanoutTest, NodeGroupIsolation) {
  create_linear_graph();

  constexpr size_t num_groups = 3;
  dag_store<dummy_node> topo(g, num_groups);

  // Modify state of nodes in one group and verify other groups are unaffected
  auto group0 = topo[0];
  auto group1 = topo[1];
  auto group2 = topo[2];

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

  dag_store<dummy_node> topo(g, 1);

  EXPECT_EQ(topo.size(), 3);
}

TEST_F(GraphTopoFanoutTest, MultipleCopiesOfSameOutputNode) {
  create_linear_graph();

  // Add the same output node multiple times
  std::vector<std::shared_ptr<dummy_node>> duplicate_out_nodes = {nodeC, nodeC, nodeB, nodeC};
  g.set_output(duplicate_out_nodes);

  dag_store<dummy_node> topo(g, 1);

  EXPECT_EQ(topo.output_offset.size(), 4); // Should have 4 output entries

  // Find the indices of nodeB and nodeC in the topology
  auto nodes_span = topo[0];
  size_t nodeB_idx = SIZE_MAX, nodeC_idx = SIZE_MAX;
  for (size_t i = 0; i < nodes_span.size(); ++i) {
    if (nodes_span[i]->name == "B") {
      nodeB_idx = i;
    } else if (nodes_span[i]->name == "C") {
      nodeC_idx = i;
    }
  }

  ASSERT_NE(nodeB_idx, SIZE_MAX);
  ASSERT_NE(nodeC_idx, SIZE_MAX);
  EXPECT_NE(nodeB_idx, nodeC_idx); // Should be different nodes at different indices
}

// Test class for testing dag_store compatibility with graph_named
class DagStoreGraphNamedTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Create some shared nodes for testing
    nodeA = std::make_shared<dummy_node>("A", 1);
    nodeB = std::make_shared<dummy_node>("B", 2);
    nodeC = std::make_shared<dummy_node>("C", 3);
    nodeD = std::make_shared<dummy_node>("D", 4);
  }

  graph_named<dummy_node> g;
  std::shared_ptr<dummy_node> nodeA, nodeB, nodeC, nodeD;

  // Helper to create simple linear graph: A -> B -> C
  void create_linear_graph_named() {
    g.add<dummy_node>("A", ctor_args, "A", 1);
    g.add<dummy_node>("B", "A", ctor_args, "B", 2);
    g.add<dummy_node>("C", "B", ctor_args, "C", 3);
    g.add_output("C");
  }

  // Helper to create diamond graph: A -> B, A -> C, B -> D, C -> D
  void create_diamond_graph_named() {
    g.add<dummy_node>("A", ctor_args, "A", 1);
    g.add<dummy_node>("B", "A", ctor_args, "B", 2);
    g.add<dummy_node>("C", "A", ctor_args, "C", 3);
    g.add<dummy_node>("D", std::vector<std::string>{"B", "C"}, "D", 4);
    g.add_output("D");
  }
};

TEST_F(DagStoreGraphNamedTest, SingleNodeGraphNamed) {
  g.add<dummy_node>("single", ctor_args, "single", 42);
  g.add_output("single");

  dag_store<dummy_node> topo(g, 1);

  EXPECT_EQ(topo.size(), 1);
  EXPECT_EQ(topo.num_nodes(), 1);
  EXPECT_EQ(topo.num_groups(), 1);

  auto nodes_span = topo[0];
  ASSERT_EQ(nodes_span.size(), 1);
  EXPECT_EQ(nodes_span[0]->name, "single");
  EXPECT_EQ(nodes_span[0]->value, 42);

  // Verify output mapping
  EXPECT_EQ(topo.output_offset.size(), 1);
  EXPECT_EQ(topo.output_offset[0].size, 1);
}

TEST_F(DagStoreGraphNamedTest, LinearGraphNamedTopologicalOrder) {
  create_linear_graph_named();

  dag_store<dummy_node> topo(g, 1);

  EXPECT_EQ(topo.size(), 3);
  EXPECT_EQ(topo.output_offset.size(), 1); // One output node

  auto nodes_span = topo[0];
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

  // Verify input/output structure through record offsets
  EXPECT_EQ(topo.record_offset.size(), 3);
  EXPECT_EQ(topo.input_offset.size(), 3);
}

TEST_F(DagStoreGraphNamedTest, DiamondGraphNamedCorrectPredecessors) {
  create_diamond_graph_named();

  dag_store<dummy_node> topo(g, 1);

  EXPECT_EQ(topo.size(), 4);

  // Verify the diamond structure is preserved in topological order
  auto nodes_span = topo[0];

  // Find indices of each node
  std::unordered_map<std::string, size_t> name_to_idx;
  for (size_t i = 0; i < nodes_span.size(); ++i) {
    name_to_idx[nodes_span[i]->name] = i;
  }

  // Verify diamond structure through topological ordering constraints
  // A should come before B and C, B and C should come before D
  EXPECT_LT(name_to_idx["A"], name_to_idx["B"]);
  EXPECT_LT(name_to_idx["A"], name_to_idx["C"]);
  EXPECT_LT(name_to_idx["B"], name_to_idx["D"]);
  EXPECT_LT(name_to_idx["C"], name_to_idx["D"]);

  // Verify input structure through input_offset
  EXPECT_EQ(topo.input_offset.size(), 4);

  // Node D should have 2 inputs (from B and C)
  auto nodeD_inputs = topo.input_offset[name_to_idx["D"]];
  EXPECT_EQ(nodeD_inputs.size(), 2);
}

TEST_F(DagStoreGraphNamedTest, MultipleGroupsGraphNamed) {
  create_linear_graph_named();

  constexpr size_t num_groups = 5;
  dag_store<dummy_node> topo(g, num_groups);

  EXPECT_EQ(topo.num_groups(), num_groups);
  EXPECT_EQ(topo.size(), 3);

  // Verify each group has independent copies
  for (size_t grp = 0; grp < num_groups; ++grp) {
    auto nodes_span = topo[grp];
    ASSERT_EQ(nodes_span.size(), 3);

    // Verify topological order is consistent
    std::unordered_map<std::string, size_t> name_to_idx;
    for (size_t i = 0; i < nodes_span.size(); ++i) {
      name_to_idx[nodes_span[i]->name] = i;
    }

    EXPECT_LT(name_to_idx["A"], name_to_idx["B"]);
    EXPECT_LT(name_to_idx["B"], name_to_idx["C"]);
  }
}

TEST_F(DagStoreGraphNamedTest, PortMappingGraphNamed) {
  // Test explicit port mapping with graph_named
  g.add<dummy_node>("A", ctor_args, "A", 1);
  g.add<dummy_node>("B", ctor_args, "B", 2);
  g.add<dummy_node>("C", std::vector<std::string>{"A.0", "B.0"}, "C", 3);
  // g.add<dummy_node>("C", "A.0", "B.0", ctor_args, "C", 3);
  g.add_output("C");
  EXPECT_TRUE(g.validate());

  dag_store<dummy_node> topo(g, 1);

  EXPECT_EQ(topo.size(), 3);

  auto nodes_span = topo[0];
  std::unordered_map<std::string, size_t> name_to_idx;
  for (size_t i = 0; i < nodes_span.size(); ++i) {
    name_to_idx[nodes_span[i]->name] = i;
  }

  // Node C should have 2 inputs
  auto nodeC_inputs = topo.input_offset[name_to_idx["C"]];
  EXPECT_EQ(nodeC_inputs.size(), 2);

  // Verify topological constraints
  EXPECT_LT(name_to_idx["A"], name_to_idx["C"]);
  EXPECT_LT(name_to_idx["B"], name_to_idx["C"]);
}

TEST_F(DagStoreGraphNamedTest, CyclicGraphNamedDetection) {
  // Create a cyclic graph using graph_named
  g.add<dummy_node>("A", ctor_args, "A", 1);
  g.add<dummy_node>("B", "A", ctor_args, "B", 2);
  g.add<dummy_node>("C", "B", ctor_args, "C", 3);

  // Create cycle by making A depend on C
  g.add_edge("A", "C");
  g.add_output("C");

  EXPECT_THROW(dag_store<dummy_node> topo(g, 1), std::runtime_error);
}

TEST_F(DagStoreGraphNamedTest, MultipleOutputsGraphNamed) {
  g.add<dummy_node>("A", ctor_args, "A", 1);
  g.add<dummy_node>("B", "A", ctor_args, "B", 2);
  g.add<dummy_node>("C", "A", ctor_args, "C", 3);
  g.add<dummy_node>("D", "B", ctor_args, "D", 4);
  g.add<dummy_node>("E", "C", ctor_args, "E", 5);

  // Set multiple outputs
  std::vector<std::string> outputs = {"D", "E"};
  g.set_output(outputs);

  dag_store<dummy_node> topo(g, 1);

  EXPECT_EQ(topo.size(), 5);
  EXPECT_EQ(topo.output_offset.size(), 2); // Two output nodes

  auto nodes_span = topo[0];

  // Verify all nodes are present
  std::unordered_set<std::string> found_names;
  for (size_t i = 0; i < nodes_span.size(); ++i) {
    found_names.insert(nodes_span[i]->name);
  }

  EXPECT_EQ(found_names.size(), 5);
  EXPECT_TRUE(found_names.count("A"));
  EXPECT_TRUE(found_names.count("B"));
  EXPECT_TRUE(found_names.count("C"));
  EXPECT_TRUE(found_names.count("D"));
  EXPECT_TRUE(found_names.count("E"));
}

TEST_F(DagStoreGraphNamedTest, EmptyGraphNamed) {
  // Test with empty graph_named
  EXPECT_EQ(g.size(), 0);

  dag_store<dummy_node> topo(g, 1);

  EXPECT_EQ(topo.size(), 0);
  EXPECT_EQ(topo.num_nodes(), 0);
  EXPECT_EQ(topo.output_offset.size(), 0);
}

TEST_F(DagStoreGraphNamedTest, GraphNamedWithEdgeTypes) {
  // Test using make_edge helper function
  g.add<dummy_node>("A", ctor_args, "A", 1);
  g.add<dummy_node>("B", ctor_args, "B", 2);

  // Add edge using make_edge
  auto edge = make_edge("A", 0);
  g.add<dummy_node>("C", std::vector<decltype(edge)>{edge, make_edge("B", 0)}, "C", 3);
  g.add_output("C");

  dag_store<dummy_node> topo(g, 1);

  EXPECT_EQ(topo.size(), 3);

  auto nodes_span = topo[0];
  std::unordered_map<std::string, size_t> name_to_idx;
  for (size_t i = 0; i < nodes_span.size(); ++i) {
    name_to_idx[nodes_span[i]->name] = i;
  }

  // Node C should have 2 inputs from A and B
  auto nodeC_inputs = topo.input_offset[name_to_idx["C"]];
  EXPECT_EQ(nodeC_inputs.size(), 2);
}

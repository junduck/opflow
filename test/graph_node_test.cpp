#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "opflow/graph_node.hpp"

using namespace opflow;

// Base class for all test nodes to make them polymorphic
struct base_node {
  using data_type = double;
  virtual ~base_node() = default;
};

// Test fixture nodes
struct dummy_node : public base_node {
  dummy_node(int id, std::string const &name) : id(id), name(name) {}

  int id = 0;
  std::string name;

  bool operator==(dummy_node const &other) const noexcept { return id == other.id && name == other.name; }
};

// Root node for testing
struct root_node : public base_node {
  root_node(size_t input_size) : input_size(input_size) {}

  size_t input_size = 0;

  bool operator==(root_node const &other) const noexcept { return input_size == other.input_size; }
};

// Template node for testing template instantiation
template <typename T>
struct template_node : public base_node {
  using data_type = T;

  template_node(T value) : value(value) {}

  T value;

  bool operator==(template_node const &other) const noexcept { return value == other.value; }
};

// Auxiliary node for testing
struct aux_node : public base_node {
  aux_node(std::string const &config) : config(config) {}

  std::string config;

  bool operator==(aux_node const &other) const noexcept { return config == other.config; }
};

// Supplementary node for testing
struct supp_node : public base_node {
  supp_node(std::string const &type) : type(type) {}

  std::string type;

  bool operator==(supp_node const &other) const noexcept { return type == other.type; }
};

class GraphNodeTest : public ::testing::Test {
protected:
  void SetUp() override {
    graph.clear();
    graph_int.clear();
    graph_with_default.clear();
  }

  graph_node<base_node> graph;
  graph_node<int> graph_int;
  graph_node<base_node, double> graph_with_default;
};

// Test graph_node_edge functionality
TEST(GraphNodeEdgeTest, DefaultPort) {
  auto node = std::make_shared<dummy_node>(1, "test");
  detail::graph_node_edge edge(node);

  EXPECT_EQ(edge.node, node);
  EXPECT_EQ(edge.port, 0u);
}

TEST(GraphNodeEdgeTest, ExplicitPort) {
  auto node = std::make_shared<dummy_node>(1, "test");
  detail::graph_node_edge edge(node, 5);

  EXPECT_EQ(edge.node, node);
  EXPECT_EQ(edge.port, 5u);
}

TEST(GraphNodeEdgeTest, PipeOperator) {
  auto node = std::make_shared<dummy_node>(1, "test");
  auto edge = node | 3;

  EXPECT_EQ(edge.node, node);
  EXPECT_EQ(edge.port, 3u);
}

TEST(GraphNodeEdgeTest, MakeEdgeFunction) {
  auto node = std::make_shared<dummy_node>(1, "test");
  auto edge1 = make_edge(node);
  auto edge2 = make_edge(node, 7);

  EXPECT_EQ(edge1.node, node);
  EXPECT_EQ(edge1.port, 0u);
  EXPECT_EQ(edge2.node, node);
  EXPECT_EQ(edge2.port, 7u);
}

TEST(GraphNodeEdgeTest, Equality) {
  auto node1 = std::make_shared<dummy_node>(1, "test1");
  auto node2 = std::make_shared<dummy_node>(2, "test2");

  detail::graph_node_edge<std::shared_ptr<dummy_node>> edge1(node1, 5);
  detail::graph_node_edge<std::shared_ptr<dummy_node>> edge2(node1, 5);
  detail::graph_node_edge<std::shared_ptr<dummy_node>> edge3(node1, 6);
  detail::graph_node_edge<std::shared_ptr<dummy_node>> edge4(node2, 5);

  EXPECT_EQ(edge1, edge2);
  EXPECT_NE(edge1, edge3);
  EXPECT_NE(edge1, edge4);
}

// Basic graph operations
TEST_F(GraphNodeTest, EmptyGraph) {
  EXPECT_TRUE(graph.empty());
  EXPECT_EQ(graph.size(), 0u);
}

TEST_F(GraphNodeTest, AddSingleNode) {
  auto node = graph.add<dummy_node>(1, "test").depends();

  EXPECT_FALSE(graph.empty());
  EXPECT_EQ(graph.size(), 1u);
  EXPECT_TRUE(graph.contains(node));

  auto dummy_ptr = std::dynamic_pointer_cast<dummy_node>(node);
  ASSERT_NE(dummy_ptr, nullptr);
  EXPECT_EQ(dummy_ptr->id, 1);
  EXPECT_EQ(dummy_ptr->name, "test");
}

TEST_F(GraphNodeTest, AddMultipleNodes) {
  auto node1 = graph.add<dummy_node>(1, "test1").depends();
  auto node2 = graph.add<dummy_node>(2, "test2").depends();
  auto node3 = graph.add<dummy_node>(3, "test3").depends();

  EXPECT_EQ(graph.size(), 3u);
  EXPECT_TRUE(graph.contains(node1));
  EXPECT_TRUE(graph.contains(node2));
  EXPECT_TRUE(graph.contains(node3));
}

TEST_F(GraphNodeTest, AddExistingNodePointer) {
  auto node = std::make_shared<dummy_node>(1, "test");
  auto added_node = graph.add(node).depends();

  EXPECT_EQ(added_node, node);
  EXPECT_EQ(graph.size(), 1u);
  EXPECT_TRUE(graph.contains(node));
}

TEST_F(GraphNodeTest, AddNullNode) {
  std::shared_ptr<dummy_node> null_node = nullptr;
  EXPECT_THROW(graph.add(null_node).depends(), std::invalid_argument);
}

TEST_F(GraphNodeTest, AddNodeWithDependencies) {
  auto root = graph.add<dummy_node>(0, "root").depends();
  auto child = graph.add<dummy_node>(1, "child").depends(root);

  EXPECT_EQ(graph.size(), 2u);

  // Check adjacency
  auto pred = graph.pred_of(child);
  EXPECT_EQ(pred.size(), 1u);
  EXPECT_TRUE(pred.contains(root));

  auto succ = graph.succ_of(root);
  EXPECT_EQ(succ.size(), 1u);
  EXPECT_TRUE(succ.contains(child));

  // Check args
  auto args = graph.args_of(child);
  EXPECT_EQ(args.size(), 1u);
  EXPECT_EQ(args[0].node, root);
  EXPECT_EQ(args[0].port, 0u);
}

TEST_F(GraphNodeTest, AddNodeWithMultipleDependencies) {
  auto input1 = graph.add<dummy_node>(1, "input1").depends();
  auto input2 = graph.add<dummy_node>(2, "input2").depends();
  auto processor = graph.add<dummy_node>(3, "processor").depends(input1, input2 | 3);

  auto pred = graph.pred_of(processor);
  EXPECT_EQ(pred.size(), 2u);
  EXPECT_TRUE(pred.contains(input1));
  EXPECT_TRUE(pred.contains(input2));

  auto args = graph.args_of(processor);
  EXPECT_EQ(args.size(), 2u);
  EXPECT_EQ(args[0].node, input1);
  EXPECT_EQ(args[0].port, 0u);
  EXPECT_EQ(args[1].node, input2);
  EXPECT_EQ(args[1].port, 3u);
}

TEST_F(GraphNodeTest, AddNodeWithRangeDependencies) {
  auto input1 = graph.add<dummy_node>(1, "input1").depends();
  auto input2 = graph.add<dummy_node>(2, "input2").depends();

  std::vector<graph_node<base_node>::shared_node_ptr> deps = {input1, input2};
  auto processor = graph.add<dummy_node>(3, "processor").depends(deps);

  auto pred = graph.pred_of(processor);
  EXPECT_EQ(pred.size(), 2u);
  EXPECT_TRUE(pred.contains(input1));
  EXPECT_TRUE(pred.contains(input2));

  auto args = graph.args_of(processor);
  EXPECT_EQ(args.size(), 2u);
  EXPECT_EQ(args[0].node, input1);
  EXPECT_EQ(args[0].port, 0u);
  EXPECT_EQ(args[1].node, input2);
  EXPECT_EQ(args[1].port, 0u);
}

TEST_F(GraphNodeTest, AddNodeWithEdgeTypes) {
  auto input1 = graph.add<dummy_node>(1, "input1").depends();
  auto input2 = graph.add<dummy_node>(2, "input2").depends();

  std::vector<graph_node<base_node>::edge_type> edge_deps = {make_edge(input1, 0), make_edge(input2, 5)};
  auto processor = graph.add<dummy_node>(3, "processor").depends(edge_deps);

  auto pred = graph.pred_of(processor);
  EXPECT_EQ(pred.size(), 2u);
  EXPECT_TRUE(pred.contains(input1));
  EXPECT_TRUE(pred.contains(input2));

  auto args = graph.args_of(processor);
  EXPECT_EQ(args.size(), 2u);
  EXPECT_EQ(args[0].node, input1);
  EXPECT_EQ(args[0].port, 0u);
  EXPECT_EQ(args[1].node, input2);
  EXPECT_EQ(args[1].port, 5u);
}

// Root node operations
TEST_F(GraphNodeTest, SetRootNode) {
  auto root = graph.root<root_node>(5);

  EXPECT_TRUE(graph.contains(root));
  EXPECT_EQ(graph.root(), root);
  EXPECT_TRUE(graph.is_root(root));

  auto root_ptr = std::dynamic_pointer_cast<root_node>(root);
  ASSERT_NE(root_ptr, nullptr);
  EXPECT_EQ(root_ptr->input_size, 5u);
}

TEST_F(GraphNodeTest, SetRootNodeFromExisting) {
  auto node = std::make_shared<root_node>(3);
  auto root = graph.root(node);

  EXPECT_EQ(root, node);
  EXPECT_EQ(graph.root(), node);
  EXPECT_TRUE(graph.contains(node));
}

TEST_F(GraphNodeTest, SetNullRootNode) {
  std::shared_ptr<root_node> null_node = nullptr;
  EXPECT_THROW(graph.root(null_node), std::invalid_argument);
}

TEST_F(GraphNodeTest, RootWithDefaultDataType) {
  auto root = graph_with_default.root<template_node>(3.14);

  auto template_ptr = std::dynamic_pointer_cast<template_node<double>>(root);
  ASSERT_NE(template_ptr, nullptr);
  EXPECT_EQ(template_ptr->value, 3.14);
}

// Auxiliary node operations
TEST_F(GraphNodeTest, AddAuxiliaryNode) {
  auto root = graph.root<root_node>(2);
  auto aux = graph.aux<aux_node>("clock_config").depends(0);

  EXPECT_EQ(graph.aux(), aux);

  auto aux_ptr = std::dynamic_pointer_cast<aux_node>(aux);
  ASSERT_NE(aux_ptr, nullptr);
  EXPECT_EQ(aux_ptr->config, "clock_config");

  auto aux_args = graph.aux_args();
  EXPECT_EQ(aux_args.size(), 1u);
  EXPECT_EQ(aux_args[0], 0u);
}

TEST_F(GraphNodeTest, AuxiliaryWithMultiplePorts) {
  auto root = graph.root<root_node>(4);
  auto aux = graph.aux<aux_node>("multi_config").depends(0, 2, 3);

  auto aux_args = graph.aux_args();
  EXPECT_EQ(aux_args.size(), 3u);
  EXPECT_EQ(aux_args[0], 0u);
  EXPECT_EQ(aux_args[1], 2u);
  EXPECT_EQ(aux_args[2], 3u);
}

TEST_F(GraphNodeTest, AuxiliaryWithPortRange) {
  auto root = graph.root<root_node>(3);
  std::vector<u32> ports = {0, 1, 2};
  auto aux = graph.aux<aux_node>("range_config").depends(ports);

  auto aux_args = graph.aux_args();
  EXPECT_EQ(aux_args.size(), 3u);
  EXPECT_EQ(aux_args[0], 0u);
  EXPECT_EQ(aux_args[1], 1u);
  EXPECT_EQ(aux_args[2], 2u);
}

TEST_F(GraphNodeTest, AddNullAuxiliaryNode) {
  std::shared_ptr<aux_node> null_aux = nullptr;
  EXPECT_THROW(graph.aux(null_aux).depends(), std::invalid_argument);
}

// Supplementary root operations
TEST_F(GraphNodeTest, SetSuppRootNode) {
  auto supp = graph.supp_root<supp_node>("params");

  EXPECT_EQ(graph.supp_root(), supp);

  auto supp_ptr = std::dynamic_pointer_cast<supp_node>(supp);
  ASSERT_NE(supp_ptr, nullptr);
  EXPECT_EQ(supp_ptr->type, "params");
}

TEST_F(GraphNodeTest, SetSuppRootFromExisting) {
  auto node = std::make_shared<supp_node>("signals");
  auto supp = graph.supp_root(node);

  EXPECT_EQ(supp, node);
  EXPECT_EQ(graph.supp_root(), node);
}

TEST_F(GraphNodeTest, SetNullSuppRootNode) {
  std::shared_ptr<supp_node> null_node = nullptr;
  EXPECT_THROW(graph.supp_root(null_node), std::invalid_argument);
}

TEST_F(GraphNodeTest, SuppLinkOperations) {
  auto supp = graph.supp_root<supp_node>("params");
  auto node1 = graph.add<dummy_node>(1, "node1").depends();
  auto node2 = graph.add<dummy_node>(2, "node2").depends();

  // Link nodes to supplementary root ports
  graph.supp_link(node1, 0u, 1u);
  graph.supp_link(node2, 2u);

  auto supp_args1 = graph.supp_link_of(node1);
  EXPECT_EQ(supp_args1.size(), 2u);
  EXPECT_EQ(supp_args1[0], 0u);
  EXPECT_EQ(supp_args1[1], 1u);

  auto supp_args2 = graph.supp_link_of(node2);
  EXPECT_EQ(supp_args2.size(), 1u);
  EXPECT_EQ(supp_args2[0], 2u);

  // Test non-linked node
  auto node3 = graph.add<dummy_node>(3, "node3").depends();
  auto supp_args3 = graph.supp_link_of(node3);
  EXPECT_TRUE(supp_args3.empty());
}

TEST_F(GraphNodeTest, SuppLinkWithPortRange) {
  auto supp = graph.supp_root<supp_node>("params");
  auto node = graph.add<dummy_node>(1, "node").depends();

  std::vector<u32> ports = {0, 2, 4};
  graph.supp_link(node, ports);

  auto supp_args = graph.supp_link_of(node);
  EXPECT_EQ(supp_args.size(), 3u);
  EXPECT_EQ(supp_args[0], 0u);
  EXPECT_EQ(supp_args[1], 2u);
  EXPECT_EQ(supp_args[2], 4u);
}

TEST_F(GraphNodeTest, SuppLinkWithEdges) {
  auto supp = graph.supp_root<supp_node>("params");
  auto node = graph.add<dummy_node>(1, "node").depends();

  graph.supp_link(node, 1, 3);

  auto supp_args = graph.supp_link_of(node);
  EXPECT_EQ(supp_args.size(), 2u);
  EXPECT_EQ(supp_args[0], 1u);
  EXPECT_EQ(supp_args[1], 3u);
}

// Output operations
TEST_F(GraphNodeTest, AddSingleOutput) {
  auto node = graph.add<dummy_node>(1, "node").depends();
  graph.add_output(node);

  auto output = graph.output();
  EXPECT_EQ(output.size(), 1u);
  EXPECT_EQ(output[0].node, node);
  EXPECT_EQ(output[0].port, 0u);
}

TEST_F(GraphNodeTest, AddMultipleOutputs) {
  auto node1 = graph.add<dummy_node>(1, "node1").depends();
  auto node2 = graph.add<dummy_node>(2, "node2").depends();
  auto node3 = graph.add<dummy_node>(3, "node3").depends();

  graph.add_output(node1, node2 | 2, node3);

  auto output = graph.output();
  EXPECT_EQ(output.size(), 3u);
  EXPECT_EQ(output[0].node, node1);
  EXPECT_EQ(output[0].port, 0u);
  EXPECT_EQ(output[1].node, node2);
  EXPECT_EQ(output[1].port, 2u);
  EXPECT_EQ(output[2].node, node3);
  EXPECT_EQ(output[2].port, 0u);
}

TEST_F(GraphNodeTest, AddOutputRange) {
  auto node1 = graph.add<dummy_node>(1, "node1").depends();
  auto node2 = graph.add<dummy_node>(2, "node2").depends();

  std::vector<graph_node<base_node>::shared_node_ptr> outputs = {node1, node2};
  graph.add_output(outputs);

  auto output = graph.output();
  EXPECT_EQ(output.size(), 2u);
  EXPECT_EQ(output[0].node, node1);
  EXPECT_EQ(output[1].node, node2);
}

TEST_F(GraphNodeTest, AddOutputEdges) {
  auto node1 = graph.add<dummy_node>(1, "node1").depends();
  auto node2 = graph.add<dummy_node>(2, "node2").depends();

  std::vector<graph_node<base_node>::edge_type> edge_outputs = {make_edge(node1, 1), make_edge(node2, 3)};
  graph.add_output(edge_outputs);

  auto output = graph.output();
  EXPECT_EQ(output.size(), 2u);
  EXPECT_EQ(output[0].node, node1);
  EXPECT_EQ(output[0].port, 1u);
  EXPECT_EQ(output[1].node, node2);
  EXPECT_EQ(output[1].port, 3u);
}

// Graph utilities
TEST_F(GraphNodeTest, FindRootsAndLeaves) {
  auto root1 = graph.add<dummy_node>(1, "root1").depends();
  auto root2 = graph.add<dummy_node>(2, "root2").depends();
  auto middle = graph.add<dummy_node>(3, "middle").depends(root1, root2);
  auto leaf1 = graph.add<dummy_node>(4, "leaf1").depends(middle);
  auto leaf2 = graph.add<dummy_node>(5, "leaf2").depends(middle);

  auto roots = graph.roots();
  EXPECT_EQ(roots.size(), 2u);
  EXPECT_TRUE(std::find(roots.begin(), roots.end(), root1) != roots.end());
  EXPECT_TRUE(std::find(roots.begin(), roots.end(), root2) != roots.end());

  auto leaves = graph.leaves();
  EXPECT_EQ(leaves.size(), 2u);
  EXPECT_TRUE(std::find(leaves.begin(), leaves.end(), leaf1) != leaves.end());
  EXPECT_TRUE(std::find(leaves.begin(), leaves.end(), leaf2) != leaves.end());

  EXPECT_TRUE(graph.is_root(root1));
  EXPECT_TRUE(graph.is_root(root2));
  EXPECT_FALSE(graph.is_root(middle));

  EXPECT_TRUE(graph.is_leaf(leaf1));
  EXPECT_TRUE(graph.is_leaf(leaf2));
  EXPECT_FALSE(graph.is_leaf(middle));
}

TEST_F(GraphNodeTest, Clear) {
  auto node1 = graph.add<dummy_node>(1, "node1").depends();
  auto node2 = graph.add<dummy_node>(2, "node2").depends(node1);
  graph.add_output(node2);

  EXPECT_FALSE(graph.empty());

  graph.clear();

  EXPECT_TRUE(graph.empty());
  EXPECT_EQ(graph.size(), 0u);
  EXPECT_EQ(graph.output().size(), 0u);
}

TEST_F(GraphNodeTest, NodeRetrieval) {
  auto node1 = graph.add<dummy_node>(1, "test1").depends();
  auto node2 = graph.add<dummy_node>(2, "test2").depends();

  // Test that node() returns the same pointer for existing nodes
  EXPECT_EQ(graph.node(node1), node1);
  EXPECT_EQ(graph.node(node2), node2);

  // Test that node() returns nullptr for non-existing nodes
  auto non_existing = std::make_shared<dummy_node>(99, "non_existing");
  EXPECT_EQ(graph.node(non_existing), nullptr);
}

// Validation
TEST_F(GraphNodeTest, ValidateEmptyGraph) { EXPECT_TRUE(graph.validate()); }

TEST_F(GraphNodeTest, ValidateSimpleGraph) {
  auto node1 = graph.add<dummy_node>(1, "node1").depends();
  auto node2 = graph.add<dummy_node>(2, "node2").depends(node1);
  graph.add_output(node2);

  EXPECT_TRUE(graph.validate());
}

TEST_F(GraphNodeTest, ValidateWithAuxiliary) {
  auto root = graph.root<root_node>(2);
  auto aux = graph.aux<aux_node>("config").depends(1);
  auto node = graph.add<dummy_node>(1, "node").depends(root);

  EXPECT_TRUE(graph.validate());
}

// Template node testing
TEST_F(GraphNodeTest, TemplateNodes) {
  graph_node<base_node, int> template_graph;

  auto int_node = template_graph.add<template_node>(42).depends();

  auto template_ptr = std::dynamic_pointer_cast<template_node<int>>(int_node);
  ASSERT_NE(template_ptr, nullptr);
  EXPECT_EQ(template_ptr->value, 42);
}

// Complex graph structure test
TEST_F(GraphNodeTest, ComplexGraphStructure) {
  /*
  Build a complex graph:
      Root ──┬── A ──┬── D ── H
             │       └── E
             ├── B ── F
             └── C ── G

      Output: E, F, G, H
      Aux: connected to Root
      Supp: connected to A, D, F, G
  */

  // Build the main graph
  auto root = graph.root<root_node>(3);
  auto a = graph.add<dummy_node>(1, "A").depends(root | 0);
  auto b = graph.add<dummy_node>(2, "B").depends(root | 1);
  auto c = graph.add<dummy_node>(3, "C").depends(root | 2);

  auto d = graph.add<dummy_node>(4, "D").depends(a);
  auto e = graph.add<dummy_node>(5, "E").depends(a);
  auto f = graph.add<dummy_node>(6, "F").depends(b);
  auto g = graph.add<dummy_node>(7, "G").depends(c);
  auto h = graph.add<dummy_node>(8, "H").depends(d);

  graph.add_output(e, f, g, h);

  // Add auxiliary node
  auto aux = graph.aux<aux_node>("clock").depends(0);

  // Add supplementary root and links
  auto supp = graph.supp_root<supp_node>("params");
  graph.supp_link(a, 0u);
  graph.supp_link(d, 1u);
  graph.supp_link(f, 2u);
  graph.supp_link(g, 3u);

  // Verify the structure
  EXPECT_EQ(graph.size(), 9u); // Root + A, B, C, D, E, F, G, H

  // Verify Root node
  EXPECT_TRUE(graph.is_root(root));
  auto root_succs = graph.succ_of(root);
  EXPECT_EQ(root_succs.size(), 3u);
  EXPECT_TRUE(root_succs.contains(a));
  EXPECT_TRUE(root_succs.contains(b));
  EXPECT_TRUE(root_succs.contains(c));

  // Verify A's dependencies and successors
  auto a_preds = graph.pred_of(a);
  EXPECT_EQ(a_preds.size(), 1u);
  EXPECT_TRUE(a_preds.contains(root));
  auto a_succs = graph.succ_of(a);
  EXPECT_EQ(a_succs.size(), 2u);
  EXPECT_TRUE(a_succs.contains(d));
  EXPECT_TRUE(a_succs.contains(e));

  // Verify port specifications
  auto a_args = graph.args_of(a);
  EXPECT_EQ(a_args.size(), 1u);
  EXPECT_EQ(a_args[0].node, root);
  EXPECT_EQ(a_args[0].port, 0u);

  // Verify output nodes are leaves
  EXPECT_TRUE(graph.is_leaf(e));
  EXPECT_TRUE(graph.is_leaf(f));
  EXPECT_TRUE(graph.is_leaf(g));
  EXPECT_TRUE(graph.is_leaf(h));

  // Verify outputs
  auto outputs = graph.output();
  EXPECT_EQ(outputs.size(), 4u);

  // Verify auxiliary
  EXPECT_EQ(graph.aux(), aux);
  auto aux_args = graph.aux_args();
  EXPECT_EQ(aux_args.size(), 1u);
  EXPECT_EQ(aux_args[0], 0u);

  // Verify supplementary links
  EXPECT_EQ(graph.supp_root(), supp);
  auto a_supp = graph.supp_link_of(a);
  EXPECT_EQ(a_supp.size(), 1u);
  EXPECT_EQ(a_supp[0], 0u);

  auto d_supp = graph.supp_link_of(d);
  EXPECT_EQ(d_supp.size(), 1u);
  EXPECT_EQ(d_supp[0], 1u);

  // Verify validation passes
  EXPECT_TRUE(graph.validate());
}

// Test generic nature of graph_node with primitive types
TEST_F(GraphNodeTest, GenericWithPrimitiveTypes) {
  // Test that graph_node can work with primitive types like int
  auto value1 = graph_int.add<int>(42).depends();
  auto value2 = graph_int.add<int>(100).depends();
  auto sum = graph_int.add<int>(142).depends(value1, value2);

  EXPECT_EQ(graph_int.size(), 3u);
  EXPECT_TRUE(graph_int.contains(value1));
  EXPECT_TRUE(graph_int.contains(value2));
  EXPECT_TRUE(graph_int.contains(sum));

  // Check that sum depends on both values
  auto pred = graph_int.pred_of(sum);
  EXPECT_EQ(pred.size(), 2u);
  EXPECT_TRUE(pred.contains(value1));
  EXPECT_TRUE(pred.contains(value2));

  // Check the stored values
  ASSERT_NE(value1, nullptr);
  ASSERT_NE(value2, nullptr);
  ASSERT_NE(sum, nullptr);

  EXPECT_EQ(*value1, 42);
  EXPECT_EQ(*value2, 100);
  EXPECT_EQ(*sum, 142);

  // Test graph operations work with primitive types
  auto roots = graph_int.roots();
  EXPECT_EQ(roots.size(), 2u);
  EXPECT_TRUE(std::find(roots.begin(), roots.end(), value1) != roots.end());
  EXPECT_TRUE(std::find(roots.begin(), roots.end(), value2) != roots.end());

  auto leaves = graph_int.leaves();
  EXPECT_EQ(leaves.size(), 1u);
  EXPECT_TRUE(std::find(leaves.begin(), leaves.end(), sum) != leaves.end());

  EXPECT_TRUE(graph_int.is_root(value1));
  EXPECT_TRUE(graph_int.is_root(value2));
  EXPECT_TRUE(graph_int.is_leaf(sum));
  EXPECT_FALSE(graph_int.is_root(sum));
}

// Edge cases and error handling
TEST_F(GraphNodeTest, GetNonexistentNodeData) {
  auto non_existing = std::make_shared<dummy_node>(99, "non_existing");

  auto pred = graph.pred_of(non_existing);
  EXPECT_EQ(pred.size(), 0u);

  auto succ = graph.succ_of(non_existing);
  EXPECT_EQ(succ.size(), 0u);

  auto args = graph.args_of(non_existing);
  EXPECT_EQ(args.size(), 0u);

  auto supp_args = graph.supp_link_of(non_existing);
  EXPECT_EQ(supp_args.size(), 0u);
}

TEST_F(GraphNodeTest, SuppLinkAccessMap) {
  auto supp = graph.supp_root<supp_node>("params");
  auto node1 = graph.add<dummy_node>(1, "node1").depends();
  auto node2 = graph.add<dummy_node>(2, "node2").depends();

  graph.supp_link(node1, 0u, 1u);
  graph.supp_link(node2, 2u);

  auto supp_link_map = graph.supp_link();
  EXPECT_EQ(supp_link_map.size(), 2u);
  EXPECT_TRUE(supp_link_map.contains(node1));
  EXPECT_TRUE(supp_link_map.contains(node2));

  auto &node1_ports = supp_link_map.at(node1);
  EXPECT_EQ(node1_ports.size(), 2u);
  EXPECT_EQ(node1_ports[0], 0u);
  EXPECT_EQ(node1_ports[1], 1u);

  auto &node2_ports = supp_link_map.at(node2);
  EXPECT_EQ(node2_ports.size(), 1u);
  EXPECT_EQ(node2_ports[0], 2u);
}

TEST_F(GraphNodeTest, AccessInternalMaps) {
  auto root = graph.add<dummy_node>(0, "root").depends();
  auto child1 = graph.add<dummy_node>(1, "child1").depends(root);
  auto child2 = graph.add<dummy_node>(2, "child2").depends(root | 1);

  // Test predecessor map
  auto pred_map = graph.pred();
  EXPECT_EQ(pred_map.size(), 3u);
  EXPECT_TRUE(pred_map.contains(root));
  EXPECT_TRUE(pred_map.contains(child1));
  EXPECT_TRUE(pred_map.contains(child2));

  // Test successor map
  auto succ_map = graph.succ();
  EXPECT_EQ(succ_map.size(), 3u);
  auto root_succs = succ_map.at(root);
  EXPECT_EQ(root_succs.size(), 2u);
  EXPECT_TRUE(root_succs.contains(child1));
  EXPECT_TRUE(root_succs.contains(child2));

  // Test args map
  auto args_map = graph.args();
  EXPECT_EQ(args_map.size(), 3u);
  auto child2_args = args_map.at(child2);
  EXPECT_EQ(child2_args.size(), 1u);
  EXPECT_EQ(child2_args[0].node, root);
  EXPECT_EQ(child2_args[0].port, 1u);
}

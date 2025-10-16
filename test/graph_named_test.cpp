#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "opflow/graph_named.hpp"

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

// First ctor arg is string like, test proper param splitting using ctor_args
struct dummy_node2 : public base_node {
  dummy_node2(std::string const &name, int id) : name(name), id(id) {}

  std::string name;
  int id = 0;

  bool operator==(dummy_node2 const &other) const noexcept { return name == other.name && id == other.id; }
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

class GraphNodeNamedTest : public ::testing::Test {
protected:
  void SetUp() override {
    graph.clear();
    graph_int.clear();
    graph_with_aux.clear();
  }

  graph_named<base_node> graph;
  graph_named<int> graph_int;
  graph_named<base_node> graph_with_aux;
};

// Test graph_named_edge parsing
TEST(GraphEdgeTest, DefaultPort) {
  detail::graph_named_edge edge("node");
  EXPECT_EQ(edge.name, "node");
  EXPECT_EQ(edge.port, 0u);
  EXPECT_EQ(std::string(edge), "node.0");

  detail::graph_named_edge<'@'> edge2("node.abc");
  EXPECT_EQ(edge2.name, "node.abc");
  EXPECT_EQ(edge2.port, 0u);
  EXPECT_EQ(std::string(edge2), "node.abc@0");
}

TEST(GraphEdgeTest, ExplicitPort) {
  detail::graph_named_edge edge("node.5");
  EXPECT_EQ(edge.name, "node");
  EXPECT_EQ(edge.port, 5u);
  EXPECT_EQ(std::string(edge), "node.5");
}

TEST(GraphEdgeTest, PortOutOfRange) {
  EXPECT_THROW(detail::graph_named_edge("node.999999999999999999999"), std::system_error);
}

TEST(GraphEdgeTest, ConstructorWithNameAndPort) {
  detail::graph_named_edge edge("test_node", 42);
  EXPECT_EQ(edge.name, "test_node");
  EXPECT_EQ(edge.port, 42u);
  EXPECT_EQ(std::string(edge), "test_node.42");

  EXPECT_THROW(detail::graph_named_edge edge2("test.node.42"), std::invalid_argument);
}

TEST(GraphEdgeTest, Equality) {
  detail::graph_named_edge edge1("node", 5);
  detail::graph_named_edge edge2("node", 5);
  detail::graph_named_edge edge3("node", 6);
  detail::graph_named_edge edge4("other", 5);

  EXPECT_EQ(edge1, edge2);
  EXPECT_NE(edge1, edge3);
  EXPECT_NE(edge1, edge4);
}

// Basic graph operations
TEST_F(GraphNodeNamedTest, EmptyGraph) {
  EXPECT_TRUE(graph.empty());
  EXPECT_EQ(graph.size(), 0u);
  EXPECT_FALSE(graph.contains("nonexistent"));
}

TEST_F(GraphNodeNamedTest, AddSingleNode) {
  graph.add<dummy_node>("node1", 1, "test").depends();

  EXPECT_FALSE(graph.empty());
  EXPECT_EQ(graph.size(), 1u);
  EXPECT_TRUE(graph.contains("node1"));

  auto node = graph.node("node1");
  ASSERT_NE(node, nullptr);
  auto dummy_ptr = std::dynamic_pointer_cast<dummy_node>(node);
  ASSERT_NE(dummy_ptr, nullptr);
  EXPECT_EQ(dummy_ptr->id, 1);
  EXPECT_EQ(dummy_ptr->name, "test");
}

TEST_F(GraphNodeNamedTest, AddMultipleNodes) {
  graph.add<dummy_node>("node1", 1, "test1").depends();
  graph.add<dummy_node>("node2", 2, "test2").depends();
  graph.add<dummy_node>("node3", 3, "test3").depends();

  EXPECT_EQ(graph.size(), 3u);
  EXPECT_TRUE(graph.contains("node1"));
  EXPECT_TRUE(graph.contains("node2"));
  EXPECT_TRUE(graph.contains("node3"));
}

TEST_F(GraphNodeNamedTest, FluentChaining) {
  // Test that fluent API returns graph reference for chaining
  graph.add<dummy_node>("node1", 1, "node1")
      .depends()
      .add<dummy_node>("node2", 2, "node2")
      .depends("node1")
      .add<dummy_node>("node3", 3, "node3")
      .depends("node2")
      .add_output("node3");

  EXPECT_EQ(graph.size(), 3u);
  EXPECT_TRUE(graph.contains("node1"));
  EXPECT_TRUE(graph.contains("node2"));
  EXPECT_TRUE(graph.contains("node3"));

  // Verify dependencies
  auto pred2 = graph.pred_of("node2");
  EXPECT_TRUE(pred2.contains("node1"));

  auto pred3 = graph.pred_of("node3");
  EXPECT_TRUE(pred3.contains("node2"));

  // Verify output
  auto outputs = graph.output();
  EXPECT_EQ(outputs.size(), 1u);
  EXPECT_EQ(outputs[0].name, "node3");
}

TEST_F(GraphNodeNamedTest, AddNodeWithDependencies) {
  graph.add<dummy_node>("root", 0, "root").depends();
  graph.add<dummy_node>("child", 1, "child").depends("root");

  EXPECT_EQ(graph.size(), 2u);

  // Check adjacency
  auto pred = graph.pred_of("child");
  EXPECT_EQ(pred.size(), 1u);
  EXPECT_TRUE(pred.contains("root"));

  auto succ = graph.succ_of("root");
  EXPECT_EQ(succ.size(), 1u);
  EXPECT_TRUE(succ.contains("child"));

  // Check args
  auto args = graph.args_of("child");
  EXPECT_EQ(args.size(), 1u);
  EXPECT_EQ(args[0].name, "root");
  EXPECT_EQ(args[0].port, 0u);
}

TEST_F(GraphNodeNamedTest, AddNodeWithMultipleDependencies) {
  graph.add<dummy_node>("input1", 1, "input1").depends();
  graph.add<dummy_node>("input2", 2, "input2").depends();
  graph.add<dummy_node>("processor", 3, "processor").depends("input1", "input2.3");

  auto pred = graph.pred_of("processor");
  EXPECT_EQ(pred.size(), 2u);
  EXPECT_TRUE(pred.contains("input1"));
  EXPECT_TRUE(pred.contains("input2"));

  auto args = graph.args_of("processor");
  EXPECT_EQ(args.size(), 2u);
  EXPECT_EQ(args[0].name, "input1");
  EXPECT_EQ(args[0].port, 0u);
  EXPECT_EQ(args[1].name, "input2");
  EXPECT_EQ(args[1].port, 3u);
}

TEST_F(GraphNodeNamedTest, AddNodeWithRangeDependencies) {
  graph.add<dummy_node>("input1", 1, "input1").depends();
  graph.add<dummy_node>("input2", 2, "input2").depends();

  std::vector<std::string> deps = {"input1.0", "input2.5"};
  graph.add<dummy_node>("processor", 3, "processor").depends(deps);

  auto pred = graph.pred_of("processor");
  EXPECT_EQ(pred.size(), 2u);
  EXPECT_TRUE(pred.contains("input1"));
  EXPECT_TRUE(pred.contains("input2"));

  auto args = graph.args_of("processor");
  EXPECT_EQ(args.size(), 2u);
  EXPECT_EQ(args[0].name, "input1");
  EXPECT_EQ(args[0].port, 0u);
  EXPECT_EQ(args[1].name, "input2");
  EXPECT_EQ(args[1].port, 5u);
}

TEST_F(GraphNodeNamedTest, AddNodeWithEdgeTypes) {
  graph.add<dummy_node>("input1", 1, "input1").depends();
  graph.add<dummy_node>("input2", 2, "input2").depends();

  std::vector<detail::graph_named_edge<>> edge_deps = {make_edge("input1", 0), make_edge("input2", 5)};
  graph.add<dummy_node>("processor", 3, "processor").depends(edge_deps);

  auto pred = graph.pred_of("processor");
  EXPECT_EQ(pred.size(), 2u);
  EXPECT_TRUE(pred.contains("input1"));
  EXPECT_TRUE(pred.contains("input2"));

  auto args = graph.args_of("processor");
  EXPECT_EQ(args.size(), 2u);
  EXPECT_EQ(args[0].name, "input1");
  EXPECT_EQ(args[0].port, 0u);
  EXPECT_EQ(args[1].name, "input2");
  EXPECT_EQ(args[1].port, 5u);
}

TEST_F(GraphNodeNamedTest, AddNodeWithNonExistingPred) {
  graph.add<dummy_node>("processor", 3, "processor").depends("nonexistent");
  EXPECT_TRUE(graph.contains("processor"));
  EXPECT_FALSE(graph.validate());

  auto preds = graph.pred_of("processor");
  EXPECT_EQ(preds.size(), 1u);
  EXPECT_TRUE(preds.contains("nonexistent"));

  graph.add<dummy_node>("nonexistent", 0, "nonexistent").depends();
  EXPECT_TRUE(graph.validate());
}

// Root node operations
TEST_F(GraphNodeNamedTest, AddRootNode) {
  graph.root<root_node>("root", 5).alias("port0", "port1", "port2");

  EXPECT_TRUE(graph.contains("root"));
  EXPECT_TRUE(graph.is_root("root"));

  auto node = graph.node("root");
  ASSERT_NE(node, nullptr);
  auto root_ptr = std::dynamic_pointer_cast<root_node>(node);
  ASSERT_NE(root_ptr, nullptr);
  EXPECT_EQ(root_ptr->input_size, 5u);
}

TEST_F(GraphNodeNamedTest, RootNodeWithNamedPorts) {
  graph.root<root_node>("root", 3).alias("input_a", "input_b", "input_c");
  graph.add<dummy_node>("node_a", 1, "node_a").depends("input_a");
  graph.add<dummy_node>("node_b", 2, "node_b").depends("input_b");
  graph.add<dummy_node>("node_c", 3, "node_c").depends("input_c");

  // Verify that named ports resolve to root node
  auto args_a = graph.args_of("node_a");
  EXPECT_EQ(args_a.size(), 1u);
  EXPECT_EQ(args_a[0].name, "root");
  EXPECT_EQ(args_a[0].port, 0u);

  auto args_b = graph.args_of("node_b");
  EXPECT_EQ(args_b.size(), 1u);
  EXPECT_EQ(args_b[0].name, "root");
  EXPECT_EQ(args_b[0].port, 1u);

  auto args_c = graph.args_of("node_c");
  EXPECT_EQ(args_c.size(), 1u);
  EXPECT_EQ(args_c[0].name, "root");
  EXPECT_EQ(args_c[0].port, 2u);
}

// Output operations
TEST_F(GraphNodeNamedTest, SetOutput) {
  graph.add<dummy_node>("node1", 1, "node1");
  graph.add<dummy_node>("node2", 2, "node2");

  std::vector<std::string> outputs = {"node1", "node2"};
  graph.add_output(outputs);

  auto output = graph.output();
  EXPECT_EQ(output.size(), 2u);
  EXPECT_EQ(output[0].name, "node1");
  EXPECT_EQ(output[1].name, "node2");
}

TEST_F(GraphNodeNamedTest, AddOutput) {
  graph.add<dummy_node>("node1", 1, "node1");
  graph.add<dummy_node>("node2", 2, "node2");
  graph.add<dummy_node>("node3", 3, "node3");

  graph.add_output("node1");
  std::vector<std::string> more_outputs = {"node2", "node3"};
  graph.add_output(more_outputs);

  auto output = graph.output();
  EXPECT_EQ(output.size(), 3u);
  EXPECT_EQ(output[0].name, "node1");
  EXPECT_EQ(output[1].name, "node2");
  EXPECT_EQ(output[2].name, "node3");
}

// Graph utilities
TEST_F(GraphNodeNamedTest, FindRootsAndLeaves) {
  graph.add<dummy_node>("root1", 1, "root1").depends();
  graph.add<dummy_node>("root2", 2, "root2").depends();
  graph.add<dummy_node>("middle", 3, "middle").depends("root1", "root2");
  graph.add<dummy_node>("leaf1", 4, "leaf1").depends("middle");
  graph.add<dummy_node>("leaf2", 5, "leaf2").depends("middle");

  auto roots = graph.roots();
  EXPECT_EQ(roots.size(), 2u);
  EXPECT_TRUE(std::find(roots.begin(), roots.end(), "root1") != roots.end());
  EXPECT_TRUE(std::find(roots.begin(), roots.end(), "root2") != roots.end());

  auto leaves = graph.leaves();
  EXPECT_EQ(leaves.size(), 2u);
  EXPECT_TRUE(std::find(leaves.begin(), leaves.end(), "leaf1") != leaves.end());
  EXPECT_TRUE(std::find(leaves.begin(), leaves.end(), "leaf2") != leaves.end());

  EXPECT_TRUE(graph.is_root("root1"));
  EXPECT_TRUE(graph.is_root("root2"));
  EXPECT_FALSE(graph.is_root("middle"));

  EXPECT_TRUE(graph.is_leaf("leaf1"));
  EXPECT_TRUE(graph.is_leaf("leaf2"));
  EXPECT_FALSE(graph.is_leaf("middle"));
}

TEST_F(GraphNodeNamedTest, Clear) {
  graph.add<dummy_node>("node1", 1, "node1").depends();
  graph.add<dummy_node>("node2", 2, "node2").depends("node1");
  graph.add_output("node2");

  EXPECT_FALSE(graph.empty());

  graph.clear();

  EXPECT_TRUE(graph.empty());
  EXPECT_EQ(graph.size(), 0u);
  EXPECT_EQ(graph.output().size(), 0u);
}

// Template node testing
TEST_F(GraphNodeNamedTest, TemplateNodes) {
  graph_named<base_node, int> template_graph;

  template_graph.add<template_node>("int_node", 42).depends();

  auto node = template_graph.node("int_node");
  ASSERT_NE(node, nullptr);
  auto template_ptr = std::dynamic_pointer_cast<template_node<int>>(node);
  ASSERT_NE(template_ptr, nullptr);
  EXPECT_EQ(template_ptr->value, 42);
}

// Complex graph structure test based on the diagram in the header file
TEST_F(GraphNodeNamedTest, ComplexGraphStructure) {
  /*
  graph TD
      Root --> A[Node A]
      Root --> B[Node B]
      Root --> C[Node C]

      A --> D[Node D]
      A --> E[Node E]
      B --> F[Node F]
      C --> G[Node G]
      D --> H[Node H]

      %% Multiple nodes connecting to output
      E --> Output[Output]
      F --> Output
      G --> Output
      H --> Output

      %% Auxiliary node directly connected to Root
      Root --> Aux[Aux Node]
      Aux --> AuxOutput[Aux Output<br/>Clock/Logger/etc]

      %% Supplementary root forming star pattern
      SuppRoot[Supp Root<br/>Params/Signals/etc] --> A
      SuppRoot --> D
      SuppRoot --> F
      SuppRoot --> G
  */

  // Build the graph
  graph.root<root_node>("Root", 3)
      .alias() // unnamed ports

      .add<dummy_node>("A", 1, "A")
      .depends("Root.0")
      .add<dummy_node>("B", 2, "B")
      .depends("Root.1")
      .add<dummy_node>("C", 3, "C")
      .depends("Root.2")

      .add<dummy_node>("D", 4, "D")
      .depends("A")
      .add<dummy_node>("E", 5, "E")
      .depends("A")
      .add<dummy_node>("F", 6, "F")
      .depends("B")
      .add<dummy_node>("G", 7, "G")
      .depends("C")
      .add<dummy_node>("H", 8, "H")
      .depends("D")

      .add_output("E", "F", "G", "H");

  // Verify the structure
  EXPECT_EQ(graph.size(), 9u); // Root + A, B, C, D, E, F, G, H

  // Verify Root node
  EXPECT_TRUE(graph.is_root("Root"));
  auto root_succs = graph.succ_of("Root");
  EXPECT_EQ(root_succs.size(), 3u);
  EXPECT_TRUE(root_succs.contains("A"));
  EXPECT_TRUE(root_succs.contains("B"));
  EXPECT_TRUE(root_succs.contains("C"));

  // Verify A's dependencies and successors
  auto a_preds = graph.pred_of("A");
  EXPECT_EQ(a_preds.size(), 1u);
  EXPECT_TRUE(a_preds.contains("Root"));
  auto a_succs = graph.succ_of("A");
  EXPECT_EQ(a_succs.size(), 2u);
  EXPECT_TRUE(a_succs.contains("D"));
  EXPECT_TRUE(a_succs.contains("E"));

  // Verify B's dependencies and successors
  auto b_preds = graph.pred_of("B");
  EXPECT_EQ(b_preds.size(), 1u);
  EXPECT_TRUE(b_preds.contains("Root"));
  auto b_succs = graph.succ_of("B");
  EXPECT_EQ(b_succs.size(), 1u);
  EXPECT_TRUE(b_succs.contains("F"));

  // Verify C's dependencies and successors
  auto c_preds = graph.pred_of("C");
  EXPECT_EQ(c_preds.size(), 1u);
  EXPECT_TRUE(c_preds.contains("Root"));
  auto c_succs = graph.succ_of("C");
  EXPECT_EQ(c_succs.size(), 1u);
  EXPECT_TRUE(c_succs.contains("G"));

  // Verify D's chain
  auto d_preds = graph.pred_of("D");
  EXPECT_EQ(d_preds.size(), 1u);
  EXPECT_TRUE(d_preds.contains("A"));
  auto d_succs = graph.succ_of("D");
  EXPECT_EQ(d_succs.size(), 1u);
  EXPECT_TRUE(d_succs.contains("H"));

  // Verify output nodes are leaves
  EXPECT_TRUE(graph.is_leaf("E"));
  EXPECT_TRUE(graph.is_leaf("F"));
  EXPECT_TRUE(graph.is_leaf("G"));
  EXPECT_TRUE(graph.is_leaf("H"));

  // Verify outputs
  auto outputs = graph.output();
  EXPECT_EQ(outputs.size(), 4u);
  EXPECT_EQ(outputs[0].name, "E");
  EXPECT_EQ(outputs[1].name, "F");
  EXPECT_EQ(outputs[2].name, "G");
  EXPECT_EQ(outputs[3].name, "H");
}

// Test supp_root functionality
TEST_F(GraphNodeNamedTest, SuppRootStructure) {
  // Build a simple graph with supplementary root
  graph.root<root_node>("Root", 2).alias();
  graph.supp_root<root_node>("SuppRoot", 4).alias("param0", "param1", "param2", "param3");

  graph.add<dummy_node>("A", 1, "A").depends("Root.0");
  graph.add<dummy_node>("B", 2, "B").depends("Root.1");
  graph.add<dummy_node>("C", 3, "C").depends("A");

  // Link nodes to supplementary root ports
  graph.supp_link("A", "param0", "param1");
  graph.supp_link("B", "param2");
  graph.supp_link("C", "param3");

  // Verify supp_root exists
  auto supp = graph.supp_root();
  ASSERT_NE(supp, nullptr);

  // Verify supp_link mappings
  auto a_supp = graph.supp_link_of("A");
  EXPECT_EQ(a_supp.size(), 2u);
  EXPECT_EQ(a_supp[0], 0u);
  EXPECT_EQ(a_supp[1], 1u);

  auto b_supp = graph.supp_link_of("B");
  EXPECT_EQ(b_supp.size(), 1u);
  EXPECT_EQ(b_supp[0], 2u);

  auto c_supp = graph.supp_link_of("C");
  EXPECT_EQ(c_supp.size(), 1u);
  EXPECT_EQ(c_supp[0], 3u);

  // Verify non-linked node returns empty
  auto empty_supp = graph.supp_link_of("NonExistent");
  EXPECT_TRUE(empty_supp.empty());
}

// Test auxiliary functionality
TEST_F(GraphNodeNamedTest, AuxiliaryNode) {
  // Build a graph with auxiliary
  graph_with_aux.root<root_node>("Root", 2).alias("input0", "input1");
  graph_with_aux.add<dummy_node>("A", 1, "A").depends("input0");
  graph_with_aux.add<dummy_node>("B", 2, "B").depends("input1");

  // Add auxiliary that depends on Root
  graph_with_aux.aux<aux_node>("clock_config", "clock_config").depends("input0");

  // Verify auxiliary exists
  auto aux = std::dynamic_pointer_cast<aux_node>(graph_with_aux.aux());
  ASSERT_NE(aux, nullptr);
  EXPECT_EQ(aux->config, "clock_config");

  // Verify auxiliary args
  auto aux_args = graph_with_aux.aux_args();
  EXPECT_EQ(aux_args.size(), 1u);
  EXPECT_EQ(aux_args[0], 0u);
}

// Edge cases and error handling
TEST_F(GraphNodeNamedTest, GetNonexistentNode) {
  auto node = graph.node("nonexistent");
  EXPECT_EQ(node, nullptr);
}

TEST_F(GraphNodeNamedTest, GetEmptyPredecessorSet) {
  auto pred = graph.pred_of("nonexistent");
  EXPECT_EQ(pred.size(), 0u);
}

TEST_F(GraphNodeNamedTest, GetEmptySuccessorSet) {
  auto succ = graph.succ_of("nonexistent");
  EXPECT_EQ(succ.size(), 0u);
}

TEST_F(GraphNodeNamedTest, GetEmptyArgsList) {
  auto args = graph.args_of("nonexistent");
  EXPECT_EQ(args.size(), 0u);
}

// Test generic nature of graph_node with primitive types
TEST_F(GraphNodeNamedTest, GenericWithPrimitiveTypes) {
  // Test that graph_node can work with primitive types like int
  graph_int.add<int>("value1", 42).depends();
  graph_int.add<int>("value2", 100).depends();
  graph_int.add<int>("sum", 142).depends("value1", "value2");

  EXPECT_EQ(graph_int.size(), 3u);
  EXPECT_TRUE(graph_int.contains("value1"));
  EXPECT_TRUE(graph_int.contains("value2"));
  EXPECT_TRUE(graph_int.contains("sum"));

  // Check that sum depends on both values
  auto pred = graph_int.pred_of("sum");
  EXPECT_EQ(pred.size(), 2u);
  EXPECT_TRUE(pred.contains("value1"));
  EXPECT_TRUE(pred.contains("value2"));

  // Check the stored values
  auto value1_node = graph_int.node("value1");
  auto value2_node = graph_int.node("value2");
  auto sum_node = graph_int.node("sum");

  ASSERT_NE(value1_node, nullptr);
  ASSERT_NE(value2_node, nullptr);
  ASSERT_NE(sum_node, nullptr);

  EXPECT_EQ(*value1_node, 42);
  EXPECT_EQ(*value2_node, 100);
  EXPECT_EQ(*sum_node, 142);

  // Test graph operations work with primitive types
  auto roots = graph_int.roots();
  EXPECT_EQ(roots.size(), 2u);
  EXPECT_TRUE(std::find(roots.begin(), roots.end(), "value1") != roots.end());
  EXPECT_TRUE(std::find(roots.begin(), roots.end(), "value2") != roots.end());

  auto leaves = graph_int.leaves();
  EXPECT_EQ(leaves.size(), 1u);
  EXPECT_TRUE(std::find(leaves.begin(), leaves.end(), "sum") != leaves.end());

  EXPECT_TRUE(graph_int.is_root("value1"));
  EXPECT_TRUE(graph_int.is_root("value2"));
  EXPECT_TRUE(graph_int.is_leaf("sum"));
  EXPECT_FALSE(graph_int.is_root("sum"));
}

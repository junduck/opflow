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

class GraphNodeNamedTest : public ::testing::Test {
protected:
  void SetUp() override {
    graph.clear();
    graph_int.clear();
  }

  graph_named<base_node> graph;
  graph_named<int> graph_int;
};

// Test graph_named_edge parsing
TEST(GraphEdgeTest, DefaultPort) {
  detail::graph_named_edge edge("node");
  EXPECT_EQ(edge.name, "node");
  EXPECT_EQ(edge.port, 0u);
  EXPECT_EQ(std::string(edge), "node");

  detail::graph_named_edge edge2("node.abc");
  EXPECT_EQ(edge2.name, "node.abc");
  EXPECT_EQ(edge2.port, 0u);
  EXPECT_EQ(std::string(edge2), "node.abc");
}

TEST(GraphEdgeTest, ExplicitPort) {
  detail::graph_named_edge edge("node.5");
  EXPECT_EQ(edge.name, "node");
  EXPECT_EQ(edge.port, 5u);
  EXPECT_EQ(std::string(edge), "node.5");
}

TEST(GraphEdgeTest, PortOutOfRange) {
  EXPECT_THROW(detail::graph_named_edge("node.999999999999999999999"), std::out_of_range);
}

TEST(GraphEdgeTest, ConstructorWithNameAndPort) {
  detail::graph_named_edge edge("test_node", 42);
  EXPECT_EQ(edge.name, "test_node");
  EXPECT_EQ(edge.port, 42u);
  EXPECT_EQ(std::string(edge), "test_node.42");

  detail::graph_named_edge edge2("test.node.42");
  EXPECT_EQ(edge2.name, "test.node");
  EXPECT_EQ(edge2.port, 42u);
  EXPECT_EQ(std::string(edge2), "test.node.42");
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
  graph.add<dummy_node>("node1", 1, "test");

  EXPECT_FALSE(graph.empty());
  EXPECT_EQ(graph.size(), 1u);
  EXPECT_TRUE(graph.contains("node1"));

  auto node = graph.get_node("node1");
  ASSERT_NE(node, nullptr);
  auto dummy_ptr = std::dynamic_pointer_cast<dummy_node>(node);
  ASSERT_NE(dummy_ptr, nullptr);
  EXPECT_EQ(dummy_ptr->id, 1);
  EXPECT_EQ(dummy_ptr->name, "test");
}

TEST_F(GraphNodeNamedTest, AddMultipleNodes) {
  graph.add<dummy_node>("node1", 1, "test1");
  graph.add<dummy_node>("node2", 2, "test2");
  graph.add<dummy_node>("node3", 3, "test3");

  EXPECT_EQ(graph.size(), 3u);
  EXPECT_TRUE(graph.contains("node1"));
  EXPECT_TRUE(graph.contains("node2"));
  EXPECT_TRUE(graph.contains("node3"));
}

TEST_F(GraphNodeNamedTest, AddNodeWithDependencies) {
  graph.add<dummy_node>("root", 0, "root");
  graph.add<dummy_node>("child", "root", 1, "child");

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
  graph.add<dummy_node>("input1", 1, "input1");
  graph.add<dummy_node>("input2", 2, "input2");
  graph.add<dummy_node>("processor", "input1", "input2.3", 3, "processor");

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
  graph.add<dummy_node>("input1", 1, "input1");
  graph.add<dummy_node>("input2", 2, "input2");

  std::vector<std::string> deps = {"input1.0", "input2.5"};
  graph.add<dummy_node>("processor", deps, 3, "processor");

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

TEST_F(GraphNodeNamedTest, AddNodeWithCtorArgsTag) {
  graph.add<dummy_node>("input", 1, "input");
  graph.add<dummy_node2>("processor", "input", ctor_args, "test_name", 42);

  auto node = graph.get_node("processor");
  ASSERT_NE(node, nullptr);
  auto dummy2_ptr = std::dynamic_pointer_cast<dummy_node2>(node);
  ASSERT_NE(dummy2_ptr, nullptr);
  EXPECT_EQ(dummy2_ptr->name, "test_name");
  EXPECT_EQ(dummy2_ptr->id, 42);

  auto pred = graph.pred_of("processor");
  EXPECT_EQ(pred.size(), 1u);
  EXPECT_TRUE(pred.contains("input"));
}

TEST_F(GraphNodeNamedTest, AddNodeWithNonExistingPred) {
  graph.add<dummy_node>("processor", "nonexistent", 3, "processor");
  EXPECT_TRUE(graph.contains("processor"));
  EXPECT_FALSE(graph.validate());

  auto preds = graph.pred_of("processor");
  EXPECT_EQ(preds.size(), 1u);
  EXPECT_TRUE(preds.contains("nonexistent"));

  graph.add<dummy_node>("nonexistent", 0, "nonexistent");
  EXPECT_TRUE(graph.validate());
}

// Root node operations
TEST_F(GraphNodeNamedTest, AddRootNode) {
  graph.root<root_node>("root", 5);

  EXPECT_TRUE(graph.contains("root"));
  EXPECT_TRUE(graph.is_root("root"));

  auto node = graph.get_node("root");
  ASSERT_NE(node, nullptr);
  auto root_ptr = std::dynamic_pointer_cast<root_node>(node);
  ASSERT_NE(root_ptr, nullptr);
  EXPECT_EQ(root_ptr->input_size, 5u);
}

// Output operations
TEST_F(GraphNodeNamedTest, SetOutput) {
  graph.add<dummy_node>("node1", 1, "node1");
  graph.add<dummy_node>("node2", 2, "node2");

  std::vector<std::string> outputs = {"node1", "node2"};
  graph.set_output(outputs);

  auto output = graph.get_output();
  EXPECT_EQ(output.size(), 2u);
  EXPECT_EQ(output[0], "node1");
  EXPECT_EQ(output[1], "node2");
}

TEST_F(GraphNodeNamedTest, AddOutput) {
  graph.add<dummy_node>("node1", 1, "node1");
  graph.add<dummy_node>("node2", 2, "node2");
  graph.add<dummy_node>("node3", 3, "node3");

  graph.add_output("node1");
  std::vector<std::string> more_outputs = {"node2", "node3"};
  graph.add_output(more_outputs);

  auto output = graph.get_output();
  EXPECT_EQ(output.size(), 3u);
  EXPECT_EQ(output[0], "node1");
  EXPECT_EQ(output[1], "node2");
  EXPECT_EQ(output[2], "node3");
}

// Node removal
TEST_F(GraphNodeNamedTest, RemoveNode) {
  graph.add<dummy_node>("node1", 1, "node1");
  graph.add<dummy_node>("node2", "node1", 2, "node2");
  graph.add<dummy_node>("node3", "node2", 3, "node3");

  EXPECT_EQ(graph.size(), 3u);

  graph.rm("node2");

  EXPECT_EQ(graph.size(), 2u);
  EXPECT_TRUE(graph.contains("node1"));
  EXPECT_FALSE(graph.contains("node2"));
  EXPECT_TRUE(graph.contains("node3"));

  // node3 should no longer have node2 as predecessor
  auto pred = graph.pred_of("node3");
  EXPECT_EQ(pred.size(), 0u);

  // node1 should no longer have node2 as successor
  auto succ = graph.succ_of("node1");
  EXPECT_EQ(succ.size(), 0u);
}

TEST_F(GraphNodeNamedTest, RemoveNonexistentNode) {
  graph.add<dummy_node>("node1", 1, "node1");
  EXPECT_EQ(graph.size(), 1u);

  graph.rm("nonexistent");
  EXPECT_EQ(graph.size(), 1u);
}

// Edge manipulation
TEST_F(GraphNodeNamedTest, AddEdge) {
  graph.add<dummy_node>("node1", 1, "node1");
  graph.add<dummy_node>("node2", 2, "node2");

  graph.add_edge("node2", "node1");

  auto pred = graph.pred_of("node2");
  EXPECT_EQ(pred.size(), 1u);
  EXPECT_TRUE(pred.contains("node1"));

  auto args = graph.args_of("node2");
  EXPECT_EQ(args.size(), 1u);
  EXPECT_EQ(args[0].name, "node1");
  EXPECT_EQ(args[0].port, 0u);
}

TEST_F(GraphNodeNamedTest, AddMultipleEdges) {
  graph.add<dummy_node>("node1", 1, "node1");
  graph.add<dummy_node>("node2", 2, "node2");
  graph.add<dummy_node>("node3", 3, "node3");

  std::vector<detail::graph_named_edge> edges = {{"node1", 0}, {"node2", 5}};
  graph.add_edge("node3", edges);

  auto pred = graph.pred_of("node3");
  EXPECT_EQ(pred.size(), 2u);
  EXPECT_TRUE(pred.contains("node1"));
  EXPECT_TRUE(pred.contains("node2"));

  auto args = graph.args_of("node3");
  EXPECT_EQ(args.size(), 2u);
  EXPECT_EQ(args[0].name, "node1");
  EXPECT_EQ(args[0].port, 0u);
  EXPECT_EQ(args[1].name, "node2");
  EXPECT_EQ(args[1].port, 5u);
}

TEST_F(GraphNodeNamedTest, RemoveEdge) {
  graph.add<dummy_node>("node1", 1, "node1");
  graph.add<dummy_node>("node2", "node1", 2, "node2");

  EXPECT_EQ(graph.pred_of("node2").size(), 1u);

  graph.rm_edge("node2", detail::graph_named_edge("node1", 0));

  auto pred = graph.pred_of("node2");
  EXPECT_EQ(pred.size(), 0u);

  auto args = graph.args_of("node2");
  EXPECT_EQ(args.size(), 0u);

  auto succ = graph.succ_of("node1");
  EXPECT_EQ(succ.size(), 0u);
}

TEST_F(GraphNodeNamedTest, RemoveNonexistentEdge) {
  graph.add<dummy_node>("node1", 1, "node1");
  graph.add<dummy_node>("node2", 2, "node2");

  // Should not crash or affect existing state
  graph.rm_edge("node2", detail::graph_named_edge("node1", 0));
  EXPECT_EQ(graph.pred_of("node2").size(), 0u);
}

// Rename and replace operations
TEST_F(GraphNodeNamedTest, RenameNode) {
  graph.add<dummy_node>("node1", 1, "node1");
  graph.add<dummy_node>("node2", "node1", 2, "node2");
  graph.add_output("node2");

  graph.rename("node1", "renamed_node");

  EXPECT_FALSE(graph.contains("node1"));
  EXPECT_TRUE(graph.contains("renamed_node"));
  EXPECT_TRUE(graph.contains("node2"));

  // Check that adjacency is preserved
  auto pred = graph.pred_of("node2");
  EXPECT_EQ(pred.size(), 1u);
  EXPECT_TRUE(pred.contains("renamed_node"));

  auto succ = graph.succ_of("renamed_node");
  EXPECT_EQ(succ.size(), 1u);
  EXPECT_TRUE(succ.contains("node2"));

  // Check that args are updated
  auto args = graph.args_of("node2");
  EXPECT_EQ(args.size(), 1u);
  EXPECT_EQ(args[0].name, "renamed_node");
}

TEST_F(GraphNodeNamedTest, RenameNonexistentNode) {
  graph.add<dummy_node>("node1", 1, "node1");

  graph.rename("nonexistent", "new_name");

  EXPECT_TRUE(graph.contains("node1"));
  EXPECT_FALSE(graph.contains("new_name"));
}

TEST_F(GraphNodeNamedTest, RenameToExistingNode) {
  graph.add<dummy_node>("node1", 1, "node1");
  graph.add<dummy_node>("node2", 2, "node2");

  graph.rename("node1", "node2");

  // Should not perform rename
  EXPECT_TRUE(graph.contains("node1"));
  EXPECT_TRUE(graph.contains("node2"));
}

TEST_F(GraphNodeNamedTest, ReplaceNode) {
  graph.add<dummy_node>("old_node", 1, "old");
  graph.add<dummy_node>("dependent", "old_node", 2, "dependent");

  graph.replace<dummy_node>("old_node", "new_node", 99, "new");

  EXPECT_FALSE(graph.contains("old_node"));
  EXPECT_TRUE(graph.contains("new_node"));

  auto node = graph.get_node("new_node");
  ASSERT_NE(node, nullptr);
  auto dummy_ptr = std::dynamic_pointer_cast<dummy_node>(node);
  ASSERT_NE(dummy_ptr, nullptr);
  EXPECT_EQ(dummy_ptr->id, 99);
  EXPECT_EQ(dummy_ptr->name, "new");

  // Check adjacency is preserved
  auto pred = graph.pred_of("dependent");
  EXPECT_EQ(pred.size(), 1u);
  EXPECT_TRUE(pred.contains("new_node"));
}

TEST_F(GraphNodeNamedTest, ReplaceEdge) {
  graph.add<dummy_node>("old_pred", 1, "old_pred");
  graph.add<dummy_node>("new_pred", 2, "new_pred");
  graph.add<dummy_node>("node", "old_pred.5", 3, "node");

  graph.replace("node", detail::graph_named_edge("old_pred", 5), detail::graph_named_edge("new_pred", 7));

  auto pred = graph.pred_of("node");
  EXPECT_EQ(pred.size(), 1u);
  EXPECT_TRUE(pred.contains("new_pred"));
  EXPECT_FALSE(pred.contains("old_pred"));

  auto args = graph.args_of("node");
  EXPECT_EQ(args.size(), 1u);
  EXPECT_EQ(args[0].name, "new_pred");
  EXPECT_EQ(args[0].port, 7u);
}

// Graph utilities
TEST_F(GraphNodeNamedTest, FindRootsAndLeaves) {
  graph.add<dummy_node>("root1", 1, "root1");
  graph.add<dummy_node>("root2", 2, "root2");
  graph.add<dummy_node>("middle", "root1", "root2", 3, "middle");
  graph.add<dummy_node>("leaf1", "middle", 4, "leaf1");
  graph.add<dummy_node>("leaf2", "middle", 5, "leaf2");

  auto roots = graph.get_roots();
  EXPECT_EQ(roots.size(), 2u);
  EXPECT_TRUE(std::find(roots.begin(), roots.end(), "root1") != roots.end());
  EXPECT_TRUE(std::find(roots.begin(), roots.end(), "root2") != roots.end());

  auto leaves = graph.get_leaves();
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
  graph.add<dummy_node>("node1", 1, "node1");
  graph.add<dummy_node>("node2", "node1", 2, "node2");
  graph.add_output("node2");

  EXPECT_FALSE(graph.empty());

  graph.clear();

  EXPECT_TRUE(graph.empty());
  EXPECT_EQ(graph.size(), 0u);
  EXPECT_EQ(graph.get_output().size(), 0u);
}

// Graph merging
TEST_F(GraphNodeNamedTest, MergeGraphs) {
  // Setup first graph
  graph.add<dummy_node>("node1", 1, "node1");
  graph.add<dummy_node>("node2", "node1", 2, "node2");

  // Setup second graph
  graph_named<base_node> other_graph;
  other_graph.add<dummy_node>("node3", 3, "node3");
  other_graph.add<dummy_node>("node4", "node3", 4, "node4");

  graph.merge(other_graph);

  EXPECT_EQ(graph.size(), 4u);
  EXPECT_TRUE(graph.contains("node1"));
  EXPECT_TRUE(graph.contains("node2"));
  EXPECT_TRUE(graph.contains("node3"));
  EXPECT_TRUE(graph.contains("node4"));

  // Check that dependencies are preserved
  auto pred2 = graph.pred_of("node2");
  EXPECT_TRUE(pred2.contains("node1"));

  auto pred4 = graph.pred_of("node4");
  EXPECT_TRUE(pred4.contains("node3"));
}

TEST_F(GraphNodeNamedTest, MergeWithOverlap) {
  // Setup first graph
  graph.add<dummy_node>("shared", 1, "original");
  graph.add<dummy_node>("node1", "shared", 2, "node1");

  // Setup second graph with same node name
  graph_named<base_node> other_graph;
  other_graph.add<dummy_node>("shared", 99, "different");
  other_graph.add<dummy_node>("node2", "shared", 3, "node2");

  graph.merge(other_graph);

  EXPECT_EQ(graph.size(), 3u);

  // Original node should be preserved
  auto shared_node = graph.get_node("shared");
  ASSERT_NE(shared_node, nullptr);
  auto dummy_ptr = std::dynamic_pointer_cast<dummy_node>(shared_node);
  ASSERT_NE(dummy_ptr, nullptr);
  EXPECT_EQ(dummy_ptr->name, "original");
  EXPECT_EQ(dummy_ptr->id, 1);

  // But new dependencies should be added
  EXPECT_TRUE(graph.contains("node2"));
  auto pred2 = graph.pred_of("node2");
  EXPECT_TRUE(pred2.contains("shared"));
}

TEST_F(GraphNodeNamedTest, GraphAdditionOperator) {
  // Setup first graph
  graph.add<dummy_node>("node1", 1, "node1");

  // Setup second graph
  graph_named<base_node> other_graph;
  other_graph.add<dummy_node>("node2", 2, "node2");

  auto combined = graph + other_graph;

  EXPECT_EQ(combined.size(), 2u);
  EXPECT_TRUE(combined.contains("node1"));
  EXPECT_TRUE(combined.contains("node2"));

  // Original graphs should be unchanged
  EXPECT_EQ(graph.size(), 1u);
  EXPECT_EQ(other_graph.size(), 1u);
}

TEST_F(GraphNodeNamedTest, GraphCompoundAssignment) {
  // Setup first graph
  graph.add<dummy_node>("node1", 1, "node1");

  // Setup second graph
  graph_named<base_node> other_graph;
  other_graph.add<dummy_node>("node2", 2, "node2");

  graph += other_graph;

  EXPECT_EQ(graph.size(), 2u);
  EXPECT_TRUE(graph.contains("node1"));
  EXPECT_TRUE(graph.contains("node2"));
}

// Template node testing
TEST_F(GraphNodeNamedTest, TemplateNodes) {
  graph_named<base_node> template_graph;

  template_graph.add<template_node<int>>("int_node", 42);

  auto node = template_graph.get_node("int_node");
  ASSERT_NE(node, nullptr);
  auto template_ptr = std::dynamic_pointer_cast<template_node<int>>(node);
  ASSERT_NE(template_ptr, nullptr);
  EXPECT_EQ(template_ptr->value, 42);
}

// Edge cases and error handling
TEST_F(GraphNodeNamedTest, GetNonexistentNode) {
  auto node = graph.get_node("nonexistent");
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
  graph_int.add<int>("value1", 42);
  graph_int.add<int>("value2", 100);
  graph_int.add<int>("sum", "value1", "value2", 142);

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
  auto value1_node = graph_int.get_node("value1");
  auto value2_node = graph_int.get_node("value2");
  auto sum_node = graph_int.get_node("sum");

  ASSERT_NE(value1_node, nullptr);
  ASSERT_NE(value2_node, nullptr);
  ASSERT_NE(sum_node, nullptr);

  EXPECT_EQ(*value1_node, 42);
  EXPECT_EQ(*value2_node, 100);
  EXPECT_EQ(*sum_node, 142);

  // Test graph operations work with primitive types
  auto roots = graph_int.get_roots();
  EXPECT_EQ(roots.size(), 2u);
  EXPECT_TRUE(std::find(roots.begin(), roots.end(), "value1") != roots.end());
  EXPECT_TRUE(std::find(roots.begin(), roots.end(), "value2") != roots.end());

  auto leaves = graph_int.get_leaves();
  EXPECT_EQ(leaves.size(), 1u);
  EXPECT_TRUE(std::find(leaves.begin(), leaves.end(), "sum") != leaves.end());

  EXPECT_TRUE(graph_int.is_root("value1"));
  EXPECT_TRUE(graph_int.is_root("value2"));
  EXPECT_TRUE(graph_int.is_leaf("sum"));
  EXPECT_FALSE(graph_int.is_root("sum"));
}

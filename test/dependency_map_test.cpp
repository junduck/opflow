#include <gtest/gtest.h>
#include <vector>

#include "opflow/dependency_map.hpp"
#include "opflow/topo.hpp"

using namespace opflow;

class DependencyMapTest : public ::testing::Test {
protected:
  dependency_map graph;
};

TEST_F(DependencyMapTest, EmptyGraph) {
  EXPECT_TRUE(graph.empty());
  EXPECT_EQ(graph.size(), 0);
}

TEST_F(DependencyMapTest, SingleNode) {
  auto id = graph.add(std::vector<size_t>{});
  EXPECT_EQ(id, 0);
  EXPECT_EQ(graph.size(), 1);
  EXPECT_FALSE(graph.empty());
  EXPECT_EQ(graph.num_predecessors(0), 0);
  EXPECT_TRUE(graph.is_root(0));
}

TEST_F(DependencyMapTest, LinearChain) {
  auto node0 = graph.add(std::vector<size_t>{});
  auto node1 = graph.add(std::vector<size_t>{0});
  auto node2 = graph.add(std::vector<size_t>{1});

  EXPECT_EQ(node0, 0);
  EXPECT_EQ(node1, 1);
  EXPECT_EQ(node2, 2);
  EXPECT_EQ(graph.size(), 3);

  EXPECT_EQ(graph.num_predecessors(0), 0);
  EXPECT_EQ(graph.num_predecessors(1), 1);
  EXPECT_EQ(graph.num_predecessors(2), 1);

  EXPECT_TRUE(graph.is_root(0));
  EXPECT_FALSE(graph.is_root(1));
  EXPECT_FALSE(graph.is_root(2));

  auto it = graph.get_roots();
  std::vector<size_t> roots(it.begin(), it.end());
  EXPECT_EQ(roots.size(), 1);
  EXPECT_EQ(roots[0], 0);

  for (auto r : graph.get_roots()) {
    EXPECT_EQ(r, 0);
  }
}

TEST_F(DependencyMapTest, DiamondPattern) {
  graph.add(std::vector<size_t>{});     // Root
  graph.add(std::vector<size_t>{0});    // Left branch
  graph.add(std::vector<size_t>{0});    // Right branch
  graph.add(std::vector<size_t>{1, 2}); // Merge

  EXPECT_EQ(graph.size(), 4);
  EXPECT_EQ(graph.num_predecessors(3), 2);

  auto deps = graph.get_predecessors(3);
  auto dep_vec = std::vector<size_t>(deps.begin(), deps.end());
  EXPECT_EQ(dep_vec.size(), 2);
  EXPECT_EQ(dep_vec[0], 1);
  EXPECT_EQ(dep_vec[1], 2);
}

TEST_F(DependencyMapTest, ValidationTests) {
  graph.add(std::vector<size_t>{});
  graph.add(std::vector<size_t>{0});

  // Valid dependencies (refer to existing nodes)
  EXPECT_TRUE(graph.validate(std::vector<size_t>{}));
  EXPECT_TRUE(graph.validate(std::vector<size_t>{0}));
  EXPECT_TRUE(graph.validate(std::vector<size_t>{1}));
  EXPECT_TRUE(graph.validate(std::vector<size_t>{0, 1}));

  // Invalid dependencies (refer to non-existent nodes)
  EXPECT_FALSE(graph.validate(std::vector<size_t>{2}));
  EXPECT_FALSE(graph.validate(std::vector<size_t>{0, 5}));

  // Test adding with invalid dependencies
  auto bad_id = graph.add(std::vector<size_t>{10});
  EXPECT_EQ(bad_id, static_cast<size_t>(-1));
  EXPECT_EQ(graph.size(), 2); // Size should not change
}

TEST_F(DependencyMapTest, MultipleRoots) {
  graph.add(std::vector<size_t>{});
  graph.add(std::vector<size_t>{});
  graph.add(std::vector<size_t>{});
  graph.add(std::vector<size_t>{0, 1, 2});

  auto it = graph.get_roots();
  std::vector<size_t> roots(it.begin(), it.end());
  EXPECT_EQ(roots.size(), 3);
  EXPECT_EQ(roots[0], 0);
  EXPECT_EQ(roots[1], 1);
  EXPECT_EQ(roots[2], 2);

  EXPECT_TRUE(graph.is_root(0));
  EXPECT_TRUE(graph.is_root(1));
  EXPECT_TRUE(graph.is_root(2));
  EXPECT_FALSE(graph.is_root(3));
}

TEST_F(DependencyMapTest, ReserveMemory) {
  // Should still work after reserving
  for (int i = 0; i < 10; ++i) {
    if (i == 0) {
      graph.add(std::vector<size_t>{});
    } else {
      graph.add(std::vector<size_t>{static_cast<size_t>(i - 1)});
    }
  }

  EXPECT_EQ(graph.size(), 10);
}

TEST_F(DependencyMapTest, ClearGraph) {
  graph.add(std::vector<size_t>{});
  graph.add(std::vector<size_t>{0});
  EXPECT_EQ(graph.size(), 2);

  graph.clear();
  EXPECT_TRUE(graph.empty());
  EXPECT_EQ(graph.size(), 0);

  // Should be able to add nodes after clearing
  auto id = graph.add(std::vector<size_t>{});
  EXPECT_EQ(id, 0);
}

TEST_F(DependencyMapTest, EdgeCaseValidation) {
  // Test validation with empty range
  EXPECT_TRUE(graph.validate(std::vector<size_t>{}));

  // Add a node and test various edge cases
  graph.add(std::vector<size_t>{});

  // Test with single invalid dependency
  EXPECT_FALSE(graph.validate(std::vector<size_t>{1}));

  // Test with mix of valid and invalid
  EXPECT_FALSE(graph.validate(std::vector<size_t>{0, 1}));
}

TEST_F(DependencyMapTest, ReserveFunctionality) {
  graph.reserve(10, 20);
  // Should still work after reserving
  for (int i = 0; i < 5; ++i) {
    if (i == 0) {
      graph.add(std::vector<size_t>{});
    } else {
      graph.add(std::vector<size_t>{static_cast<size_t>(i - 1)});
    }
  }
  EXPECT_EQ(graph.size(), 5);
}

TEST_F(DependencyMapTest, TotalDependencies) {
  EXPECT_EQ(graph.total_predecessors(), 0);

  graph.add(std::vector<size_t>{}); // 0 deps
  EXPECT_EQ(graph.total_predecessors(), 0);

  graph.add(std::vector<size_t>{0}); // 1 dep
  EXPECT_EQ(graph.total_predecessors(), 1);

  graph.add(std::vector<size_t>{0, 1}); // 2 deps
  EXPECT_EQ(graph.total_predecessors(), 3);
}

TEST_F(DependencyMapTest, GetDependents) {
  auto root = graph.add(std::vector<size_t>{});                     // 0
  auto child1 = graph.add(std::vector<size_t>{root});               // 1
  auto child2 = graph.add(std::vector<size_t>{root});               // 2
  auto grandchild = graph.add(std::vector<size_t>{child1, child2}); // 3

  auto root_dependents = graph.get_successors(root);
  EXPECT_EQ(root_dependents.size(), 2);
  EXPECT_TRUE(std::ranges::find(root_dependents, child1) != root_dependents.end());
  EXPECT_TRUE(std::ranges::find(root_dependents, child2) != root_dependents.end());

  auto child1_dependents = graph.get_successors(child1);
  EXPECT_EQ(child1_dependents.size(), 1);
  EXPECT_EQ(child1_dependents[0], grandchild);

  auto grandchild_dependents = graph.get_successors(grandchild);
  EXPECT_EQ(grandchild_dependents.size(), 0);
}

TEST_F(DependencyMapTest, DependsOn) {
  auto root = graph.add(std::vector<size_t>{});            // 0
  auto child = graph.add(std::vector<size_t>{root});       // 1
  auto grandchild = graph.add(std::vector<size_t>{child}); // 2
  auto other_root = graph.add(std::vector<size_t>{});      // 3

  // Direct dependency
  EXPECT_TRUE(graph.depends_on(child, root));
  EXPECT_TRUE(graph.depends_on(grandchild, child));

  // Indirect dependency
  EXPECT_TRUE(graph.depends_on(grandchild, root));

  // No dependency
  EXPECT_FALSE(graph.depends_on(root, child));
  EXPECT_FALSE(graph.depends_on(root, grandchild));
  EXPECT_FALSE(graph.depends_on(other_root, root));
  EXPECT_FALSE(graph.depends_on(root, other_root));

  // Self dependency (should be false)
  EXPECT_FALSE(graph.depends_on(root, root));
  EXPECT_FALSE(graph.depends_on(child, child));
}

TEST_F(DependencyMapTest, ContainsMethods) {
  auto id = graph.add(std::vector<size_t>{});

  EXPECT_TRUE(graph.contains(id));
  EXPECT_TRUE(graph.contains(0));
  EXPECT_FALSE(graph.contains(1));
  EXPECT_FALSE(graph.contains(100));
}

TEST_F(DependencyMapTest, TopoSort) {

  using vect = std::vector<std::string>;
  using lookup = std::unordered_map<std::string, size_t>;

  topological_sorter<std::string> sorter;
  sorter.add_vertex("A");
  sorter.add_vertex("B", vect{"A"});
  sorter.add_vertex("C", vect{"A"});
  sorter.add_vertex("D", vect{"A", "B"});
  sorter.add_vertex("E", vect{"B", "C"});
  sorter.add_vertex("F", vect{"C"});
  sorter.add_vertex("G", vect{"E", "F"});
  sorter.add_vertex("H", vect{"G"});

  auto sorted = sorter.make_sorted_graph();
  lookup id_lookup{};
  std::vector<size_t> deps_by_id;
  size_t expected_id = 0;
  for (auto const &[node, deps] : sorted) {

    deps_by_id.clear();
    for (auto const &dep : deps) {
      auto it = id_lookup.find(dep);
      if (it != id_lookup.end()) {
        deps_by_id.push_back(it->second);
      }
    }
    ASSERT_EQ(deps_by_id.size(), deps.size());

    auto id = graph.add(deps_by_id);
    ASSERT_EQ(id, expected_id);
    ++expected_id;

    id_lookup[node] = id;
  }
}

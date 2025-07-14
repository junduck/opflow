#include "opflow/dependency_map.hpp"
#include <gtest/gtest.h>
#include <vector>

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
  EXPECT_EQ(graph.get_degree(0), 0);
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

  EXPECT_EQ(graph.get_degree(0), 0);
  EXPECT_EQ(graph.get_degree(1), 1);
  EXPECT_EQ(graph.get_degree(2), 1);

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
  EXPECT_EQ(graph.get_degree(3), 2);

  auto deps = graph.get_dependencies(3);
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
  auto invalid_id = graph.add(std::vector<size_t>{10});
  EXPECT_EQ(invalid_id, static_cast<size_t>(-1));
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
  EXPECT_EQ(graph.total_dependencies(), 0);

  graph.add(std::vector<size_t>{}); // 0 deps
  EXPECT_EQ(graph.total_dependencies(), 0);

  graph.add(std::vector<size_t>{0}); // 1 dep
  EXPECT_EQ(graph.total_dependencies(), 1);

  graph.add(std::vector<size_t>{0, 1}); // 2 deps
  EXPECT_EQ(graph.total_dependencies(), 3);
}

TEST_F(DependencyMapTest, GetDependents) {
  auto root = graph.add(std::vector<size_t>{});                     // 0
  auto child1 = graph.add(std::vector<size_t>{root});               // 1
  auto child2 = graph.add(std::vector<size_t>{root});               // 2
  auto grandchild = graph.add(std::vector<size_t>{child1, child2}); // 3

  auto root_dependents = graph.get_dependents(root);
  EXPECT_EQ(root_dependents.size(), 2);
  EXPECT_TRUE(std::ranges::find(root_dependents, child1) != root_dependents.end());
  EXPECT_TRUE(std::ranges::find(root_dependents, child2) != root_dependents.end());

  auto child1_dependents = graph.get_dependents(child1);
  EXPECT_EQ(child1_dependents.size(), 1);
  EXPECT_EQ(child1_dependents[0], grandchild);

  auto grandchild_dependents = graph.get_dependents(grandchild);
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

TEST_F(DependencyMapTest, Statistics) {
  auto stats = graph.get_statistics();
  EXPECT_EQ(stats.node_count, 0);
  EXPECT_EQ(stats.total_dependencies, 0);
  EXPECT_EQ(stats.max_degree, 0);
  EXPECT_EQ(stats.avg_degree, 0.0);
  EXPECT_EQ(stats.root_count, 0);
  EXPECT_EQ(stats.leaf_count, 0);

  graph.add(std::vector<size_t>{});     // root
  graph.add(std::vector<size_t>{0});    // child with 1 dep
  graph.add(std::vector<size_t>{0, 1}); // child with 2 deps

  stats = graph.get_statistics();
  EXPECT_EQ(stats.node_count, 3);
  EXPECT_EQ(stats.total_dependencies, 3);
  EXPECT_EQ(stats.max_degree, 2);
  EXPECT_EQ(stats.avg_degree, 1.0);
  EXPECT_EQ(stats.root_count, 1);
  EXPECT_EQ(stats.leaf_count, 1); // node 2 is leaf
}

TEST_F(DependencyMapTest, ContainsMethods) {
  auto id = graph.add(std::vector<size_t>{});

  EXPECT_TRUE(graph.contains(id));
  EXPECT_TRUE(graph.contains(0));
  EXPECT_FALSE(graph.contains(1));
  EXPECT_FALSE(graph.contains(100));
}

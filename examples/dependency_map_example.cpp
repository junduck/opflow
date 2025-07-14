#include "opflow/dependency_map.hpp"
#include <iostream>
#include <vector>

int main() {
  using namespace opflow;

  // Create a dependency map for a simple computation graph
  dependency_map graph;

  std::cout << "Creating a dependency graph:\n";

  // Reserve space for efficiency (optional but recommended for large graphs)
  graph.reserve(10, 20);

  // Add root nodes (no dependencies) with names
  auto input_a = graph.add(std::vector<size_t>{});  // Input A
  auto input_b = graph.add(std::vector<size_t>{});  // Input B
  auto constant = graph.add(std::vector<size_t>{}); // Constant value

  std::cout << "Added root nodes: " << input_a << " (input_a), " << input_b << " (input_b), " << constant
            << " (constant)\n";

  // Add nodes that depend on previous ones
  auto process_a = graph.add(std::vector<size_t>{input_a});                          // Process A
  auto process_b = graph.add(std::vector<size_t>{input_b});                          // Process B
  auto combine = graph.add(std::vector<size_t>{input_a, input_b, constant});         // Combine inputs
  auto final_result = graph.add(std::vector<size_t>{process_a, process_b, combine}); // Final result

  std::cout << "Added dependent nodes: " << process_a << " (process_a), " << process_b << " (process_b), " << combine
            << " (combine), " << final_result << " (final)\n\n";

  // Demonstrate topology
  std::cout << "Graph topology (node -> dependencies):\n";
  for (size_t i = 0; i < graph.size(); ++i) {
    std::cout << "Node " << i << " depends on: [";
    auto deps = graph.get_dependencies(i);
    bool first = true;
    for (auto dep : deps) {
      if (!first)
        std::cout << ", ";
      std::cout << dep;
      first = false;
    }
    std::cout << "] (degree: " << graph.get_degree(i) << ")\n";
  }

  // Show root and leaf nodes
  std::cout << "\nRoot nodes: [";
  auto roots = graph.get_roots();
  for (size_t i = 0; i < roots.size(); ++i) {
    if (i > 0)
      std::cout << ", ";
    std::cout << roots[i];
  }
  std::cout << "]\n";

  std::cout << "Leaf nodes: [";
  auto leafs = graph.get_leafs();
  for (size_t i = 0; i < leafs.size(); ++i) {
    if (i > 0)
      std::cout << ", ";
    std::cout << leafs[i];
  }
  std::cout << "]\n";

  // Test validation
  std::cout << "\nValidation tests:\n";
  std::cout << "Can add node depending on 0,1: " << (graph.validate(std::vector<size_t>{0, 1}) ? "yes" : "no") << "\n";
  std::cout << "Can add node depending on 10: " << (graph.validate(std::vector<size_t>{10}) ? "yes" : "no") << "\n";

  // Demonstrate named access
  std::cout << "\nNamed node access:\n";
  std::cout << "Dependencies of 'final': [";
  auto final_deps = graph.get_dependencies(final_result);
  for (size_t i = 0; i < final_deps.size(); ++i) {
    if (i > 0)
      std::cout << ", ";
    std::cout << final_deps[i];
  }
  std::cout << "]\n";

  std::cout << "Degree of 'combine': " << graph.get_degree(combine) << "\n";
  std::cout << "Is 'input_a' a root? " << (graph.is_root(input_a) ? "yes" : "no") << "\n";

  // Demonstrate dependency queries
  std::cout << "\nDependency analysis:\n";
  std::cout << "Does 'final' depend on 'input_a'? " << (graph.depends_on(final_result, input_a) ? "yes" : "no") << "\n";
  std::cout << "Does 'input_a' depend on 'final'? " << (graph.depends_on(input_a, final_result) ? "yes" : "no") << "\n";

  // Show dependents
  std::cout << "\nDependents of 'input_a': [";
  auto input_a_dependents = graph.get_dependents(input_a);
  for (size_t i = 0; i < input_a_dependents.size(); ++i) {
    if (i > 0)
      std::cout << ", ";
    std::cout << input_a_dependents[i];
  }
  std::cout << "]\n";

  // Show statistics
  auto stats = graph.get_statistics();
  std::cout << "\nGraph Statistics:\n";
  std::cout << "  Nodes: " << stats.node_count << "\n";
  std::cout << "  Total dependencies: " << stats.total_dependencies << "\n";
  std::cout << "  Max degree: " << stats.max_degree << "\n";
  std::cout << "  Average degree: " << stats.avg_degree << "\n";
  std::cout << "  Root nodes: " << stats.root_count << "\n";
  std::cout << "  Leaf nodes: " << stats.leaf_count << "\n";

  return 0;
}

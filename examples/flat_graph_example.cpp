
#include <iostream>
#include <vector>

#include "opflow/flat_graph.hpp"
#include "opflow/topo.hpp"

int main() {
  using namespace opflow;
  using vec = std::vector<std::string>;
  using lookup = std::unordered_map<std::string, size_t>;

  topological_sorter<std::string> sorter;

  // func2(sum(func(b, mul(y, 2))), y)
  sorter.add_vertex("mul", vec{"y", "2"});
  sorter.add_vertex("func", vec{"b", "mul"});
  sorter.add_vertex("sum", vec{"func"});
  sorter.add_vertex("func2", vec{"sum", "y"});

  auto sorted = sorter.make_sorted_graph();
  auto sorted_vec = sorted.sorted_nodes();
  lookup id_lookup{};
  for (size_t i = 0; i < sorted.size(); ++i) {
    id_lookup[sorted_vec[i]] = i; // expected id when added to dependancy map
  }
  std::vector<size_t> deps_by_id;

  for (auto const &node : sorted_vec) {
    std::cout << "Node: " << node << ", ID: " << id_lookup[node];
    if (sorted.predecessors(node).empty()) {
      std::cout << " (no dependencies)";
    } else {
      std::cout << ", Dependencies: ";
      for (const auto &dep : sorted.predecessors(node)) {
        std::cout << dep << " ";
      }
    }
    std::cout << "\n";
  }

  flat_graph graph;
  for (auto const &[node, deps] : sorted) {
    deps_by_id.clear();
    for (auto const &dep : deps) {
      deps_by_id.push_back(id_lookup[dep]);
    }
    graph.add(deps_by_id);
  }

  return 0;
}

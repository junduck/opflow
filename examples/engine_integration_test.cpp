#include "opflow/op.hpp"
#include <iostream>
#include <vector>

int main() {
  using namespace opflow;

  // Create an engine with input size 2
  engine_int eng(2);

  // Add a rollsum operator that depends on the root input (node 0)
  auto rollsum_op = std::make_shared<rollsum<int>>(std::vector<size_t>{0, 1}, 0); // cumulative sum of indices 0,1
  auto rollsum_id = eng.add_op(rollsum_op, std::vector<size_t>{0});

  std::cout << "Engine created with " << eng.num_nodes() << " nodes\n";
  std::cout << "Root input is node 0, rollsum is node " << rollsum_id << "\n";

  // Validate the engine state
  std::cout << "Engine state is valid: " << (eng.validate_state() ? "yes" : "no") << "\n";

  // Process some data
  eng.step(1, {10.0, 20.0});
  eng.step(2, {5.0, 15.0});

  auto latest = eng.get_latest_output();
  std::cout << "Latest output size: " << latest.size() << "\n";

  // Get rollsum node output specifically
  auto rollsum_output = eng.get_node_output(rollsum_id);
  std::cout << "Rollsum output: ";
  for (auto val : rollsum_output) {
    std::cout << val << " ";
  }
  std::cout << "\n";

  return 0;
}

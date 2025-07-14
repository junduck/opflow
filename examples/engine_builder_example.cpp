#include "opflow/op.hpp"
#include <iostream>
#include <vector>

int main() {
  using namespace opflow;

  std::cout << "=== Engine Builder Example ===\n";

  // Create engine builder with input size 3
  engine_builder<int> builder(3);

  // Add operators using the builder
  auto rollsum1_op = std::make_shared<rollsum<int>>(std::vector<size_t>{0, 1}, 5); // window of 5, sum indices 0,1
  auto rollsum1_id = builder.add_op(rollsum1_op, std::vector<size_t>{0});

  auto rollsum2_op = std::make_shared<rollsum<int>>(std::vector<size_t>{2}, 0); // cumulative sum of index 2
  auto rollsum2_id = builder.add_op(rollsum2_op, std::vector<size_t>{0});

  std::cout << "Builder has " << builder.num_nodes() << " nodes\n";
  std::cout << "Total output size: " << builder.get_total_output_size() << "\n";

  // Build the actual engine with optimized memory layout
  auto engine = builder.build(128); // Initial history capacity of 128

  std::cout << "Engine built successfully!\n";
  std::cout << "Engine has " << engine.num_nodes() << " nodes\n";
  std::cout << "Engine output size: " << engine.total_output_size() << "\n";
  std::cout << "Engine state is valid: " << (engine.validate_state() ? "yes" : "no") << "\n";

  // Process some data
  std::cout << "\n=== Processing Data ===\n";

  for (int tick = 1; tick <= 10; ++tick) {
    double base = tick * 10.0;
    engine.step(tick, {base, base + 1, base + 2});

    auto rollsum1_output = engine.get_node_output(rollsum1_id);
    auto rollsum2_output = engine.get_node_output(rollsum2_id);

    std::cout << "Tick " << tick << ": rollsum1=" << rollsum1_output[0] << ", rollsum2=" << rollsum2_output[0] << "\n";
  }

  std::cout << "\n=== Memory Usage ===\n";
  std::cout << "Number of historical steps: " << engine.num_steps() << "\n";

  // Get all step ticks for debugging
  auto ticks = engine.get_step_ticks();
  std::cout << "Step ticks: ";
  for (auto tick : ticks) {
    std::cout << tick << " ";
  }
  std::cout << "\n";

  return 0;
}

#include "opflow/graph.hpp"
#include <iostream>
#include <string>
#include <vector>

using namespace opflow;

void example_basic_usage() {
  std::cout << "=== Basic Usage Example ===\n";

  // Create a simple build dependency graph
  TopologicalSorter<std::string> sorter;

  // Add nodes with their dependencies
  sorter.add("source");              // No dependencies
  sorter.add("compile", {"source"}); // Compile depends on source
  sorter.add("link", {"compile"});   // Link depends on compile
  sorter.add("test", {"link"});      // Test depends on link
  sorter.add("package", {"test"});   // Package depends on test

  // Get topological order
  auto order = sorter.static_order();

  std::cout << "Build order: ";
  for (const auto &task : order) {
    std::cout << task << " -> ";
  }
  std::cout << "done\n\n";
}

void example_diamond_dependency() {
  std::cout << "=== Diamond Dependency Example ===\n";

  TopologicalSorter<char> sorter;

  // Create diamond pattern: A -> {B, C} -> D
  sorter.add('A');
  sorter.add('B', {'A'});
  sorter.add('C', {'A'});
  sorter.add('D', {'B', 'C'});

  auto order = sorter.static_order();

  std::cout << "Execution order: ";
  for (char task : order) {
    std::cout << task << " ";
  }
  std::cout << "\n\n";
}

void example_parallel_processing() {
  std::cout << "=== Parallel Processing Example ===\n";

  TopologicalSorter<int> sorter;

  // Create a graph where some tasks can run in parallel
  sorter.add(1);            // Independent task
  sorter.add(2);            // Independent task
  sorter.add(3);            // Independent task
  sorter.add(4, {1});       // Depends on 1
  sorter.add(5, {2});       // Depends on 2
  sorter.add(6, {3});       // Depends on 3
  sorter.add(7, {4, 5, 6}); // Final task depends on all

  sorter.prepare();

  std::cout << "Processing simulation:\n";
  int round = 1;

  while (!sorter.done()) {
    auto ready = sorter.get_ready();

    std::cout << "Round " << round << " - Can process in parallel: ";
    for (int task : ready) {
      std::cout << task << " ";
    }
    std::cout << "\n";

    // Simulate processing
    sorter.mark_done(ready);
    round++;
  }
  std::cout << "\n";
}

void example_package_dependencies() {
  std::cout << "=== Package Dependencies Example ===\n";

  // Using the convenience function
  std::unordered_map<std::string, std::unordered_set<std::string>> packages = {
      {"myapp", {"database", "logging", "ui"}},
      {"database", {"config", "utils"}},
      {"logging", {"config", "utils"}},
      {"ui", {"utils"}},
      {"config", {"utils"}},
      {"utils", {}} // No dependencies
  };

  auto install_order = topological_sort(packages);

  std::cout << "Package installation order:\n";
  for (size_t i = 0; i < install_order.size(); ++i) {
    std::cout << i + 1 << ". " << install_order[i] << "\n";
  }
  std::cout << "\n";
}

void example_cycle_detection() {
  std::cout << "=== Cycle Detection Example ===\n";

  TopologicalSorter<std::string> sorter;

  // Create a cycle: A -> B -> C -> A
  sorter.add("A", {"C"});
  sorter.add("B", {"A"});
  sorter.add("C", {"B"});

  try {
    auto order = sorter.static_order();
    std::cout << "This should not be reached!\n";
  } catch (const CycleError &e) {
    std::cout << "Cycle detected: " << e.what() << "\n";
  }
  std::cout << "\n";
}

void example_custom_type() {
  std::cout << "=== Custom Type Example ===\n";

  struct Task {
    std::string name;
    int priority;

    bool operator==(const Task &other) const { return name == other.name; }
  };

  // Custom hash function for Task
  struct TaskHash {
    size_t operator()(const Task &task) const { return std::hash<std::string>{}(task.name); }
  };

  TopologicalSorter<Task, TaskHash> sorter;

  Task init{"Initialize", 1};
  Task setup{"Setup", 2};
  Task process{"Process", 3};
  Task cleanup{"Cleanup", 4};

  sorter.add(init);
  sorter.add(setup, {init});
  sorter.add(process, {setup});
  sorter.add(cleanup, {process});

  auto order = sorter.static_order();

  std::cout << "Task execution order:\n";
  for (const auto &task : order) {
    std::cout << "- " << task.name << " (priority: " << task.priority << ")\n";
  }
  std::cout << "\n";
}

void example_interactive_processing() {
  std::cout << "=== Interactive Processing Example ===\n";

  TopologicalSorter<std::string> sorter;

  // Simulate a job queue where we want to control execution
  sorter.add("download_data");
  sorter.add("validate_data", {"download_data"});
  sorter.add("preprocess", {"validate_data"});
  sorter.add("train_model", {"preprocess"});
  sorter.add("validate_model", {"train_model"});
  sorter.add("deploy", {"validate_model"});

  sorter.prepare();

  std::cout << "ML Pipeline execution:\n";

  while (!sorter.done()) {
    // Get next batch of ready jobs (limit to 1 for this example)
    auto ready = sorter.get_ready(1);

    if (!ready.empty()) {
      std::string job = ready[0];
      std::cout << "Executing: " << job << "... ";

      // Simulate some processing time/user interaction
      std::cout << "completed!\n";

      // Mark as done
      sorter.mark_done({job});
    }
  }
  std::cout << "Pipeline finished!\n\n";
}

int main() {
  std::cout << "C++ GraphLib Examples\n";
  std::cout << "====================\n\n";

  example_basic_usage();
  example_diamond_dependency();
  example_parallel_processing();
  example_package_dependencies();
  example_cycle_detection();
  example_custom_type();
  example_interactive_processing();

  return 0;
}

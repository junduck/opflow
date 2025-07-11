# OpFlow GraphLib - C++ Topological Sorting Library

A modern C++ implementation inspired by Python's `graphlib` module, providing generic topological sorting functionality for directed acyclic graphs (DAGs).

## Overview

This library provides a templated `TopologicalSorter` class that can work with any type that supports hashing and equality comparison. It's particularly useful for:

- Build system dependency resolution
- Package installation ordering
- Task scheduling with dependencies
- Workflow management
- Any scenario requiring topological ordering

## Features

- **Generic Template Design**: Works with any hashable type (int, string, custom objects)
- **Cycle Detection**: Automatically detects and reports circular dependencies
- **Parallel Processing Support**: Identify tasks that can run concurrently
- **Interactive Processing**: Control execution flow with `prepare()`, `get_ready()`, `mark_done()`
- **Static Ordering**: Get complete topological sort in one call
- **Modern C++20**: Uses standard library containers and exception handling
- **Comprehensive Testing**: Extensive test coverage with Google Test

## API Reference

### TopologicalSorter<T, Hash, Equal>

#### Template Parameters

- `T`: The node type (must be hashable and equality comparable)
- `Hash`: Hash function object (defaults to `std::hash<T>`)
- `Equal`: Equality comparison object (defaults to `std::equal_to<T>`)

#### Core Methods

```cpp
// Construction
TopologicalSorter();
TopologicalSorter(const NodeMap& graph);

// Adding nodes and dependencies
void add(const T& node, const NodeSet& predecessors = {});

// Interactive processing
void prepare();                           // Must call before iteration
bool done() const;                        // Check if processing complete
std::vector<T> get_ready(size_t n = 0);   // Get nodes ready for processing
void mark_done(const std::vector<T>& nodes); // Mark nodes as completed

// Static ordering
std::vector<T> static_order();            // Get complete topological order

// Graph inspection
size_t size() const;                      // Number of nodes
bool empty() const;                       // Check if graph is empty
bool contains(const T& node) const;       // Check if node exists
std::vector<T> nodes() const;             // Get all nodes
std::optional<NodeSet> dependencies(const T& node) const;
std::optional<NodeSet> successors(const T& node) const;

// Utilities
void clear();                             // Clear the graph
```

#### Convenience Function

```cpp
template<typename T, typename Hash, typename Equal>
std::vector<T> topological_sort(
    const std::unordered_map<T, std::unordered_set<T, Hash, Equal>, Hash, Equal>& graph);
```

## Usage Examples

### Basic Usage

```cpp
#include "opflow/graph.hpp"
using namespace opflow;

TopologicalSorter<std::string> sorter;

// Add build dependencies
sorter.add("source");                    // No dependencies
sorter.add("compile", {"source"});       // Compile depends on source
sorter.add("link", {"compile"});         // Link depends on compile
sorter.add("test", {"link"});            // Test depends on link

auto order = sorter.static_order();
// Result: ["source", "compile", "link", "test"]
```

### Parallel Processing

```cpp
TopologicalSorter<int> sorter;
sorter.add(1);
sorter.add(2);
sorter.add(3, {1, 2});  // Task 3 depends on both 1 and 2

sorter.prepare();

while (!sorter.done()) {
    auto ready = sorter.get_ready();  // Get all ready tasks

    // Process tasks in parallel here
    for (int task : ready) {
        std::cout << "Processing task " << task << std::endl;
    }

    sorter.mark_done(ready);  // Mark as completed
}
```

### Cycle Detection

```cpp
TopologicalSorter<std::string> sorter;
sorter.add("A", {"C"});
sorter.add("B", {"A"});
sorter.add("C", {"B"});  // Creates cycle: A -> B -> C -> A

try {
    auto order = sorter.static_order();
} catch (const CycleError& e) {
    std::cout << "Cycle detected: " << e.what() << std::endl;
}
```

### Custom Types

```cpp
struct Task {
    std::string name;
    int priority;
    bool operator==(const Task& other) const { return name == other.name; }
};

struct TaskHash {
    size_t operator()(const Task& task) const {
        return std::hash<std::string>{}(task.name);
    }
};

TopologicalSorter<Task, TaskHash> sorter;
// Use as normal...
```

### Convenience Function

```cpp
std::unordered_map<std::string, std::unordered_set<std::string>> deps = {
    {"app", {"database", "logging"}},
    {"database", {"config"}},
    {"logging", {"config"}},
    {"config", {}}
};

auto order = topological_sort(deps);
// Result: ["config", "database", "logging", "app"] (or valid permutation)
```

## Error Handling

The library throws `CycleError` (derived from `std::runtime_error`) when:

- A cycle is detected during `static_order()` or `prepare()`
- Self-referencing dependencies are found

Other `std::runtime_error` exceptions are thrown for:

- Calling methods in wrong order (e.g., `done()` before `prepare()`)
- Calling `prepare()` multiple times
- Marking non-processing nodes as done

## Performance Characteristics

- **Time Complexity**: O(V + E) for topological sort where V = vertices, E = edges
- **Space Complexity**: O(V + E) for storing the graph
- **Cycle Detection**: O(V + E) using depth-first search

## Thread Safety

This library is **not thread-safe**. If you need to use it in a multi-threaded environment, provide external synchronization.

## Dependencies

- C++20 compatible compiler
- Standard library containers: `unordered_map`, `unordered_set`, `queue`, `vector`
- Exception handling support

## Comparison with Python's graphlib

This C++ implementation provides equivalent functionality to Python's `graphlib.TopologicalSorter`:

| Python Method | C++ Equivalent | Notes |
|---------------|----------------|-------|
| `add(node, *predecessors)` | `add(node, {predecessors})` | Takes set instead of varargs |
| `prepare()` | `prepare()` | Identical behavior |
| `is_active()` | `!done()` | Inverted logic |
| `get_ready()` | `get_ready()` | Same functionality |
| `done(*nodes)` | `mark_done(nodes)` | Takes vector instead of varargs |
| `static_order()` | `static_order()` | Identical behavior |

Additional C++ features:

- Template-based generic design
- STL container integration
- Modern C++ exception handling
- Extended query methods (`dependencies()`, `successors()`, etc.)

## Building and Testing

```bash
# Build the project
cmake --build build-vscode

# Run tests
./build-vscode/test/test_graph

# Run examples
./build-vscode/examples/graph_examples
```

## License

This project follows the same license as the parent OpFlow project.

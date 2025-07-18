# A Order Preserving Topological Sorter

## Overview

The `ordered_topo_sorter` is a specialized version of the topological sorter that preserves the exact order of dependencies as they were specified. This is crucial for scenarios like function call arguments, expression trees, or any case where the order of inputs matters.

## Key Differences from Regular `topological_sorter`

### Regular Topological Sorter

```cpp
// Dependencies are stored in unordered_set - order doesn't matter
add_vertex("func", {"arg2", "arg1"});  // Could result in func(arg1, arg2) or func(arg2, arg1)
```

### Ordered Topological Sorter

```cpp
// Dependencies are stored in vector - order is preserved
add_vertex("func", {"arg2", "arg1"});  // Always results in func(arg2, arg1)
```

## Use Cases

### 1. Function Call Graphs

```cpp
ordered_topo_sorter<std::string> call_graph;

// Expression: add(mul(x, 2), sub(y, 1))
call_graph.add_vertex("x");
call_graph.add_vertex("y");
call_graph.add_vertex("2");
call_graph.add_vertex("1");
call_graph.add_vertex("mul", {"x", "2"});      // mul(x, 2)
call_graph.add_vertex("sub", {"y", "1"});      // sub(y, 1)
call_graph.add_vertex("add", {"mul", "sub"});  // add(mul(...), sub(...))

auto sorted = call_graph.sort();
// Result preserves argument order: [x, y, 2, 1, mul, sub, add]
```

### 2. Expression Trees

```cpp
// For expression: (a + b) * (c - d)
call_graph.add_vertex("a");
call_graph.add_vertex("b");
call_graph.add_vertex("c");
call_graph.add_vertex("d");
call_graph.add_vertex("add", {"a", "b"});      // a + b (left operand first)
call_graph.add_vertex("sub", {"c", "d"});      // c - d (left operand first)
call_graph.add_vertex("mul", {"add", "sub"});  // (a+b) * (c-d)
```

### 3. Build System with Ordered Dependencies

```cpp
// Some build steps require specific order
build_graph.add_vertex("compile_main", {"header1.h", "header2.h"});  // Order matters for includes
build_graph.add_vertex("link", {"main.o", "lib1.a", "lib2.a"});      // Link order matters
```

## API Reference

### Core Methods

#### `add_vertex(node, ordered_deps)`

```cpp
template <std::ranges::forward_range R>
void add_vertex(T const &node, R &&deps);
```

Adds a node with ordered dependencies. The dependencies will be processed in the exact order specified.

#### `sort()`

```cpp
std::vector<T> sort() const;
```

Returns topologically sorted nodes while preserving dependency order.

#### `get_dependency_at(node, position)`

```cpp
T const* get_dependency_at(T const &node, size_t position) const;
```

Gets the dependency at a specific position for a node.

#### `get_predecessors_at_position(node, position)`

```cpp
std::vector<T> get_predecessors_at_position(T const &node, size_t position) const;
```

Gets all nodes that use this node as their Nth dependency.

## Implementation Details

### Data Structure

```cpp
struct EdgeSet {
    NodeSet nodes;                     // O(1) lookup for existence checks
    OrderedNodeList ordered_nodes;     // Preserves insertion order
};

struct ReverseEdge {
    T dependent;     // Node that depends on this node
    size_t position; // Position in the dependent's argument list
};
```

### Sorting Algorithm

The sorter uses a modified Kahn's algorithm that:

1. Processes nodes with zero in-degree first
2. When multiple nodes become available, processes them in dependency order
3. Uses position information to maintain ordering

### Time Complexity

- Adding a vertex: O(d) where d is number of dependencies
- Sorting: O(V + E) where V is vertices and E is edges
- Space: O(V + E) for storage

## Example Output

```cpp
ordered_topo_sorter<std::string> sorter;
sorter.add_vertex("y");
sorter.add_vertex("2");
sorter.add_vertex("some_func");
sorter.add_vertex("mul", {"y", "2"});           // Order: y before 2
sorter.add_vertex("add", {"some_func", "mul"}); // Order: some_func before mul

auto sorted = sorter.sort();
// Output: [y, some_func, 2, mul, add]
//         └─ y before 2 (preserved from mul dependencies)
//         └─ some_func before mul (preserved from add dependencies)
```

## Best Practices

1. **Use for ordered scenarios**: Function calls, expression trees, ordered build steps
2. **Combine with regular topo sorter**: Use regular sorter for general dependencies, ordered for specific ordered relationships
3. **Validate arity**: Check that function nodes have the expected number of arguments
4. **Consider duplicates**: Decide whether to allow duplicate dependencies (e.g., `f(x, x, x)`)

## Error Handling

- **Cycle detection**: Returns empty vector if cycles are detected
- **Invalid positions**: Returns nullptr for out-of-bounds position queries
- **Missing nodes**: Gracefully handles queries for non-existent nodes

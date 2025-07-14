# Dependency Map

The `dependency_map` class provides a compact, high-performance data structure for maintaining dependencies between nodes in a directed acyclic graph (DAG). It's designed specifically for scenarios where nodes must be processed in topological order.

## Design Principles

### Space Efficiency

- **Flattened storage**: All dependency IDs stored in a single contiguous vector
- **Metadata separation**: Node metadata (degree, offset) stored separately
- **No redundancy**: Each dependency stored exactly once
- **Memory complexity**: O(V + E) where V = nodes, E = total dependencies

### Topological Ordering

- **Sequential IDs**: Nodes assigned IDs 0, 1, 2, ... in topological order
- **Dependency constraint**: New nodes can only depend on previously added nodes
- **Acyclic guarantee**: The structure of sequential IDs makes cycles impossible
- **No sorting needed**: Iteration 0→N is always topologically correct

### Performance Optimization

- **Constant-time queries**: Dependencies accessed via offset/length pairs
- **Cache-friendly**: Contiguous memory layout improves locality
- **Minimal overhead**: Simple data layout with no complex bookkeeping
- **Reserve support**: Pre-allocation to avoid reallocations during construction

## API Reference

### Core Operations

```cpp
// Construction and capacity
dependency_map();                                    // Default constructor
void reserve(size_t nodes, size_t deps = 0);       // Pre-allocate capacity

// Adding nodes
size_t add(Range&& deps);                           // Add with ID dependencies

// Validation
bool validate(Range&& deps) const;                 // Check if dependencies are valid

// Basic queries
size_t size() const;                               // Number of nodes
bool empty() const;                                // Check if empty
bool contains(size_t id) const;                    // Check if node exists
```

### Dependency Queries

```cpp
// Direct dependencies
span<const size_t> get_dependencies(size_t id) const;

// Dependency metrics
size_t get_degree(size_t id) const;                // Number of dependencies
size_t total_dependencies() const;                 // Total across all nodes

// Graph analysis
vector<size_t> get_dependents(size_t id) const;   // Who depends on this node
bool depends_on(size_t a, size_t b) const;        // Does A depend on B (transitively)
```

### Graph Structure

```cpp
// Root and leaf analysis
bool is_root(size_t id) const;                    // Has no dependencies
vector<size_t> get_roots() const;                 // All root nodes
vector<size_t> get_leafs() const;                 // All leaf nodes
```

### Statistics and Analysis

```cpp
struct statistics {
    size_t node_count;
    size_t total_dependencies;
    size_t max_degree;
    double avg_degree;
    size_t root_count;
    size_t leaf_count;
};

statistics get_statistics() const;                 // Comprehensive graph metrics
```

### Memory Management

```cpp
void clear();                                      // Remove all nodes and dependencies
static constexpr size_t invalid_id;                // Return value for failed operations
```

## Usage Patterns

### Basic Dependency Tracking

```cpp
dependency_map graph;

// Build a simple pipeline: input → process → output
auto input = graph.add({});
auto process = graph.add({input});
auto output = graph.add({process});
```

### Complex DAG Construction

```cpp
dependency_map graph;
graph.reserve(1000, 5000);  // Pre-allocate for efficiency

// Multiple inputs
auto input_a = graph.add({});
auto input_b = graph.add({});
auto config = graph.add({});

// Parallel processing branches
auto transform_a = graph.add({input_a, config});
auto transform_b = graph.add({input_b, config});

// Merge results
auto combine = graph.add({transform_a, transform_b});
auto output = graph.add({combine});

// Analysis
auto stats = graph.get_statistics();
std::cout << "Graph has " << stats.node_count << " nodes, "
          << stats.avg_degree << " average dependencies\n";
```

### Dependency Analysis

```cpp
// Check complex dependencies
if (graph.depends_on(output_node, input_node)) {
    std::cout << "Output transitively depends on input\n";
}

// Find all affected nodes
auto dependents = graph.get_dependents(critical_input);
for (auto node : dependents) {
    // Mark for recomputation
}

// Validate before adding
std::vector<size_t> new_deps = {node1, node2, node3};
if (graph.validate(new_deps)) {
    auto id = graph.add(new_deps);
}
```

## Error Handling

### Invalid Dependencies

- Adding a node with dependencies on non-existent nodes returns `invalid_id`
- `validate()` can pre-check dependency validity
- Debug builds include bounds checking with assertions

### Memory Safety

- All access methods include bounds checking in debug builds
- `std::span` return types provide safe range access
- Strong exception safety guarantee for all operations

## Performance Characteristics

| Operation | Time Complexity | Notes |
|-----------|----------------|-------|
| `add()` | O(k) | k = number of dependencies |
| `get_dependencies()` | O(1) | Returns span view |
| `get_degree()` | O(1) | Direct metadata access |
| `is_root()` | O(1) | Check degree == 0 |
| `depends_on()` | O(V + E) | DFS worst case |
| `get_dependents()` | O(E) | Scan all dependencies |
| `get_roots()` | O(V) | Scan all nodes |
| `get_leafs()` | O(V + E) | Mark dependents + scan |

### Memory Layout

```
dependencies: [0, 1, 0, 2, 1, 2, 3, 4, 5] // Flattened dependency storage
meta:         [{0,0}, {1,0}, {1,1}, {2,2}, {3,4}] // {degree, offset} per node
```

For node 3 with dependencies [1,2]:

- `meta[3].degree = 2` (has 2 dependencies)
- `meta[3].offset = 2` (dependencies start at index 2)
- `dependencies[2..4] = [1,2]` (the actual dependencies)

This layout provides:

- **Cache efficiency**: Related data stored contiguously
- **Space efficiency**: No pointer overhead or fragmentation
- **Access efficiency**: Direct indexing into flattened storage

## Limitations

1. **Immutable structure**: Nodes cannot be removed once added
2. **Sequential construction**: Dependencies must refer to previously added nodes
3. **Memory overhead**: Named lookup requires additional hash map storage
4. **Large graphs**: `depends_on()` can be expensive for deep dependency chains

## Best Practices

1. **Use `reserve()`** for known graph sizes to avoid reallocations
2. **Validate early** with `validate()` before committing to add operations
3. **Prefer ID access** over name access for performance-critical code
4. **Cache dependency spans** if accessing repeatedly in loops
5. **Use statistics** to analyze graph complexity and guide optimizations

# Engine Architecture Update

This document describes the updated engine implementation that separates concerns between graph construction and execution, while leveraging high-performance components for better scalability.

## Key Changes

### 1. Separated `flat_graph` Component

- **Location**: `include/opflow/flat_graph.hpp`
- **Purpose**: Compact, topologically-sorted dependency management
- **Benefits**:
  - O(1) dependency lookup using flat arrays
  - Automatic topological sorting during construction
  - Memory-efficient storage with power-of-2 growth

### 2. High-Performance `history` Buffer

- **Location**: `include/opflow/history.hpp`
- **Purpose**: Circular buffer for step data with O(1) push/pop operations
- **Benefits**:
  - Power-of-2 capacity for fast modulo operations using bit masks
  - Automatic resizing when capacity is exceeded
  - Iterator support for range-based loops
  - Memory-efficient storage of time-series data

### 3. `engine_builder` Pattern

- **Purpose**: Two-phase construction for optimal memory layout
- **Benefits**:
  - Pre-calculates total output size before engine creation
  - Allows history buffer to be initialized with correct dimensions
  - Validates dependency graph before committing resources
  - Cleaner separation of graph construction vs. execution

## Usage Patterns

### Option 1: Using `engine_builder` (Recommended)

```cpp
#include "opflow/op.hpp"
using namespace opflow;

// Create builder with input size
engine_builder<int> builder(3);

// Add operators
auto rollsum_op = std::make_shared<rollsum<int>>(std::vector<size_t>{0, 1}, 5);
auto rollsum_id = builder.add_op(rollsum_op, std::vector<size_t>{0});

// Build optimized engine
auto engine = builder.build(128); // Initial history capacity

// Use engine
engine.step(1, {10.0, 20.0, 30.0});
auto output = engine.get_node_output(rollsum_id);
```

### Option 2: Legacy Direct Construction

```cpp
#include "opflow/op.hpp"
using namespace opflow;

// Direct engine creation (backwards compatible)
engine_int engine(3);

// Add operators (less efficient - requires history reallocation)
auto rollsum_op = std::make_shared<rollsum<int>>(std::vector<size_t>{0, 1}, 5);
auto rollsum_id = engine.add_op(rollsum_op, std::vector<size_t>{0});

// Use engine
engine.step(1, {10.0, 20.0, 30.0});
auto output = engine.get_node_output(rollsum_id);
```

## Performance Improvements

### Memory Layout Optimization

- **Before**: Dynamic `std::deque` with `std::vector<double>` per step
- **After**: Single circular buffer with pre-allocated capacity
- **Benefit**: Reduces memory fragmentation and allocation overhead

### Dependency Lookup Optimization

- **Before**: `std::vector` storage with linear dependency iteration
- **After**: Flat array storage with `std::span` views
- **Benefit**: Better cache locality and faster dependency resolution

### History Management Optimization

- **Before**: Linear search through deque for watermark cleanup
- **After**: Efficient circular buffer with power-of-2 indexing
- **Benefit**: O(1) push/pop operations, O(n) cleanup only when needed

## Migration Guide

### For New Code

- Use `engine_builder<T>` for all new engine construction
- Pre-calculate operator graph before building engine
- Use larger initial history capacity for better performance

### For Existing Code

- Existing `engine<T>` constructor calls continue to work
- No changes needed for basic `step()` and output methods
- Consider migrating to `engine_builder` for better performance

## Memory Management

### History Buffer Growth

- Initial capacity can be specified in `builder.build(capacity)`
- Buffer doubles in size when full (maintains power-of-2)
- Automatic cleanup based on operator watermarks

### Watermark-Based Cleanup

- Non-cumulative operators provide watermarks for data expiration
- History automatically removes expired steps based on minimum watermark
- Prevents unlimited memory growth in long-running processes

## Examples

See the following files for usage examples:

- `examples/engine_integration_test.cpp` - Basic engine usage
- `examples/engine_builder_example.cpp` - Advanced builder pattern usage
- `test/engine_test.cpp` - Comprehensive test cases

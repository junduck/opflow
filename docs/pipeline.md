# Pipeline overview

| Stage     | Role        | Product                                   | Notes                                                               |
|-----------|-------------|-------------------------------------------|---------------------------------------------------------------------|
| 0 (Src)   | DataSource  | Raw data                                  | External file, DB, message queue, etc.                              |
| 1 (Pre)   | Preprocessor| (timestamp, key/group id, data)           | Cleans and prepares raw data for processing                         |
| 4 (Exec)  | Algo Exec   | (timestamp_as_datatype, data)             | Executes algorithms defined as DAGs                                 |
| 5 (Out)   | Observer    | (timestamp, Side effects)                 | Observes output, can be used for monitoring or further processing   |

## Defining an algorithm

Every algorithm is defined as a DAG (Directed Acyclic Graph). Data stream is processed as op nodes passing data to successor nodes.

```mermaid
graph TD
    Root["op::root (3)"]
    MA_Fast["fn::ema (12)"]
    MA_Slow["fn::ema (26)"]
    Diff["fn::sub"]
    MA_Diff["fn::ema (9)"]
    Sum_Left["op::sum (2)"]
    Sum_Right["op::sum (5)"]
    Add2["op::add2"]

    Root -- 0 --> MA_Fast
    Root -- 0 --> MA_Slow
    MA_Fast -- 0 --> Diff
    MA_Slow -- 0 --> Diff
    Diff -- 0 --> MA_Diff
    Root -- 1 --> Sum_Left
    Root -- 2 --> Sum_Right
    Sum_Left -- 0 --> Add2
    Sum_Right -- 0 --> Add2
```

```cpp
// Create DAG algo
using stream_double_op = fn_base<double>;
graph<stream_double_op> algo0_dag{};
auto root      = algo0_dag.root(3); // declare root input size
auto ma_fast   = algo0_dag.add<fn::ema<double>>({root}, 12); // can use concrete type as well
auto ma_slow   = algo0_dag.add<fn::ema>(root, 26);           // single pred dont need init list
auto diff      = algo0_dag.add<fn::sub>({ma_fast, ma_slow}); // default port 0
auto ma_diff   = algo0_dag.add<fn::ema>(diff | 0, 9);        // specify port: output[0] of diff node
auto sum_left  = algo0_dag.add<fn::sum>(root | 1, 2);        // output[1] of root node
auto sum_right = algo0_dag.add<fn::sum>(root | 2, 5);
auto add2      = algo0_dag.add<fn::add2>({sum_left | 0, sum_right | 0});
algo0_dag.output({diff, ma_diff, add2}); // exec will produce outputs of diff, ma_diff and add2

fn_exec<double> algo0(algo0_dag);
```

Equivalent with named nodes that supports a fluent API:

```cpp
// Create DAG representation of an algorithm
using stream_double_op = fn_base<double>;
graph_named<stream_double_op> algo0_dag{};
algo0_dag.root("root", 3)
   .add<fn::ema>("ma_fast", "root", 12)            // signature is name, predecessors..., constructor args...
   .add<fn::ema>("ma_slow", "root", ctor_args, 26) // constructor args are detected by first non-string arg, use ctor_args to disambiguate if necessary
   .add<fn::sub>("diff", "ma_fast.0", "ma_slow")   // use dot to specify port, or default to port 0
   .add<fn::ema>("ma_diff", "diff", 9)
   .add<fn::sum>("sum_left", "root.1", 2)
   .add<fn::sum>("sum_right", "root.2", 5)
   .add<fn::add2>("add2", std::vector<std::string>{"sum_left.0", "sum_right.0"}) // can also provide a range of predecessors
   .output("diff", "ma_diff", "add2"); // exec will produce outputs of diff, ma_diff and add2

fn_exec<double> algo0(algo0_dag);
```

## Notes on data semantics/assumptions

1. Data is in-order. -> no need for buffering and watermarking
2. Pipeline itself does not scale, its just sync fn calls from input to output for low latency. -> no need for partial aggr and other fancy distributed algos

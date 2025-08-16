# Pipeline Overview

| Stage | Role        | Product                              | Notes                                                                |
|-------|-------------|--------------------------------------|----------------------------------------------------------------------|
| 0     | DataSource  | Raw data                             | External file, DB, message queue, etc.                               |
| 1     | Parse       | (timestamp, key/group, data)         | Converts raw data to structured messages                             |
| 2     | Select      | (timestamp_as_datatype, key/group, data)    | Optional: filters/selects relevant messages                          |
| 3     | Align       | (timestamp_as_datatype, key/group, data)    | Aligns events to a common timestamp or window, emits aligned events  |
| 4     | Combine     | (timestamp_as_datatype, data)        | Combines across key/group to produce a single stream                 |
| 5     | Transform   | (timestamp_as_datatype, data)        | Applies chained transforms/aggregations                              |
| 6     | Graph Exec  | (timestamp_as_datatype, data)        | Executes the DAG                                                     |
| 7     | Observer    | (timestamp, Side effects)            | Observes output, can be used for monitoring or further processing    |

## Notes on data semantics/assumptions

1. Data is in-order. -> no need for buffering and watermarking
2. Pipeline itself does not scale, its just sync fn calls from input to output for low latency. -> no need for partial aggr and other fancy distributed algos

## Notes on Align

1. Timestamp alignment:
    - Assign to latest timestamp: Use the most recent timestamp seen across all keys/groups.
    - Assign to latest window: Snap to the latest fixed time window (e.g., 1s, 1min).
    - Assign to current walltime on emission: Use the system clock at the moment of emission.
2. Emission triggers:
   - Executor request/flush: Executor requests aligned data, triggering emission.
   - Timer-based: Emit at regular intervals (e.g., every second).
   - Event-Driven (LVCF): Emit when new data arrives, carrying forward the last value for missing keys/groups.
   - Time binning: Emit when one or all keys/groups have satisfied a window condition (e.g., all have reported for the current window).

## Notes on Combine

1. After aligner emitting data, the combiner needs to aggregate the data across keys/groups.

| key/group | timestamp | col0 | col1 | ... |
|-----------|-----------|------|------|-----|
| k0        | t0        | val  | val  | ... |
| k1        | t0        | val  | val  | ... |
| k2        | t0        | val  | val  | ... |

becomes:

| timestamp | colα | colβ | ... |
|-----------|------|------|-----|
| t0        | val  | val  | ... |

## Notes on Transform

1. transforms are chained linearly, with each transform taking the output of the previous one as input.
2. transforms can be 1:1 or N:1 (aggregation)
3. aggregation = window emitter + aggregate function
4. transform result can be observable

## Notes on Graph Exec

None.

## Notes on Observability

TBD.

## Error handling

TBD.

## Misc

1. We do not consider any backpressure. Need more benchmarks. Currently 30ns overhead per op node.

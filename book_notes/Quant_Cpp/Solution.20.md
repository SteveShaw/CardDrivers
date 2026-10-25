**I’d first ask whether the extra 95 μs is time spent executing, waiting for memory, or waiting to run.** The snippet suggests several hypotheses, but it doesn’t establish the cause.

**Start with the measurement boundary.**

Does the timing include `publish(totals)` and destruction of `totals`? Both matter: publishing might acquire a lock or block on a full queue, and destruction frees the map’s storage.

I’d record batch size, active-order count, and distinct active symbols alongside latency. A larger batch or more distinct symbols means more work; comparing those calls directly can obscure the explanation.

**What could cause the spikes?**

| Area              | Possible mechanism                                                                                                                                      |
| ----------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Map growth        | New symbols insert entries; crossing a capacity threshold triggers rehashing.                                                                           |
| Allocator         | Typical implementations allocate nodes and bucket storage. Slow paths can involve contention, acquiring memory, or faults. Destruction also costs time. |
| Input layout      | The vector stores pointers contiguously, but the orders may be scattered across memory.                                                                 |
| Memory hierarchy  | Order loads and map accesses can miss in caches or TLBs; remote NUMA memory adds latency.                                                               |
| Branch prediction | An unpredictable `active` pattern can cause mispredictions. A consistently mostly-active batch may be easy to predict.                                  |
| Hash distribution | Poor bucket distribution increases lookup work. Integer keys do not guarantee collision-free buckets.                                                   |
| Other work        | `publish`, logging, allocator locks, or shared cache lines can introduce contention.                                                                    |
| Machine state     | Preemption, interrupts, CPU migration, cold caches, frequency changes, and OS scheduling can stretch elapsed time.                                      |

`unordered_map` offers average-case performance, not a per-operation latency bound. Rehashing and collision-heavy operations can be expensive. [C++ working draft](https://eel.is/c++draft/unord.req)

One useful observation: **if every call has identical input, growth thresholds alone don’t explain occasional spikes**—the map is rebuilt every time. I’d look for changing allocator, cache, or scheduling conditions too.

**How I’d investigate**

1. **Measure realistic calls with low overhead.** Use an optimized build, representative data, and a preallocated per-thread trace buffer. Avoid printing or allocating in the measurement path. Measure instrumentation overhead.

2. **Separate aggregation, publishing, and cleanup.** Time the whole call as well as these phases in a diagnostic build. Preserve end-to-end measurements so moving work elsewhere doesn’t masquerade as an improvement.

3. **Compare wall time with thread CPU time.** On Linux, use a monotonic clock for elapsed time and `CLOCK_THREAD_CPUTIME_ID` for thread CPU consumption. A 100 μs wall interval with roughly 5 μs of thread CPU time suggests substantial off-CPU delay. If both rise, investigate additional work, memory stalls, or spinning. CPU time includes time running while stalled on memory; it isn’t “useful instruction time.” [Linux clock documentation](https://man7.org/linux/man-pages/man3/clock_gettime.3.html)

4. **Correlate slow calls with counters and traces.** For example, on Linux:
   
   ```
   perf stat -e cycles,instructions,branches,branch-misses,cache-references,cache-misses,page-faults,context-switches,cpu-migrations -- ./benchmark
   ```
   
   Then use CPU-specific cache/TLB events where supported. Generic `cache-misses` isn’t a complete breakdown of the cache hierarchy. Run smaller event groups if counters are multiplexed, and normalize by orders processed. [Linux perf stat documentation](https://github.com/torvalds/linux/blob/master/tools/perf/Documentation/perf-stat.txt)
   
   Aggregate counters can hide rare spikes. Use sampled profiles and timestamped traces to connect events to slow calls. For scheduling, `perf sched timehist` distinguishes waiting, scheduling delay, and runtime. [Linux perf sched documentation](https://github.com/torvalds/linux/blob/master/tools/perf/Documentation/perf-sched.txt)

5. **Change one factor at a time.** Compare reserved capacity, fixed storage, contiguous orders, controlled active patterns, and CPU affinity. Test both steady-state and cold/idle-start behavior under realistic load.

**Would `reserve()` solve it?**

It’s a useful experiment, not a complete solution.

`reserve(max_distinct_symbols)` provides bucket capacity for the expected number of elements under the configured load factor. It can prevent growth rehashes within that bound, but **does not preallocate all element nodes** in a typical node-based implementation. Calling it on this local map also performs setup every call. [C++ working draft](https://eel.is/c++draft/unord.req)

Reusing a reserved map helps preserve bucket capacity, but clearing it destroys entries; subsequent inserts typically allocate nodes again. It also leaves pointer chasing, input cache misses, and scheduler delays.

**How I’d eliminate dynamic allocation**

First establish limits: maximum symbols, maximum batch size, concurrency, and what happens when capacity is exceeded.

For a bounded, dense symbol universe, use persistent per-worker arrays allocated and touched during initialization:

```cpp
// Persistent worker-owned storage, initialized before processing:
// double totals[SymbolCount];
// bool seen[SymbolCount] = {};
// SymbolId touched[SymbolCount];

std::size_t count = 0;

for (const Order* order : orders) {
    if (!order->active)
        continue;

    const auto s = order->symbol; // Validated dense ID in range.

    if (!seen[s]) {
        seen[s] = true;
        totals[s] = 0.0;
        touched[count++] = s;
    }

    totals[s] += order->price * order->quantity;
}

publish_sparse(touched, count, totals);

for (std::size_t i = 0; i < count; ++i)
    seen[touched[i]] = false;
```

This is a design sketch: it assumes synchronous, nonthrowing publication and no reentrancy. Production code needs cleanup on failure and explicit ownership if publication is asynchronous.

The touched list avoids clearing or publishing the entire symbol universe. It also preserves the distinction between an absent symbol and a present symbol whose total is zero.

**The publication path must also support allocation-free output.** Converting the arrays back into a newly allocated map would reintroduce the problem. An asynchronous publisher could consume preallocated buffers, with an explicit policy for a full queue.

For sparse or unbounded external IDs, alternatives include:

- Mapping external IDs to dense internal IDs before this path.
- A fixed-capacity flat hash table with bounded occupancy and explicit overflow handling.
- A preallocated node pool or arena, which avoids general heap allocation but retains map lookup and locality costs.

An arena must not silently fall back to the heap if allocation freedom is required.

**Would a flat array be better? AoS or SoA?**

A flat array is attractive when the symbol range is bounded and compact. It removes hashing and node indirection. A huge, sparsely accessed array can still suffer cache and TLB misses, so its footprint matters.

For orders:

| Layout                          | When it helps                                                                                                    |
| ------------------------------- | ---------------------------------------------------------------------------------------------------------------- |
| Contiguous AoS: `vector<Order>` | A strong starting point when processing uses most fields of each order.                                          |
| SoA: separate field arrays      | Useful when `Order` is large and this loop needs only a few fields; can reduce bandwidth and help vectorization. |
| Compact active-order storage    | Can remove inactive-order scanning, at the cost of maintaining that representation.                              |

SoA doesn’t automatically make this loop vectorizable: repeated symbol IDs create dependent updates to the same total. Copying scattered orders into a compact layout inside every call also has a cost that must be included.

I wouldn’t blindly replace the branch with multiplication by `active`: that can load unnecessary fields, create entries for inactive symbols, and change floating-point behavior for NaNs or infinities.

**What do the latency percentiles tell us?**

- **p50:** typical behavior.
- **p99:** the threshold exceeded by approximately 1% of calls.
- **p99.9:** the threshold exceeded by approximately 0.1% of calls.

Suppose 99.8% of calls take 5 μs and 0.2% take 105 μs. The average is only **5.2 μs**, while p99.9 is **105 μs**. Even p99 misses that tail.

I’d report percentiles, sample count, deadline violations, and workload conditions. Tail estimates need enough observations. For an end-to-end service, measure from intended arrival through completion so slow processing and resulting queue buildup aren’t excluded.

**My likely redesign is bounded, persistent per-worker aggregation storage with compact input data and a preallocated publication path—but I’d validate it against slow-call traces.** Removing allocations can make execution more predictable; it cannot by itself guarantee a wall-clock deadline on a general-purpose OS.

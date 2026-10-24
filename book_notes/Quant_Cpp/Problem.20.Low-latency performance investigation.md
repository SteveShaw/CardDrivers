# Problem 20 Low-latency performance investigation

You're told this function is causing latency spikes:

```
void process(const std::vector<Order*>& orders)
{
    std::unordered_map<int, double> totals;

    for (auto* order : orders) {
        if (order->active) {
            totals[order->symbol] +=
                order->price * order->quantity;
        }
    }

    publish(totals);
}
```

The system normally takes:

```
5 μs
```

but occasionally takes:

```
100+ μs
```

You are asked:

> **Why might this happen, and how would you investigate and redesign it?**

There isn't one correct answer.

I'd expect a strong candidate to investigate several areas:

- `unordered_map` allocations and rehashing
- heap allocator latency
- pointer chasing through `Order*`
- cache misses
- branch misprediction on `active`
- hash collisions
- page faults
- context switches
- CPU migration
- lock contention elsewhere
- NUMA effects
- cold instruction/data caches
- frequency scaling
- OS scheduling

Then the interviewer may ask:

> How would you eliminate dynamic allocation?

> Would a flat array indexed by symbol ID be better?

> AoS or SoA?

> Would `reserve()` solve the problem?

> How would you measure cache misses?

> How would you distinguish CPU execution time from scheduler latency?

> What does p50/p99/p99.9 latency tell you?

> Why can average latency be misleading?

This last problem is very representative of a **senior quant/trading C++ interview**, because it combines C++, Linux, CPU architecture, data structures, and performance engineering.

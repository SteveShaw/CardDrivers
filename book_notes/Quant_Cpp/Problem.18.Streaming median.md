# Problem 18 Streaming median

Prices arrive continuously.

After every new price, return the median.

Example:

```
5       -> 5
5,2     -> 3.5
5,2,10  -> 5
5,2,10,1 -> 3.5
```

Target: $O(\log n)$

per insertion.

**Follow-ups:** Solve using two heaps. What invariants must the heaps maintain? How do you support removing old elements for a rolling median? What if prices are bounded integer ticks—can you do better than heaps?

**Tests:** heaps, invariants, streaming data.

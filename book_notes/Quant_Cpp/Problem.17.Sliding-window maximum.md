# Problem 17 Sliding-window maximum

Given prices:

```
1 3 -1 -3 5 3 6 7
```

and:

```
window = 3
```

return:

```
3 3 5 5 6 7
```

Required complexity: $O(n)$

**Follow-ups:** Why is a heap not ideal? Implement using a monotonic deque. Prove each element is inserted/removed at most once. How would you implement this for an infinite market-data stream?

**Tests:** deque, amortized analysis, streaming.

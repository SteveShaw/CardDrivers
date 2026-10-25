# Problem 15 Streaming mean and variance

Prices arrive one at a time:

```
101
102
100
103
...
```

Maintain:

```
count
mean
variance
```

without storing all observations.

**Follow-ups:** Why can the naive formula

$$
E[X^2] - (E[X])^2
$$

have numerical problems?

Implement **Welford's algorithm**.

Then extend it to a rolling window of the last `N` observations.

**Tests:** numerical programming, streaming algorithms.

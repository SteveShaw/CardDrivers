# Problem 14 Streaming VWAP

Trades arrive:

```
struct Trade {
    int64_t price;
    uint32_t quantity;
};
```

Calculate `VWAP` continuously:

$$
\frac{\sum p_iq_i}{\sum q_i}
$$

Each update should be **O(1)**.

**Follow-ups:** What types should accumulators use? Could `price * quantity` overflow? How would you calculate `VWAP` over only the last 5 minutes? How would you support VWAP for thousands of symbols efficiently?

**Tests:** basic quantitative reasoning + systems design.

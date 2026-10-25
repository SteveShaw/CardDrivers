# Problem 19 Top-K most active symbols

You receive millions of trades:

```
AAPL 100
MSFT 50
AAPL 200
NVDA 500
...
```

Return the **K symbols with highest total traded volume**.

**Follow-ups:** Give the straightforward hash-table + heap solution. Complexity? What if there are 100 million symbols? What if you need the answer continuously rather than at the end? What if exact results aren't required? How would multiple threads aggregate statistics without creating severe contention?

**Tests:** hashing, heaps, scalability.

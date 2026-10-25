# Problem 16. Position and P&L engine

Executions arrive:

```
BUY  100 @ $50
BUY  200 @ $51
SELL 150 @ $52
```

Maintain:

```
struct Position {
    int64_t quantity;
    double averagePrice;
    double realizedPnL;
};
```

Given a current market price, calculate unrealized P&L.

**Follow-ups:** What happens when a sell changes a long position into a short position? What's the new average price? FIFO versus average-cost accounting? How should fees be incorporated? Why might production code avoid `double` for money?

**Tests:** trading concepts + edge-case-heavy coding.

## Solution

Let `quantity` be signed: positive for a long position, negative for a short position. Under **average-cost accounting**, `averagePrice` is the entry price of the remaining position.

### Your example

Ignoring fees:

| Execution      | Quantity | Average price | Cumulative realized P&L |
| -------------- | -------- | ------------- | ----------------------- |
| BUY 100 @ $50  | 100      | $50.00        | $0                      |
| BUY 200 @ $51  | 300      | $50.6667      | $0                      |
| SELL 150 @ $52 | 150      | $50.6667      | $200                    |

The second buy updates the average:

$$
A=\frac{100(50)+200(51)}{300}=50.6667
$$



The sell realizes:

$$
150(52-50.6667)=\$200
$$



Selling part of a position **does not change its average entry price**.

For market price \(M\):

$$
\boxed{\text{Unrealized P\&L}=q(M-A)}
$$



The signed quantity makes this work for shorts too. Here, unrealized P&L is $150(M-50.6667)$; at \$53, it is **$350**.

### General execution logic

Represent a buy as positive `delta` and a sell as negative `delta`. For execution price `price`:

```
If the position is flat:
    quantity = delta
    averagePrice = price

Else if quantity and delta have the same sign:
    averagePrice =
        (abs(quantity) * averagePrice + abs(delta) * price)
        / (abs(quantity) + abs(delta))
    quantity += delta

Else:
    closed = min(abs(quantity), abs(delta))
    realizedPnL += closed * sign(quantity) * (price - averagePrice)

    newQuantity = quantity + delta

    If newQuantity == 0:
        averagePrice = 0
    Else if newQuantity and quantity have different signs:
        averagePrice = price
    Otherwise:
        averagePrice stays unchanged

    quantity = newQuantity
```

### When a sell flips long to short

Suppose you are long \$150 @ \$50.6667, then sell **200 @ $52**:

- First 150 shares close the long, realizing another **$200**.
- Remaining 50 shares open a short.
- New position: **quantity = −50, averagePrice = $52**.
- Cumulative realized P&L, including the earlier sell: **$400**.

The new short’s average price is the price of the execution that opened it. At a market price of \$51, its unrealized P&L is `−50 × (51 − 52) = $50`.

### FIFO versus average cost

**FIFO** closes the oldest purchase lots first. Your original sale closes 100 shares bought at \$50 and 50 bought at $51:

$$
R=100(52-50)+50(52-51)=\$250
$$



The remaining 150 shares have a cost of $51 each.

| Method       | Realized P&L | Unrealized at $53 | Total |
| ------------ | ------------ | ----------------- | ----- |
| Average cost | $200         | $350              | $550  |
| FIFO         | $250         | $300              | $550  |

The methods allocate profit differently between realized and unrealized P&L. FIFO requires storing individual lots; this three-field structure cannot represent them.

### Fees

Choose a consistent convention:

- **Expense immediately:** subtract each execution’s fee from `realizedPnL`; keep `averagePrice` based on execution prices. Unrealized P&L excludes estimated future exit fees.
- **Include in cost basis:** opening fees increase a long’s effective entry price or decrease a short’s effective entry price. Closing fees reduce realized P&L. For a position flip, allocate the fee between closing and opening quantities.

Do not both incorporate a fee into cost basis and subtract it again.

### Why avoid `double`?

Binary floating point cannot represent most decimal money values exactly. Repeated calculations can accumulate rounding differences and complicate reconciliation.

Production systems often use **scaled integers** or **decimal arithmetic**, with explicit precision and rounding rules. Average prices can require finer precision than individual trade prices, so retain adequate internal precision and round only at defined boundaries.

### What does it mean for flat position

A position is **flat** when `quantity == 0`: you hold neither a long nor a short position.

For example, buying 100 shares and then selling all 100 leaves you flat.

The next trade opens a new position, so its execution price becomes the average entry price:

```
quantity = delta       // +100 for a buy, −100 for a sell
averagePrice = price
```

Being flat does **not** reset `realizedPnL`; previously realized profits or losses remain.

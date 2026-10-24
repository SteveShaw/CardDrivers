# Problem 11 Build a basic limit order book

Receive:

```
ADD orderId side price quantity
CANCEL orderId
```

Example:

```
ADD 1 BUY  100.25 50
ADD 2 BUY  100.30 20
ADD 3 SELL 100.40 30
ADD 4 SELL 100.35 10
```

Implement:

```
Price bestBid() const;
Price bestAsk() const;
```

**Follow-ups:** What data structures would you use? Complexity of `ADD` and `CANCEL`? Why shouldn't price usually be stored as `double`? How would you represent `$100.25` using integer ticks? How would your design change for millions of updates/sec?

**Tests:** trading knowledge + data structures.

## Solution:

A straightforward design is:

```
using OrderId = uint64_t;
using Price   = int64_t;
using Qty     = uint32_t;

enum class Side {
    Buy,
    Sell
};

struct Order {
    OrderId id;
    Side side;
    Price price;
    Qty qty;
};
```

Use two ordered containers for prices:

```
std::map<Price, Qty, std::greater<Price>> bids;
std::map<Price, Qty> asks;
```

and a hash table for cancellation:

```
std::unordered_map<OrderId, Order> orders;
```

The bid map is sorted descending, so:

```
bids.begin()
```

is the best bid.

The ask map is sorted ascending, so:

```
asks.begin()
```

is the best ask.

A simple implementation:

```
class OrderBook {
public:
    using OrderId = uint64_t;
    using Price   = int64_t;
    using Qty     = uint32_t;

    enum class Side {
        Buy,
        Sell
    };

    struct Order {
        Side side;
        Price price;
        Qty qty;
    };

    bool add(OrderId id, Side side, Price price, Qty qty)
    {
        if (orders_.contains(id))
            return false;

        orders_.emplace(id, Order{side, price, qty});

        if (side == Side::Buy) {
            bids_[price] += qty;
        } else {
            asks_[price] += qty;
        }

        return true;
    }

    bool cancel(OrderId id)
    {
        auto it = orders_.find(id);

        if (it == orders_.end())
            return false;

        const Order& order = it->second;

        if (order.side == Side::Buy) {
            auto level = bids_.find(order.price);

            level->second -= order.qty;

            if (level->second == 0)
                bids_.erase(level);
        } else {
            auto level = asks_.find(order.price);

            level->second -= order.qty;

            if (level->second == 0)
                asks_.erase(level);
        }

        orders_.erase(it);

        return true;
    }

    std::optional<Price> bestBid() const
    {
        if (bids_.empty())
            return std::nullopt;

        return bids_.begin()->first;
    }

    std::optional<Price> bestAsk() const
    {
        if (asks_.empty())
            return std::nullopt;

        return asks_.begin()->first;
    }

private:
    std::map<Price, Qty, std::greater<Price>> bids_;
    std::map<Price, Qty> asks_;

    std::unordered_map<OrderId, Order> orders_;
};
```

For example:

```
ADD 1 BUY  100.25 50
ADD 2 BUY  100.30 20
ADD 3 SELL 100.40 30
ADD 4 SELL 100.35 10
```

gives:

```
Best Bid = 100.30
Best Ask = 100.35
```

### Follow-up: Why do we need `orders_`?

Without:

```
unordered_map<OrderId, Order>
```

a cancel only gives us:

```
CANCEL 12345
```

We don't know:

```
side
price
quantity
```

so we'd have to search the book to locate the order.

That could be very expensive.

With:

```
orders_[orderId]
```

we immediately find the order metadata in average O(1).

Then cancellation becomes roughly: $$O(1) + O(\log P)$

where \($P$\) is the number of price levels.

### Follow-up: Why shouldn't you use `double` for price?

This is a very common trading interview question.

Don't do:

```
double price = 100.25;
```

because floating-point values cannot represent most decimal fractions exactly.

For example:

```
0.1 + 0.2 != 0.3
```

in exact binary floating-point representation.

This creates trouble with:

```
price1 == price2
```

and with using prices as map keys.

Instead represent price as an integer number of ticks.

Suppose tick size is:

```
$0.01
```

Then:

```
$100.25 -> 10025
$100.30 -> 10030
```

So:

```
using Price = int64_t;
```

The conversion is conceptually:

$$
\text{priceTicks} = \frac{\text{price}}{\text{tickSize}}
$$

Then comparisons become exact integer comparisons.

### Follow-up: What if the tick size is $0.05?

Then:

```
100.25 / 0.05 = 2005 ticks
```

You can represent:

```
Price price = 2005;
```

The actual dollar price is:

  $2005 \times 0.05 = 100.25$
Often a production system stores either:

```
integer ticks
```

or:

```
fixed-point integer price
```

such as ten-thousandths of a dollar.

### Follow-up: Why might `std::map` be too slow?

This is where the answer becomes more quant-dev specific.

A `std::map` is normally implemented as a red-black tree.

Each node typically requires allocation and contains pointers:

```
parent
left
right
color
key
value
```

Walking the tree means pointer chasing:

```
node -> node -> node -> node
```

That causes poor cache locality.

For a high-frequency feed handler or order book, this can be much slower than a contiguous array.

### Follow-up: If the price range is bounded, what can we use instead?

Suppose the instrument trades between:

```
$50 and $150
```

with one-cent ticks.

That's only: $10000$

possible price levels.

We can use an array:

```
std::array<PriceLevel, 10001> levels;
```

Then:

```
level = levels[price - minPrice];
```

Price lookup becomes: $O(1)$ with contiguous memory.

That's often dramatically faster. The trade-off is memory usage and handling a large or dynamic price range.

### Follow-up: Could best bid/ask still be O(1)?

Yes.

Maintain them incrementally:

```
Price bestBid_;
Price bestAsk_;
```

When adding:

```
if (side == Side::Buy)
    bestBid_ = std::max(bestBid_, price);
```

When cancelling the last order at the best price, you have to find the next occupied price level.

With an array, you may scan downward or upward. 

If prices are dense and the range is small, that's often cheap.

More sophisticated implementations can use bitsets or hierarchical indexes to find the next occupied level efficiently.

### Strong interview summary
A concise answer:
"For the basic book, I'd keep bids and asks as ordered price-level maps and an unordered_map<OrderId, Order> for direct cancellation lookup. Bids are sorted descending and asks ascending, so best bid and ask are available from begin() in $O(1)$. ADD and CANCEL are $O(\log P)$ because of the price-level tree. I would represent price in integer ticks instead of double. For a low-latency implementation, I'd consider replacing std::map with a contiguous array or another cache-friendly structure if the tick range is bounded."

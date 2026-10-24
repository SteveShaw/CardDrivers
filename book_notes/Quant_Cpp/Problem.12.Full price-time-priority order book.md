# Problem 12 Full price-time-priority order book

Orders at the same price must maintain FIFO priority:

```
BUY 100.25:
    order 17
    order 23
    order 41
```

Support:

```
ADD
CANCEL
MODIFY
EXECUTE
```

Target efficient cancellation by `orderId`.

**Follow-ups:** How can cancellation be O(1)? Would you use `std::map<Price, PriceLevel>`? What does each `PriceLevel` contain? Why might an intrusive linked list outperform `std::list`? How would you obtain the top five price levels?

**Tests:** realistic exchange/order-book architecture.

## Solution:

Now each price level contains multiple orders:

```
BUY 100.25

order 17
order 23
order 41
```

The key requirement is:

> Orders at the same price must execute in FIFO order.

So the book needs to preserve:

1. **price priority**
2. **time priority**

For buys:

```
higher price first
```

For sells:

```
lower price first
```

Within the same price:

```
older order first
```

## Basic data structure

A natural model is:

```
struct Order {
    OrderId id;
    Side side;
    Price price;
    Qty qty;
};
```

and:

```
struct PriceLevel {
    std::list<Order> orders;
    Qty totalQty = 0;
};
```

Then:

```
std::map<Price, PriceLevel, std::greater<Price>> bids;
std::map<Price, PriceLevel> asks;
```

But cancellation by ID introduces an important requirement.

We want:

```
CANCEL orderId
```

to be fast.

So maintain:

```
unordered_map<OrderId, OrderLocation>
```

where `OrderLocation` stores enough information to remove the order directly.

For example:

```
struct OrderLocation {
    Side side;
    Price price;
    std::list<Order>::iterator iterator;
};
```

Then cancellation does not need to search through the price level.

# Example implementation

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

private:
    struct Order {
        OrderId id;
        Price price;
        Qty qty;
        Side side;
    };

    struct PriceLevel {
        std::list<Order> orders;
        uint64_t totalQty = 0;
    };

    using BidBook =
        std::map<Price, PriceLevel, std::greater<Price>>;

    using AskBook =
        std::map<Price, PriceLevel>;

    struct OrderLocation {
        Side side;
        Price price;
        std::list<Order>::iterator iter;
    };

public:
    bool add(OrderId id,
             Side side,
             Price price,
             Qty qty)
    {
        if (index_.contains(id))
            return false;

        if (side == Side::Buy) {
            auto& level = bids_[price];

            level.orders.push_back(
                Order{id, price, qty, side});

            auto iter = std::prev(level.orders.end());

            level.totalQty += qty;

            index_.emplace(
                id,
                OrderLocation{
                    side,
                    price,
                    iter
                });
        } else {
            auto& level = asks_[price];

            level.orders.push_back(
                Order{id, price, qty, side});

            auto iter = std::prev(level.orders.end());

            level.totalQty += qty;

            index_.emplace(
                id,
                OrderLocation{
                    side,
                    price,
                    iter
                });
        }

        return true;
    }
```

Cancellation:

```
bool cancel(OrderId id)
{
    auto idx = index_.find(id);

    if (idx == index_.end())
        return false;

    auto loc = idx->second;

    if (loc.side == Side::Buy) {
        auto levelIt = bids_.find(loc.price);

        auto& level = levelIt->second;

        level.totalQty -= loc.iter->qty;
        level.orders.erase(loc.iter);

        if (level.orders.empty())
            bids_.erase(levelIt);

    } else {
        auto levelIt = asks_.find(loc.price);

        auto& level = levelIt->second;

        level.totalQty -= loc.iter->qty;
        level.orders.erase(loc.iter);

        if (level.orders.empty())
            asks_.erase(levelIt);
    }

    index_.erase(idx);

    return true;
}
```

The important idea is:

```
index_[orderId]
```

contains a direct iterator to the order.

Therefore we don't scan:

```
order1
order2
order3
...
order5000
```

to find it.

---

# EXECUTE

Suppose we execute against the best ask.

The best ask is:

```
asks_.begin()
```

and the first order is:

```
asks_.begin()->second.orders.front()
```

because FIFO means the oldest order is at the front.

Conceptually:

```
void executeBestAsk(Qty qty)
{
    while (qty > 0 && !asks_.empty()) {

        auto levelIt = asks_.begin();
        auto& level = levelIt->second;

        auto orderIt = level.orders.begin();

        Qty executed =
            std::min(qty, orderIt->qty);

        qty -= executed;
        orderIt->qty -= executed;
        level.totalQty -= executed;

        if (orderIt->qty == 0) {
            index_.erase(orderIt->id);
            level.orders.erase(orderIt);
        }

        if (level.orders.empty()) {
            asks_.erase(levelIt);
        }
    }
}
```

This naturally preserves price-time priority.

### Follow-up: How can cancellation be O(1)?

There's a subtle answer.

The order lookup can be $O(1)$:

```
unordered_map<OrderId, OrderLocation>
```

and erasing an element from a `std::list` given an iterator is $O(1)$:

```
level.orders.erase(iterator);
```

However, if the price levels themselves are stored in:

```
std::map
```

then finding:

```
bids_.find(price)
```

is still: $O(\log P)$

So the complete operation isn't necessarily $O(1)$.

A stronger design stores a pointer or iterator to the price level as well:

```
struct OrderLocation {
    PriceLevel* level;
    OrderIterator order;
};
```

Then cancellation can directly access the price level and order.

Conceptually:

```
auto loc = index_[id];

loc.level->totalQty -= loc.order->qty;
loc.level->orders.erase(loc.order);
```

Now the order removal itself is: $O(1)$
The remaining complication is removing an empty price level from whatever structure holds it.

### Follow-up: Why use an intrusive list?

`std::list<Order>` has an important performance problem.

Each `push_back` generally creates a separate list node allocation:

```
heap allocation
```

The layout could look like:

```
node -> node -> node -> node
```

with nodes scattered across memory.

That creates:

- allocator overhead
- pointer chasing
- cache misses
- memory fragmentation

In a low-latency system, you can instead place the linkage inside `Order` itself:

```
struct Order {
    OrderId id;
    Price price;
    Qty qty;

    Order* prev;
    Order* next;
};
```

Then a price level contains:

```
struct PriceLevel {
    Order* head;
    Order* tail;
    uint64_t totalQty;
};
```

This is an **intrusive linked list**.

Add:

```
order->prev = level.tail;
order->next = nullptr;

if (level.tail)
    level.tail->next = order;
else
    level.head = order;

level.tail = order;
```

Remove:

```
if (order->prev)
    order->prev->next = order->next;
else
    level.head = order->next;

if (order->next)
    order->next->prev = order->prev;
else
    level.tail = order->prev;
```

Given an `Order*`, removal is: $O(1)$

with no separate list node allocation.

This is very common in performance-sensitive systems.

### Follow-up: Where do `Order` objects come from?

Don't necessarily use:

```
new Order
```

for every message.

A better low-latency design uses an object pool:

```
preallocated Orders

[Order][Order][Order][Order][Order]...
```

Then:

```
Order* order = pool.allocate();
```

and cancellation eventually does:

```
pool.release(order);
```

Advantages:

- no hot-path heap allocation
- predictable latency
- better locality
- less fragmentation

This directly connects Problem 12 to the object-pool problem.

### Follow-up: `std::map` versus array price levels

A simple interview design:

```
std::map<Price, PriceLevel>
```

is completely reasonable as a first solution.

But for low-latency trading, I'd explain that I would consider replacing it.

Suppose the valid tick range is:

```
9000 ... 11000
```

Then:

```
std::vector<PriceLevel> levels(2001);
```

allows:

```
PriceLevel& level =
    levels[price - minPrice];
```

which is $O(1)$.

That removes tree traversal and improves locality.

### Follow-up: What happens when an order is modified?

This depends on exchange rules.

A common rule is:

### Decrease quantity

May preserve time priority.

Example:

```
order 17: 100 shares
```

modified to:

```
order 17: 50 shares
```

It stays in the same position.

### Increase quantity

Often loses time priority.

Conceptually:

```
cancel old order
+
add new order at tail
```

Changing price also normally loses priority because the order moves to another price level.

So a good interview answer is:

> "MODIFY semantics are venue-specific. I wouldn't assume priority behavior without the exchange specification. Typically a size reduction may preserve priority while a price change or size increase loses priority."

That is an important trading-system answer.

### Follow-up: How do you get top 5 price levels?

With the map-based design it's easy.

Bids:

```
auto it = bids_.begin();

for (int i = 0;
     i < 5 && it != bids_.end();
     ++i, ++it) {
    std::cout
        << it->first
        << " "
        << it->second.totalQty;
}
```

Because bids are sorted descending:

```
best
2nd
3rd
4th
5th
```

Asks are analogous.

Complexity: $O(K)$

after obtaining `begin()`.

For fixed:

```
K = 5
```

that's effectively constant work.

### Follow-up: How would matching work?

Suppose we receive:

```
SELL 100 @ 100.00
```

and the bid book is:

```
100.10 : 50
100.05 : 30
100.00 : 100
99.95  : 200
```

The incoming sell can match prices: `bid > sellPrice`

So first:

```
100.10
```

then:

```
100.05
```

then:

```
100.00
```

until either:

```
incoming qty == 0
```

or:

```
bestBid < sellPrice
```

Within every price level, consume orders from:

```
level.head
```

or:

```
level.orders.begin()
```

which preserves FIFO.

### Follow-up: Why isn't a heap enough?

A heap can efficiently tell us:

```
best bid
```

or:

```
best ask
```

but arbitrary cancellation is problematic.

Suppose we receive:

```
CANCEL order 837261
```

If the order is buried inside a heap, removing it efficiently requires either:

- maintaining its heap index and repairing the heap, or
- lazy deletion

And a heap does not naturally model multiple FIFO orders at the same price.

So a heap alone is usually not the natural full order-book representation.

### Follow-up: What should the production-level structure look like?

For a high-throughput trading system, a common conceptual design is:

```
OrderId lookup
     |
     v

flat hash table
OrderId -> Order*
             |
             v
         Order object
         /          \
      prev          next
        \            /
         PriceLevel
```

And price levels might be:

```
Price index
    |
    v

contiguous array
of PriceLevel
```

Each level:

```
PriceLevel
--------------------------------
head -> Order -> Order -> Order
tail -------------------------^
totalQty
orderCount
```

Then operations can approach:

```
ADD       O(1)
CANCEL    O(1)
EXECUTE   O(1) per affected order
best bid  O(1) typically
best ask  O(1) typically
```

with very little allocation.

### Strong interview summary for Problem 12

For the full book:

> "Each price level needs a FIFO queue to implement time priority. I'd maintain a direct `OrderId -> Order*` or `OrderId -> iterator` index so arbitrary cancellation doesn't require searching. A simple implementation can use `std::map<Price, PriceLevel>` plus `std::list<Order>`, but for a low-latency implementation I'd use preallocated `Order` objects arranged in intrusive lists, with an array or otherwise cache-friendly price-level structure. That gives O(1) order insertion/removal within a level and eliminates hot-path heap allocation."

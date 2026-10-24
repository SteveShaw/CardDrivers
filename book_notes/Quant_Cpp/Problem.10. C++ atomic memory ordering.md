# Problem 10 C++ atomic memory ordering

Given:

```
int price = 0;
std::atomic<bool> ready{false};

// Thread A
price = 100;
ready.store(true, std::memory_order_relaxed);

// Thread B
if (ready.load(std::memory_order_relaxed))
    std::cout << price;
```

Is this guaranteed to be correct according to the C++ memory model?

If not, fix it.

**Follow-ups:** Explain acquire/release. Define "happens-before." When is `memory_order_relaxed` appropriate? What's the difference between compiler reordering and CPU reordering? Why does x86 sometimes make incorrect C++ code appear to work?

**Tests:** C++ memory model—very important for low-latency interviews.

## Solution:

The code is **not guaranteed to be correct** under the C++ memory model.

The problem is that `price` is a normal non-atomic variable, and the relaxed store/load on `ready` does **not** create a synchronization relationship that makes Thread A's write to `price` visible to Thread B.

A correct version is:

```
int price = 0;
std::atomic<bool> ready{false};

// Thread A
price = 100;
ready.store(true, std::memory_order_release);

// Thread B
if (ready.load(std::memory_order_acquire))
    std::cout << price;
```

If Thread B's acquire load reads the value written by Thread A's release store, then:

```
price = 100
```

happens-before:

```
std::cout << price
```

So Thread B is guaranteed to see `price == 100`.

## Why the original version is wrong

With:

```
ready.store(true, std::memory_order_relaxed);
```

and:

```
ready.load(std::memory_order_relaxed);
```

you only get atomicity for the `ready` variable itself.

In other words, `ready` won't be torn or corrupted. But it says nothing about ordering or visibility of:

```
price = 100;
```

relative to other threads.

The compiler or CPU may effectively expose operations in an order that lets Thread B observe:

```
ready == true
```

without being synchronized with the earlier write to `price`.

More fundamentally, because Thread A writes `price` and Thread B reads `price` without a happens-before relationship, the program contains a **data race** on `price`, which means the behavior is undefined.

That last point is the strongest interview answer.

### Follow-up 1: What do acquire and release mean?

A **release operation** says:

> Everything this thread did before this release must be visible to another thread that successfully performs a corresponding acquire.

Example:

```
price = 100;
ready.store(true, std::memory_order_release);
```

Conceptually:

```
price = 100
    |
    v
release store
```

An **acquire operation** says:

> Everything synchronized through this acquire becomes visible before subsequent operations in this thread.

Example:

```
if (ready.load(std::memory_order_acquire)) {
    std::cout << price;
}
```

Conceptually:

```
acquire load
    |
    v
read price
```

Together:

```
Thread A                         Thread B

price = 100
     |
     v
ready.store(true, release)
            |
            | synchronizes-with
            v
                     ready.load(acquire)
                             |
                             v
                         read price
```

That synchronization is what makes accessing the non-atomic `price` safe.

### Follow-up 2: What is "happens-before"?

`happens-before` is a formal relationship in the C++ memory model.

If operation A happens-before operation B, then the effects of A are guaranteed to be visible to B in the way required by the memory model.

Within a single thread, operations are normally related by **sequenced-before**:

```
price = 100;
ready.store(true, std::memory_order_release);
```

So:

```
price write
    sequenced-before
release store
```

If another thread performs:

```
ready.load(std::memory_order_acquire)
```

and that acquire reads from the release store, then the release:

```
synchronizes-with
```

the acquire.

Therefore we get:

```
price write
    sequenced-before
release store
    synchronizes-with
acquire load
    sequenced-before
price read
```

By transitivity:

```
price write
    happens-before
price read
```

This eliminates the data race.

A concise interview definition is:

> "Happens-before is the C++ memory-model relationship that establishes visibility and ordering between operations. If a write to a non-atomic object happens-before a read of that object, the read is properly synchronized."

### Follow-up 3: When is `memory_order_relaxed` appropriate?

Use relaxed ordering when you need **atomicity**, but do not need that atomic operation to order other memory accesses.

The classic example is a statistics counter:

```
std::atomic<uint64_t> packets{0};

void onPacket()
{
    packets.fetch_add(1, std::memory_order_relaxed);
}
```

Suppose multiple threads increment:

```
packets
```

You don't care what other memory operations happen before or after the increment. You only care that increments aren't lost.

That's an excellent use case for `relaxed`.

Another example:

```
std::atomic<uint64_t> requestId{0};

uint64_t getNextId()
{
    return requestId.fetch_add(
        1, std::memory_order_relaxed);
}
```

You need unique IDs, not cross-thread synchronization of unrelated data.

A useful rule:

> Relaxed gives atomicity and modification ordering for that atomic variable, but does not establish synchronization for surrounding memory.

### Follow-up 4: Could `price` itself just be atomic?

Yes.

For example:

```
std::atomic<int> price{0};
std::atomic<bool> ready{false};

price.store(100, std::memory_order_relaxed);
ready.store(true, std::memory_order_relaxed);
```

Then Thread B could read:

```
if (ready.load(std::memory_order_relaxed))
    std::cout << price.load(std::memory_order_relaxed);
```

Now there is no data race because `price` itself is atomic.

But this still does **not** guarantee the logical relationship:

> if `ready == true`, then `price == 100`.

Thread B could theoretically observe:

```
ready == true
price == old value
```

because the two atomics aren't ordered relative to each other.

So making everything atomic is not automatically sufficient to preserve higher-level invariants.

You would still typically write:

```
price.store(100, std::memory_order_relaxed);
ready.store(true, std::memory_order_release);
```

and:

```
if (ready.load(std::memory_order_acquire))
    std::cout << price.load(std::memory_order_relaxed);
```

Once acquire/release synchronizes the threads, the relaxed access to `price` can be sufficient because it is itself atomic and is ordered by the release/acquire relationship.

### Follow-up 5: What's the difference between compiler reordering and CPU reordering?

This is very important in low-latency C++ interviews.

### Compiler reordering

The compiler is allowed to transform your code as long as it preserves behavior according to the C++ abstract machine.

For example:

```
a = 1;
b = 2;
```

might be rearranged internally if the compiler determines that doing so cannot affect observable single-threaded behavior.

In concurrent code, memory-order semantics tell the compiler which reorderings are permitted.

For example:

```
ready.store(true, std::memory_order_release);
```

acts as a compiler-ordering constraint: operations before the release cannot simply be moved across it in ways that break release semantics.

### CPU reordering

Modern CPUs execute loads and stores out of order for performance.

They use mechanisms such as:

- store buffers
- load queues
- speculative execution
- out-of-order execution
- cache coherence protocols

So even if generated instructions look ordered, other cores may observe memory effects differently.

C++ atomics solve both levels.

The compiler translates:

```
memory_order_acquire
memory_order_release
```

into the appropriate combination of compiler constraints and architecture-specific instructions/fences.

A strong interview answer is:

> "The C++ memory model abstracts over both compiler optimization and hardware memory ordering. I shouldn't reason purely from emitted instruction order."

### Follow-up 6: Why does incorrect code often appear to work on x86?

x86 has a relatively strong memory model, generally described as **TSO — Total Store Order**.

Compared with architectures such as ARM or POWER, x86 permits fewer kinds of memory reordering.

For example, x86 normally preserves:

```
Store -> Store
```

ordering.

So code like:

```
price = 100;
ready.store(true, relaxed);
```

may compile into two stores that x86 hardware makes visible in the expected order.

As a result, this may seem to work in millions of tests.

But that's not enough.

There are two major problems.

First, the **compiler** still operates according to the C++ memory model, not merely the x86 hardware model.

Second, the source program has a data race on:

```
price
```

and therefore has undefined behavior according to C++.

So you cannot justify it by saying:

> "It works on x86."

That's a common interview trap.

The correct mindset is:

> First make the program valid according to the C++ memory model. Then understand how the compiler maps that model to a particular architecture.

### Follow-up 7: Why might acquire/release be cheaper than `seq_cst`?

You could write:

```
ready.store(true);
```

and:

```
if (ready.load())
```

because atomic operations default to:

```
std::memory_order_seq_cst
```

That would also be correct.

But `seq_cst` gives stronger guarantees than you actually need.

It creates a single global order over sequentially consistent atomic operations.

Acquire/release only establishes the synchronization needed between relevant producer and consumer operations.

So:

```
ready.store(true, std::memory_order_release);
ready.load(std::memory_order_acquire);
```

can give the necessary correctness with fewer constraints.

On some architectures this can translate to cheaper instructions or fewer fences.

This is particularly relevant in low-latency systems.

That said, don't optimize blindly.

For interview purposes:

> "I would initially use the weakest ordering I can confidently prove correct. If correctness is uncertain, `seq_cst` is safer; I'd weaken it only with a clear memory-model argument and measurement."

### Follow-up 8: Is release needed on the load?

No.

A common mistake is trying:

```
ready.load(std::memory_order_release);
```

That doesn't make conceptual sense and isn't valid for a pure load.

For synchronization:

```
producer                   consumer

store(release)   --->      load(acquire)
```

Remember:

```
publish data    = release
consume data    = acquire
```

For a read-modify-write operation such as:

```
compare_exchange
fetch_add
exchange
```

you may use:

```
std::memory_order_acq_rel
```

because the operation can simultaneously consume previous information and publish new information.

### Follow-up 9: What if the acquire load sees `false`?

Then no synchronization occurs.

For example:

```
if (ready.load(std::memory_order_acquire)) {
    std::cout << price;
}
```

If it reads:

```
false
```

then the body isn't executed, so that's fine.

The release/acquire synchronization relationship exists only when the acquire observes the release store, or an appropriate value in its release sequence.

For this simple flag pattern, just remember:

```
acquire sees released true
        =>
published data is visible
```

### Follow-up 10: What if there are multiple pieces of data?

Acquire/release is especially useful for publishing an entire object or message.

For example:

```
struct Quote {
    int bid;
    int ask;
    int bidSize;
    int askSize;
};

Quote quote;
std::atomic<bool> ready{false};

// Producer
quote.bid = 100;
quote.ask = 101;
quote.bidSize = 20;
quote.askSize = 30;

ready.store(true, std::memory_order_release);
```

Consumer:

```
if (ready.load(std::memory_order_acquire)) {
    process(quote);
}
```

The release/acquire pair publishes **all preceding writes**, not just one specific variable.

That's one reason this pattern is common in message passing.

### A quant-dev relevant example: SPSC queue

This same idea appears in lock-free queues.

A producer might:

```
buffer[index] = message;

tail.store(next,
           std::memory_order_release);
```

The consumer:

```
auto tailValue =
    tail.load(std::memory_order_acquire);
```

If it sees that the producer advanced `tail`, it is guaranteed that the message written into:

```
buffer[index]
```

is visible.

That's why Problem 10 directly connects to Problem 7, the SPSC ring buffer.

---

## Interview answer in 30 seconds

A strong concise answer would be:

> "The original code is not correct because relaxed accesses to `ready` do not synchronize the non-atomic write and read of `price`, so there is a data race on `price`, which is undefined behavior. I would publish with a release store and consume with an acquire load. If the acquire load observes the release store, the write to `price` happens-before the read, making it safe."

Then, if asked why not `seq_cst`:

> "`seq_cst` would also work, but acquire/release provides exactly the ordering needed for this producer-consumer synchronization without imposing a stronger global ordering."

And if asked about x86:

> "The broken version may appear to work on x86 because x86 has a relatively strong hardware memory model, but C++ correctness is defined by the C++ memory model, and the source still has a data race."

# Problem 13 Market-data sequence recovery

Packets arrive with sequence numbers:

```
100
101
103
104
102
105
```

Your application must process:

```
100 101 102 103 104 105
```

Design:

```cpp
void onPacket(uint64_t seq, Message msg);
```

**Follow-ups:** How do you detect gaps? Where do you temporarily store packets 103 and 104? What happens if 102 never arrives? How would a snapshot/recovery feed work? How could you avoid `std::map`? Could a fixed circular array indexed by sequence number work?

**Tests:** feed handlers, sequencing, memory management.

## Solution

Suppose packets arrive:

```
100
101
103
104
102
105
```

but must be processed as:

```
100
101
102
103
104
105
```

A straightforward design is to track the next sequence number we expect and temporarily buffer packets that arrive ahead of it.

```
using SeqNum = uint64_t;

class Sequencer {
public:
    explicit Sequencer(SeqNum firstExpected)
        : expected_(firstExpected) {}

    void onPacket(SeqNum seq, Message msg)
    {
        if (seq < expected_) {
            // Duplicate or already processed packet.
            return;
        }

        if (seq == expected_) {
            process(msg);
            ++expected_;

            drainBuffered();
            return;
        }

        // seq > expected_: gap detected.
        pending_.emplace(seq, std::move(msg));

        requestRecovery(expected_, seq - 1);
    }

private:
    void drainBuffered()
    {
        while (true) {
            auto it = pending_.find(expected_);

            if (it == pending_.end())
                break;

            process(it->second);
            pending_.erase(it);
            ++expected_;
        }
    }

    void process(const Message& msg)
    {
        // Deliver to downstream strategy/order book/etc.
    }

    void requestRecovery(SeqNum begin, SeqNum end)
    {
        // Ask recovery channel for missing packets.
    }

private:
    SeqNum expected_;

    std::map<SeqNum, Message> pending_;
};
```

For the sequence:

```
expected = 100
```

we get:

```
receive 100 -> process, expected = 101
receive 101 -> process, expected = 102

receive 103
    103 > 102
    buffer 103
    gap detected: missing 102

receive 104
    buffer 104

receive 102
    process 102
    expected = 103

    buffered 103 exists -> process
    expected = 104

    buffered 104 exists -> process
    expected = 105

receive 105 -> process
```

The crucial invariant is:

> Never deliver sequence number `N+1` before sequence number `N` unless the protocol explicitly permits it.

### Follow-up 1: How do you detect a gap?

Maintain:

```
SeqNum expected_;
```

For every incoming packet:

```
if (seq == expected_)
```

everything is normal.

If:

```
seq > expected_
```

you have a gap.

For example:

```
expected = 102
received = 105
```

so missing sequences are:

```
102, 103, 104
```

or equivalently the interval:

```
[expected, seq - 1]
```

You can trigger recovery:

```
requestRecovery(102, 104);
```

### Follow-up 2: What if `seq < expected_`?

Usually that means one of:

- duplicate packet
- late packet already recovered
- retransmission
- duplicate delivery from redundant feeds

For example:

```
expected = 110
receive 108
```

You've already processed 108, so normally discard it.

```
if (seq < expected_)
    return;
```

But protocol semantics matter. Some feeds may use resets, retransmission markers, or separate channels, so production code should follow the exchange specification.

### Follow-up 3: Where do you store packets 103 and 104?

The simple answer is:

```
std::map<SeqNum, Message> pending_;
```

because it keeps packets sorted by sequence number.

Then after receiving the missing packet:

```
while (pending_.contains(expected_)) {
    ...
}
```

you drain them in order.

Complexities are roughly: $O(\log G)$ for insert/find, where \(G\) is the number of buffered packets. For an interview, this is a perfectly good first solution.

For a low-latency system, though, `std::map` is often not ideal because of:

- dynamic allocation
- tree traversal
- pointer chasing
- poor cache locality

That leads naturally to the next follow-up.

### Follow-up 4: How would you avoid `std::map`?

If the maximum tolerated reorder/gap window is bounded, use a fixed-size circular array.

For example:

```
static constexpr size_t N = 4096;

struct Slot {
    SeqNum seq = 0;
    bool valid = false;
    Message message;
};

std::array<Slot, N> buffer_;
```

Map sequence numbers to slots with:

```
size_t index = seq % N;
```

Then:

```
void bufferPacket(SeqNum seq, Message msg)
{
    auto& slot = buffer_[seq % N];

    slot.seq = seq;
    slot.message = std::move(msg);
    slot.valid = true;
}
```

To check whether `expected_` is available:

```
auto& slot = buffer_[expected_ % N];

if (slot.valid && slot.seq == expected_) {
    process(slot.message);
    slot.valid = false;
    ++expected_;
}
```

The stored `seq` field is essential.

You can't simply test:

```
slot.valid
```

because sequence numbers eventually wrap around the circular buffer and reuse the same physical slot.

## Why does the circular buffer work?

Suppose:

```
N = 8
```

Then:

```
seq 100 -> slot 4
seq 101 -> slot 5
seq 102 -> slot 6
...
seq 108 -> slot 4
```

The slot is reused.

Therefore we store:

```
struct Slot {
    SeqNum seq;
    bool valid;
    Message message;
};
```

and verify:

```
slot.seq == expected_
```

before consuming it.

This gives: $O(1)$ insertion and lookup. It's also contiguous and allocation-free. That's much more attractive for a feed handler.

### Follow-up 5: What happens if the gap is larger than the circular buffer?

This is an important edge case.

Suppose:

```
buffer size = 4096
expected = 100
receive = 10000
```

You cannot safely store arbitrary packets between them because sequence numbers will collide modulo the ring size.

You need a policy.

Usually:

```
if (seq - expected_ >= buffer_size)
```

then the receiver is too far behind.

At that point you may:

- stop incremental processing
- enter recovery mode
- request a snapshot
- rebuild state
- resume from a known sequence

A good interview answer is:

> "The ring size defines the maximum reorder/recovery window I can support locally. If the gap exceeds it, I'd stop trusting the incremental stream and move into a snapshot/recovery path."

### Follow-up 6: What happens if sequence 102 never arrives?

You cannot buffer forever.

Suppose:

```
receive:
100
101
103
104
105
106
...
```

while:

```
expected = 102
```

All later packets are blocked behind the missing sequence.

You need some combination of:

- recovery request
- timeout
- retry policy
- gap-size threshold
- snapshot fallback

For example:

```
gap detected
        |
        v
request retransmission
        |
        v
wait for missing packets
        |
        +---- recovered -> resume
        |
        +---- timeout / too large
                       |
                       v
                  request snapshot
```

You should not simply skip 102 unless the feed protocol explicitly allows that.

For an order book, doing so could corrupt your state permanently.

### Follow-up 7: Why can skipping one packet be dangerous?

Imagine sequence 102 was:

```
CANCEL order 500
```

and you skip it.

Your order book still believes:

```
order 500 exists
```

Then later messages might execute against or modify state based on an order that should no longer exist.

Or sequence 102 might contain:

```
ADD bid 100.25 x 500
```

Skipping it gives you an incorrect best bid.

So market-data consumers often prefer:

> no data over silently corrupted state.

If the sequence integrity is broken, downstream trading may have to be paused until recovery completes.

### Follow-up 8: How would a retransmission/recovery channel work?

Many market-data systems conceptually have something like:

```
Live multicast feed
      |
      v
Feed Handler
      |
      +------ missing sequence detected
                       |
                       v
              Recovery / retransmit channel
```

Suppose live feed gives:

```
100
101
105
106
```

You detect:

```
missing 102-104
```

and request:

```
RETRANSMIT 102-104
```

Meanwhile live traffic continues:

```
107
108
109
```

Those packets are buffered.

Recovery channel sends:

```
102
103
104
```

Then you process:

```
102
103
104
105
106
107
108
109
```

in order.

The live feed and recovery feed therefore have to merge into one logical sequence stream.

### Follow-up 9: What is snapshot recovery?

Sometimes retransmitting missing packets isn't practical.

For example:

- gap is too large
- recovery server no longer retains old messages
- process started late
- local book state is untrusted
- system restarted

Then you request a current-state snapshot.

For an order book:

```
SNAPSHOT
--------------------------------
all current bid levels
all current ask levels
sequence = 1,000,000
```

You rebuild your book from the snapshot.

Then resume incrementals from:

```
1,000,001
```

Conceptually:

```
incremental stream
     |
     | gap too large
     v
discard/recover old state
     |
     v
load snapshot at sequence S
     |
     v
apply incrementals S+1, S+2, ...
```

### Follow-up 10: What if incremental packets arrive while you're loading the snapshot?

This is a very realistic question.

Suppose you request a snapshot.

While it is arriving, the live feed continues producing:

```
1001
1002
1003
1004
...
```

The snapshot says:

```
snapshot represents state through seq 1000
```

Then you need to buffer incrementals after 1000 while loading the snapshot.

Once the snapshot finishes:

```
load snapshot @ 1000
apply buffered 1001
apply 1002
apply 1003
...
```

This is sometimes called:

```
snapshot + replay
```

You must know the sequence boundary associated with the snapshot.

Otherwise you don't know which incremental events are already reflected in it.

### Follow-up 11: Could you use an `unordered_map` instead of `map`?

Yes.

Because you only need to ask:

```
"Do I have expected_?"
```

you don't necessarily need sorted ordering.

You could do:

```
std::unordered_map<SeqNum, Message> pending_;
```

Then:

```
auto it = pending_.find(expected_);
```

is average: $O(1)$

But compared with a fixed ring buffer it still has potential issues:

- dynamic allocation
- hashing overhead
- unpredictable collision behavior
- rehashing
- poorer memory locality

For a generic implementation:

```
unordered_map
```

is reasonable.

For a latency-sensitive bounded sequence window:

```
fixed circular array
```

is usually more attractive.

### Follow-up 12: How do you avoid sending duplicate recovery requests?

Suppose you receive:

```
expected = 102

receive 105 -> gap 102-104
receive 106 -> gap 102-105
receive 107 -> gap 102-106
```

A naive implementation might send:

```
request 102-104
request 102-105
request 102-106
```

which is wasteful.

Track the recovery range already requested.

For example:

```
SeqNum recoveryBegin_;
SeqNum recoveryEnd_;
bool recoveryOutstanding_;
```

If you've requested:

```
102-104
```

and later receive 106, perhaps request only:

```
105
```

or extend the outstanding range, depending on the recovery protocol.

A robust recovery manager needs to coalesce missing ranges.

### Follow-up 13: How would you represent multiple gaps?

Suppose packets arrive:

```
100
103
105
102
```

Initially:

```
missing 101-102
```

Then after 102 arrives:

```
101 is still missing
```

You could conceptually maintain missing intervals:

```
[101, 101]
[104, 104]
```

But with a strict sequencer you often don't need a sophisticated interval structure.

The most important number is:

```
expected_
```

because nothing after `expected_` may be processed yet.

Buffer all future packets and continue waiting for `expected_`.

This simplifies the design considerably.

### Follow-up 14: What about duplicated packets from two redundant feeds?

This is common in market data.

You may receive identical sequence numbers from two network paths:

```
Feed A: 100 101 102 103
Feed B: 100 101 102 103
```

or with different delays:

```
Feed A: 100 102 103
Feed B: 100 101 102
```

A common architecture is:

```
Feed A -----\
             > sequencer -> downstream
Feed B -----/
```

For each sequence number, use whichever feed delivers the packet first.

Then later duplicates are discarded:

```
if (seq < expected_)
    return;
```

This can reduce packet-loss sensitivity and tail latency.

It's sometimes called **A/B feed arbitration** or redundant-feed merging.

### Follow-up 15: Does sequence number wraparound matter?

Yes, depending on the protocol.

If sequence numbers are:

```
uint32_t
```

then eventually:

```
4294967295
```

wraps to:

```
0
```

Naively comparing:

```
if (seq < expected_)
```

can break around wraparound.

One common technique is modular sequence comparison.

For example, with a 32-bit sequence:

```
bool before(uint32_t a, uint32_t b)
{
    return static_cast<int32_t>(a - b) < 0;
}
```

This works under the assumption that valid sequence differences never exceed half the sequence space.

However, whether you need this depends entirely on the feed specification. Many exchange protocols use sufficiently large sequence types or session resets that avoid practical ambiguity.

In an interview, I'd say:

> "I would follow the feed protocol's defined wrap/reset semantics rather than inventing my own."

### A more production-oriented ring-buffer design

Conceptually:

```
template<size_t N>
class Sequencer {
public:
    void onPacket(uint64_t seq, Message msg)
    {
        if (seq < expected_)
            return;

        if (seq - expected_ >= N) {
            beginSnapshotRecovery();
            return;
        }

        auto& slot = slots_[seq % N];

        if (slot.valid && slot.seq == seq) {
            // duplicate
            return;
        }

        slot.seq = seq;
        slot.msg = std::move(msg);
        slot.valid = true;

        if (seq > expected_)
            onGap(expected_, seq - 1);

        drain();
    }

private:
    void drain()
    {
        for (;;) {
            auto& slot = slots_[expected_ % N];

            if (!slot.valid ||
                slot.seq != expected_)
                break;

            process(slot.msg);

            slot.valid = false;
            ++expected_;
        }
    }

    struct Slot {
        uint64_t seq{};
        bool valid{};
        Message msg;
    };

    uint64_t expected_{};
    std::array<Slot, N> slots_;
};
```

## Strong interview answer

A concise answer would be:

> "I'd maintain the next expected sequence number. If an incoming packet matches it, I process it and then drain any consecutive buffered packets. If the sequence is greater than expected, I've detected a gap, so I buffer the packet and request retransmission for the missing range. If it is less than expected, it's a duplicate or late packet and can normally be discarded. A simple implementation can use a `std::map`, but for a low-latency bounded recovery window I'd use a fixed circular buffer indexed by `seq % N`, storing the actual sequence number in each slot to distinguish wraparound/reuse. If the gap exceeds the local recovery window or cannot be filled, I'd rebuild from a snapshot and replay buffered incrementals after the snapshot sequence."

For a **quant-dev/feed-handler interview**, the three points I would emphasize most are:

1. **Never deliver out of sequence.**
2. **Use a fixed preallocated ring instead of `std::map` when the reorder window is bounded.**
3. **Have a clear transition from packet retransmission to snapshot recovery when incremental recovery is no longer trustworthy.**

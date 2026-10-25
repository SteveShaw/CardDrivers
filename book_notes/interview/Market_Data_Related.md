Prepare around one central idea: **preserve the order needed for correct state, recover missing data explicitly, and scale across independent streams.** Interviewers will usually care more about how you reason through failures and trade-offs than which technologies you name.

Here’s a guide you can rehearse.

### 1. Start by clarifying the problem

Before drawing an architecture, ask:

- Are we building a trading strategy’s feed handler, an order book service, or a market data platform for many clients?
- Is the transport UDP multicast or TCP?
- Are sequence numbers assigned per packet, message, channel, or instrument?
- Are redundant feeds, retransmissions, and snapshots available?
- What are the peak message rate and latency requirements?
- During a gap, should consumers stop, receive stale data with a warning, or continue under a defined policy?

Then state an assumption:

> “I’ll assume UDP market data with sequence numbers per feed channel, redundant feeds, and a recovery service. We need to reconstruct accurate order books with bounded memory and low latency.”

**Protocol details matter.** Nasdaq MoldUDP64 identifies the first **message** sequence number in a packet; a packet may contain multiple messages. CME recovery also involves packet sequence numbers and, for some book recovery methods, per-instrument sequences. Don’t assume every packet advances the sequence by one. [Nasdaq specification](https://nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/moldudp64.pdf), [CME book recovery](https://cmegroupclientsite.atlassian.net/wiki/spaces/EPICSANDBOX/pages/457705188)

### 2. Explain the basic sequencing algorithm

For a simplified stream with one update per sequence number, maintain:

- `expected`: the next sequence to apply.
- A bounded buffer for updates that arrive early.
- A gap timer and recovery state.
- The feed’s session or epoch identifier.

| Incoming sequence       | Action                                                               |
| ----------------------- | -------------------------------------------------------------------- |
| Equal to `expected`     | Apply it, advance `expected`, and drain consecutive buffered updates |
| Greater than `expected` | Buffer it and detect the missing range                               |
| Less than `expected`    | Discard as a duplicate or late update already applied                |
| Different session       | Follow the protocol’s reset/reinitialization procedure               |

For example:

```
Expected: 100
Arrivals: 100, 102, 103, 101

100 → apply; expected becomes 101
102 → buffer; detect missing 101
103 → buffer
101 → apply, then drain 102 and 103
```

The key invariant:

> “Within a sequencing domain, I only apply a contiguous sequence of updates, and I apply each update at most once.”

Use sequence numbers for ordering. Timestamps can help measure latency, but they may be equal, reflect different clocks, or lack an ordering guarantee.

### 3. Handle the harder question: what if 101 never arrives?

**A reorder buffer alone is insufficient.** You need recovery.

1. Check whether the redundant feed supplies the missing update.
2. Allow a bounded opportunity for ordinary reordering, according to the latency budget.
3. Request the missing range from the recovery service.
4. Continue receiving live traffic into bounded storage.
5. Apply recovered updates, then drain the buffered stream.
6. If recovery is unavailable or the buffer limit is exceeded, rebuild from a snapshot.

Coalesce overlapping recovery requests, limit retries, and avoid repeatedly requesting the same range.

A snapshot must have a usable sequence boundary:

```
Snapshot represents state through sequence S
              ↓
Install complete snapshot
              ↓
Apply buffered updates after S in order
              ↓
Resume current-state publication
```

The snapshot’s scope and boundary must match the feed’s recovery rules. CME, for example, provides a last-processed incremental packet sequence to synchronize snapshots with live updates. [CME snapshot recovery](https://cmegroupclientsite.atlassian.net/wiki/spaces/EPICSANDBOX/pages/457673198/MDP%2B3.0%2B-%2BMarket%2BData%2BSnapshot%2B-%2BMBOFD%2Band%2BMBOLD%2BRecovery)

**Important distinction:** a snapshot can restore the current book, but it cannot recreate every missed trade or intermediate event. Consumers that need complete history require replay or another historical source.

### 4. Draw a simple architecture

```
Exchange feeds A/B
        ↓
Validate session and packet structure
        ↓
Deduplicate + sequence per channel ← Recovery service
        ↓
Decode and route ordered updates
        ↓
Order book workers
        ↓
Strategies / pricing / risk / client distribution

Recording and monitoring run alongside these stages
```

Explain that a sequencer has one logical owner per channel. This makes ordering and duplicate suppression easier to reason about.

After establishing channel order, route updates to workers by instrument where the protocol permits independent processing. Preserve order for each instrument and respect any event or transaction boundaries.

### 5. Prepare for the scalability follow-ups

| Interviewer asks                             | Strong response                                                                                                                                          |
| -------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------- |
| “Can we add more threads?”                   | Parallelize independent channels or instruments. Preserve one logical owner for each ordered state stream.                                               |
| “What if one symbol is extremely busy?”      | Give it a dedicated worker or rebalance ownership with a controlled handoff that preserves order.                                                        |
| “How large is the buffer?”                   | Size it from peak arrival rate × tolerated gap duration × stored update size, including overhead and headroom. Enforce hard limits.                      |
| “What if downstream is slow?”                | Use bounded queues and consumer-specific policies. A quote display may coalesce updates; a full-history consumer cannot silently drop them.              |
| “Can we process later updates during a gap?” | Only if the protocol proves the relevant state is unaffected or independently recoverable. Otherwise the missing update may invalidate later processing. |
| “How do A/B feeds help?”                     | Merge equivalent streams by session and sequence. Accept the first valid copy and suppress duplicates; both feeds can still lose the same data.          |
| “Do we need global ordering?”                | Usually ordering is scoped to a feed or instrument. Separate venues generally provide no authoritative shared total order.                               |
| “Can Kafka solve this?”                      | A log can support distribution and replay, but it doesn’t identify missing exchange updates or establish their correct sequence automatically.           |
| “How do we fail over?”                       | Recover a checkpoint and replay from a known boundary; ensure a standby has contiguous state before making it authoritative.                             |

An illustrative sizing calculation:

```
2 million updates/second × 50 ms gap × 64 bytes/update
= 6.4 MB of payload storage
```

Actual capacity needs to include metadata, allocation overhead, bursts, and concurrent gaps.

### 6. Tie correctness to trading decisions

An order book update may add, modify, or delete an order or price level. Applying updates out of order can produce an incorrect book or a modification to an order that hasn’t been added yet.

Expose data quality alongside values:

- **Healthy:** contiguous, current state.
- **Gap detected:** state is valid only through the last contiguous update.
- **Recovering:** rebuilding or catching up.
- **Stale:** freshness exceeds the permitted threshold.

> “I would publish the last applied sequence and freshness state so strategies and risk controls can decide whether the data is safe to use.”

Define which consumers pause or restrict trading during degraded data. That policy depends on the business requirements.

### 7. Extend the discussion to a trading system

For a broader front office design, rehearse this flow:

```
Market data → Books / pricing → Strategy
                                  ↓
                          Pre-trade risk
                                  ↓
                       Order manager / gateway
                                  ↓
                               Exchange
                                  ↓
                     Acknowledgments and fills
                                  ↓
                      Positions / P&L / risk
```

Be ready to discuss:

- **Order state:** pending, acknowledged, partially filled, filled, canceled, rejected.
- **Risk:** quantity, notional, position, credit and price limits; reservations for outstanding orders.
- **Retries:** a timeout leaves order status uncertain. Reconcile before retrying in a way that could create another order.
- **Cancel/fill races:** a fill can arrive while cancellation is pending.
- **Recovery:** rebuild orders, executions and positions from durable records and exchange reconciliation.
- **Controls:** kill switches, audit trails, access controls and stale-data policies.
- **Latency:** separate critical processing from reporting and analytics while preserving required durability.

### 8. Describe how you would verify it

Use deterministic replay with injected failures:

- Reordering, duplicates and missing ranges.
- Loss on one or both redundant feeds.
- Delayed or unavailable recovery.
- Session resets and restart during recovery.
- Snapshot/live-update overlap.
- Burst traffic, buffer exhaustion and slow consumers.

Check that output matches an ordered reference stream, no update is applied twice, memory stays bounded, and invalid state is never presented as healthy.

Measure tail latency, gap duration, queue depth, recovery time and data freshness—not just average throughput.

### A 90-second answer to rehearse

> “First I’d clarify the sequencing domain, whether sequence numbers refer to packets or messages, and what recovery facilities the exchange provides.
> 
> For each channel, I’d maintain the next expected sequence and a bounded reorder buffer. An expected update is applied immediately, later updates are buffered, and already-applied sequences are discarded. After applying an update, I drain any contiguous buffered updates.
> 
> A gap triggers recovery using the redundant feed or retransmission service. I keep receiving live traffic, but I don’t apply updates across an unresolved gap to affected state. If recovery exceeds our time or memory limits, I rebuild from a snapshot with a defined sequence boundary and replay subsequent updates.
> 
> To scale, I partition independent channels and distribute ordered updates to instrument workers while preserving the feed’s event semantics. I expose freshness and recovery status to consumers and test the design through replay with loss, duplication, reordering and bursts.”

# How to arbitrate for feeds A/B which provides same data

Use **one shared sequencer for feeds A and B**. For each sequence number, accept the first valid copy from either feed and discard the duplicate. Both feeds remain active.

First confirm that A/B contain equivalent updates with the **same session and sequence-number scheme**. If their numbering differs, you need the exchange’s defined mapping or event identifiers.

```
Feed A ──┐
         ├── Validate → Shared sequencer → Ordered updates → Book
Feed B ──┘
```

For a simplified protocol with one update per sequence number, maintain `expected` and a bounded buffer:

| Incoming update                | Action                                                                      |
| ------------------------------ | --------------------------------------------------------------------------- |
| `seq == expected`              | Apply, advance `expected`, and drain consecutive buffered updates           |
| `seq > expected`               | Buffer the first valid copy; wait for the missing sequence from either feed |
| `seq < expected`               | Discard because it has already been applied                                 |
| Same sequence already buffered | Discard the duplicate                                                       |

For example:

```
Expected = 100

A: 100 → apply; expected = 101
A: 102 → buffer; missing 101
B: 100 → discard duplicate
B: 101 → apply, then drain buffered 102; expected = 103
B: 102 → discard duplicate
```

**You choose a copy per sequence number, rather than choosing one feed for the whole stream.** A may deliver 100 first, while B supplies 101.

A few details make this interview answer stronger:

- **One owner of sequencing state:** both receivers enqueue to a single sequencer per channel. This avoids races where both copies get applied.
- **Bounded gap handling:** if neither feed supplies the missing sequence within the allowed time, request recovery. Later updates stay buffered.
- **Validate before accepting:** check session, channel, packet structure and protocol integrity requirements.
- **Conflicting copies:** if the same sequence carries different logical content, treat it as a data-integrity fault. A duplicate check can detect this when both copies are retained; detecting it before publication requires waiting, which adds latency.
- **Session resets:** identify updates by `(channel, session, sequence)`, not sequence alone.
- **Multi-message packets:** deduplicate at the protocol’s sequencing unit. If packets contain overlapping message ranges, accept only unseen messages and preserve their order.
- **Monitor each feed separately:** measure gaps, arrival lag and errors even when the combined stream is healthy.

A concise interview response:

> “I’d run A and B active-active into one sequencer per channel. The first valid copy of each sequence wins, duplicates are suppressed, and updates are applied only in contiguous order. Either feed can fill a gap. If both miss it, I use retransmission or snapshot recovery, with bounded buffering and explicit stale-data status.”

# Some Sort Algorithms

```cpp
static inline void mergeSort(vector<int>&nums, ssize_t left, ssize_t right)
   {
       if(left >= right) { return; }
       auto mid = left + (right - left) /2;
       mergeSort(nums, left, mid);
       mergeSort(nums, mid+1, right);
       compCopy(nums, left, mid, right);
   }

   static inline void compCopy(vector<int>& nums, ssize_t left, ssize_t mid, ssize_t right)
   {
       auto l = left;
       auto r = mid + 1;
       auto k = 0;
       vector<int> temp(right - left + 1, 0);

       while(cmp_less_equal(l, mid) && cmp_less_equal(r, right)) {
           if(nums[l] <= nums[r]) {
               temp[k++] = nums[l++];
           } else {
               temp[k++] = nums[r++];
           }
       }

       while(l <= mid) {
           temp[k++] = nums[l++];
       }

       while(r<=right) {
           temp[k++] = nums[r++];
       }

       copy(begin(temp), end(temp), begin(nums) + left);
   }

   static inline int parition(vector<int>& nums, ssize_t left, ssize_t right)
   {
       int randomIndex = left + rand() % (right - left + 1);
       swap(nums[randomIndex], nums[right]);

       auto pivot = nums[right];
       auto i = left;
       for(auto j = left; j< right; ++j) {
           if(nums[j] <= pivot) {
               swap(nums[i], nums[j]);
               ++i;
           }
       }

       swap(nums[i], nums[right]);
       return i;
   }

   static inline int quick_select(vector<int>& nums, int k)
   {
       ssize_t left = 0;
       ssize_t right = ssize(nums) - 1;

       while(left <= right) {
           auto pivot = parition(nums, left, right);
           if(pivot == k) {
               return nums[pivot];
           }

           if(k < pivot) {
               right = pivot - 1;
           } else {
               left = pivot + 1;
           }
       }

       return -1;
   }
```

# Buffer Packets

There’s no fixed packet count. **Choose a maximum waiting time and a memory limit, then size the buffer from peak traffic.**

A useful estimate is:

```
Buffer capacity ≈ peak packets/second × maximum gap waiting time
```

For example, assume:

- Peak rate: **200,000 packets/second**
- Maximum wait for a missing packet: **10 ms**
- Average stored packet size: **1 KB**

You need approximately **2,000 packets**, or **2 MB of payload storage**, plus metadata and burst headroom. These are illustrative numbers; the actual limits depend on your latency requirements and feed behavior.

Suppose you expect packet `101`, but receive `102` onward:

```
101 missing → buffer 102, 103, 104, ...
                    ↓
          Either 101 arrives
          → apply 101 and drain buffer

          Or time/memory limit is reached
          → escalate recovery
```

Use three limits together:

| Limit                          | Purpose                                                          |
| ------------------------------ | ---------------------------------------------------------------- |
| Maximum gap age                | Prevent waiting indefinitely                                     |
| Maximum buffered packets/bytes | Bound memory consumption                                         |
| Maximum sequence distance      | Prevent a far-ahead packet from consuming excessive buffer space |

**Don’t wait until the buffer fills before requesting recovery.** Give feed B a short, measured opportunity to fill the gap, then request retransmission while continuing to buffer live traffic.

If a packet arrives very late:

- **Still needed and in the same session:** accept it and drain consecutive buffered packets.
- **Already applied through another feed or retransmission:** discard it.
- **Superseded by a recovered snapshot:** discard it if it falls at or before the snapshot boundary.
- **From an old session:** discard it.

If recovery cannot finish within the limits, mark affected state stale and use the protocol’s resynchronization procedure. **Simply dropping the oldest buffered packet and continuing can corrupt the order book.**

An interview-ready answer:

> “I’d size a bounded reorder buffer using peak packet rate times the tolerated gap duration, with burst headroom. I’d enforce both byte and time limits, request recovery before those limits are reached, and resynchronize if recovery fails. A late packet is accepted only if its sequence is still needed in the current session.”

# Build From Snapshot

**A snapshot replaces the damaged book with a complete state at a known sequence number. You then apply every update after that point to catch up with the live feed.**

The difficult part is ensuring there is **no gap between the snapshot and the updates you retained**.

### 1. Understand what the snapshot contains

Assume a simplified protocol where each incremental update has a sequence number:

```
Snapshot:
  Instrument: XYZ
  Session: trading-session-1
  Last included sequence: 500
  Complete bids and asks
```

This means:

> “This book already incorporates all relevant updates through sequence 500.”

Therefore, updates `≤ 500` are unnecessary for rebuilding this book. You need `501, 502, 503...`.

Real protocols differ in snapshot scope and sequencing. For example, CME’s MBO snapshots include a last-processed incremental packet sequence and chunk information to synchronize snapshot recovery with the live feed. [CME snapshot specification](https://cmegroupclientsite.atlassian.net/wiki/spaces/EPICSANDBOX/pages/457673198/MDP%2B3.0%2B-%2BMarket%2BData%2BSnapshot%2B-%2BMBOFD%2Band%2BMBOLD%2BRecovery)

### 2. Mark the current book stale and keep receiving

Suppose:

```
Last successfully applied: 100
Missing:                  101
Buffered:                 102–150
Retransmission:           unavailable
```

Stop presenting the affected book as current. You can retain its last known state, but consumers must know it is stale.

**Keep receiving and arbitrating A/B feeds while obtaining the snapshot.** Start or continue bounded storage of live updates before fetching the snapshot; otherwise, updates that occur during the fetch could be lost.

### 3. Build a separate candidate book

Receive the snapshot into a new book rather than modifying the published book piece by piece.

If the snapshot spans multiple packets:

- Collect every required chunk.
- Check that chunks belong to the same instrument, session and snapshot generation, where identified by the protocol.
- Verify completeness using the protocol’s rules.
- Reject an incomplete or inconsistent snapshot.

This prevents consumers from seeing a half-built book.

### 4. Align buffered updates with the snapshot boundary

Suppose the complete snapshot represents state through `140`, and your buffer contains `102–170`.

```
Snapshot through 140
         +
Buffered updates 141–170
         =
Book through 170
```

Discard buffered updates `≤ 140` for that recovered state, then apply `141–170` in order.

But if the buffer contains:

```
141, 142, 144, 145...
          ↑
       143 missing
```

the snapshot has **not** solved the entire problem. You still need `143`, or a newer snapshot whose boundary covers that gap.

The acceptance rule is:

> **Accept a snapshot only when you can establish a complete continuation from its boundary to the handoff point.**

For instrument-level recovery, use the protocol’s instrument sequencing rules; a missing channel packet does not necessarily prove a missing update for that instrument.

### 5. Catch up and publish atomically

Live updates continue arriving while you replay the buffer:

```
Receive snapshot through 140
Replay 141–170
New arrivals: 171–175
Replay 171–175
Switch to live processing
```

Use one logical owner of the recovered book to coordinate the handoff:

1. Install the candidate snapshot.
2. Apply contiguous buffered updates through a chosen handoff sequence `H`.
3. Finish any required event or transaction boundary.
4. Atomically make the candidate book authoritative.
5. Continue with `H + 1` through the same ordered processing path.

That prevents an update from being lost or applied twice during the switch. Mark the book healthy only when it is both consistent and sufficiently fresh.

### 6. What if the original buffer already overflowed?

**Track what was lost. Clearing the buffer does not remove the sequencing gap.**

For example:

```
Old book valid through:       100
Discarded/missing updates:    101–160
Retained contiguous updates:  161–200
```

| Snapshot boundary | Can you recover?             |
| ----------------- | ---------------------------- |
| `150`             | No: you still lack `151–160` |
| `160`             | Yes: replay `161–200`        |
| `180`             | Yes: replay `181–200`        |

You need a snapshot that covers the discarded history and a complete retained continuation after it.

A bounded rolling buffer can retain recent updates while waiting for a sufficiently new snapshot, but **every eviction must invalidate snapshots that would require the evicted updates**.

If the buffer overflows again during rebuilding, abandon that candidate when necessary and try a newer snapshot. If snapshot delivery is too slow, or processing cannot catch up with incoming traffic, repeated retries will not solve the capacity problem. Keep the book stale and address buffer capacity, recovery speed or processing throughput.

### A complete example

```
1. Apply through 100.
2. Lose 101; buffer later updates.
3. Recovery fails; mark book stale.
4. Buffer overflows; retain only 161 onward.
5. Receive complete snapshot through 180.
6. Build candidate book from snapshot.
7. Discard retained updates ≤ 180.
8. Apply 181–220, verifying continuity.
9. Publish candidate through 220.
10. Process 221 onward normally.
```

The missing `101` no longer matters for **current book reconstruction**, because its effect is already represented in the snapshot. It still matters if you need a complete historical event record.

An interview-ready explanation:

> “I keep receiving live data while building a separate book from a complete snapshot. The snapshot gives me a sequence boundary, so I discard updates already represented and replay the contiguous updates after it. If the buffer overflowed, I require a snapshot that covers the lost range. Once the rebuilt book catches up, I switch it into service atomically and resume normal sequencing.”

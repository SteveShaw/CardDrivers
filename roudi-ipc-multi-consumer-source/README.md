# Linux C++23 shared-memory quote IPC: multiple consumers

For RHEL 7 / GCC 12 and older Linux headers, see [RHEL7.md](RHEL7.md). This
revision avoids `sys/random.h`, guards modern syscalls and supplies missing seal
constants. Older kernels can use proc-based liveness and opt-in POSIX shm storage.

This is a separate project derived from `roudi-ipc`; the original project is
unchanged. One `consumer --owner` owns `/roudi`, while any number of
`consumer --reader` processes open it read-only. All consumers independently
receive the same available producer streams, each with its own cursors, FDs and
three symbol-affine reader threads. Up to 64 producers each own a separate memfd and a
Unix-domain `SOCK_SEQPACKET` listening socket. The consumer discovers producers,
requests their FDs using a versioned protocol, maps their regions, and prints
quotes. Three readers partition work by underlying symbol: every AAPL option
stream, from every producer, belongs to the same reader. There are no external
C++ library dependencies. The shared-memory layouts and FD protocol remain v2;
the original v2 producer is compatible. This project adds lifecycle roles without
changing the producer wire format. The original and new owner use the same
singleton lock, so they cannot both own `/roudi` simultaneously.

## Build and demo

The native path uses Linux 5.3+; older kernels use [compatibility fallbacks](RHEL7.md).
Requires readable `/proc` and `/dev/urandom`, GCC 12+ with matching libstdc++,
CMake 3.20+, and Python 3 for the integration test. Run all processes as the same
UID in the same PID, IPC and mount namespaces. Only one registry owner may run;
multiple readers can use `/roudi` concurrently. Its name is fixed as requested.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Without CMake:

```sh
mkdir -p build
g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude src/ipc.cpp src/producer.cpp -o build/producer -lrt
g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -pthread -Iinclude src/ipc.cpp src/registry.cpp src/consumer.cpp -o build/consumer -lrt
python3 tests/smoke.py build
python3 tests/multi_consumer.py build
```

Manual demo, in a shell with no existing `/roudi` application:

```sh
./build/producer --symbols AAPL:AAPL_20261218_C200,MSFT:MSFT_20261218_C400 > producer1.log 2>&1 & p1=$!
./build/producer --symbols AAPL:AAPL_20261218_P200,TSLA:TSLA_20261218_C300 > producer2.log 2>&1 & p2=$!
./build/consumer --reader > reader1.log 2>&1 & r1=$!
./build/consumer --reader > reader2.log 2>&1 & r2=$!
sleep 1
./build/consumer --owner > owner.log 2>&1 & c=$!
sleep 2
kill -KILL "$p1"
sleep 1
kill -TERM "$p2" "$r1" "$r2" "$c"
wait "$p1" "$p2" "$r1" "$r2" "$c" 2>/dev/null || true
cat owner.log
cat reader1.log
```

Expect `ATTACH` and `QUOTE` records for both PIDs in every consumer's log, and `DETACH` for the crashed
producer. `SUBSCRIBE`/`QUOTE` records identify the underlying, full instrument,
source PID and reader ID. Both producers' AAPL contracts go to the same worker;
MSFT and TSLA go to other workers. Worker IDs depend on discovery order.
Ctrl-C or SIGTERM causes orderly shutdown. Every producer publishes to each of
its streams roughly every 10 ms with `price = sequence` and `quantity = sequence * 3`, modulo
32 bits. Socket handshakes can delay this rate. Logging is intentionally simple.

`--symbols` accepts 1..16 comma-separated `UNDERLYING:INSTRUMENT` entries.
For example, `AAPL:AAPL_C1,AAPL:AAPL_C2,MSFT:MSFT_C1` creates three independent
instrument rings, with both AAPL contracts sharing a routing key. The shorthand
`AAPL,MSFT` uses the underlying as the instrument name. No argument defaults to
`AAPL`. Names must use uppercase A-Z, digits, dot, underscore or hyphen, with at
most 31 underlying characters and 63 instrument characters. Duplicate instruments
within one producer are rejected; duplicates across producers are allowed.
These are application-defined identifiers: the demo does not parse exchange
option symbology. Producers must use consistent underlying keys across feeds.

## Owner and reader lifecycle

Mode selection is mandatory: `consumer --owner` or `consumer --reader`. No
arguments or an unknown mode fail before touching `/roudi`.

| Responsibility | Owner consumer | Reader consumer |
|---|---|---|
| Create, initialize, unlink `/roudi` | Yes | Never |
| Registry FD/mapping | O_RDWR / PROT_READ\|PROT_WRITE | O_RDONLY / PROT_READ |
| Registry snapshot lock | Exclusive, with stale-slot cleanup | Shared, copy only |
| Request producer FDs and consume quotes | Yes | Yes |
| Detect producer death, release local mappings | Yes | Yes |
| Acquire lifetime singleton lock | Yes | Never |

Readers reopen `/roudi` by name on every discovery pass (normally about 20 ms,
plus discovery/handshake work). If absent, locked, initializing or incompatible,
they retry; their quote worker threads continue independently. Each successful
snapshot copies records while holding a shared lock, then releases the mapping,
FD and lock before any socket I/O. `Map<const Registry>` enforces a read-only
mapping and const access. Readers never hold an old registry mapping indefinitely.
The owner's scans use its owned FD and an exclusive lock; only that code path
has mutable registry access for clearing stale slots.

On owner crash, the named registry remains. Producers can still register into
available slots and readers can still discover them. Dead producer slots are not
reclaimed until an owner returns. Already connected streams continue regardless
of registry availability, and every consumer detects exits using its own process references.
If the dead registry fills up, new registration waits for owner recovery.

On graceful owner exit, `/roudi` is unlinked. Readers keep consuming existing
producer mappings, while producers and discovery loops wait for the registry to
return. A restarted owner creates a fresh registry with a new epoch. Producers
re-register within their normal retry interval; readers discover the new object
by reopening its name. Epoch changes are logged as `READY ... epoch=...` but do
not reset existing producer mappings or cursors. Producer identity deduplication
prevents replay caused by registry replacement. There is no automatic owner
election or takeover: start a new `--owner` explicitly.

Reader exit or crash only closes that reader's FDs and mappings. It never unlinks
the registry and does not affect other consumers. Fan-out is lossy and independent:
consumers starting at different times, or lagging at different rates, can receive
different retained subsets. A paused reader can overrun its ring without advancing
another reader's cursor. There is no distributed acknowledgement or exactly-once
delivery guarantee.

## Symbol routing and threads

Every consumer has four threads: one discovery thread and three reader
`std::jthread`s (worker IDs 0, 1, 2). The main thread scans the registry, reclaims
slots if it is the owner, and performs socket handshakes and mapping validation. Each reader reads
quotes and detects producer death for its own group. Slow discovery handshakes
do not block reading from already attached producers.

The consumer keeps a process-local `underlying -> {worker, stream count}` table.
A newly discovered underlying goes to the worker with the fewest distinct active
underlyings; ties choose the lowest worker ID. Every subsequent stream for that
underlying, including new contracts and other producer sources, uses the same
worker. Nine distinct underlyings distribute 3/3/3 in the absence of departures.

Routing tables and assignments are private to each consumer. All AAPL streams
use one worker within a consumer, but that worker ID can differ in another
consumer because discovery order can differ. An underlying retains its worker while any of its subscriptions remain active.
The route is removed after its last stream detaches; a later appearance is assigned
again. Consumer restart also rebuilds the table. Existing active routes do not
migrate. This balances distinct symbol counts, not quote rates or CPU load: one
very busy underlying can still dominate a worker. Keeping that underlying on one
thread is intentional. With only one underlying, only one worker has quote work.

A process-local mutex protects group membership while adding, removing, counting,
or copying subscriptions. Each reader snapshots its group's
`shared_ptr<Subscription>` objects and releases the mutex before reading/logging.
Each subscription holds the producer's shared `Connection` (one mapping and FD
pair per producer), a stream index, and a reader-private cursor. A producer
publishing AAPL and MSFT can therefore be read by two workers, each reading only
its assigned instrument rings. There is no dispatcher copying quotes to queues,
and no worker scans or filters other workers' symbol payloads.

On death, each reader removes its subscriptions and logs `UNSUBSCRIBE`. An atomic
remaining-subscription count ensures the last reader logs the producer's global
`DETACH` only after all readers have stopped consuming that producer. Snapshot
ownership keeps its mapping/FDs alive until all outstanding references are gone.
No quote follows global `DETACH`. Per-(producer, instrument) sequence order is
preserved. There is no exchange-time ordering or deduplication across producer
sources or contracts, even when they share one worker.
These pointers and the mutex are ordinary consumer-local objects, never shared
memory fields. `std::osyncstream` keeps log records from all threads intact.

Shutdown requests all stop tokens before joining readers and destroying mappings
and the registry. This also holds during exceptions and partial thread startup.
Each reader has a separate exception slot, read by main after joining. The signal
flag is a lock-free atomic because signals can reach any thread. Logging uses synchronous stdout:
a blocked output destination can delay quote reading and shutdown.

## Registry: ownership and synchronization

`include/ipc.hpp` is the canonical ABI definition. `Registry` is 9,752 bytes:

| Header field | Type | Meaning |
|---|---|---|
| magic/version/bytes/slots | four uint32_t | Validate ABI, version 2, full size, capacity 64 |
| epoch | uint64_t | Nonzero random identity for this consumer instance |
| entries | Slot[64] | Fixed-size producer records |

Each 152-byte `Slot` contains an aligned uint64_t commit flag, a random producer
identity, Linux process start ticks, monotonic heartbeat nanoseconds, int32_t PID,
reserved uint32_t, NUL-terminated 108-byte socket path, and explicit padding.
No heap pointers, strings, mutex implementation objects or process-local handles
are stored in shared memory. Static assertions enforce layout sizes.

Every registry access uses a nonblocking `flock` on its POSIX shm FD: producers
and the owner take an exclusive lock, reader consumers take a shared lock.
Busy participants retry instead of blocking the data path. Each process opens its
own FD; do not share the same open-file description across forked participants.
Linux releases locks when the owning open-file description closes, including on
SIGKILL. This avoids unrecoverable userspace mutex ownership. A writer fills an
uncommitted slot and then atomically publishes `committed=1`. A crash before the
commit leaves a reusable slot; a crash after it leaves a complete record whose
dead process is reclaimed. Heartbeats are informational; interrupted heartbeat
updates cannot corrupt identity or falsely declare death.

Only the owner consumer holds a separate lifetime lock at
`/tmp/roudi-ipc-UID/consumer.lock`. **Never delete this lock file while any instance
is running**: replacing its inode could permit two consumers. The directory is
validated as owned by the current UID with no group/other access. The consumer
unlinks any old `/roudi` and creates a fresh object only after taking its lifetime
lock. It never resizes an old mapped object. On clean exit it unlinks its registry;
on crash the next consumer replaces it. The `/roudi` name is reserved to this
application; other implementations must honor the same ownership lock.

Producers open `/roudi` by name every 200 ms, check its size and header while
locked, and refresh or insert their slot. If it is absent, initializing, busy, or
full they retry while continuing to publish and serve their socket. Reopening
also discovers a new inode after consumer restart. A producer might briefly
register in an already unlinked old object; its next attempt corrects this.
Stale slots are reclaimed by the consumer, not by heartbeat expiry.

## Producer liveness and generations

Each consumer calls `pidfd_open`, then verifies `/proc/PID/stat` start time and
polls the pidfd before accepting a producer. A pidfd refers to the particular
process rather than a reusable PID. The socket peer's UID and PID are checked
with `SO_PEERCRED`; the random 64-bit producer identity must also match the
response and mapped header. Identity, PID and start ticks distinguish process
generations; the random token is not an authentication secret.

If the pidfd syscall is not declared or not implemented, an opened `/proc/PID/stat`
FD plus start-time/state checks replaces it; see [RHEL7.md](RHEL7.md) for the
fallback's behavior and limits. No modern glibc random or memfd wrapper is required.

Readable pidfds cause subscription removal, eventual `munmap`/FD closure after
all local readers release the producer, and slot reclamation by the owner only.
A crash after a liveness check can allow an already in-flight quote
to be printed before the next check; a crash cannot invalidate the consumer's
mapping. A reader stops reading a stream once it detects death; other workers may
still finish in-flight reads until their next check. A stopped or hung
producer is still alive and is not evicted by an arbitrary timeout. Detecting
application hangs would require a separately defined lease policy.

SIGKILL can leave a pathname socket in the private runtime directory. The random
identity in each socket filename prevents rebinding a stale path. Graceful exit
removes the producer's socket; after all processes are stopped an operator can
remove old `p-*.sock` entries. The test removes only its own socket leftovers.

## Quote region and delivery semantics

Each 67,232-byte region contains a 32-byte immutable header and 16 fixed-size
instrument stream slots. The active count is 1..16; unused slots stay zeroed.

| Offset in region | Type | Field |
|---|---|---|
| 0 | four uint32_t | magic, version, bytes, capacity=256 |
| 16 | uint64_t | producer identity |
| 24 | uint32_t | active stream count |
| 28 | uint32_t | reserved=0 |
| 32 | Stream[16] | instrument ring slots |

Each 4,200-byte `Stream` contains `symbol[32]` (underlying), `instrument[64]`
(full contract), an aligned uint64_t published sequence at offset 96, and 256
16-byte cells at offset 104. Names are NUL-terminated and immutable after startup.
The consumer validates all names, count, reserved field and duplicate instruments
before publishing any subscription to a reader. Adding/removing instruments requires
restarting that producer; there is no concurrent stream-directory mutation.

Each cell contains an aligned sequence stamp and packed 64-bit quote (`price` in
the high 32 bits, `quantity` in the low 32 bits). `Quote` remains the original
`{ uint32_t price, quantity; }`; its underlying and contract come from the ring's
immutable metadata rather than being repeated in every message.

Sequences start at 1 independently in every producer/instrument ring.
For sequence N the producer selects `(N-1)%256`, stores
stamp `2*N-1` (busy), the packed payload, stamp `2*N` (complete), then publishes N.
The consumer reads stamp, payload and stamp again, accepting only two matching
`2*N` values. All concurrent cell and publication accesses use lock-free
`atomic_ref<uint64_t>` with sequential consistency. The payload itself is atomic,
so an unsuccessful snapshot does not introduce the data race present in naive
seqlocks over ordinary structs. Sequence exhaustion is detected before wrapping.

This is a bounded, nonblocking **lossy** stream. A slow consumer skips overwritten
quotes and emits a per-stream `DROPPED` count. A newly attached consumer begins at the oldest
currently retained quote; history lost before attachment is not counted. Consumer
restart may replay retained quotes. There is no exactly-once guarantee, persistence,
or producer backpressure. A crash midway through publication cannot cause an
incomplete quote to be accepted. The producer owns the write side; consumers never
modify data. Consumer quote mappings use `Map<const Region>` / `PROT_READ` in
both modes. The transferred producer FD still permits writing, so a malicious
same-UID consumer could remap it writable; this remains a cooperative-peer design.

The native memfd has `F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL`, checked by the consumer,
so its exact verified size cannot change underneath an existing mapping. Its
storage survives producer death until the last mapping/FD is released.

When explicitly permitted with `ROUDI_ALLOW_UNSEALED_SHM=1` in all processes,
unavailable memfd support falls back to immediately unlinked POSIX shm. This
retains FD/mapping lifetime but **cannot enforce a fixed size without seals**;
faulty writable-FD holders could cause SIGBUS by truncating it. Consumer validation
only permits this fallback for expected-size, same-UID, unlinked application
objects on tmpfs. See [RHEL7.md](RHEL7.md) before enabling it.

## FD acquisition protocol v2

Each connection carries one request and one response, then closes. Packet
boundaries come from `SOCK_SEQPACKET`. Both directions use the following exact
40-byte native-endian layout; offsets are part of the ABI:

| Offset | Type | Field |
|---|---|---|
| 0 | uint32_t | magic = 0x524f5544 |
| 4 | uint32_t | version = 2 |
| 8 | uint32_t | bytes = 40 |
| 12 | uint32_t | kind: 1=request, 2=response |
| 16 | uint64_t | producer identity from registry |
| 24 | uint64_t | request nonce, echoed by response |
| 32 | uint32_t | status: request 0; response 0=OK, 1=bad request |
| 36 | uint32_t | region_bytes: request 0; successful response 67232 |

A successful response carries exactly one `SOL_SOCKET/SCM_RIGHTS` FD via
`sendmsg`; a rejected request gets status 1 and no FD. The producer rejects wrong
packet lengths, versions, identities, kinds and request fields. The consumer uses
`recvmsg(MSG_CMSG_CLOEXEC)`, closes all received descriptors on rejection, and
rejects truncation, extra FDs, mismatched nonce/identity, incorrect response fields,
wrong file size, missing seals and incompatible region headers. Handshakes have
100 ms waits and are retried on later scans; one client is served per producer
iteration. No registry lock is held during socket I/O.

## Scope and tests

This is Linux-specific, same-build/same-endian ABI IPC, using the Linux GCC/Clang
lock-free shared-memory atomic convention; ISO C++ alone does not define
interprocess atomics. Unsupported layouts/atomic widths fail at compile time.
It assumes cooperative processes of the same UID, a common `/proc` view, no
forking after startup, and no malicious writes to registry or region memory.
Resource exhaustion fails visibly; connection failures retry. The example uses
polling and synchronous stdout output rather than a high-throughput event loop.

Both Python tests refuse to run over an existing `/roudi` and run serially under CTest.
`tests/smoke.py` runs the prior suite with explicit owner mode. It checks early and late
producer startup, nine underlyings split 3/3/3 across four consumer threads,
AAPL affinity across producers 1/3/5, several contracts for one underlying,
mixed-symbol producers, one mapping per producer, shared-mapping cleanup,
symbol affinity and assignment after exits, per-stream ordering, singleton ownership,
registry-lock holder death with an incomplete record, reader progress during
stalled discovery handshakes, actual FD transfer,
malformed request rejection, producer pause, ring overwrite,
SIGKILL and graceful producer exit, slot reclamation, consumer crash and clean
restart, re-registration and quote integrity. The test controls and kills only
processes it started. Run it in an isolated namespace if another application owns
`/roudi`.

`tests/multi_consumer.py` checks readers/producers before owner startup, fan-out
of identical sequence numbers to multiple consumers, sole owner enforcement,
read-only quote mappings, reader-local lag/overrun, owner SIGKILL, no stale-slot
mutation by readers, discovery while the dead owner's registry survives, fresh
registry discovery after owner restart, cursor preservation across epochs,
graceful owner exit, reader exit/crash isolation, and a late reader. It also
verifies symbol affinity and per-stream increasing sequences for each consumer.

`tests/compatibility.cpp` checks proc-based liveness, generation mismatch,
stopped/zombie/reaped processes and explicit acceptance/rejection of the
unsealed storage fallback. CMake builds it as `compatibility_test`.

Linux API references: [flock](https://man7.org/linux/man-pages/man2/flock.2.html),
[pidfd_open](https://man7.org/linux/man-pages/man2/pidfd_open.2.html),
[memfd_create](https://man7.org/linux/man-pages/man2/memfd_create.2.html), and
[Unix sockets / SCM_RIGHTS](https://man7.org/linux/man-pages/man7/unix.7.html).

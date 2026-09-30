# Linux C++23 shared-memory quote IPC

One consumer owns `/roudi`; up to 64 producers each own a separate memfd and a
Unix-domain `SOCK_SEQPACKET` listening socket. The consumer discovers producers,
requests their FDs using a versioned protocol, maps their regions, and prints
quotes. There are no external C++ library dependencies.

## Build and demo

Requires Linux 5.3+, a mounted `/proc`, GCC 13+ (or an equivalent C++23 compiler),
CMake 3.20+, and Python 3 for the integration test. Run all processes as the same
UID in the same PID, IPC and mount namespaces. Only one instance of this demo can
use `/roudi` at a time. This name is fixed as requested.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Without CMake:

```sh
mkdir -p build
g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude src/ipc.cpp src/producer.cpp -o build/producer -lrt
g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude src/ipc.cpp src/consumer.cpp -o build/consumer -lrt
python3 tests/smoke.py build
```

Manual demo, in a shell with no existing `/roudi` application:

```sh
./build/producer > producer1.log 2>&1 & p1=$!
./build/producer > producer2.log 2>&1 & p2=$!
sleep 1
./build/consumer > consumer.log 2>&1 & c=$!
sleep 2
kill -KILL "$p1"
sleep 1
kill -TERM "$p2" "$c"
wait "$p1" "$p2" "$c" 2>/dev/null || true
cat consumer.log
```

Expect `ATTACH` and `QUOTE` records for both PIDs and `DETACH` for the crashed
producer. Ctrl-C or SIGTERM causes orderly shutdown. Every producer publishes
roughly every 10 ms with `price = sequence` and `quantity = sequence * 3`, modulo
32 bits. Socket handshakes can delay this rate. Logging is intentionally simple.

## Registry: ownership and synchronization

`include/ipc.hpp` is the canonical ABI definition. `Registry` is 9,752 bytes:

| Header field | Type | Meaning |
|---|---|---|
| magic/version/bytes/slots | four uint32_t | Validate ABI, version 1, full size, capacity 64 |
| epoch | uint64_t | Nonzero random identity for this consumer instance |
| entries | Slot[64] | Fixed-size producer records |

Each 152-byte `Slot` contains an aligned uint64_t commit flag, a random producer
identity, Linux process start ticks, monotonic heartbeat nanoseconds, int32_t PID,
reserved uint32_t, NUL-terminated 108-byte socket path, and explicit padding.
No heap pointers, strings, mutex implementation objects or process-local handles
are stored in shared memory. Static assertions enforce layout sizes.

Every registry access uses an exclusive, nonblocking `flock` on its POSIX shm FD.
Busy participants retry instead of blocking the data path. Each process opens its
own FD; do not share the same open-file description across forked participants.
Linux releases locks when the owning open-file description closes, including on
SIGKILL. This avoids unrecoverable userspace mutex ownership. A writer fills an
uncommitted slot and then atomically publishes `committed=1`. A crash before the
commit leaves a reusable slot; a crash after it leaves a complete record whose
dead process is reclaimed. Heartbeats are informational; interrupted heartbeat
updates cannot corrupt identity or falsely declare death.

The consumer holds a separate lifetime lock at
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

The consumer calls `pidfd_open`, then verifies `/proc/PID/stat` start time and
polls the pidfd before accepting a producer. A pidfd refers to the particular
process rather than a reusable PID. The socket peer's UID and PID are checked
with `SO_PEERCRED`; the random 64-bit producer identity must also match the
response and mapped header. Identity, PID and start ticks distinguish process
generations; the random token is not an authentication secret.

Readable pidfds cause connection removal, `munmap`, FD closure and slot
reclamation. A crash after a liveness check can allow an already in-flight quote
to be printed before the next check; a crash cannot invalidate the consumer's
mapping. No further reads are made after death is detected. A stopped or hung
producer is still alive and is not evicted by an arbitrary timeout. Detecting
application hangs would require a separately defined lease policy.

SIGKILL can leave a pathname socket in the private runtime directory. The random
identity in each socket filename prevents rebinding a stale path. Graceful exit
removes the producer's socket; after all processes are stopped an operator can
remove old `p-*.sock` entries. The test removes only its own socket leftovers.

## Quote region and delivery semantics

Each 4,128-byte region contains a 32-byte immutable/header-publication area and
256 cells. Header fields are magic/version/bytes/capacity, producer identity and
an aligned published sequence. Each 16-byte cell contains an aligned sequence
stamp and a packed 64-bit quote (`price` in the high 32 bits, `quantity` in the
low 32 bits). The public message is `Quote { uint32_t price, quantity; }`.

Sequences start at 1. For sequence N the producer selects `(N-1)%256`, stores
stamp `2*N-1` (busy), the packed payload, stamp `2*N` (complete), then publishes N.
The consumer reads stamp, payload and stamp again, accepting only two matching
`2*N` values. All concurrent cell and publication accesses use lock-free
`atomic_ref<uint64_t>` with sequential consistency. The payload itself is atomic,
so an unsuccessful snapshot does not introduce the data race present in naive
seqlocks over ordinary structs. Sequence exhaustion is detected before wrapping.

This is a bounded, nonblocking **lossy** stream. A slow consumer skips overwritten
quotes and emits a `DROPPED` count. A newly attached consumer begins at the oldest
currently retained quote; history lost before attachment is not counted. Consumer
restart may replay retained quotes. There is no exactly-once guarantee, persistence,
or producer backpressure. A crash midway through publication cannot cause an
incomplete quote to be accepted. The producer owns the write side; consumers never
modify data. The transferred FD and mapping are read/write in this trusted-peer
demo, so this is a convention, not a security boundary.

The memfd has `F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL`, checked by the consumer,
so its exact verified size cannot change underneath an existing mapping. Its
storage survives producer death until the last mapping/FD is released.

## FD acquisition protocol v1

Each connection carries one request and one response, then closes. Packet
boundaries come from `SOCK_SEQPACKET`. Both directions use the following exact
40-byte native-endian layout; offsets are part of the ABI:

| Offset | Type | Field |
|---|---|---|
| 0 | uint32_t | magic = 0x524f5544 |
| 4 | uint32_t | version = 1 |
| 8 | uint32_t | bytes = 40 |
| 12 | uint32_t | kind: 1=request, 2=response |
| 16 | uint64_t | producer identity from registry |
| 24 | uint64_t | request nonce, echoed by response |
| 32 | uint32_t | status: request 0; response 0=OK, 1=bad request |
| 36 | uint32_t | region_bytes: request 0; successful response 4128 |

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

`tests/smoke.py` refuses to run over an existing `/roudi`. It checks early and late
producer startup, three simultaneous producers, singleton consumer ownership,
registry-lock holder death with an incomplete record, actual FD transfer,
malformed request rejection, producer pause, ring overwrite,
SIGKILL and graceful producer exit, slot reclamation, consumer crash and clean
restart, re-registration and quote integrity. The test controls and kills only
processes it started. Run it in an isolated namespace if another application owns
`/roudi`.

Linux API references: [flock](https://man7.org/linux/man-pages/man2/flock.2.html),
[pidfd_open](https://man7.org/linux/man-pages/man2/pidfd_open.2.html),
[memfd_create](https://man7.org/linux/man-pages/man2/memfd_create.2.html), and
[Unix sockets / SCM_RIGHTS](https://man7.org/linux/man-pages/man7/unix.7.html).

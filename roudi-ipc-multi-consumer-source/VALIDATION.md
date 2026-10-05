# Validation: separate owner/reader project

Updated and validated on October 3, 2026 in Ubuntu under WSL with GCC 13.3.0.
Both executables compiled as
C++23 with `-O2 -Wall -Wextra -Wpedantic -Werror`; the consumer also uses
`-pthread`. Compilation completed without diagnostics. The direct compiler
commands in README.md were used; CMake/CTest was not executed.

## Older-kernel compatibility validation

The reported deployment target is GCC 12 and kernel `3.10.0-1160`. That exact
compiler/kernel was not available locally. The following passed on WSL:

| Build | Checks | Result |
|---|---|---|
| Normal modern build | Existing smoke and multi-consumer lifecycle suites below | Passed |
| `-DROUDI_DISABLE_MODERN_SYSCALLS=1`, `ROUDI_ALLOW_UNSEALED_SHM=1` | Same two complete integration suites, using proc liveness and POSIX shm | Passed |
| C++ compatibility test | Process generation mismatch, stopped/zombie/reaped child, pinned proc FD, explicit fallback opt-in, read-only mapping reads | Passed |

The no-modern-syscall build does not compile references to `SYS_pidfd_open` or
`SYS_memfd_create`. No source includes `sys/random.h` or calls the glibc memfd
wrapper. Unit output:

```text
PASS process generation, stopped/zombie/reaped child, pinned proc FD, explicit unsealed opt-in and shared data
```

This exercises the fallback implementation but does not prove compatibility with
every RHEL backport or its system libraries. See RHEL7.md for target build/run
instructions and the loss of enforced size seals in the opt-in storage fallback.

The original `roudi-ipc` directory and `roudi-ipc-source.zip` were preserved:
all 12 pre-existing file SHA-256 hashes and the original file count matched after
building and testing this separate project. No original binaries were rebuilt.

## Existing behavior regression suite

`python3 tests/smoke.py build` completed with exit code 0:

```text
PASS producer-first startup, 3 producers, singleton consumer, killed registry writer
PASS nine underlyings balanced 3/3/3; AAPL from producers 1/3/5 shares one reader
PASS multiple contracts per underlying, mixed-symbol producer, one mapping per producer
PASS reader threads deliver quotes during stalled discovery
PASS FD transfer and rejection of wrong version, identity, packet size
PASS stopped producer remains attached; slow-consumer overrun detected
PASS SIGKILL and graceful exit detach mappings and reclaim slots
PASS existing underlying retains affinity; new underlying uses least-loaded reader
PASS consumer restart, quote integrity, single-reader ownership and ordering
ALL SMOKE TESTS PASSED
```

## Multiple-consumer lifecycle suite

`python3 tests/multi_consumer.py build` completed with exit code 0:

```text
PASS readers/producers start before owner; singleton owner; fan-out; read-only quote mappings
PASS slow reader does not stall owner or other reader
PASS owner crash preserves reading/discovery; readers detect deaths without mutating slots
PASS owner replacement/re-registration; reader cursors and mappings preserved
PASS graceful owner exit leaves existing streams usable; absent registry retries
PASS reader exit/crash does not unlink registry; owner alone cleans up
PASS same quotes delivered independently; symbol affinity and sequence integrity across owner restarts
ALL MULTI-CONSUMER TESTS PASSED
```

These are functional integration tests, not a throughput benchmark, exhaustive
concurrency proof, malicious-peer audit, or cross-architecture validation.

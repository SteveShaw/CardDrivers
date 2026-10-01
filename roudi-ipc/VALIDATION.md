# Validation performed

## Current revision: three readers

Successfully rebuilt both executables and ran the expanded smoke suite in Ubuntu
under WSL on September 30, 2026. The previously blocked validation is complete.
The available environment uses GCC 13.3.0 and Python 3.12.3.
Both producer and consumer compiled as C++23 with `-O2 -Wall -Wextra
-Wpedantic -Werror`, with no diagnostics. The consumer additionally uses `-pthread`.

The final integration test completed with exit code 0:

```text
PASS producer-first startup, 3 producers, singleton consumer, killed registry writer
PASS nine producers evenly assigned to three reader threads
PASS reader threads deliver quotes during stalled discovery
PASS FD transfer and rejection of wrong version, identity, packet size
PASS stopped producer remains attached; slow-consumer overrun detected
PASS SIGKILL and graceful exit detach mappings and reclaim slots
PASS new producer uses a less-loaded reader after exits
PASS consumer restart, quote integrity, single-reader ownership and ordering
ALL SMOKE TESTS PASSED
```

CMake was unavailable during the earlier environment check; this validation used
the direct compiler commands documented in README.md. The CMake/CTest entry
points are supplied but were not executed. No claim of exhaustive concurrency,
hostile-peer, PID-reuse stress, sanitizer, or cross-architecture testing is made.

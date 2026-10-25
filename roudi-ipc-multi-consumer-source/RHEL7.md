# RHEL 7 / GCC 12 compatibility

This update applies to the separate `roudi-ipc-multi-consumer` project. The
original `roudi-ipc` project is unchanged. Shared layouts and protocol remain v2.
The reported target is GCC 12 with kernel `3.10.0-1160`.

## Fixes

Missing headers/constants and missing kernel features are separate problems.
Defining a constant does not implement its system call.

| Original dependency | Updated implementation |
|---|---|
| `<sys/random.h>` / `getrandom()` | `/dev/urandom`, handling interruptions and short reads |
| `SYS_pidfd_open` | Compile-time guard and runtime fallback to a pinned `/proc/PID/stat` FD |
| `F_ADD_SEALS`, `F_GET_SEALS`, `F_SEAL_*` | Missing stable Linux UAPI constants supplied in `include/linux_compat.hpp` |
| glibc `memfd_create()` / `MFD_*` | Guarded direct syscall plus missing flag constants |
| No sealed memfd support | Opt-in, immediately unlinked POSIX shared-memory fallback |

The macro is **`SYS_pidfd_open`**, not `Sys_pidfd_open`. No architecture-specific
syscall numbers are hardcoded. If headers do not expose a syscall macro, the
fallback is used, even if a kernel happens to backport that syscall.

Upstream introduced pidfd_open in Linux 5.3, memfd_create in Linux 3.17 (glibc
wrapper in 2.27), and getrandom in Linux 3.17 (glibc wrapper in 2.25). Enterprise
kernels can backport features, so the program checks actual availability instead
of assuming it from the kernel version alone.

Sources: [pidfd_open](https://man7.org/linux/man-pages/man2/pidfd_open.2.html),
[memfd_create](https://man7.org/linux/man-pages/man2/memfd_create.2.html),
[getrandom](https://man7.org/linux/man-pages/man2/getrandom.2.html),
[Linux seal constants](https://github.com/torvalds/linux/blob/master/include/uapi/linux/fcntl.h).

## Build with GCC 12

GCC 12 with its matching libstdc++ supports the features used here. The old stock
RHEL 7 compiler is insufficient. Build on the target with your GCC 12 toolchain;
do not copy the Ubuntu-built binaries to an older glibc installation.

```sh
mkdir -p build
g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude src/ipc.cpp src/producer.cpp -o build/producer -lrt
g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror -pthread -Iinclude src/ipc.cpp src/registry.cpp src/consumer.cpp -o build/consumer -lrt
```

CMake 3.20+ is optional if using those commands. Python 3 runs the smoke tests.

## Run

The default remains sealed memfd storage. If unavailable, the producer gives an
explicit error instead of silently reducing protection. To permit the fallback,
set this in **every producer and consumer environment**, including other shells
or service configurations:

```sh
export ROUDI_ALLOW_UNSEALED_SHM=1
./build/consumer --owner
# Other terminals: export the same variable, then start:
./build/consumer --reader
./build/producer --symbols AAPL:AAPL_C1,MSFT:MSFT_C1
```

The variable permits the fallback; native sealed memfd remains preferred when
usable. A consumer without it rejects unsealed fallback FDs. Proc-based liveness
needs no opt-in and is selected automatically if pidfds are unavailable.

## Liveness without pidfds

The fallback opens `/proc/PID/stat` for each process reference, verifies the
registered start ticks, and keeps the FD open. `pread` avoids sharing a file
offset between symbol workers. The pinned proc inode does not change to a new
process when its numeric PID is reused. Zombie/dead state, disappearance or a
start-time mismatch means the producer has exited. SIGSTOP is not death.
Socket credentials and random generation identity checks remain in place.

This is slower than pidfd polling and requires readable `/proc` in a common PID
namespace. The supplied producer is single-threaded. A future multithreaded
producer whose main thread exits before its other threads needs a different
proc liveness policy. Tests cover stopped, zombie and reaped children and
start-time mismatches; they do not force actual PID reuse.

## Storage guarantees

The fallback creates a unique 0600 object `/roudi-quotes-UID-RANDOM`, immediately
unlinks it, sizes it once and transfers its FD via SCM_RIGHTS. `/dev/shm` must be
tmpfs. FD/mapping lifetime across producer crashes is unchanged. A crash in the
small window between creation and unlink can leave a named object behind.

**Without seals, the kernel cannot prevent a writable FD holder from truncating
the region.** This implementation never resizes it after initialization. A
faulty/malicious peer could still shrink it and cause SIGBUS in readers; that is
why fallback storage requires explicit opt-in. Use a kernel with memfd/sealing
if enforced fixed size is mandatory. All consumer mappings remain PROT_READ.

Fallback FD checks require the expected size, same UID, unlinked tmpfs storage
and this application's `/dev/shm/roudi-quotes-UID-` path prefix. They do not isolate
the application from malicious same-UID processes. The default sealed path still
rejects descriptors lacking the required seals.

## Exercise legacy paths on a newer system

Set `ROUDI_FORCE_LEGACY=1` with `ROUDI_ALLOW_UNSEALED_SHM=1` to force proc/shm
fallbacks at runtime. To compile without modern syscall references, use
`-DROUDI_DISABLE_MODERN_SYSCALLS=1` or the corresponding CMake option:

```sh
cmake -S . -B build-legacy -DROUDI_DISABLE_MODERN_SYSCALLS=ON
cmake --build build-legacy -j
ROUDI_ALLOW_UNSEALED_SHM=1 ctest --test-dir build-legacy --output-on-failure
```

The native and no-modern-syscall builds have been tested on Ubuntu/WSL. This
exercises fallback logic but is not execution on kernel `3.10.0-1160`; validate
on the deployment machine as well.

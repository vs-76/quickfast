# Building QuickFAST (CMake)

Native library, examples, and tests use **C++23** and do not depend on Boost.
First-party code is always compiled with `-Wall -Werror -pedantic`.

## Prerequisites

```bash
sudo apt-get install -y cmake build-essential libxerces-c-dev
```

Optional compilers / libc++ / coverage (examples below use the packaged names on this tree):

```bash
sudo apt-get install -y g++-16 clang++-22 libc++-dev libc++abi-dev gcovr
```

CMake fetches standalone [Asio](https://github.com/chriskohlhoff/asio) and
[GoogleTest](https://github.com/google/googletest) on first configure.
With `-DQUICKFAST_USE_SPDLOG=ON` it also finds or fetches
[spdlog](https://github.com/gabime/spdlog).

| CMake option | Default | Meaning |
| --- | --- | --- |
| `QUICKFAST_BUILD_TESTS` | `ON` | Build `QuickFASTTest` and register ctest |
| `QUICKFAST_BUILD_EXAMPLES` | `ON` | Build example applications |
| `QUICKFAST_USE_LIBCXX` | `OFF` | Use LLVM libc++ (`-stdlib=libc++`); **Clang/AppleClang only** |
| `QUICKFAST_ENABLE_COVERAGE` | `OFF` | Instrument library + tests with `--coverage`; add `coverage` target if `gcovr` is installed |
| `QUICKFAST_SANITIZE_ADDRESS` | `OFF` | AddressSanitizer (`-fsanitize=address`) |
| `QUICKFAST_SANITIZE_UNDEFINED` | `OFF` | UndefinedBehaviorSanitizer (`-fsanitize=undefined`) |
| `QUICKFAST_SANITIZE_THREAD` | `OFF` | ThreadSanitizer (`-fsanitize=thread`; incompatible with ASan/UBSan) |
| `QUICKFAST_USE_SPDLOG` | `OFF` | Build `SpdlogLogger` + `managed_file_sink_mt` (find/FetchContent spdlog; requires zlib, tzdata) |
| `QUICKFAST_ENABLE_TEST_HOOKS` | follows `QUICKFAST_BUILD_TESTS` | Compile `managed_file_sink_mt` test-only hooks into the library; keep `OFF` for shipping builds |
| `QUICKFAST_ENABLE_PVS_STUDIO` | `ON` | Create `pvs-studio` target if `pvs-studio-analyzer` is installed |
| `CMAKE_BUILD_TYPE` | (unset) | Prefer `Release` or `Debug` |
| `CMAKE_CXX_COMPILER` | system default | e.g. `g++-16`, `clang++-22` |

Set `QUICKFAST_ROOT` to the source tree when running tests outside ctest’s
discovered environment:

```bash
export QUICKFAST_ROOT="$(pwd)"
```

---

## Default compiler (system `c++`)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Outputs: `build/lib/libQuickFAST.so`, tests/examples under `build/bin/`.

---

## g++

### Release (tests + examples)

```bash
cmake -S . -B build-gcc \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++
cmake --build build-gcc -j
ctest --test-dir build-gcc --output-on-failure
```

### Specific version (e.g. g++ 16)

```bash
cmake -S . -B build-gcc16 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++-16
cmake --build build-gcc16 -j
ctest --test-dir build-gcc16 --output-on-failure
```

### Debug

```bash
cmake -S . -B build-gcc-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=g++-16
cmake --build build-gcc-debug -j
```

### Library only (no tests, no examples)

```bash
cmake -S . -B build-gcc-lib \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++-16 \
  -DQUICKFAST_BUILD_TESTS=OFF \
  -DQUICKFAST_BUILD_EXAMPLES=OFF
cmake --build build-gcc-lib -j
```

`QUICKFAST_USE_LIBCXX=ON` with g++ is rejected at configure time.

---

## clang++

### Release with default stdlib (libstdc++ on Linux)

```bash
cmake -S . -B build-clang \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++
cmake --build build-clang -j
ctest --test-dir build-clang --output-on-failure
```

### Specific version (e.g. clang++ 22)

```bash
cmake -S . -B build-clang22 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++-22
cmake --build build-clang22 -j
ctest --test-dir build-clang22 --output-on-failure
```

### clang++ + libc++

```bash
cmake -S . -B build-clang22-libcxx \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++-22 \
  -DQUICKFAST_USE_LIBCXX=ON
cmake --build build-clang22-libcxx -j
ctest --test-dir build-clang22-libcxx --output-on-failure
```

Note: system Xerces-C may still pull in `libstdc++` at load time; QuickFAST
and GoogleTest translation units are built against libc++ when this option is on.

### Debug + libc++

```bash
cmake -S . -B build-clang-libcxx-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=clang++-22 \
  -DQUICKFAST_USE_LIBCXX=ON
cmake --build build-clang-libcxx-debug -j
```

### Library only

```bash
cmake -S . -B build-clang-lib \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++-22 \
  -DQUICKFAST_BUILD_TESTS=OFF \
  -DQUICKFAST_BUILD_EXAMPLES=OFF
cmake --build build-clang-lib -j
```

---

## Dual-compiler smoke check

```bash
cmake -S . -B build-gcc16 -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-16
cmake --build build-gcc16 -j && ctest --test-dir build-gcc16 --output-on-failure

cmake -S . -B build-clang22 -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++-22
cmake --build build-clang22 -j && ctest --test-dir build-clang22 --output-on-failure

cmake -S . -B build-clang22-libcxx -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++-22 -DQUICKFAST_USE_LIBCXX=ON
cmake --build build-clang22-libcxx -j && ctest --test-dir build-clang22-libcxx --output-on-failure
```

---

## Useful extras

Parallel build with an explicit job count:

```bash
cmake --build build-gcc16 -j"$(nproc)"
```

Reconfigure an existing build directory (same `-B` path):

```bash
cmake -S . -B build-clang22 -DQUICKFAST_BUILD_EXAMPLES=OFF
cmake --build build-clang22 -j
```

Clean rebuild: remove the build directory and configure again.

```bash
rm -rf build-gcc16
```

---

## PVS-Studio

If [PVS-Studio](https://pvs-studio.com/) is installed (`pvs-studio-analyzer` and
`plog-converter` on `PATH`), configure creates an optional target that analyzes
the `QuickFAST` library (FetchContent deps and `src/DotNet` are excluded):

```bash
cmake -S . -B build-gcc16 -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-16
cmake --build build-gcc16 -j --target QuickFAST   # build first
cmake --build build-gcc16 --target pvs-studio
# report: build-gcc16/pvs-studio.log
```

Disable the target even when tools are present:

```bash
cmake -S . -B build-gcc16 -DQUICKFAST_ENABLE_PVS_STUDIO=OFF
```

A license file is typically expected at `~/.config/PVS-Studio/PVS-Studio.lic`.

---

## Optional spdlog logger adapter

When `-DQUICKFAST_USE_SPDLOG=ON`, QuickFAST builds:
- `Common::SpdlogLogger` — `Common::Logger` → application `spdlog::logger`
- `Common::managed_file_sink_mt` — rotating/compressing file sink on a shared
  `asio::io_context` (zlib required)

Core codecs/communication stay on the `Logger` interface; inject the adapter via
your message consumer and/or `Communication::AsioService::setLogger`.

Requires system **tzdata** for IANA zones (`std::chrono` tzdb).

```bash
cmake -S . -B build-spdlog -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++-16 \
  -DQUICKFAST_USE_SPDLOG=ON -DQUICKFAST_BUILD_EXAMPLES=OFF
cmake --build build-spdlog -j --target QuickFASTTest
ctest --test-dir build-spdlog --output-on-failure
```

### Console logger

```cpp
#include <Common/SpdlogLogger.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

auto lg = spdlog::stdout_color_mt("quickfast");
lg->set_level(spdlog::level::info);
QuickFAST::Common::SpdlogLogger qfLog(lg);
```

### Managed file sink (size + midnight, gzip, retention)

Shares QuickFAST’s standalone Asio `io_context` for hard midnight rotation without
traffic. Defaults are **8 MiB** per file within a **32 MiB** budget of managed log
bytes, i.e. four generations. Does **not** install its own `signal_set` — call
`request_shutdown()` from the app handler.

Under `RetentionMode::ManagedBytes`, `max_file_bytes` must not exceed
`max_managed_bytes`, and the constructor rejects a config that does. Retention
never evicts the active file, so a larger per-file limit would let the active file
alone overrun the budget with nothing left to reclaim. Raise both together to keep
more history. The limit is not enforced under `FilesystemFreePercent`, which does
not use the budget.

Rotated files are named `<stem>.YYYY-MM-DD_HHMMSS[.N].<ext>`, gaining `.gz` once
compressed; `.N` disambiguates repeated rotations inside one second.

`request_shutdown()` is idempotent and blocks until an in-flight rotation timer
handler has returned, so the sink may be destroyed while the shared `io_context`
keeps running. Beyond shutdown, the sink exposes `rotate_now()` (on-demand
rotation, e.g. from a `SIGHUP` handler), `wait_for_compression_idle(timeout)`,
and `managed_bytes()`.

Scheduling is DST-safe. The rotation time is resolved with
`std::chrono::choose::latest`, because a local time such as midnight does not
exist on one day a year in zones that shift the clock at 24:00
(`America/Santiago`, `America/Havana`, `Asia/Beirut`), and the two-argument
`zoned_time` throws on such a time. A gap resolves to the instant the offset
changes; an ambiguous time resolves to the later of its two instants. See
`managed_file_sink_mt::next_rotation_after()`, which is public and testable.

Because the rotation timer runs on an `io_context` the application also uses for
its feed handlers, the handler never lets an exception escape — that would
propagate out of `io_context::run()` and take the thread down. Failures are
counted in `rotation_failures()`, and `rotation_schedule_lost()` becomes true if
a failure also prevented the timer being re-armed (size-based rotation keeps
working; scheduled rotation stops until restart). Both are worth alerting on.

The `FilesystemFreePercent` policy needs free space to be queryable (`statvfs`
on POSIX, `GetDiskFreeSpaceEx` on Windows); when the query fails the policy is
treated as not triggered rather than evicting on a guess.

```cpp
#include <Common/ManagedFileSink.h>
#include <Common/SpdlogLogger.h>
#include <Communication/AsioService.h>
#include <spdlog/logger.h>
#include <asio.hpp>

QuickFAST::Communication::AsioService asioService; // or app-owned io_context

auto cfg = QuickFAST::Common::ManagedFileSinkConfigBuilder()
  .base_path("logs/quickfast.log")
  .max_file_bytes(100ull << 20)                // default 8 MiB
  .max_managed_bytes(1ull << 30)               // must be >= max_file_bytes
  // .time_zone("Europe/Moscow")               // optional IANA; default = system
  // .retention(RetentionMode::FilesystemFreePercent)
  // .free_percent_min(10)
  .pattern("%Y-%m-%d %H:%M:%S.%e [%l] %v")
  .build();

auto sink = std::make_shared<QuickFAST::Common::managed_file_sink_mt>(
  asioService.ioService(), cfg);
auto lg = std::make_shared<spdlog::logger>("quickfast", sink);
QuickFAST::Common::SpdlogLogger qfLog(lg);

asio::signal_set signals(asioService.ioService(), SIGINT, SIGTERM);
signals.async_wait([&](const asio::error_code &, int) {
  sink->request_shutdown();
  asioService.stopService();
});
```

`Context::setLogOutput(std::ostream&)` remains a separate debug path and is not
bridged to spdlog.

---

## Coverage (gcov / gcovr)

Supported with **GCC** (`g++-16`). Prefer `Debug`. Do not mix coverage
instrumentation with ASan/TSan or Valgrind runs.

Requires `gcovr` for the `coverage` CMake target (`sudo apt-get install -y gcovr`).
Use a `gcov` that matches the compiler major version (e.g. `gcov-16` with `g++-16`);
CMake selects `gcov-<major>` automatically when present.
Instrumentation still applies when coverage is enabled even if `gcovr` is missing;
only the report target is skipped.

```bash
cmake -S . -B build-cov -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++-16 \
  -DQUICKFAST_ENABLE_COVERAGE=ON -DQUICKFAST_BUILD_EXAMPLES=OFF
cmake --build build-cov -j --target coverage
# report: build-cov/coverage/coverage.txt
# HTML:   build-cov/coverage/html/index.html
```

The `coverage` target builds `QuickFASTTest`, runs `ctest`, then generates a
gcovr summary for first-party `src/` (deps under `_deps` are excluded).

---

## Sanitizers (ASan / UBSan / TSan)

Prefer `Debug` for readable stacks. ASan and UBSan may be combined; TSan cannot
be combined with either.

### AddressSanitizer + UndefinedBehaviorSanitizer

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++-16 \
  -DQUICKFAST_SANITIZE_ADDRESS=ON -DQUICKFAST_SANITIZE_UNDEFINED=ON \
  -DQUICKFAST_BUILD_EXAMPLES=OFF
cmake --build build-asan -j --target QuickFASTTest
export QUICKFAST_ROOT="$(pwd)"
# Fail on any sanitizer finding
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
ctest --test-dir build-asan --output-on-failure
```

### ThreadSanitizer

```bash
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++-16 \
  -DQUICKFAST_SANITIZE_THREAD=ON \
  -DQUICKFAST_BUILD_EXAMPLES=OFF
cmake --build build-tsan -j --target QuickFASTTest
export QUICKFAST_ROOT="$(pwd)"
export TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1
ctest --test-dir build-tsan --output-on-failure
```

---

## Valgrind (memcheck / helgrind / cachegrind)

Use a **non-sanitizer** build with debug info (`RelWithDebInfo` or `Debug`).
Do not mix Valgrind with ASan/TSan binaries.

```bash
cmake -S . -B build-valgrind -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CXX_COMPILER=g++-16 -DQUICKFAST_BUILD_EXAMPLES=OFF
cmake --build build-valgrind -j --target QuickFASTTest
export QUICKFAST_ROOT="$(pwd)"
TEST=build-valgrind/bin/QuickFASTTest
```

### Memcheck

```bash
valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all \
  --track-origins=yes --error-exitcode=1 \
  --log-file=valgrind-memcheck.log \
  "$TEST"
```

### Helgrind (data races / lock order)

Valgrind’s tool name is `helgrind` (one “l”).

```bash
valgrind --tool=helgrind --error-exitcode=1 \
  --log-file=valgrind-helgrind.log \
  "$TEST"
```

#### Known third-party noise: `libp11-kit`

On Ubuntu/Debian (and similar), Helgrind may report errors that are **not**
QuickFAST defects. Observed on `QuickFASTTest`:

- **Symptom:** `pthread_mutex_destroy with invalid argument` (often 2 contexts)
- **Where:** `libp11-kit.so` → `_dl_fini` / `__run_exit_handlers` (process teardown)
- **What it is:** [p11-kit](https://p11-glue.github.io/p11-glue/p11-kit.html) —
  a system PKCS#11 module manager, pulled in transitively (crypto / TLS stack),
  not linked or owned by QuickFAST
- **Action:** Treat as suppressible host noise unless a stack frame points into
  QuickFAST (`libQuickFAST`, `QuickFASTTest`, or project `src/`). Do not fail a
  release gate on p11-kit-only Helgrind summaries.

TSan on the same tests is the stronger first-party race check; it reported clean
when Helgrind only showed the p11-kit teardown findings above.

### Cachegrind (cache / branch simulation)

```bash
valgrind --tool=cachegrind --cachegrind-out-file=cachegrind.out \
  --log-file=valgrind-cachegrind.log \
  "$TEST"
# optional: cg_annotate cachegrind.out | less
```

Legacy MPC / `setup.sh` remains available for older toolchains; prefer CMake on modern Linux.

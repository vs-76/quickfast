# Building QuickFAST (CMake)

Native library, examples, and tests use **C++23** and do not depend on Boost.
First-party code is always compiled with `-Wall -Werror -pedantic`.

## Prerequisites

```bash
sudo apt-get install -y cmake build-essential libxerces-c-dev
```

Optional compilers / libc++ (examples below use the packaged names on this tree):

```bash
sudo apt-get install -y g++-16 clang++-22 libc++-dev libc++abi-dev
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
traffic. Default retention is **32 MiB** of managed log bytes. Does **not** install
its own `signal_set` — call `request_shutdown()` from the app handler.

Rotated files are named `<stem>.YYYY-MM-DD_HHMMSS[.N].<ext>`, gaining `.gz` once
compressed; `.N` disambiguates repeated rotations inside one second.

`request_shutdown()` is idempotent and blocks until an in-flight rotation timer
handler has returned, so the sink may be destroyed while the shared `io_context`
keeps running. Beyond shutdown, the sink exposes `rotate_now()` (on-demand
rotation, e.g. from a `SIGHUP` handler), `wait_for_compression_idle(timeout)`,
and `managed_bytes()`.

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
  .max_file_bytes(100ull << 20)
  .max_managed_bytes(32ull << 20)              // default
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

Legacy MPC / `setup.sh` remains available for older toolchains; prefer CMake on modern Linux.

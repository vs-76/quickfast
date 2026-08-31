# Building QuickFAST (CMake)

Native library, examples, and tests require **C++20 or later**
(`CMAKE_CXX_STANDARD` defaults to 20 and cannot be set below that) and do not
depend on Boost.
First-party code is always compiled with `-Wall -Werror -pedantic`.

Third-party libraries are resolved only with `find_package` from **Conan 2** or
**vcpkg** (pick one per build directory). System packages are never linked for
those dependencies; there is no CMake FetchContent fallback.

## Prerequisites

```bash
sudo apt-get install -y cmake build-essential
# plus Conan 2 (`pip install conan`) or a vcpkg clone — see below
```

Optional compilers / libc++ / coverage:

```bash
sudo apt-get install -y g++-16 clang++-22 libc++-dev libc++abi-dev gcovr
```

**Xerces-C++ must be ≥ 3.2.5** (CVE-2024-23807); Conan and vcpkg pin **3.3.0**.
Also required (defaults ON unless noted): standalone
[Asio](https://github.com/chriskohlhoff/asio),
[GoogleTest](https://github.com/google/googletest) (tests),
[spdlog](https://github.com/gabime/spdlog) + zlib, and
[libpcap](https://www.tcpdump.org/) (optional via `-DQUICKFAST_USE_LIBPCAP=OFF`).
[c-ares](https://c-ares.org/) hostname resolve (optional via `-DQUICKFAST_USE_CARES=OFF`).
spdlog needs host **tzdata** for IANA zones.

| CMake option | Default | Meaning |
| --- | --- | --- |
| `QUICKFAST_BUILD_TESTS` | `ON` | Build `QuickFASTTest` and register ctest |
| `QUICKFAST_BUILD_EXAMPLES` | `ON` | Build example applications |
| `QUICKFAST_BUILD_DOTNET` | `OFF` | Build C++/CLI `QuickFASTDotNet` (.NET Framework; **MSVC Windows only**) |
| `QUICKFAST_BUILD_DOTNET_EXAMPLES` | `OFF` | Build C# examples (`InterpretFASTDotNet`, `PerformanceTestDotNet`; needs DotNet) |
| `BUILD_SHARED_LIBS` | `OFF` | Static `libQuickFAST.a` by default; set `ON` for a shared library |
| `QUICKFAST_USE_LIBCXX` | `OFF` | Use LLVM libc++ (`-stdlib=libc++`); **Clang/AppleClang only** |
| `QUICKFAST_ENABLE_COVERAGE` | `OFF` | Instrument library + tests with `--coverage`; add `coverage` target if `gcovr` is installed |
| `QUICKFAST_SANITIZE_ADDRESS` | `OFF` | AddressSanitizer (`-fsanitize=address`) |
| `QUICKFAST_SANITIZE_UNDEFINED` | `OFF` | UndefinedBehaviorSanitizer (`-fsanitize=undefined`) |
| `QUICKFAST_SANITIZE_THREAD` | `OFF` | ThreadSanitizer (`-fsanitize=thread`; incompatible with ASan/UBSan) |
| `QUICKFAST_USE_SPDLOG` | `ON` | Build `SpdlogLogger` + `managed_file_sink_mt` (spdlog + zlib; tzdata) |
| `QUICKFAST_USE_LIBPCAP` | `ON` | Read capture files through libpcap (pcapng / nanosecond pcap) |
| `QUICKFAST_USE_CARES` | `ON` | Resolve hostnames with c-ares (hosts file + DNS; no glibc NSS) |
| `QUICKFAST_ENABLE_TEST_HOOKS` | follows `QUICKFAST_BUILD_TESTS` | Compile test-only hooks (`managed_file_sink_mt`, `PCapReader::dissectFrameForTest`); keep `OFF` for shipping builds |
| `QUICKFAST_BUILD_FUZZERS` | `OFF` | Build libFuzzer harnesses under `tests/fuzz/` (**Clang only**; implies test hooks) |
| `QUICKFAST_ENABLE_PVS_STUDIO` | `ON` | Create `pvs-studio` target if `pvs-studio-analyzer` is installed |
| `CMAKE_BUILD_TYPE` | (unset) | Prefer `Release` or `Debug` |
| `CMAKE_CXX_COMPILER` | system default | e.g. `g++-16`, `clang++-22` |

Set `QUICKFAST_ROOT` to the source tree when running tests outside ctest’s
discovered environment:

```bash
export QUICKFAST_ROOT="$(pwd)"
```

---

## Dependency managers

Use **either** Conan 2 **or** vcpkg; do not pass both toolchains into the same
build directory.

### Conan 2

Requires [Conan 2](https://docs.conan.io/2/) (`pip install conan` or your OS package)
and a default profile (`conan profile detect`).

Conan installs **static** dependency archives (`*:shared=False`) and leaves
QuickFAST static as well (`BUILD_SHARED_LIBS` defaults to `OFF`).

```bash
# defaults: libpcap + c-ares + tests + spdlog; all deps static
conan install . -of build/conan -s build_type=Release --build=missing

# disable spdlog adapter
conan install . -of build/conan -s build_type=Release --build=missing \
  -o '&:with_spdlog=False'

# disable c-ares (fall back to Asio getaddrinfo)
conan install . -of build/conan -s build_type=Release --build=missing \
  -o '&:with_cares=False'

cmake -S . -B build-conan \
  -DCMAKE_TOOLCHAIN_FILE="$(pwd)/build/conan/conan_toolchain.cmake" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-conan -j
ctest --test-dir build-conan --output-on-failure
```

`conanfile.py` options: `with_spdlog`, `with_pcap`, `with_cares`, `build_tests`.
The recipe pins current Conan Center releases: `xerces-c/3.3.0` (ICU transcoder;
`icu/78.2` override), `asio/1.38.2`,
`spdlog/1.17.0` + `zlib/1.3.2` (default), `libpcap/1.10.6`,
`c-ares/1.34.8` (default), and `gtest/1.18.0` as a
test requirement. It also writes the matching `QUICKFAST_*` / `BUILD_SHARED_LIBS`
CMake cache values into the toolchain.

### vcpkg (manifest mode)

Requires a [vcpkg](https://vcpkg.io/) clone and `VCPKG_ROOT` pointing at it.
Manifest: `vcpkg.json` (features `pcap`, `tests`, `spdlog`, `cares` by default).
`xerces-c` is installed with the `icu` feature (network feature off); ICU is pinned
to `78.3#2` (Conan uses `icu/78.2`).
Pins latest registry versions via `builtin-baseline` + `overrides` (asio on vcpkg
is currently 1.32.0 while Conan has 1.38.2; c-ares pinned to **1.34.8**).

Use the repo overlay triplets under `triplets/` so ports build as **static**
libraries. QuickFAST is static by default (`BUILD_SHARED_LIBS` defaults to `OFF`).

| Host | `VCPKG_TARGET_TRIPLET` |
| --- | --- |
| Linux x86_64 | `x64-linux-static` |
| Linux aarch64 | `arm64-linux-static` |
| Windows MSVC | `x64-windows-static-md` |
| macOS Intel | `x64-osx-static` |
| macOS Apple Silicon | `arm64-osx-static` |

```bash
export VCPKG_ROOT=/path/to/vcpkg   # once
TRIPLET=x64-linux-static           # see table above

cmake -S . -B build-vcpkg \
  -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_OVERLAY_TRIPLETS="$(pwd)/triplets" \
  -DVCPKG_TARGET_TRIPLET="${TRIPLET}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DQUICKFAST_USE_LIBPCAP=ON \
  -DQUICKFAST_BUILD_TESTS=ON
cmake --build build-vcpkg -j
ctest --test-dir build-vcpkg --output-on-failure
```

c-ares is on by default with the manifest. To turn it off with vcpkg:

```bash
cmake -S . -B build-vcpkg-nocares \
  -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_OVERLAY_TRIPLETS="$(pwd)/triplets" \
  -DVCPKG_TARGET_TRIPLET="${TRIPLET}" \
  -DVCPKG_MANIFEST_FEATURES="pcap;tests;spdlog" \
  -DCMAKE_BUILD_TYPE=Release \
  -DQUICKFAST_USE_CARES=OFF
```

Enable spdlog is the default. To turn the feature off with vcpkg:

```bash
cmake -S . -B build-vcpkg-nospdlog \
  -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_OVERLAY_TRIPLETS="$(pwd)/triplets" \
  -DVCPKG_TARGET_TRIPLET=x64-linux-static \
  -DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON \
  -DVCPKG_MANIFEST_FEATURES="pcap;tests" \
  -DCMAKE_BUILD_TYPE=Release \
  -DQUICKFAST_USE_SPDLOG=OFF
```

For a shared QuickFAST, pass `-DBUILD_SHARED_LIBS=ON` together with your Conan
or vcpkg toolchain file.

---

## .NET (C++/CLI) — Windows MSVC only

`src/DotNet` is a mixed-mode assembly (`QuickFASTDotNet.dll`) built with MSVC
`/clr` against **.NET Framework 4.x**. It is off by default and unavailable with
clang-cl, Ninja+clang, or non-Windows hosts.

```bash
# after a normal MSVC Conan (or vcpkg) install into build/conan-msvc
cmake -S . -B build-msvc-conan -G "Visual Studio 18 2026" -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="$(pwd)/build/conan-msvc/conan_toolchain.cmake" \
  -DQUICKFAST_BUILD_DOTNET=ON \
  -DQUICKFAST_BUILD_DOTNET_EXAMPLES=ON
cmake --build build-msvc-conan --config Release --target QuickFASTDotNet
# optional C# net48 examples:
cmake --build build-msvc-conan --config Release \
  --target InterpretFASTDotNet --target PerformanceTestDotNet
```

Outputs land under `build-msvc-conan/bin/<Config>/` (DLL + example exes).

---

## API docs (Doxygen)

`doc/Doxyfile` is maintained for **Doxygen 1.15+**. From `doc/`:

```bash
doxygen Doxyfile
# HTML → doc/html/index.html
```

Header-only (`*.h`) under `Codecs`, `Common`, `Communication`, `Messages`,
`Application`, `Examples`, and `DotNet`. LaTeX output is off by default.

The generated docs are **warning-free**; treat a new Doxygen warning as a
failure. `WARN_IF_UNDOCUMENTED` is on, so a new public member needs a `///`
comment with `@param` / `@returns` where they apply.

The landing page and the quick-start examples come from the `\mainpage` comment
in `src/Common/QuickFASTPch.h`. Per-class usage examples live in `@par Example`
`@code` blocks on the type they document — `Codecs::Decoder`, `Codecs::Encoder`,
`Application::DecoderConnection`, `Messages::MessageToJson`, and
`Common::managed_file_sink_mt` among others. Keep those blocks compiling: they
are copied into real code.

---

## Default compiler (system `c++`)

Install deps with Conan (or vcpkg) first, then:

```bash
conan install . -of build/conan -s build_type=Release --build=missing
cmake -S . -B build   -DCMAKE_TOOLCHAIN_FILE="$(pwd)/build/conan/conan_toolchain.cmake"   -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Outputs: `build/lib/libQuickFAST.a` (or `.so` with `-DBUILD_SHARED_LIBS=ON`),
tests/examples under `build/bin/`.

---

## g++

Every configure below needs a Conan or vcpkg toolchain, for example
`-DCMAKE_TOOLCHAIN_FILE="$(pwd)/build/conan/conan_toolchain.cmake"` after
`conan install` (omitted from the snippets for brevity).

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

## Windows compile gate (mingw-w64)

`src/Common` carries `_WIN32` branches — `windows.h`, `GetDiskFreeSpaceExW`, and
`wchar_t` filesystem paths — that a Linux build never compiles. This script
cross-compiles those sources with mingw-w64 and fails on any warning, so the
Windows paths cannot rot unnoticed between real Windows builds:

```bash
sudo apt install g++-mingw-w64-x86-64   # once
scripts/check-windows-compile.sh                          # all of src/Common
scripts/check-windows-compile.sh src/Common/ManagedFileSink.cpp
```

It is compile-only. Linking QuickFAST for Windows also needs Xerces-C, zlib, fmt
and spdlog cross-built for the target, which the script does not attempt; it
borrows the host's portable third-party headers for the compile. Override
`MINGW_CXX`, `ASIO_INCLUDE_DIR` or `HOST_INCLUDE_DIR` if autodetection is wrong
(Conan/vcpkg include trees, or set `ASIO_INCLUDE_DIR`).

A narrow-string path passed to a `...W` API, for example, fails the gate while
building cleanly on POSIX.

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
the `QuickFAST` library (packaged deps under the build prefix and `src/DotNet` are excluded):

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

## spdlog logger adapter (default ON)

With `QUICKFAST_USE_SPDLOG=ON` (the default), QuickFAST builds:
- `Common::SpdlogLogger` — `Common::Logger` → application `spdlog::logger`
- `Common::managed_file_sink_mt` — rotating/compressing file sink on a shared
  `asio::io_context` (zlib required)

Core codecs/communication stay on the `Logger` interface; inject the adapter via
your message consumer and/or `Communication::AsioService::setLogger`.

Requires system **tzdata** for IANA zones (`std::chrono` tzdb).
Disable with `-DQUICKFAST_USE_SPDLOG=OFF`.

```bash
cmake -S . -B build-spdlog -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++-16 \
  -DQUICKFAST_BUILD_EXAMPLES=OFF
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
gcovr summary for first-party `src/` (third-party prefixes are excluded).

---

## libFuzzer harnesses

Clang-only. Builds five binaries that link libFuzzer with ASan and UBSan.
Seed corpora live under `tests/fuzz/corpora/<harness>/`. Exceptions from
malformed input are swallowed; sanitizer aborts are the signal.

| Binary | Surface |
| --- | --- |
| `fuzz_decode_message` | `Decoder::decodeMessage` over FAST bytes |
| `fuzz_pcap_dissect` | `PCapReader` Ethernet / Linux-SLL / raw UDP dissection |
| `fuzz_decimal` | `Decimal` arithmetic and ordering trichotomy |
| `fuzz_xml_template` | `XMLTemplateParser::parse` |
| `fuzz_header_analyzer` | `FixedSizeHeaderAnalyzer` |

```bash
cmake -S . -B build-fuzz -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=clang++-22 \
  -DQUICKFAST_BUILD_FUZZERS=ON \
  -DQUICKFAST_BUILD_TESTS=OFF \
  -DQUICKFAST_BUILD_EXAMPLES=OFF
cmake --build build-fuzz -j
```

Short smoke run (also useful in CI). Pass a **copy** of the seed corpus so
libFuzzer does not rewrite the checked-in seeds:

```bash
BIN=build-fuzz/bin
SEED=tests/fuzz/corpora
WORKDIR=$(mktemp -d)
trap 'rm -rf "$WORKDIR"' EXIT
for name in decode_message pcap_dissect decimal xml_template header_analyzer; do
  mkdir -p "$WORKDIR/$name"
  cp -a "$SEED/$name/." "$WORKDIR/$name/"
done
"$BIN/fuzz_decode_message" -max_total_time=30 -max_len=4096 \
  -rss_limit_mb=2048 "$WORKDIR/decode_message"
"$BIN/fuzz_pcap_dissect" -max_total_time=30 -max_len=2048 \
  "$WORKDIR/pcap_dissect"
"$BIN/fuzz_decimal" -max_total_time=30 "$WORKDIR/decimal"
"$BIN/fuzz_xml_template" -max_total_time=30 -max_len=8192 \
  "$WORKDIR/xml_template"
"$BIN/fuzz_header_analyzer" -max_total_time=30 -max_len=512 \
  "$WORKDIR/header_analyzer"
```

Do not combine with `QUICKFAST_SANITIZE_THREAD`. Shipping builds should leave
`QUICKFAST_BUILD_FUZZERS=OFF` (and usually `QUICKFAST_ENABLE_TEST_HOOKS=OFF`).

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

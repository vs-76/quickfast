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

| CMake option | Default | Meaning |
| --- | --- | --- |
| `QUICKFAST_BUILD_TESTS` | `ON` | Build `QuickFASTTest` and register ctest |
| `QUICKFAST_BUILD_EXAMPLES` | `ON` | Build example applications |
| `QUICKFAST_USE_LIBCXX` | `OFF` | Use LLVM libc++ (`-stdlib=libc++`); **Clang/AppleClang only** |
| `QUICKFAST_ENABLE_COVERAGE` | `OFF` | Instrument library + tests with `--coverage`; add `coverage` target if `gcovr` is installed |
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

Legacy MPC / `setup.sh` remains available for older toolchains; prefer CMake on modern Linux.

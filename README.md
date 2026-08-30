## QuickFAST -- An implementation of the FAST protocol for native C++ and .NET

QuickFAST is an Open Source native C++ implementation of the FAST Protocol [SM]. FAST(Fix Adapted for STreaming) protocol 
was developed by FIX Protocol Limited [FPL] (http://www.fixprotocol.org/fast/) as a way to reduce the bandwidth and network-latency 

Because FAST not specific to market data or the financial industry, there are opportunities for using FAST in a wide variety of situations.

QuickFAST is written to be portable to many platforms. It is routinely tested on Windows and Linux. The project also includes a .NET wrapper 
which supports using QuickFAST in the .NET environment. Ask if you want support for other platforms.

### Linux build (CMake + C++20)

See **[BUILD.md](BUILD.md)** for g++ / clang++ recipes and **Conan 2** / **vcpkg**
dependency install (required — Conan 2 or vcpkg only; no system or FetchContent deps).

Native library, examples, and tests no longer depend on Boost. Dependencies
(installed via Conan or vcpkg):

- C++20 or later (CMake enforces `CMAKE_CXX_STANDARD >= 20`; verified with g++ 15/16 and clang++ 22)
- [Xerces-C++](https://xerces.apache.org/xerces-c/) ≥ 3.2.5 (pinned 3.3.0 in manifests)
- Standalone [Asio](https://github.com/chriskohlhoff/asio) and [GoogleTest](https://github.com/google/googletest) / GoogleMock
- [spdlog](https://github.com/gabime/spdlog) + zlib (default ON; `-DQUICKFAST_USE_SPDLOG=OFF` to disable)
- [libpcap](https://www.tcpdump.org/) (default ON)

QuickFAST defaults to a static archive (`BUILD_SHARED_LIBS=OFF`).

```bash
sudo apt-get install -y cmake build-essential
# Conan 2: pip install conan && conan profile detect

conan install . -of build/conan -s build_type=Release --build=missing
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE="$(pwd)/build/conan/conan_toolchain.cmake" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Dual-compiler check (re-run `conan install` if the profile compiler changes):

```bash
cmake -S . -B build-gcc16 -DCMAKE_TOOLCHAIN_FILE="$(pwd)/build/conan/conan_toolchain.cmake" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-16
cmake --build build-gcc16 -j && ctest --test-dir build-gcc16 --output-on-failure

cmake -S . -B build-clang22 -DCMAKE_TOOLCHAIN_FILE="$(pwd)/build/conan/conan_toolchain.cmake" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++-22
cmake --build build-clang22 -j && ctest --test-dir build-clang22 --output-on-failure
```

Optional flags: `-DQUICKFAST_BUILD_TESTS=OFF`, `-DQUICKFAST_BUILD_EXAMPLES=OFF`,
`-DQUICKFAST_USE_LIBCXX=ON` (Clang only), `-DQUICKFAST_ENABLE_PVS_STUDIO=OFF`.

The legacy MPC/`setup.sh` flow remains for older toolchains; prefer CMake + Conan/vcpkg.

Instructions for [getting started with QuickFAST are here](https://github.com/objectcomputing/quickfast/wiki/GettingStarted)

QuickFAST was developed by Object Computing Inc.(OCI) St. Louis Missouri USA. OCI has made QuickFAST available as open source software 
which may be used without payment of development or runtime license fees. OCI offers commercial support for QuickFAST.

For questions and discussion of QuickFAST, visit the [QuickFAST users mailing list](https://groups.google.com/forum/#!forum/quickfast_users)

###List Rules:
* Normal mailing list rules apply on the list. Discussions should be civil and on-topic. Offensive messages, off-topic chatter, and spam will not be tolerated.

* Messages from new members will be moderated due to the high volume of spam postings that are sent to this (and any) mailing list. For practical purposes this means there may be a delay before your first message to the list is published. Once you have est

* Messages on the list must be posted in English. It is acceptable to have the message in another language as well, but an English translation must appear first. For more information about this, click this link

## See also:

* For a open source Java implementation of FAST, see https://sourceforge.net/projects/openfast/
* For an open source C++ implementation of the FIX protocol, see http://www.quickfixengine.org/
* For an open source Java implementation of the FIX protocol, see http://www.quickfixj.org/

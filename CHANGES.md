# Changes

Notable changes to **quickfast-ng**, the community fork of
[objectcomputing/quickfast](https://github.com/objectcomputing/quickfast).

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Upstream history before the fork is preserved in `ChangeLog`.

## [2.0.0] - 2026-08-31

First release of the fork. The major bump reflects that this is not a
drop-in replacement for OCI QuickFAST 1.5: the dependency set, the build
system and several decoder behaviours changed. `SOVERSION` moves to 2.

Everything below is the work done since the fork point, upstream commit
`f9403cf` (2017-03-14), the last commit of OCI QuickFAST 1.5. That is 151
commits touching 427 first-party files (excluding generated Doxygen output).

The fork keeps the FAST wire protocol behaviour and the `QuickFAST::` API shape,
but replaces the build system, the dependency story and the third-party
runtime, and hardens the decoder against malformed input.

### Breaking changes

- **Boost is gone.** The native library, examples and tests build against the
  standard library, standalone [Asio](https://think-async.com/Asio/) and
  GoogleTest/GoogleMock. `FieldCPtr` is now `std::shared_ptr<const Field>`.
- **C++20 is the enforced minimum.** CMake rejects `CMAKE_CXX_STANDARD < 20`
  and raises package-manager defaults that fall below it. Verified with
  g++ 15/16 and clang++ 22.
- **MPC and the legacy setup tooling were removed** (`QuickFAST.mwc`, `*.mpc`,
  `*.mpb`, `setup.sh`, `setup.cmd`, `m.sh`, `m.cmd`). CMake is the only build
  system.
- **Dependencies must come from Conan 2 or vcpkg.** System packages and
  `FetchContent` are no longer consulted.
- **Static by default.** `BUILD_SHARED_LIBS` defaults to `OFF`, so QuickFAST
  and its dependencies link as static archives unless asked otherwise.
- **Xerces-C 3.2.5 or newer is required** (CVE-2024-23807); the build pins
  3.3.0 and selects the ICU transcoder.
- **Unit tests moved** from `src/Tests/` to `tests/`, and the Doxygen config
  moved from `src/Doxyfile` to `doc/Doxyfile` with HTML output in `doc/html/`.
- **Decoder behaviour tightened** where upstream accepted invalid input: a
  missing field throws `FieldNotPresent` instead of returning zero, eight-bit
  data in `ascii` fields is rejected, decimal exponents outside the specified
  range are refused, truncated ascii strings are reported, and
  `setStrict(false)` now actually relaxes overflow checking instead of doing
  nothing.

### Added

- **Package manager support.** Conan 2 recipe (`conanfile.py`) and vcpkg
  manifest (`vcpkg.json`) with pinned versions for Xerces-C, ICU, Asio,
  GoogleTest, spdlog, zlib, libpcap and c-ares.
- **QuickFAST installs as a consumable package.** `cmake --install` lays down
  the headers under their module directory, the library, and a
  `QuickFASTConfig.cmake` that re-finds every dependency reaching the link
  interface, so a static QuickFAST resolves libpcap and c-ares for its
  consumers. Downstream projects use it as:

  ```cmake
  find_package(QuickFAST 2.0.0 REQUIRED)
  target_link_libraries(my_app PRIVATE QuickFAST::QuickFAST)
  ```

  `QUICKFAST_INSTALL=OFF` suppresses the install rules for vendored builds.
- **Distributable packages.** `conanfile.py` now builds and packages the library
  (`conan create`, validated by `test_package/`) in addition to installing
  dependencies for development, and `packaging/vcpkg/ports/quickfast-ng` is a
  real vcpkg port with `pcap`, `spdlog` and `cares` features.
- **`tests/package`**, a standalone consumer that sees QuickFAST only through
  `find_package`, so a broken install layout fails there and not in a user's
  tree.
- **JSON output.** `Messages::MessageToJson` converts a decoded message to
  JSON with configurable name/id keys and exact decimal and `uInt64` rendering;
  `InterpretApplication -ojson` exposes it on the command line.
- **Source-specific multicast.** Receivers can join an (S,G) group, and a
  multicast feed is now described separately from how it joins.
- **Capture file input through libpcap** (`QUICKFAST_USE_LIBPCAP`, default ON),
  covering pcap, pcapng and nanosecond timestamps.
- **Hostname resolution through c-ares** (`QUICKFAST_USE_CARES`, default ON).
  `HostResolver` has Asio and c-ares backends so a statically linked binary can
  avoid glibc NSS `getaddrinfo`.
- **spdlog logging** (`QUICKFAST_USE_SPDLOG`, default ON): a `SpdlogLogger`
  adapter plus `Common::managed_file_sink_mt`, a file sink with size and
  midnight rotation, gzip compression and a retention budget.
- **Synthetic decode metadata.** `ReceiveTime` (UTC microseconds captured when
  the buffer is accepted) and `PktSize` (accepted packet length) are injected
  as `uInt64` fields after FAST decode.
- **CMake C++/CLI build for `QuickFASTDotNet`** (`QUICKFAST_BUILD_DOTNET`,
  Windows MSVC only), replacing the MPC-driven .NET build.
- **Fuzzing.** Five libFuzzer harnesses (`fuzz_decimal`, `fuzz_decode_message`,
  `fuzz_header_analyzer`, `fuzz_pcap_dissect`, `fuzz_xml_template`) under
  `tests/fuzz/`, with seed corpora including inputs the fuzzer found.
- **Analysis and coverage tooling.** ASan/UBSan/TSan options, Valgrind recipes,
  a gcovr coverage target, and an optional PVS-Studio target
  (`QUICKFAST_ENABLE_PVS_STUDIO`).
- **`BufferQueueBench` example** for timing `bufferMutex` contention.
- **`BUILD.md`**, a full build, test and analysis guide, and a rewritten
  Doxygen configuration whose HTML is tracked in `doc/html/`.

### Changed

- The repository moved to
  [vs-76/quickfast-ng](https://github.com/vs-76/quickfast-ng). The CMake
  summary line and the `-V` product banner now say *QuickFAST-ng*; the library
  target, the `QuickFAST::` namespace and the installed headers keep their
  names.
- Unit tests were ported from Boost.Test to GoogleTest/GoogleMock; a
  fully-featured Release build runs 469 ctest cases covering codec, assembler,
  communication, logging and CLI paths.
- Builds run warning-free with `-Wall -Werror -pedantic` on g++ 16 and
  clang++ 22, and a mingw-w64 compile gate keeps the Windows code paths honest.
- The Windows MSVC build and its tests work again.
- Doxygen output is warning-free, with `@par Example` blocks on the primary
  entry points.
- Post-fork changes are attributed to *QuickFAST contributors*; the original
  Object Computing, Inc. headers are left intact.

### Fixed

Eighty-six fixes landed. The larger themes:

- **Codec hardening.** Unsigned integer overflow detection while decoding,
  integer delta and increment range checks on both sides, decimal mantissa
  scaling bounds, decimal delta range checks, hard ceilings for byte-vector and
  sequence lengths, presence-map decode bounds and a full buffer clear on
  reset, correct fixed-size header endianness, header block size limits,
  declared packet length validation, template boolean/pmap/overflow attribute
  validation, misplaced XML template elements reported instead of crashing, and
  a sequencing assembler that reports, bounds and wraps correctly.
- **Static analysis (28).** PVS-Studio General Analysis findings, including
  incomplete copy/assignment pairs (V690), throwing exception constructors
  (V1067), `condition_variable` waits without predicates (V1089), uninitialized
  members (V730), a virtual `stop()` in a destructor (V1053) and a
  `unique_ptr`/`new` mismatch (V554).
- **Memory and lifetime.** `WorkingBuffer::grow` overrunning a shrunken
  allocation, `StringBufferT` resize/char-assignment/erase, the implicit
  sequence length instruction being freed early, `LinkedBuffer` indexing an
  external buffer, `FieldSet`'s nothrow assumption, and dictionary `Value`
  string storage surviving an erase.
- **Communication.** Atomic receiver control flags and counters, a shutdown
  path and wait result for `RecoveryFeed`, draining the queue before end of
  input becomes a stop, and bounded wire lengths throughout the pcap dissection
  layer.
- **Application and CLI.** No longer deleting `std::cin` or dereferencing a
  null echo stream, rejecting a zero buffer size or count, reporting a bad
  option value as such instead of as an unknown option, guarding the null
  verbose stream, keeping multicast feeds across copies, and failing the
  connection when a receiver cannot start.
- **Logging.** DST-safe rotation schedule, a reachable retention budget,
  per-entry `error_code` handling, and safe teardown and rotation naming.

### Performance

- O(1) template-id lookup once a registry is finalized.
- Amortized field lookup on the encode path; duplicate identity detection moved
  from every add to the lookup.
- Contiguous stop-bit decode for 64-bit integers and contiguous copies for
  `ascii` and `byteVector` decode, backed by a bulk append on `WorkingBuffer`.
- Interned immutable scalar `Field` instances and a denser dictionary `Value`
  layout for numeric entries.
- Geometric presence-map growth instead of quadratic.
- The encoder no longer copies an instruction pointer per field.

### Removed

- MPC/MWC project files, `setup.sh`, `setup.cmd`, `m.sh`, `m.cmd` and the
  `bin/` build-configuration scripts.
- `src/Communication/AtomicQueue.h` and its forward header.
- `.hgignore` and `bin/hgcle.py`, the Mercurial-era changelog tooling.

[2.0.0]: https://github.com/vs-76/quickfast-ng/releases/tag/v2.0.0

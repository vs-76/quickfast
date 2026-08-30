#!/usr/bin/env bash
#
# Windows portability gate for the platform-sensitive sources.
#
# Cross-compiles with mingw-w64 and fails on any warning, so that _WIN32 branches
# (windows.h, GetDiskFreeSpaceExW, wchar_t filesystem paths) cannot rot unnoticed
# between real Windows builds.
#
# This is compile-only. Linking QuickFAST for Windows additionally needs Xerces-C,
# zlib, fmt and spdlog cross-built for the target, which this script does not do;
# portable headers are borrowed from the host for the compile.
#
# Usage:
#   scripts/check-windows-compile.sh [source.cpp ...]   # default: src/Common/*.cpp
#
# Environment overrides:
#   MINGW_CXX          cross compiler          (default x86_64-w64-mingw32-g++)
#   ASIO_INCLUDE_DIR   standalone Asio headers (Conan, vcpkg, or /usr/include)
#   HOST_INCLUDE_DIR   spdlog/fmt/zlib headers (default /usr/include)

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${repo_root}"

CXX="${MINGW_CXX:-x86_64-w64-mingw32-g++}"
if ! command -v "${CXX}" >/dev/null 2>&1; then
  echo "error: ${CXX} not found; install mingw-w64 or set MINGW_CXX" >&2
  exit 127
fi

asio_include="${ASIO_INCLUDE_DIR:-}"
if [[ -z "${asio_include}" ]]; then
  # Prefer Conan/vcpkg staging, then system.
  for candidate in \
      ./build*/vcpkg_installed/*/include \
      ./build/conan \
      ./build*/conan; do
    for dir in ${candidate}; do
      if [[ -f "${dir}/asio.hpp" ]]; then
        asio_include="${dir}"
        break 2
      fi
      if [[ -f "${dir}/asio/asio.hpp" ]]; then
        asio_include="${dir}"
        break 2
      fi
    done
  done
fi
if [[ -z "${asio_include}" || ! -d "${asio_include}" ]]; then
  if [[ -f /usr/include/asio.hpp ]]; then
    asio_include=/usr/include
  fi
fi
if [[ -z "${asio_include}" || ! -d "${asio_include}" ]]; then
  echo "error: standalone Asio headers not found; configure a Conan/vcpkg build first" >&2
  echo "       or set ASIO_INCLUDE_DIR" >&2
  exit 1
fi

host_include="${HOST_INCLUDE_DIR:-/usr/include}"

# The host include root cannot be passed wholesale: its glibc headers would
# shadow the mingw ones. Stage only the portable third-party headers.
staged="$(mktemp -d)"
trap 'rm -rf "${staged}"' EXIT
for header in spdlog fmt zlib.h zconf.h; do
  if [[ ! -e "${host_include}/${header}" ]]; then
    echo "error: ${host_include}/${header} not found (install spdlog/fmt/zlib dev headers)" >&2
    exit 1
  fi
  ln -s "${host_include}/${header}" "${staged}/"
done

sources=("$@")
if [[ ${#sources[@]} -eq 0 ]]; then
  mapfile -t sources < <(ls src/Common/*.cpp)
fi

# Kept in step with the QuickFAST target in CMakeLists.txt.
defines=(
  -DASIO_NO_DEPRECATED
  -DASIO_STANDALONE
  -DQUICKFAST_HAS_SPDLOG=1
  -DQUICKFAST_ENABLE_TEST_HOOKS=1
  -DQUICKFAST_BUILD_DLL
  -DQuickFAST_EXPORTS
  -DSPDLOG_COMPILED_LIB
  -DSPDLOG_FMT_EXTERNAL
)

echo "Windows compile gate: ${CXX}"
echo "  asio: ${asio_include}"
echo

failed=0
for source in "${sources[@]}"; do
  log="${staged}/$(basename "${source}").log"
  if "${CXX}" -fsyntax-only "${source}" \
      -std=c++20 -Wall -Wextra -pedantic -Werror \
      "${defines[@]}" \
      -I src -isystem "${asio_include}" -isystem "${staged}" \
      >"${log}" 2>&1; then
    echo "  ok    ${source}"
  else
    echo "  FAIL  ${source}"
    sed 's/^/        /' "${log}"
    failed=$((failed + 1))
  fi
done

echo
if [[ ${failed} -ne 0 ]]; then
  echo "${failed} source(s) do not compile for Windows"
  exit 1
fi
echo "all ${#sources[@]} source(s) compile for Windows"

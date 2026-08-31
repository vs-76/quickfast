#!/usr/bin/env bash
# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# Print the SHA512 of a released GitHub source tarball, for the SHA512 field of
# packaging/vcpkg/ports/quickfast-ng/portfile.cmake.
#
# A port pins the hash of the archive it downloads, and that archive contains the
# portfile, so the value cannot exist inside the tag it describes. Run this once
# the tag is pushed, then commit the result.
#
#   scripts/vcpkg-port-sha512.sh v2.0.0
#   scripts/vcpkg-port-sha512.sh v2.0.0 --write

set -euo pipefail

REPO=${QUICKFAST_REPO:-vs-76/quickfast-ng}
TAG=${1:-}
MODE=${2:-}

if [[ -z ${TAG} ]]; then
  echo "usage: $(basename "$0") <tag> [--write]" >&2
  echo "example: $(basename "$0") v2.0.0 --write" >&2
  exit 2
fi

URL="https://github.com/${REPO}/archive/refs/tags/${TAG}.tar.gz"
TARBALL=$(mktemp --suffix=.tar.gz)
trap 'rm -f "${TARBALL}"' EXIT

if ! curl -fsSL "${URL}" -o "${TARBALL}"; then
  echo "error: cannot download ${URL}" >&2
  echo "is the tag pushed?" >&2
  exit 1
fi

SHA=$(sha512sum "${TARBALL}" | cut -d' ' -f1)
echo "${SHA}"

if [[ ${MODE} == "--write" ]]; then
  PORTFILE="$(dirname "$0")/../packaging/vcpkg/ports/quickfast-ng/portfile.cmake"
  if [[ ! -f ${PORTFILE} ]]; then
    echo "error: ${PORTFILE} not found" >&2
    exit 1
  fi
  # Replace whatever the SHA512 line holds: the 0 bootstrap value or a stale hash.
  sed -i -E "s|^([[:space:]]*)SHA512 .*|\1SHA512 ${SHA}|" "${PORTFILE}"
  echo "updated ${PORTFILE}" >&2
fi

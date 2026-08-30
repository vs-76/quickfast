# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.

# Locate libpcap within CMAKE_PREFIX_PATH only (Conan / vcpkg).
# System prefixes are disabled by QuickFASTPackageManager before this runs.
#
# Result variables:
#   PCAP_FOUND        - true if libpcap was located
#   PCAP_INCLUDE_DIRS - directory holding pcap.h
#   PCAP_LIBRARIES    - the library to link
#   PCAP_VERSION      - version string, when pcap.h reports one
#
# Imported target:
#   PCAP::PCAP

find_path(PCAP_INCLUDE_DIR
  NAMES pcap.h
  PATH_SUFFIXES pcap
  DOC "Directory containing pcap.h"
)

find_library(PCAP_LIBRARY
  NAMES pcap pcap_static wpcap
  DOC "Path to the libpcap library"
)

# pcap.h itself carries no version macro; pcap/pcap.h in 1.9+ does not either,
# so fall back to the runtime string baked into the library when available.
if(PCAP_INCLUDE_DIR AND EXISTS "${PCAP_INCLUDE_DIR}/pcap/pcap.h")
  file(STRINGS "${PCAP_INCLUDE_DIR}/pcap/pcap.h" _pcap_version_line
    REGEX "^#define[ \t]+PCAP_VERSION_(MAJOR|MINOR)")
  if(_pcap_version_line)
    string(REGEX MATCHALL "[0-9]+" _pcap_version_parts "${_pcap_version_line}")
    list(JOIN _pcap_version_parts "." PCAP_VERSION)
  endif()
  unset(_pcap_version_line)
  unset(_pcap_version_parts)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PCAP
  REQUIRED_VARS PCAP_LIBRARY PCAP_INCLUDE_DIR
  VERSION_VAR PCAP_VERSION
)

if(PCAP_FOUND)
  set(PCAP_INCLUDE_DIRS "${PCAP_INCLUDE_DIR}")
  set(PCAP_LIBRARIES "${PCAP_LIBRARY}")

  if(NOT TARGET PCAP::PCAP)
    add_library(PCAP::PCAP UNKNOWN IMPORTED)
    set_target_properties(PCAP::PCAP PROPERTIES
      IMPORTED_LOCATION "${PCAP_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${PCAP_INCLUDE_DIR}"
    )
  endif()
endif()

mark_as_advanced(PCAP_INCLUDE_DIR PCAP_LIBRARY)

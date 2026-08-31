# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# Resolve libpcap from Conan or vcpkg only (CONFIG packages).

find_package(libpcap QUIET CONFIG)
if(libpcap_FOUND AND TARGET libpcap::libpcap)
  if(NOT TARGET PCAP::PCAP)
    add_library(PCAP::PCAP ALIAS libpcap::libpcap)
  endif()
  set(PCAP_FOUND TRUE)
  set(PCAP_LIBRARY libpcap::libpcap)
  if(NOT libpcap_VERSION AND DEFINED PCAP_VERSION)
    set(libpcap_VERSION "${PCAP_VERSION}")
  endif()
  quickfast_report_dependency("libpcap" libpcap_VERSION)
  return()
endif()

find_package(unofficial-libpcap QUIET CONFIG)
if(unofficial-libpcap_FOUND AND TARGET unofficial::libpcap::libpcap)
  if(NOT TARGET PCAP::PCAP)
    add_library(PCAP::PCAP ALIAS unofficial::libpcap::libpcap)
  endif()
  set(PCAP_FOUND TRUE)
  set(PCAP_LIBRARY unofficial::libpcap::libpcap)
  if(NOT unofficial-libpcap_VERSION AND DEFINED PCAP_VERSION)
    set(unofficial-libpcap_VERSION "${PCAP_VERSION}")
  endif()
  # Prefer a stable display name; version var may be unofficial-libpcap_VERSION.
  if(unofficial-libpcap_VERSION)
    set(libpcap_VERSION "${unofficial-libpcap_VERSION}")
  endif()
  quickfast_report_dependency("libpcap" libpcap_VERSION)
  return()
endif()

# Last resort: module find restricted to CMAKE_PREFIX_PATH (no system paths).
find_package(PCAP REQUIRED)
quickfast_report_dependency("libpcap" PCAP_VERSION)

# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# Resolve libpcap for capture-file support.
# Prefer Conan / vcpkg CONFIG packages, then the project FindPCAP module.

find_package(libpcap QUIET CONFIG)
if(libpcap_FOUND AND TARGET libpcap::libpcap)
  if(NOT TARGET PCAP::PCAP)
    add_library(PCAP::PCAP ALIAS libpcap::libpcap)
  endif()
  set(PCAP_FOUND TRUE)
  set(PCAP_LIBRARY libpcap::libpcap)
  message(STATUS "Using packaged libpcap (libpcap::libpcap)")
  return()
endif()

find_package(unofficial-libpcap QUIET CONFIG)
if(unofficial-libpcap_FOUND AND TARGET unofficial::libpcap::libpcap)
  if(NOT TARGET PCAP::PCAP)
    add_library(PCAP::PCAP ALIAS unofficial::libpcap::libpcap)
  endif()
  set(PCAP_FOUND TRUE)
  set(PCAP_LIBRARY unofficial::libpcap::libpcap)
  message(STATUS "Using packaged libpcap (unofficial::libpcap::libpcap)")
  return()
endif()

find_package(PCAP REQUIRED)
message(STATUS "Using libpcap via FindPCAP: ${PCAP_LIBRARY}")

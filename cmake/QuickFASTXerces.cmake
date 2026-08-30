# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# Resolve Xerces-C++ for QuickFAST.
# Prefer a system install that is patched for CVE-2024-23807 (>= 3.2.5).
# Otherwise FetchContent Apache Xerces-C 3.3.0 (API-compatible; rebuild required).

set(QUICKFAST_XERCES_MIN_VERSION 3.2.5)
set(QUICKFAST_XERCES_FETCH_VERSION 3.3.0)

find_package(XercesC ${QUICKFAST_XERCES_MIN_VERSION} QUIET)
if(XercesC_FOUND)
  message(STATUS "Using system Xerces-C ${XercesC_VERSION}")
  return()
endif()

message(STATUS
  "Xerces-C >= ${QUICKFAST_XERCES_MIN_VERSION} not found "
  "(CVE-2024-23807); fetching Apache Xerces-C ${QUICKFAST_XERCES_FETCH_VERSION}")

include(FetchContent)

# PATCH_COMMAND runs with cwd = extracted source tree. Drop docs/tests/samples
# so FetchContent does not pollute the QuickFAST build or ctest.
file(WRITE "${CMAKE_BINARY_DIR}/_qf_patch_xerces.cmake" [=[
file(READ "CMakeLists.txt" _qf_xerces_cm)
string(REPLACE "add_subdirectory(doc)" "# add_subdirectory(doc)" _qf_xerces_cm "${_qf_xerces_cm}")
string(REPLACE "add_subdirectory(tests)" "# add_subdirectory(tests)" _qf_xerces_cm "${_qf_xerces_cm}")
string(REPLACE "add_subdirectory(samples)" "# add_subdirectory(samples)" _qf_xerces_cm "${_qf_xerces_cm}")
file(WRITE "CMakeLists.txt" "${_qf_xerces_cm}")
]=])

FetchContent_Declare(
  xercesc
  URL https://archive.apache.org/dist/xerces/c/3/sources/xerces-c-${QUICKFAST_XERCES_FETCH_VERSION}.tar.gz
  URL_HASH SHA256=9555f1d06f82987fbb4658862705515740414fd34b4db6ad2ed76a2dc08d3bde
  PATCH_COMMAND ${CMAKE_COMMAND} -P "${CMAKE_BINARY_DIR}/_qf_patch_xerces.cmake"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# Xerces pins CMAKE_CXX_STANDARD to 14; modern ICU headers need C++17+. Prefer
# gnuiconv so the FetchContent build does not depend on ICU at all.
set(transcoder "gnuiconv" CACHE STRING "Xerces transcoder" FORCE)

FetchContent_MakeAvailable(xercesc)

if(NOT TARGET xerces-c)
  message(FATAL_ERROR
    "Fetched Xerces-C ${QUICKFAST_XERCES_FETCH_VERSION} but target xerces-c was not created")
endif()

if(NOT TARGET XercesC::XercesC)
  add_library(XercesC::XercesC ALIAS xerces-c)
endif()

set(XercesC_FOUND TRUE)
set(XercesC_VERSION ${QUICKFAST_XERCES_FETCH_VERSION})
message(STATUS "Using fetched Xerces-C ${XercesC_VERSION}")

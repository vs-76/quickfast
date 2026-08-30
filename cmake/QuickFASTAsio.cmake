# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# Resolve standalone Asio: find_package (Conan / vcpkg / system) then optional FetchContent.
# Sets QUICKFAST_ASIO_TARGET to the INTERFACE/IMPORTED library to link.

set(QUICKFAST_ASIO_FETCH_TAG asio-1-30-2)

find_package(asio QUIET CONFIG)
if(asio_FOUND)
  if(TARGET asio::asio)
    set(QUICKFAST_ASIO_TARGET asio::asio)
  elseif(TARGET asio)
    set(QUICKFAST_ASIO_TARGET asio)
  endif()
  if(QUICKFAST_ASIO_TARGET)
    message(STATUS "Using packaged Asio (${QUICKFAST_ASIO_TARGET})")
    return()
  endif()
endif()

if(NOT QUICKFAST_FETCH_DEPS)
  message(FATAL_ERROR
    "Asio not found. Install via Conan (asio), vcpkg (asio), or a system package, "
    "or set -DQUICKFAST_FETCH_DEPS=ON to download ${QUICKFAST_ASIO_FETCH_TAG}.")
endif()

include(FetchContent)
FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG        ${QUICKFAST_ASIO_FETCH_TAG}
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(asio)

if(NOT TARGET asio)
  add_library(asio INTERFACE)
endif()
if(NOT TARGET asio::asio)
  add_library(asio::asio ALIAS asio)
endif()
target_include_directories(asio SYSTEM INTERFACE ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)
set(QUICKFAST_ASIO_TARGET asio::asio)
message(STATUS "Using FetchContent Asio ${QUICKFAST_ASIO_FETCH_TAG}")

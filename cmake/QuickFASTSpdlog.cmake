# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# Resolve spdlog + zlib when QUICKFAST_USE_SPDLOG is ON.
# Prefer Conan / vcpkg / system packages (find_package); optional FetchContent.

find_package(spdlog 1.12 QUIET CONFIG)
if(spdlog_FOUND)
  message(STATUS "Using packaged spdlog (find_package)")
else()
  if(NOT QUICKFAST_FETCH_DEPS)
    message(FATAL_ERROR
      "spdlog >= 1.12 not found. Install via Conan (spdlog + zlib), vcpkg "
      "(feature spdlog), or a system package, or set -DQUICKFAST_FETCH_DEPS=ON "
      "to download v1.15.1.")
  endif()
  include(FetchContent)
  set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
  set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
  set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.1
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(spdlog)
  if(TARGET spdlog)
    target_compile_options(spdlog PRIVATE -w)
  endif()
  message(STATUS "Using FetchContent spdlog v1.15.1")
endif()

# zlib is a direct QuickFAST dependency for managed_file_sink_mt compression.
# Conan and vcpkg ship it with the spdlog feature; find_package picks that up
# when CMAKE_PREFIX_PATH / the toolchain is set.
find_package(ZLIB QUIET)
if(ZLIB_FOUND)
  message(STATUS "Using packaged zlib (find_package)")
else()
  if(NOT QUICKFAST_FETCH_DEPS)
    message(FATAL_ERROR
      "zlib not found (needed with QUICKFAST_USE_SPDLOG). Install via Conan "
      "(zlib), vcpkg (zlib / feature spdlog), or zlib1g-dev, or set "
      "-DQUICKFAST_FETCH_DEPS=ON to download zlib 1.3.1.")
  endif()
  include(FetchContent)
  set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    zlib
    URL https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz
    URL_HASH SHA256=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(zlib)
  if(TARGET zlib AND NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlib)
  elseif(TARGET zlibstatic AND NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
  endif()
  if(NOT TARGET ZLIB::ZLIB)
    message(FATAL_ERROR
      "Fetched zlib 1.3.1 but neither zlib nor zlibstatic / ZLIB::ZLIB was created")
  endif()
  set(ZLIB_FOUND TRUE)
  message(STATUS "Using FetchContent zlib 1.3.1")
endif()

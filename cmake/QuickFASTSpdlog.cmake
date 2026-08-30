# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# Resolve spdlog (+ zlib) when QUICKFAST_USE_SPDLOG is ON.

find_package(spdlog 1.12 QUIET CONFIG)
if(spdlog_FOUND)
  message(STATUS "Using packaged spdlog (find_package)")
else()
  if(NOT QUICKFAST_FETCH_DEPS)
    message(FATAL_ERROR
      "spdlog >= 1.12 not found. Install via Conan (spdlog), vcpkg (spdlog), "
      "or a system package, or set -DQUICKFAST_FETCH_DEPS=ON to download v1.15.1.")
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

find_package(ZLIB REQUIRED)

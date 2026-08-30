# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# Resolve GoogleTest / GoogleMock: find_package then optional FetchContent.

set(QUICKFAST_GTEST_FETCH_TAG v1.15.2)

find_package(GTest QUIET CONFIG)
if(NOT GTest_FOUND)
  find_package(GTest QUIET)
endif()
if(GTest_FOUND AND TARGET GTest::gtest_main)
  message(STATUS "Using packaged GoogleTest (find_package)")
  return()
endif()

if(NOT QUICKFAST_FETCH_DEPS)
  message(FATAL_ERROR
    "GoogleTest not found. Install via Conan (gtest), vcpkg (gtest), or a system "
    "package, or set -DQUICKFAST_FETCH_DEPS=ON to download ${QUICKFAST_GTEST_FETCH_TAG}.")
endif()

include(FetchContent)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        ${QUICKFAST_GTEST_FETCH_TAG}
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(googletest)
foreach(_gtest_target gtest gtest_main gmock gmock_main)
  if(TARGET ${_gtest_target})
    target_compile_options(${_gtest_target} PRIVATE -w)
  endif()
endforeach()
message(STATUS "Using FetchContent GoogleTest ${QUICKFAST_GTEST_FETCH_TAG}")

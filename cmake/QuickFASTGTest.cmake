# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# Resolve GoogleTest / GoogleMock from Conan or vcpkg only.

find_package(GTest QUIET CONFIG)
if(NOT GTest_FOUND)
  find_package(GTest REQUIRED)
endif()
if(NOT TARGET GTest::gtest_main)
  message(FATAL_ERROR
    "GoogleTest found but GTest::gtest_main is missing. "
    "Install via Conan (gtest) or vcpkg (gtest).")
endif()
quickfast_report_dependency("GoogleTest" GTest_VERSION)

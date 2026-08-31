# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# Standalone consumer of an *installed* QuickFAST. This is deliberately not part
# of the main build: it must see the package only through find_package, the same
# way a downstream project does, so a broken install layout or a missing
# find_dependency in QuickFASTConfig.cmake fails here rather than in a user's
# tree. See "Consumer smoke test" in BUILD.md for how to run it.

cmake_minimum_required(VERSION 3.16)
project(quickfast_package_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QuickFAST 2.0.0 REQUIRED)

add_executable(quickfast_package_test main.cpp)
target_link_libraries(quickfast_package_test PRIVATE QuickFAST::QuickFAST)

message(STATUS
  "QuickFAST ${QuickFAST_VERSION}: spdlog=${QuickFAST_WITH_SPDLOG} "
  "libpcap=${QuickFAST_WITH_LIBPCAP} c-ares=${QuickFAST_WITH_CARES} "
  "shared=${QuickFAST_SHARED}")

enable_testing()
add_test(NAME consumer COMMAND quickfast_package_test)

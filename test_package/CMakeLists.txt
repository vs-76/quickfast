# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.

cmake_minimum_required(VERSION 3.16)
project(quickfast_test_package LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QuickFAST REQUIRED CONFIG)

add_executable(test_package main.cpp)
target_link_libraries(test_package PRIVATE QuickFAST::QuickFAST)

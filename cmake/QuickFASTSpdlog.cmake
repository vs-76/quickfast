# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# Resolve spdlog + zlib from Conan or vcpkg only when QUICKFAST_USE_SPDLOG is ON.

find_package(spdlog 1.12 REQUIRED CONFIG)
quickfast_report_dependency("spdlog" spdlog_VERSION)

find_package(ZLIB REQUIRED)
quickfast_report_dependency("zlib" ZLIB_VERSION)

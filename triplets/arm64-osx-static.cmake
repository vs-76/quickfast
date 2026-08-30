# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# Overlay triplet: static libraries on macOS arm64.

set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)

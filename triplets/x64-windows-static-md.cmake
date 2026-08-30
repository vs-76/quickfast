# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# Overlay triplet: static libraries on Windows x64 (dynamic CRT).

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

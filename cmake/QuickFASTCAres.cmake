# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# Resolve c-ares from Conan or vcpkg only (CONFIG packages).
# Sets QUICKFAST_CARES_TARGET to the imported/INTERFACE library to link.

find_package(c-ares REQUIRED CONFIG)
if(TARGET c-ares::cares)
  set(QUICKFAST_CARES_TARGET c-ares::cares)
elseif(TARGET cares)
  set(QUICKFAST_CARES_TARGET cares)
elseif(TARGET c-ares)
  set(QUICKFAST_CARES_TARGET c-ares)
else()
  message(FATAL_ERROR
    "find_package(c-ares) succeeded but no known imported target was created. "
    "Install c-ares via Conan (c-ares) or vcpkg (c-ares).")
endif()
quickfast_report_dependency("c-ares" c-ares_VERSION)

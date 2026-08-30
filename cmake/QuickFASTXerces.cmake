# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# Resolve Xerces-C++ from Conan or vcpkg only (>= 3.2.5 for CVE-2024-23807).
# Prefer CONFIG so vcpkg's XercesCConfig.cmake pulls ICU (static ICU transcoder);
# the CMake FindXercesC module finds libxerces-c.a but omits ICU link deps.

set(QUICKFAST_XERCES_MIN_VERSION 3.2.5)

find_package(XercesC ${QUICKFAST_XERCES_MIN_VERSION} CONFIG REQUIRED)
quickfast_report_dependency("Xerces-C" XercesC_VERSION)

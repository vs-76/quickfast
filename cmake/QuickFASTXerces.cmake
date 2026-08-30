# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# Resolve Xerces-C++ from Conan or vcpkg only (>= 3.2.5 for CVE-2024-23807).

set(QUICKFAST_XERCES_MIN_VERSION 3.2.5)

find_package(XercesC ${QUICKFAST_XERCES_MIN_VERSION} REQUIRED)
quickfast_report_dependency("Xerces-C" XercesC_VERSION)

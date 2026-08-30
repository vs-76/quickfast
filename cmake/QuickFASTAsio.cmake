# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# Resolve standalone Asio from Conan or vcpkg only.
# Sets QUICKFAST_ASIO_TARGET to the INTERFACE/IMPORTED library to link.

find_package(asio REQUIRED CONFIG)
if(TARGET asio::asio)
  set(QUICKFAST_ASIO_TARGET asio::asio)
elseif(TARGET asio)
  set(QUICKFAST_ASIO_TARGET asio)
else()
  message(FATAL_ERROR
    "find_package(asio) succeeded but neither asio::asio nor asio was created. "
    "Install Asio via Conan (asio) or vcpkg (asio).")
endif()
quickfast_report_dependency("Asio" asio_VERSION)

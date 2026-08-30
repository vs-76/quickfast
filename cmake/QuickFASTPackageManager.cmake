# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# Require Conan 2 or vcpkg for all linked third-party libraries, and disable
# CMake searches of system prefixes for those packages (Threads stays host).

function(quickfast_detect_package_manager)
  set(_qf_pm "")
  if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "conan_toolchain\\.cmake")
    set(_qf_pm "conan")
  elseif(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg\\.cmake")
    set(_qf_pm "vcpkg")
  elseif(DEFINED VCPKG_TOOLCHAIN OR (DEFINED VCPKG_TARGET_TRIPLET AND DEFINED ENV{VCPKG_ROOT}))
    # vcpkg.cmake may chain-load another toolchain; triplet + VCPKG_ROOT is a strong signal.
    if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
      set(_qf_pm "vcpkg")
    elseif(DEFINED VCPKG_TOOLCHAIN AND VCPKG_TOOLCHAIN)
      set(_qf_pm "vcpkg")
    endif()
  endif()

  if(_qf_pm STREQUAL "")
    message(FATAL_ERROR
      "QuickFAST refuses system-linked third-party libraries.\n"
      "Configure with Conan 2 or vcpkg only:\n"
      "  Conan:  conan install . -of build/conan -s build_type=Release --build=missing\n"
      "          cmake -S . -B build "
      "-DCMAKE_TOOLCHAIN_FILE=build/conan/conan_toolchain.cmake ...\n"
      "  vcpkg:  cmake -S . -B build "
      "-DCMAKE_TOOLCHAIN_FILE=\$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake "
      "-DVCPKG_OVERLAY_TRIPLETS=triplets "
      "-DVCPKG_TARGET_TRIPLET=<triplet> ...")
  endif()

  set(QUICKFAST_PACKAGE_MANAGER "${_qf_pm}" PARENT_SCOPE)
  message(STATUS "Package manager: ${_qf_pm} (system packages disabled for linked deps)")
endfunction()

function(quickfast_disable_system_dependency_search)
  # Host pthread/winpthread still found before this is called.
  set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH OFF PARENT_SCOPE)
  set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH OFF PARENT_SCOPE)
  set(CMAKE_FIND_USE_PACKAGE_REGISTRY OFF PARENT_SCOPE)
  set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY OFF PARENT_SCOPE)
  set(CMAKE_FIND_USE_INSTALL_PREFIX OFF PARENT_SCOPE)
  # Keep CMAKE_FIND_USE_CMAKE_PATH / CMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH so
  # Conan/vcpkg CMAKE_PREFIX_PATH and toolchain hints remain searchable.
endfunction()

quickfast_detect_package_manager()

# STATUS line: "-- Using <Name> <version>" (version required for linked deps).
function(quickfast_report_dependency nice_name version_var)
  set(_qf_ver "${${version_var}}")
  if(NOT _qf_ver)
    set(_qf_ver "unknown")
  endif()
  message(STATUS "Using ${nice_name} ${_qf_ver}")
endfunction()

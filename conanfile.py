# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# Conan 2 recipe for QuickFAST-ng. It serves two roles:
#
#   Development — install the third-party dependencies and a CMake toolchain,
#   then drive CMake by hand:
#
#     conan install . -of build/conan -s build_type=Release --build=missing
#     cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan/conan_toolchain.cmake \
#       -DCMAKE_BUILD_TYPE=Release
#     cmake --build build -j
#     ctest --test-dir build --output-on-failure
#
#   Packaging — build and package the library itself:
#
#     conan create . -s build_type=Release --build=missing -o '&:build_tests=False'
#
# There is deliberately no layout(), so `-of build/conan` puts
# conan_toolchain.cmake straight into build/conan/ as BUILD.md documents.
# cmake_layout would bury it under build/conan/build/<cfg>/generators/.

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from conan.tools.files import copy, load, rmdir
import os
import re


class QuickFASTConan(ConanFile):
    name = "quickfast-ng"
    package_type = "library"
    license = "BSD-3-Clause"
    homepage = "https://github.com/vs-76/quickfast-ng"
    url = "https://github.com/vs-76/quickfast-ng"
    description = (
        "Native C++ implementation of the FIX Adapted for STreaming (FAST) "
        "protocol; community fork of OCI QuickFAST"
    )
    topics = ("fast", "fix", "market-data", "codec", "protocol")
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "with_spdlog": [True, False],
        "with_pcap": [True, False],
        "with_cares": [True, False],
        "build_tests": [True, False],
    }
    default_options = {
        "shared": False,
        "with_spdlog": True,
        "with_pcap": True,
        "with_cares": True,
        "build_tests": True,
        "*:shared": False,
    }
    # Position-independent code is always on (CMAKE_POSITION_INDEPENDENT_CODE),
    # so there is no fPIC option to expose.
    exports_sources = (
        "CMakeLists.txt",
        "cmake/*",
        "src/*",
        "tests/*",
        "license.txt",
    )
    implements = ["auto_shared_fpic"]

    def set_version(self):
        # Single source of truth: project(QuickFAST VERSION x.y.z) in CMakeLists.
        if self.version:
            return
        cmakelists = load(self, os.path.join(self.recipe_folder, "CMakeLists.txt"))
        match = re.search(r"project\(QuickFAST\s+VERSION\s+([0-9.]+)", cmakelists)
        if not match:
            raise RuntimeError("cannot read the project version from CMakeLists.txt")
        self.version = match.group(1)

    def requirements(self):
        self.requires("xerces-c/3.3.0", transitive_headers=True)
        # Override xerces-c's icu/74.2 pin (vcpkg pins ICU separately at 78.3).
        self.requires("icu/78.2", override=True)
        self.requires("asio/1.38.2", transitive_headers=True)
        if self.options.with_spdlog:
            self.requires("spdlog/1.17.0", transitive_headers=True)
            # Direct dep for managed_file_sink_mt gzip; also satisfies spdlog consumers.
            self.requires("zlib/1.3.2")
        if self.options.with_pcap:
            self.requires("libpcap/1.10.6")
        if self.options.with_cares:
            self.requires("c-ares/1.34.8")

    def build_requirements(self):
        if self.options.build_tests:
            self.test_requires("gtest/1.18.0")

    def configure(self):
        # Prefer static archives for every dependency that offers a shared option.
        self.options["xerces-c"].shared = False
        if self.options.with_spdlog:
            self.options["spdlog"].shared = False
            self.options["zlib"].shared = False
        if self.options.with_pcap:
            self.options["libpcap"].shared = False
        if self.options.with_cares:
            self.options["c-ares"].shared = False
        if self.options.build_tests:
            self.options["gtest"].shared = False
        # ICU transcoder (icu/78.2 via requirements override); keep network off.
        self.options["xerces-c"].transcoder = "icu"
        self.options["xerces-c"].network = False
        self.options["icu"].shared = False

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.variables["QUICKFAST_USE_SPDLOG"] = bool(self.options.with_spdlog)
        tc.variables["QUICKFAST_USE_LIBPCAP"] = bool(self.options.with_pcap)
        tc.variables["QUICKFAST_USE_CARES"] = bool(self.options.with_cares)
        tc.variables["QUICKFAST_BUILD_TESTS"] = bool(self.options.build_tests)
        tc.variables["BUILD_SHARED_LIBS"] = bool(self.options.shared)
        # Examples and the analyzer target are developer tooling, not package content.
        tc.variables["QUICKFAST_BUILD_EXAMPLES"] = False
        tc.variables["QUICKFAST_ENABLE_PVS_STUDIO"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(self, "license.txt",
             src=self.source_folder,
             dst=os.path.join(self.package_folder, "licenses"))
        # The installed QuickFASTConfig.cmake targets non-Conan consumers. Conan
        # generates its own config for this package, and shipping both leaves
        # find_package(QuickFAST) ambiguous, so drop ours from the package.
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "share"))

    def package_info(self):
        self.cpp_info.libs = ["QuickFAST"]
        self.cpp_info.set_property("cmake_file_name", "QuickFAST")
        self.cpp_info.set_property("cmake_target_name", "QuickFAST::QuickFAST")

        self.cpp_info.defines = [
            "QUICKFAST_HAS_DLL=1" if self.options.shared else "QUICKFAST_HAS_DLL=0",
            "ASIO_STANDALONE",
            "ASIO_NO_DEPRECATED",
        ]
        if self.options.with_spdlog:
            self.cpp_info.defines.append("QUICKFAST_HAS_SPDLOG=1")
        if self.options.with_pcap:
            self.cpp_info.defines.append("QUICKFAST_HAVE_LIBPCAP=1")
        if self.options.with_cares:
            self.cpp_info.defines.append("QUICKFAST_HAVE_CARES=1")

        self.cpp_info.requires = ["xerces-c::xerces-c", "asio::asio"]
        if self.options.with_spdlog:
            self.cpp_info.requires += ["spdlog::spdlog", "zlib::zlib"]
        if self.options.with_pcap:
            self.cpp_info.requires.append("libpcap::libpcap")
        if self.options.with_cares:
            self.cpp_info.requires.append("c-ares::c-ares")

        if self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.system_libs.extend(["pthread", "m"])
        elif self.settings.os == "Windows":
            self.cpp_info.system_libs.extend(["ws2_32", "mswsock"])

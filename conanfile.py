# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# Conan 2 consumer recipe for QuickFAST third-party dependencies.
# CMake remains the build system; this file only installs packages and a toolchain.
# Dependencies and QuickFAST itself are built as static libraries by default.
#
#   conan install . -of build/conan -s build_type=Release --build=missing \
#     -o '&:with_spdlog=True' -o '&:with_pcap=True' -o '&:build_tests=True'
#   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan/conan_toolchain.cmake \
#     -DCMAKE_BUILD_TYPE=Release -DQUICKFAST_FETCH_DEPS=OFF
#   cmake --build build -j
#   ctest --test-dir build --output-on-failure

from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps


class QuickFASTConan(ConanFile):
    name = "quickfast"
    version = "1.5.0"
    package_type = "application"
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "with_spdlog": [True, False],
        "with_pcap": [True, False],
        "build_tests": [True, False],
    }
    default_options = {
        "with_spdlog": False,
        "with_pcap": True,
        "build_tests": True,
        "*:shared": False,
    }

    def requirements(self):
        self.requires("xerces-c/3.3.0")
        self.requires("asio/1.30.2")
        if self.options.with_spdlog:
            self.requires("spdlog/1.15.1")
            self.requires("zlib/[>=1.2.11 <2]")
        if self.options.with_pcap:
            self.requires("libpcap/1.10.4")

    def build_requirements(self):
        if self.options.build_tests:
            self.test_requires("gtest/1.16.0")

    def configure(self):
        # Prefer static archives for every dependency that offers a shared option.
        self.options["xerces-c"].shared = False
        if self.options.with_spdlog:
            self.options["spdlog"].shared = False
            self.options["zlib"].shared = False
        if self.options.with_pcap:
            self.options["libpcap"].shared = False
        if self.options.build_tests:
            self.options["gtest"].shared = False
        # Match FetchContent defaults: gnuiconv, no ICU.
        self.options["xerces-c"].transcoder = "gnuiconv"
        self.options["xerces-c"].network = False

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.variables["QUICKFAST_FETCH_DEPS"] = False
        tc.variables["QUICKFAST_USE_SPDLOG"] = bool(self.options.with_spdlog)
        tc.variables["QUICKFAST_USE_LIBPCAP"] = bool(self.options.with_pcap)
        tc.variables["QUICKFAST_BUILD_TESTS"] = bool(self.options.build_tests)
        tc.variables["BUILD_SHARED_LIBS"] = False
        tc.generate()

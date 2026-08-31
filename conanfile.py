# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# Conan 2 consumer recipe for QuickFAST third-party dependencies.
# CMake remains the build system; this file only installs packages and a toolchain.
# Dependencies and QuickFAST itself are built as static libraries by default.
#
#   conan install . -of build/conan -s build_type=Release --build=missing \
#     -o '&:with_spdlog=False' -o '&:with_pcap=True' -o '&:with_cares=False' \
#     -o '&:build_tests=True'
#   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan/conan_toolchain.cmake \
#     -DCMAKE_BUILD_TYPE=Release
#   cmake --build build -j
#   ctest --test-dir build --output-on-failure

from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps


class QuickFASTConan(ConanFile):
    name = "quickfast"
    version = "2.0.0"
    package_type = "application"
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "with_spdlog": [True, False],
        "with_pcap": [True, False],
        "with_cares": [True, False],
        "build_tests": [True, False],
    }
    default_options = {
        "with_spdlog": True,
        "with_pcap": True,
        "with_cares": True,
        "build_tests": True,
        "*:shared": False,
    }

    def requirements(self):
        self.requires("xerces-c/3.3.0")
        # Override xerces-c's icu/74.2 pin (vcpkg pins ICU separately at 78.3).
        self.requires("icu/78.2", override=True)
        self.requires("asio/1.38.2")
        if self.options.with_spdlog:
            self.requires("spdlog/1.17.0")
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
        tc.variables["BUILD_SHARED_LIBS"] = False
        tc.generate()

# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# A port describes a *released* tarball, so SHA512 below is the hash of the
# v${VERSION} archive on GitHub. That archive contains this file, so the hash
# cannot be known while the release is being cut: it is filled in immediately
# after the tag is pushed. `SHA512 0` is vcpkg's bootstrap idiom — the first
# install attempt fails and prints the actual hash.
#
# Refresh it with:  scripts/vcpkg-port-sha512.sh v2.0.0
#
# See "vcpkg port" in BUILD.md for consuming this port via --overlay-ports.

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO vs-76/quickfast-ng
    REF "v${VERSION}"
    SHA512 18cbe4fb51f4e0a4fa44b39b557f4736b7e7b2d9863a7218236e55775ba7d3f91ce4b94d97b8a4d8876394f716549a88436db174ebbfb041ba029599955b3959
    HEAD_REF develop
)

vcpkg_check_features(
    OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        cares   QUICKFAST_USE_CARES
        pcap    QUICKFAST_USE_LIBPCAP
        spdlog  QUICKFAST_USE_SPDLOG
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        -DQUICKFAST_BUILD_TESTS=OFF
        -DQUICKFAST_BUILD_EXAMPLES=OFF
        -DQUICKFAST_BUILD_FUZZERS=OFF
        -DQUICKFAST_ENABLE_PVS_STUDIO=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME QuickFAST CONFIG_PATH lib/cmake/QuickFAST)
vcpkg_copy_pdbs()

# Headers and the CMake package ship once, from the release tree.
file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

# QuickFAST installs its license under share/licenses; vcpkg wants
# share/${PORT}/copyright instead.
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/share/licenses")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/license.txt")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")

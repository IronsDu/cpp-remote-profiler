set(VCPKG_POLICY_EMPTY_PKG enabled)

vcpkg_from_gitee(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO "mirrors/cpp-remote-profiler"
    REF "v${VERSION}"
    SHA512 0
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DREMOTE_PROFILER_BUILD_EXAMPLES=OFF
        -DREMOTE_PROFILER_BUILD_TESTS=OFF
        -DREMOTE_PROFILER_INSTALL=ON
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/cpp-remote-profiler)

vcpkg_copy_tools(TOOL_NAMES profiler_example AUTO_CLEAN)

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

# Install license
file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/${PORT}/share/cpp-remote-profiler" RENAME copyright)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

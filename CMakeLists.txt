cmake_minimum_required(VERSION 3.15)

# Version configuration
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 1)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

project(cpp-remote-profiler VERSION ${PROJECT_VERSION})

message(STATUS "Project version: ${PROJECT_VERSION}")

# ============================================================================
# Build Options
# ============================================================================

option(BUILD_SHARED_LIBS "Build shared libraries (DLL)" ON)
option(REMOTE_PROFILER_INSTALL "Generate install target" ON)
option(REMOTE_PROFILER_BUILD_EXAMPLES "Build example programs" ON)
option(REMOTE_PROFILER_BUILD_TESTS "Build test programs" ON)

message(STATUS "Build options:")
message(STATUS "  BUILD_SHARED_LIBS: ${BUILD_SHARED_LIBS}")
message(STATUS "  REMOTE_PROFILER_INSTALL: ${REMOTE_PROFILER_INSTALL}")
message(STATUS "  REMOTE_PROFILER_BUILD_EXAMPLES: ${REMOTE_PROFILER_BUILD_EXAMPLES}")
message(STATUS "  REMOTE_PROFILER_BUILD_TESTS: ${REMOTE_PROFILER_BUILD_TESTS}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build type to RelWithDebInfo to include debug symbols
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# Add debug symbols for better profiling support
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -O2")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -g -O2")

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "CXX flags: ${CMAKE_CXX_FLAGS}")

# Find dependencies via vcpkg
find_package(Drogon CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Backward CONFIG REQUIRED)
find_package(absl REQUIRED)

# Find gperftools via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPERFTOOLS REQUIRED libprofiler libtcmalloc)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${GPERFTOOLS_INCLUDE_DIRS})

# Source files
set(PROFILER_SOURCES
    src/profiler_manager.cpp
    src/symbolize.cpp
    src/web_resources.cpp
    src/web_server.cpp
)

set(PROFILER_HEADERS
    include/profiler_manager.h
    include/symbolize.h
    include/web_resources.h
    include/web_server.h
)

# Create library
add_library(profiler_lib ${PROFILER_SOURCES} ${PROFILER_HEADERS})

# Private link libraries (not exported)
target_link_libraries(profiler_lib
    PRIVATE
        Backward::Backward
        absl::symbolize
        absl::stacktrace
        absl::debugging_internal
        absl::demangle_internal
        pthread
        ${CMAKE_DL_LIBS}
    PUBLIC
        ${GPERFTOOLS_LIBRARIES}
)

# Include directories
target_include_directories(profiler_lib
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include/cpp-remote-profiler>
)

# Example executable
if(REMOTE_PROFILER_BUILD_EXAMPLES)
    add_executable(profiler_example example/main.cpp example/workload.cpp)
    target_link_libraries(profiler_example
        profiler_lib
        Drogon::Drogon
    )
    message(STATUS "Example program will be built")
else()
    message(STATUS "Example program disabled")
endif()

# Tests
if(REMOTE_PROFILER_BUILD_TESTS)
    enable_testing()

    # CPU profile 测试
    add_executable(test_cpu_profile tests/test_cpu_profile.cpp)
    target_link_libraries(test_cpu_profile
        profiler_lib
        Drogon::Drogon
        GTest::gtest
        GTest::gtest_main
        pthread
    )

    add_test(NAME CPUProfileTest COMMAND test_cpu_profile)

    # Full flow 测试
    add_executable(test_full_flow tests/test_full_flow.cpp)
    target_link_libraries(test_full_flow
        profiler_lib
        Drogon::Drogon
        GTest::gtest
        GTest::gtest_main
        pthread
    )

    add_test(NAME FullFlowTest COMMAND test_full_flow)
    message(STATUS "Tests will be built")
else()
    message(STATUS "Tests disabled")
endif()

# Note: Web files are now embedded in C++ code (src/web_resources.cpp)
# No need to copy web directory to build directory

# ============================================================================
# Installation and Export
# ============================================================================

if(REMOTE_PROFILER_INSTALL)
    # Install library and export
    # Note: Only PUBLIC and INTERFACE link libraries will be exported
    # PRIVATE dependencies (Backward, absl) will not be visible to consumers
    install(TARGETS profiler_lib
        EXPORT cpp-remote-profiler-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION include/cpp-remote-profiler
    )

    # Install headers
    install(FILES ${PROFILER_HEADERS}
        DESTINATION include/cpp-remote-profiler
    )

    # Install version header
    install(FILES include/version.h
        DESTINATION include/cpp-remote-profiler
    )

    # Export targets
    install(EXPORT cpp-remote-profiler-targets
        FILE cpp-remote-profiler-targets.cmake
        NAMESPACE cpp-remote-profiler::
        DESTINATION lib/cmake/cpp-remote-profiler
    )

    # Create and install config files
    include(CMakePackageConfigHelpers)
    include(GNUInstallDirs)

    # Generate version file
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/cpp-remote-profiler-config-version.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion
    )

    # Configure package config file from template
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cpp-remote-profiler-config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cpp-remote-profiler-config.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-remote-profiler
        PATH_VARS
            CMAKE_INSTALL_INCLUDEDIR
            CMAKE_INSTALL_LIBDIR
            CMAKE_INSTALL_PREFIX
    )

    # Install config files
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/cpp-remote-profiler-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/cpp-remote-profiler-config-version.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-remote-profiler
    )

    message(STATUS "Install targets will be generated")
else()
    message(STATUS "Install targets disabled")
endif()


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
option(REMOTE_PROFILER_ENABLE_WEB "Enable web UI (requires Drogon)" ON)
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)

option(BUILD_DOCS "Build API documentation" OFF)

message(STATUS "Build options:")
message(STATUS "  BUILD_SHARED_LIBS: ${BUILD_SHARED_LIBS}")
message(STATUS "  REMOTE_PROFILER_INSTALL: ${REMOTE_PROFILER_INSTALL}")
message(STATUS "  REMOTE_PROFILER_BUILD_EXAMPLES: ${REMOTE_PROFILER_BUILD_EXAMPLES}")
message(STATUS "  REMOTE_PROFILER_BUILD_TESTS: ${REMOTE_PROFILER_BUILD_TESTS}")
message(STATUS "  REMOTE_PROFILER_ENABLE_WEB: ${REMOTE_PROFILER_ENABLE_WEB}")
message(STATUS "  ENABLE_COVERAGE: ${ENABLE_COVERAGE}")
message(STATUS "  BUILD docs: ${BUILD_DOCS}")

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

# Code coverage configuration
if(ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(WARNING "Code coverage only works with GCC or Clang")
    endif()
    message(STATUS "Code coverage enabled")
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "CXX flags: ${CMAKE_CXX_FLAGS}")

# Find core dependencies via vcpkg / pkg-config
find_package(GTest CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Backward CONFIG REQUIRED)
find_package(absl REQUIRED)

# Find gperftools via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPERFTOOLS REQUIRED libprofiler libtcmalloc)

# Find web dependencies (optional)
if(REMOTE_PROFILER_ENABLE_WEB)
    find_package(Drogon CONFIG REQUIRED)
endif()

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${GPERFTOOLS_INCLUDE_DIRS})

# ============================================================================
# Version Configuration
# ============================================================================
# Generate profiler_version.h with version information and namespace macros
set(PROFILER_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(PROFILER_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(PROFILER_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(PROFILER_VERSION_NAMESPACE v${PROJECT_VERSION_MAJOR}_${PROJECT_VERSION_MINOR}_${PROJECT_VERSION_PATCH})

configure_file(
    ${PROJECT_SOURCE_DIR}/include/profiler_version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/profiler_version.h
    @ONLY
)

message(STATUS "Profiler version: ${PROJECT_VERSION}")
message(STATUS "Profiler version namespace: ${PROFILER_VERSION_NAMESPACE}")

# ============================================================================
# Core library (always built, no Drogon/spdlog dependency)
# ============================================================================

set(PROFILER_CORE_SOURCES
    src/profiler_manager.cpp
    src/symbolize.cpp
    src/http_handlers.cpp
    src/internal/log_manager.cpp
    src/internal/default_log_sink.cpp
)

set(PROFILER_CORE_HEADERS
    include/profiler_manager.h
    include/profiler/log_sink.h
    include/profiler/logger.h
    include/profiler/http_handlers.h
)

add_library(profiler_core ${PROFILER_CORE_SOURCES} ${PROFILER_CORE_HEADERS})

target_link_libraries(profiler_core
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

target_include_directories(profiler_core
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include/cpp-remote-profiler>
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

# Note: Symbol visibility can be controlled with CXX_VISIBILITY_PRESET hidden
# once proper export macros are added to the public API.
# For now, default visibility is used.

# ============================================================================
# Web library (optional, requires Drogon)
# ============================================================================

if(REMOTE_PROFILER_ENABLE_WEB)
    set(PROFILER_WEB_SOURCES
        src/drogon_adapter.cpp
        src/web_resources.cpp
    )

    set(PROFILER_WEB_HEADERS
        include/profiler/drogon_adapter.h
    )

    add_library(profiler_web ${PROFILER_WEB_SOURCES} ${PROFILER_WEB_HEADERS})

    target_link_libraries(profiler_web
        PUBLIC
            profiler_core
        PRIVATE
            Drogon::Drogon
            pthread
    )

    target_include_directories(profiler_web
        PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    message(STATUS "Web UI enabled (profiler_web target)")
else()
    message(STATUS "Web UI disabled")
endif()

# ============================================================================
# Example executable
# ============================================================================

if(REMOTE_PROFILER_BUILD_EXAMPLES)
    add_executable(profiler_example example/main.cpp example/workload.cpp)
    if(REMOTE_PROFILER_ENABLE_WEB)
        target_link_libraries(profiler_example
            profiler_web
            Drogon::Drogon
        )
        target_compile_definitions(profiler_example PRIVATE REMOTE_PROFILER_ENABLE_WEB)
    else()
        target_link_libraries(profiler_example profiler_core)
    endif()
    message(STATUS "Example program will be built")
else()
    message(STATUS "Example program disabled")
endif()

# ============================================================================
# Tests
# ============================================================================

if(REMOTE_PROFILER_BUILD_TESTS)
    enable_testing()

    # CPU profile test
    add_executable(test_cpu_profile tests/test_cpu_profile.cpp)
    target_link_libraries(test_cpu_profile
        profiler_core
        GTest::gtest
        GTest::gtest_main
        pthread
    )
    add_test(NAME CPUProfileTest COMMAND test_cpu_profile)

    # Full flow test
    add_executable(test_full_flow tests/test_full_flow.cpp)
    if(REMOTE_PROFILER_ENABLE_WEB)
        target_link_libraries(test_full_flow
            profiler_web
            Drogon::Drogon
            GTest::gtest
            GTest::gtest_main
            pthread
        )
    else()
        target_link_libraries(test_full_flow
            profiler_core
            GTest::gtest
            GTest::gtest_main
            pthread
        )
    endif()
    add_test(NAME FullFlowTest COMMAND test_full_flow)

    # Logger test
    add_executable(test_logger tests/test_logger.cpp)
    target_link_libraries(test_logger
        profiler_core
        GTest::gtest
        GTest::gtest_main
        pthread
    )
    add_test(NAME LoggerTest COMMAND test_logger)
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
    # Install core library
    install(TARGETS profiler_core
        EXPORT cpp-remote-profiler-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION include/cpp-remote-profiler
    )

    # Install web library (if built)
    if(REMOTE_PROFILER_ENABLE_WEB)
        install(TARGETS profiler_web
            EXPORT cpp-remote-profiler-targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()

    # Install core headers
    install(FILES ${PROFILER_CORE_HEADERS}
        DESTINATION include/cpp-remote-profiler
    )

    # Install profiler subdirectory headers
    install(FILES
        include/profiler/log_sink.h
        include/profiler/logger.h
        include/profiler/http_handlers.h
        DESTINATION include/cpp-remote-profiler/profiler
    )

    # Install web header (if built)
    if(REMOTE_PROFILER_ENABLE_WEB)
        install(FILES include/profiler/drogon_adapter.h
            DESTINATION include/cpp-remote-profiler/profiler
        )
    endif()

    # Install version header
    install(FILES include/version.h
        DESTINATION include/cpp-remote-profiler
    )

    # Install generated profiler_version.h
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/profiler_version.h
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

# ============================================================================
# Documentation
# ============================================================================

if(BUILD_DOCS)
    find_package(Doxygen)

    if(DOxygen_FOUND)
        # Check for dot (Graphviz) for diagrams
        find_program(DOT_EXECUTABLE)

        if(DOT_EXECUTABLE)
            set(DOXYGEN_HAVE_DOT "YES")
        else()
            set(DOXYGEN_HAVE_DOT "NO")
        endif()

        # Configure Doxyfile
        configure_file(
            ${PROJECT_SOURCE_DIR}/docs/Doxyfile.in
            ${CMAKE_CURRENT_BINARY_DIR}/docs/Doxyfile
            @ONLY
        )

        # Add docs target
        add_custom_target(docs ALL
            COMMAND ${DOxygen_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/docs/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation"
        )

        # Install docs (optional)
        install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs/html
            DESTINATION share/doc/cpp-remote-profiler
        )

        message(STATUS "Documentation will be generated")
    else()
        message(WARNING "Doxygen not found - documentation will not be generated")
    endif()
endif()

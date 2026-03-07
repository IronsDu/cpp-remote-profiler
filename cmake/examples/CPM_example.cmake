# ============================================================================
# CPM.cmake Integration Example for cpp-remote-profiler
# ============================================================================
#
# CPM.cmake is a lightweight CMake package manager that uses FetchContent
# under the hood but provides a simpler interface and caching support.
#
# Usage:
#   1. Include CPM.cmake in your project
#   2. Use CPMAddPackage() to add cpp-remote-profiler
#
# Full CPM.cmake documentation: https://github.com/cpm-cmake/CPM.cmake
# ============================================================================

# ============================================================================
# Step 1: Include CPM.cmake
# ============================================================================

set(CPM_DOWNLOAD_VERSION 0.40.2)

if(NOT EXISTS "${CMAKE_BINARY_DIR}/cpm/CPM.cmake")
    message(STATUS "Downloading CPM.cmake v${CPM_DOWNLOAD_VERSION}...")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/cpm")
    file(
        DOWNLOAD
        "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
        "${CMAKE_BINARY_DIR}/cpm/CPM.cmake"
        SHOW_PROGRESS
    )
endif()

include("${CMAKE_BINARY_DIR}/cpm/CPM.cmake")

# ============================================================================
# Step 2: Add cpp-remote-profiler using CPM
# ============================================================================

# Option 1: From GitHub release tag
CPMAddPackage(
    NAME cpp-remote-profiler
    GITHUB_REPOSITORY your-org/cpp-remote-profiler
    GIT_TAG v0.1.0
    OPTIONS
        "REMOTE_PROFILER_BUILD_EXAMPLES OFF"
        "REMOTE_PROFILER_BUILD_TESTS OFF"
        "REMOTE_PROFILER_INSTALL OFF"
)

# Option 2: From GitHub branch
# CPMAddPackage(
#     NAME cpp-remote-profiler
#     GITHUB_REPOSITORY your-org/cpp-remote-profiler
#     GIT_TAG main
#     OPTIONS
#         "REMOTE_PROFILER_BUILD_EXAMPLES OFF"
#         "REMOTE_PROFILER_BUILD_TESTS OFF"
#         "REMOTE_PROFILER_INSTALL OFF"
# )

# Option 3: From local directory (for development)
# CPMAddPackage(
#     NAME cpp-remote-profiler
#     SOURCE_DIR /path/to/cpp-remote-profiler
#     OPTIONS
#         "REMOTE_PROFILER_BUILD_EXAMPLES OFF"
#         "REMOTE_PROFILER_BUILD_TESTS OFF"
#         "REMOTE_PROFILER_INSTALL OFF"
# )

# ============================================================================
# Step 3: Link to your target
# ============================================================================

# target_link_libraries(your_target
#     cpp-remote-profiler::profiler_lib
#     Drogon::Drogon  # Required if using web features
# )

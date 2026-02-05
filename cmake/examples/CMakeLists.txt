cmake_minimum_required(VERSION 3.15)

project(FetchContentExample VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# Option 1: FetchContent from GitHub
# ============================================================================

include(FetchContent)

FetchContent_Declare(
    cpp-remote-profiler
    GIT_REPOSITORY https://github.com/your-org/cpp-remote-profiler.git
    GIT_TAG v0.1.0
    GIT_SHALLOW TRUE
)

# Configure cpp-remote-profiler options
set(REMOTE_PROFILER_BUILD_EXAMPLES OFF CACHE BOOL "")
set(REMOTE_PROFILER_BUILD_TESTS OFF CACHE BOOL "")
set(REMOTE_PROFILER_INSTALL OFF CACHE BOOL "")

# Fetch and make available
FetchContent_MakeAvailable(cpp-remote-profiler)

# Find dependencies (manual - since we're not using vcpkg here)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPERFTOOLS REQUIRED libprofiler libtcmalloc)
find_package(Drogon REQUIRED)

# ============================================================================
# Build example executable
# ============================================================================

add_executable(fetch_content_example FetchContent_integration_example.cpp)

target_link_libraries(fetch_content_example
    profiler_lib
    Drogon::Drogon
    ${GPERFTOOLS_LIBRARIES}
)

# ============================================================================
# Alternative: Option 2 - Using local path
# ============================================================================

# If you have cpp-remote-profiler locally:
# add_subdirectory(/path/to/cpp-remote-profiler)

# Then link:
# target_link_libraries(your_app profiler_lib)

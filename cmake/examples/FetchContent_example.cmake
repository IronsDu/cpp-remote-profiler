# ============================================================================
# FetchContent Integration Example for cpp-remote-profiler
# ============================================================================
#
# This file demonstrates how to use FetchContent to include
# cpp-remote-profiler in your project without installing it.
#
# Usage:
#   In your project's CMakeLists.txt, add:
#     include(cmake/FetchContent_example.cmake)
#
# Or copy the content to your CMakeLists.txt directly.
# ============================================================================

include(FetchContent)

# Method 1: From GitHub (release)
FetchContent_Declare(
    cpp-remote-profiler
    GIT_REPOSITORY https://github.com/your-org/cpp-remote-profiler.git
    GIT_TAG v0.1.0
    GIT_SHALLOW TRUE
)

# Method 2: From GitHub (branch)
# FetchContent_Declare(
#     cpp-remote-profiler
#     GIT_REPOSITORY https://github.com/your-org/cpp-remote-profiler.git
#     GIT_TAG main
#     GIT_SHALLOW TRUE
# )

# Method 3: From URL (archive)
# FetchContent_Declare(
#     cpp-remote-profiler
#     URL https://github.com/your-org/cpp-remote-profiler/archive/refs/tags/v0.1.0.tar.gz
#     URL_HASH SHA256=<compute_hash>
# )

# Set options for cpp-remote-profiler
set(REMOTE_PROFILER_BUILD_EXAMPLES OFF CACHE BOOL "")
set(REMOTE_PROFILER_BUILD_TESTS OFF CACHE BOOL "")
set(REMOTE_PROFILER_INSTALL OFF CACHE BOOL "")
set(BUILD_SHARED_LIBS ON CACHE BOOL "")

# Make available
FetchContent_MakeAvailable(cpp-remote-profiler)

# Link to your target
# target_link_libraries(your_target
#     cpp-remote-profiler::profiler_lib
#     Drogon::Drogon  # Required if using web features
# )

cmake_minimum_required(VERSION 3.15)
project(test_fetch_content VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Disable cpp-remote-profiler's own examples/tests/install to avoid conflicts
set(REMOTE_PROFILER_BUILD_EXAMPLES OFF CACHE BOOL "")
set(REMOTE_PROFILER_BUILD_TESTS OFF CACHE BOOL "")
set(REMOTE_PROFILER_INSTALL OFF CACHE BOOL "")

# Use add_subdirectory to bring in cpp-remote-profiler
# This simulates what FetchContent does at configure time
# SOURCE_DIR is passed by the CI test step
if(NOT DEFINED PROFILER_SOURCE_DIR)
    message(FATAL_ERROR "PROFILER_SOURCE_DIR must be set to the cpp-remote-profiler source directory")
endif()

add_subdirectory(${PROFILER_SOURCE_DIR} ${CMAKE_BINARY_DIR}/cpp-remote-profiler)

# Test: link against profiler_core
add_executable(test_core main.cpp)
target_link_libraries(test_core profiler_core)
# Include generated profiler_version.h from the subproject's binary dir
target_include_directories(test_core PRIVATE ${CMAKE_BINARY_DIR}/cpp-remote-profiler)

# Test: link against profiler_web (if available)
if(TARGET profiler_web)
    add_executable(test_web main.cpp)
    target_link_libraries(test_web profiler_web)
    target_include_directories(test_web PRIVATE ${CMAKE_BINARY_DIR}/cpp-remote-profiler)
    message(STATUS "test_fetch_content: profiler_web available, building test_web")
else()
    message(STATUS "test_fetch_content: profiler_web not available, skipping test_web")
endif()

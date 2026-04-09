cmake_minimum_required(VERSION 3.15)
project(test_find_package VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the installed cpp-remote-profiler package
find_package(cpp-remote-profiler REQUIRED)

# Test 1: Link against profiler_core (always available)
add_executable(test_core main.cpp)
target_link_libraries(test_core cpp-remote-profiler::profiler_core)

# Test 2: If profiler_web is available, test linking against it
if(TARGET cpp-remote-profiler::profiler_web)
    add_executable(test_web main.cpp)
    target_link_libraries(test_web cpp-remote-profiler::profiler_web)
    message(STATUS "test_find_package: profiler_web available, building test_web")
else()
    message(STATUS "test_find_package: profiler_web not available, skipping test_web")
endif()

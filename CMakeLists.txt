cmake_minimum_required(VERSION 3.15)
project(cpp-remote-profiler VERSION 1.0.0)

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
target_link_libraries(profiler_lib
    ${GPERFTOOLS_LIBRARIES}
    Backward::Backward
    absl::symbolize
    ${CMAKE_DL_LIBS}
)

# Example executable
add_executable(profiler_example example/main.cpp example/workload.cpp)
target_link_libraries(profiler_example
    profiler_lib
    Drogon::Drogon
)

# Tests
enable_testing()

# CPU profile 测试
add_executable(test_cpu_profile tests/test_cpu_profile.cpp)
target_link_libraries(test_cpu_profile
    profiler_lib
    GTest::gtest
    GTest::gtest_main
    pthread
)

add_test(NAME CPUProfileTest COMMAND test_cpu_profile)

# Full flow 测试
add_executable(test_full_flow tests/test_full_flow.cpp)
target_link_libraries(test_full_flow
    profiler_lib
    GTest::gtest
    GTest::gtest_main
    pthread
)

add_test(NAME FullFlowTest COMMAND test_full_flow)

# Note: Web files are now embedded in C++ code (src/web_resources.cpp)
# No need to copy web directory to build directory


cmake_minimum_required(VERSION 3.15)
project(cpp-remote-profiler VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies via vcpkg
find_package(Drogon CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

# Find gperftools via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPERFTOOLS REQUIRED libprofiler libtcmalloc)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${GPERFTOOLS_INCLUDE_DIRS})

# Source files
set(PROFILER_SOURCES
    src/profiler_manager.cpp
)

set(PROFILER_HEADERS
    include/profiler_manager.h
)

# Create library
add_library(profiler_lib ${PROFILER_SOURCES} ${PROFILER_HEADERS})
target_link_libraries(profiler_lib
    ${GPERFTOOLS_LIBRARIES}
    Drogon::Drogon
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    pthread
)

# Example executable
add_executable(profiler_example example/main.cpp)
target_link_libraries(profiler_example profiler_lib)

# Tests
enable_testing()

# 编译测试
add_executable(profiler_test tests/profiler_test.cpp)
target_link_libraries(profiler_test
    profiler_lib
    GTest::gtest
    GTest::gtest_main
    pthread
)

add_test(NAME ProfilerTest COMMAND profiler_test)

# Copy web files to build directory
file(COPY ${PROJECT_SOURCE_DIR}/web DESTINATION ${CMAKE_BINARY_DIR})

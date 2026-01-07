cmake_minimum_required(VERSION 3.15)
project(cpp-remote-profiler VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPERFTOOLS REQUIRED libprofiler)
pkg_check_modules(TCMALLOC REQUIRED libtcmalloc_minimal)

find_package(Drogon CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

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
    tcmalloc_and_profiler
    profiler
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
find_package(GTest REQUIRED)

# 创建GTest库
include_directories(/usr/include)

# 编译测试
add_executable(profiler_test tests/profiler_test.cpp)
target_link_libraries(profiler_test
    profiler_lib
    pthread
    /usr/lib/x86_64-linux-gnu/libgtest.a
    /usr/lib/x86_64-linux-gnu/libgtest_main.a
)

add_test(NAME ProfilerTest COMMAND profiler_test)

# Copy web files to build directory
file(COPY ${PROJECT_SOURCE_DIR}/web DESTINATION ${CMAKE_BINARY_DIR})

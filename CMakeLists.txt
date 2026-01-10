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
find_package(Protobuf CONFIG REQUIRED)
find_package(Backward CONFIG REQUIRED)
find_package(absl REQUIRED)

# Find gperftools via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPERFTOOLS REQUIRED libprofiler libtcmalloc)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/third_party/profile)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${GPERFTOOLS_INCLUDE_DIRS})
include_directories(${Protobuf_INCLUDE_DIRS})

# Source files
set(PROFILER_SOURCES
    src/profiler_manager.cpp
    src/stack_collector.cpp
    src/profile_parser.cpp
    src/symbolize.cpp
)

set(PROFILER_HEADERS
    include/profiler_manager.h
    include/stack_collector.h
    include/profile_parser.h
    include/symbolize.h
)

# Generate protobuf files
set(PROTO_PATH ${PROJECT_SOURCE_DIR}/third_party/profile)
set(PROTO_SRC ${PROTO_PATH}/profile.proto)

# 使用 protobuf_generate 生成代码，并关联到目标
protobuf_generate(
    TARGETS profiler_lib profiler_example profiler_test
    LANGUAGE cpp
    OUT_VAR PROTO_FILES
    PROTOS ${PROTO_SRC}
)

message(STATUS "Generated protobuf files: ${PROTO_FILES}")
message(STATUS "Protobuf include dirs: ${Protobuf_INCLUDE_DIRS}")
message(STATUS "CMAKE_CURRENT_BINARY_DIR: ${CMAKE_CURRENT_BINARY_DIR}")

# Create library
add_library(profiler_lib ${PROFILER_SOURCES} ${PROFILER_HEADERS} ${PROTO_FILES})
target_link_libraries(profiler_lib
    ${GPERFTOOLS_LIBRARIES}
    protobuf::libprotobuf
    Backward::Backward
    absl::symbolize
    ${CMAKE_DL_LIBS}
)

# Example executable
add_executable(profiler_example example/main.cpp ${PROTO_FILES})
target_link_libraries(profiler_example
    profiler_lib
    Drogon::Drogon
)

# Tests
enable_testing()

# 编译测试
add_executable(profiler_test tests/profiler_test.cpp ${PROTO_FILES})
target_link_libraries(profiler_test
    profiler_lib
    GTest::gtest
    GTest::gtest_main
    pthread
)

add_test(NAME ProfilerTest COMMAND profiler_test)

# CPU profile 解析测试
add_executable(cpu_profile_test tests/test_cpu_profile.cpp ${PROTO_FILES})
target_link_libraries(cpu_profile_test
    profiler_lib
    GTest::gtest
    GTest::gtest_main
    pthread
)

add_test(NAME CPUProfileTest COMMAND cpu_profile_test)

# Full flow 测试
add_executable(test_full_flow tests/test_full_flow.cpp ${PROTO_FILES})
target_link_libraries(test_full_flow
    profiler_lib
    GTest::gtest
    GTest::gtest_main
    pthread
)

add_test(NAME FullFlowTest COMMAND test_full_flow)


# Copy web files to build directory
file(COPY ${PROJECT_SOURCE_DIR}/web DESTINATION ${CMAKE_BINARY_DIR})

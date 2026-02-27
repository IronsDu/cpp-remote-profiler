# TSAN (ThreadSanitizer) Configuration
# Usage: cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON ..

option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(ENABLE_TSAN)
    message(STATUS "ThreadSanitizer enabled")

    # Add TSAN compiler/linker flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -g -O1")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread -g -O1")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=thread")

    # Set suppression file path
    set(TSAN_SUPPRESSIONS_FILE "${PROJECT_SOURCE_DIR}/tsan.supp")

    # Create test runner script with TSAN options
    configure_file(
        "${PROJECT_SOURCE_DIR}/cmake/tsan_test.sh.in"
        "${CMAKE_BINARY_DIR}/tsan_test.sh"
        @ONLY
    )
endif()

cmake_minimum_required(VERSION 3.10)

project(RightwareMemoryAllocator 
    VERSION 1.0.0
    DESCRIPTION "Deterministic Memory Allocator for Embedded Systems"
    LANGUAGES CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic -Werror")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -Wall -Wextra")

# Default to Release build
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Create static library
add_library(memory_allocator STATIC
    src/memory_allocator.cpp
)

target_include_directories(memory_allocator PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Test executable
add_executable(test_memory_allocator
    tests/test_memory_allocator.cpp
)

target_link_libraries(test_memory_allocator 
    memory_allocator
)

# Example executable
add_executable(example_usage
    examples/example_usage.cpp
)

target_link_libraries(example_usage 
    memory_allocator
)

# Performance benchmark executable
add_executable(benchmark
    tests/benchmark.cpp
)

target_link_libraries(benchmark 
    memory_allocator
)

# Install rules
install(TARGETS memory_allocator
    EXPORT MemoryAllocatorTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY include/
    DESTINATION include
)

# Enable testing
enable_testing()

# Add test
add_test(NAME memory_allocator_tests 
         COMMAND test_memory_allocator)

# Custom target to run tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS test_memory_allocator
)

# Print build configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug flags: ${CMAKE_CXX_FLAGS_DEBUG}")
else()
    message(STATUS "Release flags: ${CMAKE_CXX_FLAGS_RELEASE}")
endif()

cmake_minimum_required(VERSION 3.20)

# Project Definition
project(
    MatchingEngine
    VERSION 0.1.0
    LANGUAGES CXX
)

# Enforce out-of-source builds
if(CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR "In-source builds are disabled. Please create a separate build directory (e.g., 'mkdir build && cd build').")
endif()

# Global C++20 Standard Settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Option Flags
option(ENABLE_NATIVE_OPTIMIZATION "Build with -march=native for ultra-low latency" ON)
option(BUILD_TESTS "Build GoogleTest unit test suite" ON)  # Enabled by default
option(BUILD_BENCHMARKS "Build Google Benchmark performance benchmarks" OFF)

# Set Default Build Type to Release if unspecified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

# Compiler-Specific Low-Latency Optimization Flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
    )
    
    # Release-specific high-performance flags
    add_compile_options(
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-fno-omit-frame-pointer>
        $<$<CONFIG:Release>:-ffast-math>
    )

    if(ENABLE_NATIVE_OPTIMIZATION)
        add_compile_options($<$<CONFIG:Release>:-march=native>)
    endif()

elseif(MSVC)
    add_compile_options(/W4 /permissive- /O2 /Gy)
endif()

# Include Directories Configuration
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Core Library Target (Header-only or Static Policy Engine)
add_library(matching_engine_core STATIC
    src/engine/matching_engine_ticker.cpp
)

target_include_directories(matching_engine_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(matching_engine_core PUBLIC cxx_std_20)

# Main Executable Target
add_executable(matching_engine
    src/main.cpp
)

target_link_libraries(matching_engine
    PRIVATE
        matching_engine_core
)

# Platform Threading Requirement (Pthreads for CPU affinity)
find_package(Threads REQUIRED)
target_link_libraries(matching_engine_core PRIVATE Threads::Threads)

# ==============================================================================
# GoogleTest Integration (Automated via FetchContent)
# ==============================================================================
if(BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    
    # Fetch GoogleTest v1.14.0 directly from GitHub
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    
    # Prevent overriding parent compiler/linker settings on Windows/MSVC
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    
    # Suppress installation rules for GoogleTest targets when building main project
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(googletest)

    # Register Unit Test Executable
    add_executable(unit_tests
        tests/test_matching.cpp
    )

    target_link_libraries(unit_tests
        PRIVATE
            matching_engine_core
            GTest::gtest_main
    )

    # Enables `ctest` command discovery
    include(GoogleTest)
    gtest_discover_tests(unit_tests)
endif()

# ==============================================================================
# Benchmarking Configuration (Optional)
# ==============================================================================
if(BUILD_BENCHMARKS)
    include(FetchContent)
    
    FetchContent_Declare(
        benchmark
        URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)

    add_executable(latency_benchmarks
        benchmarks/bench_matching.cpp
    )

    target_link_libraries(latency_benchmarks
        PRIVATE
            matching_engine_core
            benchmark::benchmark
    )
endif()

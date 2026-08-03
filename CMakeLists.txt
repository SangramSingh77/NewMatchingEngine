cmake_minimum_required(VERSION 3.20)

project(NewMatchingEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build GoogleTest suite" ON)
option(BUILD_BENCHMARKS "Build basic matching-engine benchmark" OFF)

add_library(matching_engine_core STATIC
    src/matching_engine.cpp
)
target_include_directories(matching_engine_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(matching_engine_core PRIVATE -O3 -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

add_executable(matching_engine src/main.cpp)
target_link_libraries(matching_engine PRIVATE matching_engine_core)

if(BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    add_executable(matching_engine_tests tests/test_engine.cpp)
    target_link_libraries(matching_engine_tests PRIVATE matching_engine_core GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(matching_engine_tests)
endif()

if(BUILD_BENCHMARKS)
    add_executable(matching_engine_benchmark benchmarks/benchmark_matching.cpp)
    target_link_libraries(matching_engine_benchmark PRIVATE matching_engine_core)
endif()

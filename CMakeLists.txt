cmake_minimum_required(VERSION 3.20)
project(cw1_quicksort LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE
        Release
        CACHE STRING "Build type" FORCE
    )
endif()
option(NATIVE "Optimize for the build machine" OFF)
option(SANITIZE "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(ENABLE_CLANG_TIDY "Run clang-tidy on project sources during compilation" OFF)
option(WARNINGS_AS_ERRORS "Treat project compiler warnings as errors" ON)
find_package(Threads REQUIRED)
foreach(dependency parlaylib googletest benchmark)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/${dependency}/CMakeLists.txt")
        message(FATAL_ERROR "Run: git submodule update --init --recursive")
    endif()
endforeach()
set(INSTALL_GTEST
    OFF
    CACHE BOOL "" FORCE
)
set(BUILD_GMOCK
    OFF
    CACHE BOOL "" FORCE
)
set(gtest_force_shared_crt
    ON
    CACHE BOOL "" FORCE
)
set(BENCHMARK_ENABLE_TESTING
    OFF
    CACHE BOOL "" FORCE
)
set(BENCHMARK_ENABLE_INSTALL
    OFF
    CACHE BOOL "" FORCE
)
set(BENCHMARK_ENABLE_WERROR
    OFF
    CACHE BOOL "" FORCE
)
add_subdirectory(third_party/googletest EXCLUDE_FROM_ALL)
add_subdirectory(third_party/benchmark EXCLUDE_FROM_ALL)
add_library(quicksort INTERFACE)
target_include_directories(quicksort INTERFACE include)
target_include_directories(quicksort SYSTEM INTERFACE third_party/parlaylib/include)
target_link_libraries(quicksort INTERFACE Threads::Threads)
if(MSVC)
    target_compile_options(quicksort INTERFACE /W4 /permissive-)
else()
    target_compile_options(
        quicksort
        INTERFACE -Wall
                  -Wextra
                  -Wpedantic
                  -Wshadow
                  -Wconversion
                  -Wsign-conversion
                  -Wformat=2
                  -Wundef
                  -Wnull-dereference
    )
endif()
if(WARNINGS_AS_ERRORS)
    if(MSVC)
        target_compile_options(quicksort INTERFACE /WX)
    else()
        target_compile_options(quicksort INTERFACE -Werror)
    endif()
endif()
if(NATIVE AND NOT MSVC)
    target_compile_options(quicksort INTERFACE -march=native)
endif()
if(SANITIZE)
    if(MSVC)
        message(FATAL_ERROR "SANITIZE requires GCC or Clang with ASan and UBSan")
    endif()
    target_compile_options(quicksort INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(quicksort INTERFACE -fsanitize=address,undefined)
endif()
add_executable(quicksort_benchmark src/benchmark.cpp)
target_link_libraries(quicksort_benchmark PRIVATE quicksort benchmark::benchmark)
add_executable(correctness tests/correctness.cpp)
target_link_libraries(correctness PRIVATE quicksort GTest::gtest_main)

find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy clang-tidy-23)
if(ENABLE_CLANG_TIDY AND NOT CLANG_TIDY_EXECUTABLE)
    message(FATAL_ERROR "Install clang-tidy 23.1.4: pipx install clang-tidy==23.1.4")
endif()
if(CLANG_TIDY_EXECUTABLE)
    if(ENABLE_CLANG_TIDY)
        set_target_properties(
            quicksort_benchmark correctness
            PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE};--experimental-custom-checks"
        )
    endif()
    add_custom_target(
        lint
        COMMAND "${CLANG_TIDY_EXECUTABLE}" --experimental-custom-checks -p "${CMAKE_BINARY_DIR}"
                src/benchmark.cpp tests/correctness.cpp
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Checking project sources and headers with clang-tidy"
        USES_TERMINAL VERBATIM
    )
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
endif()
enable_testing()
foreach(workers 1 2 4)
    add_test(NAME correctness_${workers} COMMAND correctness)
    set_tests_properties(
        correctness_${workers} PROPERTIES ENVIRONMENT "PARLAY_NUM_THREADS=${workers}" TIMEOUT 180
    )
endforeach()

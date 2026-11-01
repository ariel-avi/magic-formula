cmake_minimum_required(VERSION 4.0)

project(magic_formula VERSION 0.1.0 LANGUAGES CXX)

# Set after project(): the Conan toolchain sets the standard while project() runs, and these values must win.
set(CMAKE_CXX_STANDARD 26)
# CMake 4.2 knows no C++26 flag for MSVC (Modules/Compiler/MSVC-CXX.cmake stops at -std:c++latest = 23) and none for
# AppleClang (AppleClang-CXX.cmake stops at -std=c++2b). Fall back to 23 there; GCC >= 14 and Clang >= 17 define the
# option and keep -std=c++26. The code uses no C++26-only language feature.
if (NOT DEFINED CMAKE_CXX26_STANDARD_COMPILE_OPTION)
    message(STATUS "CMake has no C++26 flag for ${CMAKE_CXX_COMPILER_ID}; using C++23")
    set(CMAKE_CXX_STANDARD 23)
endif ()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# No C++ modules. Otherwise CMake scans C++20+ sources (needs clang-scan-deps) and puts @<obj>.modmap response files,
# which exist only after a build, into compile_commands.json; clang-tidy then fails on a configured-only tree.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

get_property(_mf_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (NOT _mf_multi_config AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif ()

option(MF_BUILD_TESTS "Build the unit and module tests" ON)
option(MF_ENABLE_COVERAGE "Instrument project targets for gcov coverage and add the 'coverage' target" OFF)
option(MF_WARNINGS_AS_ERRORS "Treat compiler warnings as errors on project targets" ON)
option(MF_BUILD_PYTHON "Build the pybind11 extension magic_formula._core from python/ (python/pyproject.toml sets it; not a CTest test, not coverage-gated)" OFF)
option(MF_BUILD_BENCH "Build the Google Benchmark executable mf_bench from bench/ (not a CTest test, not coverage-gated)" OFF)
option(MF_BUILD_WASM "Build the WebAssembly module mf_wasm from wasm/ (Emscripten toolchain, wasm preset)" OFF)
set(MF_SANITIZER "" CACHE STRING "Instrument project targets with one sanitizer: address, undefined or thread (empty: none; presets asan, ubsan, tsan)")
set_property(CACHE MF_SANITIZER PROPERTY STRINGS "" address undefined thread)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(Coverage)
include(MfCompileOptions)
include(MfQualityGates)

# Eigen (header-only, from Conan): a usage requirement of the headers target.
find_package(Eigen3 CONFIG REQUIRED)

# Targets live in the directory that holds their files; each directory lists its files explicitly.
add_subdirectory(include)   # mf_headers (mf::headers)
add_subdirectory(src)       # mflib (mf::mflib), src/app: mf

if (MF_BUILD_TESTS)
    enable_testing()
endif ()

# Always configured so that clang-format_all sees their files; each checks its own option before defining a target.
add_subdirectory(tests)     # unit, module, support (MF_BUILD_TESTS)
add_subdirectory(bench)     # mf_bench (MF_BUILD_BENCH)
add_subdirectory(python)    # mf_python (MF_BUILD_PYTHON)
add_subdirectory(wasm)      # mf_wasm (MF_BUILD_WASM, Emscripten wasm preset only)

if (MF_ENABLE_COVERAGE)
    get_property(MF_TEST_TARGETS GLOBAL PROPERTY MF_TEST_TARGETS)
    add_coverage_target(
            NAME coverage
            ROOT "${PROJECT_SOURCE_DIR}"
            OBJECT_DIRECTORY "${PROJECT_BINARY_DIR}"
            OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/coverage"
            FILTERS "src/" "include/"
            FAIL_UNDER_LINE 100
            FAIL_UNDER_BRANCH 100
            CTEST_ARGS --output-on-failure --no-tests=error
            DEPENDS mf ${MF_TEST_TARGETS})
endif ()

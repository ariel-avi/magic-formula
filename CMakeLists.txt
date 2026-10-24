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
option(MF_BUILD_BENCH "Build the Google Benchmark executable mf_bench from bench/ (not a CTest test, not coverage-gated)" OFF)
option(MF_BUILD_WASM "Build the WebAssembly module mf_wasm from wasm/ (Emscripten toolchain, wasm preset)" OFF)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(ClangFormat)
include(ClangTidy)
include(Coverage)
include(MfCompileOptions)

# Eigen (header-only, from Conan): vectorised model evaluation; a PUBLIC usage requirement of mflib.
find_package(Eigen3 CONFIG REQUIRED)

# Library: every src/**/*.cpp except the CLI under src/app/.
file(GLOB_RECURSE MF_LIB_SOURCES CONFIGURE_DEPENDS RELATIVE "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/src/*.cpp")
list(FILTER MF_LIB_SOURCES EXCLUDE REGEX "^src/app/")
add_library(mflib STATIC ${MF_LIB_SOURCES})
add_library(mf::mflib ALIAS mflib)
target_include_directories(mflib PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(mflib PUBLIC Eigen3::Eigen)
target_compile_definitions(mflib PRIVATE "MF_VERSION_STRING=\"${PROJECT_VERSION}\"")
mf_configure_target(mflib)

# CLI: src/app/**/*.cpp
file(GLOB_RECURSE MF_APP_SOURCES CONFIGURE_DEPENDS RELATIVE "${PROJECT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src/app/*.cpp")
add_executable(mf ${MF_APP_SOURCES})
target_link_libraries(mf PRIVATE mf::mflib)
mf_configure_target(mf)

# Quality gates from the lab templates: clang-format_all (formats in place) and clang-tidy_all.
file(GLOB_RECURSE MF_FORMAT_FILES CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/include/*.hpp"
        "${PROJECT_SOURCE_DIR}/src/*.hpp" "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/tests/*.hpp" "${PROJECT_SOURCE_DIR}/tests/*.cpp"
        "${PROJECT_SOURCE_DIR}/bench/*.hpp" "${PROJECT_SOURCE_DIR}/bench/*.cpp"
        "${PROJECT_SOURCE_DIR}/wasm/*.cpp")
add_clang_format_for(magic_formula FILES ${MF_FORMAT_FILES})
add_clang_tidy_for(mflib)
add_clang_tidy_for(mf)

if (MF_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

# Google Benchmark (bench/mf_bench): ON in the release preset; never a CTest test and never coverage-gated.
if (MF_BUILD_BENCH)
    add_subdirectory(bench)
endif ()

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

# WebAssembly module (wasm preset only): embind adapter in wasm/, linked against mflib; not part of the native build.
if (MF_BUILD_WASM)
    if (NOT EMSCRIPTEN)
        message(FATAL_ERROR "MF_BUILD_WASM needs the Emscripten toolchain (wasm preset)")
    endif ()
    add_subdirectory(wasm)
endif ()

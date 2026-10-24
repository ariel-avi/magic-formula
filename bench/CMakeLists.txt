# mf_bench: one Google Benchmark executable built from every bench/*.cpp. Added by the root CMakeLists.txt only when
# MF_BUILD_BENCH is ON (the release preset sets it). Not registered with CTest and not part of the coverage gate.
find_package(benchmark CONFIG QUIET)
if (NOT benchmark_FOUND)
    message(FATAL_ERROR "MF_BUILD_BENCH is ON but Google Benchmark was not found: run conan install with -o with_benchmark=True (the default for Release)")
endif ()

file(GLOB MF_BENCH_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
if (NOT MF_BENCH_SOURCES)
    message(STATUS "bench/ has no sources; mf_bench is not built")
    return()
endif ()

add_executable(mf_bench ${MF_BENCH_SOURCES})
target_link_libraries(mf_bench PRIVATE mf::mflib benchmark::benchmark_main)
# Same define as the tests (cmake/MfTesting.cmake): the benchmarks load tests/data/*.tir.
target_compile_definitions(mf_bench PRIVATE "MF_TEST_DATA_DIR=\"${PROJECT_SOURCE_DIR}/tests/data\"")
mf_configure_target(mf_bench)

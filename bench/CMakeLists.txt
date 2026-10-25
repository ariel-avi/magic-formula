# mf_bench: one Google Benchmark executable. A new benchmark source is added to MF_BENCH_SOURCES (one path per line,
# LC_ALL=C sorted); the list also feeds clang-format_all. The directory is always configured (so the format gate sees
# the files), but the target exists only when MF_BUILD_BENCH is ON (the release preset sets it). Not registered with
# CTest and not part of the coverage gate.
set(MF_BENCH_SOURCES
        mf_bench.cpp)

mf_add_clang_format(mf_bench FILES ${MF_BENCH_SOURCES})

if (NOT MF_BUILD_BENCH)
    return()
endif ()

find_package(benchmark CONFIG QUIET)
if (NOT benchmark_FOUND)
    message(FATAL_ERROR "MF_BUILD_BENCH is ON but Google Benchmark was not found: run conan install with -o with_benchmark=True (the default for Release)")
endif ()

add_executable(mf_bench ${MF_BENCH_SOURCES})
target_link_libraries(mf_bench PRIVATE mf::mflib benchmark::benchmark_main)
# Same define as the tests (tests/support): the benchmarks load tests/data/*.tir.
target_compile_definitions(mf_bench PRIVATE "MF_TEST_DATA_DIR=\"${PROJECT_SOURCE_DIR}/tests/data\"")
mf_configure_target(mf_bench)

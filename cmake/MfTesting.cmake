include_guard(GLOBAL)

include(GoogleTest)
include(MfCompileOptions)

# mf_add_test(<source> <label>): one GoogleTest executable per *_test.cpp (target name = file stem). Links mf::mflib
# and GTest::gtest_main, defines MF_TEST_DATA_DIR and MF_VERSION_STRING, registers each TEST via gtest_discover_tests
# with CTest label <label>, and records the target in the global property MF_TEST_TARGETS (coverage DEPENDS).
function(mf_add_test TEST_SOURCE LABEL)
    get_filename_component(_name "${TEST_SOURCE}" NAME_WLE)
    add_executable(${_name} "${TEST_SOURCE}")
    target_link_libraries(${_name} PRIVATE mf::mflib GTest::gtest_main)
    target_compile_definitions(${_name} PRIVATE
            "MF_TEST_DATA_DIR=\"${PROJECT_SOURCE_DIR}/tests/data\""
            "MF_VERSION_STRING=\"${PROJECT_VERSION}\"")
    mf_configure_target(${_name})
    # DISCOVERY_TIMEOUT: the default 5 s is too short on Windows, where real-time AV scanning delays the first launch of
    # every freshly linked test executable (POST_BUILD discovery runs it).
    set(_props LABELS "${LABEL}")
    # per_point_golden compares bit for bit against values captured with glibc libm's baseline (non-FMA) variants. On
    # x86-64 Linux, libm picks its FMA/AVX2 ifunc variants of its transcendental functions (for example exp, log, pow, sin, cos, tan,
    # atan) when the CPU has them; observed CI mismatches were 1 ulp in MZ only. The tunable pins all of them to the baseline
    # variants (a no-op on CPUs without FMA/AVX2).
    if(_name STREQUAL "per_point_golden_test" AND CMAKE_SYSTEM_NAME STREQUAL "Linux"
            AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
        list(APPEND _props ENVIRONMENT "GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX2,-FMA,-FMA4")
    endif()
    gtest_discover_tests(${_name} DISCOVERY_TIMEOUT 60 PROPERTIES ${_props})
    set_property(GLOBAL APPEND PROPERTY MF_TEST_TARGETS ${_name})
endfunction()

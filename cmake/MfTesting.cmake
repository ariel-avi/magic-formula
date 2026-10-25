include_guard(GLOBAL)

include(GoogleTest)
include(MfCompileOptions)

# mf_add_test(<source> <label>): one GoogleTest executable per *_test.cpp (target name = file stem). Links mf::mflib,
# mf::test_support (MF_TEST_DATA_DIR, fixtures) and GTest::gtest_main, defines MF_VERSION_STRING, registers each TEST
# via gtest_discover_tests with CTest label <label>, and records the target in the global property MF_TEST_TARGETS (coverage DEPENDS).
function(mf_add_test TEST_SOURCE LABEL)
    get_filename_component(_name "${TEST_SOURCE}" NAME_WLE)
    add_executable(${_name} "${TEST_SOURCE}")
    target_link_libraries(${_name} PRIVATE mf::mflib mf::test_support GTest::gtest_main)
    target_compile_definitions(${_name} PRIVATE "MF_VERSION_STRING=\"${PROJECT_VERSION}\"")
    mf_configure_target(${_name})
    # DISCOVERY_TIMEOUT: the default 5 s is too short on Windows, where real-time AV scanning delays the first launch of
    # every freshly linked test executable (POST_BUILD discovery runs it).
    set(_props LABELS "${LABEL}")
    gtest_discover_tests(${_name} DISCOVERY_TIMEOUT 60 PROPERTIES ${_props})
    set_property(GLOBAL APPEND PROPERTY MF_TEST_TARGETS ${_name})
endfunction()

file(GLOB MF_MODULE_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp")
foreach (_test_source IN LISTS MF_MODULE_TESTS)
    mf_add_test("${_test_source}" module)
endforeach ()

# CLI smoke test: 'mf' prints "mf <version>" on stdout (PASS_REGULAR_EXPRESSION ignores the exit code).
string(REPLACE "." "\\." _mf_version_regex "${PROJECT_VERSION}")
add_test(NAME mf_cli_prints_version COMMAND $<TARGET_FILE:mf>)
set_tests_properties(mf_cli_prints_version PROPERTIES
        LABELS module
        PASS_REGULAR_EXPRESSION "^mf ${_mf_version_regex}\n$")

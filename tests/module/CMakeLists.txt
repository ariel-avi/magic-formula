# Module tests (CTest label module): one executable per file (target name = file stem). A new test file is added to
# MF_MODULE_TESTS (one path per line, LC_ALL=C sorted); the list also feeds clang-format_all.
set(MF_MODULE_TESTS
        cli_test.cpp
        fixtures_test.cpp
        mfeval_reference_test.cpp
        tir_edit_test.cpp)

mf_add_clang_format(mf_module_tests FILES ${MF_MODULE_TESTS})

if (NOT MF_BUILD_TESTS)
    return()
endif ()

foreach (_test_source IN LISTS MF_MODULE_TESTS)
    mf_add_test("${_test_source}" module)
endforeach ()

# CLI process tests: they run the real 'mf' executable (so they cover src/app/main.cpp).
#  - mf_cli_prints_version: 'mf --version' prints "mf <version>" on stdout.
#  - mf_cli_usage_error: 'mf' without arguments is a usage error (exit code 2, non-zero), hence WILL_FAIL.
#  - mf_cli_end_to_end: 'mf <S vectors table> <PAC2002 fixture S>' prints the results header line and no error.
# (PASS_REGULAR_EXPRESSION ignores the exit code.)
string(REPLACE "." "\\." _mf_version_regex "${PROJECT_VERSION}")
add_test(NAME mf_cli_prints_version COMMAND $<TARGET_FILE:mf> --version)
set_tests_properties(mf_cli_prints_version PROPERTIES
        LABELS module
        PASS_REGULAR_EXPRESSION "^mf ${_mf_version_regex}\n$")

add_test(NAME mf_cli_usage_error COMMAND $<TARGET_FILE:mf>)
set_tests_properties(mf_cli_usage_error PROPERTIES
        LABELS module
        WILL_FAIL TRUE)

add_test(NAME mf_cli_end_to_end
        COMMAND $<TARGET_FILE:mf>
        "${PROJECT_SOURCE_DIR}/tests/data/inputs_s_vectors.csv"
        "${PROJECT_SOURCE_DIR}/tests/data/pac2002_S.tir")
set_tests_properties(mf_cli_end_to_end PROPERTIES
        LABELS module
        PASS_REGULAR_EXPRESSION "FZ\\[N\\],SA\\[rad\\],SR\\[-\\],IA\\[rad\\],VX\\[m/s\\],FX\\[N\\],FY\\[N\\],MZ\\[N\\*m\\],MX\\[N\\*m\\],MY\\[N\\*m\\]"
        FAIL_REGULAR_EXPRESSION "mf: error")

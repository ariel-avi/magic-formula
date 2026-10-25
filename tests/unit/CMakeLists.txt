# Unit tests (CTest label unit): one executable per file (target name = file stem). A new test file is added to
# MF_UNIT_TESTS (one path per line, LC_ALL=C sorted); the list also feeds clang-format_all.
set(MF_UNIT_TESTS
        allocation_test.cpp
        cli_options_test.cpp
        error_test.cpp
        input_table_test.cpp
        lanes_test.cpp
        mf52_model_test.cpp
        mf5x_bindings_test.cpp
        mf5x_coefficients_test.cpp
        mf5x_kernels_test.cpp
        mf5x_layout_test.cpp
        mf5x_pipeline_test.cpp
        model_checks_test.cpp
        model_contract_test.cpp
        model_layout_test.cpp
        model_loader_test.cpp
        model_routing_test.cpp
        number_test.cpp
        pac2002_model_test.cpp
        result_writer_test.cpp
        text_file_test.cpp
        text_test.cpp
        tir_document_test.cpp
        tir_reader_test.cpp
        tire_model_test.cpp
        units_test.cpp
        version_test.cpp)

mf_add_clang_format(mf_unit_tests FILES ${MF_UNIT_TESTS})

if (NOT MF_BUILD_TESTS)
    return()
endif ()

foreach (_test_source IN LISTS MF_UNIT_TESTS)
    mf_add_test("${_test_source}" unit)
endforeach ()

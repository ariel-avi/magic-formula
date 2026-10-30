# Unit tests (CTest label unit): one executable per file (target name = file stem). A new test file is added to
# MF_UNIT_TESTS (one path per line, LC_ALL=C sorted); the list also feeds clang-format_all.
set(MF_UNIT_TESTS
        allocation_test.cpp
        cli_options_test.cpp
        coordinate_conversion_test.cpp
        coordinate_identification_test.cpp
        coordinate_system_test.cpp
        dual_lane_test.cpp
        error_test.cpp
        identification_probes_test.cpp
        input_table_test.cpp
        lanes_test.cpp
        mf52_model_test.cpp
        mf5x_bindings_test.cpp
        mf5x_characteristics_test.cpp
        mf5x_coefficients_test.cpp
        mf5x_kernels_test.cpp
        mf5x_layout_test.cpp
        mf5x_pipeline_test.cpp
        mf5x_signs_test.cpp
        mf61_model_test.cpp
        model_checks_test.cpp
        model_contract_test.cpp
        model_layout_test.cpp
        model_loader_test.cpp
        model_routing_test.cpp
        number_test.cpp
        pac2002_model_test.cpp
        result_writer_test.cpp
        stationary_point_test.cpp
        text_file_test.cpp
        text_test.cpp
        tir_conversion_test.cpp
        tir_document_test.cpp
        tir_editor_test.cpp
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

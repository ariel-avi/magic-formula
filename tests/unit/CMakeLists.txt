file(GLOB MF_UNIT_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp")
foreach (_test_source IN LISTS MF_UNIT_TESTS)
    mf_add_test("${_test_source}" unit)
endforeach ()

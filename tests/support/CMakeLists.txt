# Header-only test helpers: INTERFACE target mf_test_support (alias mf::test_support). Every test links it through
# mf_add_test. A new helper header is added to MF_TEST_SUPPORT_HEADERS (one path per line, LC_ALL=C sorted).
set(MF_TEST_SUPPORT_HEADERS
        fixtures.hpp)
mf_add_clang_format(mf_test_support FILES ${MF_TEST_SUPPORT_HEADERS})

if (NOT MF_BUILD_TESTS)
    return()
endif ()

add_library(mf_test_support INTERFACE)
add_library(mf::test_support ALIAS mf_test_support)
target_sources(mf_test_support INTERFACE
        FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
        FILES ${MF_TEST_SUPPORT_HEADERS})
target_compile_definitions(mf_test_support INTERFACE "MF_TEST_DATA_DIR=\"${PROJECT_SOURCE_DIR}/tests/data\"")
target_link_libraries(mf_test_support INTERFACE mf::mflib GTest::gtest)

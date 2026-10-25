# Always configured so that clang-format_all sees the test files; each subdirectory registers its files with the
# format gate first and returns before defining targets when MF_BUILD_TESTS is OFF.
if (MF_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
    include(MfTesting)
endif ()

add_subdirectory(support)
add_subdirectory(unit)
add_subdirectory(module)

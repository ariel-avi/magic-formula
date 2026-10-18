include_guard(GLOBAL)

# mf_configure_target(<target>): project warning set (-Werror with MF_WARNINGS_AS_ERRORS) and, with
# MF_ENABLE_COVERAGE, gcov instrumentation via enable_coverage_for() (Coverage.cmake, included by the root CMakeLists).
function(mf_configure_target TARGET_NAME)
    target_compile_options(${TARGET_NAME} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow)
    if (MF_WARNINGS_AS_ERRORS)
        target_compile_options(${TARGET_NAME} PRIVATE -Werror)
    endif ()
    if (MF_ENABLE_COVERAGE)
        enable_coverage_for(${TARGET_NAME})
    endif ()
endfunction()

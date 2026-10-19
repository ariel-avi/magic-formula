include_guard(GLOBAL)

# mf_configure_target(<target>): project warning set (-Werror / /WX with MF_WARNINGS_AS_ERRORS) and, with
# MF_ENABLE_COVERAGE, gcov instrumentation via enable_coverage_for() (Coverage.cmake, included by the root CMakeLists).
# cl.exe reads -W<x> as its own /W<level> option, so the MSVC front end (cl, clang-cl) gets its own set; GCC and Clang
# keep the GNU-style flags unchanged.
function(mf_configure_target TARGET_NAME)
    if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        # /W4 covers the -Wall -Wextra -Wconversion -Wsign-conversion -Wshadow family (C4244, C4267, C4389, C4456-4459).
        target_compile_options(${TARGET_NAME} PRIVATE /W4 /permissive- /utf-8 /EHsc /Zc:__cplusplus)
        if (MF_WARNINGS_AS_ERRORS)
            target_compile_options(${TARGET_NAME} PRIVATE /WX)
        endif ()
    else ()
        target_compile_options(${TARGET_NAME} PRIVATE
                -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow)
        if (MF_WARNINGS_AS_ERRORS)
            target_compile_options(${TARGET_NAME} PRIVATE -Werror)
        endif ()
    endif ()
    if (MF_ENABLE_COVERAGE)
        enable_coverage_for(${TARGET_NAME})
    endif ()
endfunction()

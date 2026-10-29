include_guard(GLOBAL)
include(MfSanitizers)

# mf_configure_target(<target>): project warning set (-Werror / /WX with MF_WARNINGS_AS_ERRORS) and, with
# MF_ENABLE_COVERAGE, gcov instrumentation via enable_coverage_for() (Coverage.cmake, included by the root CMakeLists).
# With MF_SANITIZER set (address, undefined, thread) every target is also instrumented via mf_enable_sanitizer()
# (MfSanitizers.cmake).
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
    # Every configuration that does not define NDEBUG (Debug, coverage, and an empty CMAKE_BUILD_TYPE) gets
    # EIGEN_RUNTIME_NO_MALLOC: lets tests assert that code does not touch Eigen's heap
    # (Eigen::internal::set_is_malloc_allowed) and matches the allocation_test #error guard. Every target goes through
    # this function, so the definition is uniform within a configuration (no ODR mismatch); Release-like
    # configurations (bench, production) are unaffected.
    target_compile_definitions(${TARGET_NAME} PRIVATE
        "$<$<NOT:$<CONFIG:Release,RelWithDebInfo,MinSizeRel>>:EIGEN_RUNTIME_NO_MALLOC>")
    if (MF_ENABLE_COVERAGE)
        enable_coverage_for(${TARGET_NAME})
    endif ()
    if (MF_SANITIZER)
        mf_enable_sanitizer(${TARGET_NAME})
    endif ()
endfunction()

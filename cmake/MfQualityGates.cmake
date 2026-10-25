include_guard(GLOBAL)

include(ClangFormat)
include(ClangTidy)

# Thin wrappers around the verbatim lab functions add_clang_format_for / add_clang_tidy_for. Those resolve relative
# paths against the top-level source directory, which is wrong once a target lives in a sub-directory, so the wrappers
# make every path absolute against the calling directory first.

# mf_add_clang_format(<name> FILES <file>...): adds the target clang-format_<name> (part of clang-format_all).
function(mf_add_clang_format NAME)
    # An earlier call already found no clang-format (the lab module warned once): stay quiet in the other directories.
    if (DEFINED CLANG_FORMAT_EXE AND NOT CLANG_FORMAT_EXE)
        return()
    endif ()
    cmake_parse_arguments(QG "" "" "FILES" ${ARGN})
    set(_abs_files)
    foreach (_file IN LISTS QG_FILES)
        cmake_path(ABSOLUTE_PATH _file BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" NORMALIZE OUTPUT_VARIABLE _abs)
        list(APPEND _abs_files "${_abs}")
    endforeach ()
    if (_abs_files)
        add_clang_format_for(${NAME} FILES ${_abs_files})
    endif ()
endfunction()

# mf_add_clang_tidy(<target> FILES <file>...): adds the target clang-tidy_<target> (part of clang-tidy_all). Only the
# translation units in FILES are checked; headers are reached through them (HeaderFilterRegex in .clang-tidy).
function(mf_add_clang_tidy TARGET_NAME)
    cmake_parse_arguments(QG "" "" "FILES" ${ARGN})
    set(_abs_files)
    foreach (_file IN LISTS QG_FILES)
        if (_file MATCHES "\\.(c|cc|cxx|cpp)$")
            cmake_path(ABSOLUTE_PATH _file BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" NORMALIZE OUTPUT_VARIABLE _abs)
            list(APPEND _abs_files "${_abs}")
        endif ()
    endforeach ()
    if (_abs_files)
        add_clang_tidy_for(${TARGET_NAME} FILES ${_abs_files})
    endif ()
endfunction()

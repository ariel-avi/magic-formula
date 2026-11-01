include_guard(GLOBAL)

# MF_SANITIZER (address, undefined or thread; empty: none) instruments project targets with one sanitizer.
# mf_configure_target() (MfCompileOptions.cmake) calls mf_enable_sanitizer(<target>) for every project target
# (mflib, mf, tests). GTest from Conan stays uninstrumented. GCC and Clang front ends only.

set(_mf_sanitizers address undefined thread)
if (NOT MF_SANITIZER STREQUAL "" AND NOT MF_SANITIZER IN_LIST _mf_sanitizers)
    message(FATAL_ERROR "MF_SANITIZER='${MF_SANITIZER}' is not supported; allowed values: address, undefined, thread (empty: no sanitizer)")
endif ()
if (NOT MF_SANITIZER STREQUAL "" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    message(FATAL_ERROR "MF_SANITIZER requires a GCC or Clang front end; the asan, ubsan, tsan and valgrind presets are supported and tested on Ubuntu 24.04 with GCC or Clang (nightly-sanitizers.yml)")
endif ()

# Link options are PUBLIC (like Coverage.cmake) so executables that link the static mflib also link the runtime.
# float-cast-overflow is explicit because GCC's 'undefined' group lacks it while Clang's has it.
# float-divide-by-zero is deliberately not enabled: IEEE division by zero is defined and the numerical guards rely on it.
# -fno-sanitize-recover=all ends the process with a non-zero exit at the first UB.
function(mf_enable_sanitizer TARGET_NAME)
    if (MF_SANITIZER STREQUAL "address")
        target_compile_options(${TARGET_NAME} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(${TARGET_NAME} PUBLIC -fsanitize=address)
    elseif (MF_SANITIZER STREQUAL "undefined")
        target_compile_options(${TARGET_NAME} PRIVATE
                -fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_link_options(${TARGET_NAME} PUBLIC -fsanitize=undefined,float-cast-overflow)
    elseif (MF_SANITIZER STREQUAL "thread")
        target_compile_options(${TARGET_NAME} PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
        target_link_options(${TARGET_NAME} PUBLIC -fsanitize=thread)
    endif ()
endfunction()

# Python extension magic_formula._core: a pybind11 adapter that embeds mflib. A new binding source is added to
# MF_PYTHON_SOURCES (one path per line, LC_ALL=C sorted); the list also feeds clang-format_all. The directory is always
# configured (so the format gate sees the files), but the target exists only when MF_BUILD_PYTHON is ON
# (python/pyproject.toml sets it). Not registered with CTest and not part of the coverage gate.
set(MF_PYTHON_SOURCES
        bindings/mf_python.cpp)

mf_add_clang_format(mf_python FILES ${MF_PYTHON_SOURCES})

if (NOT MF_BUILD_PYTHON)
    return()
endif ()

find_package(Python 3.10 REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 CONFIG REQUIRED)

# The extension embeds the static mflib: a shared module needs PIC, and hidden visibility keeps mflib's symbols out of
# the module's export table.
set_target_properties(mflib PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)

# Conan's toolchain puts tools.build:sharedlinkflags into CMAKE_SHARED_LINKER_FLAGS only; an extension is a MODULE
# library (on macOS the Homebrew libc++ -L/-rpath flags would otherwise be missing).
string(APPEND CMAKE_MODULE_LINKER_FLAGS " ${CMAKE_SHARED_LINKER_FLAGS}")

# NO_EXTRAS: no LTO/strip (mflib is not LTO-compiled; Apple ld cannot read Homebrew-LLVM bitcode).
pybind11_add_module(mf_python MODULE NO_EXTRAS ${MF_PYTHON_SOURCES})
set_target_properties(mf_python PROPERTIES OUTPUT_NAME _core)
target_link_libraries(mf_python PRIVATE mf::mflib)
# Same warnings (-Werror) and EIGEN_RUNTIME_NO_MALLOC rule as mflib (no ODR mismatch).
mf_configure_target(mf_python)

install(TARGETS mf_python LIBRARY DESTINATION magic_formula)

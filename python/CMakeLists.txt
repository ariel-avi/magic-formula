find_package(Python 3.10 REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 CONFIG REQUIRED)

# The extension embeds the static mflib: a shared module needs PIC, and hidden visibility keeps mflib's symbols out of
# the module's export table.
set_target_properties(mflib PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)

# Conan's toolchain puts tools.build:sharedlinkflags into CMAKE_SHARED_LINKER_FLAGS only; an extension is a MODULE
# library (on macOS the Homebrew libc++ -L/-rpath flags would otherwise be missing).
string(APPEND CMAKE_MODULE_LINKER_FLAGS " ${CMAKE_SHARED_LINKER_FLAGS}")

# NO_EXTRAS: no LTO/strip (mflib is not LTO-compiled; Apple ld cannot read Homebrew-LLVM bitcode).
pybind11_add_module(mf_python MODULE NO_EXTRAS bindings/mf_python.cpp)
set_target_properties(mf_python PROPERTIES OUTPUT_NAME _core)
target_link_libraries(mf_python PRIVATE mf::mflib)
# Same warnings (-Werror) and EIGEN_RUNTIME_NO_MALLOC rule as mflib (no ODR mismatch).
mf_configure_target(mf_python)

install(TARGETS mf_python LIBRARY DESTINATION magic_formula)

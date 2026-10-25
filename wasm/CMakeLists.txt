# WebAssembly module mf_wasm: an embind adapter that links mflib. A new source is added to MF_WASM_SOURCES (one path per
# line, LC_ALL=C sorted); the list also feeds clang-format_all. The directory is always configured (so the format gate
# sees the files), but the target exists only when MF_BUILD_WASM is ON (wasm preset, Emscripten toolchain). Not
# registered with CTest and not part of the coverage gate.
set(MF_WASM_SOURCES
        mf_wasm.cpp)

mf_add_clang_format(mf_wasm FILES ${MF_WASM_SOURCES})

if (NOT MF_BUILD_WASM)
    return()
endif ()
if (NOT EMSCRIPTEN)
    message(FATAL_ERROR "MF_BUILD_WASM needs the Emscripten toolchain (wasm preset)")
endif ()

set(MF_WASM_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/web/src/wasm" CACHE PATH "Directory that receives mf_wasm.js")
add_executable(mf_wasm ${MF_WASM_SOURCES})
target_link_libraries(mf_wasm PRIVATE mf::mflib)
mf_configure_target(mf_wasm)
# -sKEY=VALUE form only: CMake de-duplicates repeated options, which would break the two-token "-s KEY=VALUE" form.
target_link_options(mf_wasm PRIVATE -lembind -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createMfModule
                    -sENVIRONMENT=web,worker,node -sSINGLE_FILE=1 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576)
set_target_properties(mf_wasm PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${MF_WASM_OUTPUT_DIR}" SUFFIX ".js")

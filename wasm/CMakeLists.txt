set(MF_WASM_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/web/src/wasm" CACHE PATH "Directory that receives mf_wasm.js")
add_executable(mf_wasm mf_wasm.cpp)
target_link_libraries(mf_wasm PRIVATE mf::mflib)
mf_configure_target(mf_wasm)
# -sKEY=VALUE form only: CMake de-duplicates repeated options, which would break the two-token "-s KEY=VALUE" form.
target_link_options(mf_wasm PRIVATE -lembind -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createMfModule
                    -sENVIRONMENT=web,worker,node -sSINGLE_FILE=1 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576)
set_target_properties(mf_wasm PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${MF_WASM_OUTPUT_DIR}" SUFFIX ".js")

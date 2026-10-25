# CLI executable mf: one call into mf::cli::run. The binary stays at <build dir>/mf.
set(MF_APP_SOURCES
        main.cpp)

add_executable(mf ${MF_APP_SOURCES})
target_link_libraries(mf PRIVATE mf::mflib)
set_target_properties(mf PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
mf_configure_target(mf)

mf_add_clang_format(mf FILES ${MF_APP_SOURCES})
mf_add_clang_tidy(mf FILES ${MF_APP_SOURCES})

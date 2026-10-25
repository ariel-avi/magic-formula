# Library mflib (alias mf::mflib): every translation unit under src/ except the CLI under src/app/. A new source or
# private header is added to MF_LIB_SOURCES (one path per line, LC_ALL=C sorted); the list also feeds clang-format_all
# and clang-tidy_all.
set(MF_LIB_SOURCES
        cli/cli.cpp
        detail/text.cpp
        error.cpp
        io/input_table.cpp
        io/mf5x_bindings.cpp
        io/model_loader.cpp
        io/model_routing.cpp
        io/number.cpp
        io/result_writer.cpp
        io/text_file.cpp
        io/tir_document.cpp
        io/tir_editor.cpp
        io/tir_reader.cpp
        io/units.cpp
        model/mf52_model.cpp
        model/mf5x_coefficients.cpp
        model/mf5x_model.cpp
        model/model_checks.cpp
        model/model_layout.cpp
        model/pac2002_model.cpp
        model/tire_model.cpp
        version.cpp)

add_library(mflib STATIC ${MF_LIB_SOURCES})
add_library(mf::mflib ALIAS mflib)
target_link_libraries(mflib PUBLIC mf::headers Eigen3::Eigen)
target_compile_definitions(mflib PRIVATE "MF_VERSION_STRING=\"${PROJECT_VERSION}\"")
mf_configure_target(mflib)

mf_add_clang_format(mflib FILES ${MF_LIB_SOURCES})
mf_add_clang_tidy(mflib FILES ${MF_LIB_SOURCES})

add_subdirectory(app)

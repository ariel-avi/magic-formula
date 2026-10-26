# Public headers: INTERFACE target mf_headers (alias mf::headers). It carries the headers as a HEADERS file set, the
# include directory (consumers write #include <magic_formula/...>) and the Eigen usage requirement. A new public header
# is added to MF_PUBLIC_HEADERS (one path per line, LC_ALL=C sorted); it also feeds clang-format_all.
set(MF_PUBLIC_HEADERS
        magic_formula/cli/cli.hpp
        magic_formula/coordinate_identification.hpp
        magic_formula/coordinate_system.hpp
        magic_formula/detail/identification_probes.hpp
        magic_formula/detail/lanes.hpp
        magic_formula/detail/mf5x_kernels.hpp
        magic_formula/detail/text.hpp
        magic_formula/error.hpp
        magic_formula/evaluation_types.hpp
        magic_formula/io/input_table.hpp
        magic_formula/io/mf5x_bindings.hpp
        magic_formula/io/model_loader.hpp
        magic_formula/io/model_routing.hpp
        magic_formula/io/number.hpp
        magic_formula/io/result_writer.hpp
        magic_formula/io/text_file.hpp
        magic_formula/io/tir_conversion.hpp
        magic_formula/io/tir_document.hpp
        magic_formula/io/tir_editor.hpp
        magic_formula/io/tir_reader.hpp
        magic_formula/io/units.hpp
        magic_formula/mf52_model.hpp
        magic_formula/mf5x_coefficients.hpp
        magic_formula/mf5x_forms.hpp
        magic_formula/mf5x_layout.hpp
        magic_formula/mf5x_model.hpp
        magic_formula/mf5x_signs.hpp
        magic_formula/model_checks.hpp
        magic_formula/model_layout.hpp
        magic_formula/pac2002_model.hpp
        magic_formula/sign_transform.hpp
        magic_formula/tire_model.hpp
        magic_formula/version.hpp)

add_library(mf_headers INTERFACE)
add_library(mf::headers ALIAS mf_headers)
target_sources(mf_headers INTERFACE
        FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
        FILES ${MF_PUBLIC_HEADERS})
target_include_directories(mf_headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
# Eigen (header-only, from Conan): vectorised model evaluation; a usage requirement of every consumer of the headers.
target_link_libraries(mf_headers INTERFACE Eigen3::Eigen)

mf_add_clang_format(mf_headers FILES ${MF_PUBLIC_HEADERS})

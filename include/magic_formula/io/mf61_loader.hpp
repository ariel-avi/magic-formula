#pragma once

#include <expected>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/io/model_loader.hpp>
#include <magic_formula/io/tir_reader.hpp>
#include <vector>

namespace mf::io {
  /// Loads the coefficients, limits and metadata of an already identified MF6.1 document.
  [[nodiscard]] std::expected<loaded_model, error> load_mf61(tir_reader &reader, std::vector<warning> warnings,
                                                             const model_options &options);
} // namespace mf::io

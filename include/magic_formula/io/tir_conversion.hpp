#pragma once

#include <cstddef>
#include <expected>
#include <magic_formula/coordinate_system.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/sign_transform.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace mf::io {
  /// One rewritten entry.
  struct tir_change {
    std::string section; ///< upper-case
    std::string key;     ///< upper-case
    std::size_t line = 0;
    std::string before; ///< value text as written
    std::string after;
  };

  /// The result of `convert_tir`.
  struct tir_conversion {
    std::string text;                ///< converted .tir text (every other byte unchanged)
    sign_transforms applied;         ///< empty: already in the target system (text unchanged)
    std::vector<tir_change> changes; ///< in file order
  };

  /// Text negation of a number as written: "-x" -> "x", "+x" -> "-x", otherwise "-" + text.
  [[nodiscard]] std::string negate_number_text(std::string_view text);

  /// Converts .tir text to `target`: `tir_document::parse` and `load_tire_model` (their errors are returned
  /// unchanged), then the plan `plan_conversion(model->find_coordinate_system().effective, target)`, then:
  ///  - every present key of the family's coefficient table (MF 5.x: `mf5x::COEFFICIENT_SIGNS`) whose transform is
  ///    planned gets `negate_number_text(value)`, except values that are zero;
  ///  - every limit pair of the model layout whose transform is planned and whose two keys are both present becomes
  ///    LOWER' = negate(UPPER text), UPPER' = negate(LOWER text); a lone bound (ignored by the model) is left as
  ///    written.
  /// An edit whose new text equals the text as written is dropped. Edits are spliced at the entries' value offsets;
  /// flipped keys are never quoted, because quoted numbers fail to load. The converted text loads to a model whose
  /// coefficients and limits equal `model->transformed(applied)` value for value (parse(-x) == -parse(x)); zero values
  /// stay as written, so the loaded zeros are +0.0 where the transformed model has -0.0, which compare equal.
  /// @param text .tir file content
  /// @param target the system to convert to
  /// @param file_name name used in error locations
  /// @return the conversion, or the parse / load error
  [[nodiscard]] std::expected<tir_conversion, error> convert_tir(std::string_view text, coordinate_system target,
                                                                 std::string file_name = {});
} // namespace mf::io

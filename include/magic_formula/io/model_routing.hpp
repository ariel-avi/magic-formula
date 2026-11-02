#pragma once

#include <cstdint>
#include <expected>
#include <magic_formula/error.hpp>
#include <magic_formula/io/tir_reader.hpp>
#include <vector>

namespace mf::io {
  /// The formulation a .tir file is evaluated with.
  enum class model_kind : std::uint8_t {
    /// ADAMS PAC2002 (`pac2002_model`).
    PAC2002,
    /// MF-Tyre 5.2, FITTYP 6 (`mf52_model`).
    MF52,
    /// MF-Tyre 6.1, FITTYP 61 (`mf61_model`).
    MF61,
  };

  /// Decides which formulation a .tir file is written for (docs/research/model-routing.md). Reads PROPERTY_FILE_FORMAT
  /// and FITTYP through `reader` (both are marked consumed).
  ///
  /// The rules, applied in this order:
  ///  1. PROPERTY_FILE_FORMAT present and not 'PAC2002', 'MF_05', 'USER' or 'MF-TYRE' (case-insensitive, blanks
  ///     trimmed): UNSUPPORTED_MODEL. 'USER' and 'MF-TYRE' (TNO MF-Tyre ADAMS spellings) behave exactly like an absent
  ///     key, so rules 5 to 7 route them by FITTYP.
  ///  2. FITTYP present but not an integer (5.5, a quoted string, junk): the error of `tir_reader::integer`
  ///     (INVALID_VALUE or INVALID_NUMBER), unchanged.
  ///  3. FITTYP 60, 62 or 70 (MF-Tyre 6.x): UNSUPPORTED_MODEL "not implemented", even with 'PAC2002'.
  ///     FITTYP 61 selects MF 6.1, including files declaring PAC2002.
  ///  4. PROPERTY_FILE_FORMAT 'PAC2002': PAC2002. A FITTYP other than 5 adds the warning
  ///     "FITTYP <n> ignored for PAC2002".
  ///  5. FITTYP 6: MF 5.2 (PROPERTY_FILE_FORMAT absent, 'MF_05', 'USER' or 'MF-TYRE').
  ///  6. FITTYP 5, 21, 51 or 52: UNSUPPORTED_MODEL naming the family (the FITTYP 5 message suggests PAC2002).
  ///  7. Anything else (FITTYP absent, 0, 7, ...): UNSUPPORTED_MODEL "cannot identify the tire model".
  /// @param reader reader over the document
  /// @param warnings receives the warning of rule 4
  /// @return the model kind, or the UNSUPPORTED_MODEL / INVALID_VALUE / INVALID_NUMBER error located at the
  ///         PROPERTY_FILE_FORMAT or FITTYP entry (line 0 when that key is absent)
  [[nodiscard]] std::expected<model_kind, error> identify_model(tir_reader &reader, std::vector<warning> &warnings);
} // namespace mf::io

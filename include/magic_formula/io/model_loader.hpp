#pragma once

#include <expected>
#include <filesystem>
#include <magic_formula/error.hpp>
#include <magic_formula/io/tir_document.hpp>
#include <magic_formula/tire_model.hpp>
#include <memory>
#include <string>
#include <vector>

namespace mf::io {
  /// The result of loading a .tir file: the validated model plus everything the caller should tell the user.
  struct loaded_model {
    /// The concrete formulation chosen by the routing rules (`pac2002_model` or `mf52_model`), immutable.
    std::unique_ptr<const tire_model> model;
    /// Non-fatal diagnostics in the order they were found: parse warnings, routing, MF 5.2 foreign keys, ranges,
    /// USE_MODE, TYRESIDE, FE_METHOD, then defaulted coefficients, friction ellipse and rolling resistance. The library
    /// never prints them.
    std::vector<warning> warnings;
    /// Upper-case keys that are present in the file but were not used by anything, in file order (for example
    /// FILE_TYPE, or PAC2002 scaling factors that have no meaning in the model).
    std::vector<std::string> ignored_keys;
  };

  /// Factory: turns a parsed .tir document into a validated tire model (see docs/research/policies.md).
  ///
  /// The steps, in order:
  ///  1. `check_units` (SI only).
  ///  2. `identify_model` (PROPERTY_FILE_FORMAT / FITTYP).
  ///  3. Bind every coefficient key of `mf5x_common_bindings()` (and, for PAC2002, the camber factors LGAX, LGAY and
  ///     LGAZ). A required key that is absent is MISSING_KEY; an absent P/Q/S coefficient keeps 0 and is listed in one
  ///     warning; an absent scaling factor keeps 1. For MF 5.2 the keys of `mf52_foreign_keys()` that are
  ///     present give one warning, together with every key inside the sections of `mf52_foreign_sections()`, and
  ///     count as used.
  ///  4. R* rule. PAC2002: all 20 R* keys select the cosine method, none select the friction
  ///     ellipse (with a warning), a partial set is MISSING_KEY. MF 5.2: absent R* keys default to 0 and join the
  ///     warning of step 3.
  ///  5. LONGVL is required only when QSY3 or QSY4 is non-zero; otherwise it defaults to 1 m/s.
  ///  6. VXLOW (default 1 m/s) and the four validity ranges: both bounds give an interval, one bound
  ///     gives a warning and no interval.
  ///  7. [MODEL] metadata: FITTYP, USE_MODE (echoed only; a warning when outside 0..9) and TYRESIDE (LEFT,
  ///     RIGHT with a warning, anything else NOT_IMPLEMENTED). An FE_METHOD key gives a warning.
  ///  8. MF 5.2 only: WIDTH and RIM_RADIUS must be present; their values are not used.
  ///  9. `pac2002_model::create` / `mf52_model::create`. A core error gets the file and line of its key attached.
  /// 10. Warnings for defaulted coefficients, the friction-ellipse selection and the FITTYP 5 rolling-resistance form.
  ///
  /// The document is only read; the result does not refer to it.
  /// @param document parsed .tir file
  /// @return the model with its warnings and ignored keys, or the first error (with file, 1-based line when the key
  ///         exists, and key)
  [[nodiscard]] std::expected<loaded_model, error> load_tire_model(const tir_document &document);

  /// `read_tir_file` followed by `load_tire_model`.
  /// @param path .tir file
  /// @return the loaded model, or an IO, SYNTAX, DUPLICATE_KEY or any `load_tire_model` error
  [[nodiscard]] std::expected<loaded_model, error> load_tire_model_file(const std::filesystem::path &path);
} // namespace mf::io

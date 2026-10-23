#pragma once

#include <cstddef>
#include <cstdint>
#include <magic_formula/error.hpp>
#include <optional>

namespace mf {
  /// Combined-slip handling of an evaluation (USE_MODE 4 / 3 semantics).
  enum class slip_mode : std::uint8_t {
    COMBINED,  ///< Fx, Fy and Mz include the combined-slip interaction
    UNCOMBINED ///< pure-slip Fx0, Fy0 and Mz0 only
  };

  /// Construction-time evaluation options (stored by the model; immutable).
  struct model_options {
    slip_mode mode = slip_mode::COMBINED; ///< combined or pure-slip outputs
    /// true: the evaluate functions run the free checks (`mf::validate_input` and friends) and throw
    /// `mf::model_failure`; false: no check at all on the evaluation path.
    bool throw_if_model_failure = false;
  };

  /// Tire mounting side (TYRESIDE). Mirroring is not implemented.
  enum class tyre_side : std::uint8_t { LEFT, RIGHT };

  /// [MODEL] values that are echoed by the model; FITTYP also selects the PAC2002 rolling-resistance form.
  struct model_metadata {
    /// FITTYP, when present.
    std::optional<int> fittyp;
    /// USE_MODE, when present (echoed only).
    std::optional<int> use_mode;
    /// TYRESIDE; absent means LEFT.
    tyre_side side = tyre_side::LEFT;
  };

  /// The first failing point of a batch evaluation.
  struct batch_error {
    /// Zero-based index of the failing operating point.
    std::size_t index = 0;
    /// Why that point failed.
    error cause;
  };
} // namespace mf

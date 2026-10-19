#pragma once

#include <cstddef>
#include <cstdint>
#include <magic_formula/error.hpp>
#include <optional>

namespace mf {
  /// One steady-state operating point. SI units, TYDEX W-axis system with ISO orientation
  /// (docs/research/conventions.md sections 1-3).
  struct operating_point {
    /// Vertical load Fz [N]; > 0 = load, <= 0 = no contact and zero outputs (AD-01).
    double fz_n = 0.0;
    /// Slip ratio SR [-]; kappa = -Vsx/Vx (conventions.md section 2); -1 = locked wheel.
    double kappa = 0.0;
    /// Slip angle SA [rad]; tan(alpha) = Vsy/|Vx| (conventions.md section 2).
    double alpha_rad = 0.0;
    /// Inclination angle IA [rad]; ISO sign (conventions.md section 3).
    double gamma_rad = 0.0;
    /// Longitudinal speed Vx [m/s]; negative values are rejected (AD-02).
    double vx_mps = 0.0;
  };

  /// Which input adjustments were applied to one evaluated row.
  struct input_flags {
    /// Fz was clamped to FZMIN/FZMAX (AD-15, conventions.md section 8).
    bool fz_clamped = false;
    /// SR was clamped to KPUMIN/KPUMAX.
    bool kappa_clamped = false;
    /// SA was clamped to ALPMIN/ALPMAX.
    bool alpha_clamped = false;
    /// IA was clamped to CAMMIN/CAMMAX.
    bool gamma_clamped = false;
    /// Vx < VXLOW; the row is flagged but not modified (AD-05).
    bool below_vxlow = false;
  };

  /// Contact-point forces and moments in the W-axis system, ISO orientation (conventions.md sections 1 and 4).
  struct tire_outputs {
    /// Longitudinal force Fx [N].
    double fx_n = 0.0;
    /// Lateral force Fy [N].
    double fy_n = 0.0;
    /// Self-aligning moment Mz [N*m].
    double mz_nm = 0.0;
    /// Overturning moment Mx [N*m].
    double mx_nm = 0.0;
    /// Rolling-resistance moment My [N*m]; negative when rolling forward (ISO sign).
    double my_nm = 0.0;
    /// Adjustments applied to the inputs of this row.
    input_flags flags;
  };

  /// Combined-slip handling of an evaluation (USE_MODE 4 / 3 semantics, decision D-26 and AD-16).
  enum class slip_mode : std::uint8_t {
    COMBINED,  ///< Fx, Fy and Mz include the combined-slip interaction
    UNCOMBINED ///< pure-slip Fx0, Fy0 and Mz0 only
  };

  /// A closed interval [lower, upper]. `lower`/`upper` avoid the Windows `min`/`max` macros.
  struct interval {
    /// Lower bound.
    double lower = 0.0;
    /// Upper bound; `validate_limits` rejects lower > upper.
    double upper = 0.0;
  };

  /// Validity ranges (*_RANGE sections of the .tir file). An interval applies only when both bounds exist
  /// (AD-15, conventions.md section 8).
  struct operating_limits {
    /// FZMIN/FZMAX [N].
    std::optional<interval> fz_n;
    /// KPUMIN/KPUMAX [-].
    std::optional<interval> kappa;
    /// ALPMIN/ALPMAX [rad].
    std::optional<interval> alpha_rad;
    /// CAMMIN/CAMMAX [rad].
    std::optional<interval> gamma_rad;
    /// VXLOW [m/s]; only flags rows, it never changes results (AD-05).
    double vxlow_mps = 1.0;
  };

  /// Tire mounting side (TYRESIDE). Mirroring is not implemented (AD-06, conventions.md section 9).
  enum class tyre_side : std::uint8_t { LEFT, RIGHT };

  /// [MODEL] values that are echoed by the model; FITTYP also selects the PAC2002 rolling-resistance form.
  struct model_metadata {
    /// FITTYP, when present.
    std::optional<int> fittyp;
    /// USE_MODE, when present (echoed only, AD-16).
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

#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <magic_formula/model_layout.hpp>

/// Index maps of the MF 5.x family (PAC2002 and MF 5.2): which vector position holds which input, output and limit.
namespace mf::mf5x {
  // The index enums are unscoped on purpose, an exception to the enum-class rule: an enumerator IS the vector
  // position and is directly usable as an Eigen or buffer index (`y(output_index::FX)`, `data[output_index::FX]`)
  // without a cast. The family namespace scopes the names. Enumerator names are unique across the three enums.
  // The input order is the CSV echo order and the flag order is the warning order. MF 6.2 gets its own `mf::mf6x`
  // namespace and appends P as input 5.

  // NOLINTBEGIN(cppcoreguidelines-use-enum-class): deliberately unscoped, see the note above

  /// Positions of the input vector.
  enum input_index : std::uint8_t { FZ, SA, SR, IA, VX };

  /// Positions of the output vector: forces and moments, then 0.0/1.0 flags.
  enum output_index : std::uint8_t { FX, FY, MZ, MX, MY, FZ_CLAMPED, SR_CLAMPED, SA_CLAMPED, IA_CLAMPED, BELOW_VXLOW };

  /// Positions of the limits vector.
  enum limit_index : std::uint8_t { FZMIN, FZMAX, KPUMIN, KPUMAX, ALPMIN, ALPMAX, CAMMIN, CAMMAX, VXLOW };

  // NOLINTEND(cppcoreguidelines-use-enum-class)

  inline constexpr int INPUT_LENGTH = 5;
  inline constexpr int OUTPUT_LENGTH = 10;
  inline constexpr int LIMITS_LENGTH = 9;

  /// Default of an absent range bound.
  inline constexpr double INF = std::numeric_limits<double>::infinity();

  /// Input channels of PAC2002: Vx >= 0.
  inline constexpr std::array<input_channel, INPUT_LENGTH> PAC2002_INPUTS{{
      {.name = "FZ", .unit = "N", .negated_by = sign_transform::VERTICAL},
      {.name = "SA", .unit = "rad", .negated_by = sign_transform::LATERAL},
      {.name = "SR", .unit = "-"},
      {.name = "IA", .unit = "rad"},
      {.name = "VX", .unit = "m/s", .minimum = 0.0, .minimum_inclusive = true},
  }};

  /// Input channels of MF 5.2: Vx > 0.
  inline constexpr std::array<input_channel, INPUT_LENGTH> MF52_INPUTS{{
      {.name = "FZ", .unit = "N", .negated_by = sign_transform::VERTICAL},
      {.name = "SA", .unit = "rad", .negated_by = sign_transform::LATERAL},
      {.name = "SR", .unit = "-"},
      {.name = "IA", .unit = "rad"},
      {.name = "VX", .unit = "m/s", .minimum = 0.0, .minimum_inclusive = false},
  }};

  /// Output channels shared by the MF 5.x models.
  inline constexpr std::array<output_channel, OUTPUT_LENGTH> OUTPUTS{{
      {.name = "FX", .unit = "N"},
      {.name = "FY", .unit = "N", .negated_by = sign_transform::VERTICAL},
      {.name = "MZ", .unit = "N*m", .negated_by = sign_transform::VERTICAL},
      {.name = "MX", .unit = "N*m"},
      {.name = "MY", .unit = "N*m", .negated_by = sign_transform::VERTICAL},
      {.name = "FZ_CLAMPED", .unit = "-"},
      {.name = "SR_CLAMPED", .unit = "-"},
      {.name = "SA_CLAMPED", .unit = "-"},
      {.name = "IA_CLAMPED", .unit = "-"},
      {.name = "BELOW_VXLOW", .unit = "-"},
  }};

  /// Limit channels shared by the MF 5.x models. An absent bound is -inf / +inf.
  inline constexpr std::array<limit_channel, LIMITS_LENGTH> LIMITS{{
      {.key = "FZMIN", .role = limit_role::LOWER, .default_value = -INF, .negated_by = sign_transform::VERTICAL},
      {.key = "FZMAX", .role = limit_role::UPPER, .default_value = INF, .negated_by = sign_transform::VERTICAL},
      {.key = "KPUMIN", .role = limit_role::LOWER, .default_value = -INF},
      {.key = "KPUMAX", .role = limit_role::UPPER, .default_value = INF},
      {.key = "ALPMIN", .role = limit_role::LOWER, .default_value = -INF, .negated_by = sign_transform::LATERAL},
      {.key = "ALPMAX", .role = limit_role::UPPER, .default_value = INF, .negated_by = sign_transform::LATERAL},
      {.key = "CAMMIN", .role = limit_role::LOWER, .default_value = -INF},
      {.key = "CAMMAX", .role = limit_role::UPPER, .default_value = INF},
      {.key = "VXLOW", .role = limit_role::VALUE, .default_value = 1.0},
  }};

  /// Index map of PAC2002.
  inline constexpr model_layout PAC2002_LAYOUT{.inputs = PAC2002_INPUTS, .outputs = OUTPUTS, .limits = LIMITS};

  /// Index map of MF 5.2.
  inline constexpr model_layout MF52_LAYOUT{.inputs = MF52_INPUTS, .outputs = OUTPUTS, .limits = LIMITS};

  static_assert(PAC2002_INPUTS.size() == INPUT_LENGTH);
  static_assert(MF52_INPUTS.size() == INPUT_LENGTH);
  static_assert(OUTPUTS.size() == OUTPUT_LENGTH);
  static_assert(LIMITS.size() == LIMITS_LENGTH);
  static_assert(limit_pairs_well_formed(LIMITS));

  static_assert(PAC2002_INPUTS[FZ].name == "FZ");
  static_assert(PAC2002_INPUTS[SA].name == "SA");
  static_assert(PAC2002_INPUTS[SR].name == "SR");
  static_assert(PAC2002_INPUTS[IA].name == "IA");
  static_assert(PAC2002_INPUTS[VX].name == "VX");
  static_assert(MF52_INPUTS[FZ].name == "FZ");
  static_assert(MF52_INPUTS[SA].name == "SA");
  static_assert(MF52_INPUTS[SR].name == "SR");
  static_assert(MF52_INPUTS[IA].name == "IA");
  static_assert(MF52_INPUTS[VX].name == "VX");

  static_assert(OUTPUTS[FX].name == "FX");
  static_assert(OUTPUTS[FY].name == "FY");
  static_assert(OUTPUTS[MZ].name == "MZ");
  static_assert(OUTPUTS[MX].name == "MX");
  static_assert(OUTPUTS[MY].name == "MY");
  static_assert(OUTPUTS[FZ_CLAMPED].name == "FZ_CLAMPED");
  static_assert(OUTPUTS[SR_CLAMPED].name == "SR_CLAMPED");
  static_assert(OUTPUTS[SA_CLAMPED].name == "SA_CLAMPED");
  static_assert(OUTPUTS[IA_CLAMPED].name == "IA_CLAMPED");
  static_assert(OUTPUTS[BELOW_VXLOW].name == "BELOW_VXLOW");

  static_assert(PAC2002_INPUTS[FZ].negated_by == sign_transform::VERTICAL);
  static_assert(PAC2002_INPUTS[SA].negated_by == sign_transform::LATERAL);
  static_assert(PAC2002_INPUTS[SR].negated_by == sign_transform::NONE);
  static_assert(PAC2002_INPUTS[IA].negated_by == sign_transform::NONE);
  static_assert(PAC2002_INPUTS[VX].negated_by == sign_transform::NONE);
  static_assert(MF52_INPUTS[FZ].negated_by == sign_transform::VERTICAL);
  static_assert(MF52_INPUTS[SA].negated_by == sign_transform::LATERAL);
  static_assert(MF52_INPUTS[SR].negated_by == sign_transform::NONE);
  static_assert(MF52_INPUTS[IA].negated_by == sign_transform::NONE);
  static_assert(MF52_INPUTS[VX].negated_by == sign_transform::NONE);
  static_assert(OUTPUTS[FX].negated_by == sign_transform::NONE);
  static_assert(OUTPUTS[FY].negated_by == sign_transform::VERTICAL);
  static_assert(OUTPUTS[MZ].negated_by == sign_transform::VERTICAL);
  static_assert(OUTPUTS[MX].negated_by == sign_transform::NONE);
  static_assert(OUTPUTS[MY].negated_by == sign_transform::VERTICAL);
  static_assert(OUTPUTS[FZ_CLAMPED].negated_by == sign_transform::NONE);
  static_assert(OUTPUTS[SR_CLAMPED].negated_by == sign_transform::NONE);
  static_assert(OUTPUTS[SA_CLAMPED].negated_by == sign_transform::NONE);
  static_assert(OUTPUTS[IA_CLAMPED].negated_by == sign_transform::NONE);
  static_assert(OUTPUTS[BELOW_VXLOW].negated_by == sign_transform::NONE);
  static_assert(LIMITS[FZMIN].negated_by == sign_transform::VERTICAL);
  static_assert(LIMITS[FZMAX].negated_by == sign_transform::VERTICAL);
  static_assert(LIMITS[KPUMIN].negated_by == sign_transform::NONE);
  static_assert(LIMITS[KPUMAX].negated_by == sign_transform::NONE);
  static_assert(LIMITS[ALPMIN].negated_by == sign_transform::LATERAL);
  static_assert(LIMITS[ALPMAX].negated_by == sign_transform::LATERAL);
  static_assert(LIMITS[CAMMIN].negated_by == sign_transform::NONE);
  static_assert(LIMITS[CAMMAX].negated_by == sign_transform::NONE);
  static_assert(LIMITS[VXLOW].negated_by == sign_transform::NONE);
  static_assert(limit_signs_well_formed(LIMITS));

  static_assert(LIMITS[FZMIN].key == "FZMIN");
  static_assert(LIMITS[FZMAX].key == "FZMAX");
  static_assert(LIMITS[KPUMIN].key == "KPUMIN");
  static_assert(LIMITS[KPUMAX].key == "KPUMAX");
  static_assert(LIMITS[ALPMIN].key == "ALPMIN");
  static_assert(LIMITS[ALPMAX].key == "ALPMAX");
  static_assert(LIMITS[CAMMIN].key == "CAMMIN");
  static_assert(LIMITS[CAMMAX].key == "CAMMAX");
  static_assert(LIMITS[VXLOW].key == "VXLOW");
} // namespace mf::mf5x

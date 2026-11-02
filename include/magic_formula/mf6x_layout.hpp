#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/model_layout.hpp>
namespace mf::mf6x {
  // NOLINTBEGIN(cppcoreguidelines-use-enum-class): model enumerators are vector positions
  /// Input positions, with pressure and turn slip appended to the MF 5.x order.
  enum input_index : std::uint8_t { FZ, SA, SR, IA, VX, P, TS };
  /// Output positions; the first ten retain the MF 5.x order.
  enum output_index : std::uint8_t {
    FX,
    FY,
    MZ,
    MX,
    MY,
    FZ_CLAMPED,
    SR_CLAMPED,
    SA_CLAMPED,
    IA_CLAMPED,
    BELOW_VXLOW,
    P_CLAMPED,
    RE,
    RL
  };
  /// Validity-limit positions.
  enum limit_index : std::uint8_t {
    FZMIN,
    FZMAX,
    KPUMIN,
    KPUMAX,
    ALPMIN,
    ALPMAX,
    CAMMIN,
    CAMMAX,
    VXLOW,
    PRESMIN,
    PRESMAX
  };
  /// Characteristic positions, identical to MF 5.x.
  enum characteristic_index : std::uint8_t {
    DFY_DSA,
    DFX_DSR,
    FY_OVER_SA,
    FX_OVER_SR,
    DY,
    DX,
    SA_AT_PEAK_FY,
    SR_AT_PEAK_FX
  };
  // NOLINTEND(cppcoreguidelines-use-enum-class)
  inline constexpr int INPUT_LENGTH = 7;
  inline constexpr int OUTPUT_LENGTH = 13;
  inline constexpr int LIMITS_LENGTH = 11;
  inline constexpr int CHARACTERISTICS_LENGTH = 8;
  inline constexpr std::array<input_channel, INPUT_LENGTH> INPUTS{{
      {.name = "FZ", .unit = "N", .negated_by = sign_transform::VERTICAL},
      {.name = "SA", .unit = "rad", .negated_by = sign_transform::LATERAL},
      {.name = "SR", .unit = "-"},
      {.name = "IA", .unit = "rad"},
      {.name = "VX", .unit = "m/s", .minimum = 0.0, .minimum_inclusive = false},
      {.name = "P", .unit = "Pa", .minimum = 0.0, .minimum_inclusive = false},
      {.name = "TS", .unit = "1/m"},
  }};
  inline constexpr auto OUTPUTS = [] {
    std::array<output_channel, OUTPUT_LENGTH> values{};
    std::ranges::copy(mf5x::OUTPUTS, values.begin());
    values[P_CLAMPED] = {.name = "P_CLAMPED", .unit = "-"};
    values[RE] = {.name = "RE", .unit = "m"};
    values[RL] = {.name = "RL", .unit = "m"};
    return values;
  }();
  inline constexpr auto LIMITS = [] {
    std::array<limit_channel, LIMITS_LENGTH> values{};
    std::ranges::copy(mf5x::LIMITS, values.begin());
    values[PRESMIN] = {.key = "PRESMIN", .role = limit_role::LOWER, .default_value = -mf5x::INF};
    values[PRESMAX] = {.key = "PRESMAX", .role = limit_role::UPPER, .default_value = mf5x::INF};
    return values;
  }();
  inline constexpr auto CHARACTERISTICS = mf5x::CHARACTERISTICS;
  inline constexpr model_layout LAYOUT{
      .inputs = INPUTS, .outputs = OUTPUTS, .limits = LIMITS, .characteristics = CHARACTERISTICS};
  static_assert(limit_pairs_well_formed(LIMITS));
  static_assert(limit_signs_well_formed(LIMITS));
} // namespace mf::mf6x

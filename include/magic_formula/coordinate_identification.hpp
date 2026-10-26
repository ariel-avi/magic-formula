#pragma once

#include <Eigen/Core>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <magic_formula/coordinate_system.hpp>
#include <magic_formula/error.hpp>
#include <numbers>
#include <span>
#include <string_view>
#include <vector>

namespace mf {
  /// Probe loads of a load-swept indicator, as multiples of the signed reference FZ (FNOMIN*LFZO).
  inline constexpr std::array<double, 3> IDENTIFICATION_LOAD_SCALES{0.5, 1.0, 1.5};
  /// Minimum valid vote weight for an axis to be decided.
  inline constexpr double IDENTIFICATION_MIN_WEIGHT = 0.25;
  /// Minimum share of the valid vote weight that the winning side must hold (a tie is 0.5).
  inline constexpr double IDENTIFICATION_MIN_CONFIDENCE = 0.6;
  /// Degrees to radians.
  inline constexpr double RAD_PER_DEG = std::numbers::pi / 180.0;

  /// What the sign of an indicator says something about.
  enum class identification_axis : std::uint8_t {
    VERTICAL, ///< loaded FZ sign (z up / z down); readings flip under the VERTICAL transform only
    LATERAL,  ///< sign of dFy/dSA; readings flip under both transforms
    CHECK,    ///< invariant under both transforms: a consistency check that never votes
  };

  /// How a value is obtained at one probe load.
  enum class indicator_form : std::uint8_t {
    INPUT_LEVEL,        ///< input `channel` at the probe point (FZ: the signed nominal load)
    OUTPUT_LEVEL,       ///< output `response` at the probe point
    CENTRAL_DIFFERENCE, ///< (response(channel + step) - response(channel - step)) / (2*step)
  };

  /// One row of the data-driven indicator table.
  struct indicator_definition {
    std::string_view name;    ///< unique short name used in warnings
    std::string_view meaning; ///< the physical expectation behind the vote, used in warnings
    std::string_view unit;    ///< unit of the value
    identification_axis axis = identification_axis::CHECK;
    indicator_form form = indicator_form::OUTPUT_LEVEL;
    std::string_view channel;  ///< input name (INPUT_LEVEL, CENTRAL_DIFFERENCE); empty otherwise
    std::string_view response; ///< output name (OUTPUT_LEVEL, CENTRAL_DIFFERENCE); empty otherwise
    double step = 0.0;         ///< half step, unit of `channel`
    double polarity = 1.0;     ///< vote = polarity*sign(value); +1 means the ISO side (CHECK: consistent)
    double weight = 0.0;       ///< total vote weight, shared evenly by its readings; 0 for CHECK
    double threshold = 0.0;    ///< abstains when |value|/|probe FZ| <= threshold (value unit per N)
    bool load_sweep = false;   ///< one reading per IDENTIFICATION_LOAD_SCALES entry, else one at scale 1
  };

  /// The default table. The axis assignment, weights and thresholds follow coordinate-systems.md.
  // clang-format off
  inline constexpr std::array<indicator_definition, 11> IDENTIFICATION_INDICATORS{{
      {.name = "FZ0",
       .meaning = "sign of the nominal load FNOMIN*LFZO: the loaded side of the equations",
       .unit = "N",
       .axis = identification_axis::VERTICAL,
       .form = indicator_form::INPUT_LEVEL,
       .channel = "FZ",
       .response = "",
       .step = 0.0,
       .polarity = 1.0,
       .weight = 1.0,
       .threshold = 0.5,
       .load_sweep = false},
      {.name = "dFy/dIA",
       .meaning = "camber thrust toward the lean (dFy/dIA < 0 with z up)",
       .unit = "N/rad",
       .axis = identification_axis::VERTICAL,
       .form = indicator_form::CENTRAL_DIFFERENCE,
       .channel = "IA",
       .response = "FY",
       .step = 1e-3,
       .polarity = -1.0,
       .weight = 0.25,
       .threshold = 1e-3,
       .load_sweep = true},
      {.name = "My",
       .meaning = "rolling resistance opposes rolling (My < 0 with z up)",
       .unit = "N*m",
       .axis = identification_axis::VERTICAL,
       .form = indicator_form::OUTPUT_LEVEL,
       .channel = "",
       .response = "MY",
       .step = 0.0,
       .polarity = -1.0,
       .weight = 0.25,
       .threshold = 1e-6,
       .load_sweep = false},
      {.name = "dFy/dSA",
       .meaning = "cornering stiffness (dFy/dSA > 0 in ISO)",
       .unit = "N/rad",
       .axis = identification_axis::LATERAL,
       .form = indicator_form::CENTRAL_DIFFERENCE,
       .channel = "SA",
       .response = "FY",
       .step = 1e-4,
       .polarity = 1.0,
       .weight = 1.0,
       .threshold = 1e-3,
       .load_sweep = true},
      {.name = "Fy secant 1 deg",
       .meaning = "odd part of Fy, immune to offsets",
       .unit = "N/rad",
       .axis = identification_axis::LATERAL,
       .form = indicator_form::CENTRAL_DIFFERENCE,
       .channel = "SA",
       .response = "FY",
       .step = 1.0 * RAD_PER_DEG,
       .polarity = 1.0,
       .weight = 1.0 / 3.0,
       .threshold = 1e-3,
       .load_sweep = true},
      {.name = "Fy secant 3 deg",
       .meaning = "odd part of Fy, immune to offsets",
       .unit = "N/rad",
       .axis = identification_axis::LATERAL,
       .form = indicator_form::CENTRAL_DIFFERENCE,
       .channel = "SA",
       .response = "FY",
       .step = 3.0 * RAD_PER_DEG,
       .polarity = 1.0,
       .weight = 1.0 / 3.0,
       .threshold = 1e-3,
       .load_sweep = true},
      {.name = "Fy secant 6 deg",
       .meaning = "odd part of Fy, immune to offsets",
       .unit = "N/rad",
       .axis = identification_axis::LATERAL,
       .form = indicator_form::CENTRAL_DIFFERENCE,
       .channel = "SA",
       .response = "FY",
       .step = 6.0 * RAD_PER_DEG,
       .polarity = 1.0,
       .weight = 1.0 / 3.0,
       .threshold = 1e-3,
       .load_sweep = true},
      {.name = "Fy odd part 20 deg",
       .meaning = "peak lateral force opposes the slip direction",
       .unit = "N/rad",
       .axis = identification_axis::LATERAL,
       .form = indicator_form::CENTRAL_DIFFERENCE,
       .channel = "SA",
       .response = "FY",
       .step = 20.0 * RAD_PER_DEG,
       .polarity = 1.0,
       .weight = 0.5,
       .threshold = 1e-3,
       .load_sweep = true},
      {.name = "dMz/dSA",
       .meaning = "positive pneumatic trail (dMz/dSA opposite to dFy/dSA)",
       .unit = "N*m/rad",
       .axis = identification_axis::LATERAL,
       .form = indicator_form::CENTRAL_DIFFERENCE,
       .channel = "SA",
       .response = "MZ",
       .step = 1e-4,
       .polarity = -1.0,
       .weight = 0.5,
       .threshold = 1e-5,
       .load_sweep = true},
      {.name = "dFx/dSR",
       .meaning = "driving force grows with slip ratio: the SR convention or the PKX1 sign is inconsistent with FNOMIN",
       .unit = "N",
       .axis = identification_axis::CHECK,
       .form = indicator_form::CENTRAL_DIFFERENCE,
       .channel = "SR",
       .response = "FX",
       .step = 1e-4,
       .polarity = 1.0,
       .weight = 0.0,
       .threshold = 1e-3,
       .load_sweep = true},
      {.name = "Fx secant 5 %",
       .meaning = "driving force grows with slip ratio: the SR convention or the PKX1 sign is inconsistent with FNOMIN",
       .unit = "N",
       .axis = identification_axis::CHECK,
       .form = indicator_form::CENTRAL_DIFFERENCE,
       .channel = "SR",
       .response = "FX",
       .step = 0.05,
       .polarity = 1.0,
       .weight = 0.0,
       .threshold = 1e-3,
       .load_sweep = true},
  }};
  // clang-format on

  /// True when no two rows share a name (quadratic comparison).
  [[nodiscard]] constexpr bool indicator_names_unique(std::span<const indicator_definition> table) noexcept {
    for (std::size_t i = 0; i < table.size(); ++i) {
      for (std::size_t j = i + 1; j < table.size(); ++j) {
        if (table[i].name == table[j].name) {
          return false;
        }
      }
    }
    return true;
  }

  /// True when every CHECK row has weight 0.
  [[nodiscard]] constexpr bool check_weights_zero(std::span<const indicator_definition> table) noexcept {
    const auto zero_unless_vote = [](const indicator_definition &row) {
      return row.axis != identification_axis::CHECK || row.weight == 0.0;
    };
    return std::ranges::all_of(table, zero_unless_vote);
  }

  /// Sum of the weights of the rows on `axis`.
  [[nodiscard]] constexpr double axis_weight(std::span<const indicator_definition> table,
                                             identification_axis axis) noexcept {
    double total = 0.0;
    for (const indicator_definition &row : table) {
      if (row.axis == axis) {
        total += row.weight;
      }
    }
    return total;
  }

  inline constexpr double IDENTIFICATION_VERTICAL_WEIGHT =
      axis_weight(IDENTIFICATION_INDICATORS, identification_axis::VERTICAL);

  static_assert(IDENTIFICATION_INDICATORS[0].name == "FZ0");
  static_assert(indicator_names_unique(IDENTIFICATION_INDICATORS));
  static_assert(check_weights_zero(IDENTIFICATION_INDICATORS));
  static_assert(IDENTIFICATION_INDICATORS[0].weight >
                IDENTIFICATION_VERTICAL_WEIGHT - IDENTIFICATION_INDICATORS[0].weight);
  static_assert(IDENTIFICATION_INDICATORS[0].weight / IDENTIFICATION_VERTICAL_WEIGHT >= IDENTIFICATION_MIN_CONFIDENCE);

  /// Outcome of one reading.
  enum class reading_status : std::uint8_t {
    VOTE,            ///< valid: contributes `vote` with `weight`
    BELOW_THRESHOLD, ///< |normalised| <= threshold: abstains
    NOT_FINITE,      ///< value or normalised value NaN/inf: abstains
    CLAMPED,         ///< a "*_CLAMPED" output was set on one of its probe rows: skipped
    MISSING_CHANNEL, ///< the layout lacks FZ, `channel` or `response`: skipped
  };

  /// One evaluated indicator at one probe load.
  struct indicator_reading {
    indicator_definition definition; ///< copied; the caller's table strings must outlive it
    double load_scale = 1.0;
    double load_n = 0.0;                                          ///< probe FZ [N]
    double value = std::numeric_limits<double>::quiet_NaN();      ///< in definition.unit
    double normalised = std::numeric_limits<double>::quiet_NaN(); ///< value / |load_n|
    reading_status status = reading_status::MISSING_CHANNEL;
    int vote = 0;        ///< +1 / -1 for VOTE, else 0
    double weight = 0.0; ///< definition.weight / (load_sweep ? 3 : 1) for VOTE, else 0
  };

  /// POSITIVE is the ISO side of an axis.
  enum class axis_sign : std::uint8_t { POSITIVE, NEGATIVE, UNDETERMINED };

  /// Why an axis is or is not decided.
  enum class decision_basis : std::uint8_t { DECIDED, INSUFFICIENT_EVIDENCE, CONFLICTING_EVIDENCE };

  /// Weighted vote of one axis.
  struct axis_decision {
    axis_sign sign = axis_sign::UNDETERMINED;
    decision_basis basis = decision_basis::INSUFFICIENT_EVIDENCE;
    double confidence = 0.0;      ///< max(W+, W-) / (W+ + W-); 0 without valid votes
    double positive_weight = 0.0; ///< W+
    double negative_weight = 0.0; ///< W-
  };

  /// Decided signs used for classification and conversion (never UNDETERMINED).
  struct axis_signs {
    axis_sign vertical = axis_sign::POSITIVE;
    axis_sign lateral = axis_sign::POSITIVE;
  };

  /// The system a coefficient set belongs to.
  enum class system_classification : std::uint8_t { ISO, SAE, MIXED };

  /// The result of `tire_model::find_coordinate_system`.
  struct coordinate_identification {
    system_classification classification = system_classification::ISO; ///< from `effective`
    axis_decision vertical;
    axis_decision lateral;
    axis_signs effective;
    std::vector<indicator_reading> readings; ///< every reading, table order, then load order
    Eigen::VectorXd reference_input;         ///< `tire_model::reference_input()`
    std::vector<warning> warnings;           ///< `identification_warnings(*this)`
  };

  /// One reading. Status precedence, first match wins: missing_channel -> MISSING_CHANNEL; clamped -> CLAMPED;
  /// !isfinite(value) or !isfinite(normalised) -> NOT_FINITE; |normalised| <= threshold -> BELOW_THRESHOLD; else VOTE
  /// with vote = value*polarity > 0 ? +1 : -1 and weight = definition.weight / (load_sweep ? 3 : 1). normalised =
  /// value/|load_n|. value and normalised are always stored.
  [[nodiscard]] indicator_reading make_reading(const indicator_definition &definition, double load_scale, double load_n,
                                               double value, bool clamped, bool missing_channel) noexcept;

  /// Sums the VOTE weights of `axis` (other axes, CHECK and non-VOTE readings ignored). confidence = W > 0 ? max/W : 0;
  /// W < IDENTIFICATION_MIN_WEIGHT -> UNDETERMINED, INSUFFICIENT_EVIDENCE; confidence < IDENTIFICATION_MIN_CONFIDENCE
  /// -> UNDETERMINED, CONFLICTING_EVIDENCE; else DECIDED, POSITIVE iff W+ > W-.
  [[nodiscard]] axis_decision decide_axis(std::span<const indicator_reading> readings,
                                          identification_axis axis) noexcept;

  /// Each decided sign; an undetermined axis takes the other axis's decided sign; POSITIVE when both are undetermined.
  [[nodiscard]] axis_signs effective_signs(const axis_decision &vertical, const axis_decision &lateral) noexcept;

  /// (+,+) ISO, (-,-) SAE, otherwise MIXED.
  [[nodiscard]] system_classification classify_system(const axis_signs &effective) noexcept;

  /// Target signs: ISO (+,+), SAE (-,-). vertical = effective.vertical != target vertical; the lateral sign after it
  /// is negated iff vertical; lateral = that sign != target lateral. (+,-)->ISO {L}, (+,-)->SAE {V,L}, (-,+)->ISO
  /// {V,L}, (-,+)->SAE {L}, ISO<->SAE {V}, same system {}.
  [[nodiscard]] sign_transforms plan_conversion(const axis_signs &effective, coordinate_system target) noexcept;

  /// Decisions, effective signs, classification and warnings from the readings.
  [[nodiscard]] coordinate_identification assemble_identification(std::vector<indicator_reading> readings,
                                                                  Eigen::VectorXd reference_input);

  /// The warnings, in order: vertical axis (key "FZ"), lateral axis (key "SA"), mixed (key "SA"), failed checks (key =
  /// the check's channel). Numbers are formatted "{:.2f}" (confidence) or "{:.6g}" (values); file empty, line 0.
  ///  - an axis without evidence: "coordinate system: the <vertical|lateral> axis is undetermined (no significant
  ///    indicator); <fallback>"
  ///  - an axis with conflicting evidence: "... (the indicators conflict: confidence <c> < 0.60); <fallback>"; the
  ///    fallback is "it is assumed to match the <other> axis (<ISO|SAE>)" or "ISO is assumed"
  ///  - a decided axis with dissenting votes: "coordinate system: <name (meaning)>, ... disagree with the <axis> axis
  ///    decision (<ISO|SAE>, confidence <c>)"
  ///  - MIXED: "coordinate system: the coefficients mix conventions: ..."
  ///  - a CHECK with a negative reading: "coordinate system: <name> = <value> <unit> at <scale> x the nominal load is
  ///    negative: <meaning>; no conversion changes this"
  [[nodiscard]] std::vector<warning> identification_warnings(const coordinate_identification &identification);

  /// "VERTICAL", "LATERAL", "CHECK".
  [[nodiscard]] std::string_view identification_axis_name(identification_axis axis) noexcept;
  /// "INPUT_LEVEL", "OUTPUT_LEVEL", "CENTRAL_DIFFERENCE".
  [[nodiscard]] std::string_view indicator_form_name(indicator_form form) noexcept;
  /// "VOTE", "BELOW_THRESHOLD", "NOT_FINITE", "CLAMPED", "MISSING_CHANNEL".
  [[nodiscard]] std::string_view reading_status_name(reading_status status) noexcept;
  /// "POSITIVE", "NEGATIVE", "UNDETERMINED".
  [[nodiscard]] std::string_view axis_sign_name(axis_sign sign) noexcept;
  /// "DECIDED", "INSUFFICIENT_EVIDENCE", "CONFLICTING_EVIDENCE".
  [[nodiscard]] std::string_view decision_basis_name(decision_basis basis) noexcept;
  /// "ISO", "SAE", "MIXED".
  [[nodiscard]] std::string_view system_classification_name(system_classification classification) noexcept;
} // namespace mf

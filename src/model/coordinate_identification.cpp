#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <magic_formula/coordinate_identification.hpp>
#include <magic_formula/coordinate_system.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/sign_transform.hpp>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mf {
  namespace {
    constexpr std::array<std::string_view, 3> AXIS_NAMES{"VERTICAL", "LATERAL", "CHECK"};
    constexpr std::array<std::string_view, 3> FORM_NAMES{"INPUT_LEVEL", "OUTPUT_LEVEL", "CENTRAL_DIFFERENCE"};
    constexpr std::array<std::string_view, 5> STATUS_NAMES{"VOTE", "BELOW_THRESHOLD", "NOT_FINITE", "CLAMPED",
                                                           "MISSING_CHANNEL"};
    constexpr std::array<std::string_view, 3> SIGN_NAMES{"POSITIVE", "NEGATIVE", "UNDETERMINED"};
    constexpr std::array<std::string_view, 3> BASIS_NAMES{"DECIDED", "INSUFFICIENT_EVIDENCE", "CONFLICTING_EVIDENCE"};
    constexpr std::array<std::string_view, 3> CLASSIFICATION_NAMES{"ISO", "SAE", "MIXED"};

    /// Indexed by 2*(vertical == NEGATIVE) + (lateral == NEGATIVE).
    constexpr std::array<system_classification, 4> CLASSIFICATION_TABLE{
        system_classification::ISO, system_classification::MIXED, system_classification::MIXED,
        system_classification::SAE};

    /// Words of a decided axis sign, indexed by axis_sign.
    constexpr std::array<std::string_view, 3> SIGN_WORDS{"ISO", "SAE", "undetermined"};
    /// Relation of the axis quantity to zero on the loaded side, indexed by axis_sign.
    constexpr std::array<std::string_view, 3> SIGN_RELATIONS{">", "<", "?"};
    /// Names of the VERTICAL and LATERAL axes in warnings, indexed by identification_axis.
    constexpr std::array<std::string_view, 2> AXIS_WORDS{"vertical", "lateral"};
    /// The key of the warnings of the VERTICAL and LATERAL axes, indexed by identification_axis.
    constexpr std::array<std::string_view, 2> AXIS_KEYS{"FZ", "SA"};

    /// `table[index]` for a runtime index (the enumerators are the table positions).
    template <typename Value_T, std::size_t Extent_T>
    const Value_T &lookup(const std::array<Value_T, Extent_T> &table, std::size_t index) noexcept {
      return std::span<const Value_T, Extent_T>(table)[index];
    }

    std::string_view sign_word(axis_sign sign) noexcept {
      return lookup(SIGN_WORDS, static_cast<std::size_t>(sign));
    }

    std::string_view sign_relation(axis_sign sign) noexcept {
      return lookup(SIGN_RELATIONS, static_cast<std::size_t>(sign));
    }

    std::string_view axis_word(identification_axis axis) noexcept {
      return lookup(AXIS_WORDS, static_cast<std::size_t>(axis));
    }

    std::string_view axis_key(identification_axis axis) noexcept {
      return lookup(AXIS_KEYS, static_cast<std::size_t>(axis));
    }

    /// The reading status of one value; first match wins.
    reading_status status_of(const indicator_definition &definition, double value, double normalised, bool clamped,
                             bool missing_channel) noexcept {
      if (missing_channel) {
        return reading_status::MISSING_CHANNEL;
      }
      if (clamped) {
        return reading_status::CLAMPED;
      }
      if (!std::isfinite(value) || !std::isfinite(normalised)) {
        return reading_status::NOT_FINITE;
      }
      if (std::abs(normalised) <= definition.threshold) {
        return reading_status::BELOW_THRESHOLD;
      }
      return reading_status::VOTE;
    }

    /// The decided sign of `own`, else the decided sign of `other`, else POSITIVE.
    axis_sign resolve_sign(const axis_decision &own, const axis_decision &other) noexcept {
      if (own.sign != axis_sign::UNDETERMINED) {
        return own.sign;
      }
      if (other.sign != axis_sign::UNDETERMINED) {
        return other.sign;
      }
      return axis_sign::POSITIVE;
    }

    /// POSITIVE <-> NEGATIVE.
    axis_sign opposite(axis_sign sign) noexcept {
      return sign == axis_sign::POSITIVE ? axis_sign::NEGATIVE : axis_sign::POSITIVE;
    }

    /// A warning without file and line; built member by member and never returned (a returned local would add an
    /// exception-cleanup-only line that no test can execute).
    void push_warning(std::vector<warning> &warnings, std::string_view key, std::string message) {
      warning item;
      item.message = std::move(message);
      item.where.key = key;
      warnings.push_back(std::move(item));
    }

    /// True for a VOTE reading of `axis` whose vote equals `against`.
    bool is_vote_against(const indicator_reading &reading, identification_axis axis, int against) noexcept {
      if (reading.definition.axis != axis) {
        return false;
      }
      if (reading.status != reading_status::VOTE) {
        return false;
      }
      return reading.vote == against;
    }

    /// True when `names` holds `name`.
    bool listed(const std::vector<std::string_view> &names, std::string_view name) {
      return std::ranges::find(names, name) != names.end();
    }

    /// "name (meaning)" of the distinct definitions, in reading order, of the VOTE readings of `axis` whose vote is
    /// `against`, joined with ", ".
    std::string dissenters(std::span<const indicator_reading> readings, identification_axis axis, int against) {
      std::vector<std::string_view> seen;
      std::string joined;
      for (const indicator_reading &reading : readings) {
        if (is_vote_against(reading, axis, against) && !listed(seen, reading.definition.name)) {
          if (!joined.empty()) {
            joined += ", ";
          }
          joined += std::format("{} ({})", reading.definition.name, reading.definition.meaning);
          seen.push_back(reading.definition.name);
        }
      }
      // Moved into a new string: a returned (elided) local leaves its destructor call off the closing brace, whose
      // only code is then the exception-cleanup resume that no test can execute.
      return std::string{std::move(joined)};
    }

    /// What an undetermined axis is assumed to be.
    std::string fallback_text(const axis_decision &other, identification_axis other_axis) {
      if (other.sign != axis_sign::UNDETERMINED) {
        return std::format("it is assumed to match the {} axis ({})", axis_word(other_axis), sign_word(other.sign));
      }
      return "ISO is assumed";
    }

    /// At most one warning for `axis`: no evidence, conflicting evidence, or dissenting votes of a decided axis.
    void add_axis_warning(const coordinate_identification &identification, identification_axis axis,
                          std::vector<warning> &warnings) {
      const bool vertical = axis == identification_axis::VERTICAL;
      const axis_decision &own = vertical ? identification.vertical : identification.lateral;
      const axis_decision &other = vertical ? identification.lateral : identification.vertical;
      const identification_axis other_axis = vertical ? identification_axis::LATERAL : identification_axis::VERTICAL;
      if (own.basis == decision_basis::INSUFFICIENT_EVIDENCE) {
        push_warning(warnings, axis_key(axis),
                     std::format("coordinate system: the {} axis is undetermined (no significant indicator); {}",
                                 axis_word(axis), fallback_text(other, other_axis)));
      } else if (own.basis == decision_basis::CONFLICTING_EVIDENCE) {
        push_warning(warnings, axis_key(axis),
                     std::format("coordinate system: the {} axis is undetermined (the indicators conflict: "
                                 "confidence {:.2f} < {:.2f}); {}",
                                 axis_word(axis), own.confidence, IDENTIFICATION_MIN_CONFIDENCE,
                                 fallback_text(other, other_axis)));
      } else if (std::min(own.positive_weight, own.negative_weight) > 0.0) {
        const int against = own.sign == axis_sign::POSITIVE ? -1 : 1;
        push_warning(warnings, axis_key(axis),
                     std::format("coordinate system: {} disagree with the {} axis decision ({}, confidence {:.2f})",
                                 dissenters(identification.readings, axis, against), axis_word(axis),
                                 sign_word(own.sign), own.confidence));
      }
    }

    /// The mixed-conventions warning when the classification is MIXED.
    void add_mixed_warning(const coordinate_identification &identification, std::vector<warning> &warnings) {
      if (identification.classification != system_classification::MIXED) {
        return;
      }
      const axis_signs &e = identification.effective;
      push_warning(warnings, "SA",
                   std::format("coordinate system: the coefficients mix conventions: the vertical axis is {} (a loaded "
                               "tire has FZ {} 0) but the lateral axis is {} (dFy/dSA {} 0); convert the model to ISO "
                               "or SAE",
                               sign_word(e.vertical), sign_relation(e.vertical), sign_word(e.lateral),
                               sign_relation(e.lateral)));
    }

    /// One warning per distinct CHECK definition with a negative reading (its first one).
    void add_check_warnings(const coordinate_identification &identification, std::vector<warning> &warnings) {
      std::vector<std::string_view> seen;
      for (const indicator_reading &reading : identification.readings) {
        if (is_vote_against(reading, identification_axis::CHECK, -1) && !listed(seen, reading.definition.name)) {
          seen.push_back(reading.definition.name);
          push_warning(warnings, reading.definition.channel,
                       std::format("coordinate system: {} = {:.6g} {} at {:g} x the nominal load is negative: {}; no "
                                   "conversion changes this",
                                   reading.definition.name, reading.value, reading.definition.unit, reading.load_scale,
                                   reading.definition.meaning));
        }
      }
    }
  } // namespace

  indicator_reading make_reading(const indicator_definition &definition, double load_scale, double load_n, double value,
                                 bool clamped, bool missing_channel) noexcept {
    const double normalised = value / std::abs(load_n);
    const reading_status status = status_of(definition, value, normalised, clamped, missing_channel);
    const bool voting = status == reading_status::VOTE;
    const int direction = (value * definition.polarity > 0.0) ? 1 : -1;
    const double share = definition.load_sweep ? static_cast<double>(IDENTIFICATION_LOAD_SCALES.size()) : 1.0;
    return indicator_reading{.definition = definition,
                             .load_scale = load_scale,
                             .load_n = load_n,
                             .value = value,
                             .normalised = normalised,
                             .status = status,
                             .vote = voting ? direction : 0,
                             .weight = voting ? definition.weight / share : 0.0};
  }

  axis_decision decide_axis(std::span<const indicator_reading> readings, identification_axis axis) noexcept {
    double positive = 0.0;
    double negative = 0.0;
    for (const indicator_reading &reading : readings) {
      if (reading.definition.axis == axis && reading.status == reading_status::VOTE) {
        if (reading.vote > 0) {
          positive += reading.weight;
        } else {
          negative += reading.weight;
        }
      }
    }
    const double total = positive + negative;
    const double confidence = total > 0.0 ? std::max(positive, negative) / total : 0.0;
    if (total < IDENTIFICATION_MIN_WEIGHT) {
      return axis_decision{.sign = axis_sign::UNDETERMINED,
                           .basis = decision_basis::INSUFFICIENT_EVIDENCE,
                           .confidence = confidence,
                           .positive_weight = positive,
                           .negative_weight = negative};
    }
    if (confidence < IDENTIFICATION_MIN_CONFIDENCE) {
      return axis_decision{.sign = axis_sign::UNDETERMINED,
                           .basis = decision_basis::CONFLICTING_EVIDENCE,
                           .confidence = confidence,
                           .positive_weight = positive,
                           .negative_weight = negative};
    }
    return axis_decision{.sign = positive > negative ? axis_sign::POSITIVE : axis_sign::NEGATIVE,
                         .basis = decision_basis::DECIDED,
                         .confidence = confidence,
                         .positive_weight = positive,
                         .negative_weight = negative};
  }

  axis_signs effective_signs(const axis_decision &vertical, const axis_decision &lateral) noexcept {
    return axis_signs{.vertical = resolve_sign(vertical, lateral), .lateral = resolve_sign(lateral, vertical)};
  }

  system_classification classify_system(const axis_signs &effective) noexcept {
    const std::size_t index = (2U * static_cast<std::size_t>(effective.vertical == axis_sign::NEGATIVE)) +
                              static_cast<std::size_t>(effective.lateral == axis_sign::NEGATIVE);
    return lookup(CLASSIFICATION_TABLE, index);
  }

  sign_transforms plan_conversion(const axis_signs &effective, coordinate_system target) noexcept {
    const axis_sign target_sign = target == coordinate_system::SAE ? axis_sign::NEGATIVE : axis_sign::POSITIVE;
    const bool vertical = effective.vertical != target_sign;
    const axis_sign lateral_after = vertical ? opposite(effective.lateral) : effective.lateral;
    return sign_transforms{.vertical = vertical, .lateral = lateral_after != target_sign};
  }

  coordinate_identification assemble_identification(std::vector<indicator_reading> readings,
                                                    Eigen::VectorXd reference_input) {
    const axis_decision vertical = decide_axis(readings, identification_axis::VERTICAL);
    const axis_decision lateral = decide_axis(readings, identification_axis::LATERAL);
    const axis_signs effective = effective_signs(vertical, lateral);
    coordinate_identification partial{.classification = classify_system(effective),
                                      .vertical = vertical,
                                      .lateral = lateral,
                                      .effective = effective,
                                      .readings = std::move(readings),
                                      .reference_input = std::move(reference_input),
                                      .warnings = {}};
    std::vector<warning> warnings = identification_warnings(partial);
    // Built as a new prvalue from moved parts: a returned (elided) local leaves its destructor call off the closing
    // brace, whose only code is then the exception-cleanup resume that no test can execute.
    return coordinate_identification{.classification = partial.classification,
                                     .vertical = partial.vertical,
                                     .lateral = partial.lateral,
                                     .effective = partial.effective,
                                     .readings = std::move(partial.readings),
                                     .reference_input = std::move(partial.reference_input),
                                     .warnings = std::move(warnings)};
  }

  std::vector<warning> identification_warnings(const coordinate_identification &identification) {
    std::vector<warning> warnings;
    add_axis_warning(identification, identification_axis::VERTICAL, warnings);
    add_axis_warning(identification, identification_axis::LATERAL, warnings);
    add_mixed_warning(identification, warnings);
    add_check_warnings(identification, warnings);
    // Moved out explicitly: a returned (elided) local leaves its destructor call off the closing brace, whose only
    // code is then the exception-cleanup resume that no test can execute.
    return std::vector<warning>{std::move(warnings)};
  }

  std::string_view identification_axis_name(identification_axis axis) noexcept {
    return lookup(AXIS_NAMES, static_cast<std::size_t>(axis));
  }

  std::string_view indicator_form_name(indicator_form form) noexcept {
    return lookup(FORM_NAMES, static_cast<std::size_t>(form));
  }

  std::string_view reading_status_name(reading_status status) noexcept {
    return lookup(STATUS_NAMES, static_cast<std::size_t>(status));
  }

  std::string_view axis_sign_name(axis_sign sign) noexcept {
    return lookup(SIGN_NAMES, static_cast<std::size_t>(sign));
  }

  std::string_view decision_basis_name(decision_basis basis) noexcept {
    return lookup(BASIS_NAMES, static_cast<std::size_t>(basis));
  }

  std::string_view system_classification_name(system_classification classification) noexcept {
    return lookup(CLASSIFICATION_NAMES, static_cast<std::size_t>(classification));
  }
} // namespace mf

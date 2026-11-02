#include <Eigen/Core>
#include <array>
#include <expected>
#include <format>
#include <magic_formula/detail/text.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/io/mf61_loader.hpp>
#include <magic_formula/io/model_loader.hpp>
#include <magic_formula/io/tir_reader.hpp>
#include <magic_formula/mf61_coefficients.hpp>
#include <magic_formula/mf61_model.hpp>
#include <magic_formula/mf6x_layout.hpp>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mf::io {
  namespace {
    /// Values the MF6.1 equations require rather than estimate.
    constexpr std::array<std::string_view, 12> REQUIRED_KEYS{"FNOMIN",
                                                             "UNLOADED_RADIUS",
                                                             "NOMPRES",
                                                             "LONGVL",
                                                             "VERTICAL_STIFFNESS",
                                                             "PCX1",
                                                             "PDX1",
                                                             "PKX1",
                                                             "PCY1",
                                                             "PDY1",
                                                             "PKY1",
                                                             "PKY2"};

    void warn(std::vector<warning> &warnings, const tir_reader &reader, std::string message, std::string_view key) {
      warning item;
      item.message = std::move(message);
      item.where = reader.locate(key);
      warnings.push_back(std::move(item));
    }

    std::expected<std::optional<double>, error> read_coefficient(tir_reader &reader, std::string_view key) {
      const auto result = reader.number(key);
      if (!result.has_value()) {
        return std::unexpected<error>(result.error());
      }
      const std::optional<double> value = *result;
      if (key != "LFZ0") {
        return value;
      }
      const auto alias_result = reader.number("LFZO");
      if (!alias_result.has_value()) {
        return std::unexpected<error>(alias_result.error());
      }
      const std::optional<double> alias = *alias_result;
      if (value.has_value()) {
        if (alias.has_value()) {
          if (*value != *alias) {
            return make_error(error_code::INVALID_VALUE, "LFZ0 and LFZO have conflicting values",
                              reader.locate("LFZO"));
          }
        }
        return value;
      }
      return alias;
    }

    std::expected<void, error> bind_coefficients(tir_reader &reader, mf61_coefficients &coefficients,
                                                 std::vector<warning> &warnings) {
      std::vector<std::string> defaulted;
      for (const auto &binding : mf6x::COEFFICIENTS) {
        const auto result = read_coefficient(reader, binding.key);
        if (!result.has_value()) {
          return std::unexpected<error>(result.error());
        }
        const std::optional<double> value = *result;
        if (value.has_value()) {
          coefficients.*binding.member = *value;
        } else if (std::ranges::contains(REQUIRED_KEYS, binding.key)) {
          return make_error(error_code::MISSING_KEY, std::format("required key {} is missing for MF 6.1", binding.key),
                            reader.locate(binding.key));
        } else if (binding.key == "LMUV" || !binding.key.starts_with('L')) {
          defaulted.emplace_back(binding.key);
        }
      }
      if (!defaulted.empty()) {
        warn(warnings, reader,
             std::format("MF 6.1: {} absent coefficient(s) use model defaults: {}", defaulted.size(),
                         detail::join(defaulted, ", ")),
             "FITTYP");
      }
      return {};
    }

    std::expected<void, error> bind_limits(tir_reader &reader, Eigen::VectorXd &limits,
                                           std::vector<warning> &warnings) {
      const std::span<const limit_channel> channels = mf6x::LIMITS;
      for (std::size_t i = 0; i < channels.size(); ++i) {
        if (channels[i].role == limit_role::UPPER) {
          continue;
        }
        const auto lower_result = reader.number(channels[i].key);
        if (!lower_result.has_value()) {
          return std::unexpected<error>(lower_result.error());
        }
        const std::optional<double> lower = *lower_result;
        if (channels[i].role == limit_role::VALUE) {
          limits(static_cast<Eigen::Index>(i)) = lower.value_or(channels[i].default_value);
          continue;
        }
        const auto upper_result = reader.number(channels[i + 1].key);
        if (!upper_result.has_value()) {
          return std::unexpected<error>(upper_result.error());
        }
        const std::optional<double> upper = *upper_result;
        if (lower.has_value() && upper.has_value()) {
          limits(static_cast<Eigen::Index>(i)) = *lower;
          limits(static_cast<Eigen::Index>(i + 1)) = *upper;
        } else if (lower.has_value() != upper.has_value()) {
          warn(warnings, reader,
               std::format("only one of {}/{} given; the range is not applied", channels[i].key, channels[i + 1].key),
               channels[i].key);
        }
      }
      return {};
    }

    std::expected<void, error> bind_metadata(tir_reader &reader, model_metadata &metadata,
                                             std::vector<warning> &warnings) {
      metadata.fittyp = 61;
      const auto use_mode = reader.integer("USE_MODE");
      if (!use_mode.has_value()) {
        return std::unexpected<error>(use_mode.error());
      }
      metadata.use_mode = *use_mode;
      if (use_mode->has_value()) {
        warn(warnings, reader, "USE_MODE is metadata only; slip and turn-slip modes are selected by the caller",
             "USE_MODE");
      }
      const auto side = reader.text("TYRESIDE");
      if (side.has_value()) {
        const std::string_view text = detail::trim(*side);
        if (detail::iequals_ascii(text, "RIGHT")) {
          metadata.side = tyre_side::RIGHT;
          warn(warnings, reader, "TYRESIDE 'RIGHT': no mirroring is applied", "TYRESIDE");
        } else if (!detail::iequals_ascii(text, "LEFT")) {
          return make_error(error_code::NOT_IMPLEMENTED, "only TYRESIDE LEFT or RIGHT is implemented",
                            reader.locate("TYRESIDE"));
        }
      }
      if (reader.contains("FE_METHOD")) {
        warn(warnings, reader, "FE_METHOD is ignored; MF 6.1 uses its cosine combined-slip equations", "FE_METHOD");
      }
      return {};
    }
  } // namespace

  std::expected<loaded_model, error> load_mf61(tir_reader &reader, std::vector<warning> warnings,
                                               const model_options &options) {
    mf61_parameters parameters;
    if (const auto bound = bind_coefficients(reader, parameters.coefficients, warnings); !bound.has_value()) {
      return std::unexpected<error>(bound.error());
    }
    if (const auto bound = bind_limits(reader, parameters.limits, warnings); !bound.has_value()) {
      return std::unexpected<error>(bound.error());
    }
    if (const auto bound = bind_metadata(reader, parameters.metadata, warnings); !bound.has_value()) {
      return std::unexpected<error>(bound.error());
    }
    auto created = mf61_model::create(parameters, options);
    if (!created.has_value()) {
      error cause = created.error();
      cause.where = reader.locate(cause.where.key);
      return std::unexpected<error>(std::move(cause));
    }
    loaded_model result;
    result.model = std::make_unique<mf61_model>(std::move(*created));
    result.warnings = std::move(warnings);
    result.ignored_keys = reader.unconsumed_keys();
    return result;
  }
} // namespace mf::io

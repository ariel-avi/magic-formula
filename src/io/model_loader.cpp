#include <Eigen/Core>
#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <magic_formula/detail/text.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/io/mf5x_bindings.hpp>
#include <magic_formula/io/mf61_loader.hpp>
#include <magic_formula/io/model_loader.hpp>
#include <magic_formula/io/model_routing.hpp>
#include <magic_formula/io/tir_document.hpp>
#include <magic_formula/io/tir_reader.hpp>
#include <magic_formula/io/units.hpp>
#include <magic_formula/mf52_model.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/pac2002_model.hpp>
#include <magic_formula/tire_model.hpp>
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
    /// Result of a loading step that produces no value.
    using status = std::expected<void, error>;

    /// A PAC2002 file has either all 20 combined-slip R* keys or none.
    constexpr std::size_t COMBINED_KEY_COUNT = 20;

    /// USE_MODE values from 0 up to (excluding) this limit are the documented ones; others only warn.
    constexpr int USE_MODE_LIMIT = 10;

    /// Keys that MF 5.2 files must carry although the model does not use their values.
    constexpr std::array<std::string_view, 2> MF52_DIMENSION_KEYS{"WIDTH", "RIM_RADIUS"};

    // clang-format off
    /// Format strings of the diagnostics that take arguments.
    constexpr std::string_view ONLY_ONE_BOUND_MESSAGE = "only one of {}/{} given; the range is not applied";
    constexpr std::string_view USE_MODE_MESSAGE =
        "USE_MODE {} is echoed only; combined/uncombined is selected by the caller";
    constexpr std::string_view DEFAULTED_MESSAGE = "{} coefficient(s) absent, defaulted to 0: {}";
    // clang-format on

    /// Everything the loading steps read and write.
    struct load_state {
      load_state(const tir_document &document, const model_options &model_opts)
          : reader(document), warnings(document.warnings().begin(), document.warnings().end()), options(model_opts) {}

      /// Typed, consumption-tracking access to the document.
      tir_reader reader;
      /// Warnings found so far; starts with the parse warnings of the document.
      std::vector<warning> warnings;
      /// Model chosen by `identify_model`.
      model_kind kind = model_kind::PAC2002;
      /// Parameters under construction.
      mf5x_parameters parameters;
      /// P/Q/S coefficient keys (and, for MF 5.2, R* keys) that were absent and keep the default 0.
      std::vector<std::string> defaulted;
      /// R* keys that were absent.
      std::vector<std::string> missing_r;
      /// PAC2002 combined-slip method; always COSINE for MF 5.2.
      combined_method method = combined_method::COSINE;
      /// Options forwarded to the model's `create`.
      model_options options;
    };

    /// A model together with the facts that the final warnings need.
    struct built_model {
      std::unique_ptr<const tire_model> model;
      bool my_from_shifts = false;
    };

    /// Location that names only the file of the document.
    location file_location(const tir_reader &reader) {
      return location{.file = reader.document().file_name(), .line = 0, .key = std::string{}};
    }

    void add_warning(load_state &state, std::string message, location where) {
      // Built member by member: an aggregate initialiser adds cleanup branches that no test can take.
      warning item;
      item.message = std::move(message);
      item.where = std::move(where);
      state.warnings.push_back(std::move(item));
    }

    /// Reads one bound key into its member, or records that it is absent according to its role.
    status bind_coefficient(load_state &state, const coefficient_binding &binding) {
      const std::expected<std::optional<double>, error> result = state.reader.number(binding.key);
      if (!result.has_value()) {
        return std::unexpected<error>(result.error());
      }
      const std::optional<double> value = *result;
      if (value.has_value()) {
        mf5x_coefficients &coefficients = state.parameters.coefficients;
        coefficients.*binding.member = *value;
        return {};
      }
      if (binding.role == binding_role::REQUIRED) {
        std::string message = std::format("required key {} is missing", binding.key);
        return make_error(error_code::MISSING_KEY, std::move(message), state.reader.locate(binding.key));
      }
      if (binding.role == binding_role::COEFFICIENT) {
        state.defaulted.emplace_back(binding.key);
      } else if (binding.role == binding_role::COMBINED) {
        state.missing_r.emplace_back(binding.key);
      }
      return {};
    }

    /// Binds every key of `bindings`; stops at the first error.
    status bind_span(load_state &state, std::span<const coefficient_binding> bindings) {
      for (const coefficient_binding &binding : bindings) {
        if (const status result = bind_coefficient(state, binding); !result.has_value()) {
          return result;
        }
      }
      return {};
    }

    /// MF 5.2: one warning for the present keys that are not MF 5.2 parameters: the listed keys, in table order, then
    /// every key of the foreign sections, in file order. The keys count as used. The warning is located at the
    /// first of them.
    void warn_foreign_keys(load_state &state) {
      std::vector<location> found;
      for (const std::string_view key : mf52_foreign_keys()) {
        if (state.reader.contains(key)) {
          found.push_back(state.reader.locate(key));
        }
      }
      for (const std::string_view section : mf52_foreign_sections()) {
        const std::vector<location> section_keys = state.reader.take_section(section);
        found.insert(found.end(), section_keys.begin(), section_keys.end());
      }
      if (!found.empty()) {
        const std::vector<std::string> present =
            found | std::views::transform(&location::key) | std::ranges::to<std::vector<std::string>>();
        const std::string names = detail::join(present, ", ");
        std::string message = std::format("MF 5.2 ignores parameters that are not part of MF 5.2: {}", names);
        add_warning(state, std::move(message), found.front());
      }
    }

    /// Step: the coefficient keys of the chosen model.
    status bind_coefficients(load_state &state) {
      if (const status common = bind_span(state, mf5x_common_bindings()); !common.has_value()) {
        return common;
      }
      if (state.kind == model_kind::PAC2002) {
        return bind_span(state, pac2002_camber_bindings());
      }
      warn_foreign_keys(state);
      return {};
    }

    /// Step: the R* rule.
    status select_combined_method(load_state &state) {
      if (state.kind == model_kind::MF52) {
        state.defaulted.insert(state.defaulted.end(), state.missing_r.begin(), state.missing_r.end());
        return {};
      }
      if (state.missing_r.empty()) {
        return {};
      }
      if (state.missing_r.size() == COMBINED_KEY_COUNT) {
        state.method = combined_method::FRICTION_ELLIPSE;
        return {};
      }
      const std::string names = detail::join(state.missing_r, ", ");
      std::string message = std::format("incomplete combined-slip coefficient set; missing: {}", names);
      return make_error(error_code::MISSING_KEY, std::move(message), state.reader.locate(state.missing_r.front()));
    }

    /// Step: LONGVL is required only when the speed terms QSY3 or QSY4 use it.
    status read_longvl(load_state &state) {
      const std::expected<std::optional<double>, error> result = state.reader.number("LONGVL");
      if (!result.has_value()) {
        return std::unexpected<error>(result.error());
      }
      const std::optional<double> value = *result;
      mf5x_coefficients &coefficients = state.parameters.coefficients;
      if (value.has_value()) {
        coefficients.LONGVL = *value;
        return {};
      }
      if (coefficients.QSY3 != 0.0 || coefficients.QSY4 != 0.0) {
        std::string message = "LONGVL is required when QSY3 or QSY4 is non-zero";
        return make_error(error_code::MISSING_KEY, std::move(message), state.reader.locate("LONGVL"));
      }
      return {};
    }

    /// Reads the LOWER/UPPER keys of the pair that starts at limit index `first`: both give the two bounds, one gives
    /// a warning and both bounds keep their defaults.
    status read_limit_pair(load_state &state, Eigen::Index first) {
      const std::span<const limit_channel> channels = mf5x::LIMITS;
      const std::string_view lower_key = channels[static_cast<std::size_t>(first)].key;
      const std::string_view upper_key = channels[static_cast<std::size_t>(first) + 1].key;
      const std::expected<std::optional<double>, error> lower_result = state.reader.number(lower_key);
      if (!lower_result.has_value()) {
        return std::unexpected<error>(lower_result.error());
      }
      const std::expected<std::optional<double>, error> upper_result = state.reader.number(upper_key);
      if (!upper_result.has_value()) {
        return std::unexpected<error>(upper_result.error());
      }
      const std::optional<double> lower = *lower_result;
      const std::optional<double> upper = *upper_result;
      if (lower.has_value() && upper.has_value()) {
        state.parameters.limits(first) = *lower;
        state.parameters.limits(first + 1) = *upper;
      } else if (lower.has_value() != upper.has_value()) {
        const std::string_view given = lower.has_value() ? lower_key : upper_key;
        std::string message = std::format(ONLY_ONE_BOUND_MESSAGE, lower_key, upper_key);
        add_warning(state, std::move(message), state.reader.locate(given));
      }
      return {};
    }

    /// Step: the limits vector, driven by `mf5x::LIMITS` in index order: a VALUE (VXLOW) overrides its default, a
    /// LOWER starts a range pair (see `read_limit_pair`), an UPPER is handled with its LOWER.
    status read_limits(load_state &state) {
      const std::span<const limit_channel> channels = mf5x::LIMITS;
      for (std::size_t i = 0; i < channels.size(); ++i) {
        const auto index = static_cast<Eigen::Index>(i);
        if (channels[i].role == limit_role::VALUE) {
          const std::expected<std::optional<double>, error> result = state.reader.number(channels[i].key);
          if (!result.has_value()) {
            return std::unexpected<error>(result.error());
          }
          double &value = state.parameters.limits(index);
          value = result->value_or(value);
        } else if (channels[i].role == limit_role::LOWER) {
          if (const status pair = read_limit_pair(state, index); !pair.has_value()) {
            return pair;
          }
        }
      }
      return {};
    }

    /// Step: FITTYP and USE_MODE (echoed only).
    status read_metadata(load_state &state) {
      model_metadata &metadata = state.parameters.metadata;
      // identify_model has already rejected a FITTYP that is not an integer, so no error can occur here.
      const std::expected<std::optional<int>, error> fittyp = state.reader.integer("FITTYP");
      metadata.fittyp = fittyp.value_or(std::optional<int>{});

      const std::expected<std::optional<int>, error> use_mode_result = state.reader.integer("USE_MODE");
      if (!use_mode_result.has_value()) {
        return std::unexpected<error>(use_mode_result.error());
      }
      const std::optional<int> use_mode = *use_mode_result;
      metadata.use_mode = use_mode;
      if (use_mode.has_value() && (*use_mode < 0 || *use_mode >= USE_MODE_LIMIT)) {
        std::string message = std::format(USE_MODE_MESSAGE, *use_mode);
        add_warning(state, std::move(message), state.reader.locate("USE_MODE"));
      }
      return {};
    }

    /// Step: TYRESIDE; LEFT is the default, RIGHT warns because nothing is mirrored.
    status read_tyreside(load_state &state) {
      const std::optional<std::string_view> text = state.reader.text("TYRESIDE");
      if (!text.has_value()) {
        return {};
      }
      const std::string_view side = detail::trim(*text);
      if (detail::iequals_ascii(side, "LEFT")) {
        return {};
      }
      if (detail::iequals_ascii(side, "RIGHT")) {
        state.parameters.metadata.side = tyre_side::RIGHT;
        std::string message = "TYRESIDE 'RIGHT': no mirroring is applied";
        add_warning(state, std::move(message), state.reader.locate("TYRESIDE"));
        return {};
      }
      std::string message = std::format("TYRESIDE '{}' is not implemented; only LEFT or RIGHT", side);
      return make_error(error_code::NOT_IMPLEMENTED, std::move(message), state.reader.locate("TYRESIDE"));
    }

    /// Step: FE_METHOD is not a PAC2002 or MF 5.2 key and is ignored.
    status warn_fe_method(load_state &state) {
      if (!state.reader.contains("FE_METHOD")) {
        return {};
      }
      std::string message = "FE_METHOD is not a PAC2002 or MF 5.2 key and is ignored";
      add_warning(state, std::move(message), state.reader.locate("FE_METHOD"));
      return {};
    }

    /// Step: MF 5.2 files must carry WIDTH and RIM_RADIUS; the values are not used.
    status require_mf52_dimensions(load_state &state) {
      if (state.kind != model_kind::MF52) {
        return {};
      }
      for (const std::string_view key : MF52_DIMENSION_KEYS) {
        const std::expected<std::optional<double>, error> result = state.reader.number(key);
        if (!result.has_value()) {
          return std::unexpected<error>(result.error());
        }
        if (!result->has_value()) {
          std::string message = std::format("required key {} is missing for MF 5.2", key);
          return make_error(error_code::MISSING_KEY, std::move(message), state.reader.locate(key));
        }
      }
      return {};
    }

    /// A loading step: reads keys, fills the state and may fail.
    using load_step = status (*)(load_state &state);

    // clang-format off
    /// The steps after model identification, in the order of `load_tire_model`.
    constexpr std::array<load_step, 8> STEPS{
        bind_coefficients,
        select_combined_method,
        read_longvl,
        read_limits,
        read_metadata,
        read_tyreside,
        warn_fe_method,
        require_mf52_dimensions,
    };
    // clang-format on

    /// A core error gets the file and the line of the .tir key it names; the key is kept.
    error with_source_location(const tir_reader &reader, error value) {
      value.where = reader.locate(value.where.key);
      return value;
    }

    std::expected<built_model, error> create_pac2002(const load_state &state) {
      std::expected<pac2002_model, error> created =
          pac2002_model::create(state.parameters, state.method, state.options);
      if (!created.has_value()) {
        return std::unexpected<error>(with_source_location(state.reader, created.error()));
      }
      const bool my_from_shifts = created->uses_my_from_shifts();
      return built_model{.model = std::make_unique<pac2002_model>(std::move(*created)),
                         .my_from_shifts = my_from_shifts};
    }

    std::expected<built_model, error> create_mf52(const load_state &state) {
      std::expected<mf52_model, error> created = mf52_model::create(state.parameters, state.options);
      if (!created.has_value()) {
        return std::unexpected<error>(with_source_location(state.reader, created.error()));
      }
      return built_model{.model = std::make_unique<mf52_model>(std::move(*created)), .my_from_shifts = false};
    }

    /// Builds the model that `identify_model` chose.
    std::expected<built_model, error> create_model(const load_state &state) {
      if (state.kind == model_kind::PAC2002) {
        return create_pac2002(state);
      }
      return create_mf52(state);
    }

    /// Warnings that depend on the created model: defaulted coefficients, ellipse method and the FITTYP 5
    /// rolling-resistance form.
    void add_final_warnings(load_state &state, bool my_from_shifts) {
      if (!state.defaulted.empty()) {
        const std::string names = detail::join(state.defaulted, ", ");
        std::string message = std::format(DEFAULTED_MESSAGE, state.defaulted.size(), names);
        add_warning(state, std::move(message), file_location(state.reader));
      }
      if (state.method == combined_method::FRICTION_ELLIPSE) {
        std::string message = "no combined-slip R* coefficients: using the friction-ellipse method";
        add_warning(state, std::move(message), file_location(state.reader));
      }
      if (my_from_shifts) {
        std::string message = "FITTYP 5 with QSY1 = QSY2 = 0: My uses the shift form -R0*(SVx + Kx*SHx)";
        add_warning(state, std::move(message), state.reader.locate("FITTYP"));
      }
    }
  } // namespace

  std::expected<loaded_model, error> load_tire_model(const tir_document &document, const model_options &options) {
    load_state state(document, options);
    if (const status units = check_units(state.reader); !units.has_value()) {
      return std::unexpected<error>(units.error());
    }
    const std::expected<model_kind, error> kind = identify_model(state.reader, state.warnings);
    if (!kind.has_value()) {
      return std::unexpected<error>(kind.error());
    }
    state.kind = *kind;
    if (state.kind == model_kind::MF61) {
      return load_mf61(state.reader, std::move(state.warnings), options);
    }
    for (const load_step apply : STEPS) {
      if (const status result = apply(state); !result.has_value()) {
        return std::unexpected<error>(result.error());
      }
    }
    std::expected<built_model, error> built = create_model(state);
    if (!built.has_value()) {
      return std::unexpected<error>(built.error());
    }
    add_final_warnings(state, built->my_from_shifts);
    // Built member by member: an aggregate initialiser adds cleanup branches that no test can take.
    loaded_model result;
    result.model = std::move(built->model);
    result.warnings = std::move(state.warnings);
    result.ignored_keys = state.reader.unconsumed_keys();
    return result;
  }

  std::expected<loaded_model, error> load_tire_model_file(const std::filesystem::path &path,
                                                          const model_options &options) {
    const std::expected<tir_document, error> document = read_tir_file(path);
    if (!document.has_value()) {
      return std::unexpected<error>(document.error());
    }
    return load_tire_model(*document, options);
  }
} // namespace mf::io

#include <Eigen/Core>
#include <array>
#include <cstddef>
#include <format>
#include <magic_formula/coordinate_system.hpp>
#include <magic_formula/detail/text.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/sign_transform.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace mf {
  namespace {
    constexpr std::array<std::string_view, 2> NAMES{"ISO", "SAE"};

    struct spelling {
      std::string_view text;
      coordinate_system system;
    };

    constexpr std::array<spelling, 2> SPELLINGS{{
        {.text = "ISO", .system = coordinate_system::ISO},
        {.text = "SAE", .system = coordinate_system::SAE},
    }};

    /// Format strings of the load-side warning, indexed by the system the table is read in: {} = row count, then the
    /// mixed-sign suffix.
    constexpr std::array<std::string_view, 2> LOAD_SIDE_FORMATS{
        "FZ < 0 in {} row(s) of an ISO table: a loaded tire has FZ > 0 in ISO; these rows are lift-off (zero forces "
        "and moments){}",
        "FZ > 0 in {} row(s) of an SAE table: a loaded tire has FZ < 0 in SAE; these rows are lift-off (zero forces "
        "and moments){}",
    };

    constexpr std::string_view MIXED_SUFFIX = " (the table mixes FZ signs)";
  } // namespace

  std::string_view coordinate_system_name(coordinate_system system) noexcept {
    const std::span<const std::string_view> names{NAMES};
    return names[static_cast<std::size_t>(system)];
  }

  std::optional<coordinate_system> parse_coordinate_system(std::string_view text) noexcept {
    for (const spelling &candidate : SPELLINGS) {
      if (detail::iequals_ascii(text, candidate.text)) {
        return candidate.system;
      }
    }
    return std::nullopt;
  }

  sign_transforms data_transforms(coordinate_system from, coordinate_system to) noexcept {
    return sign_transforms{.vertical = from != to, .lateral = false};
  }

  // NOLINTBEGIN(performance-unnecessary-value-param): a writable Eigen::Ref is taken by value so that it binds Map
  // temporaries (the Eigen idiom); a const reference to it would not bind a writable parameter.
  void convert_inputs(const model_layout &layout, coordinate_system from, coordinate_system to,
                      Eigen::Ref<Eigen::MatrixXd> inputs) noexcept {
    apply_sign_transforms(layout.inputs, data_transforms(from, to), inputs);
  }

  void convert_outputs(const model_layout &layout, coordinate_system from, coordinate_system to,
                       Eigen::Ref<Eigen::MatrixXd> outputs) noexcept {
    apply_sign_transforms(layout.outputs, data_transforms(from, to), outputs);
  }
  // NOLINTEND(performance-unnecessary-value-param)

  fz_sign_census census_fz(const model_layout &layout, const Eigen::Ref<const Eigen::MatrixXd> &inputs) noexcept {
    fz_sign_census census;
    const std::optional<std::size_t> fz = find_channel(layout.inputs, "FZ");
    if (!fz.has_value()) {
      return census;
    }
    const auto column = static_cast<Eigen::Index>(*fz);
    for (Eigen::Index r = 0; r < inputs.rows(); ++r) {
      const double value = inputs(r, column);
      const auto row = static_cast<std::size_t>(r);
      if (value > 0.0) {
        if (census.positive == 0) {
          census.first_positive = row;
        }
        ++census.positive;
      } else if (value < 0.0) {
        if (census.negative == 0) {
          census.first_negative = row;
        }
        ++census.negative;
      }
    }
    return census;
  }

  std::optional<batch_warning> load_side_warning(const fz_sign_census &census, coordinate_system side) {
    const bool iso = side == coordinate_system::ISO;
    const std::size_t count = iso ? census.negative : census.positive;
    const std::size_t first = iso ? census.first_negative : census.first_positive;
    if (count == 0) {
      return std::nullopt;
    }
    const std::string_view suffix = (census.positive > 0 && census.negative > 0) ? MIXED_SUFFIX : std::string_view{};
    const std::span<const std::string_view> formats{LOAD_SIDE_FORMATS};
    // The message is formatted before any object with a destructor is live, so the call has no cleanup path here.
    std::string message = std::vformat(formats[static_cast<std::size_t>(side)], std::make_format_args(count, suffix));
    batch_warning warn;
    warn.index = first;
    warn.cause.message = std::move(message);
    warn.cause.where.key = "FZ";
    // Moved into a new optional: a returned (elided) local leaves its destructor call off the closing brace, whose
    // only code is then the exception-cleanup resume that no test can execute.
    return std::make_optional(std::move(warn));
  }
} // namespace mf

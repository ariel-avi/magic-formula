#include <Eigen/Core>
#include <cstddef>
#include <expected>
#include <format>
#include <magic_formula/error.hpp>
#include <magic_formula/model_layout.hpp>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace mf {
  namespace {
    /// Location that names only a key. Built through a named local and a move so that the `make_error` call sites
    /// carry no exception-cleanup branches of their own.
    location key_location(std::string_view key) {
      std::string text(key);
      return location{.file = {}, .line = 0, .key = std::move(text)};
    }

    /// Negates every column of `values` whose channel's `negated_by` is in `transforms` (exact sign flips).
    // NOLINTBEGIN(performance-unnecessary-value-param): a writable Eigen::Ref is taken by value so that it binds Map
    // temporaries (the Eigen idiom); a const reference to it would not bind a writable parameter.
    template <typename Channel_T>
    void negate_columns(std::span<const Channel_T> channels, const sign_transforms &transforms,
                        Eigen::Ref<Eigen::MatrixXd> values) noexcept {
      for (std::size_t i = 0; i < channels.size(); ++i) {
        if (contains(transforms, channels[i].negated_by)) {
          const auto column = static_cast<Eigen::Index>(i);
          values.col(column) = -values.col(column);
        }
      }
    }
    // NOLINTEND(performance-unnecessary-value-param)
  } // namespace

  // NOLINTBEGIN(performance-unnecessary-value-param): writable Eigen::Ref by value, see negate_columns.
  void apply_sign_transforms(std::span<const input_channel> channels, const sign_transforms &transforms,
                             Eigen::Ref<Eigen::MatrixXd> values) noexcept {
    negate_columns(channels, transforms, values);
  }

  void apply_sign_transforms(std::span<const output_channel> channels, const sign_transforms &transforms,
                             Eigen::Ref<Eigen::MatrixXd> values) noexcept {
    negate_columns(channels, transforms, values);
  }
  // NOLINTEND(performance-unnecessary-value-param)

  Eigen::VectorXd transform_limits(const Eigen::Ref<const Eigen::VectorXd> &limits,
                                   std::span<const limit_channel> channels, const sign_transforms &transforms) {
    Eigen::VectorXd result = limits;
    for (std::size_t i = 0; i < channels.size(); ++i) {
      if (channels[i].role == limit_role::LOWER && contains(transforms, channels[i].negated_by)) {
        const auto lower = static_cast<Eigen::Index>(i);
        result(lower) = -limits(lower + 1);
        result(lower + 1) = -limits(lower);
      }
    }
    // Returned through a move into a prvalue: a returned local would add an exception-cleanup-only line.
    return Eigen::VectorXd(std::move(result)); // NOLINT(modernize-return-braced-init-list): see default_limits
  }

  Eigen::VectorXd default_limits(std::span<const limit_channel> channels) {
    Eigen::VectorXd values(static_cast<Eigen::Index>(channels.size()));
    for (std::size_t i = 0; i < channels.size(); ++i) {
      values(static_cast<Eigen::Index>(i)) = channels[i].default_value;
    }
    // Returned through a move into a prvalue: a returned local would add an exception-cleanup-only line.
    // The braced form would trip -Wpessimizing-move, so the tidy check is suppressed here.
    return Eigen::VectorXd(std::move(values)); // NOLINT(modernize-return-braced-init-list): see above
  }

  std::expected<void, error> validate_limits(const Eigen::Ref<const Eigen::VectorXd> &limits,
                                             std::span<const limit_channel> channels) {
    if (limits.size() != static_cast<Eigen::Index>(channels.size())) {
      return make_error(error_code::INVALID_ARGUMENT,
                        std::format("limits vector has {} values, the layout has {}", limits.size(), channels.size()));
    }
    if (!limit_pairs_well_formed(channels)) {
      return make_error(error_code::INVALID_ARGUMENT,
                        "malformed limit layout: every LOWER must be directly followed by its UPPER");
    }
    for (std::size_t i = 0; i < channels.size(); ++i) {
      if (channels[i].role == limit_role::LOWER) {
        const double lower = limits(static_cast<Eigen::Index>(i));
        const double upper = limits(static_cast<Eigen::Index>(i + 1));
        if (!(lower <= upper)) {
          return make_error(error_code::INVALID_VALUE,
                            std::format("{} {} > {} {}: lower limit above upper limit", channels[i].key, lower,
                                        channels[i + 1].key, upper),
                            key_location(channels[i].key));
        }
      }
    }
    return {};
  }
} // namespace mf

#pragma once

#include <Eigen/Core>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <magic_formula/error.hpp>
#include <magic_formula/sign_transform.hpp>
#include <optional>
#include <span>
#include <string_view>

namespace mf {
  /// One position of a model's input vector.
  struct input_channel {
    std::string_view name; ///< upper-case; also the input-table column ("FZ")
    std::string_view unit; ///< SI unit printed in the CSV header ("N", "rad", "-", "m/s", "Pa")
    double minimum = -std::numeric_limits<double>::infinity(); ///< lower end of the model domain
    bool minimum_inclusive = true;                             ///< true: value >= minimum; false: value > minimum
    /// The transform that negates this input (MF 5.x: FZ VERTICAL, SA LATERAL).
    sign_transform negated_by = sign_transform::NONE;
  };

  /// One position of a model's output vector. Flags use unit "-" and hold 0.0 or 1.0.
  struct output_channel {
    std::string_view name;
    std::string_view unit;
    /// The transform that negates this output (MF 5.x: FY, MZ and MY VERTICAL).
    sign_transform negated_by = sign_transform::NONE;
  };

  /// Role of a limits-vector position. LOWER is immediately followed by its UPPER.
  enum class limit_role : std::uint8_t {
    LOWER, ///< lower end of a validity range
    UPPER, ///< upper end of a validity range
    VALUE, ///< a single value (for example VXLOW)
  };

  /// One position of a model's limits vector.
  struct limit_channel {
    std::string_view key; ///< .tir key ("FZMIN")
    limit_role role = limit_role::VALUE;
    double default_value = 0.0; ///< -inf for LOWER, +inf for UPPER, e.g. 1.0 for VXLOW
    /// For a LOWER/UPPER pair, the transform under which the pair becomes (-upper, -lower); both rows carry the same
    /// value. NONE for VALUE rows.
    sign_transform negated_by = sign_transform::NONE;
  };

  /// The index map of a model: element i of each span describes position i of the matching vector.
  struct model_layout {
    std::span<const input_channel> inputs;
    std::span<const output_channel> outputs;
    std::span<const limit_channel> limits;
  };

  /// Position of the first channel whose name equals `name` (exact, case-sensitive comparison), or empty.
  template <typename Channel_T>
  [[nodiscard]] constexpr std::optional<std::size_t> find_channel(std::span<const Channel_T> channels,
                                                                  std::string_view name) noexcept {
    for (std::size_t i = 0; i < channels.size(); ++i) {
      if (channels[i].name == name) {
        return i;
      }
    }
    return std::nullopt;
  }

  /// Every LOWER is directly followed by an UPPER and every UPPER directly follows a LOWER. An empty span is well
  /// formed.
  [[nodiscard]] constexpr bool limit_pairs_well_formed(std::span<const limit_channel> channels) noexcept {
    for (std::size_t i = 0; i < channels.size(); ++i) {
      if (channels[i].role == limit_role::LOWER) {
        if (i + 1 == channels.size()) {
          return false;
        }
        if (channels[i + 1].role != limit_role::UPPER) {
          return false;
        }
      }
      if (channels[i].role == limit_role::UPPER) {
        if (i == 0) {
          return false;
        }
        if (channels[i - 1].role != limit_role::LOWER) {
          return false;
        }
      }
    }
    return true;
  }

  /// Every LOWER row and its UPPER row carry the same negated_by; every VALUE row carries NONE.
  [[nodiscard]] constexpr bool limit_signs_well_formed(std::span<const limit_channel> channels) noexcept {
    for (std::size_t i = 0; i < channels.size(); ++i) {
      if (channels[i].role == limit_role::LOWER) {
        if (i + 1 == channels.size()) {
          return false;
        }
        if (channels[i + 1].negated_by != channels[i].negated_by) {
          return false;
        }
      }
      if (channels[i].role == limit_role::VALUE) {
        if (channels[i].negated_by != sign_transform::NONE) {
          return false;
        }
      }
    }
    return true;
  }

  /// Negates in place every column i whose channels[i].negated_by is in `transforms`. Exact sign flips: NaN stays NaN,
  /// +0.0 becomes -0.0. Precondition: values.cols() == channels.size().
  void apply_sign_transforms(std::span<const input_channel> channels, const sign_transforms &transforms,
                             Eigen::Ref<Eigen::MatrixXd> values) noexcept;

  /// The same for the columns of an output matrix.
  void apply_sign_transforms(std::span<const output_channel> channels, const sign_transforms &transforms,
                             Eigen::Ref<Eigen::MatrixXd> values) noexcept;

  /// The limits after `transforms`: every pair whose LOWER channel's negated_by is in `transforms` becomes
  /// (-upper, -lower) (infinite bounds stay infinite and ordered); every other position is copied. Exact; its own
  /// inverse. Preconditions: limits.size() == channels.size(); limit_pairs_well_formed(channels).
  [[nodiscard]] Eigen::VectorXd transform_limits(const Eigen::Ref<const Eigen::VectorXd> &limits,
                                                 std::span<const limit_channel> channels,
                                                 const sign_transforms &transforms);

  /// Vector of the channels' default values, in channel order.
  [[nodiscard]] Eigen::VectorXd default_limits(std::span<const limit_channel> channels);

  /// Checks a limits vector against its channels. INVALID_ARGUMENT for a size that differs from the channel count
  /// or for malformed LOWER/UPPER pairs; then INVALID_VALUE (key = the LOWER key) for the first pair, in index order,
  /// that does not satisfy lower <= upper (a NaN bound fails). Equal bounds and infinite bounds are valid.
  [[nodiscard]] std::expected<void, error> validate_limits(const Eigen::Ref<const Eigen::VectorXd> &limits,
                                                           std::span<const limit_channel> channels);
} // namespace mf

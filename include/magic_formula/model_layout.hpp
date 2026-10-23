#pragma once

#include <Eigen/Core>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <magic_formula/error.hpp>
#include <span>
#include <string_view>

namespace mf {
  /// One position of a model's input vector.
  struct input_channel {
    std::string_view name; ///< upper-case; also the input-table column ("FZ")
    std::string_view unit; ///< SI unit printed in the CSV header ("N", "rad", "-", "m/s", "Pa")
    double minimum = -std::numeric_limits<double>::infinity(); ///< lower end of the model domain
    bool minimum_inclusive = true;                             ///< true: value >= minimum; false: value > minimum
  };

  /// One position of a model's output vector. Flags use unit "-" and hold 0.0 or 1.0.
  struct output_channel {
    std::string_view name;
    std::string_view unit;
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
  };

  /// The index map of a model: element i of each span describes position i of the matching vector.
  struct model_layout {
    std::span<const input_channel> inputs;
    std::span<const output_channel> outputs;
    std::span<const limit_channel> limits;
  };

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

  /// Vector of the channels' default values, in channel order.
  [[nodiscard]] Eigen::VectorXd default_limits(std::span<const limit_channel> channels);

  /// Checks a limits vector against its channels. INVALID_ARGUMENT for a size that differs from the channel count
  /// or for malformed LOWER/UPPER pairs; then INVALID_VALUE (key = the LOWER key) for the first pair, in index order,
  /// that does not satisfy lower <= upper (a NaN bound fails). Equal bounds and infinite bounds are valid.
  [[nodiscard]] std::expected<void, error> validate_limits(const Eigen::Ref<const Eigen::VectorXd> &limits,
                                                           std::span<const limit_channel> channels);
} // namespace mf

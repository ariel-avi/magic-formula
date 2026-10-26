#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace mf {
  /// An elementary, exact sign transform between tire coordinate conventions (coordinate-systems.md). Every input,
  /// output, limit pair and coefficient declares the one transform that negates it. Both transforms are their own
  /// inverse and commute.
  enum class sign_transform : std::uint8_t {
    NONE,     ///< never negated
    VERTICAL, ///< ISO <-> SAE: FZ -> -FZ; FY, MZ, MY negated; SA, SR, IA, VX, FX, MX and flags unchanged
    LATERAL,  ///< slip-angle convention: SA -> -SA; every output unchanged
  };

  /// A conversion plan: which transforms to apply. Both false: identity.
  struct sign_transforms {
    bool vertical = false;
    bool lateral = false;
  };

  /// True when `set` contains `transform` (never for NONE).
  [[nodiscard]] constexpr bool contains(const sign_transforms &set, sign_transform transform) noexcept {
    const std::array<bool, 3> members{false, set.vertical, set.lateral};
    return std::span<const bool>{members}[static_cast<std::size_t>(transform)];
  }

  /// True when `plan` negates a value whose negating transforms are `negators`: an odd number of the transforms in
  /// `negators` is in `plan` (each one negates the value; VERTICAL and LATERAL together cancel).
  [[nodiscard]] constexpr bool negates(const sign_transforms &negators, const sign_transforms &plan) noexcept {
    return (negators.vertical && plan.vertical) != (negators.lateral && plan.lateral);
  }

  /// "NONE", "VERTICAL", "LATERAL".
  [[nodiscard]] constexpr std::string_view sign_transform_name(sign_transform transform) noexcept {
    constexpr std::array<std::string_view, 3> names{"NONE", "VERTICAL", "LATERAL"};
    return std::span<const std::string_view>{names}[static_cast<std::size_t>(transform)];
  }
} // namespace mf

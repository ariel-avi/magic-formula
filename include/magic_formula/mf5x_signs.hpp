#pragma once

#include <array>
#include <cstddef>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/sign_transform.hpp>
#include <span>
#include <string_view>

namespace mf::mf5x {
  /// One coefficient that changes sign under a transform. `key` equals the member name and the .tir key.
  struct coefficient_sign {
    std::string_view key;
    double mf5x_coefficients::*member;
    sign_transform negated_by; ///< VERTICAL or LATERAL
  };

  /// The canonical sign table of PAC2002 and MF 5.2 (coordinate-systems.md). Every coefficient not listed, including
  /// every scaling factor L*, is unchanged.
  /// VERTICAL (15): FNOMIN, PDX1, PDX2, PKX1, PKX2, PVX1, PVX2, QSX1, QSX2, QSX3, QSY2, SSZ1, SSZ2, SSZ3, SSZ4.
  /// LATERAL (15): PKY1, PHY1, PHY2, PHY3, PEY3, PEY4, QHZ1, QHZ2, QHZ3, QHZ4, QEZ4, QEZ5, QBZ10, RBY3, RHX1.
  // clang-format off
  inline constexpr std::array<coefficient_sign, 30> COEFFICIENT_SIGNS{{
      {.key = "FNOMIN", .member = &mf5x_coefficients::FNOMIN, .negated_by = sign_transform::VERTICAL},
      {.key = "PDX1", .member = &mf5x_coefficients::PDX1, .negated_by = sign_transform::VERTICAL},
      {.key = "PDX2", .member = &mf5x_coefficients::PDX2, .negated_by = sign_transform::VERTICAL},
      {.key = "PKX1", .member = &mf5x_coefficients::PKX1, .negated_by = sign_transform::VERTICAL},
      {.key = "PKX2", .member = &mf5x_coefficients::PKX2, .negated_by = sign_transform::VERTICAL},
      {.key = "PVX1", .member = &mf5x_coefficients::PVX1, .negated_by = sign_transform::VERTICAL},
      {.key = "PVX2", .member = &mf5x_coefficients::PVX2, .negated_by = sign_transform::VERTICAL},
      {.key = "QSX1", .member = &mf5x_coefficients::QSX1, .negated_by = sign_transform::VERTICAL},
      {.key = "QSX2", .member = &mf5x_coefficients::QSX2, .negated_by = sign_transform::VERTICAL},
      {.key = "QSX3", .member = &mf5x_coefficients::QSX3, .negated_by = sign_transform::VERTICAL},
      {.key = "QSY2", .member = &mf5x_coefficients::QSY2, .negated_by = sign_transform::VERTICAL},
      {.key = "SSZ1", .member = &mf5x_coefficients::SSZ1, .negated_by = sign_transform::VERTICAL},
      {.key = "SSZ2", .member = &mf5x_coefficients::SSZ2, .negated_by = sign_transform::VERTICAL},
      {.key = "SSZ3", .member = &mf5x_coefficients::SSZ3, .negated_by = sign_transform::VERTICAL},
      {.key = "SSZ4", .member = &mf5x_coefficients::SSZ4, .negated_by = sign_transform::VERTICAL},
      {.key = "PKY1", .member = &mf5x_coefficients::PKY1, .negated_by = sign_transform::LATERAL},
      {.key = "PHY1", .member = &mf5x_coefficients::PHY1, .negated_by = sign_transform::LATERAL},
      {.key = "PHY2", .member = &mf5x_coefficients::PHY2, .negated_by = sign_transform::LATERAL},
      {.key = "PHY3", .member = &mf5x_coefficients::PHY3, .negated_by = sign_transform::LATERAL},
      {.key = "PEY3", .member = &mf5x_coefficients::PEY3, .negated_by = sign_transform::LATERAL},
      {.key = "PEY4", .member = &mf5x_coefficients::PEY4, .negated_by = sign_transform::LATERAL},
      {.key = "QHZ1", .member = &mf5x_coefficients::QHZ1, .negated_by = sign_transform::LATERAL},
      {.key = "QHZ2", .member = &mf5x_coefficients::QHZ2, .negated_by = sign_transform::LATERAL},
      {.key = "QHZ3", .member = &mf5x_coefficients::QHZ3, .negated_by = sign_transform::LATERAL},
      {.key = "QHZ4", .member = &mf5x_coefficients::QHZ4, .negated_by = sign_transform::LATERAL},
      {.key = "QEZ4", .member = &mf5x_coefficients::QEZ4, .negated_by = sign_transform::LATERAL},
      {.key = "QEZ5", .member = &mf5x_coefficients::QEZ5, .negated_by = sign_transform::LATERAL},
      {.key = "QBZ10", .member = &mf5x_coefficients::QBZ10, .negated_by = sign_transform::LATERAL},
      {.key = "RBY3", .member = &mf5x_coefficients::RBY3, .negated_by = sign_transform::LATERAL},
      {.key = "RHX1", .member = &mf5x_coefficients::RHX1, .negated_by = sign_transform::LATERAL},
  }};
  // clang-format on

  /// Number of rows whose `negated_by` equals `transform`.
  [[nodiscard]] constexpr std::size_t count_negated_by(std::span<const coefficient_sign> rows,
                                                       sign_transform transform) noexcept {
    std::size_t count = 0;
    for (const coefficient_sign &row : rows) {
      if (row.negated_by == transform) {
        ++count;
      }
    }
    return count;
  }

  /// True when no two rows share a key (quadratic comparison).
  [[nodiscard]] constexpr bool coefficient_keys_unique(std::span<const coefficient_sign> rows) noexcept {
    for (std::size_t i = 0; i < rows.size(); ++i) {
      for (std::size_t j = i + 1; j < rows.size(); ++j) {
        if (rows[i].key == rows[j].key) {
          return false;
        }
      }
    }
    return true;
  }

  static_assert(count_negated_by(COEFFICIENT_SIGNS, sign_transform::VERTICAL) == 15);
  static_assert(count_negated_by(COEFFICIENT_SIGNS, sign_transform::LATERAL) == 15);
  static_assert(count_negated_by(COEFFICIENT_SIGNS, sign_transform::NONE) == 0);
  static_assert(coefficient_keys_unique(COEFFICIENT_SIGNS));

  /// `coefficients` with every COEFFICIENT_SIGNS member whose transform is in `transforms` negated. Exact; its own
  /// inverse; the two transforms commute.
  [[nodiscard]] mf5x_coefficients transform_coefficients(const mf5x_coefficients &coefficients,
                                                         const sign_transforms &transforms) noexcept;
} // namespace mf::mf5x

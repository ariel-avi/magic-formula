#pragma once
#include <array>
#include <magic_formula/mf61_coefficients.hpp>
#include <magic_formula/sign_transform.hpp>
#include <string_view>
namespace mf::mf6x {
  /// A coefficient negated by one algebraic coordinate gauge.
  struct coefficient_sign {
    std::string_view key;
    double mf61_coefficients::*member;
    sign_transform negated_by;
  };
  inline constexpr std::array<coefficient_sign, 35> COEFFICIENT_SIGNS{{
      {.key = "FNOMIN", .member = &mf61_coefficients::FNOMIN, .negated_by = sign_transform::VERTICAL},
      {.key = "VERTICAL_STIFFNESS",
       .member = &mf61_coefficients::VERTICAL_STIFFNESS,
       .negated_by = sign_transform::VERTICAL},
      {.key = "PDX1", .member = &mf61_coefficients::PDX1, .negated_by = sign_transform::VERTICAL},
      {.key = "PDX2", .member = &mf61_coefficients::PDX2, .negated_by = sign_transform::VERTICAL},
      {.key = "PKX1", .member = &mf61_coefficients::PKX1, .negated_by = sign_transform::VERTICAL},
      {.key = "PKX2", .member = &mf61_coefficients::PKX2, .negated_by = sign_transform::VERTICAL},
      {.key = "PVX1", .member = &mf61_coefficients::PVX1, .negated_by = sign_transform::VERTICAL},
      {.key = "PVX2", .member = &mf61_coefficients::PVX2, .negated_by = sign_transform::VERTICAL},
      {.key = "QSX1", .member = &mf61_coefficients::QSX1, .negated_by = sign_transform::VERTICAL},
      {.key = "QSX2", .member = &mf61_coefficients::QSX2, .negated_by = sign_transform::VERTICAL},
      {.key = "QSX3", .member = &mf61_coefficients::QSX3, .negated_by = sign_transform::VERTICAL},
      {.key = "QSX4", .member = &mf61_coefficients::QSX4, .negated_by = sign_transform::VERTICAL},
      {.key = "QSX10", .member = &mf61_coefficients::QSX10, .negated_by = sign_transform::VERTICAL},
      {.key = "QSX12", .member = &mf61_coefficients::QSX12, .negated_by = sign_transform::VERTICAL},
      {.key = "QSX13", .member = &mf61_coefficients::QSX13, .negated_by = sign_transform::VERTICAL},
      {.key = "QSX14", .member = &mf61_coefficients::QSX14, .negated_by = sign_transform::VERTICAL},
      {.key = "QSY2", .member = &mf61_coefficients::QSY2, .negated_by = sign_transform::VERTICAL},
      {.key = "SSZ1", .member = &mf61_coefficients::SSZ1, .negated_by = sign_transform::VERTICAL},
      {.key = "SSZ2", .member = &mf61_coefficients::SSZ2, .negated_by = sign_transform::VERTICAL},
      {.key = "SSZ3", .member = &mf61_coefficients::SSZ3, .negated_by = sign_transform::VERTICAL},
      {.key = "SSZ4", .member = &mf61_coefficients::SSZ4, .negated_by = sign_transform::VERTICAL},
      {.key = "PKY1", .member = &mf61_coefficients::PKY1, .negated_by = sign_transform::LATERAL},
      {.key = "PHY1", .member = &mf61_coefficients::PHY1, .negated_by = sign_transform::LATERAL},
      {.key = "PHY2", .member = &mf61_coefficients::PHY2, .negated_by = sign_transform::LATERAL},
      {.key = "PEY3", .member = &mf61_coefficients::PEY3, .negated_by = sign_transform::LATERAL},
      {.key = "PEY4", .member = &mf61_coefficients::PEY4, .negated_by = sign_transform::LATERAL},
      {.key = "QHZ1", .member = &mf61_coefficients::QHZ1, .negated_by = sign_transform::LATERAL},
      {.key = "QHZ2", .member = &mf61_coefficients::QHZ2, .negated_by = sign_transform::LATERAL},
      {.key = "QHZ3", .member = &mf61_coefficients::QHZ3, .negated_by = sign_transform::LATERAL},
      {.key = "QHZ4", .member = &mf61_coefficients::QHZ4, .negated_by = sign_transform::LATERAL},
      {.key = "QEZ4", .member = &mf61_coefficients::QEZ4, .negated_by = sign_transform::LATERAL},
      {.key = "QEZ5", .member = &mf61_coefficients::QEZ5, .negated_by = sign_transform::LATERAL},
      {.key = "QBZ10", .member = &mf61_coefficients::QBZ10, .negated_by = sign_transform::LATERAL},
      {.key = "RBY3", .member = &mf61_coefficients::RBY3, .negated_by = sign_transform::LATERAL},
      {.key = "RHX1", .member = &mf61_coefficients::RHX1, .negated_by = sign_transform::LATERAL},
  }};
  /// Exact coefficient sign flips; transforms commute and are self-inverse.
  [[nodiscard]] inline mf61_coefficients transform_coefficients(const mf61_coefficients &coefficients,
                                                                const sign_transforms &transforms) noexcept {
    mf61_coefficients result = coefficients;
    for (const coefficient_sign &row : COEFFICIENT_SIGNS) {
      if (contains(transforms, row.negated_by)) {
        result.*row.member = -(result.*row.member);
      }
    }
    return result;
  }
} // namespace mf::mf6x

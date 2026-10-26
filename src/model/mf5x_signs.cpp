#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_signs.hpp>
#include <magic_formula/sign_transform.hpp>

namespace mf::mf5x {
  mf5x_coefficients transform_coefficients(const mf5x_coefficients &coefficients,
                                           const sign_transforms &transforms) noexcept {
    mf5x_coefficients result = coefficients;
    for (const auto &row : COEFFICIENT_SIGNS) {
      if (contains(transforms, row.negated_by)) {
        result.*row.member = -(result.*row.member);
      }
    }
    return result;
  }
} // namespace mf::mf5x

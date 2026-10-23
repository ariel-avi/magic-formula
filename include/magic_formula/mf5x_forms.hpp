#pragma once

#include <cstdint>

namespace mf {
  /// How the combined forces are computed.
  enum class combined_method : std::uint8_t {
    COSINE,          ///< weighting functions Gxa and Gyk plus SVyk (R* keys present)
    FRICTION_ELLIPSE ///< friction ellipse with SVyk = 0 (no R* keys)
  };

  /// Cosine factor of the pneumatic trail t.
  enum class trail_cosine_form : std::uint8_t {
    COS_ALPHA, ///< cos(alpha)
    COS_PRIME  ///< cos'(alpha) = Vx/(hypot(Vx, tan(alpha)*Vx) + eps)
  };

  /// The force F'y that multiplies the trail in Mz.
  enum class fy_prime_form : std::uint8_t {
    SVYK, ///< F'y = Fy - SVyk
    SVYK0 ///< F'y = Fy - SVyk0, with mu_y at gamma = 0
  };

  /// Output rolling-resistance moment My.
  enum class rolling_resistance_form : std::uint8_t {
    QSY,   ///< from the QSY coefficients (`detail::my`)
    SHIFTS ///< from the longitudinal shifts (`detail::my_from_shifts`)
  };

  /// The closed set of PAC2002 / MF 5.2 formulation differences, chosen at construction.
  struct mf5x_forms {
    trail_cosine_form trail_cosine = trail_cosine_form::COS_ALPHA;
    combined_method combined = combined_method::COSINE;
    fy_prime_form fy_prime = fy_prime_form::SVYK;
    rolling_resistance_form rolling_resistance = rolling_resistance_form::QSY;
  };
} // namespace mf

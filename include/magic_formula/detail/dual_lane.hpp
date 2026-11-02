#pragma once

#include <Eigen/Core>
#include <magic_formula/detail/lanes.hpp>

/// First-order forward-mode lane for exact derivatives (internal, not API). Every operation applies its exact
/// derivative rule (chain rule), so a lane-generic kernel instantiated on `dual_lane` returns the analytic derivative
/// of its closed-form result: there is no step size and no finite difference. The value
/// lane of every operation equals the plain-lane expression bit for bit.
namespace mf::detail {
  /// Value f and derivative df/ds, element-wise, for one seeded input s. The value lane is computed with exactly the
  /// plain-lane expression of each operation.
  template <typename Lane_T> struct dual_lane {
    // NOLINTBEGIN(readability-identifier-naming): Eigen lane-trait names, required by mask_of
    static constexpr int RowsAtCompileTime = Lane_T::RowsAtCompileTime;
    static constexpr int MaxRowsAtCompileTime = Lane_T::MaxRowsAtCompileTime;
    // NOLINTEND(readability-identifier-naming)

    /// f.
    Lane_T value;
    /// df/ds.
    Lane_T derivative;

    // NOLINTBEGIN(readability-identifier-naming): mirrors the Eigen factory names the kernels call
    /// (x, 0) for `n` elements.
    [[nodiscard]] static dual_lane Constant(Eigen::Index n, double x) noexcept {
      dual_lane result;
      result.value = Lane_T::Constant(n, x);
      result.derivative = Lane_T::Zero(n);
      return result;
    }

    /// (0, 0) for `n` elements.
    [[nodiscard]] static dual_lane Zero(Eigen::Index n) noexcept {
      dual_lane result;
      result.value = Lane_T::Zero(n);
      result.derivative = Lane_T::Zero(n);
      return result;
    }
    // NOLINTEND(readability-identifier-naming)

    /// Number of elements.
    [[nodiscard]] Eigen::Index size() const noexcept {
      return value.size();
    }

    /// (atan v, v'/(1 + v^2)).
    [[nodiscard]] dual_lane atan() const noexcept {
      const Lane_T v = value.atan();
      const Lane_T d = derivative / (1.0 + (value * value));
      return dual_lane{.value = v, .derivative = d};
    }

    /// (sin v, v' cos v).
    [[nodiscard]] dual_lane sin() const noexcept {
      const Lane_T v = value.sin();
      const Lane_T d = derivative * value.cos();
      return dual_lane{.value = v, .derivative = d};
    }

    /// (cos v, -v' sin v).
    [[nodiscard]] dual_lane cos() const noexcept {
      const Lane_T v = value.cos();
      const Lane_T d = -(derivative * value.sin());
      return dual_lane{.value = v, .derivative = d};
    }

    /// (e^v, v' e^v).
    [[nodiscard]] dual_lane exp() const noexcept {
      const Lane_T e = value.exp();
      const Lane_T d = derivative * e;
      return dual_lane{.value = e, .derivative = d};
    }

    /// (sqrt v, v'/(2 sqrt v)).
    [[nodiscard]] dual_lane sqrt() const noexcept {
      const Lane_T r = value.sqrt();
      const Lane_T d = derivative / (2.0 * r);
      return dual_lane{.value = r, .derivative = d};
    }

    /// (|v|, sgn+(v) v'): the right derivative at v = +-0.
    [[nodiscard]] dual_lane abs() const noexcept {
      const Lane_T v = value.abs();
      const Lane_T d = sgn_plus(value) * derivative;
      return dual_lane{.value = v, .derivative = d};
    }

    /// (min(v, bound), v' where v < bound, else 0).
    [[nodiscard]] dual_lane min(double bound) const noexcept {
      const Lane_T v = value.min(bound);
      const mask_of<Lane_T> below = value < bound;
      const Lane_T zero = Lane_T::Zero(value.size());
      const Lane_T d = below.select(derivative, zero);
      return dual_lane{.value = v, .derivative = d};
    }

    /// -a, with the value and derivative both negated.
    [[nodiscard]] friend dual_lane operator-(const dual_lane &a) noexcept {
      const Lane_T v = -a.value;
      const Lane_T d = -a.derivative;
      return dual_lane{.value = v, .derivative = d};
    }

    /// a + b.
    [[nodiscard]] friend dual_lane operator+(const dual_lane &a, const dual_lane &b) noexcept {
      const Lane_T v = a.value + b.value;
      const Lane_T d = a.derivative + b.derivative;
      return dual_lane{.value = v, .derivative = d};
    }

    /// a - b.
    [[nodiscard]] friend dual_lane operator-(const dual_lane &a, const dual_lane &b) noexcept {
      const Lane_T v = a.value - b.value;
      const Lane_T d = a.derivative - b.derivative;
      return dual_lane{.value = v, .derivative = d};
    }

    /// a * b.
    [[nodiscard]] friend dual_lane operator*(const dual_lane &a, const dual_lane &b) noexcept {
      const Lane_T v = a.value * b.value;
      const Lane_T d = (a.derivative * b.value) + (a.value * b.derivative);
      return dual_lane{.value = v, .derivative = d};
    }

    /// a / b.
    [[nodiscard]] friend dual_lane operator/(const dual_lane &a, const dual_lane &b) noexcept {
      const Lane_T v = a.value / b.value;
      const Lane_T d = ((a.derivative * b.value) - (a.value * b.derivative)) / (b.value * b.value);
      return dual_lane{.value = v, .derivative = d};
    }

    /// a + x.
    [[nodiscard]] friend dual_lane operator+(const dual_lane &a, double x) noexcept {
      const Lane_T v = a.value + x;
      return dual_lane{.value = v, .derivative = a.derivative};
    }

    /// x + a.
    [[nodiscard]] friend dual_lane operator+(double x, const dual_lane &a) noexcept {
      const Lane_T v = x + a.value;
      return dual_lane{.value = v, .derivative = a.derivative};
    }

    /// a - x.
    [[nodiscard]] friend dual_lane operator-(const dual_lane &a, double x) noexcept {
      const Lane_T v = a.value - x;
      return dual_lane{.value = v, .derivative = a.derivative};
    }

    /// x - a.
    [[nodiscard]] friend dual_lane operator-(double x, const dual_lane &a) noexcept {
      const Lane_T v = x - a.value;
      const Lane_T d = -a.derivative;
      return dual_lane{.value = v, .derivative = d};
    }

    /// a * x.
    [[nodiscard]] friend dual_lane operator*(const dual_lane &a, double x) noexcept {
      const Lane_T v = a.value * x;
      const Lane_T d = a.derivative * x;
      return dual_lane{.value = v, .derivative = d};
    }

    /// x * a.
    [[nodiscard]] friend dual_lane operator*(double x, const dual_lane &a) noexcept {
      const Lane_T v = x * a.value;
      const Lane_T d = x * a.derivative;
      return dual_lane{.value = v, .derivative = d};
    }

    /// a / x.
    [[nodiscard]] friend dual_lane operator/(const dual_lane &a, double x) noexcept {
      const Lane_T v = a.value / x;
      const Lane_T d = a.derivative / x;
      return dual_lane{.value = v, .derivative = d};
    }

    /// Mask of a.value >= x.
    [[nodiscard]] friend mask_of<Lane_T> operator>=(const dual_lane &a, double x) noexcept {
      const mask_of<Lane_T> mask = a.value >= x;
      return mask;
    }

    /// Mask of a.value > x.
    [[nodiscard]] friend mask_of<Lane_T> operator>(const dual_lane &a, double x) noexcept {
      const mask_of<Lane_T> mask = a.value > x;
      return mask;
    }
  };

  /// Element-wise select of value and derivative; more specialised than the generic `select_lanes`, found by ADL.
  template <typename Mask_T, typename Lane_T>
  [[nodiscard]] dual_lane<Lane_T> select_lanes(const Mask_T &mask, const dual_lane<Lane_T> &if_true,
                                               const dual_lane<Lane_T> &if_false) noexcept {
    const Lane_T v = mask.select(if_true.value, if_false.value);
    const Lane_T d = mask.select(if_true.derivative, if_false.derivative);
    return dual_lane<Lane_T>{.value = v, .derivative = d};
  }

  /// sgn+ of the value lane; derivative 0 (sgn+ is piecewise constant).
  template <typename Lane_T> [[nodiscard]] dual_lane<Lane_T> sgn_plus(const dual_lane<Lane_T> &x) noexcept {
    const Lane_T v = sgn_plus(x.value);
    const Lane_T d = Lane_T::Zero(x.size());
    return dual_lane<Lane_T>{.value = v, .derivative = d};
  }

  /// (value, derivative).
  template <typename Lane_T>
  [[nodiscard]] dual_lane<Lane_T> make_dual(const Lane_T &value, const Lane_T &derivative) noexcept {
    return dual_lane<Lane_T>{.value = value, .derivative = derivative};
  }
} // namespace mf::detail

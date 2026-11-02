#pragma once

#include <Eigen/Core>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <magic_formula/detail/dual_lane.hpp>
#include <magic_formula/detail/lanes.hpp>
#include <magic_formula/detail/mf61_equations.hpp>
#include <magic_formula/detail/stationary_point.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf61_coefficients.hpp>
#include <magic_formula/mf6x_layout.hpp>
#include <numbers>
#include <span>

namespace mf::detail {
  /// One lane per named MF 6.1 input channel.
  template <typename Lane_T> using mf61_inputs = std::array<Lane_T, mf6x::INPUT_LENGTH>;
  /// A conditioned point, its contact mask and the clamping flags.
  template <typename Lane_T> struct mf61_conditioned {
    mf61_inputs<Lane_T> point;
    std::array<Lane_T, 6> moved;
    mask_of<Lane_T> contact;
  };
  /// Applies validity ranges and substitutes safe nominal values to the equation lanes without contact.
  template <typename Lane_T>
  [[nodiscard]] mf61_conditioned<Lane_T> mf61_condition(const mf61_coefficients &c,
                                                        const Eigen::Ref<const Eigen::VectorXd> &limits,
                                                        const mf61_inputs<Lane_T> &raw) noexcept {
    mf61_conditioned<Lane_T> out;
    out.point = raw;
    const Eigen::Index count = raw[mf6x::FZ].size();
    const Lane_T zero = Lane_T::Zero(count);
    const double side = std::copysign(1.0, c.FNOMIN);
    const mask_of<Lane_T> raw_contact = raw[mf6x::FZ] * side > 0.0;
    constexpr std::array<int, 5> inputs{mf6x::FZ, mf6x::SR, mf6x::SA, mf6x::IA, mf6x::P};
    constexpr std::array<int, 5> lower{mf6x::FZMIN, mf6x::KPUMIN, mf6x::ALPMIN, mf6x::CAMMIN, mf6x::PRESMIN};
    const std::span<const int> input_channels{inputs};
    const std::span<const int> lower_limits{lower};
    const std::span<const Lane_T> raw_values{raw};
    std::span<Lane_T> conditioned_values{out.point};
    std::span<Lane_T> moved{out.moved};
    for (std::size_t i = 0; i < input_channels.size(); ++i) {
      const auto channel = static_cast<std::size_t>(input_channels[i]);
      conditioned_values[channel] =
          clamp_to_limits(raw_values[channel], limits(lower_limits[i]), limits(lower_limits[i] + 1), moved[i]);
      moved[i] = raw_contact.select(moved[i], zero);
    }
    out.moved[5] = to_flag<Lane_T>(raw[mf6x::VX] < limits(mf6x::VXLOW));
    out.contact = raw_contact && (out.point[mf6x::FZ] * side > 0.0);
    const Lane_T nominal_load = Lane_T::Constant(count, c.FNOMIN * c.LFZ0);
    out.point[mf6x::FZ] = out.contact.select(out.point[mf6x::FZ], nominal_load);
    return out;
  }
  /// Planar forces and their pure-slip peak factors.
  template <typename Lane_T> struct mf61_planar_forces {
    Lane_T fx;
    Lane_T fy;
    Lane_T dx;
    Lane_T dy;
  };
  /// The selected pure or combined force equations, shared by evaluation and analytic derivatives.
  template <typename Lane_T>
  [[nodiscard]] mf61_planar_forces<Lane_T> mf61_planar(const mf61_coefficients &c, slip_mode mode, bool turn_slip,
                                                       const mf61_inputs<Lane_T> &p) noexcept {
    const mf61_equations<Lane_T> equations(c, p[mf6x::FZ].size(), turn_slip);
    const Lane_T zero = Lane_T::Zero(p[mf6x::FZ].size());
    mf61_planar_forces<Lane_T> out;
    if (mode == slip_mode::COMBINED) {
      out.fx = equations.fx(p[mf6x::SA], p[mf6x::SR], p[mf6x::IA], p[mf6x::FZ], p[mf6x::P], p[mf6x::VX], p[mf6x::TS]);
      out.fy = equations.fy(p[mf6x::SA], p[mf6x::SR], p[mf6x::IA], p[mf6x::FZ], p[mf6x::P], p[mf6x::VX], p[mf6x::TS]);
    } else {
      out.fx = equations.fx_pure(p[mf6x::SR], p[mf6x::IA], p[mf6x::FZ], p[mf6x::P], p[mf6x::VX], p[mf6x::TS]);
      out.fy = equations.fy_pure(p[mf6x::SA], p[mf6x::IA], p[mf6x::FZ], p[mf6x::P], p[mf6x::VX], p[mf6x::TS]);
    }
    const Lane_T dfz = equations.dfz(p[mf6x::FZ]);
    const Lane_T dpi = equations.dpi(p[mf6x::P]);
    const Lane_T phi_x =
        equations.phi(zero, p[mf6x::SR], p[mf6x::FZ], p[mf6x::IA], p[mf6x::VX], p[mf6x::P], p[mf6x::TS]);
    const Lane_T phi_y =
        equations.phi(p[mf6x::SA], zero, p[mf6x::FZ], p[mf6x::IA], p[mf6x::VX], p[mf6x::P], p[mf6x::TS]);
    const Lane_T mux = equations.mux(dfz, p[mf6x::IA], dpi, equations.lmux_star(zero, p[mf6x::SR], p[mf6x::VX]));
    const Lane_T muy = equations.muy(dfz, p[mf6x::IA], dpi, equations.lmuy_star(p[mf6x::SA], zero, p[mf6x::VX]));
    out.dx = equations.dx(mux, p[mf6x::FZ], p[mf6x::SR], phi_x);
    out.dy = equations.dy(p[mf6x::FZ], muy, p[mf6x::SA], phi_y);
    return out;
  }
  /// Full point or block evaluation; flags retain their raw-input meaning.
  template <typename Lane_T>
  [[nodiscard]] std::array<Lane_T, mf6x::OUTPUT_LENGTH>
  mf61_pipeline(const mf61_coefficients &c, const model_options &options,
                const Eigen::Ref<const Eigen::VectorXd> &limits, const mf61_inputs<Lane_T> &raw) noexcept {
    const mf61_conditioned<Lane_T> conditioned = mf61_condition(c, limits, raw);
    const mf61_inputs<Lane_T> &p = conditioned.point;
    const mf61_equations<Lane_T> equations(c, p[mf6x::FZ].size(), options.turn_slip);
    const mf61_planar_forces<Lane_T> planar = mf61_planar(c, options.mode, options.turn_slip, p);
    const Lane_T zero = Lane_T::Zero(p[mf6x::FZ].size());
    std::array<Lane_T, mf6x::OUTPUT_LENGTH> out;
    out[mf6x::FX] = planar.fx;
    out[mf6x::FY] = planar.fy;
    if (options.mode == slip_mode::COMBINED) {
      out[mf6x::MZ] =
          equations.mz(p[mf6x::SA], p[mf6x::IA], p[mf6x::SR], p[mf6x::FZ], p[mf6x::P], p[mf6x::VX], p[mf6x::TS]);
    } else {
      out[mf6x::MZ] = equations.mz_pure(p[mf6x::SA], p[mf6x::IA], p[mf6x::FZ], p[mf6x::P], p[mf6x::VX], p[mf6x::TS]);
    }
    out[mf6x::MX] = equations.mx(p[mf6x::IA], p[mf6x::FZ], p[mf6x::P], planar.fy);
    out[mf6x::MY] = equations.my(p[mf6x::IA], p[mf6x::FZ], p[mf6x::VX], p[mf6x::P], planar.fx);
    out[mf6x::RE] = equations.re(p[mf6x::SR], p[mf6x::FZ], p[mf6x::VX], p[mf6x::P]);
    out[mf6x::RL] =
        equations.rl(p[mf6x::SA], p[mf6x::SR], p[mf6x::IA], p[mf6x::FZ], p[mf6x::P], p[mf6x::VX], out[mf6x::RE]);
    constexpr std::array<int, 7> physical{mf6x::FX, mf6x::FY, mf6x::MZ, mf6x::MX, mf6x::MY, mf6x::RE, mf6x::RL};
    std::span<Lane_T> output_values{out};
    for (const int index : physical) {
      const auto channel = static_cast<std::size_t>(index);
      output_values[channel] = conditioned.contact.select(output_values[channel], zero);
    }
    out[mf6x::FZ_CLAMPED] = conditioned.moved[0];
    out[mf6x::SR_CLAMPED] = conditioned.moved[1];
    out[mf6x::SA_CLAMPED] = conditioned.moved[2];
    out[mf6x::IA_CLAMPED] = conditioned.moved[3];
    out[mf6x::P_CLAMPED] = conditioned.moved[4];
    out[mf6x::BELOW_VXLOW] = conditioned.moved[5];
    return out;
  }
  /// Seeds one slip input, keeping other operating conditions constant.
  template <typename Lane_T>
  [[nodiscard]] mf61_inputs<dual_lane<Lane_T>> mf61_seed(const mf61_inputs<Lane_T> &p, std::size_t channel,
                                                         const Lane_T &value, const Lane_T &seed) noexcept {
    mf61_inputs<dual_lane<Lane_T>> out;
    const Lane_T zero = Lane_T::Zero(value.size());
    std::ranges::transform(p, out.begin(), [&zero](const Lane_T &input) noexcept { return make_dual(input, zero); });
    std::span<dual_lane<Lane_T>> seeded{out};
    seeded[channel] = make_dual(value, seed);
    return out;
  }
  /// First peak search extent on the side of the raw slip.
  template <typename Lane_T>
  [[nodiscard]] Lane_T mf61_peak_end(const Lane_T &side, double lower, double upper, double extent) noexcept {
    const Lane_T high = Lane_T::Constant(side.size(), upper);
    const Lane_T low = Lane_T::Constant(side.size(), lower);
    const Lane_T bound = (side > 0.0).select(high, low);
    const Lane_T reach = (side * bound).max(0.0).min(extent);
    return side * reach;
  }
  /// Analytic tangent and secant stiffness, peak factors and first stationary slips.
  template <typename Lane_T>
  [[nodiscard]] std::array<Lane_T, mf6x::CHARACTERISTICS_LENGTH>
  mf61_characteristics(const mf61_coefficients &c, const model_options &options,
                       const Eigen::Ref<const Eigen::VectorXd> &limits, const mf61_inputs<Lane_T> &raw) noexcept {
    const mf61_conditioned<Lane_T> conditioned = mf61_condition(c, limits, raw);
    const mf61_inputs<Lane_T> &p = conditioned.point;
    const Eigen::Index count = raw[mf6x::FZ].size();
    const Lane_T zero = Lane_T::Zero(count);
    const Lane_T one = Lane_T::Constant(count, 1.0);
    const Lane_T nan = Lane_T::Constant(count, std::numeric_limits<double>::quiet_NaN());
    const mf61_planar_forces<Lane_T> at = mf61_planar(c, options.mode, options.turn_slip, p);
    const Lane_T alpha_seed = one - conditioned.moved[2];
    const Lane_T kappa_seed = one - conditioned.moved[1];
    const mf61_planar_forces<dual_lane<Lane_T>> lateral =
        mf61_planar(c, options.mode, options.turn_slip, mf61_seed(p, mf6x::SA, p[mf6x::SA], alpha_seed));
    const mf61_planar_forces<dual_lane<Lane_T>> longitudinal =
        mf61_planar(c, options.mode, options.turn_slip, mf61_seed(p, mf6x::SR, p[mf6x::SR], kappa_seed));
    const Lane_T dy = lateral.fy.derivative;
    const Lane_T dx = longitudinal.fx.derivative;
    const Lane_T secant_y = (raw[mf6x::SA] != 0.0).select(at.fy / raw[mf6x::SA], dy);
    const Lane_T secant_x = (raw[mf6x::SR] != 0.0).select(at.fx / raw[mf6x::SR], dx);
    const Lane_T alpha_end =
        mf61_peak_end(sgn_plus(raw[mf6x::SA]), limits(mf6x::ALPMIN), limits(mf6x::ALPMAX), std::numbers::pi / 2.0);
    const auto lateral_slope = [&](const Lane_T &slip) noexcept -> Lane_T {
      return mf61_planar(c, options.mode, options.turn_slip, mf61_seed(p, mf6x::SA, slip, one)).fy.derivative;
    };
    const Lane_T alpha_peak = first_stationary_point(lateral_slope, alpha_end);
    const Lane_T kappa_end = mf61_peak_end(sgn_plus(raw[mf6x::SR]), limits(mf6x::KPUMIN), limits(mf6x::KPUMAX), 1.0);
    const auto longitudinal_slope = [&](const Lane_T &slip) noexcept -> Lane_T {
      return mf61_planar(c, options.mode, options.turn_slip, mf61_seed(p, mf6x::SR, slip, one)).fx.derivative;
    };
    const Lane_T kappa_peak = first_stationary_point(longitudinal_slope, kappa_end);
    return {{conditioned.contact.select(dy, zero), conditioned.contact.select(dx, zero),
             conditioned.contact.select(secant_y, zero), conditioned.contact.select(secant_x, zero),
             conditioned.contact.select(at.dy, zero), conditioned.contact.select(at.dx, zero),
             conditioned.contact.select(alpha_peak, nan), conditioned.contact.select(kappa_peak, nan)}};
  }
} // namespace mf::detail

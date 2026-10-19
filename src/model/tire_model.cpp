#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <format>
#include <magic_formula/tire_model.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mf {
  namespace {
    /// One validity range: the .tir keys of its bounds and the member of `operating_limits` that holds it.
    struct limit_entry {
      std::string_view min_key;
      std::string_view max_key;
      std::optional<interval> operating_limits::*member;
    };

    constexpr std::array<limit_entry, 4> LIMIT_TABLE{{
        {.min_key = "FZMIN", .max_key = "FZMAX", .member = &operating_limits::fz_n},
        {.min_key = "KPUMIN", .max_key = "KPUMAX", .member = &operating_limits::kappa},
        {.min_key = "ALPMIN", .max_key = "ALPMAX", .member = &operating_limits::alpha_rad},
        {.min_key = "CAMMIN", .max_key = "CAMMAX", .member = &operating_limits::gamma_rad},
    }};

    /// Clamps `value` to `range` when the range exists (AD-15). A value that is outside the range sets `flag`;
    /// exact limit values are not flagged. Total function: no precondition.
    double clamp_input(double value, const std::optional<interval> &range, bool &flag) noexcept {
      if (!range.has_value()) {
        return value;
      }
      if (value < range->lower) {
        flag = true;
        return range->lower;
      }
      if (value > range->upper) {
        flag = true;
        return range->upper;
      }
      return value;
    }

    /// Location that names only a key. Built through a named local and a move so that the `make_error` call sites
    /// carry no exception-cleanup branches of their own.
    location key_location(std::string_view key) {
      std::string text(key);
      return location{.file = {}, .line = 0, .key = std::move(text)};
    }
  } // namespace

  std::expected<void, error> validate_limits(const operating_limits &limits) {
    for (const limit_entry &entry : LIMIT_TABLE) {
      const std::optional<interval> &range = limits.*(entry.member);
      if (range.has_value() && range->lower > range->upper) {
        return make_error(error_code::INVALID_VALUE,
                          std::format("{} {} > {} {}: lower limit above upper limit (AD-15)", entry.min_key,
                                      range->lower, entry.max_key, range->upper),
                          key_location(entry.min_key));
      }
    }
    return {};
  }

  tire_model::tire_model(const operating_limits &limits, const model_metadata &metadata)
      : limits_(limits), metadata_(metadata) {}

  const operating_limits &tire_model::limits() const noexcept {
    return limits_;
  }

  const model_metadata &tire_model::metadata() const noexcept {
    return metadata_;
  }

  std::expected<void, error> tire_model::validate_input(const operating_point &point) const {
    const std::array<std::pair<std::string_view, double>, 5> fields{{
        {"FZ", point.fz_n},
        {"SR", point.kappa},
        {"SA", point.alpha_rad},
        {"IA", point.gamma_rad},
        {"VX", point.vx_mps},
    }};
    for (const auto &[key, value] : fields) {
      if (!std::isfinite(value)) {
        return make_error(error_code::INVALID_INPUT, std::format("{} is not finite ({})", key, value),
                          key_location(key));
      }
    }
    if (point.vx_mps < 0.0) {
      return make_error(error_code::INVALID_INPUT,
                        std::format("Vx = {} m/s is negative; reverse driving is not supported (AD-02)", point.vx_mps),
                        key_location("VX"));
    }
    return {};
  }

  std::expected<tire_outputs, error> tire_model::evaluate(const operating_point &point, slip_mode mode) const {
    if (const std::expected<void, error> valid = validate_input(point); !valid.has_value()) {
      return std::unexpected<error>(valid.error());
    }

    input_flags flags{};
    flags.below_vxlow = point.vx_mps < limits_.vxlow_mps;

    // AD-01: decided on the raw Fz, before clamping; -0.0 is included.
    if (point.fz_n <= 0.0) {
      return tire_outputs{.flags = flags};
    }

    operating_point clamped = point;
    clamped.fz_n = clamp_input(point.fz_n, limits_.fz_n, flags.fz_clamped);
    clamped.kappa = clamp_input(point.kappa, limits_.kappa, flags.kappa_clamped);
    clamped.alpha_rad = clamp_input(point.alpha_rad, limits_.alpha_rad, flags.alpha_clamped);
    clamped.gamma_rad = clamp_input(point.gamma_rad, limits_.gamma_rad, flags.gamma_clamped);

    // A validated Fz interval may still end at or below zero; the hook contract (Fz > 0) then gives the AD-01 zeros.
    if (clamped.fz_n <= 0.0) {
      return tire_outputs{.flags = flags};
    }

    tire_outputs out = compute(clamped, mode);
    out.flags = flags;

    // AD-22: a non-finite result is an error, never a printed value.
    const std::array<std::pair<std::string_view, double>, 5> results{{
        {"FX", out.fx_n},
        {"FY", out.fy_n},
        {"MZ", out.mz_nm},
        {"MX", out.mx_nm},
        {"MY", out.my_nm},
    }};
    for (const auto &[key, value] : results) {
      if (!std::isfinite(value)) {
        return make_error(error_code::NUMERICAL, std::format("non-finite {} output ({}) (AD-22)", key, value),
                          key_location(key));
      }
    }
    return out;
  }

  std::expected<std::vector<tire_outputs>, batch_error>
  tire_model::evaluate_batch(std::span<const operating_point> points, slip_mode mode) const {
    std::vector<tire_outputs> results;
    results.reserve(points.size());
    for (std::size_t index = 0; index < points.size(); ++index) {
      std::expected<tire_outputs, error> evaluated = evaluate(points[index], mode);
      if (!evaluated.has_value()) {
        return std::unexpected<batch_error>(batch_error{.index = index, .cause = std::move(evaluated.error())});
      }
      results.push_back(*evaluated);
    }
    return results;
  }
} // namespace mf

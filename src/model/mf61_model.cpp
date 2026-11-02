#include <Eigen/Core>
#include <algorithm>
#include <array>
#include <cmath>
#include <expected>
#include <magic_formula/detail/lanes.hpp>
#include <magic_formula/detail/mf61_kernels.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf61_coefficients.hpp>
#include <magic_formula/mf61_model.hpp>
#include <magic_formula/mf6x_layout.hpp>
#include <magic_formula/mf6x_signs.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/sign_transform.hpp>
#include <magic_formula/tire_model.hpp>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

namespace mf {
  namespace {
    detail::mf61_inputs<detail::point_lane> load_point(const Eigen::Ref<const Eigen::VectorXd> &input) noexcept {
      detail::mf61_inputs<detail::point_lane> lanes;
      Eigen::Index column = 0;
      for (auto &lane : lanes) {
        lane = detail::point_lane::Constant(input(column));
        ++column;
      }
      return lanes;
    }
    detail::mf61_inputs<detail::block_lane> load_block(const Eigen::Ref<const Eigen::MatrixXd> &inputs,
                                                       Eigen::Index first, Eigen::Index count) noexcept {
      detail::mf61_inputs<detail::block_lane> lanes;
      Eigen::Index column = 0;
      for (auto &lane : lanes) {
        lane = inputs.col(column).segment(first, count).array();
        ++column;
      }
      return lanes;
    }
    template <std::size_t Count_T>
    void store_point(const std::array<detail::point_lane, Count_T> &lanes,
                     Eigen::Ref<Eigen::VectorXd> output) noexcept {
      Eigen::Index column = 0;
      for (const auto &lane : lanes) {
        output(column) = lane(0);
        ++column;
      }
    }
    template <std::size_t Count_T>
    void store_block(const std::array<detail::block_lane, Count_T> &lanes, Eigen::Ref<Eigen::MatrixXd> outputs,
                     Eigen::Index first, Eigen::Index count) noexcept {
      Eigen::Index column = 0;
      for (const auto &lane : lanes) {
        outputs.col(column).segment(first, count) = lane.matrix();
        ++column;
      }
    }
  } // namespace
  mf61_model::mf61_model(const mf61_parameters &parameters, const model_options &options)
      : tire_model(mf6x::INPUT_LENGTH, mf6x::OUTPUT_LENGTH, parameters.limits, parameters.metadata, options),
        coefficients_(parameters.coefficients) {}
  std::expected<mf61_model, error> mf61_model::create(mf61_parameters parameters, const model_options &options) {
    if (const auto valid = validate_coefficients(parameters.coefficients); !valid.has_value()) {
      return std::unexpected<error>(valid.error());
    }
    if (const auto valid = validate_limits(parameters.limits, mf6x::LIMITS); !valid.has_value()) {
      return std::unexpected<error>(valid.error());
    }
    for (const int index : std::array<int, 2>{mf6x::PRESMIN, mf6x::PRESMAX}) {
      const double bound = parameters.limits(index);
      if (std::isfinite(bound) && !(bound > 0.0)) {
        location where;
        where.key = std::span<const limit_channel>{mf6x::LIMITS}[static_cast<std::size_t>(index)].key;
        return make_error(error_code::INVALID_VALUE, "pressure limits must be positive", std::move(where));
      }
    }
    return mf61_model(parameters, options);
  }
  std::string_view mf61_model::name() const noexcept {
    return "MF 6.1";
  }
  int mf61_model::input_length() const noexcept {
    return mf6x::INPUT_LENGTH;
  }
  int mf61_model::output_length() const noexcept {
    return mf6x::OUTPUT_LENGTH;
  }
  const model_layout &mf61_model::layout() const noexcept {
    return mf6x::LAYOUT;
  }
  const mf61_coefficients &mf61_model::coefficients() const noexcept {
    return coefficients_;
  }
  Eigen::VectorXd mf61_model::reference_input() const {
    std::array<double, mf6x::INPUT_LENGTH> values{};
    values[mf6x::FZ] = coefficients_.FNOMIN * coefficients_.LFZ0;
    values[mf6x::VX] = coefficients_.LONGVL;
    values[mf6x::P] = coefficients_.NOMPRES;
    return Eigen::Map<const Eigen::VectorXd>(values.data(), mf6x::INPUT_LENGTH);
  }
  std::unique_ptr<tire_model> mf61_model::rebuild(const sign_transforms &transforms,
                                                  const Eigen::VectorXd &limits) const {
    mf61_parameters parameters;
    parameters.coefficients = mf6x::transform_coefficients(coefficients_, transforms);
    parameters.limits = limits;
    parameters.metadata = metadata();
    mf61_model model(parameters, options());
    return std::make_unique<mf61_model>(std::move(model));
  }
  void mf61_model::compute(const Eigen::Ref<const Eigen::VectorXd> &input,
                           Eigen::Ref<Eigen::VectorXd> output) const noexcept {
    store_point(detail::mf61_pipeline(coefficients_, options(), limits(), load_point(input)), output);
  }
  void mf61_model::compute_batch(const Eigen::Ref<const Eigen::MatrixXd> &inputs,
                                 Eigen::Ref<Eigen::MatrixXd> outputs) const noexcept {
    for (Eigen::Index first = 0; first < inputs.rows(); first += detail::BLOCK_ROWS) {
      const Eigen::Index count = std::min(detail::BLOCK_ROWS, inputs.rows() - first);
      store_block(detail::mf61_pipeline(coefficients_, options(), limits(), load_block(inputs, first, count)), outputs,
                  first, count);
    }
  }
  void mf61_model::compute_characteristics(const Eigen::Ref<const Eigen::VectorXd> &input,
                                           Eigen::Ref<Eigen::VectorXd> characteristics) const noexcept {
    store_point(detail::mf61_characteristics(coefficients_, options(), limits(), load_point(input)), characteristics);
  }
  void mf61_model::compute_characteristics_batch(const Eigen::Ref<const Eigen::MatrixXd> &inputs,
                                                 Eigen::Ref<Eigen::MatrixXd> characteristics) const noexcept {
    for (Eigen::Index first = 0; first < inputs.rows(); first += detail::BLOCK_ROWS) {
      const Eigen::Index count = std::min(detail::BLOCK_ROWS, inputs.rows() - first);
      store_block(detail::mf61_characteristics(coefficients_, options(), limits(), load_block(inputs, first, count)),
                  characteristics, first, count);
    }
  }
} // namespace mf

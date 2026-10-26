#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <gtest/gtest.h>
#include <limits>
#include <magic_formula/coordinate_identification.hpp>
#include <magic_formula/coordinate_system.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf52_model.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/mf5x_signs.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/pac2002_model.hpp>
#include <magic_formula/sign_transform.hpp>
#include <magic_formula/tire_model.hpp>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
  using mf::axis_sign;
  using mf::decision_basis;
  using mf::identification_axis;
  using mf::reading_status;
  using mf::sign_transforms;
  using mf::system_classification;
  using mf::test::bits;

  constexpr double QUIET_NAN = std::numeric_limits<double>::quiet_NaN();
  constexpr double POSITIVE_INF = std::numeric_limits<double>::infinity();

  /// Tolerance of the reading comparisons under a sign transform: the maps are exact up to libm and SIMD differences.
  constexpr double READING_REL_TOL = 1e-12;

  constexpr std::string_view MIXED_VERTICAL_ISO_LATERAL_SAE =
      "coordinate system: the coefficients mix conventions: the vertical axis is ISO (a loaded tire has FZ > 0) but "
      "the lateral axis is SAE (dFy/dSA < 0); convert the model to ISO or SAE";
  constexpr std::string_view MIXED_VERTICAL_SAE_LATERAL_ISO =
      "coordinate system: the coefficients mix conventions: the vertical axis is SAE (a loaded tire has FZ < 0) but "
      "the lateral axis is ISO (dFy/dSA > 0); convert the model to ISO or SAE";
  constexpr std::string_view LATERAL_UNDETERMINED_ISO =
      "coordinate system: the lateral axis is undetermined (no significant indicator); it is assumed to match the "
      "vertical axis (ISO)";
  constexpr std::string_view CAMBER_MEANING = "camber thrust toward the lean (dFy/dIA < 0 with z up)";
  constexpr std::string_view FX_MEANING =
      "driving force grows with slip ratio: the SR convention or the PKX1 sign is inconsistent with FNOMIN";
  constexpr std::string_view V_NONE_ISO =
      "coordinate system: the vertical axis is undetermined (no significant indicator); ISO is assumed";
  constexpr std::string_view L_NONE_ISO =
      "coordinate system: the lateral axis is undetermined (no significant indicator); ISO is assumed";
  constexpr std::string_view V_NONE_MATCH_L_SAE =
      "coordinate system: the vertical axis is undetermined (no significant indicator); it is assumed to match the "
      "lateral axis (SAE)";
  constexpr std::string_view V_CONFLICT_ISO =
      "coordinate system: the vertical axis is undetermined (the indicators conflict: confidence 0.50 < "
      "0.60); ISO is assumed";
  constexpr std::string_view L_CONFLICT_055_ISO =
      "coordinate system: the lateral axis is undetermined (the indicators conflict: confidence 0.55 < "
      "0.60); ISO is assumed";
  constexpr std::string_view V_CONFLICT_MATCH_L_ISO =
      "coordinate system: the vertical axis is undetermined (the indicators conflict: confidence 0.50 < "
      "0.60); it is assumed to match the lateral axis (ISO)";
  constexpr std::string_view L_CONFLICT_MATCH_V_SAE =
      "coordinate system: the lateral axis is undetermined (the indicators conflict: confidence 0.50 < "
      "0.60); it is assumed to match the vertical axis (SAE)";
  constexpr std::string_view L_CONFLICT_MATCH_V_ISO =
      "coordinate system: the lateral axis is undetermined (the indicators conflict: confidence 0.50 < "
      "0.60); it is assumed to match the vertical axis (ISO)";
  constexpr std::string_view V_DISSENT_ISO =
      "coordinate system: dFy/dIA (camber thrust toward the lean (dFy/dIA < 0 with z up)), My (rolling "
      "resistance opposes rolling (My < 0 with z up)) disagree with the vertical axis decision (ISO, "
      "confidence 0.67)";
  constexpr std::string_view L_DISSENT_SAE =
      "coordinate system: dFy/dSA (cornering stiffness (dFy/dSA > 0 in ISO)), Fy secant 1 deg (odd part of Fy, "
      "immune to offsets) disagree with the lateral axis decision (SAE, confidence 0.75)";
  constexpr std::string_view DMZ_DISSENT_ISO_080 =
      "coordinate system: dMz/dSA (positive pneumatic trail (dMz/dSA opposite to dFy/dSA)) disagree with the lateral "
      "axis decision (ISO, confidence 0.80)";
  constexpr std::string_view DMZ_DISSENT_SAE_083 =
      "coordinate system: dMz/dSA (positive pneumatic trail (dMz/dSA opposite to dFy/dSA)) disagree with the lateral "
      "axis decision (SAE, confidence 0.83)";

  // Indices into IDENTIFICATION_INDICATORS.
  constexpr std::size_t FZ0 = 0;
  constexpr std::size_t D_FY_D_IA = 1;
  constexpr std::size_t MY = 2;
  constexpr std::size_t D_FY_D_SA = 3;
  constexpr std::size_t SECANT_1 = 4;
  constexpr std::size_t SECANT_3 = 5;
  constexpr std::size_t D_MZ_D_SA = 8;
  constexpr std::size_t D_FX_D_SR = 9;
  constexpr std::size_t FX_SECANT = 10;

  /// A table definition with explicit axis, polarity, weight, threshold and sweep.
  mf::indicator_definition make_definition(identification_axis axis, double polarity, double weight, double threshold,
                                           bool sweep) {
    return mf::indicator_definition{.name = "crafted",
                                    .meaning = "crafted meaning",
                                    .unit = "N",
                                    .axis = axis,
                                    .form = mf::indicator_form::OUTPUT_LEVEL,
                                    .channel = "",
                                    .response = "",
                                    .step = 0.0,
                                    .polarity = polarity,
                                    .weight = weight,
                                    .threshold = threshold,
                                    .load_sweep = sweep};
  }

  /// A VOTE reading of `axis` with the given vote and weight.
  mf::indicator_reading vote_reading(identification_axis axis, int vote, double weight) {
    mf::indicator_reading reading;
    reading.definition = make_definition(axis, 1.0, 1.0, 0.0, false);
    reading.status = reading_status::VOTE;
    reading.vote = vote;
    reading.weight = weight;
    return reading;
  }

  /// A reading of a row of the default table at `load_scale` x 4000 N.
  mf::indicator_reading table_reading(std::size_t table_index, double load_scale, double value) {
    return mf::make_reading(mf::IDENTIFICATION_INDICATORS[table_index], load_scale, load_scale * 4000.0, value, false,
                            false);
  }

  mf::axis_decision decided(axis_sign sign, double positive, double negative, double confidence) {
    return mf::axis_decision{.sign = sign,
                             .basis = decision_basis::DECIDED,
                             .confidence = confidence,
                             .positive_weight = positive,
                             .negative_weight = negative};
  }

  mf::axis_decision conflicting(double confidence) {
    return mf::axis_decision{.sign = axis_sign::UNDETERMINED,
                             .basis = decision_basis::CONFLICTING_EVIDENCE,
                             .confidence = confidence,
                             .positive_weight = 0.5,
                             .negative_weight = 0.5};
  }

  /// A hand-built identification (the warnings use only the decisions, the classification, the effective signs and
  /// the readings).
  mf::coordinate_identification build_identification(const mf::axis_decision &vertical,
                                                     const mf::axis_decision &lateral, const mf::axis_signs &effective,
                                                     system_classification classification,
                                                     std::vector<mf::indicator_reading> readings = {}) {
    mf::coordinate_identification identification;
    identification.classification = classification;
    identification.vertical = vertical;
    identification.lateral = lateral;
    identification.effective = effective;
    identification.readings = std::move(readings);
    return identification;
  }

  constexpr mf::axis_signs ISO_SIGNS{.vertical = axis_sign::POSITIVE, .lateral = axis_sign::POSITIVE};
  constexpr mf::axis_signs SAE_SIGNS{.vertical = axis_sign::NEGATIVE, .lateral = axis_sign::NEGATIVE};
  constexpr mf::axis_signs PLUS_MINUS{.vertical = axis_sign::POSITIVE, .lateral = axis_sign::NEGATIVE};
  constexpr mf::axis_signs MINUS_PLUS{.vertical = axis_sign::NEGATIVE, .lateral = axis_sign::POSITIVE};

  /// Expects `warnings[index]` to have this key and message and an empty file and line.
  void expect_warning_at(const std::vector<mf::warning> &warnings, std::size_t index, std::string_view key,
                         std::string_view message) {
    ASSERT_LT(index, warnings.size());
    const mf::warning &item = warnings[index];
    EXPECT_EQ(item.where.key, key) << "warning " << index;
    EXPECT_EQ(item.message, message) << "warning " << index;
    EXPECT_TRUE(item.where.file.empty()) << "warning " << index;
    EXPECT_EQ(item.where.line, 0U) << "warning " << index;
  }

  /// Expects exactly one warning with this key and message.
  void expect_one(const std::vector<mf::warning> &warnings, std::string_view key, std::string_view message) {
    ASSERT_EQ(warnings.size(), 1U);
    expect_warning_at(warnings, 0, key, message);
  }

  /// The warning text of a failed CHECK reading: `head` is "<name> = <value> <unit> at <scale> x the nominal load".
  std::string check_text(std::string_view head) {
    return std::format("coordinate system: {} is negative: {}; no conversion changes this", head, FX_MEANING);
  }

  /// The warning text of the dFy/dIA dissent with the given decision phrase.
  std::string camber_dissent_text(std::string_view decision) {
    return std::format("coordinate system: dFy/dIA ({}) disagree with {}", CAMBER_MEANING, decision);
  }

  // ---- models ------------------------------------------------------------------------------------------------------

  /// A PAC2002 model of `parameters` (the creation must succeed).
  std::unique_ptr<mf::tire_model> make_pac2002(const mf::mf5x_parameters &parameters,
                                               const mf::model_options &options = {}) {
    std::expected<mf::pac2002_model, mf::error> created =
        mf::pac2002_model::create(parameters, mf::combined_method::COSINE, options);
    if (!created.has_value()) {
      ADD_FAILURE() << mf::to_string(created.error());
      return nullptr;
    }
    return std::make_unique<mf::pac2002_model>(std::move(*created));
  }

  /// An MF 5.2 model of `parameters` (FITTYP 6; the creation must succeed).
  std::unique_ptr<mf::tire_model> make_mf52(mf::mf5x_parameters parameters) {
    parameters.metadata.fittyp = 6;
    std::expected<mf::mf52_model, mf::error> created = mf::mf52_model::create(std::move(parameters));
    if (!created.has_value()) {
      ADD_FAILURE() << mf::to_string(created.error());
      return nullptr;
    }
    return std::make_unique<mf::mf52_model>(std::move(*created));
  }

  /// The identification of a PAC2002 model of `parameters`.
  mf::coordinate_identification identify(const mf::mf5x_parameters &parameters, const mf::model_options &options = {}) {
    const std::unique_ptr<mf::tire_model> model = make_pac2002(parameters, options);
    if (model == nullptr) {
      return mf::coordinate_identification{};
    }
    return model->find_coordinate_system();
  }

  /// The formulations the metamorphic tests run over.
  enum class formulation : std::uint8_t { PAC_COSINE, PAC_ELLIPSE, MF52 };

  constexpr std::array<formulation, 3> ALL_FORMULATIONS{formulation::PAC_COSINE, formulation::PAC_ELLIPSE,
                                                        formulation::MF52};

  std::string_view formulation_name(formulation form) {
    if (form == formulation::PAC_COSINE) {
      return "PAC2002 cosine";
    }
    if (form == formulation::PAC_ELLIPSE) {
      return "PAC2002 friction ellipse";
    }
    return "MF 5.2";
  }

  /// The model of formulation `form` for `parameters` (MF 5.2 sets FITTYP 6; the creation must succeed).
  std::unique_ptr<mf::tire_model> make_model(formulation form, const mf::mf5x_parameters &parameters) {
    if (form == formulation::MF52) {
      return make_mf52(parameters);
    }
    if (form == formulation::PAC_ELLIPSE) {
      std::expected<mf::pac2002_model, mf::error> created =
          mf::pac2002_model::create(parameters, mf::combined_method::FRICTION_ELLIPSE, mf::model_options{});
      if (!created.has_value()) {
        ADD_FAILURE() << mf::to_string(created.error());
        return nullptr;
      }
      return std::make_unique<mf::pac2002_model>(std::move(*created));
    }
    return make_pac2002(parameters);
  }

  /// The identification of the model of formulation `form` for `parameters`.
  mf::coordinate_identification identify(formulation form, const mf::mf5x_parameters &parameters) {
    const std::unique_ptr<mf::tire_model> model = make_model(form, parameters);
    if (model == nullptr) {
      return mf::coordinate_identification{};
    }
    return model->find_coordinate_system();
  }

  /// `source` with the coefficients and limits transformed by `transforms`.
  mf::mf5x_parameters transformed_parameters(const mf::mf5x_parameters &source, const sign_transforms &transforms) {
    mf::mf5x_parameters result;
    result.coefficients = mf::mf5x::transform_coefficients(source.coefficients, transforms);
    result.limits = mf::transform_limits(source.limits, mf::mf5x::LIMITS, transforms);
    result.metadata = source.metadata;
    return result;
  }

  /// The readings of `identification` whose definition is called `name`.
  std::vector<mf::indicator_reading> readings_named(const mf::coordinate_identification &identification,
                                                    std::string_view name) {
    std::vector<mf::indicator_reading> result;
    for (const mf::indicator_reading &reading : identification.readings) {
      if (reading.definition.name == name) {
        result.push_back(reading);
      }
    }
    return result;
  }

  /// Expects `count` readings called `name`, all with `status` and `vote`.
  void expect_readings(const mf::coordinate_identification &identification, std::string_view name, std::size_t count,
                       reading_status status, int vote) {
    const std::vector<mf::indicator_reading> readings = readings_named(identification, name);
    EXPECT_EQ(readings.size(), count) << name;
    for (const mf::indicator_reading &reading : readings) {
      EXPECT_EQ(reading.status, status) << name << " at " << reading.load_scale;
      EXPECT_EQ(reading.vote, vote) << name << " at " << reading.load_scale;
    }
  }

  /// Expects both axes decided with the given signs and a confidence of 1.
  void expect_decided(const mf::coordinate_identification &identification, axis_sign vertical, axis_sign lateral) {
    EXPECT_EQ(identification.vertical.basis, decision_basis::DECIDED);
    EXPECT_EQ(identification.vertical.sign, vertical);
    EXPECT_NEAR(identification.vertical.confidence, 1.0, 1e-12);
    EXPECT_EQ(identification.lateral.basis, decision_basis::DECIDED);
    EXPECT_EQ(identification.lateral.sign, lateral);
    EXPECT_NEAR(identification.lateral.confidence, 1.0, 1e-12);
  }

  // ---- linear stub -------------------------------------------------------------------------------------------------

  constexpr mf::input_channel STUB_FZ{.name = "FZ", .unit = "N"};
  constexpr mf::input_channel STUB_SA{.name = "SA", .unit = "rad"};
  constexpr mf::output_channel STUB_FY{.name = "FY", .unit = "N"};
  constexpr std::array<mf::input_channel, 2> STUB_INPUTS_WITH_FZ{STUB_FZ, STUB_SA};
  constexpr std::array<mf::input_channel, 1> STUB_INPUTS_NO_FZ{STUB_SA};
  constexpr std::array<mf::output_channel, 1> STUB_OUTPUTS{STUB_FY};
  constexpr mf::model_layout STUB_LAYOUT_WITH_FZ{.inputs = STUB_INPUTS_WITH_FZ, .outputs = STUB_OUTPUTS, .limits = {}};
  constexpr mf::model_layout STUB_LAYOUT_NO_FZ{.inputs = STUB_INPUTS_NO_FZ, .outputs = STUB_OUTPUTS, .limits = {}};

  /// FY = stiffness*SA with a configurable reference FZ and an optional FZ input.
  class linear_stub final : public mf::tire_model {
  public:
    linear_stub(double stiffness, double reference_fz, bool with_fz)
        : tire_model(with_fz ? 2 : 1, 1, Eigen::VectorXd(), mf::model_metadata{}, mf::model_options{}),
          stiffness_(stiffness), reference_fz_(reference_fz), with_fz_(with_fz) {}

    [[nodiscard]] std::string_view name() const noexcept override {
      return "linear-stub";
    }
    [[nodiscard]] int input_length() const noexcept override {
      return with_fz_ ? 2 : 1;
    }
    [[nodiscard]] int output_length() const noexcept override {
      return 1;
    }
    [[nodiscard]] const mf::model_layout &layout() const noexcept override {
      return with_fz_ ? STUB_LAYOUT_WITH_FZ : STUB_LAYOUT_NO_FZ;
    }
    [[nodiscard]] Eigen::VectorXd reference_input() const override {
      Eigen::VectorXd input = Eigen::VectorXd::Zero(input_length());
      if (with_fz_) {
        input(0) = reference_fz_;
      }
      return input;
    }

  private:
    [[nodiscard]] std::unique_ptr<mf::tire_model> rebuild(const sign_transforms & /*transforms*/,
                                                          const Eigen::VectorXd & /*limits*/) const override {
      return std::make_unique<linear_stub>(stiffness_, reference_fz_, with_fz_);
    }

    [[nodiscard]] Eigen::Index sa_column() const noexcept {
      return with_fz_ ? 1 : 0;
    }

    void compute(const Eigen::Ref<const Eigen::VectorXd> &input,
                 Eigen::Ref<Eigen::VectorXd> output) const noexcept override {
      output(0) = stiffness_ * input(sa_column());
    }

    void compute_batch(const Eigen::Ref<const Eigen::MatrixXd> &inputs,
                       Eigen::Ref<Eigen::MatrixXd> outputs) const noexcept override {
      for (Eigen::Index r = 0; r < inputs.rows(); ++r) {
        outputs(r, 0) = stiffness_ * inputs(r, sa_column());
      }
    }

    double stiffness_;
    double reference_fz_;
    bool with_fz_;
  };

  /// A lateral central-difference row of the stub tables with the given polarity and weight.
  mf::indicator_definition stub_lateral_row(double polarity, double weight) {
    return mf::indicator_definition{.name = "stub lateral",
                                    .meaning = "stub",
                                    .unit = "N/rad",
                                    .axis = identification_axis::LATERAL,
                                    .form = mf::indicator_form::CENTRAL_DIFFERENCE,
                                    .channel = "SA",
                                    .response = "FY",
                                    .step = 1e-3,
                                    .polarity = polarity,
                                    .weight = weight,
                                    .threshold = 1e-3,
                                    .load_sweep = false};
  }
} // namespace

// ---- make_reading ---------------------------------------------------------------------------------------------------

TEST(coordinate_identification, MakeReading_Missing_MissingChannelBeatsClamp) {
  const mf::indicator_definition definition = make_definition(identification_axis::VERTICAL, 1.0, 1.0, 1e-3, false);
  const mf::indicator_reading reading = mf::make_reading(definition, 1.0, 4000.0, QUIET_NAN, true, true);
  EXPECT_EQ(reading.status, reading_status::MISSING_CHANNEL);
  EXPECT_EQ(reading.vote, 0);
  EXPECT_EQ(reading.weight, 0.0);
  EXPECT_TRUE(std::isnan(reading.value));
  EXPECT_TRUE(std::isnan(reading.normalised));
  EXPECT_EQ(reading.definition.name, "crafted");
  EXPECT_EQ(bits(reading.load_n), bits(4000.0));
}

TEST(coordinate_identification, MakeReading_Clamped_BeatsNotFinite) {
  const mf::indicator_definition definition = make_definition(identification_axis::VERTICAL, 1.0, 1.0, 1e-3, false);
  const mf::indicator_reading reading = mf::make_reading(definition, 1.0, 4000.0, QUIET_NAN, true, false);
  EXPECT_EQ(reading.status, reading_status::CLAMPED);
  EXPECT_EQ(reading.vote, 0);
  EXPECT_EQ(reading.weight, 0.0);
}

TEST(coordinate_identification, MakeReading_NanValue_NotFinite) {
  const mf::indicator_definition definition = make_definition(identification_axis::VERTICAL, 1.0, 1.0, 1e-3, false);
  EXPECT_EQ(mf::make_reading(definition, 1.0, 4000.0, QUIET_NAN, false, false).status, reading_status::NOT_FINITE);
  EXPECT_EQ(mf::make_reading(definition, 1.0, 4000.0, POSITIVE_INF, false, false).status, reading_status::NOT_FINITE);
}

TEST(coordinate_identification, MakeReading_InfNormalised_NotFinite) {
  const mf::indicator_definition definition = make_definition(identification_axis::VERTICAL, 1.0, 1.0, 1e-3, false);
  const mf::indicator_reading reading = mf::make_reading(definition, 1.0, 0.0, 1.0, false, false);
  EXPECT_EQ(reading.status, reading_status::NOT_FINITE);
  EXPECT_EQ(bits(reading.value), bits(1.0));
  EXPECT_TRUE(std::isinf(reading.normalised));
  EXPECT_EQ(reading.vote, 0);
}

TEST(coordinate_identification, MakeReading_SmallValue_BelowThreshold) {
  const mf::indicator_definition definition = make_definition(identification_axis::VERTICAL, 1.0, 1.0, 0.5, false);
  // |normalised| == threshold abstains (2000 / 4000 == 0.5 exactly).
  const mf::indicator_reading at_threshold = mf::make_reading(definition, 1.0, 4000.0, 2000.0, false, false);
  EXPECT_EQ(at_threshold.status, reading_status::BELOW_THRESHOLD);
  EXPECT_EQ(at_threshold.vote, 0);
  EXPECT_EQ(at_threshold.weight, 0.0);
  EXPECT_EQ(bits(at_threshold.normalised), bits(0.5));
  EXPECT_EQ(bits(at_threshold.value), bits(2000.0));
  const mf::indicator_reading above = mf::make_reading(definition, 1.0, 4000.0, 2001.0, false, false);
  EXPECT_EQ(above.status, reading_status::VOTE);
}

TEST(coordinate_identification, MakeReading_PositiveVote_SweptWeightIsThird) {
  const mf::indicator_definition definition = make_definition(identification_axis::LATERAL, 1.0, 1.0, 1e-3, true);
  const mf::indicator_reading reading = mf::make_reading(definition, 0.5, 2000.0, 10.0, false, false);
  EXPECT_EQ(reading.status, reading_status::VOTE);
  EXPECT_EQ(reading.vote, 1);
  EXPECT_NEAR(reading.weight, 1.0 / 3.0, 1e-15);
  EXPECT_EQ(bits(reading.load_scale), bits(0.5));
  EXPECT_EQ(bits(reading.normalised), bits(10.0 / 2000.0));
}

TEST(coordinate_identification, MakeReading_NegativeVote_UnsweptFullWeight) {
  const mf::indicator_definition negative_polarity =
      make_definition(identification_axis::LATERAL, -1.0, 0.75, 1e-3, false);
  const mf::indicator_reading positive_value = mf::make_reading(negative_polarity, 1.0, 4000.0, 10.0, false, false);
  EXPECT_EQ(positive_value.vote, -1);
  EXPECT_EQ(bits(positive_value.weight), bits(0.75));
  const mf::indicator_reading negative_value = mf::make_reading(negative_polarity, 1.0, 4000.0, -10.0, false, false);
  EXPECT_EQ(negative_value.vote, 1);
  // The normalisation uses the magnitude of the load: a negative load does not change the sign of the value.
  const mf::indicator_reading negative_load = mf::make_reading(negative_polarity, 1.0, -4000.0, 10.0, false, false);
  EXPECT_EQ(bits(negative_load.normalised), bits(10.0 / 4000.0));
  EXPECT_EQ(negative_load.vote, -1);
}

// ---- decide_axis ----------------------------------------------------------------------------------------------------

TEST(coordinate_identification, DecideAxis_Empty_Insufficient) {
  const mf::axis_decision decision = mf::decide_axis({}, identification_axis::VERTICAL);
  EXPECT_EQ(decision.sign, axis_sign::UNDETERMINED);
  EXPECT_EQ(decision.basis, decision_basis::INSUFFICIENT_EVIDENCE);
  EXPECT_EQ(decision.confidence, 0.0);
  EXPECT_EQ(decision.positive_weight, 0.0);
  EXPECT_EQ(decision.negative_weight, 0.0);
}

TEST(coordinate_identification, DecideAxis_SmallTotal_Insufficient) {
  const std::vector<mf::indicator_reading> small{vote_reading(identification_axis::LATERAL, 1, 0.1),
                                                 vote_reading(identification_axis::LATERAL, -1, 0.1)};
  const mf::axis_decision non_zero = mf::decide_axis(small, identification_axis::LATERAL);
  EXPECT_EQ(non_zero.sign, axis_sign::UNDETERMINED);
  EXPECT_EQ(non_zero.basis, decision_basis::INSUFFICIENT_EVIDENCE);
  EXPECT_NEAR(non_zero.confidence, 0.5, 1e-12);
  EXPECT_NEAR(non_zero.positive_weight, 0.1, 1e-15);
  EXPECT_NEAR(non_zero.negative_weight, 0.1, 1e-15);
  const std::vector<mf::indicator_reading> zero{vote_reading(identification_axis::LATERAL, 1, 0.0)};
  const mf::axis_decision no_weight = mf::decide_axis(zero, identification_axis::LATERAL);
  EXPECT_EQ(no_weight.basis, decision_basis::INSUFFICIENT_EVIDENCE);
  EXPECT_EQ(no_weight.confidence, 0.0);
}

TEST(coordinate_identification, DecideAxis_ExactTie_ConflictingHalfConfidence) {
  const std::vector<mf::indicator_reading> tie{vote_reading(identification_axis::LATERAL, 1, 0.5),
                                               vote_reading(identification_axis::LATERAL, -1, 0.5)};
  const mf::axis_decision decision = mf::decide_axis(tie, identification_axis::LATERAL);
  EXPECT_EQ(decision.sign, axis_sign::UNDETERMINED);
  EXPECT_EQ(decision.basis, decision_basis::CONFLICTING_EVIDENCE);
  EXPECT_EQ(decision.confidence, 0.5);
  EXPECT_EQ(decision.positive_weight, 0.5);
  EXPECT_EQ(decision.negative_weight, 0.5);
}

TEST(coordinate_identification, DecideAxis_LowConfidence_Conflicting) {
  const std::vector<mf::indicator_reading> split{vote_reading(identification_axis::VERTICAL, 1, 0.5),
                                                 vote_reading(identification_axis::VERTICAL, -1, 0.4)};
  const mf::axis_decision decision = mf::decide_axis(split, identification_axis::VERTICAL);
  EXPECT_EQ(decision.sign, axis_sign::UNDETERMINED);
  EXPECT_EQ(decision.basis, decision_basis::CONFLICTING_EVIDENCE);
  EXPECT_NEAR(decision.confidence, 0.5 / 0.9, 1e-12);
}

TEST(coordinate_identification, DecideAxis_AtMinimumConfidence_Decided) {
  const std::vector<mf::indicator_reading> split{vote_reading(identification_axis::VERTICAL, 1, 3.0),
                                                 vote_reading(identification_axis::VERTICAL, -1, 2.0)};
  const mf::axis_decision decision = mf::decide_axis(split, identification_axis::VERTICAL);
  EXPECT_EQ(decision.basis, decision_basis::DECIDED);
  EXPECT_EQ(decision.sign, axis_sign::POSITIVE);
}

TEST(coordinate_identification, DecideAxis_PositiveMajority_DecidedPositive) {
  const std::vector<mf::indicator_reading> votes{vote_reading(identification_axis::LATERAL, 1, 0.8),
                                                 vote_reading(identification_axis::LATERAL, -1, 0.2)};
  const mf::axis_decision decision = mf::decide_axis(votes, identification_axis::LATERAL);
  EXPECT_EQ(decision.sign, axis_sign::POSITIVE);
  EXPECT_EQ(decision.basis, decision_basis::DECIDED);
  EXPECT_NEAR(decision.confidence, 0.8, 1e-12);
  EXPECT_EQ(decision.positive_weight, 0.8);
  EXPECT_EQ(decision.negative_weight, 0.2);
}

TEST(coordinate_identification, DecideAxis_NegativeMajority_DecidedNegative) {
  const std::vector<mf::indicator_reading> votes{vote_reading(identification_axis::LATERAL, -1, 0.8),
                                                 vote_reading(identification_axis::LATERAL, 1, 0.2)};
  const mf::axis_decision decision = mf::decide_axis(votes, identification_axis::LATERAL);
  EXPECT_EQ(decision.sign, axis_sign::NEGATIVE);
  EXPECT_EQ(decision.basis, decision_basis::DECIDED);
  EXPECT_NEAR(decision.confidence, 0.8, 1e-12);
}

TEST(coordinate_identification, DecideAxis_OtherAxisCheckAndNonVote_Ignored) {
  mf::indicator_reading clamped = vote_reading(identification_axis::VERTICAL, -1, 5.0);
  clamped.status = reading_status::CLAMPED;
  std::vector<mf::indicator_reading> readings;
  readings.push_back(vote_reading(identification_axis::VERTICAL, 1, 0.5));
  readings.push_back(vote_reading(identification_axis::LATERAL, -1, 5.0));
  readings.push_back(vote_reading(identification_axis::CHECK, -1, 5.0));
  readings.push_back(clamped);
  const mf::axis_decision vertical = mf::decide_axis(readings, identification_axis::VERTICAL);
  EXPECT_EQ(vertical.sign, axis_sign::POSITIVE);
  EXPECT_EQ(vertical.basis, decision_basis::DECIDED);
  EXPECT_EQ(vertical.positive_weight, 0.5);
  EXPECT_EQ(vertical.negative_weight, 0.0);
  const mf::axis_decision lateral = mf::decide_axis(readings, identification_axis::LATERAL);
  EXPECT_EQ(lateral.sign, axis_sign::NEGATIVE);
  EXPECT_EQ(lateral.negative_weight, 5.0);
  EXPECT_EQ(lateral.positive_weight, 0.0);
}

// ---- effective signs, classification, plan --------------------------------------------------------------------------

TEST(coordinate_identification, EffectiveSigns_FourCases) {
  const mf::axis_decision plus = decided(axis_sign::POSITIVE, 1.0, 0.0, 1.0);
  const mf::axis_decision minus = decided(axis_sign::NEGATIVE, 0.0, 1.0, 1.0);
  const mf::axis_decision none{};

  mf::axis_signs both_decided = mf::effective_signs(plus, minus);
  EXPECT_EQ(both_decided.vertical, axis_sign::POSITIVE);
  EXPECT_EQ(both_decided.lateral, axis_sign::NEGATIVE);
  both_decided = mf::effective_signs(minus, plus);
  EXPECT_EQ(both_decided.vertical, axis_sign::NEGATIVE);
  EXPECT_EQ(both_decided.lateral, axis_sign::POSITIVE);

  const mf::axis_signs vertical_undetermined = mf::effective_signs(none, minus);
  EXPECT_EQ(vertical_undetermined.vertical, axis_sign::NEGATIVE);
  EXPECT_EQ(vertical_undetermined.lateral, axis_sign::NEGATIVE);

  const mf::axis_signs lateral_undetermined = mf::effective_signs(minus, none);
  EXPECT_EQ(lateral_undetermined.vertical, axis_sign::NEGATIVE);
  EXPECT_EQ(lateral_undetermined.lateral, axis_sign::NEGATIVE);

  const mf::axis_signs both_undetermined = mf::effective_signs(none, none);
  EXPECT_EQ(both_undetermined.vertical, axis_sign::POSITIVE);
  EXPECT_EQ(both_undetermined.lateral, axis_sign::POSITIVE);
}

TEST(coordinate_identification, ClassifySystem_FourCases) {
  EXPECT_EQ(mf::classify_system(ISO_SIGNS), system_classification::ISO);
  EXPECT_EQ(mf::classify_system(SAE_SIGNS), system_classification::SAE);
  EXPECT_EQ(mf::classify_system(PLUS_MINUS), system_classification::MIXED);
  EXPECT_EQ(mf::classify_system(MINUS_PLUS), system_classification::MIXED);
}

TEST(coordinate_identification, PlanConversion_EightCases) {
  struct plan_case {
    mf::axis_signs state;
    mf::coordinate_system target;
    bool vertical;
    bool lateral;
  };
  const std::array<plan_case, 8> cases{{
      {.state = ISO_SIGNS, .target = mf::coordinate_system::ISO, .vertical = false, .lateral = false},
      {.state = ISO_SIGNS, .target = mf::coordinate_system::SAE, .vertical = true, .lateral = false},
      {.state = SAE_SIGNS, .target = mf::coordinate_system::ISO, .vertical = true, .lateral = false},
      {.state = SAE_SIGNS, .target = mf::coordinate_system::SAE, .vertical = false, .lateral = false},
      {.state = PLUS_MINUS, .target = mf::coordinate_system::ISO, .vertical = false, .lateral = true},
      {.state = PLUS_MINUS, .target = mf::coordinate_system::SAE, .vertical = true, .lateral = true},
      {.state = MINUS_PLUS, .target = mf::coordinate_system::ISO, .vertical = true, .lateral = true},
      {.state = MINUS_PLUS, .target = mf::coordinate_system::SAE, .vertical = false, .lateral = true},
  }};
  for (const plan_case &item : cases) {
    const sign_transforms plan = mf::plan_conversion(item.state, item.target);
    const std::string_view vertical_name = mf::axis_sign_name(item.state.vertical);
    const std::string_view lateral_name = mf::axis_sign_name(item.state.lateral);
    const std::string_view target_name = mf::coordinate_system_name(item.target);
    SCOPED_TRACE(std::format("{},{} to {}", vertical_name, lateral_name, target_name));
    EXPECT_EQ(plan.vertical, item.vertical);
    EXPECT_EQ(plan.lateral, item.lateral);
  }
}

TEST(coordinate_identification, Names_EveryEnumerator_Spelling) {
  EXPECT_EQ(mf::identification_axis_name(identification_axis::VERTICAL), "VERTICAL");
  EXPECT_EQ(mf::identification_axis_name(identification_axis::LATERAL), "LATERAL");
  EXPECT_EQ(mf::identification_axis_name(identification_axis::CHECK), "CHECK");
  EXPECT_EQ(mf::indicator_form_name(mf::indicator_form::INPUT_LEVEL), "INPUT_LEVEL");
  EXPECT_EQ(mf::indicator_form_name(mf::indicator_form::OUTPUT_LEVEL), "OUTPUT_LEVEL");
  EXPECT_EQ(mf::indicator_form_name(mf::indicator_form::CENTRAL_DIFFERENCE), "CENTRAL_DIFFERENCE");
  EXPECT_EQ(mf::reading_status_name(reading_status::VOTE), "VOTE");
  EXPECT_EQ(mf::reading_status_name(reading_status::BELOW_THRESHOLD), "BELOW_THRESHOLD");
  EXPECT_EQ(mf::reading_status_name(reading_status::NOT_FINITE), "NOT_FINITE");
  EXPECT_EQ(mf::reading_status_name(reading_status::CLAMPED), "CLAMPED");
  EXPECT_EQ(mf::reading_status_name(reading_status::MISSING_CHANNEL), "MISSING_CHANNEL");
  EXPECT_EQ(mf::axis_sign_name(axis_sign::POSITIVE), "POSITIVE");
  EXPECT_EQ(mf::axis_sign_name(axis_sign::NEGATIVE), "NEGATIVE");
  EXPECT_EQ(mf::axis_sign_name(axis_sign::UNDETERMINED), "UNDETERMINED");
  EXPECT_EQ(mf::decision_basis_name(decision_basis::DECIDED), "DECIDED");
  EXPECT_EQ(mf::decision_basis_name(decision_basis::INSUFFICIENT_EVIDENCE), "INSUFFICIENT_EVIDENCE");
  EXPECT_EQ(mf::decision_basis_name(decision_basis::CONFLICTING_EVIDENCE), "CONFLICTING_EVIDENCE");
  EXPECT_EQ(mf::system_classification_name(system_classification::ISO), "ISO");
  EXPECT_EQ(mf::system_classification_name(system_classification::SAE), "SAE");
  EXPECT_EQ(mf::system_classification_name(system_classification::MIXED), "MIXED");
}

TEST(coordinate_identification, Assemble_Readings_DecisionsClassificationReferenceAndWarnings) {
  std::vector<mf::indicator_reading> readings;
  readings.push_back(table_reading(FZ0, 1.0, 4000.0));
  readings.push_back(table_reading(D_FY_D_SA, 1.0, -5.0e4));
  Eigen::VectorXd reference(2);
  reference << 4000.0, 16.6;
  const mf::coordinate_identification result = mf::assemble_identification(std::move(readings), reference);
  ASSERT_EQ(result.readings.size(), 2U);
  EXPECT_EQ(result.vertical.sign, axis_sign::POSITIVE);
  EXPECT_EQ(result.vertical.basis, decision_basis::DECIDED);
  EXPECT_EQ(result.lateral.sign, axis_sign::NEGATIVE);
  EXPECT_EQ(result.lateral.basis, decision_basis::DECIDED);
  EXPECT_EQ(result.effective.vertical, axis_sign::POSITIVE);
  EXPECT_EQ(result.effective.lateral, axis_sign::NEGATIVE);
  EXPECT_EQ(result.classification, system_classification::MIXED);
  ASSERT_EQ(result.reference_input.size(), 2);
  EXPECT_EQ(bits(result.reference_input(0)), bits(4000.0));
  expect_one(result.warnings, "SA", MIXED_VERTICAL_ISO_LATERAL_SAE);
}

// ---- identification_warnings on hand-built structs ------------------------------------------------------------------

TEST(coordinate_identification, Warnings_None_Empty) {
  const mf::axis_decision vertical = decided(axis_sign::POSITIVE, 1.0, 0.0, 1.0);
  const mf::axis_decision lateral = decided(axis_sign::POSITIVE, 3.0, 0.0, 1.0);
  const mf::coordinate_identification identification =
      build_identification(vertical, lateral, ISO_SIGNS, system_classification::ISO);
  EXPECT_TRUE(mf::identification_warnings(identification).empty());
}

TEST(coordinate_identification, Warnings_VerticalInsufficientLateralDecided_MatchesLateral) {
  const mf::axis_decision lateral = decided(axis_sign::NEGATIVE, 0.0, 3.0, 1.0);
  const mf::coordinate_identification identification =
      build_identification(mf::axis_decision{}, lateral, SAE_SIGNS, system_classification::SAE);
  expect_one(mf::identification_warnings(identification), "FZ", V_NONE_MATCH_L_SAE);
}

TEST(coordinate_identification, Warnings_VerticalInsufficientBothUndetermined_IsoAssumed) {
  const mf::coordinate_identification identification =
      build_identification(mf::axis_decision{}, mf::axis_decision{}, ISO_SIGNS, system_classification::ISO);
  const std::vector<mf::warning> warnings = mf::identification_warnings(identification);
  ASSERT_EQ(warnings.size(), 2U);
  expect_warning_at(warnings, 0, "FZ", V_NONE_ISO);
  expect_warning_at(warnings, 1, "SA", L_NONE_ISO);
}

TEST(coordinate_identification, Warnings_VerticalConflicting_MatchesLateral) {
  const mf::axis_decision lateral = decided(axis_sign::POSITIVE, 3.0, 0.0, 1.0);
  const mf::coordinate_identification identification =
      build_identification(conflicting(0.5), lateral, ISO_SIGNS, system_classification::ISO);
  expect_one(mf::identification_warnings(identification), "FZ", V_CONFLICT_MATCH_L_ISO);
}

TEST(coordinate_identification, Warnings_VerticalConflictingBothUndetermined_IsoAssumed) {
  const mf::coordinate_identification identification =
      build_identification(conflicting(0.5), conflicting(0.55), ISO_SIGNS, system_classification::ISO);
  const std::vector<mf::warning> warnings = mf::identification_warnings(identification);
  ASSERT_EQ(warnings.size(), 2U);
  expect_warning_at(warnings, 0, "FZ", V_CONFLICT_ISO);
  expect_warning_at(warnings, 1, "SA", L_CONFLICT_055_ISO);
}

TEST(coordinate_identification, Warnings_LateralInsufficientVerticalDecided_MatchesVertical) {
  const mf::axis_decision vertical = decided(axis_sign::POSITIVE, 1.25, 0.0, 1.0);
  const mf::coordinate_identification identification =
      build_identification(vertical, mf::axis_decision{}, ISO_SIGNS, system_classification::ISO);
  expect_one(mf::identification_warnings(identification), "SA", LATERAL_UNDETERMINED_ISO);
}

TEST(coordinate_identification, Warnings_LateralInsufficientBothUndetermined_IsoAssumed) {
  const mf::coordinate_identification identification =
      build_identification(conflicting(0.5), mf::axis_decision{}, ISO_SIGNS, system_classification::ISO);
  const std::vector<mf::warning> warnings = mf::identification_warnings(identification);
  ASSERT_EQ(warnings.size(), 2U);
  expect_warning_at(warnings, 0, "FZ", V_CONFLICT_ISO);
  expect_warning_at(warnings, 1, "SA", L_NONE_ISO);
}

TEST(coordinate_identification, Warnings_LateralConflicting_MatchesVertical) {
  const mf::axis_decision vertical = decided(axis_sign::NEGATIVE, 0.0, 1.25, 1.0);
  const mf::coordinate_identification identification =
      build_identification(vertical, conflicting(0.5), SAE_SIGNS, system_classification::SAE);
  expect_one(mf::identification_warnings(identification), "SA", L_CONFLICT_MATCH_V_SAE);
}

TEST(coordinate_identification, Warnings_LateralConflictingBothUndetermined_IsoAssumed) {
  const mf::coordinate_identification identification =
      build_identification(mf::axis_decision{}, conflicting(0.55), ISO_SIGNS, system_classification::ISO);
  const std::vector<mf::warning> warnings = mf::identification_warnings(identification);
  ASSERT_EQ(warnings.size(), 2U);
  expect_warning_at(warnings, 0, "FZ", V_NONE_ISO);
  expect_warning_at(warnings, 1, "SA", L_CONFLICT_055_ISO);
}

TEST(coordinate_identification, Warnings_VerticalDissent_NamesDissentersInOrder) {
  std::vector<mf::indicator_reading> readings;
  readings.push_back(table_reading(FZ0, 1.0, 4000.0));
  // Vote -1: dissent.
  readings.push_back(table_reading(D_FY_D_IA, 0.5, 400.0));
  // Vote -1: the same name again.
  readings.push_back(table_reading(D_FY_D_IA, 1.0, 400.0));
  // Vote -1: dissent.
  readings.push_back(table_reading(MY, 1.0, 2.0));
  // Vote -1 on the other axis.
  readings.push_back(table_reading(D_FY_D_SA, 1.0, -5.0e4));
  // Below the threshold on the same axis.
  readings.push_back(table_reading(D_FY_D_IA, 1.5, 0.0));
  // Vote +1: agrees.
  readings.push_back(table_reading(D_FY_D_IA, 1.5, -400.0));
  const mf::axis_decision vertical = decided(axis_sign::POSITIVE, 1.0, 0.5, 2.0 / 3.0);
  const mf::axis_decision lateral = decided(axis_sign::POSITIVE, 3.0, 0.0, 1.0);
  const mf::coordinate_identification identification =
      build_identification(vertical, lateral, ISO_SIGNS, system_classification::ISO, std::move(readings));
  expect_one(mf::identification_warnings(identification), "FZ", V_DISSENT_ISO);
}

TEST(coordinate_identification, Warnings_LateralDissent_NamesDistinctNames) {
  std::vector<mf::indicator_reading> readings;
  // Vote +1: dissent.
  readings.push_back(table_reading(D_FY_D_SA, 0.5, 5.0e4));
  // Vote +1: the same name again.
  readings.push_back(table_reading(D_FY_D_SA, 1.0, 5.0e4));
  // Vote +1: dissent.
  readings.push_back(table_reading(SECANT_1, 1.0, 5.0e4));
  // Below the threshold.
  readings.push_back(table_reading(SECANT_3, 1.0, 0.0));
  // Vote -1: agrees.
  readings.push_back(table_reading(D_MZ_D_SA, 1.0, 1000.0));
  const mf::axis_decision vertical = decided(axis_sign::NEGATIVE, 0.0, 1.25, 1.0);
  const mf::axis_decision lateral = decided(axis_sign::NEGATIVE, 1.0, 3.0, 0.75);
  const mf::coordinate_identification identification =
      build_identification(vertical, lateral, SAE_SIGNS, system_classification::SAE, std::move(readings));
  expect_one(mf::identification_warnings(identification), "SA", L_DISSENT_SAE);
}

TEST(coordinate_identification, Warnings_MixedPlusMinus_ExactText) {
  const mf::axis_decision vertical = decided(axis_sign::POSITIVE, 1.25, 0.0, 1.0);
  const mf::axis_decision lateral = decided(axis_sign::NEGATIVE, 0.0, 3.0, 1.0);
  const mf::coordinate_identification identification =
      build_identification(vertical, lateral, PLUS_MINUS, system_classification::MIXED);
  expect_one(mf::identification_warnings(identification), "SA", MIXED_VERTICAL_ISO_LATERAL_SAE);
}

TEST(coordinate_identification, Warnings_MixedMinusPlus_ExactText) {
  const mf::axis_decision vertical = decided(axis_sign::NEGATIVE, 0.0, 1.25, 1.0);
  const mf::axis_decision lateral = decided(axis_sign::POSITIVE, 3.0, 0.0, 1.0);
  const mf::coordinate_identification identification =
      build_identification(vertical, lateral, MINUS_PLUS, system_classification::MIXED);
  expect_one(mf::identification_warnings(identification), "SA", MIXED_VERTICAL_SAE_LATERAL_ISO);
}

TEST(coordinate_identification, Warnings_CheckNegative_OnePerDefinitionFirstReading) {
  std::vector<mf::indicator_reading> readings;
  // First negative reading of dFx/dSR.
  readings.push_back(table_reading(D_FX_D_SR, 0.5, -5.0));
  // Negative again: no second warning.
  readings.push_back(table_reading(D_FX_D_SR, 1.0, -6.0));
  // Consistent.
  readings.push_back(table_reading(D_FX_D_SR, 1.5, 7.0));
  readings.push_back(table_reading(FX_SECANT, 0.5, 3.0));
  // First negative reading of the secant.
  readings.push_back(table_reading(FX_SECANT, 1.0, -8.0));
  // Negative again.
  readings.push_back(table_reading(FX_SECANT, 1.5, -9.0));
  // Another axis, negative vote.
  readings.push_back(table_reading(D_FY_D_SA, 1.0, -5.0e4));
  const mf::axis_decision vertical = decided(axis_sign::POSITIVE, 1.25, 0.0, 1.0);
  const mf::axis_decision lateral = decided(axis_sign::POSITIVE, 3.0, 0.0, 1.0);
  const mf::coordinate_identification identification =
      build_identification(vertical, lateral, ISO_SIGNS, system_classification::ISO, std::move(readings));
  const std::string first = check_text("dFx/dSR = -5 N at 0.5 x the nominal load");
  const std::string second = check_text("Fx secant 5 % = -8 N at 1 x the nominal load");
  const std::vector<mf::warning> warnings = mf::identification_warnings(identification);
  ASSERT_EQ(warnings.size(), 2U);
  expect_warning_at(warnings, 0, "SR", first);
  expect_warning_at(warnings, 1, "SR", second);
}

TEST(coordinate_identification, Warnings_Order_AxisAxisMixedChecks) {
  std::vector<mf::indicator_reading> readings;
  // Vote +1 against the negative vertical decision.
  readings.push_back(table_reading(D_FY_D_IA, 1.0, -400.0));
  // Vote -1 against the positive lateral decision.
  readings.push_back(table_reading(D_MZ_D_SA, 1.0, 1000.0));
  // Failed check.
  readings.push_back(table_reading(D_FX_D_SR, 1.0, -5.0));
  const mf::axis_decision vertical = decided(axis_sign::NEGATIVE, 0.25, 1.0, 0.8);
  const mf::axis_decision lateral = decided(axis_sign::POSITIVE, 2.0, 0.5, 0.8);
  const mf::coordinate_identification identification =
      build_identification(vertical, lateral, MINUS_PLUS, system_classification::MIXED, std::move(readings));
  const std::string check = check_text("dFx/dSR = -5 N at 1 x the nominal load");
  const std::string camber = camber_dissent_text("the vertical axis decision (SAE, confidence 0.80)");
  const std::vector<mf::warning> warnings = mf::identification_warnings(identification);
  ASSERT_EQ(warnings.size(), 4U);
  expect_warning_at(warnings, 0, "FZ", camber);
  expect_warning_at(warnings, 1, "SA", DMZ_DISSENT_ISO_080);
  expect_warning_at(warnings, 2, "SA", MIXED_VERTICAL_SAE_LATERAL_ISO);
  expect_warning_at(warnings, 3, "SR", check);
}

// ---- the default table ----------------------------------------------------------------------------------------------

TEST(coordinate_identification, Table_StaticProperties_RuntimeMirror) {
  const std::span<const mf::indicator_definition> table{mf::IDENTIFICATION_INDICATORS};
  EXPECT_EQ(table[0].name, "FZ0");
  EXPECT_TRUE(mf::indicator_names_unique(table));
  EXPECT_TRUE(mf::check_weights_zero(table));
  const double vertical_weight = mf::axis_weight(table, identification_axis::VERTICAL);
  EXPECT_NEAR(vertical_weight, 1.5, 1e-12);
  EXPECT_NEAR(mf::axis_weight(table, identification_axis::LATERAL), 3.0, 1e-12);
  EXPECT_EQ(mf::axis_weight(table, identification_axis::CHECK), 0.0);
  EXPECT_GT(table[0].weight, vertical_weight - table[0].weight);
  EXPECT_GE(table[0].weight / vertical_weight, mf::IDENTIFICATION_MIN_CONFIDENCE);
  std::size_t readings = 0;
  for (const mf::indicator_definition &row : table) {
    readings += row.load_sweep ? mf::IDENTIFICATION_LOAD_SCALES.size() : 1U;
  }
  EXPECT_EQ(readings, 29U);
  EXPECT_EQ(table.size(), 11U);
}

TEST(coordinate_identification, Table_CraftedFailingTables_Detected) {
  const mf::indicator_definition vertical = make_definition(identification_axis::VERTICAL, 1.0, 1.0, 0.0, false);
  const mf::indicator_definition lateral = make_definition(identification_axis::LATERAL, 1.0, 1.0, 0.0, false);
  const mf::indicator_definition weighted = make_definition(identification_axis::CHECK, 1.0, 0.5, 0.0, false);
  const mf::indicator_definition unweighted = make_definition(identification_axis::CHECK, 1.0, 0.0, 0.0, false);

  const std::array<mf::indicator_definition, 2> duplicate{vertical, lateral};
  EXPECT_FALSE(mf::indicator_names_unique(duplicate));
  const std::array<mf::indicator_definition, 1> single{vertical};
  EXPECT_TRUE(mf::indicator_names_unique(single));
  EXPECT_TRUE(mf::indicator_names_unique({}));

  const std::array<mf::indicator_definition, 2> weighted_check{vertical, weighted};
  EXPECT_FALSE(mf::check_weights_zero(weighted_check));
  const std::array<mf::indicator_definition, 2> zero_check{vertical, unweighted};
  EXPECT_TRUE(mf::check_weights_zero(zero_check));
}

// ---- identification of models ---------------------------------------------------------------------------------------

TEST(coordinate_identification, Identify_PacS_MixedVerticalIsoLateralSae) {
  const mf::coordinate_identification id = identify(mf::test::fixture_s());
  ASSERT_EQ(id.readings.size(), 29U);

  const std::vector<mf::indicator_reading> fz0 = readings_named(id, "FZ0");
  ASSERT_EQ(fz0.size(), 1U);
  EXPECT_EQ(fz0[0].status, reading_status::VOTE);
  EXPECT_EQ(fz0[0].vote, 1);
  EXPECT_NEAR(fz0[0].weight, 1.0, 1e-12);
  EXPECT_EQ(bits(fz0[0].value), bits(4000.0));

  expect_readings(id, "dFy/dIA", 3, reading_status::BELOW_THRESHOLD, 0);
  expect_readings(id, "My", 1, reading_status::VOTE, 1);
  const std::vector<mf::indicator_reading> my = readings_named(id, "My");
  EXPECT_NEAR(my[0].weight, 0.25, 1e-12);

  expect_readings(id, "dFy/dSA", 3, reading_status::VOTE, -1);
  expect_readings(id, "Fy secant 1 deg", 3, reading_status::VOTE, -1);
  expect_readings(id, "Fy secant 3 deg", 3, reading_status::VOTE, -1);
  expect_readings(id, "Fy secant 6 deg", 3, reading_status::VOTE, -1);
  expect_readings(id, "Fy odd part 20 deg", 3, reading_status::VOTE, -1);
  expect_readings(id, "dMz/dSA", 3, reading_status::VOTE, -1);
  expect_readings(id, "dFx/dSR", 3, reading_status::VOTE, 1);
  expect_readings(id, "Fx secant 5 %", 3, reading_status::VOTE, 1);

  expect_decided(id, axis_sign::POSITIVE, axis_sign::NEGATIVE);
  EXPECT_NEAR(id.vertical.positive_weight, 1.25, 1e-12);
  EXPECT_EQ(id.vertical.negative_weight, 0.0);
  EXPECT_NEAR(id.lateral.negative_weight, 3.0, 1e-12);
  EXPECT_EQ(id.lateral.positive_weight, 0.0);
  EXPECT_EQ(id.effective.vertical, axis_sign::POSITIVE);
  EXPECT_EQ(id.effective.lateral, axis_sign::NEGATIVE);
  EXPECT_EQ(id.classification, system_classification::MIXED);
  expect_one(id.warnings, "SA", MIXED_VERTICAL_ISO_LATERAL_SAE);
}

TEST(coordinate_identification, Identify_PacSIso_IsoNoWarnings) {
  const mf::coordinate_identification id = identify(mf::test::fixture_s_iso());
  EXPECT_EQ(id.classification, system_classification::ISO);
  expect_decided(id, axis_sign::POSITIVE, axis_sign::POSITIVE);
  expect_readings(id, "FZ0", 1, reading_status::VOTE, 1);
  expect_readings(id, "dFy/dIA", 3, reading_status::VOTE, 1);
  expect_readings(id, "My", 1, reading_status::VOTE, 1);
  expect_readings(id, "dFy/dSA", 3, reading_status::VOTE, 1);
  expect_readings(id, "dMz/dSA", 3, reading_status::VOTE, 1);
  expect_readings(id, "dFx/dSR", 3, reading_status::VOTE, 1);
  EXPECT_NEAR(id.vertical.positive_weight, 1.5, 1e-12);
  EXPECT_NEAR(id.lateral.positive_weight, 3.0, 1e-12);
  EXPECT_TRUE(id.warnings.empty());
  // dFy/dIA = F'z0*PVY3 at the nominal load.
  const std::vector<mf::indicator_reading> camber = readings_named(id, "dFy/dIA");
  EXPECT_NEAR(camber[1].value, -400.0, 1e-3);
}

TEST(coordinate_identification, Identify_PacSSae_SaeNoWarnings) {
  const mf::coordinate_identification id = identify(mf::test::fixture_s_sae());
  EXPECT_EQ(id.classification, system_classification::SAE);
  expect_decided(id, axis_sign::NEGATIVE, axis_sign::NEGATIVE);
  expect_readings(id, "FZ0", 1, reading_status::VOTE, -1);
  expect_readings(id, "dFy/dIA", 3, reading_status::VOTE, -1);
  expect_readings(id, "My", 1, reading_status::VOTE, -1);
  expect_readings(id, "dFy/dSA", 3, reading_status::VOTE, -1);
  expect_readings(id, "dMz/dSA", 3, reading_status::VOTE, -1);
  expect_readings(id, "dFx/dSR", 3, reading_status::VOTE, 1);
  EXPECT_TRUE(id.warnings.empty());
}

TEST(coordinate_identification, Identify_Mf52S52_Mixed) {
  const std::unique_ptr<mf::tire_model> model = make_mf52(mf::test::fixture_s52());
  ASSERT_TRUE(model != nullptr);
  const mf::coordinate_identification id = model->find_coordinate_system();
  EXPECT_EQ(id.classification, system_classification::MIXED);
  expect_decided(id, axis_sign::POSITIVE, axis_sign::NEGATIVE);
  expect_one(id.warnings, "SA", MIXED_VERTICAL_ISO_LATERAL_SAE);
}

TEST(coordinate_identification, Identify_PacSPositivePvy3_VerticalDissentNamesCamberThrust) {
  const mf::coordinate_identification id = identify(mf::test::fixture_s_with(0.1));
  EXPECT_EQ(id.vertical.basis, decision_basis::DECIDED);
  EXPECT_EQ(id.vertical.sign, axis_sign::POSITIVE);
  expect_readings(id, "dFy/dIA", 3, reading_status::VOTE, -1);
  const std::string dissent = camber_dissent_text("the vertical axis decision (ISO, confidence 0.83)");
  ASSERT_EQ(id.warnings.size(), 2U);
  expect_warning_at(id.warnings, 0, "FZ", dissent);
  expect_warning_at(id.warnings, 1, "SA", MIXED_VERTICAL_ISO_LATERAL_SAE);
}

TEST(coordinate_identification, Identify_PacSNegativeQdz1_LateralDissentNamesDmzDsa) {
  const mf::coordinate_identification id = identify(mf::test::fixture_s_with(0.0, -15.0, 20.0, -0.1));
  EXPECT_EQ(id.lateral.basis, decision_basis::DECIDED);
  EXPECT_EQ(id.lateral.sign, axis_sign::NEGATIVE);
  expect_readings(id, "dMz/dSA", 3, reading_status::VOTE, 1);
  ASSERT_EQ(id.warnings.size(), 2U);
  expect_warning_at(id.warnings, 0, "SA", DMZ_DISSENT_SAE_083);
  expect_warning_at(id.warnings, 1, "SA", MIXED_VERTICAL_ISO_LATERAL_SAE);
}

TEST(coordinate_identification, Identify_PacSNegativePkx1_CheckWarnings) {
  const mf::coordinate_identification id = identify(mf::test::fixture_s_with(0.0, -15.0, -20.0));
  expect_readings(id, "dFx/dSR", 3, reading_status::VOTE, -1);
  expect_readings(id, "Fx secant 5 %", 3, reading_status::VOTE, -1);
  ASSERT_EQ(id.warnings.size(), 3U);
  EXPECT_EQ(id.warnings[0].message, MIXED_VERTICAL_ISO_LATERAL_SAE);
  EXPECT_EQ(id.warnings[1].where.key, "SR");
  EXPECT_TRUE(id.warnings[1].message.starts_with("coordinate system: dFx/dSR = -"));
  EXPECT_NE(id.warnings[1].message.find(" N at 0.5 x the nominal load is negative: "), std::string::npos);
  EXPECT_TRUE(id.warnings[1].message.ends_with("; no conversion changes this"));
  EXPECT_EQ(id.warnings[2].where.key, "SR");
  EXPECT_TRUE(id.warnings[2].message.starts_with("coordinate system: Fx secant 5 % = -"));
  EXPECT_NE(id.warnings[2].message.find(" N at 0.5 x the nominal load is negative: "), std::string::npos);
}

TEST(coordinate_identification, Identify_NarrowSlipAngleRange_LargeSecantsClampedLateralDecided) {
  mf::mf5x_parameters parameters = mf::test::fixture_s_rng();
  parameters.limits(mf::mf5x::ALPMIN) = -0.06;
  parameters.limits(mf::mf5x::ALPMAX) = 0.06;
  const mf::coordinate_identification id = identify(parameters);
  expect_readings(id, "Fy secant 6 deg", 3, reading_status::CLAMPED, 0);
  expect_readings(id, "Fy odd part 20 deg", 3, reading_status::CLAMPED, 0);
  expect_readings(id, "dFy/dSA", 3, reading_status::VOTE, -1);
  expect_readings(id, "Fy secant 1 deg", 3, reading_status::VOTE, -1);
  expect_readings(id, "Fy secant 3 deg", 3, reading_status::VOTE, -1);
  expect_readings(id, "dMz/dSA", 3, reading_status::VOTE, -1);
  EXPECT_EQ(id.lateral.basis, decision_basis::DECIDED);
  EXPECT_EQ(id.lateral.sign, axis_sign::NEGATIVE);
}

TEST(coordinate_identification, Identify_ZeroSlipAngleRange_LateralInsufficientClassificationIso) {
  mf::mf5x_parameters parameters = mf::test::fixture_s();
  parameters.limits(mf::mf5x::ALPMIN) = 0.0;
  parameters.limits(mf::mf5x::ALPMAX) = 0.0;
  const mf::coordinate_identification id = identify(parameters);
  for (const mf::indicator_reading &reading : id.readings) {
    if (reading.definition.channel == "SA") {
      EXPECT_EQ(reading.status, reading_status::CLAMPED) << reading.definition.name;
    }
  }
  EXPECT_EQ(id.lateral.basis, decision_basis::INSUFFICIENT_EVIDENCE);
  EXPECT_EQ(id.lateral.sign, axis_sign::UNDETERMINED);
  EXPECT_EQ(id.effective.lateral, axis_sign::POSITIVE);
  EXPECT_EQ(id.classification, system_classification::ISO);
  expect_one(id.warnings, "SA", LATERAL_UNDETERMINED_ISO);
}

TEST(coordinate_identification, Identify_FzminAboveProbeLoads_EverySweptReadingClamped) {
  mf::mf5x_parameters parameters = mf::test::fixture_s();
  parameters.limits(mf::mf5x::FZMIN) = 7000.0;
  parameters.limits(mf::mf5x::FZMAX) = 10000.0;
  const mf::coordinate_identification id = identify(parameters);
  for (const mf::indicator_reading &reading : id.readings) {
    if (reading.definition.form == mf::indicator_form::INPUT_LEVEL) {
      EXPECT_EQ(reading.status, reading_status::VOTE) << reading.definition.name;
    } else {
      EXPECT_EQ(reading.status, reading_status::CLAMPED) << reading.definition.name << " at " << reading.load_scale;
    }
  }
  EXPECT_EQ(id.vertical.sign, axis_sign::POSITIVE);
  EXPECT_EQ(id.lateral.basis, decision_basis::INSUFFICIENT_EVIDENCE);
  EXPECT_EQ(id.classification, system_classification::ISO);
  expect_one(id.warnings, "SA", LATERAL_UNDETERMINED_ISO);
}

TEST(coordinate_identification, Identify_ThrowingModel_DoesNotThrow) {
  const mf::model_options options{.mode = mf::slip_mode::COMBINED, .throw_if_model_failure = true};
  const std::unique_ptr<mf::tire_model> model = make_pac2002(mf::test::fixture_s(), options);
  ASSERT_TRUE(model != nullptr);
  EXPECT_NO_THROW(static_cast<void>(model->find_coordinate_system()));
  const mf::coordinate_identification id = model->find_coordinate_system();
  EXPECT_EQ(id.readings.size(), 29U);
}

TEST(coordinate_identification, Identify_ReferenceInput_EchoedBitwise) {
  const std::unique_ptr<mf::tire_model> model = make_pac2002(mf::test::fixture_s());
  ASSERT_TRUE(model != nullptr);
  const mf::coordinate_identification id = model->find_coordinate_system();
  const Eigen::VectorXd reference = model->reference_input();
  ASSERT_EQ(id.reference_input.size(), reference.size());
  for (Eigen::Index i = 0; i < reference.size(); ++i) {
    EXPECT_EQ(bits(id.reference_input(i)), bits(reference(i))) << "entry " << i;
  }
}

// ---- linear stub with crafted tables --------------------------------------------------------------------------------

TEST(coordinate_identification, Stub_LateralTie_ConflictingMatchesVertical) {
  const linear_stub stub(1000.0, 1000.0, true);
  const mf::indicator_definition positive_row = stub_lateral_row(1.0, 0.5);
  const mf::indicator_definition negative_row = stub_lateral_row(-1.0, 0.5);
  const std::array<mf::indicator_definition, 3> table{mf::IDENTIFICATION_INDICATORS[0], positive_row, negative_row};
  const mf::coordinate_identification id = stub.find_coordinate_system(table);
  ASSERT_EQ(id.readings.size(), 3U);
  EXPECT_EQ(id.vertical.sign, axis_sign::POSITIVE);
  EXPECT_EQ(id.lateral.basis, decision_basis::CONFLICTING_EVIDENCE);
  EXPECT_EQ(id.lateral.confidence, 0.5);
  EXPECT_EQ(id.effective.lateral, axis_sign::POSITIVE);
  EXPECT_EQ(id.classification, system_classification::ISO);
  expect_one(id.warnings, "SA", L_CONFLICT_MATCH_V_ISO);
}

TEST(coordinate_identification, Stub_OnlyLateralEvidence_VerticalTakesLateralSign) {
  const linear_stub stub(-1000.0, 1000.0, true);
  const std::array<mf::indicator_definition, 1> table{stub_lateral_row(1.0, 0.5)};
  const mf::coordinate_identification id = stub.find_coordinate_system(table);
  EXPECT_EQ(id.lateral.sign, axis_sign::NEGATIVE);
  EXPECT_EQ(id.vertical.basis, decision_basis::INSUFFICIENT_EVIDENCE);
  EXPECT_EQ(id.effective.vertical, axis_sign::NEGATIVE);
  EXPECT_EQ(id.classification, system_classification::SAE);
  expect_one(id.warnings, "FZ", V_NONE_MATCH_L_SAE);
}

TEST(coordinate_identification, Stub_NoEvidence_BothUndeterminedIso) {
  const linear_stub stub(0.0, 1000.0, true);
  const std::array<mf::indicator_definition, 1> table{stub_lateral_row(1.0, 0.5)};
  const mf::coordinate_identification id = stub.find_coordinate_system(table);
  ASSERT_EQ(id.readings.size(), 1U);
  EXPECT_EQ(id.readings[0].status, reading_status::BELOW_THRESHOLD);
  EXPECT_EQ(id.effective.vertical, axis_sign::POSITIVE);
  EXPECT_EQ(id.effective.lateral, axis_sign::POSITIVE);
  EXPECT_EQ(id.classification, system_classification::ISO);
  ASSERT_EQ(id.warnings.size(), 2U);
  expect_warning_at(id.warnings, 0, "FZ", V_NONE_ISO);
  expect_warning_at(id.warnings, 1, "SA", L_NONE_ISO);
}

TEST(coordinate_identification, Stub_NoFzChannel_EveryReadingMissingChannel) {
  const linear_stub stub(1000.0, 0.0, false);
  const mf::coordinate_identification id = stub.find_coordinate_system();
  ASSERT_EQ(id.readings.size(), 29U);
  for (const mf::indicator_reading &reading : id.readings) {
    EXPECT_EQ(reading.status, reading_status::MISSING_CHANNEL) << reading.definition.name;
  }
  EXPECT_EQ(id.vertical.basis, decision_basis::INSUFFICIENT_EVIDENCE);
  EXPECT_EQ(id.lateral.basis, decision_basis::INSUFFICIENT_EVIDENCE);
  EXPECT_EQ(id.classification, system_classification::ISO);
  EXPECT_EQ(id.warnings.size(), 2U);
}

// ---- metamorphic ----------------------------------------------------------------------------------------------------

namespace {
  constexpr sign_transforms NO_PLAN{.vertical = false, .lateral = false};
  constexpr sign_transforms PLAN_V{.vertical = true, .lateral = false};
  constexpr sign_transforms PLAN_L{.vertical = false, .lateral = true};
  constexpr sign_transforms PLAN_VL{.vertical = true, .lateral = true};

  /// One of the four states of a base ISO set: ISO, SAE (V), PLUS_MINUS (L) or MINUS_PLUS (V and L).
  struct state_case {
    sign_transforms plan;
    mf::axis_signs signs;
    system_classification classification;
  };

  const std::array<state_case, 4> STATES{{
      {.plan = NO_PLAN, .signs = ISO_SIGNS, .classification = system_classification::ISO},
      {.plan = PLAN_V, .signs = SAE_SIGNS, .classification = system_classification::SAE},
      {.plan = PLAN_L, .signs = PLUS_MINUS, .classification = system_classification::MIXED},
      {.plan = PLAN_VL, .signs = MINUS_PLUS, .classification = system_classification::MIXED},
  }};

  /// The transforms that take the state `signs` to `target`.
  sign_transforms expected_plan(const mf::axis_signs &signs, mf::coordinate_system target) {
    const bool vertical_flips = (signs.vertical == axis_sign::NEGATIVE) != (target == mf::coordinate_system::SAE);
    // After the vertical transform the lateral sign is negated as well.
    const bool lateral_after_negative = (signs.lateral == axis_sign::NEGATIVE) != vertical_flips;
    const bool lateral_flips = lateral_after_negative != (target == mf::coordinate_system::SAE);
    return sign_transforms{.vertical = vertical_flips, .lateral = lateral_flips};
  }

  /// Success iff `actual` is within the reading tolerance of `expected` (both NaN also match).
  void expect_same_value(double actual, double expected, double scale, const std::string &what) {
    if (std::isnan(actual) && std::isnan(expected)) {
      return;
    }
    EXPECT_TRUE(mf::test::near_scaled(actual, expected, READING_REL_TOL, scale)) << what;
  }
} // namespace

TEST(coordinate_identification, Metamorphic_PlausibleIso_FourStatesIdentified) {
  for (const formulation form : ALL_FORMULATIONS) {
    SCOPED_TRACE(formulation_name(form));
    for (std::uint64_t seed = 1; seed <= 70; ++seed) {
      SCOPED_TRACE(std::format("seed {}", seed));
      const mf::mf5x_parameters base = mf::test::random_mf5x_parameters(seed, mf::test::random_profile::PLAUSIBLE_ISO);
      for (const state_case &state : STATES) {
        const mf::mf5x_parameters parameters = transformed_parameters(base, state.plan);
        const mf::coordinate_identification id = identify(form, parameters);
        ASSERT_EQ(id.readings.size(), 29U);
        SCOPED_TRACE(std::format("vertical {} lateral {}", state.plan.vertical, state.plan.lateral));
        EXPECT_EQ(id.classification, state.classification);
        EXPECT_EQ(id.effective.vertical, state.signs.vertical);
        EXPECT_EQ(id.effective.lateral, state.signs.lateral);
        EXPECT_EQ(id.vertical.basis, decision_basis::DECIDED);
        EXPECT_EQ(id.lateral.basis, decision_basis::DECIDED);
      }
    }
  }
}

TEST(coordinate_identification, Metamorphic_ConvertEachStateToEachTarget_IdentifiesAsTargetWithExpectedPlan) {
  for (const formulation form : ALL_FORMULATIONS) {
    SCOPED_TRACE(formulation_name(form));
    for (std::uint64_t seed = 1; seed <= 12; ++seed) {
      SCOPED_TRACE(std::format("seed {}", seed));
      const mf::mf5x_parameters base = mf::test::random_mf5x_parameters(seed, mf::test::random_profile::PLAUSIBLE_ISO);
      for (const state_case &state : STATES) {
        const std::unique_ptr<mf::tire_model> model = make_model(form, transformed_parameters(base, state.plan));
        ASSERT_TRUE(model != nullptr);
        for (const mf::coordinate_system target : {mf::coordinate_system::ISO, mf::coordinate_system::SAE}) {
          const mf::model_conversion conversion = model->convert_coordinate_system(target);
          ASSERT_TRUE(conversion.model != nullptr);
          const sign_transforms expected = expected_plan(state.signs, target);
          EXPECT_EQ(conversion.applied.vertical, expected.vertical) << mf::coordinate_system_name(target);
          EXPECT_EQ(conversion.applied.lateral, expected.lateral) << mf::coordinate_system_name(target);
          EXPECT_EQ(conversion.identification.classification, state.classification);
          const mf::coordinate_identification converted = conversion.model->find_coordinate_system();
          const system_classification expected_class =
              target == mf::coordinate_system::ISO ? system_classification::ISO : system_classification::SAE;
          EXPECT_EQ(converted.classification, expected_class) << mf::coordinate_system_name(target);
          EXPECT_EQ(converted.vertical.basis, decision_basis::DECIDED) << mf::coordinate_system_name(target);
          EXPECT_EQ(converted.lateral.basis, decision_basis::DECIDED) << mf::coordinate_system_name(target);
          EXPECT_GE(converted.vertical.confidence, mf::IDENTIFICATION_MIN_CONFIDENCE);
          EXPECT_GE(converted.lateral.confidence, mf::IDENTIFICATION_MIN_CONFIDENCE);
          for (const mf::warning &item : converted.warnings) {
            EXPECT_EQ(item.message.find("mix conventions"), std::string::npos) << item.message;
            EXPECT_EQ(item.message.find("is undetermined"), std::string::npos) << item.message;
          }
        }
      }
    }
  }
}

TEST(coordinate_identification, Metamorphic_WildReadings_NegateExactlyOnExpectedAxes) {
  for (const formulation form : ALL_FORMULATIONS) {
    SCOPED_TRACE(formulation_name(form));
    for (std::uint64_t seed = 1; seed <= 40; ++seed) {
      SCOPED_TRACE(std::format("seed {}", seed));
      const mf::mf5x_parameters base = mf::test::random_mf5x_parameters(seed, mf::test::random_profile::WILD);
      const mf::coordinate_identification original = identify(form, base);
      const mf::coordinate_identification vertical = identify(form, transformed_parameters(base, PLAN_V));
      const mf::coordinate_identification lateral = identify(form, transformed_parameters(base, PLAN_L));
      ASSERT_EQ(original.readings.size(), 29U);
      ASSERT_EQ(vertical.readings.size(), 29U);
      ASSERT_EQ(lateral.readings.size(), 29U);
      for (std::size_t i = 0; i < original.readings.size(); ++i) {
        const mf::indicator_reading &base_reading = original.readings[i];
        const mf::indicator_reading &under_v = vertical.readings[i];
        const mf::indicator_reading &under_l = lateral.readings[i];
        const std::string what = std::format("{} at {}", base_reading.definition.name, base_reading.load_scale);
        const double scale = std::abs(base_reading.value) + 1.0;
        const double normalised_scale = std::abs(base_reading.normalised) + 1.0;
        EXPECT_EQ(under_v.status, base_reading.status) << what;
        EXPECT_EQ(under_l.status, base_reading.status) << what;
        const bool flips_under_v = base_reading.definition.axis != identification_axis::CHECK;
        const bool flips_under_l = base_reading.definition.axis == identification_axis::LATERAL;
        const double sign_v = flips_under_v ? -1.0 : 1.0;
        const double sign_l = flips_under_l ? -1.0 : 1.0;
        expect_same_value(under_v.value, sign_v * base_reading.value, scale, what + " (V value)");
        expect_same_value(under_v.normalised, sign_v * base_reading.normalised, normalised_scale,
                          what + " (V normalised)");
        expect_same_value(under_l.value, sign_l * base_reading.value, scale, what + " (L value)");
        expect_same_value(under_l.normalised, sign_l * base_reading.normalised, normalised_scale,
                          what + " (L normalised)");
      }
    }
  }
}

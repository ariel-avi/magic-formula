#include "../support/fixtures.hpp"

#include <array>
#include <expected>
#include <filesystem>
#include <gtest/gtest.h>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/io/mf5x_bindings.hpp>
#include <magic_formula/io/model_loader.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_model.hpp>
#include <magic_formula/pac2002_model.hpp>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
  using mf::error_code;
  using mf::io::binding_role;
  using mf::io::load_tire_model_file;
  using mf::io::loaded_model;
  using mf::test::bits;
  using mf::test::data_file;
  using coeffs = mf::mf5x_coefficients;
  using load_result = std::expected<loaded_model, mf::error>;

  /// One file of tests/data and the way loading it must end.
  struct expected_file {
    /// File name inside tests/data.
    std::string_view name;
    /// The error code the load must fail with; no value when the file must load.
    std::optional<error_code> failure;
  };

  // clang-format off
  /// All .tir files of tests/data and their documented outcome (tests/data/README.md).
  constexpr std::array<expected_file, 8> EXPECTED_FILES{
      expected_file{.name = "pac2002_S.tir", .failure = std::nullopt},
      expected_file{.name = "mf52_S52.tir", .failure = std::nullopt},
      expected_file{.name = "mf52_user_pff.tir", .failure = std::nullopt},
      expected_file{.name = "pac2002_S_rng.tir", .failure = std::nullopt},
      expected_file{.name = "pac2002_S_ell.tir", .failure = std::nullopt},
      expected_file{.name = "pac2002_S_part.tir", .failure = error_code::MISSING_KEY},
      expected_file{.name = "mf61_unsupported.tir", .failure = error_code::UNSUPPORTED_MODEL},
      expected_file{.name = "lexical_mix.tir", .failure = std::nullopt},
  };

  /// Keys of fixture S that no code reads: the header keys and the scaling factors that PAC2002 does not use.
  const std::vector<std::string> S_IGNORED_KEYS{"FILE_TYPE", "FILE_VERSION", "FILE_FORMAT", "LCZ", "LSGKP", "LSGAL", "LGYR"};

  /// The keys of mf52_user_pff.tir that no code reads: S_IGNORED_KEYS plus USER_SUB_ID (after FILE_FORMAT, file order).
  const std::vector<std::string> USER_PFF_IGNORED_KEYS{"FILE_TYPE", "FILE_VERSION", "FILE_FORMAT", "USER_SUB_ID", "LCZ", "LSGKP", "LSGAL", "LGYR"};

  /// The keys of lexical_mix.tir that no code reads: S_IGNORED_KEYS plus TIRE_DESCRIPTION and [INERTIA] MASS.
  const std::vector<std::string> LEXICAL_IGNORED_KEYS{"FILE_TYPE", "FILE_VERSION", "FILE_FORMAT", "TIRE_DESCRIPTION", "MASS",
                                                      "LCZ", "LSGKP", "LSGAL", "LGYR"};
  // clang-format on

  /// Coefficients returned when a model is not an MF 5.x model (the test fails separately).
  const coeffs NO_COEFFICIENTS{};

  /// Loads a file of tests/data.
  load_result load_fixture(std::string_view name) {
    return load_tire_model_file(data_file(name));
  }

  /// "loaded" or the error text, for assertion messages.
  std::string describe(const load_result &result) {
    return result.has_value() ? std::string("loaded") : mf::to_string(result.error());
  }

  /// The common bindings followed by the camber bindings.
  std::vector<mf::io::coefficient_binding> all_bindings() {
    const std::span<const mf::io::coefficient_binding> common = mf::io::mf5x_common_bindings();
    const std::span<const mf::io::coefficient_binding> camber = mf::io::pac2002_camber_bindings();
    std::vector<mf::io::coefficient_binding> all(common.begin(), common.end());
    all.insert(all.end(), camber.begin(), camber.end());
    return all;
  }

  /// All 117 members bit-equal (the documented exception for identity vectors).
  void expect_same_coefficients(const coeffs &actual, const coeffs &expected) {
    for (const mf::io::coefficient_binding &binding : all_bindings()) {
      EXPECT_EQ(bits(actual.*binding.member), bits(expected.*binding.member)) << binding.key;
    }
    EXPECT_EQ(bits(actual.LONGVL), bits(expected.LONGVL)) << "LONGVL";
  }

  /// Everything except the 20 R* keys equals fixture S; the R* keys are absent and stay 0.
  void expect_fixture_s_without_combined(const coeffs &actual) {
    const mf::mf5x_parameters fixture_parameters = mf::test::fixture_s();
    const coeffs &fixture = fixture_parameters.coefficients;
    for (const mf::io::coefficient_binding &binding : all_bindings()) {
      const double expected = binding.role == binding_role::COMBINED ? 0.0 : fixture.*binding.member;
      EXPECT_EQ(bits(actual.*binding.member), bits(expected)) << binding.key;
    }
  }

  /// The coefficients of a loaded MF 5.x model.
  const coeffs &coefficients_of(const loaded_model &loaded) {
    const auto *const model = dynamic_cast<const mf::mf5x_model *>(loaded.model.get());
    if (model == nullptr) {
      ADD_FAILURE() << "the loaded model is not an MF 5.x model";
      return NO_COEFFICIENTS;
    }
    return model->coefficients();
  }

  /// The loaded model as a PAC2002 model, or nullptr.
  const mf::pac2002_model *pac2002_of(const loaded_model &loaded) {
    return dynamic_cast<const mf::pac2002_model *>(loaded.model.get());
  }

  /// True when both intervals are absent, or both are present with bit-equal bounds.
  bool same_interval(const std::optional<mf::interval> &actual, const std::optional<mf::interval> &expected) {
    if (!actual.has_value() || !expected.has_value()) {
      return actual.has_value() == expected.has_value();
    }
    return bits(actual->lower) == bits(expected->lower) && bits(actual->upper) == bits(expected->upper);
  }

  /// FITTYP, USE_MODE and TYRESIDE of the model equal those of the code fixture.
  void expect_same_metadata(const mf::model_metadata &actual, const mf::model_metadata &expected) {
    EXPECT_EQ(actual.fittyp, expected.fittyp);
    EXPECT_EQ(actual.use_mode, expected.use_mode);
    EXPECT_EQ(actual.side, expected.side);
  }

  /// VXLOW and the four ranges of the model equal those of the code fixture.
  void expect_same_limits(const mf::operating_limits &actual, const mf::operating_limits &expected) {
    EXPECT_EQ(bits(actual.vxlow_mps), bits(expected.vxlow_mps));
    EXPECT_TRUE(same_interval(actual.fz_n, expected.fz_n));
    EXPECT_TRUE(same_interval(actual.kappa, expected.kappa));
    EXPECT_TRUE(same_interval(actual.alpha_rad, expected.alpha_rad));
    EXPECT_TRUE(same_interval(actual.gamma_rad, expected.gamma_rad));
  }

  /// Names of the .tir files in tests/data.
  std::set<std::string> tir_files_in_data_dir() {
    std::set<std::string> names;
    for (const auto &entry : std::filesystem::directory_iterator(std::filesystem::path{MF_TEST_DATA_DIR})) {
      if (entry.path().extension() == ".tir") {
        names.insert(entry.path().filename().string());
      }
    }
    return names;
  }

  /// Loads one file of tests/data and checks that it ends as documented.
  void expect_outcome(const expected_file &file) {
    const load_result loaded = load_fixture(file.name);
    if (file.failure.has_value()) {
      ASSERT_FALSE(loaded.has_value()) << file.name;
      EXPECT_EQ(loaded.error().code, *file.failure) << file.name << ": " << loaded.error().message;
    } else {
      EXPECT_TRUE(loaded.has_value()) << file.name << ": " << describe(loaded);
    }
  }
} // namespace

TEST(data_fixtures, PacS_LoadsAsFixtureS) {
  const load_result loaded = load_fixture("pac2002_S.tir");
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  const mf::mf5x_parameters expected = mf::test::fixture_s();
  EXPECT_EQ(loaded->model->name(), "PAC2002");
  expect_same_coefficients(coefficients_of(*loaded), expected.coefficients);
  expect_same_metadata(loaded->model->metadata(), expected.metadata);
  expect_same_limits(loaded->model->limits(), expected.limits);
  const mf::pac2002_model *const pac2002 = pac2002_of(*loaded);
  ASSERT_NE(pac2002, nullptr);
  EXPECT_EQ(pac2002->method(), mf::combined_method::COSINE);
  EXPECT_TRUE(loaded->warnings.empty());
  EXPECT_EQ(loaded->ignored_keys, S_IGNORED_KEYS);
}

TEST(data_fixtures, Mf52S52_LoadsAsFixtureS52) {
  const load_result loaded = load_fixture("mf52_S52.tir");
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  const mf::mf5x_parameters expected = mf::test::fixture_s52();
  EXPECT_EQ(loaded->model->name(), "MF 5.2");
  expect_same_coefficients(coefficients_of(*loaded), expected.coefficients);
  EXPECT_EQ(bits(coefficients_of(*loaded).LGAX), bits(1.0));
  EXPECT_EQ(bits(coefficients_of(*loaded).LGAY), bits(1.0));
  EXPECT_EQ(bits(coefficients_of(*loaded).LGAZ), bits(1.0));
  expect_same_metadata(loaded->model->metadata(), expected.metadata);
  EXPECT_EQ(loaded->model->metadata().fittyp, std::optional<int>{6});
  expect_same_limits(loaded->model->limits(), expected.limits);

  // The PAC2002-only camber factors of the file give one warning at the first of them (line 34).
  ASSERT_EQ(loaded->warnings.size(), 1U);
  EXPECT_EQ(loaded->warnings[0].message, "MF 5.2 ignores parameters that are not part of MF 5.2: LGAX, LGAY, LGAZ");
  EXPECT_EQ(loaded->warnings[0].where.file, data_file("mf52_S52.tir").string());
  EXPECT_EQ(loaded->warnings[0].where.key, "LGAX");
  EXPECT_EQ(loaded->warnings[0].where.line, 34U);
  EXPECT_EQ(loaded->ignored_keys, S_IGNORED_KEYS);
}

TEST(data_fixtures, Mf52UserPff_LoadsAsMf52WithoutWarnings) {
  // PROPERTY_FILE_FORMAT = 'USER' (TNO MF-Tyre ADAMS form) routes by FITTYP 6.
  const load_result loaded = load_fixture("mf52_user_pff.tir");
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  const mf::mf5x_parameters expected = mf::test::fixture_s52();
  EXPECT_EQ(loaded->model->name(), "MF 5.2");
  expect_same_coefficients(coefficients_of(*loaded), expected.coefficients);
  expect_same_metadata(loaded->model->metadata(), expected.metadata);
  expect_same_limits(loaded->model->limits(), expected.limits);
  EXPECT_TRUE(loaded->warnings.empty());
  // USER_SUB_ID needs no handling: it is only listed as an ignored key.
  EXPECT_EQ(loaded->ignored_keys, USER_PFF_IGNORED_KEYS);
}

TEST(data_fixtures, PacSRng_LimitsEqualFixture) {
  const load_result loaded = load_fixture("pac2002_S_rng.tir");
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  const mf::mf5x_parameters expected = mf::test::fixture_s_rng();
  expect_same_limits(loaded->model->limits(), expected.limits);
  EXPECT_TRUE(loaded->model->limits().fz_n.has_value());
  EXPECT_TRUE(loaded->model->limits().kappa.has_value());
  EXPECT_TRUE(loaded->model->limits().alpha_rad.has_value());
  EXPECT_TRUE(loaded->model->limits().gamma_rad.has_value());
  expect_same_coefficients(coefficients_of(*loaded), expected.coefficients);
  EXPECT_TRUE(loaded->warnings.empty());
}

TEST(data_fixtures, PacSEll_SelectsEllipseWithWarning) {
  const load_result loaded = load_fixture("pac2002_S_ell.tir");
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  const mf::pac2002_model *const pac2002 = pac2002_of(*loaded);
  ASSERT_NE(pac2002, nullptr);
  EXPECT_EQ(pac2002->method(), mf::combined_method::FRICTION_ELLIPSE);

  expect_fixture_s_without_combined(coefficients_of(*loaded));

  ASSERT_EQ(loaded->warnings.size(), 1U);
  EXPECT_EQ(loaded->warnings[0].message, "no combined-slip R* coefficients: using the friction-ellipse method");
}

TEST(data_fixtures, PacSPart_FailsNamingRhx1) {
  // V23
  const load_result loaded = load_fixture("pac2002_S_part.tir");
  ASSERT_FALSE(loaded.has_value());
  EXPECT_EQ(loaded.error().code, error_code::MISSING_KEY);
  EXPECT_EQ(loaded.error().where.key, "RHX1");
  EXPECT_EQ(loaded.error().where.file, data_file("pac2002_S_part.tir").string());
  EXPECT_TRUE(loaded.error().message.contains("RHX1")) << loaded.error().message;
}

TEST(data_fixtures, Mf61Unsupported_Fails) {
  const load_result loaded = load_fixture("mf61_unsupported.tir");
  ASSERT_FALSE(loaded.has_value());
  EXPECT_EQ(loaded.error().code, error_code::UNSUPPORTED_MODEL);
  EXPECT_EQ(loaded.error().where.key, "FITTYP");
  EXPECT_EQ(loaded.error().where.file, data_file("mf61_unsupported.tir").string());
  EXPECT_EQ(loaded.error().where.line, 8U);
  EXPECT_TRUE(loaded.error().message.contains("FITTYP 61")) << loaded.error().message;
}

TEST(data_fixtures, LexicalMix_EqualsFixtureS) {
  const load_result loaded = load_fixture("lexical_mix.tir");
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  const mf::mf5x_parameters expected = mf::test::fixture_s();
  EXPECT_EQ(loaded->model->name(), "PAC2002");
  expect_same_coefficients(coefficients_of(*loaded), expected.coefficients);
  expect_same_metadata(loaded->model->metadata(), expected.metadata);
  expect_same_limits(loaded->model->limits(), expected.limits);
  EXPECT_TRUE(loaded->warnings.empty());
  EXPECT_EQ(loaded->ignored_keys, LEXICAL_IGNORED_KEYS);
}

TEST(data_fixtures, EveryTirFile_HasExpectedOutcome) {
  std::set<std::string> expected_names;
  for (const expected_file &file : EXPECTED_FILES) {
    expected_names.emplace(file.name);
  }
  EXPECT_EQ(tir_files_in_data_dir(), expected_names);

  for (const expected_file &file : EXPECTED_FILES) {
    expect_outcome(file);
  }
}

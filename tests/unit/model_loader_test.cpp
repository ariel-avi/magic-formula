#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <gtest/gtest.h>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/io/mf5x_bindings.hpp>
#include <magic_formula/io/model_loader.hpp>
#include <magic_formula/io/text_file.hpp>
#include <magic_formula/io/tir_document.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/mf5x_model.hpp>
#include <magic_formula/pac2002_model.hpp>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
  using mf::error_code;
  using mf::io::load_tire_model;
  using mf::io::load_tire_model_file;
  using mf::io::loaded_model;
  using mf::io::tir_document;
  using mf::test::bits;
  using mf::test::data_file;
  using coeffs = mf::mf5x_coefficients;
  using load_result = std::expected<loaded_model, mf::error>;

  /// File name given to every parsed variant; errors and warnings must carry it.
  constexpr std::string_view VARIANT_NAME = "variant.tir";

  // clang-format off
  /// The REQUIRED keys.
  constexpr std::array<std::string_view, 9> REQUIRED_KEYS{
      "FNOMIN", "UNLOADED_RADIUS", "PCX1", "PDX1", "PKX1", "PCY1", "PDY1", "PKY1", "PKY2",
  };

  /// The 20 combined-slip R* keys.
  constexpr std::array<std::string_view, 20> COMBINED_KEYS{
      "RBX1", "RBX2", "RCX1", "REX1", "REX2", "RHX1", "RBY1", "RBY2", "RBY3", "RCY1",
      "REY1", "REY2", "RHY1", "RHY2", "RVY1", "RVY2", "RVY3", "RVY4", "RVY5", "RVY6",
  };
  // clang-format on

  /// The camber scaling factors that only PAC2002 has.
  constexpr std::array<std::string_view, 3> CAMBER_KEYS{"LGAX", "LGAY", "LGAZ"};

  /// Warning of the friction-ellipse selection.
  constexpr std::string_view ELLIPSE_MESSAGE = "no combined-slip R* coefficients: using the friction-ellipse method";

  /// Format string of the USE_MODE warning.
  constexpr std::string_view USE_MODE_MESSAGE =
      "USE_MODE {} is echoed only; combined/uncombined is selected by the caller";

  /// One row of the LONGVL rule: the text edits and the expected outcome.
  struct longvl_case {
    /// Short description, for failure messages.
    std::string_view label;
    /// New value of LONGVL; empty removes the key.
    std::string_view longvl;
    /// New value of QSY3; empty keeps the fixture value 0.
    std::string_view qsy3;
    /// New value of QSY4; empty keeps the fixture value 0.
    std::string_view qsy4;
    /// True when MISSING_KEY is expected.
    bool missing;
    /// Expected LONGVL when the load succeeds.
    double expected;
  };

  // clang-format off
  constexpr std::array<longvl_case, 5> LONGVL_CASES{
      longvl_case{.label = "present", .longvl = "20", .qsy3 = "", .qsy4 = "", .missing = false, .expected = 20.0},
      longvl_case{.label = "present with QSY3", .longvl = "20", .qsy3 = "0.1", .qsy4 = "", .missing = false,
                  .expected = 20.0},
      longvl_case{.label = "absent, QSY3 = QSY4 = 0", .longvl = "", .qsy3 = "", .qsy4 = "", .missing = false,
                  .expected = 1.0},
      longvl_case{.label = "absent, QSY3 != 0", .longvl = "", .qsy3 = "0.1", .qsy4 = "", .missing = true,
                  .expected = 0.0},
      longvl_case{.label = "absent, QSY4 != 0", .longvl = "", .qsy3 = "", .qsy4 = "0.01", .missing = true,
                  .expected = 0.0},
  };
  // clang-format on

  /// One LOWER/UPPER pair of the limits vector and the .tir section that holds it.
  struct limit_pair {
    /// Section header line.
    std::string_view section;
    /// Key of the lower bound.
    std::string_view lower_key;
    /// Key of the upper bound.
    std::string_view upper_key;
    /// Position of the lower bound in the limits vector.
    Eigen::Index lower_index;
    /// Position of the upper bound in the limits vector.
    Eigen::Index upper_index;
    /// A lower bound that is valid below `upper_value`.
    double lower_value;
    /// An upper bound above `lower_value`.
    double upper_value;
  };

  constexpr std::array<limit_pair, 4> LIMIT_PAIRS{
      limit_pair{.section = "[VERTICAL_FORCE_RANGE]",
                 .lower_key = "FZMIN",
                 .upper_key = "FZMAX",
                 .lower_index = mf::mf5x::FZMIN,
                 .upper_index = mf::mf5x::FZMAX,
                 .lower_value = 200.0,
                 .upper_value = 10000.0},
      limit_pair{.section = "[LONG_SLIP_RANGE]",
                 .lower_key = "KPUMIN",
                 .upper_key = "KPUMAX",
                 .lower_index = mf::mf5x::KPUMIN,
                 .upper_index = mf::mf5x::KPUMAX,
                 .lower_value = -0.5,
                 .upper_value = 0.75},
      limit_pair{.section = "[SLIP_ANGLE_RANGE]",
                 .lower_key = "ALPMIN",
                 .upper_key = "ALPMAX",
                 .lower_index = mf::mf5x::ALPMIN,
                 .upper_index = mf::mf5x::ALPMAX,
                 .lower_value = -1.25,
                 .upper_value = 1.5},
      limit_pair{.section = "[INCLINATION_ANGLE_RANGE]",
                 .lower_key = "CAMMIN",
                 .upper_key = "CAMMAX",
                 .lower_index = mf::mf5x::CAMMIN,
                 .upper_index = mf::mf5x::CAMMAX,
                 .lower_value = -0.2,
                 .upper_value = 0.25},
  };

  class model_loader_required : public ::testing::TestWithParam<std::string_view> {};

  class model_loader_longvl : public ::testing::TestWithParam<longvl_case> {};

  // clang-format off
  /// Keys of fixture S that no code reads: the header keys and the scaling factors that PAC2002 does not use.
  const std::vector<std::string> S_IGNORED_KEYS{"FILE_TYPE", "FILE_VERSION", "FILE_FORMAT", "LCZ", "LSGKP", "LSGAL", "LGYR"};
  // clang-format on

  /// Coefficients returned when a model is not an MF 5.x model (the test fails separately).
  const coeffs NO_COEFFICIENTS{};

  /// The text of a file in tests/data; a read failure is a test failure and yields an empty string.
  std::string read_fixture(std::string_view name) {
    const std::expected<std::string, mf::error> text = mf::io::read_text_file(data_file(name));
    if (!text.has_value()) {
      ADD_FAILURE() << mf::to_string(text.error());
      return {};
    }
    return *text;
  }

  /// Splits `text` at '\n' (no terminator kept, no empty piece after the final '\n').
  std::vector<std::string> split_lines(std::string_view text) {
    std::vector<std::string> lines;
    std::size_t begin = 0;
    while (begin < text.size()) {
      const std::size_t end = text.find('\n', begin);
      const std::size_t stop = end == std::string_view::npos ? text.size() : end;
      lines.emplace_back(text.substr(begin, stop - begin));
      begin = stop + 1;
    }
    return lines;
  }

  /// Joins lines, each followed by '\n'.
  std::string join_lines(const std::vector<std::string> &lines) {
    std::string text;
    for (const std::string &line : lines) {
      text += line;
      text += '\n';
    }
    return text;
  }

  /// Removes the line "<key> = ..."; exactly one such line must exist.
  std::string remove_key(std::string_view text, std::string_view key) {
    std::vector<std::string> lines = split_lines(text);
    const std::string prefix = std::format("{} =", key);
    const auto is_key_line = [&prefix](const std::string &line) { return line.starts_with(prefix); };
    const std::size_t removed = std::erase_if(lines, is_key_line);
    EXPECT_EQ(removed, 1U) << key;
    return join_lines(lines);
  }

  /// Replaces the value of the line "<key> = ..."; exactly one such line must exist.
  std::string set_value(std::string_view text, std::string_view key, std::string_view value) {
    std::vector<std::string> lines = split_lines(text);
    const std::string prefix = std::format("{} =", key);
    std::size_t replaced = 0;
    for (std::string &line : lines) {
      if (line.starts_with(prefix)) {
        line = std::format("{} = {}", key, value);
        ++replaced;
      }
    }
    EXPECT_EQ(replaced, 1U) << key;
    return join_lines(lines);
  }

  /// Inserts `added` (it may hold several lines) after the first line that equals `anchor`.
  std::string insert_after(std::string_view text, std::string_view anchor, std::string_view added) {
    std::vector<std::string> lines = split_lines(text);
    const std::string anchor_line(anchor);
    const auto found = std::ranges::find(lines, anchor_line);
    if (found == lines.end()) {
      ADD_FAILURE() << "anchor line not found: " << anchor;
      return std::string(text);
    }
    lines.insert(found + 1, std::string(added));
    return join_lines(lines);
  }

  /// `text` followed by `extra`.
  std::string append(std::string_view text, std::string_view extra) {
    std::string result(text);
    result += extra;
    return result;
  }

  /// 1-based line number of the line "<key> = ..." in `text`.
  std::size_t line_of(std::string_view text, std::string_view key) {
    const std::vector<std::string> lines = split_lines(text);
    const std::string prefix = std::format("{} =", key);
    const auto is_key_line = [&prefix](const std::string &line) { return line.starts_with(prefix); };
    const auto found = std::ranges::find_if(lines, is_key_line);
    if (found == lines.end()) {
      ADD_FAILURE() << "key not found: " << key;
      return 0;
    }
    return static_cast<std::size_t>(found - lines.begin()) + 1U;
  }

  /// Fixture S (PAC2002) as file text.
  std::string s_text() {
    return read_fixture("pac2002_S.tir");
  }

  /// Fixture S52 (MF 5.2) as file text with the PAC2002-only keys LGAX, LGAY and LGAZ removed, so that it loads
  /// without warnings.
  std::string mf52_clean_text() {
    std::string text = read_fixture("mf52_S52.tir");
    for (const std::string_view key : CAMBER_KEYS) {
      text = remove_key(text, key);
    }
    return text;
  }

  /// Parses `text` under the name "variant.tir" and loads it with `options`. A parse failure is a test failure.
  load_result load_text(std::string_view text, const mf::model_options &options = {}) {
    const std::expected<tir_document, mf::error> document = tir_document::parse(text, std::string(VARIANT_NAME));
    if (!document.has_value()) {
      ADD_FAILURE() << mf::to_string(document.error());
      return mf::make_error(error_code::SYNTAX, "test variant does not parse");
    }
    return load_tire_model(*document, options);
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

  /// Both vectors have the same size and bit-equal elements (infinities and signs match exactly).
  void expect_same_limits(const Eigen::VectorXd &actual, const Eigen::VectorXd &expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (Eigen::Index i = 0; i < actual.size(); ++i) {
      EXPECT_EQ(bits(actual(i)), bits(expected(i))) << "limit index " << i;
    }
  }

  /// The pair at `lower_index` holds -inf and +inf (bit-equal).
  void expect_pair_unbounded(const Eigen::VectorXd &limits, Eigen::Index lower_index) {
    EXPECT_EQ(bits(limits(lower_index)), bits(-mf::mf5x::INF)) << "lower at " << lower_index;
    EXPECT_EQ(bits(limits(lower_index + 1)), bits(mf::mf5x::INF)) << "upper at " << lower_index + 1;
  }

  /// Every pair of the limits vector is unbounded.
  void expect_all_pairs_unbounded(const Eigen::VectorXd &limits) {
    for (const limit_pair &pair : LIMIT_PAIRS) {
      expect_pair_unbounded(limits, pair.lower_index);
    }
  }

  /// Fixture S with the section of `pair` and the given key lines appended.
  std::string with_range_lines(const limit_pair &pair, std::string_view lines) {
    return append(s_text(), std::format("{}\n{}", pair.section, lines));
  }

  /// Checks code, key, line and file of an error.
  void expect_error(const load_result &result, error_code code, std::string_view key, std::size_t line) {
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, code) << result.error().message;
    EXPECT_EQ(result.error().where.key, key) << result.error().message;
    EXPECT_EQ(result.error().where.line, line) << result.error().message;
    EXPECT_EQ(result.error().where.file, VARIANT_NAME) << result.error().message;
  }

  /// Checks that the loaded model has exactly one warning with this message, key and line.
  void expect_single_warning(const loaded_model &loaded, std::string_view message, std::string_view key,
                             std::size_t line) {
    ASSERT_EQ(loaded.warnings.size(), 1U);
    EXPECT_EQ(loaded.warnings[0].message, message);
    EXPECT_EQ(loaded.warnings[0].where.key, key);
    EXPECT_EQ(loaded.warnings[0].where.line, line);
    EXPECT_EQ(loaded.warnings[0].where.file, VARIANT_NAME);
  }

  /// True when `keys` holds `key`.
  bool has_key(const std::vector<std::string> &keys, std::string_view key) {
    return std::ranges::find(keys, std::string(key)) != keys.end();
  }

  /// All 20 combined-slip R* members of the coefficient set are 0 (the file has none of them).
  void expect_combined_zero(const coeffs &actual) {
    std::size_t count = 0;
    for (const mf::io::coefficient_binding &binding : all_bindings()) {
      if (binding.role == mf::io::binding_role::COMBINED) {
        EXPECT_EQ(bits(actual.*binding.member), bits(0.0)) << binding.key;
        ++count;
      }
    }
    EXPECT_EQ(count, COMBINED_KEYS.size());
  }

  /// Fixture S with LONGVL, QSY3 and QSY4 edited as the case says.
  std::string longvl_variant(const longvl_case &param) {
    std::string text = s_text();
    text = param.longvl.empty() ? remove_key(text, "LONGVL") : set_value(text, "LONGVL", param.longvl);
    if (!param.qsy3.empty()) {
      text = set_value(text, "QSY3", param.qsy3);
    }
    if (!param.qsy4.empty()) {
      text = set_value(text, "QSY4", param.qsy4);
    }
    return text;
  }

  /// The load must fail with MISSING_KEY on LONGVL.
  void expect_longvl_missing(const load_result &result, const longvl_case &param) {
    ASSERT_FALSE(result.has_value()) << param.label;
    expect_error(result, error_code::MISSING_KEY, "LONGVL", 0U);
    EXPECT_EQ(result.error().message, "LONGVL is required when QSY3 or QSY4 is non-zero");
  }

  /// The load must succeed with the expected LONGVL.
  void expect_longvl_value(const load_result &result, const longvl_case &param) {
    ASSERT_TRUE(result.has_value()) << param.label << ": " << describe(result);
    EXPECT_EQ(bits(coefficients_of(*result).LONGVL), bits(param.expected)) << param.label;
  }
} // namespace

TEST(model_loader, Load_FixtureS_Pac2002CosineNoWarnings) {
  const load_result loaded = load_text(s_text());
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  EXPECT_EQ(loaded->model->name(), "PAC2002");
  expect_same_coefficients(coefficients_of(*loaded), mf::test::fixture_s().coefficients);

  const mf::model_metadata &metadata = loaded->model->metadata();
  EXPECT_FALSE(metadata.fittyp.has_value());
  EXPECT_EQ(metadata.use_mode, std::optional<int>{4});
  EXPECT_EQ(metadata.side, mf::tyre_side::LEFT);

  const Eigen::VectorXd &limits = loaded->model->limits();
  ASSERT_EQ(limits.size(), mf::mf5x::LIMITS_LENGTH);
  EXPECT_EQ(bits(limits(mf::mf5x::VXLOW)), bits(1.0));
  expect_all_pairs_unbounded(limits);

  const mf::pac2002_model *const pac2002 = pac2002_of(*loaded);
  ASSERT_NE(pac2002, nullptr);
  EXPECT_EQ(pac2002->method(), mf::combined_method::COSINE);
  EXPECT_FALSE(pac2002->uses_my_from_shifts());

  EXPECT_TRUE(loaded->warnings.empty());
  EXPECT_EQ(loaded->ignored_keys, S_IGNORED_KEYS);
}

TEST(model_loader, Load_FixtureS52Clean_Mf52NoWarnings) {
  const load_result loaded = load_text(mf52_clean_text());
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  EXPECT_EQ(loaded->model->name(), "MF 5.2");
  expect_same_coefficients(coefficients_of(*loaded), mf::test::fixture_s52().coefficients);
  EXPECT_EQ(loaded->model->metadata().fittyp, std::optional<int>{6});
  EXPECT_TRUE(loaded->warnings.empty());
  EXPECT_EQ(loaded->ignored_keys, S_IGNORED_KEYS);
}

TEST_P(model_loader_required, Load_MissingRequiredKey_ReturnsMissingKey) {
  const std::string_view key = GetParam();
  const load_result result = load_text(remove_key(s_text(), key));
  ASSERT_FALSE(result.has_value()) << key;
  expect_error(result, error_code::MISSING_KEY, key, 0U);
  EXPECT_EQ(result.error().message, std::format("required key {} is missing", key));
}

INSTANTIATE_TEST_SUITE_P(required, model_loader_required, ::testing::ValuesIn(REQUIRED_KEYS));

TEST(model_loader, Load_Mf52MissingWidth_ReturnsMissingKey) {
  const load_result result = load_text(remove_key(mf52_clean_text(), "WIDTH"));
  ASSERT_FALSE(result.has_value());
  expect_error(result, error_code::MISSING_KEY, "WIDTH", 0U);
  EXPECT_EQ(result.error().message, "required key WIDTH is missing for MF 5.2");
}

TEST(model_loader, Load_Mf52MissingRimRadius_ReturnsMissingKey) {
  const load_result result = load_text(remove_key(mf52_clean_text(), "RIM_RADIUS"));
  ASSERT_FALSE(result.has_value());
  expect_error(result, error_code::MISSING_KEY, "RIM_RADIUS", 0U);
  EXPECT_TRUE(result.error().message.contains("MF 5.2")) << result.error().message;
}

TEST(model_loader, Load_Mf52InvalidWidth_ReturnsInvalidNumber) {
  const std::string text = set_value(mf52_clean_text(), "WIDTH", "wide");
  const load_result result = load_text(text);
  ASSERT_FALSE(result.has_value());
  expect_error(result, error_code::INVALID_NUMBER, "WIDTH", line_of(text, "WIDTH"));
}

TEST(model_loader, Load_Pac2002WithoutWidthAndRimRadius_Succeeds) {
  // WIDTH and RIM_RADIUS are required for MF 5.2 only; fixture S has neither.
  const load_result loaded = load_text(s_text());
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
}

TEST(model_loader, Load_ScalingAbsent_DefaultsOneSilently) {
  const load_result loaded = load_text(remove_key(s_text(), "LMUX"));
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  EXPECT_EQ(bits(coefficients_of(*loaded).LMUX), bits(1.0));
  expect_same_coefficients(coefficients_of(*loaded), mf::test::fixture_s().coefficients);
  EXPECT_TRUE(loaded->warnings.empty());
}

TEST(model_loader, Load_CoefficientsAbsent_DefaultZeroWithOneWarning) {
  const load_result loaded = load_text(remove_key(remove_key(s_text(), "PDX2"), "QBZ2"));
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  EXPECT_EQ(bits(coefficients_of(*loaded).PDX2), bits(0.0));
  EXPECT_EQ(bits(coefficients_of(*loaded).QBZ2), bits(0.0));
  ASSERT_EQ(loaded->warnings.size(), 1U);
  EXPECT_EQ(loaded->warnings[0].message, "2 coefficient(s) absent, defaulted to 0: PDX2, QBZ2");
  EXPECT_EQ(loaded->warnings[0].where.file, VARIANT_NAME);
  EXPECT_EQ(loaded->warnings[0].where.line, 0U);
  EXPECT_TRUE(loaded->warnings[0].where.key.empty());
}

TEST(model_loader, Load_NoCombinedKeys_SelectsEllipseWithWarning) {
  const load_result loaded = load_tire_model_file(data_file("pac2002_S_ell.tir"));
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  const mf::pac2002_model *const pac2002 = pac2002_of(*loaded);
  ASSERT_NE(pac2002, nullptr);
  EXPECT_EQ(pac2002->method(), mf::combined_method::FRICTION_ELLIPSE);
  expect_combined_zero(coefficients_of(*loaded));
  ASSERT_EQ(loaded->warnings.size(), 1U);
  EXPECT_EQ(loaded->warnings[0].message, ELLIPSE_MESSAGE);
  EXPECT_FALSE(loaded->warnings[0].where.file.empty());
}

TEST(model_loader, Load_AllCombinedKeysRemovedByText_SelectsEllipse) {
  std::string text = s_text();
  for (const std::string_view key : COMBINED_KEYS) {
    text = remove_key(text, key);
  }
  const load_result loaded = load_text(text);
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  const mf::pac2002_model *const pac2002 = pac2002_of(*loaded);
  ASSERT_NE(pac2002, nullptr);
  EXPECT_EQ(pac2002->method(), mf::combined_method::FRICTION_ELLIPSE);
  expect_single_warning(*loaded, ELLIPSE_MESSAGE, "", 0U);
}

TEST(model_loader, Load_PartialCombined_MissingKeyNamingRhx1) {
  // V23
  const load_result result = load_text(remove_key(s_text(), "RHX1"));
  ASSERT_FALSE(result.has_value());
  expect_error(result, error_code::MISSING_KEY, "RHX1", 0U);
  EXPECT_EQ(result.error().message, "incomplete combined-slip coefficient set; missing: RHX1");
}

TEST(model_loader, Load_PartialCombinedTwoMissing_NamesBothAndKeysTheFirst) {
  const load_result result = load_text(remove_key(remove_key(s_text(), "RVY6"), "RBX1"));
  ASSERT_FALSE(result.has_value());
  expect_error(result, error_code::MISSING_KEY, "RBX1", 0U);
  EXPECT_EQ(result.error().message, "incomplete combined-slip coefficient set; missing: RBX1, RVY6");
}

TEST(model_loader, Load_Mf52PartialCombined_DefaultsWithWarning) {
  // M9
  const load_result loaded = load_text(remove_key(mf52_clean_text(), "RBX1"));
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  EXPECT_EQ(bits(coefficients_of(*loaded).RBX1), bits(0.0));
  expect_single_warning(*loaded, "1 coefficient(s) absent, defaulted to 0: RBX1", "", 0U);
}

TEST(model_loader, Load_Mf52NoCombinedKeys_DefaultsAllWithoutEllipseWarning) {
  std::string text = mf52_clean_text();
  for (const std::string_view key : COMBINED_KEYS) {
    text = remove_key(text, key);
  }
  const load_result loaded = load_text(text);
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  ASSERT_EQ(loaded->warnings.size(), 1U);
  const std::string &message = loaded->warnings[0].message;
  EXPECT_TRUE(message.starts_with("20 coefficient(s) absent, defaulted to 0: RBX1, RBX2")) << message;
  EXPECT_TRUE(message.ends_with("RVY5, RVY6")) << message;
  EXPECT_FALSE(message.contains("ellipse")) << message;
}

TEST(model_loader, Load_Mf52ForeignKeys_WarnConsumedAndForcedOne) {
  // M3
  const std::string text = set_value(read_fixture("mf52_S52.tir"), "LGAX", "0.5");
  const load_result loaded = load_text(text);
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  constexpr std::string_view message = "MF 5.2 ignores parameters that are not part of MF 5.2: LGAX, LGAY, LGAZ";
  expect_single_warning(*loaded, message, "LGAX", line_of(text, "LGAX"));
  EXPECT_FALSE(has_key(loaded->ignored_keys, "LGAX"));
  EXPECT_FALSE(has_key(loaded->ignored_keys, "LGAY"));
  EXPECT_FALSE(has_key(loaded->ignored_keys, "LGAZ"));
  EXPECT_EQ(bits(coefficients_of(*loaded).LGAX), bits(1.0));
  EXPECT_EQ(bits(coefficients_of(*loaded).LGAY), bits(1.0));
  EXPECT_EQ(bits(coefficients_of(*loaded).LGAZ), bits(1.0));
}

TEST(model_loader, Load_Mf52OtherForeignKey_WarnsWithThatKey) {
  const std::string text = append(mf52_clean_text(), "PKY4 = 2\n");
  const load_result loaded = load_text(text);
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  const std::size_t line = line_of(text, "PKY4");
  expect_single_warning(*loaded, "MF 5.2 ignores parameters that are not part of MF 5.2: PKY4", "PKY4", line);
  EXPECT_FALSE(has_key(loaded->ignored_keys, "PKY4"));
}

TEST(model_loader, Load_Mf52EveryForeignKey_WarnsWithThatKeyAndConsumesIt) {
  // The pressure, turn-slip and Q_FYS keys of mf52_foreign_keys() are as foreign as the PAC2002 ones.
  for (const std::string_view key : mf::io::mf52_foreign_keys()) {
    const std::string text = append(mf52_clean_text(), std::format("{} = 1\n", key));
    const load_result loaded = load_text(text);
    ASSERT_TRUE(loaded.has_value()) << key << ": " << describe(loaded);
    const std::string message = std::format("MF 5.2 ignores parameters that are not part of MF 5.2: {}", key);
    expect_single_warning(*loaded, message, key, line_of(text, key));
    EXPECT_FALSE(has_key(loaded->ignored_keys, std::string(key))) << key;
  }
}

TEST(model_loader, Load_Mf52PressureTurnSlipStructuralInertia_OneWarningNamesEach) {
  // One foreign-key warning lists the table keys (table order) and then the keys of the foreign sections.
  constexpr std::string_view foreign_lines = "PPX1 = -4.8571\n"
                                             "PPMX1 = 0\n"
                                             "PDXP1 = 0.4\n"
                                             "QDRP1 = 0\n"
                                             "Q_FYS1 = 0\n"
                                             "[STRUCTURAL]\n"
                                             "LONGITUDINAL_STIFFNESS = 2.5e5\n"
                                             "[INERTIA]\n"
                                             "MASS = 9.3\n";
  const std::string text = append(mf52_clean_text(), foreign_lines);
  const load_result loaded = load_text(text);
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  constexpr std::string_view message =
      "MF 5.2 ignores parameters that are not part of MF 5.2: PPX1, PPMX1, PDXP1, QDRP1, Q_FYS1, "
      "LONGITUDINAL_STIFFNESS, MASS";
  expect_single_warning(*loaded, message, "PPX1", line_of(text, "PPX1"));
  for (const std::string_view key : {"PPX1", "PPMX1", "PDXP1", "QDRP1", "Q_FYS1", "LONGITUDINAL_STIFFNESS", "MASS"}) {
    EXPECT_FALSE(has_key(loaded->ignored_keys, std::string(key))) << key;
  }
  // [UNITS] MASS = 'kg' still validated: the load succeeded and the model is MF 5.2.
  EXPECT_EQ(loaded->model->name(), "MF 5.2");
}

TEST(model_loader, Load_Mf52InertiaMassOnly_WarningIsLocatedAtInertiaMassNotUnitsMass) {
  const std::string base = mf52_clean_text();
  const std::string text = append(base, "[INERTIA]\nMASS = 9.3\n");
  const load_result loaded = load_text(text);
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  // The appended MASS follows the [INERTIA] header, which follows the last line of the base text.
  const std::size_t inertia_mass_line = split_lines(base).size() + 2U;
  expect_single_warning(*loaded, "MF 5.2 ignores parameters that are not part of MF 5.2: MASS", "MASS",
                        inertia_mass_line);
  EXPECT_FALSE(has_key(loaded->ignored_keys, "MASS"));
}

TEST(model_loader, Load_Mf52ContactPatchSection_AllKeysWarnedAndConsumed) {
  const std::string text = append(mf52_clean_text(), "[CONTACT_PATCH]\nELLIPS_SHIFT = 0.1\nENV_C1 = 0.2\n");
  const load_result loaded = load_text(text);
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  constexpr std::string_view message = "MF 5.2 ignores parameters that are not part of MF 5.2: ELLIPS_SHIFT, ENV_C1";
  expect_single_warning(*loaded, message, "ELLIPS_SHIFT", line_of(text, "ELLIPS_SHIFT"));
  EXPECT_FALSE(has_key(loaded->ignored_keys, "ELLIPS_SHIFT"));
  EXPECT_FALSE(has_key(loaded->ignored_keys, "ENV_C1"));
}

TEST(model_loader, Load_Mf52UnitsMassOnly_NoForeignWarning) {
  // MASS in [UNITS] is a unit name, not an [INERTIA] value: nothing is foreign here.
  const load_result loaded = load_text(mf52_clean_text());
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  EXPECT_TRUE(loaded->warnings.empty());
}

TEST(model_loader, Load_Pac2002SameKeys_NoForeignKeyWarningAndKeysIgnored) {
  constexpr std::string_view foreign_lines = "PPX1 = -4.8571\n"
                                             "PDXP1 = 0.4\n"
                                             "Q_FYS1 = 0\n"
                                             "[STRUCTURAL]\n"
                                             "LONGITUDINAL_STIFFNESS = 2.5e5\n"
                                             "[INERTIA]\n"
                                             "MASS = 9.3\n";
  const std::string text = append(s_text(), foreign_lines);
  const load_result loaded = load_text(text);
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  EXPECT_EQ(loaded->model->name(), "PAC2002");
  EXPECT_TRUE(loaded->warnings.empty());
  for (const std::string_view key : {"PPX1", "PDXP1", "Q_FYS1", "LONGITUDINAL_STIFFNESS", "MASS"}) {
    EXPECT_TRUE(has_key(loaded->ignored_keys, std::string(key))) << key;
  }
}

TEST(model_loader, Load_Pac2002CamberScaling_Read) {
  const load_result loaded = load_text(set_value(s_text(), "LGAX", "0.5"));
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  EXPECT_EQ(bits(coefficients_of(*loaded).LGAX), bits(0.5));
  EXPECT_EQ(bits(coefficients_of(*loaded).LGAY), bits(1.0));
  EXPECT_EQ(bits(coefficients_of(*loaded).LGAZ), bits(1.0));
  EXPECT_TRUE(loaded->warnings.empty());
}

TEST(model_loader, Load_Pac2002CamberBadNumber_ReturnsInvalidNumber) {
  const std::string text = set_value(s_text(), "LGAZ", "abc");
  const load_result result = load_text(text);
  ASSERT_FALSE(result.has_value());
  expect_error(result, error_code::INVALID_NUMBER, "LGAZ", line_of(text, "LGAZ"));
}

TEST_P(model_loader_longvl, Load_Longvl_Rule) {
  const longvl_case &param = GetParam();
  const load_result result = load_text(longvl_variant(param));
  if (param.missing) {
    expect_longvl_missing(result, param);
  } else {
    expect_longvl_value(result, param);
  }
}

INSTANTIATE_TEST_SUITE_P(rule, model_loader_longvl, ::testing::ValuesIn(LONGVL_CASES));

TEST(model_loader, Load_LongvlBadNumber_ReturnsInvalidNumber) {
  const std::string text = set_value(s_text(), "LONGVL", "fast");
  const load_result result = load_text(text);
  ASSERT_FALSE(result.has_value());
  expect_error(result, error_code::INVALID_NUMBER, "LONGVL", line_of(text, "LONGVL"));
}

TEST(model_loader, Load_Vxlow_DefaultAndExplicit) {
  const load_result absent = load_text(remove_key(s_text(), "VXLOW"));
  ASSERT_TRUE(absent.has_value()) << describe(absent);
  EXPECT_EQ(bits(absent->model->limits()(mf::mf5x::VXLOW)), bits(1.0));

  const load_result explicit_value = load_text(set_value(s_text(), "VXLOW", "2.5"));
  ASSERT_TRUE(explicit_value.has_value()) << describe(explicit_value);
  EXPECT_EQ(bits(explicit_value->model->limits()(mf::mf5x::VXLOW)), bits(2.5));
}

TEST(model_loader, Vxlow_Absent_DefaultsToOne) {
  const load_result loaded = load_text(remove_key(s_text(), "VXLOW"));
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  EXPECT_EQ(bits(loaded->model->limits()(mf::mf5x::VXLOW)), bits(1.0));
  EXPECT_TRUE(loaded->warnings.empty());
}

TEST(model_loader, Vxlow_Explicit_Stored) {
  const load_result loaded = load_text(set_value(s_text(), "VXLOW", "3.25"));
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  EXPECT_EQ(bits(loaded->model->limits()(mf::mf5x::VXLOW)), bits(3.25));
  expect_all_pairs_unbounded(loaded->model->limits());
}

TEST(model_loader, Load_VxlowBadNumber_ReturnsInvalidNumber) {
  const std::string text = set_value(s_text(), "VXLOW", "slow");
  const load_result result = load_text(text);
  ASSERT_FALSE(result.has_value());
  expect_error(result, error_code::INVALID_NUMBER, "VXLOW", line_of(text, "VXLOW"));
}

TEST(model_loader, Load_Ranges_BothBoundsApplied) {
  const load_result loaded = load_tire_model_file(data_file("pac2002_S_rng.tir"));
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  const Eigen::VectorXd expected = mf::test::fixture_s_rng().limits;
  const Eigen::VectorXd &limits = loaded->model->limits();
  expect_same_limits(limits, expected);
  EXPECT_EQ(bits(limits(mf::mf5x::FZMIN)), bits(200.0));
  EXPECT_EQ(bits(limits(mf::mf5x::FZMAX)), bits(10000.0));
  EXPECT_TRUE(loaded->warnings.empty());
}

TEST(model_loader, Load_Ranges_SingleBound_WarnsNotApplied) {
  const std::string lower_only = append(s_text(), "[VERTICAL_FORCE_RANGE]\nFZMIN = 200\n");
  const load_result lower = load_text(lower_only);
  ASSERT_TRUE(lower.has_value()) << describe(lower);
  expect_pair_unbounded(lower->model->limits(), mf::mf5x::FZMIN);
  const std::size_t min_line = line_of(lower_only, "FZMIN");
  expect_single_warning(*lower, "only one of FZMIN/FZMAX given; the range is not applied", "FZMIN", min_line);

  const std::string upper_only = append(s_text(), "[VERTICAL_FORCE_RANGE]\nFZMAX = 10000\n");
  const load_result upper = load_text(upper_only);
  ASSERT_TRUE(upper.has_value()) << describe(upper);
  expect_pair_unbounded(upper->model->limits(), mf::mf5x::FZMIN);
  const std::size_t max_line = line_of(upper_only, "FZMAX");
  expect_single_warning(*upper, "only one of FZMIN/FZMAX given; the range is not applied", "FZMAX", max_line);
}

TEST(model_loader, Load_Ranges_OtherRangesFillTheirOwnInterval) {
  const std::string text = append(s_text(), "[LONG_SLIP_RANGE]\nKPUMIN = -0.5\nKPUMAX = 0.75\n");
  const load_result loaded = load_text(text);
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  const Eigen::VectorXd &limits = loaded->model->limits();
  EXPECT_EQ(bits(limits(mf::mf5x::KPUMIN)), bits(-0.5));
  EXPECT_EQ(bits(limits(mf::mf5x::KPUMAX)), bits(0.75));
  expect_pair_unbounded(limits, mf::mf5x::FZMIN);
  expect_pair_unbounded(limits, mf::mf5x::ALPMIN);
  expect_pair_unbounded(limits, mf::mf5x::CAMMIN);
  EXPECT_TRUE(loaded->warnings.empty());
}

TEST(model_loader, Load_Ranges_LowerAboveUpper_InvalidValueAtFzminLine) {
  const std::string text = append(s_text(), "[VERTICAL_FORCE_RANGE]\nFZMIN = 500\nFZMAX = 100\n");
  const load_result result = load_text(text);
  ASSERT_FALSE(result.has_value());
  expect_error(result, error_code::INVALID_VALUE, "FZMIN", line_of(text, "FZMIN"));
}

TEST(model_loader, Load_RangeLowerBadNumber_ReturnsInvalidNumber) {
  const std::string text = append(s_text(), "[VERTICAL_FORCE_RANGE]\nFZMIN = low\nFZMAX = 100\n");
  const load_result result = load_text(text);
  ASSERT_FALSE(result.has_value());
  expect_error(result, error_code::INVALID_NUMBER, "FZMIN", line_of(text, "FZMIN"));
}

TEST(model_loader, Load_RangeUpperBadNumber_ReturnsInvalidNumber) {
  const std::string text = append(s_text(), "[VERTICAL_FORCE_RANGE]\nFZMIN = 1\nFZMAX = high\n");
  const load_result result = load_text(text);
  ASSERT_FALSE(result.has_value());
  expect_error(result, error_code::INVALID_NUMBER, "FZMAX", line_of(text, "FZMAX"));
}

TEST(model_loader, Limits_BothBoundsEachPair_Stored) {
  for (const limit_pair &pair : LIMIT_PAIRS) {
    const std::string lower_line = std::format("{} = {}\n", pair.lower_key, pair.lower_value);
    const std::string upper_line = std::format("{} = {}\n", pair.upper_key, pair.upper_value);
    const load_result loaded = load_text(with_range_lines(pair, lower_line + upper_line));
    ASSERT_TRUE(loaded.has_value()) << pair.lower_key << ": " << describe(loaded);
    const Eigen::VectorXd &limits = loaded->model->limits();
    EXPECT_EQ(bits(limits(pair.lower_index)), bits(pair.lower_value)) << pair.lower_key;
    EXPECT_EQ(bits(limits(pair.upper_index)), bits(pair.upper_value)) << pair.upper_key;
    EXPECT_EQ(bits(limits(mf::mf5x::VXLOW)), bits(1.0)) << pair.lower_key;
    for (const limit_pair &other : LIMIT_PAIRS) {
      if (other.lower_index != pair.lower_index) {
        expect_pair_unbounded(limits, other.lower_index);
      }
    }
    EXPECT_TRUE(loaded->warnings.empty()) << pair.lower_key;
  }
}

TEST(model_loader, Limits_OnlyLowerBound_WarningAndBothInfinite) {
  for (const limit_pair &pair : LIMIT_PAIRS) {
    const std::string text = with_range_lines(pair, std::format("{} = {}\n", pair.lower_key, pair.lower_value));
    const load_result loaded = load_text(text);
    ASSERT_TRUE(loaded.has_value()) << pair.lower_key << ": " << describe(loaded);
    expect_all_pairs_unbounded(loaded->model->limits());
    const std::string message =
        std::format("only one of {}/{} given; the range is not applied", pair.lower_key, pair.upper_key);
    expect_single_warning(*loaded, message, pair.lower_key, line_of(text, pair.lower_key));
  }
}

TEST(model_loader, Limits_OnlyUpperBound_WarningAndBothInfinite) {
  for (const limit_pair &pair : LIMIT_PAIRS) {
    const std::string text = with_range_lines(pair, std::format("{} = {}\n", pair.upper_key, pair.upper_value));
    const load_result loaded = load_text(text);
    ASSERT_TRUE(loaded.has_value()) << pair.upper_key << ": " << describe(loaded);
    expect_all_pairs_unbounded(loaded->model->limits());
    const std::string message =
        std::format("only one of {}/{} given; the range is not applied", pair.lower_key, pair.upper_key);
    expect_single_warning(*loaded, message, pair.upper_key, line_of(text, pair.upper_key));
  }
}

TEST(model_loader, Limits_NoRanges_AllInfinite) {
  const load_result loaded = load_text(s_text());
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  const Eigen::VectorXd &limits = loaded->model->limits();
  ASSERT_EQ(limits.size(), mf::mf5x::LIMITS_LENGTH);
  expect_all_pairs_unbounded(limits);
  EXPECT_EQ(bits(limits(mf::mf5x::VXLOW)), bits(1.0));
  EXPECT_TRUE(loaded->warnings.empty());
}

TEST(model_loader, Limits_InvalidFzminAndInvalidVxlow_ReportsFzminFirst) {
  const std::string text = append(set_value(s_text(), "VXLOW", "slow"), "[VERTICAL_FORCE_RANGE]\nFZMIN = low\n");
  const load_result result = load_text(text);
  ASSERT_FALSE(result.has_value());
  expect_error(result, error_code::INVALID_NUMBER, "FZMIN", line_of(text, "FZMIN"));
}

TEST(model_loader, Limits_InvalidUpperNumber_ReturnsError) {
  const std::string text = append(s_text(), "[INCLINATION_ANGLE_RANGE]\nCAMMIN = -0.2\nCAMMAX = high\n");
  const load_result result = load_text(text);
  ASSERT_FALSE(result.has_value());
  expect_error(result, error_code::INVALID_NUMBER, "CAMMAX", line_of(text, "CAMMAX"));
}

TEST(model_loader, Limits_LowerAboveUpper_InvalidValueLocatedAtLowerKey) {
  const std::string text = append(s_text(), "[SLIP_ANGLE_RANGE]\nALPMIN = 1\nALPMAX = -1\n");
  const load_result result = load_text(text);
  ASSERT_FALSE(result.has_value());
  expect_error(result, error_code::INVALID_VALUE, "ALPMIN", line_of(text, "ALPMIN"));
}

TEST(model_loader, Options_ForwardedToPac2002) {
  const mf::model_options options{.mode = mf::slip_mode::UNCOMBINED, .throw_if_model_failure = true};
  const load_result loaded = load_text(s_text(), options);
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  EXPECT_EQ(loaded->model->name(), "PAC2002");
  EXPECT_EQ(loaded->model->options().mode, mf::slip_mode::UNCOMBINED);
  EXPECT_TRUE(loaded->model->options().throw_if_model_failure);
}

TEST(model_loader, Options_ForwardedToMf52) {
  const mf::model_options options{.mode = mf::slip_mode::UNCOMBINED, .throw_if_model_failure = true};
  const load_result loaded = load_text(mf52_clean_text(), options);
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  EXPECT_EQ(loaded->model->name(), "MF 5.2");
  EXPECT_EQ(loaded->model->options().mode, mf::slip_mode::UNCOMBINED);
  EXPECT_TRUE(loaded->model->options().throw_if_model_failure);
}

TEST(model_loader, Options_DefaultCombinedUnchecked) {
  const load_result loaded = load_text(s_text());
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  EXPECT_EQ(loaded->model->options().mode, mf::slip_mode::COMBINED);
  EXPECT_FALSE(loaded->model->options().throw_if_model_failure);
}

TEST(model_loader, Load_Tyreside_AbsentLeft) {
  const load_result loaded = load_text(remove_key(s_text(), "TYRESIDE"));
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  EXPECT_EQ(loaded->model->metadata().side, mf::tyre_side::LEFT);
  EXPECT_TRUE(loaded->warnings.empty());
}

TEST(model_loader, Load_Tyreside_LowercaseLeft) {
  const load_result loaded = load_text(set_value(s_text(), "TYRESIDE", "'  left '"));
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  EXPECT_EQ(loaded->model->metadata().side, mf::tyre_side::LEFT);
  EXPECT_TRUE(loaded->warnings.empty());
}

TEST(model_loader, Load_Tyreside_Right_Warns) {
  const std::string text = set_value(s_text(), "TYRESIDE", "'RIGHT'");
  const load_result loaded = load_text(text);
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  EXPECT_EQ(loaded->model->metadata().side, mf::tyre_side::RIGHT);
  const std::size_t line = line_of(text, "TYRESIDE");
  expect_single_warning(*loaded, "TYRESIDE 'RIGHT': no mirroring is applied", "TYRESIDE", line);
}

TEST(model_loader, Load_Tyreside_Symmetric_NotImplemented) {
  const std::string text = set_value(s_text(), "TYRESIDE", "'SYMMETRIC'");
  const load_result result = load_text(text);
  ASSERT_FALSE(result.has_value());
  expect_error(result, error_code::NOT_IMPLEMENTED, "TYRESIDE", line_of(text, "TYRESIDE"));
  EXPECT_EQ(result.error().message, "TYRESIDE 'SYMMETRIC' is not implemented; only LEFT or RIGHT");
}

TEST(model_loader, Load_UseMode_AbsentAnd4_NoWarning) {
  const load_result absent = load_text(remove_key(s_text(), "USE_MODE"));
  ASSERT_TRUE(absent.has_value()) << describe(absent);
  EXPECT_FALSE(absent->model->metadata().use_mode.has_value());
  EXPECT_TRUE(absent->warnings.empty());

  const load_result four = load_text(s_text());
  ASSERT_TRUE(four.has_value()) << describe(four);
  EXPECT_EQ(four->model->metadata().use_mode, std::optional<int>{4});
  EXPECT_TRUE(four->warnings.empty());
}

TEST(model_loader, Load_UseMode_Boundaries_ZeroQuietTenWarns) {
  const load_result zero = load_text(set_value(s_text(), "USE_MODE", "0"));
  ASSERT_TRUE(zero.has_value()) << describe(zero);
  EXPECT_TRUE(zero->warnings.empty());

  const std::string text = set_value(s_text(), "USE_MODE", "10");
  const load_result ten = load_text(text);
  ASSERT_TRUE(ten.has_value()) << describe(ten);
  EXPECT_EQ(ten->model->metadata().use_mode, std::optional<int>{10});
  ASSERT_EQ(ten->warnings.size(), 1U);
}

TEST(model_loader, Load_UseMode_MinusOneAnd11_Warn) {
  const std::string minus_text = set_value(s_text(), "USE_MODE", "-1");
  const load_result minus = load_text(minus_text);
  ASSERT_TRUE(minus.has_value()) << describe(minus);
  EXPECT_EQ(minus->model->metadata().use_mode, std::optional<int>{-1});
  const std::size_t minus_line = line_of(minus_text, "USE_MODE");
  expect_single_warning(*minus, std::format(USE_MODE_MESSAGE, -1), "USE_MODE", minus_line);

  const std::string eleven_text = set_value(s_text(), "USE_MODE", "11");
  const load_result eleven = load_text(eleven_text);
  ASSERT_TRUE(eleven.has_value()) << describe(eleven);
  EXPECT_EQ(eleven->model->metadata().use_mode, std::optional<int>{11});
  const std::size_t eleven_line = line_of(eleven_text, "USE_MODE");
  expect_single_warning(*eleven, std::format(USE_MODE_MESSAGE, 11), "USE_MODE", eleven_line);
}

TEST(model_loader, Load_UseMode_NonIntegral_InvalidValue) {
  const std::string text = set_value(s_text(), "USE_MODE", "4.5");
  const load_result result = load_text(text);
  ASSERT_FALSE(result.has_value());
  expect_error(result, error_code::INVALID_VALUE, "USE_MODE", line_of(text, "USE_MODE"));
}

TEST(model_loader, Load_Pac2002Fittyp6_WarningForwarded) {
  const std::string text = insert_after(s_text(), "[MODEL]", "FITTYP = 6");
  const load_result loaded = load_text(text);
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  EXPECT_EQ(loaded->model->name(), "PAC2002");
  EXPECT_EQ(loaded->model->metadata().fittyp, std::optional<int>{6});
  ASSERT_EQ(loaded->warnings.size(), 1U);
  EXPECT_TRUE(loaded->warnings[0].message.contains("FITTYP 6 ignored for PAC2002")) << loaded->warnings[0].message;
  EXPECT_EQ(loaded->warnings[0].where.key, "FITTYP");
  EXPECT_EQ(loaded->warnings[0].where.line, line_of(text, "FITTYP"));
}

TEST(model_loader, Load_FeMethod_Warns) {
  const std::string text = insert_after(s_text(), "[MODEL]", "FE_METHOD = 'ELLIPSE'");
  const load_result loaded = load_text(text);
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  const std::size_t line = line_of(text, "FE_METHOD");
  expect_single_warning(*loaded, "FE_METHOD is not a PAC2002 or MF 5.2 key and is ignored", "FE_METHOD", line);
  EXPECT_FALSE(has_key(loaded->ignored_keys, "FE_METHOD"));
}

TEST(model_loader, Load_MyFromShifts_Warns) {
  const std::string text = set_value(insert_after(s_text(), "[MODEL]", "FITTYP = 5"), "QSY1", "0");
  const load_result loaded = load_text(text);
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  const mf::pac2002_model *const pac2002 = pac2002_of(*loaded);
  ASSERT_NE(pac2002, nullptr);
  EXPECT_TRUE(pac2002->uses_my_from_shifts());
  const std::size_t line = line_of(text, "FITTYP");
  expect_single_warning(*loaded, "FITTYP 5 with QSY1 = QSY2 = 0: My uses the shift form -R0*(SVx + Kx*SHx)", "FITTYP",
                        line);
}

TEST(model_loader, Load_Fittyp5WithQsy1_NoMyFromShiftsWarning) {
  const load_result loaded = load_text(insert_after(s_text(), "[MODEL]", "FITTYP = 5"));
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  const mf::pac2002_model *const pac2002 = pac2002_of(*loaded);
  ASSERT_NE(pac2002, nullptr);
  EXPECT_FALSE(pac2002->uses_my_from_shifts());
  EXPECT_TRUE(loaded->warnings.empty());
}

TEST(model_loader, Load_InvalidFnomin_InvalidValueWithFileAndLine) {
  const std::string text = set_value(s_text(), "FNOMIN", "-1");
  const load_result result = load_text(text);
  ASSERT_FALSE(result.has_value());
  expect_error(result, error_code::INVALID_VALUE, "FNOMIN", line_of(text, "FNOMIN"));
}

TEST(model_loader, Load_Mf52InvalidFnomin_InvalidValueWithFileAndLine) {
  const std::string text = set_value(mf52_clean_text(), "FNOMIN", "-1");
  const load_result result = load_text(text);
  ASSERT_FALSE(result.has_value());
  expect_error(result, error_code::INVALID_VALUE, "FNOMIN", line_of(text, "FNOMIN"));
}

TEST(model_loader, Load_QuotedNumber_InvalidValue) {
  const std::string text = set_value(s_text(), "PDX1", "'1.0'");
  const load_result result = load_text(text);
  ASSERT_FALSE(result.has_value());
  expect_error(result, error_code::INVALID_VALUE, "PDX1", line_of(text, "PDX1"));
}

TEST(model_loader, Load_BadNumber_InvalidNumber) {
  const std::string text = set_value(s_text(), "PDX1", "abc");
  const load_result result = load_text(text);
  ASSERT_FALSE(result.has_value());
  expect_error(result, error_code::INVALID_NUMBER, "PDX1", line_of(text, "PDX1"));
}

TEST(model_loader, Load_UnitsError_Propagated) {
  const std::string text = set_value(s_text(), "LENGTH", "'mm'");
  const load_result result = load_text(text);
  ASSERT_FALSE(result.has_value());
  expect_error(result, error_code::UNSUPPORTED_UNIT, "LENGTH", line_of(text, "LENGTH"));
}

TEST(model_loader, Load_RoutingError_Propagated) {
  const std::string text = insert_after(s_text(), "[MODEL]", "FITTYP = 61");
  const load_result result = load_text(text);
  ASSERT_FALSE(result.has_value());
  expect_error(result, error_code::UNSUPPORTED_MODEL, "FITTYP", line_of(text, "FITTYP"));
}

TEST(model_loader, Load_DocumentWarnings_ForwardedFirst) {
  const std::string tables = "[SHAPE]\n{radial width}\n1 0\n[SHAPE]\n{radial width}\n2 0\n";
  const std::string text = append(insert_after(s_text(), "[MODEL]", "FITTYP = 6"), tables);
  const load_result loaded = load_text(text);
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  ASSERT_EQ(loaded->warnings.size(), 2U);
  EXPECT_TRUE(loaded->warnings[0].message.contains("duplicate table section [SHAPE]")) << loaded->warnings[0].message;
  EXPECT_TRUE(loaded->warnings[1].message.contains("FITTYP 6 ignored for PAC2002")) << loaded->warnings[1].message;
}

TEST(model_loader, Load_UnknownKey_InIgnoredKeys) {
  const load_result loaded = load_text(append(s_text(), "FOO = 1\n"));
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  ASSERT_FALSE(loaded->ignored_keys.empty());
  EXPECT_EQ(loaded->ignored_keys.back(), "FOO");
  EXPECT_EQ(loaded->ignored_keys.size(), 8U);
  EXPECT_TRUE(loaded->warnings.empty());
}

TEST(model_loader, LoadFile_Nonexistent_ReturnsIo) {
  const std::filesystem::path path = data_file("does_not_exist.tir");
  const load_result result = load_tire_model_file(path);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::IO);
  EXPECT_EQ(result.error().where.file, path.string());
}

TEST(model_loader, LoadFile_SyntaxError_Propagated) {
  // A directory that is unique per run, so that parallel test processes never share the file.
  std::random_device random;
  const std::filesystem::path directory =
      std::filesystem::temp_directory_path() / std::format("mf_model_loader_syntax_{}_{}", random(), random());
  std::filesystem::create_directories(directory);
  const std::filesystem::path path = directory / "syntax.tir";
  {
    std::ofstream output(path, std::ios::binary);
    output << "KEY = 1\n";
  }
  const load_result result = load_tire_model_file(path);
  std::filesystem::remove_all(directory);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::SYNTAX);
  EXPECT_EQ(result.error().where.file, path.string());
  EXPECT_EQ(result.error().where.line, 1U);
}

TEST(model_loader, LoadFile_ForwardsOptions) {
  const mf::model_options options{.mode = mf::slip_mode::UNCOMBINED, .throw_if_model_failure = true};
  const load_result loaded = load_tire_model_file(data_file("pac2002_S.tir"), options);
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  EXPECT_EQ(loaded->model->options().mode, mf::slip_mode::UNCOMBINED);
  EXPECT_TRUE(loaded->model->options().throw_if_model_failure);
}

TEST(model_loader, LoadFile_FixtureS_Succeeds) {
  const load_result loaded = load_tire_model_file(data_file("pac2002_S.tir"));
  ASSERT_TRUE(loaded.has_value()) << describe(loaded);
  EXPECT_EQ(loaded->model->name(), "PAC2002");
  EXPECT_TRUE(loaded->warnings.empty());
}

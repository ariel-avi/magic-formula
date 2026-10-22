#include <array>
#include <expected>
#include <format>
#include <gtest/gtest.h>
#include <magic_formula/error.hpp>
#include <magic_formula/io/model_routing.hpp>
#include <magic_formula/io/tir_document.hpp>
#include <magic_formula/io/tir_reader.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace {
  using mf::error_code;
  using mf::io::identify_model;
  using mf::io::model_kind;
  using mf::io::tir_document;
  using mf::io::tir_reader;

  constexpr std::string_view FILE_NAME = "r.tir";

  /// FITTYP values of the families that are recognised but not supported (rule 6 of `identify_model`).
  constexpr std::array<int, 3> UNSUPPORTED_FAMILIES{21, 51, 52};

  /// FITTYP values of MF-Tyre 6.x (rule 3).
  constexpr std::array<int, 4> MF6X_CODES{60, 61, 62, 70};

  class model_routing_unsupported_family : public ::testing::TestWithParam<int> {};

  class model_routing_mf_six_x : public ::testing::TestWithParam<int> {};

  /// Parses `text` and routes it. A document that does not parse is a test failure.
  std::expected<model_kind, mf::error> route(std::string_view text, std::vector<mf::warning> &warnings) {
    const auto document = tir_document::parse(text, std::string(FILE_NAME));
    if (!document.has_value()) {
      ADD_FAILURE() << mf::to_string(document.error());
      return mf::make_error(error_code::SYNTAX, "test document does not parse");
    }
    tir_reader reader(*document);
    return identify_model(reader, warnings);
  }

  /// Routes `text` and returns the model kind; fails the test when routing fails or warns.
  model_kind route_clean(std::string_view text) {
    std::vector<mf::warning> warnings;
    const auto result = route(text, warnings);
    EXPECT_TRUE(warnings.empty()) << text;
    EXPECT_TRUE(result.has_value()) << text;
    return result.value_or(model_kind::MF52);
  }
} // namespace

TEST(model_routing, Route_R1_Pac2002NoFittyp_Pac2002NoWarning) {
  std::vector<mf::warning> warnings;
  const auto result = route("[MODEL]\nPROPERTY_FILE_FORMAT = 'PAC2002'\n", warnings);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, model_kind::PAC2002);
  EXPECT_TRUE(warnings.empty());
}

TEST(model_routing, Route_R2_Pac2002Fittyp5_Pac2002NoWarning) {
  std::vector<mf::warning> warnings;
  const auto result = route("[MODEL]\nPROPERTY_FILE_FORMAT = 'PAC2002'\nFITTYP = 5\n", warnings);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, model_kind::PAC2002);
  EXPECT_TRUE(warnings.empty());
}

TEST(model_routing, Route_R3_Pac2002Fittyp6_Pac2002WithWarning) {
  std::vector<mf::warning> warnings;
  const auto result = route("[MODEL]\nPROPERTY_FILE_FORMAT = 'PAC2002'\nFITTYP = 6\n", warnings);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, model_kind::PAC2002);
  ASSERT_EQ(warnings.size(), 1U);
  EXPECT_EQ(warnings[0].message, "FITTYP 6 ignored for PAC2002");
  EXPECT_EQ(warnings[0].where.file, FILE_NAME);
  EXPECT_EQ(warnings[0].where.line, 3U);
  EXPECT_EQ(warnings[0].where.key, "FITTYP");
}

TEST(model_routing, Route_Pac2002OtherFittyp_WarnsAboutThatValue) {
  std::vector<mf::warning> warnings;
  const auto result = route("[MODEL]\nPROPERTY_FILE_FORMAT = 'PAC2002'\nFITTYP = 21\n", warnings);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, model_kind::PAC2002);
  ASSERT_EQ(warnings.size(), 1U);
  EXPECT_TRUE(warnings[0].message.contains("FITTYP 21 ignored for PAC2002")) << warnings[0].message;
}

TEST(model_routing, Route_R4_NoPffFittyp6_Mf52) {
  EXPECT_EQ(route_clean("[MODEL]\nFITTYP = 6\n"), model_kind::MF52);
}

TEST(model_routing, Route_R4_Mf05Fittyp6_Mf52) {
  EXPECT_EQ(route_clean("[MODEL]\nPROPERTY_FILE_FORMAT = 'MF_05'\nFITTYP = 6\n"), model_kind::MF52);
}

TEST(model_routing, Route_Fittyp6Point0_Mf52) {
  EXPECT_EQ(route_clean("[MODEL]\nFITTYP = 6.0\n"), model_kind::MF52);
}

TEST(model_routing, Route_R5_Fittyp5WithoutPac2002_UnsupportedModel) {
  const std::array<std::string_view, 2> documents{
      "[MODEL]\nFITTYP = 5\n",
      "[MODEL]\nPROPERTY_FILE_FORMAT = 'MF_05'\nFITTYP = 5\n",
  };
  for (const std::string_view text : documents) {
    std::vector<mf::warning> warnings;
    const auto result = route(text, warnings);
    ASSERT_FALSE(result.has_value()) << text;
    EXPECT_EQ(result.error().code, error_code::UNSUPPORTED_MODEL) << text;
    EXPECT_EQ(result.error().where.key, "FITTYP") << text;
    EXPECT_EQ(result.error().message,
              "FITTYP 5 (MF-Tyre 5.0/5.1) is not supported; set PROPERTY_FILE_FORMAT = 'PAC2002' to evaluate the file "
              "with PAC2002")
        << text;
    EXPECT_TRUE(warnings.empty()) << text;
  }
}

TEST_P(model_routing_unsupported_family, Route_R6_UnsupportedFamily_UnsupportedModel) {
  const int code = GetParam();
  const std::array<std::string, 2> documents{
      std::format("[MODEL]\nFITTYP = {}\n", code),
      std::format("[MODEL]\nPROPERTY_FILE_FORMAT = 'MF_05'\nFITTYP = {}\n", code),
  };
  for (const std::string &text : documents) {
    std::vector<mf::warning> warnings;
    const auto result = route(text, warnings);
    ASSERT_FALSE(result.has_value()) << text;
    EXPECT_EQ(result.error().code, error_code::UNSUPPORTED_MODEL) << text;
    EXPECT_EQ(result.error().where.key, "FITTYP") << text;
    EXPECT_TRUE(result.error().message.contains(std::format("FITTYP {} ", code))) << result.error().message;
    EXPECT_TRUE(result.error().message.contains("is not supported")) << result.error().message;
  }
}

INSTANTIATE_TEST_SUITE_P(codes, model_routing_unsupported_family, ::testing::ValuesIn(UNSUPPORTED_FAMILIES));

TEST_P(model_routing_mf_six_x, Route_R7_Mf6x_NotImplemented) {
  const int code = GetParam();
  std::vector<mf::warning> warnings;
  const auto result = route(std::format("[MODEL]\nFITTYP = {}\n", code), warnings);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::UNSUPPORTED_MODEL);
  EXPECT_EQ(result.error().where.key, "FITTYP");
  EXPECT_EQ(result.error().where.line, 2U);
  EXPECT_EQ(result.error().message, std::format("FITTYP {}: MF-Tyre 6.x is not implemented", code));
}

INSTANTIATE_TEST_SUITE_P(codes, model_routing_mf_six_x, ::testing::ValuesIn(MF6X_CODES));

TEST(model_routing, Route_R8_NoPffNoFittyp_CannotIdentify) {
  std::vector<mf::warning> warnings;
  const auto result = route("[MODEL]\nUSE_MODE = 4\n", warnings);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::UNSUPPORTED_MODEL);
  EXPECT_EQ(result.error().where.key, "FITTYP");
  EXPECT_EQ(result.error().where.line, 0U);
  EXPECT_EQ(result.error().message,
            "cannot identify the tire model: set PROPERTY_FILE_FORMAT = 'PAC2002' or FITTYP = 6");
}

TEST(model_routing, Route_R8_Fittyp0And7_CannotIdentify) {
  const std::array<std::string_view, 2> documents{"[MODEL]\nFITTYP = 0\n", "[MODEL]\nFITTYP = 7\n"};
  for (const std::string_view text : documents) {
    std::vector<mf::warning> warnings;
    const auto result = route(text, warnings);
    ASSERT_FALSE(result.has_value()) << text;
    EXPECT_EQ(result.error().code, error_code::UNSUPPORTED_MODEL) << text;
    EXPECT_EQ(result.error().where.line, 2U) << text;
    EXPECT_TRUE(result.error().message.contains("cannot identify the tire model")) << result.error().message;
  }
}

TEST(model_routing, Route_R8_Mf05NoFittyp_CannotIdentify) {
  std::vector<mf::warning> warnings;
  const auto result = route("[MODEL]\nPROPERTY_FILE_FORMAT = 'MF_05'\n", warnings);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::UNSUPPORTED_MODEL);
  EXPECT_TRUE(result.error().message.contains("cannot identify the tire model")) << result.error().message;
}

TEST(model_routing, Route_R9_UnknownPff_UnsupportedModel) {
  std::vector<mf::warning> warnings;
  const auto result = route("[MODEL]\nPROPERTY_FILE_FORMAT = 'MF61'\nFITTYP = 6\n", warnings);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::UNSUPPORTED_MODEL);
  EXPECT_EQ(result.error().where.key, "PROPERTY_FILE_FORMAT");
  EXPECT_EQ(result.error().where.line, 2U);
  EXPECT_EQ(result.error().where.file, FILE_NAME);
  EXPECT_EQ(result.error().message,
            "unknown PROPERTY_FILE_FORMAT 'MF61' (expected 'PAC2002', 'MF_05', 'USER' or 'MF-TYRE')");
}

TEST(model_routing, Route_UserPffFittyp6_Mf52NoWarning) {
  EXPECT_EQ(route_clean("[MODEL]\nPROPERTY_FILE_FORMAT = 'USER'\nUSER_SUB_ID = 815\nFITTYP = 6\n"), model_kind::MF52);
}

TEST(model_routing, Route_UserPffLowerCaseFittyp6_Mf52NoWarning) {
  EXPECT_EQ(route_clean("[MODEL]\nPROPERTY_FILE_FORMAT = 'user'\nFITTYP = 6\n"), model_kind::MF52);
}

TEST(model_routing, Route_MfTyrePffFittyp6_Mf52NoWarning) {
  const std::array<std::string_view, 2> documents{
      "[MODEL]\nPROPERTY_FILE_FORMAT = 'MF-TYRE'\nFITTYP = 6\n",
      "[MODEL]\nPROPERTY_FILE_FORMAT = ' mf-tyre '\nFITTYP = 6\n",
  };
  for (const std::string_view text : documents) {
    EXPECT_EQ(route_clean(text), model_kind::MF52) << text;
  }
}

TEST(model_routing, Route_UserPffFittyp61_Mf6xNotImplemented) {
  std::vector<mf::warning> warnings;
  const auto result = route("[MODEL]\nPROPERTY_FILE_FORMAT = 'USER'\nFITTYP = 61\n", warnings);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::UNSUPPORTED_MODEL);
  EXPECT_EQ(result.error().where.key, "FITTYP");
  EXPECT_EQ(result.error().where.line, 3U);
  EXPECT_TRUE(result.error().message.contains("MF-Tyre 6.x")) << result.error().message;
  EXPECT_TRUE(warnings.empty());
}

TEST(model_routing, Route_UserAndMfTyrePffFittyp5_UnsupportedModel) {
  const std::array<std::string_view, 2> documents{
      "[MODEL]\nPROPERTY_FILE_FORMAT = 'USER'\nFITTYP = 5\n",
      "[MODEL]\nPROPERTY_FILE_FORMAT = 'MF-TYRE'\nFITTYP = 5\n",
  };
  for (const std::string_view text : documents) {
    std::vector<mf::warning> warnings;
    const auto result = route(text, warnings);
    ASSERT_FALSE(result.has_value()) << text;
    EXPECT_EQ(result.error().code, error_code::UNSUPPORTED_MODEL) << text;
    EXPECT_EQ(result.error().where.key, "FITTYP") << text;
    EXPECT_TRUE(result.error().message.contains("FITTYP 5")) << result.error().message;
    EXPECT_TRUE(result.error().message.contains("PAC2002")) << result.error().message;
  }
}

TEST(model_routing, Route_UserAndMfTyrePffOtherFamilies_UnsupportedModel) {
  constexpr std::array<std::string_view, 2> formats{"USER", "MF-TYRE"};
  for (const int code : UNSUPPORTED_FAMILIES) {
    for (const std::string_view format : formats) {
      std::vector<mf::warning> warnings;
      const std::string text = std::format("[MODEL]\nPROPERTY_FILE_FORMAT = '{}'\nFITTYP = {}\n", format, code);
      const auto result = route(text, warnings);
      ASSERT_FALSE(result.has_value()) << text;
      EXPECT_EQ(result.error().code, error_code::UNSUPPORTED_MODEL);
      EXPECT_TRUE(result.error().message.contains(std::format("FITTYP {} ", code))) << result.error().message;
    }
  }
}

TEST(model_routing, Route_UserPffWithoutFittyp_CannotIdentify) {
  std::vector<mf::warning> warnings;
  const auto result = route("[MODEL]\nPROPERTY_FILE_FORMAT = 'USER'\nUSER_SUB_ID = 815\n", warnings);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::UNSUPPORTED_MODEL);
  EXPECT_EQ(result.error().where.key, "FITTYP");
  EXPECT_TRUE(result.error().message.contains("cannot identify the tire model")) << result.error().message;
}

TEST(model_routing, Route_UserPffUnrelatedKey_StaysUnconsumed) {
  // USER_SUB_ID needs no handling: routing leaves it alone (it is only an ignored key).
  const auto document = tir_document::parse("[MODEL]\nPROPERTY_FILE_FORMAT = 'USER'\nUSER_SUB_ID = 815\nFITTYP = 6\n");
  ASSERT_TRUE(document.has_value());
  tir_reader reader(*document);
  std::vector<mf::warning> warnings;
  const auto result = identify_model(reader, warnings);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, model_kind::MF52);
  const std::vector<std::string> expected{"USER_SUB_ID"};
  EXPECT_EQ(reader.unconsumed_keys(), expected);
}

TEST(model_routing, Route_EmptyPff_UnsupportedModel) {
  std::vector<mf::warning> warnings;
  const auto result = route("[MODEL]\nPROPERTY_FILE_FORMAT = ''\nFITTYP = 6\n", warnings);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::UNSUPPORTED_MODEL);
  EXPECT_EQ(result.error().where.key, "PROPERTY_FILE_FORMAT");
}

TEST(model_routing, Route_Pac2002Fittyp61_UnsupportedModel) {
  // Precedence: MF-Tyre 6.x is rejected even when the file declares PAC2002.
  std::vector<mf::warning> warnings;
  const auto result = route("[MODEL]\nPROPERTY_FILE_FORMAT = 'PAC2002'\nFITTYP = 61\n", warnings);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::UNSUPPORTED_MODEL);
  EXPECT_EQ(result.error().where.key, "FITTYP");
  EXPECT_EQ(result.error().where.line, 3U);
  EXPECT_EQ(result.error().message, "FITTYP 61: MF-Tyre 6.x is not implemented");
  EXPECT_TRUE(warnings.empty());
}

TEST(model_routing, Route_UnknownPffAndFittyp61_ReportsPffError) {
  std::vector<mf::warning> warnings;
  const auto result = route("[MODEL]\nPROPERTY_FILE_FORMAT = 'MF61'\nFITTYP = 61\n", warnings);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().where.key, "PROPERTY_FILE_FORMAT");
  EXPECT_TRUE(result.error().message.contains("unknown PROPERTY_FILE_FORMAT")) << result.error().message;
}

TEST(model_routing, Route_PffCaseAndBlanks_Normalised) {
  const std::array<std::string_view, 4> documents{
      "[MODEL]\nPROPERTY_FILE_FORMAT = 'pac2002'\n",
      "[MODEL]\nPROPERTY_FILE_FORMAT = ' Pac2002 '\n",
      "[MODEL]\nPROPERTY_FILE_FORMAT = PAC2002\n",
      "[MODEL]\nproperty_file_format = 'PaC2002'\n",
  };
  for (const std::string_view text : documents) {
    EXPECT_EQ(route_clean(text), model_kind::PAC2002) << text;
  }
}

TEST(model_routing, Route_Mf05CaseAndBlanks_Normalised) {
  const std::array<std::string_view, 2> documents{
      "[MODEL]\nPROPERTY_FILE_FORMAT = ' mf_05 '\nFITTYP = 6\n",
      "[MODEL]\nPROPERTY_FILE_FORMAT = MF_05\nFITTYP = 6\n",
  };
  for (const std::string_view text : documents) {
    EXPECT_EQ(route_clean(text), model_kind::MF52) << text;
  }
}

TEST(model_routing, Route_NonIntegralFittyp_InvalidValue) {
  std::vector<mf::warning> warnings;
  const auto result = route("[MODEL]\nFITTYP = 5.5\n", warnings);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::INVALID_VALUE);
  EXPECT_EQ(result.error().where.key, "FITTYP");
  EXPECT_EQ(result.error().where.line, 2U);
}

TEST(model_routing, Route_QuotedFittyp_InvalidValue) {
  std::vector<mf::warning> warnings;
  const auto result = route("[MODEL]\nFITTYP = 'six'\n", warnings);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::INVALID_VALUE);
  EXPECT_EQ(result.error().where.key, "FITTYP");
}

TEST(model_routing, Route_JunkFittyp_InvalidNumber) {
  std::vector<mf::warning> warnings;
  const auto result = route("[MODEL]\nFITTYP = six\n", warnings);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::INVALID_NUMBER);
  EXPECT_EQ(result.error().where.key, "FITTYP");
}

TEST(model_routing, Route_Pac2002WithBadFittyp_ReportsFittypError) {
  // Rule 2 comes before the PAC2002 shortcut: a malformed FITTYP is never ignored.
  std::vector<mf::warning> warnings;
  const auto result = route("[MODEL]\nPROPERTY_FILE_FORMAT = 'PAC2002'\nFITTYP = 5.5\n", warnings);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::INVALID_VALUE);
  EXPECT_EQ(result.error().where.line, 3U);
}

TEST(model_routing, Route_ErrorLocation_NamesFittypLine) {
  std::vector<mf::warning> warnings;
  const auto result = route("[MDI_HEADER]\nFILE_TYPE = 'tir'\n[MODEL]\nFITTYP = 21\n", warnings);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().where.file, FILE_NAME);
  EXPECT_EQ(result.error().where.line, 4U);
  EXPECT_EQ(result.error().where.key, "FITTYP");
}

TEST(model_routing, Route_ConsumesPffAndFittypOnly) {
  const auto document = tir_document::parse("[MODEL]\nPROPERTY_FILE_FORMAT = 'PAC2002'\nFITTYP = 5\nUSE_MODE = 4\n");
  ASSERT_TRUE(document.has_value());
  tir_reader reader(*document);
  std::vector<mf::warning> warnings;
  const auto result = identify_model(reader, warnings);
  ASSERT_TRUE(result.has_value());
  const std::vector<std::string> expected{"USE_MODE"};
  EXPECT_EQ(reader.unconsumed_keys(), expected);
}

#include "../support/fixtures.hpp"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <gtest/gtest.h>
#include <magic_formula/coordinate_identification.hpp>
#include <magic_formula/coordinate_system.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/io/model_loader.hpp>
#include <magic_formula/io/text_file.hpp>
#include <magic_formula/io/tir_conversion.hpp>
#include <magic_formula/io/tir_document.hpp>
#include <magic_formula/mf5x_model.hpp>
#include <magic_formula/sign_transform.hpp>
#include <magic_formula/tire_model.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace {
  using mf::coordinate_system;
  using mf::io::convert_tir;
  using mf::io::negate_number_text;
  using mf::io::tir_change;
  using mf::io::tir_conversion;

  /// Content of a file of tests/data.
  std::string read_fixture(std::string_view name) {
    const auto text = mf::io::read_text_file(mf::test::data_file(name));
    EXPECT_TRUE(text.has_value()) << name;
    return text.has_value() ? *text : std::string{};
  }

  /// `text` with the first occurrence of `from` replaced by `to`.
  std::string replaced(std::string text, std::string_view from, std::string_view to) {
    const std::size_t position = text.find(from);
    EXPECT_NE(position, std::string::npos) << from;
    if (position != std::string::npos) {
      text.replace(position, from.size(), to);
    }
    return text;
  }

  /// Parses and loads `text`.
  std::expected<mf::io::loaded_model, mf::error> load_text(std::string_view text) {
    const auto document = mf::io::tir_document::parse(text);
    if (!document.has_value()) {
      return std::unexpected<mf::error>(document.error());
    }
    return mf::io::load_tire_model(*document);
  }

  /// The MF 5.x view of a loaded model.
  const mf::mf5x_model &as_mf5x(const mf::tire_model &model) {
    return dynamic_cast<const mf::mf5x_model &>(model);
  }

  /// Value text of the first entry named `key` in `text`; empty when absent.
  std::string value_of(std::string_view text, std::string_view key) {
    const auto document = mf::io::tir_document::parse(text);
    if (!document.has_value()) {
      return "<unparsable>";
    }
    const mf::io::tir_entry *const entry = document->find(key);
    return entry == nullptr ? std::string{"<absent>"} : entry->value;
  }

  bool has_change(const tir_conversion &conversion, std::string_view key) {
    return std::ranges::any_of(conversion.changes, [key](const tir_change &change) { return change.key == key; });
  }

  void expect_change(const tir_change &change, std::string_view section, std::string_view key, std::size_t line,
                     std::string_view before, std::string_view after) {
    EXPECT_EQ(change.section, section);
    EXPECT_EQ(change.key, key);
    EXPECT_EQ(change.line, line);
    EXPECT_EQ(change.before, before);
    EXPECT_EQ(change.after, after);
  }

  mf::system_classification classification_of(coordinate_system target) {
    return target == coordinate_system::ISO ? mf::system_classification::ISO : mf::system_classification::SAE;
  }
} // namespace

TEST(tir_conversion, NegateNumberText_Plain_PrefixesMinus) {
  EXPECT_EQ(negate_number_text("4000"), "-4000");
  EXPECT_EQ(negate_number_text("1e-3"), "-1e-3");
  EXPECT_EQ(negate_number_text("0"), "-0");
}

TEST(tir_conversion, NegateNumberText_Negative_DropsMinus) {
  EXPECT_EQ(negate_number_text("-15"), "15");
}

TEST(tir_conversion, NegateNumberText_LeadingPlus_ReplacedByMinus) {
  EXPECT_EQ(negate_number_text("+1.5"), "-1.5");
}

TEST(tir_conversion, ConvertTir_PacSToIso_FlipsOnlyNonZeroLateralKeys) {
  const std::string text = read_fixture("pac2002_S.tir");
  const auto conversion = convert_tir(text, coordinate_system::ISO);
  ASSERT_TRUE(conversion.has_value()) << mf::to_string(conversion.error());
  EXPECT_FALSE(conversion->applied.vertical);
  EXPECT_TRUE(conversion->applied.lateral);
  ASSERT_EQ(conversion->changes.size(), 1U);
  expect_change(conversion->changes[0], "LATERAL_COEFFICIENTS", "PKY1", 88, "-15", "15");
  EXPECT_EQ(conversion->text, replaced(text, "PKY1 = -15", "PKY1 = 15"));
  EXPECT_EQ(value_of(conversion->text, "PHY1"), "0");
}

TEST(tir_conversion, ConvertTir_PacSToSae_FlipsBothInFileOrder) {
  const std::string text = read_fixture("pac2002_S.tir");
  const auto conversion = convert_tir(text, coordinate_system::SAE);
  ASSERT_TRUE(conversion.has_value()) << mf::to_string(conversion.error());
  EXPECT_TRUE(conversion->applied.vertical);
  EXPECT_TRUE(conversion->applied.lateral);
  ASSERT_EQ(conversion->changes.size(), 4U);
  expect_change(conversion->changes[0], "VERTICAL", "FNOMIN", 22, "4000", "-4000");
  expect_change(conversion->changes[1], "LONGITUDINAL_COEFFICIENTS", "PDX1", 55, "1.0", "-1.0");
  expect_change(conversion->changes[2], "LONGITUDINAL_COEFFICIENTS", "PKX1", 62, "20", "-20");
  expect_change(conversion->changes[3], "LATERAL_COEFFICIENTS", "PKY1", 88, "-15", "15");
}

TEST(tir_conversion, ConvertTir_IsoToIso_UnchangedText) {
  const std::string text = read_fixture("pac2002_S_iso.tir");
  const auto conversion = convert_tir(text, coordinate_system::ISO);
  ASSERT_TRUE(conversion.has_value()) << mf::to_string(conversion.error());
  EXPECT_FALSE(conversion->applied.vertical);
  EXPECT_FALSE(conversion->applied.lateral);
  EXPECT_TRUE(conversion->changes.empty());
  EXPECT_EQ(conversion->text, text);
}

TEST(tir_conversion, ConvertTir_IsoToSaeToIso_RoundTripByteIdentical) {
  const std::string text = read_fixture("pac2002_S_iso.tir");
  const auto sae = convert_tir(text, coordinate_system::SAE);
  ASSERT_TRUE(sae.has_value()) << mf::to_string(sae.error());
  EXPECT_NE(sae->text, text);
  const auto back = convert_tir(sae->text, coordinate_system::ISO);
  ASSERT_TRUE(back.has_value()) << mf::to_string(back.error());
  EXPECT_EQ(back->text, text);
}

TEST(tir_conversion, ConvertTir_SaeFileFromIso_EqualsCommittedSaeFixtureValues) {
  const auto conversion = convert_tir(read_fixture("pac2002_S_iso.tir"), coordinate_system::SAE);
  ASSERT_TRUE(conversion.has_value()) << mf::to_string(conversion.error());
  const auto converted = load_text(conversion->text);
  const auto committed = load_text(read_fixture("pac2002_S_sae.tir"));
  ASSERT_TRUE(converted.has_value());
  ASSERT_TRUE(committed.has_value());
  EXPECT_TRUE(mf::test::coefficients_value_equal(as_mf5x(*converted->model).coefficients(),
                                                 as_mf5x(*committed->model).coefficients()));
  EXPECT_TRUE(mf::test::limits_value_equal(converted->model->limits(), committed->model->limits()));
}

TEST(tir_conversion, ConvertTir_RangedToSae_SwapNegatesFzLimits) {
  const auto conversion = convert_tir(read_fixture("pac2002_S_rng.tir"), coordinate_system::SAE);
  ASSERT_TRUE(conversion.has_value()) << mf::to_string(conversion.error());
  EXPECT_EQ(value_of(conversion->text, "FZMIN"), "-10000");
  EXPECT_EQ(value_of(conversion->text, "FZMAX"), "-200");
  EXPECT_TRUE(has_change(*conversion, "FZMIN"));
  EXPECT_TRUE(has_change(*conversion, "FZMAX"));
  EXPECT_FALSE(has_change(*conversion, "ALPMIN"));
  EXPECT_FALSE(has_change(*conversion, "ALPMAX"));
  EXPECT_EQ(value_of(conversion->text, "ALPMIN"), "-1.5");
  EXPECT_EQ(value_of(conversion->text, "ALPMAX"), "1.5");
}

TEST(tir_conversion, ConvertTir_LoneFzmin_LeftAsWritten) {
  const std::string text = replaced(read_fixture("pac2002_S.tir"), "FNOMIN = 4000\n", "FNOMIN = 4000\nFZMIN = 200\n");
  const auto conversion = convert_tir(text, coordinate_system::SAE);
  ASSERT_TRUE(conversion.has_value()) << mf::to_string(conversion.error());
  EXPECT_TRUE(conversion->applied.vertical);
  EXPECT_EQ(value_of(conversion->text, "FZMIN"), "200");
  EXPECT_FALSE(has_change(*conversion, "FZMIN"));
}

TEST(tir_conversion, ConvertTir_LoneFzmax_LeftAsWritten) {
  const std::string text = replaced(read_fixture("pac2002_S.tir"), "FNOMIN = 4000\n", "FNOMIN = 4000\nFZMAX = 9000\n");
  const auto conversion = convert_tir(text, coordinate_system::SAE);
  ASSERT_TRUE(conversion.has_value()) << mf::to_string(conversion.error());
  EXPECT_TRUE(conversion->applied.vertical);
  EXPECT_EQ(value_of(conversion->text, "FZMAX"), "9000");
  EXPECT_FALSE(has_change(*conversion, "FZMAX"));
}

TEST(tir_conversion, ConvertTir_TrailingCommentAndPlus_OnlyValueBytesChange) {
  const std::string text = replaced(read_fixture("pac2002_S.tir"), "PKX1 = 20", "PKX1 = +20 $ stiffness");
  const auto conversion = convert_tir(text, coordinate_system::SAE);
  ASSERT_TRUE(conversion.has_value()) << mf::to_string(conversion.error());
  EXPECT_NE(conversion->text.find("PKX1 = -20 $ stiffness\n"), std::string::npos);
  EXPECT_TRUE(has_change(*conversion, "PKX1"));
}

TEST(tir_conversion, ConvertTir_AbsentOptionalKey_Skipped) {
  const std::string text = replaced(read_fixture("pac2002_S.tir"), "PKX2 = 0\n", "");
  const auto conversion = convert_tir(text, coordinate_system::SAE);
  ASSERT_TRUE(conversion.has_value()) << mf::to_string(conversion.error());
  EXPECT_EQ(conversion->changes.size(), 4U);
  EXPECT_EQ(value_of(conversion->text, "PKX2"), "<absent>");
}

TEST(tir_conversion, ConvertTir_ParseError_PassedThroughWithFileName) {
  const auto conversion = convert_tir("FNOMIN = 1\n", coordinate_system::ISO, "x.tir");
  ASSERT_FALSE(conversion.has_value());
  EXPECT_EQ(conversion.error().code, mf::error_code::SYNTAX);
  EXPECT_EQ(conversion.error().where.file, "x.tir");
  EXPECT_EQ(conversion.error().where.line, 1U);
}

TEST(tir_conversion, ConvertTir_LoadError_PassedThrough) {
  const auto conversion = convert_tir(read_fixture("pac2002_S_part.tir"), coordinate_system::ISO);
  ASSERT_FALSE(conversion.has_value());
  EXPECT_EQ(conversion.error().code, mf::error_code::MISSING_KEY);
  EXPECT_EQ(conversion.error().where.key, "RHX1");
}

namespace {
  /// Converts `fixture` to both systems and checks each result against the transformed original model.
  void expect_loads_as_transformed_model(std::string_view fixture) {
    for (const coordinate_system target : {coordinate_system::ISO, coordinate_system::SAE}) {
      SCOPED_TRACE(std::string{fixture} + " -> " + std::string{mf::coordinate_system_name(target)});
      const std::string text = read_fixture(fixture);
      const auto original = load_text(text);
      ASSERT_TRUE(original.has_value()) << mf::to_string(original.error());
      const auto conversion = convert_tir(text, target);
      ASSERT_TRUE(conversion.has_value()) << mf::to_string(conversion.error());
      const auto converted = load_text(conversion->text);
      ASSERT_TRUE(converted.has_value()) << mf::to_string(converted.error());

      const std::unique_ptr<mf::tire_model> expected = original.value().model->transformed(conversion->applied);
      EXPECT_TRUE(mf::test::coefficients_value_equal(as_mf5x(*converted->model).coefficients(),
                                                     as_mf5x(*expected).coefficients()));
      EXPECT_TRUE(mf::test::limits_value_equal(converted->model->limits(), expected->limits()));
      EXPECT_EQ(converted->model->find_coordinate_system().classification, classification_of(target));
    }
  }
} // namespace

TEST(tir_conversion, ConvertTir_EveryLoadableFixture_LoadsAsTransformedModel) {
  expect_loads_as_transformed_model("pac2002_S.tir");
  expect_loads_as_transformed_model("pac2002_S_iso.tir");
  expect_loads_as_transformed_model("pac2002_S_sae.tir");
  expect_loads_as_transformed_model("pac2002_S_ell.tir");
  expect_loads_as_transformed_model("pac2002_S_rng.tir");
  expect_loads_as_transformed_model("mf52_S52.tir");
  expect_loads_as_transformed_model("mf52_user_pff.tir");
  expect_loads_as_transformed_model("lexical_mix.tir");
}

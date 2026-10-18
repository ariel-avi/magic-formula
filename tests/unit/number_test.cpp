#include <array>
#include <bit>
#include <cstdint>
#include <gtest/gtest.h>
#include <magic_formula/io/number.hpp>
#include <string_view>

namespace {
  struct number_case {
    std::string_view text;
    double expected;
  };

  struct reject_case {
    std::string_view text;
  };

  // Kept one entry per line: clang-format would repack these tables into padded columns.
  // clang-format off
  constexpr std::array<number_case, 10> ACCEPTED_CASES{
      number_case{.text = "14", .expected = 14.0},
      number_case{.text = "4850", .expected = 4850.0},
      number_case{.text = "2.1e+005", .expected = 210000.0},
      number_case{.text = "-3.7604e-005", .expected = -3.7604e-05},
      number_case{.text = "+1.0", .expected = 1.0},
      number_case{.text = "1.", .expected = 1.0},
      number_case{.text = ".5", .expected = 0.5},
      number_case{.text = "1E-3", .expected = 0.001},
      number_case{.text = "-0.0", .expected = -0.0},
      number_case{.text = "+.5", .expected = 0.5},
  };

  constexpr std::array<reject_case, 18> REJECTED_CASES{
      reject_case{.text = ""},
      reject_case{.text = "+"},
      reject_case{.text = "+-1"},
      reject_case{.text = "++1"},
      reject_case{.text = "inf"},
      reject_case{.text = "-inf"},
      reject_case{.text = "nan"},
      reject_case{.text = "infinity"},
      reject_case{.text = "0x1p3"},
      reject_case{.text = "1,5"},
      reject_case{.text = "1e"},
      reject_case{.text = "."},
      reject_case{.text = "1.0D-3"},
      reject_case{.text = " 1"},
      reject_case{.text = "1 "},
      reject_case{.text = "1e999"},
      reject_case{.text = "-1e999"},
      reject_case{.text = "1e-400"},
  };
  // clang-format on

  class parse_number_accept : public ::testing::TestWithParam<number_case> {};

  class parse_number_reject : public ::testing::TestWithParam<reject_case> {};
} // namespace

TEST_P(parse_number_accept, Parse_ValidText_ReturnsBitExactValue) {
  const number_case &param = GetParam();
  const auto result = mf::io::parse_number(param.text);
  ASSERT_TRUE(result.has_value()) << param.text;
  // Bit-exact comparison against the C++ literal (distinguishes -0.0 from 0.0).
  EXPECT_EQ(std::bit_cast<std::uint64_t>(result.value_or(0.5)), std::bit_cast<std::uint64_t>(param.expected))
      << param.text;
}

INSTANTIATE_TEST_SUITE_P(cases, parse_number_accept, ::testing::ValuesIn(ACCEPTED_CASES));

TEST_P(parse_number_reject, Parse_InvalidText_ReturnsNullopt) {
  const std::string_view text = GetParam().text;
  EXPECT_FALSE(mf::io::parse_number(text).has_value()) << text;
}

INSTANTIATE_TEST_SUITE_P(cases, parse_number_reject, ::testing::ValuesIn(REJECTED_CASES));

TEST(parse_integer, Parse_IntegralValues_ReturnsInt) {
  constexpr int sentinel = 12345;
  EXPECT_EQ(mf::io::parse_integer("6").value_or(sentinel), 6);
  EXPECT_EQ(mf::io::parse_integer("-3").value_or(sentinel), -3);
  EXPECT_EQ(mf::io::parse_integer("6.0").value_or(sentinel), 6);
  EXPECT_EQ(mf::io::parse_integer("2147483647").value_or(sentinel), 2147483647);
  EXPECT_EQ(mf::io::parse_integer("-2147483648").value_or(sentinel), -2147483647 - 1);
}

TEST(parse_integer, Parse_NonIntegral_ReturnsNullopt) {
  EXPECT_FALSE(mf::io::parse_integer("5.5").has_value());
}

TEST(parse_integer, Parse_OutOfRange_ReturnsNullopt) {
  EXPECT_FALSE(mf::io::parse_integer("1e10").has_value());
  EXPECT_FALSE(mf::io::parse_integer("-1e10").has_value());
  EXPECT_FALSE(mf::io::parse_integer("2147483648").has_value());
  EXPECT_FALSE(mf::io::parse_integer("-2147483649").has_value());
}

TEST(parse_integer, Parse_NotANumber_ReturnsNullopt) {
  EXPECT_FALSE(mf::io::parse_integer("x").has_value());
}

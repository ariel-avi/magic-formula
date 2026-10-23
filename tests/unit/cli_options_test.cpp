#include <cstddef>
#include <expected>
#include <format>
#include <gtest/gtest.h>
#include <magic_formula/cli/cli.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/io/result_writer.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace {
  using mf::cli::command;
  using args_list = std::vector<std::string_view>;
  using parse_result = std::expected<mf::cli::options, std::string>;

  /// A command line with a name for the test report.
  struct line_case {
    /// Test-name suffix (letters, digits and underscores).
    std::string_view name;
    /// The arguments, `argv[0]` excluded.
    args_list args;
    /// Number of positional arguments in `args`.
    std::size_t positionals = 0;
  };

  class cli_options_position : public ::testing::TestWithParam<line_case> {};

  class cli_options_wrong_count : public ::testing::TestWithParam<line_case> {};

  parse_result parse(const args_list &args) {
    return mf::cli::parse_command_line(args);
  }

  std::string case_name(const ::testing::TestParamInfo<line_case> &info) {
    return std::string(info.param.name);
  }

  // clang-format off
  /// The two options in front of, between and behind the two positional arguments.
  const std::vector<line_case> POSITION_CASES{
      line_case{.name = "before", .args = {"--uncombined", "--verbose", "table.csv", "model.tir"}, .positionals = 2},
      line_case{.name = "between", .args = {"table.csv", "--uncombined", "--verbose", "model.tir"}, .positionals = 2},
      line_case{.name = "after", .args = {"table.csv", "model.tir", "--uncombined", "--verbose"}, .positionals = 2},
  };

  /// Command lines with 0, 1 and 3 positional arguments.
  const std::vector<line_case> WRONG_COUNT_CASES{
      line_case{.name = "none", .args = {}, .positionals = 0},
      line_case{.name = "options_only", .args = {"--uncombined", "--verbose"}, .positionals = 0},
      line_case{.name = "one", .args = {"table.csv"}, .positionals = 1},
      line_case{.name = "three", .args = {"a.csv", "b.tir", "c"}, .positionals = 3},
  };
  // clang-format on
} // namespace

TEST(cli_options, Parse_TwoPositionals_Run) {
  const parse_result parsed = parse({"table.csv", "model.tir"});
  ASSERT_TRUE(parsed.has_value()) << parsed.error();
  EXPECT_EQ(parsed->action, command::RUN);
  EXPECT_EQ(parsed->inputs_path, "table.csv");
  EXPECT_EQ(parsed->tir_path, "model.tir");
  EXPECT_EQ(parsed->mode, mf::slip_mode::COMBINED);
  EXPECT_FALSE(parsed->verbose);
}

TEST_P(cli_options_position, Parse_OptionPosition_Accepted) {
  const parse_result parsed = parse(GetParam().args);
  ASSERT_TRUE(parsed.has_value()) << parsed.error();
  EXPECT_EQ(parsed->action, command::RUN);
  EXPECT_EQ(parsed->inputs_path, "table.csv");
  EXPECT_EQ(parsed->tir_path, "model.tir");
  EXPECT_EQ(parsed->mode, mf::slip_mode::UNCOMBINED);
  EXPECT_TRUE(parsed->verbose);
}

INSTANTIATE_TEST_SUITE_P(positions, cli_options_position, ::testing::ValuesIn(POSITION_CASES), case_name);

TEST(cli_options, Parse_Uncombined_SetsMode) {
  const parse_result parsed = parse({"--uncombined", "table.csv", "model.tir"});
  ASSERT_TRUE(parsed.has_value()) << parsed.error();
  EXPECT_EQ(parsed->mode, mf::slip_mode::UNCOMBINED);
  EXPECT_FALSE(parsed->verbose);
}

TEST(cli_options, Parse_Verbose_SetsFlag) {
  const parse_result parsed = parse({"--verbose", "table.csv", "model.tir"});
  ASSERT_TRUE(parsed.has_value()) << parsed.error();
  EXPECT_TRUE(parsed->verbose);
  EXPECT_EQ(parsed->mode, mf::slip_mode::COMBINED);
}

TEST(cli_options, Parse_ShortHelp_ReturnsHelp) {
  const parse_result parsed = parse({"-h"});
  ASSERT_TRUE(parsed.has_value()) << parsed.error();
  EXPECT_EQ(parsed->action, command::HELP);
}

TEST(cli_options, Parse_LongHelp_ReturnsHelp) {
  const parse_result parsed = parse({"--help"});
  ASSERT_TRUE(parsed.has_value()) << parsed.error();
  EXPECT_EQ(parsed->action, command::HELP);
}

TEST(cli_options, Parse_Version_ReturnsVersion) {
  const parse_result alone = parse({"--version"});
  ASSERT_TRUE(alone.has_value()) << alone.error();
  EXPECT_EQ(alone->action, command::VERSION);

  // --version wins over the positional count and over the options in front of it.
  const parse_result with_others = parse({"--verbose", "a", "--version", "b", "c"});
  ASSERT_TRUE(with_others.has_value()) << with_others.error();
  EXPECT_EQ(with_others->action, command::VERSION);
}

TEST(cli_options, Parse_HelpWithWrongPositionalCount_ReturnsHelp) {
  const parse_result none = parse({"--help"});
  ASSERT_TRUE(none.has_value()) << none.error();
  EXPECT_EQ(none->action, command::HELP);

  const parse_result three = parse({"a", "b", "c", "-h"});
  ASSERT_TRUE(three.has_value()) << three.error();
  EXPECT_EQ(three->action, command::HELP);
}

TEST_P(cli_options_wrong_count, Parse_WrongPositionalCount_ReturnsError) {
  const parse_result parsed = parse(GetParam().args);
  ASSERT_FALSE(parsed.has_value());
  EXPECT_EQ(parsed.error(),
            std::format("expected 2 arguments <inputs-table> <file.tir>, got {}", GetParam().positionals));
}

INSTANTIATE_TEST_SUITE_P(counts, cli_options_wrong_count, ::testing::ValuesIn(WRONG_COUNT_CASES), case_name);

TEST(cli_options, Parse_UnknownLongOption_ReturnsError) {
  const parse_result parsed = parse({"table.csv", "--x", "model.tir"});
  ASSERT_FALSE(parsed.has_value());
  EXPECT_EQ(parsed.error(), "unknown option '--x'");
}

TEST(cli_options, Parse_UnknownShortOption_ReturnsError) {
  const parse_result parsed = parse({"-x", "table.csv", "model.tir"});
  ASSERT_FALSE(parsed.has_value());
  EXPECT_EQ(parsed.error(), "unknown option '-x'");
}

TEST(cli_options, Parse_LoneDash_ReturnsError) {
  const parse_result parsed = parse({"table.csv", "-", "model.tir"});
  ASSERT_FALSE(parsed.has_value());
  EXPECT_EQ(parsed.error(), "unknown option '-'");
}

TEST(cli_options, Usage_Text_DocumentsSynopsisColumnsHeaderAndExitCodes) {
  const std::string_view text = mf::cli::usage();
  EXPECT_TRUE(text.starts_with("Usage: mf [options] <inputs-table> <file.tir>")) << text;
  EXPECT_TRUE(text.contains("--uncombined"));
  EXPECT_TRUE(text.contains("--verbose"));
  EXPECT_TRUE(text.contains("--help"));
  EXPECT_TRUE(text.contains("--version"));
  EXPECT_TRUE(text.contains("FZ"));
  EXPECT_TRUE(text.contains("FZ[N]"));
  EXPECT_TRUE(text.contains("SA[rad|deg]"));
  EXPECT_TRUE(text.contains("SR[-]"));
  EXPECT_TRUE(text.contains("IA[rad|deg]"));
  EXPECT_TRUE(text.contains("VX[m/s]"));
  EXPECT_TRUE(text.contains("P[Pa]"));
  EXPECT_TRUE(text.contains("Exit status"));
  ASSERT_FALSE(text.empty());
  EXPECT_EQ(text.back(), '\n');
}

TEST(cli_options, Usage_ContainsPac2002ResultsHeader) {
  EXPECT_TRUE(mf::cli::usage().contains(mf::io::results_header(mf::mf5x::PAC2002_LAYOUT)));
}

TEST(cli_options, Usage_ContainsMf52ResultsHeader) {
  EXPECT_TRUE(mf::cli::usage().contains(mf::io::results_header(mf::mf5x::MF52_LAYOUT)));
}

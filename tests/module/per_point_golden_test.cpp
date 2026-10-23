#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <format>
#include <gtest/gtest.h>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/io/model_loader.hpp>
#include <magic_formula/io/text_file.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Per-point results against the golden files in tests/data/golden (tests/data/README.md, "Golden per-point results"):
// the outputs of the implementation before the index-mapped interface, written as hexfloats.
//
// Platform waiver: on Linux x86-64 (the toolchain that produced the golden files) the per-point results are
// bit-identical. Elsewhere FMA contraction (Clang on arm64) and a different libm or CRT may move a result by a few
// ULP, so the comparison allows PLATFORM_REL_TOL. The 0/1 flags must match exactly everywhere.
namespace {
  using mf::test::data_file;

  /// Columns of a golden file: TIR, the operating point (FZ, SR, SA, IA, VX), the five results and the five flags.
  constexpr std::string_view GOLDEN_HEADER =
      "TIR,FZ,SR,SA,IA,VX,FX,FY,MZ,MX,MY,FZ_CLAMPED,SR_CLAMPED,SA_CLAMPED,IA_CLAMPED,BELOW_VXLOW";
  constexpr std::size_t FIELD_COUNT = 16;

  /// Scaled relative tolerance accepted off Linux x86-64 (FMA contraction, different libm/CRT); tens of ULP can
  /// accumulate through the shape-factor chains.
  [[maybe_unused]] constexpr double PLATFORM_REL_TOL = 1e-12;

  /// Absolute floor of the batch tolerance, so that near-zero results compare on a meaningful scale.
  constexpr double BATCH_SCALE = 1.0;

  /// One data row of a golden file.
  struct golden_row {
    std::string tir;
    /// Inputs in `mf::mf5x::input_index` order.
    Eigen::VectorXd input;
    /// Results then flags in `mf::mf5x::output_index` order.
    Eigen::VectorXd expected;
    /// 1-based line in the file.
    std::size_t line = 0;
  };

  /// Splits `line` at ',' (a trailing ',' yields an empty last field).
  std::vector<std::string_view> split_commas(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t begin = 0;
    while (true) {
      const std::size_t end = line.find(',', begin);
      fields.push_back(line.substr(begin, end - begin));
      if (end == std::string_view::npos) {
        return fields;
      }
      begin = end + 1;
    }
  }

  /// Parses a complete hexfloat (or plain decimal) field; nothing when the field is empty or has trailing characters.
  std::optional<double> parse_double(std::string_view field) {
    const std::string text{field};
    char *end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0') {
      return std::nullopt;
    }
    return value;
  }

  /// Converts one data line; a malformed line is a test failure and yields nothing.
  std::optional<golden_row> parse_row(std::string_view line, std::size_t line_number) {
    const std::vector<std::string_view> fields = split_commas(line);
    if (fields.size() != FIELD_COUNT) {
      ADD_FAILURE() << std::format("line {}: expected {} fields, found {}", line_number, FIELD_COUNT, fields.size());
      return std::nullopt;
    }
    std::array<double, FIELD_COUNT - 1> numbers{};
    for (std::size_t i = 0; i < numbers.size(); ++i) {
      const std::optional<double> value = parse_double(fields[i + 1]);
      if (!value.has_value()) {
        ADD_FAILURE() << std::format("line {}: bad number '{}'", line_number, fields[i + 1]);
        return std::nullopt;
      }
      numbers[i] = *value;
    }
    golden_row row;
    row.tir = std::string{fields[0]};
    row.line = line_number;
    // File order of the point is FZ, SR, SA, IA, VX.
    row.input.resize(mf::mf5x::INPUT_LENGTH);
    row.input(mf::mf5x::FZ) = numbers[0];
    row.input(mf::mf5x::SR) = numbers[1];
    row.input(mf::mf5x::SA) = numbers[2];
    row.input(mf::mf5x::IA) = numbers[3];
    row.input(mf::mf5x::VX) = numbers[4];
    // File order of the outputs is FX, FY, MZ, MX, MY, then the five flags: the output_index order.
    row.expected.resize(mf::mf5x::OUTPUT_LENGTH);
    for (Eigen::Index c = 0; c < mf::mf5x::OUTPUT_LENGTH; ++c) {
      row.expected(c) = numbers[static_cast<std::size_t>(c) + 5];
    }
    return row;
  }

  /// All rows of a golden file; a read or parse failure is a test failure and leaves `rows` short.
  void load_golden(std::string_view name, std::vector<golden_row> &rows) {
    const std::expected<std::string, mf::error> text = mf::io::read_text_file(data_file(name));
    ASSERT_TRUE(text.has_value()) << mf::to_string(text.error());
    std::istringstream stream(*text);
    std::string line;
    ASSERT_TRUE(std::getline(stream, line));
    ASSERT_EQ(line, GOLDEN_HEADER);
    for (std::size_t line_number = 2; std::getline(stream, line); ++line_number) {
      std::optional<golden_row> row = parse_row(line, line_number);
      if (row.has_value()) {
        rows.push_back(std::move(*row));
      }
    }
  }

  /// Loads every distinct model of `rows` (keyed by the TIR column) with `mode` and no throw-on-failure.
  void load_models(const std::vector<golden_row> &rows, mf::slip_mode mode,
                   std::map<std::string, mf::io::loaded_model> &models) {
    for (const golden_row &row : rows) {
      if (models.contains(row.tir)) {
        continue;
      }
      std::expected<mf::io::loaded_model, mf::error> loaded = mf::io::load_tire_model_file(
          data_file(row.tir), mf::model_options{.mode = mode, .throw_if_model_failure = false});
      ASSERT_TRUE(loaded.has_value()) << row.tir << ": " << mf::to_string(loaded.error());
      models.emplace(row.tir, std::move(*loaded));
    }
  }

  /// A per-point result against its golden value: bit-exact on Linux x86-64, within PLATFORM_REL_TOL (scaled)
  /// elsewhere (see the waiver at the top of the file).
  ::testing::AssertionResult matches_golden(double actual, double expected) {
#if defined(__linux__) && defined(__x86_64__)
    if (mf::test::bits(actual) == mf::test::bits(expected)) {
      return ::testing::AssertionSuccess();
    }
#else
    if (mf::test::bits(actual) == mf::test::bits(expected)) {
      return ::testing::AssertionSuccess();
    }
    if (std::isfinite(actual) && std::isfinite(expected) &&
        mf::test::near_scaled(actual, expected, PLATFORM_REL_TOL, BATCH_SCALE)) {
      return ::testing::AssertionSuccess();
    }
#endif
    return ::testing::AssertionFailure() << std::format("actual {:a} ({:.17g}), expected {:a} ({:.17g})", actual,
                                                        actual, expected, expected);
  }

  /// Compares the results of one row with the golden values; the flags must match exactly.
  void expect_row(const golden_row &row, const Eigen::Ref<const Eigen::VectorXd> &actual, std::string_view form) {
    SCOPED_TRACE(std::format("{} line {} {}", row.tir, row.line, form));
    ASSERT_EQ(actual.size(), mf::mf5x::OUTPUT_LENGTH);
    for (Eigen::Index c = 0; c < mf::mf5x::FZ_CLAMPED; ++c) {
      EXPECT_TRUE(matches_golden(actual(c), row.expected(c))) << "output " << c;
    }
    for (Eigen::Index c = mf::mf5x::FZ_CLAMPED; c < mf::mf5x::OUTPUT_LENGTH; ++c) {
      // Flag, exactly 0.0 or 1.0 (bit-equal comparison).
      EXPECT_EQ(mf::test::bits(actual(c)), mf::test::bits(row.expected(c))) << "flag " << c;
    }
  }

  /// Columns (FX, FY, MZ, MX, MY) that a golden file may hold constant: the S fixtures have QSX1 = QSX2 = 0, so MX is
  /// identically zero. The R fixtures allow none.
  using constant_columns = std::vector<Eigen::Index>;
  const constant_columns NO_CONSTANT_COLUMNS{};
  const constant_columns MX_CONSTANT{mf::mf5x::MX};

  /// A bit-exact comparison against a constant column proves nothing: every one of FX, FY, MZ, MX, MY must hold more
  /// than one distinct value in the golden file, except the columns in `allowed_constant`. With `require_nonzero_my`
  /// at least one MY value must also be non-zero.
  void expect_non_degenerate(const std::vector<golden_row> &rows, const constant_columns &allowed_constant,
                             bool require_nonzero_my) {
    for (Eigen::Index c = mf::mf5x::FX; c <= mf::mf5x::MY; ++c) {
      std::set<double> distinct;
      for (const golden_row &row : rows) {
        distinct.insert(row.expected(c));
      }
      const bool allowed = std::ranges::find(allowed_constant, c) != allowed_constant.end();
      if (!allowed) {
        EXPECT_GT(distinct.size(), 1U) << "golden column " << c << " is constant";
      }
    }
    if (require_nonzero_my) {
      const bool any_nonzero =
          std::ranges::any_of(rows, [](const golden_row &row) { return row.expected(mf::mf5x::MY) != 0.0; });
      EXPECT_TRUE(any_nonzero) << "golden MY column is all zero";
    }
  }

  /// Evaluates every row of a golden file per point through the Eigen::Ref form and the raw-pointer form, after
  /// checking that the golden columns are not degenerate.
  void check_per_point(std::string_view name, mf::slip_mode mode,
                       const constant_columns &allowed_constant = NO_CONSTANT_COLUMNS,
                       bool require_nonzero_my = false) {
    std::vector<golden_row> rows;
    load_golden(name, rows);
    ASSERT_FALSE(rows.empty());
    expect_non_degenerate(rows, allowed_constant, require_nonzero_my);
    std::map<std::string, mf::io::loaded_model> models;
    load_models(rows, mode, models);
    ASSERT_FALSE(models.empty());
    for (const golden_row &row : rows) {
      const mf::tire_model &model = *models.at(row.tir).model;
      ASSERT_EQ(model.input_length(), mf::mf5x::INPUT_LENGTH);
      ASSERT_EQ(model.output_length(), mf::mf5x::OUTPUT_LENGTH);

      const Eigen::VectorXd by_ref = model.evaluate(row.input);
      expect_row(row, by_ref, "Eigen::Ref form");

      Eigen::VectorXd by_pointer(mf::mf5x::OUTPUT_LENGTH);
      model.evaluate(row.input.data(), mf::mf5x::INPUT_LENGTH, by_pointer.data(), mf::mf5x::OUTPUT_LENGTH);
      expect_row(row, by_pointer, "raw pointer form");
    }
  }

  /// Evaluates the rows of each model of a golden file as one batch (a file with several TIR files gives one batch per
  /// TIR file) and compares every cell with the golden value within BATCH_REL_TOL.
  void check_batch(std::string_view name, mf::slip_mode mode) {
    std::vector<golden_row> rows;
    load_golden(name, rows);
    ASSERT_FALSE(rows.empty());
    std::map<std::string, mf::io::loaded_model> models;
    load_models(rows, mode, models);
    ASSERT_FALSE(models.empty());
    for (const auto &[tir, loaded] : models) {
      std::vector<const golden_row *> group;
      for (const golden_row &row : rows) {
        if (row.tir == tir) {
          group.push_back(&row);
        }
      }
      ASSERT_FALSE(group.empty());
      Eigen::MatrixXd inputs(static_cast<Eigen::Index>(group.size()), mf::mf5x::INPUT_LENGTH);
      for (std::size_t r = 0; r < group.size(); ++r) {
        inputs.row(static_cast<Eigen::Index>(r)) = group[r]->input.transpose();
      }
      const Eigen::MatrixXd outputs = loaded.model->evaluate_batch(inputs);
      ASSERT_EQ(outputs.rows(), inputs.rows());
      ASSERT_EQ(outputs.cols(), mf::mf5x::OUTPUT_LENGTH);
      for (std::size_t r = 0; r < group.size(); ++r) {
        SCOPED_TRACE(std::format("{} line {} batch", tir, group[r]->line));
        for (Eigen::Index c = 0; c < mf::mf5x::OUTPUT_LENGTH; ++c) {
          EXPECT_TRUE(mf::test::near_scaled(outputs(static_cast<Eigen::Index>(r), c), group[r]->expected(c),
                                            mf::test::BATCH_REL_TOL, BATCH_SCALE))
              << "output " << c;
        }
      }
    }
  }
} // namespace

TEST(per_point_golden, Pac2002Combined_PerPoint_MatchesGolden) {
  check_per_point("golden/per_point_pac2002_combined.csv", mf::slip_mode::COMBINED, MX_CONSTANT);
}

TEST(per_point_golden, Pac2002Uncombined_PerPoint_MatchesGolden) {
  check_per_point("golden/per_point_pac2002_uncombined.csv", mf::slip_mode::UNCOMBINED, MX_CONSTANT);
}

TEST(per_point_golden, Mf52Combined_PerPoint_MatchesGolden) {
  check_per_point("golden/per_point_mf52_combined.csv", mf::slip_mode::COMBINED, MX_CONSTANT);
}

TEST(per_point_golden, Mf52Uncombined_PerPoint_MatchesGolden) {
  check_per_point("golden/per_point_mf52_uncombined.csv", mf::slip_mode::UNCOMBINED, MX_CONSTANT);
}

TEST(per_point_golden, Pac2002Combined_Batch_MatchesGoldenWithinTolerance) {
  check_batch("golden/per_point_pac2002_combined.csv", mf::slip_mode::COMBINED);
}

TEST(per_point_golden, Pac2002Uncombined_Batch_MatchesGoldenWithinTolerance) {
  check_batch("golden/per_point_pac2002_uncombined.csv", mf::slip_mode::UNCOMBINED);
}

TEST(per_point_golden, Mf52Combined_Batch_MatchesGoldenWithinTolerance) {
  check_batch("golden/per_point_mf52_combined.csv", mf::slip_mode::COMBINED);
}

TEST(per_point_golden, Mf52Uncombined_Batch_MatchesGoldenWithinTolerance) {
  check_batch("golden/per_point_mf52_uncombined.csv", mf::slip_mode::UNCOMBINED);
}

TEST(per_point_golden, Pac2002EllCombined_PerPoint_MatchesGolden) {
  check_per_point("golden/per_point_pac2002_ell_combined.csv", mf::slip_mode::COMBINED, MX_CONSTANT);
}

TEST(per_point_golden, Pac2002EllCombined_Batch_MatchesGoldenWithinTolerance) {
  check_batch("golden/per_point_pac2002_ell_combined.csv", mf::slip_mode::COMBINED);
}

TEST(per_point_golden, Pac2002EllUncombined_PerPoint_MatchesGolden) {
  check_per_point("golden/per_point_pac2002_ell_uncombined.csv", mf::slip_mode::UNCOMBINED, MX_CONSTANT);
}

TEST(per_point_golden, Pac2002EllUncombined_Batch_MatchesGoldenWithinTolerance) {
  check_batch("golden/per_point_pac2002_ell_uncombined.csv", mf::slip_mode::UNCOMBINED);
}

// The dense R fixtures: every coefficient family non-zero, so none of the five result columns may be constant.

TEST(per_point_golden, Pac2002RCombined_PerPoint_MatchesGolden) {
  check_per_point("golden/per_point_pac2002_R_combined.csv", mf::slip_mode::COMBINED);
}

TEST(per_point_golden, Pac2002RCombined_Batch_MatchesGoldenWithinTolerance) {
  check_batch("golden/per_point_pac2002_R_combined.csv", mf::slip_mode::COMBINED);
}

TEST(per_point_golden, Pac2002RUncombined_PerPoint_MatchesGolden) {
  check_per_point("golden/per_point_pac2002_R_uncombined.csv", mf::slip_mode::UNCOMBINED);
}

TEST(per_point_golden, Pac2002RUncombined_Batch_MatchesGoldenWithinTolerance) {
  check_batch("golden/per_point_pac2002_R_uncombined.csv", mf::slip_mode::UNCOMBINED);
}

TEST(per_point_golden, Pac2002REllCombined_PerPoint_MatchesGolden) {
  check_per_point("golden/per_point_pac2002_R_ell_combined.csv", mf::slip_mode::COMBINED);
}

TEST(per_point_golden, Pac2002REllCombined_Batch_MatchesGoldenWithinTolerance) {
  check_batch("golden/per_point_pac2002_R_ell_combined.csv", mf::slip_mode::COMBINED);
}

TEST(per_point_golden, Pac2002REllUncombined_PerPoint_MatchesGolden) {
  check_per_point("golden/per_point_pac2002_R_ell_uncombined.csv", mf::slip_mode::UNCOMBINED);
}

TEST(per_point_golden, Pac2002REllUncombined_Batch_MatchesGoldenWithinTolerance) {
  check_batch("golden/per_point_pac2002_R_ell_uncombined.csv", mf::slip_mode::UNCOMBINED);
}

TEST(per_point_golden, Pac2002RFit5Combined_PerPoint_MatchesGolden) {
  check_per_point("golden/per_point_pac2002_R_fit5_combined.csv", mf::slip_mode::COMBINED, NO_CONSTANT_COLUMNS, true);
}

TEST(per_point_golden, Pac2002RFit5Combined_Batch_MatchesGoldenWithinTolerance) {
  check_batch("golden/per_point_pac2002_R_fit5_combined.csv", mf::slip_mode::COMBINED);
}

TEST(per_point_golden, Pac2002RFit5Uncombined_PerPoint_MatchesGolden) {
  check_per_point("golden/per_point_pac2002_R_fit5_uncombined.csv", mf::slip_mode::UNCOMBINED, NO_CONSTANT_COLUMNS,
                  true);
}

TEST(per_point_golden, Pac2002RFit5Uncombined_Batch_MatchesGoldenWithinTolerance) {
  check_batch("golden/per_point_pac2002_R_fit5_uncombined.csv", mf::slip_mode::UNCOMBINED);
}

TEST(per_point_golden, Mf52R52Combined_PerPoint_MatchesGolden) {
  check_per_point("golden/per_point_mf52_R52_combined.csv", mf::slip_mode::COMBINED);
}

TEST(per_point_golden, Mf52R52Combined_Batch_MatchesGoldenWithinTolerance) {
  check_batch("golden/per_point_mf52_R52_combined.csv", mf::slip_mode::COMBINED);
}

TEST(per_point_golden, Mf52R52Uncombined_PerPoint_MatchesGolden) {
  check_per_point("golden/per_point_mf52_R52_uncombined.csv", mf::slip_mode::UNCOMBINED);
}

TEST(per_point_golden, Mf52R52Uncombined_Batch_MatchesGoldenWithinTolerance) {
  check_batch("golden/per_point_mf52_R52_uncombined.csv", mf::slip_mode::UNCOMBINED);
}

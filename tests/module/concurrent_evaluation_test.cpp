#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <gtest/gtest.h>
#include <latch>
#include <magic_formula/coordinate_identification.hpp>
#include <magic_formula/detail/lanes.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/io/input_table.hpp>
#include <magic_formula/io/model_loader.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/model_checks.hpp>
#include <magic_formula/tire_model.hpp>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

// Models are immutable: several threads evaluate one shared model at the same time and get the single-thread results
// bit for bit. Run under ThreadSanitizer (nightly), the test also checks that the const evaluate forms and
// find_coordinate_system share no mutable state.
namespace {
  constexpr int THREAD_COUNT = 4; ///< worker threads sharing one model
  constexpr int REPETITIONS = 8;  ///< evaluations of every form per thread

  /// One model under test: a tests/data .tir file and the options it is loaded with.
  struct concurrency_case {
    std::string_view name; ///< stable test-name suffix
    std::string_view tir;
    mf::model_options options;
  };

  /// Prints the case by name and file (GoogleTest would otherwise print the raw bytes, padding included).
  // NOLINTNEXTLINE(readability-identifier-naming): GoogleTest finds the printer by the name PrintTo
  void PrintTo(const concurrency_case &test_case, std::ostream *os) {
    *os << test_case.name << " (" << test_case.tir << ")";
  }

  /// Results of every const evaluate form for one input batch.
  struct evaluation_results {
    Eigen::MatrixXd batch;           ///< evaluate_batch
    Eigen::MatrixXd per_point;       ///< evaluate(const Eigen::Ref<const Eigen::VectorXd>&) row by row
    Eigen::MatrixXd raw_per_point;   ///< evaluate(const double*, int, double*, int) row by row
    Eigen::MatrixXd characteristics; ///< evaluate_characteristics_batch
  };

  /// Loads `test_case.tir` from tests/data with `test_case.options`; nullptr when loading fails.
  [[nodiscard]] std::unique_ptr<const mf::tire_model> load_model(const concurrency_case &test_case) {
    auto loaded = mf::io::load_tire_model_file(mf::test::data_file(test_case.tir), test_case.options);
    if (!loaded.has_value()) {
      return nullptr;
    }
    return std::move(loaded->model);
  }

  /// Valid operating points for `model`: the rows of inputs_s_vectors.csv with Fz > 0, tiled so that the batch holds
  /// two full blocks of `BLOCK_ROWS` rows and a partial tail block. Empty when the table cannot be read.
  [[nodiscard]] Eigen::MatrixXd valid_inputs(const mf::tire_model &model) {
    const auto table = mf::io::read_model_inputs(mf::test::data_file("inputs_s_vectors.csv"), model.layout().inputs);
    if (!table.has_value()) {
      return Eigen::MatrixXd{};
    }
    const Eigen::MatrixXd &values = table->values;
    Eigen::MatrixXd loaded_rows(values.rows(), values.cols());
    Eigen::Index count = 0;
    for (Eigen::Index r = 0; r < values.rows(); ++r) {
      if (values(r, mf::mf5x::FZ) > 0.0) {
        loaded_rows.row(count) = values.row(r);
        ++count;
      }
    }
    if (count == 0) {
      return Eigen::MatrixXd{};
    }
    const Eigen::Index repeats = ((2 * mf::detail::BLOCK_ROWS) / count) + 1;
    return Eigen::MatrixXd{loaded_rows.topRows(count).replicate(repeats, 1)};
  }

  /// Runs every const evaluate form once on `model` for all rows of `inputs`. Throws nothing for valid inputs.
  [[nodiscard]] evaluation_results evaluate_all(const mf::tire_model &model, const Eigen::MatrixXd &inputs) {
    const Eigen::Index rows = inputs.rows();
    const Eigen::Index outputs = model.output_length();
    evaluation_results results{.batch = model.evaluate_batch(inputs),
                               .per_point = Eigen::MatrixXd(rows, outputs),
                               .raw_per_point = Eigen::MatrixXd(rows, outputs),
                               .characteristics = model.evaluate_characteristics_batch(inputs)};
    for (Eigen::Index r = 0; r < rows; ++r) {
      const Eigen::VectorXd input = inputs.row(r).transpose();
      results.per_point.row(r) = model.evaluate(input).transpose();

      Eigen::VectorXd output(outputs);
      model.evaluate(input.data(), static_cast<int>(input.size()), output.data(), static_cast<int>(output.size()));
      results.raw_per_point.row(r) = output.transpose();
    }
    return results;
  }

  /// Success iff `actual` and `expected` have the same shape and the same bit pattern at every element.
  [[nodiscard]] ::testing::AssertionResult bit_equal(const Eigen::MatrixXd &actual, const Eigen::MatrixXd &expected) {
    if (actual.rows() != expected.rows() || actual.cols() != expected.cols()) {
      const std::string message =
          std::format("shape {}x{} vs {}x{}", actual.rows(), actual.cols(), expected.rows(), expected.cols());
      return ::testing::AssertionFailure() << message;
    }
    for (Eigen::Index j = 0; j < actual.cols(); ++j) {
      for (Eigen::Index i = 0; i < actual.rows(); ++i) {
        if (mf::test::bits(actual(i, j)) != mf::test::bits(expected(i, j))) {
          const std::string message =
              std::format("element ({}, {}): actual {:.17g}, expected {:.17g}", i, j, actual(i, j), expected(i, j));
          return ::testing::AssertionFailure() << message;
        }
      }
    }
    return ::testing::AssertionSuccess();
  }

  /// Success iff the classification, the effective and decided signs, the decision bases, the confidences (bit for
  /// bit) and the number of readings agree; the failure names the first differing member.
  [[nodiscard]] ::testing::AssertionResult same_identification(const mf::coordinate_identification &actual,
                                                               const mf::coordinate_identification &expected) {
    const bool same_vertical_confidence =
        mf::test::bits(actual.vertical.confidence) == mf::test::bits(expected.vertical.confidence);
    const bool same_lateral_confidence =
        mf::test::bits(actual.lateral.confidence) == mf::test::bits(expected.lateral.confidence);
    const std::array<std::pair<std::string_view, bool>, 10> checks{{
        {"classification", actual.classification == expected.classification},
        {"effective.vertical", actual.effective.vertical == expected.effective.vertical},
        {"effective.lateral", actual.effective.lateral == expected.effective.lateral},
        {"vertical.sign", actual.vertical.sign == expected.vertical.sign},
        {"lateral.sign", actual.lateral.sign == expected.lateral.sign},
        {"vertical.basis", actual.vertical.basis == expected.vertical.basis},
        {"lateral.basis", actual.lateral.basis == expected.lateral.basis},
        {"vertical.confidence", same_vertical_confidence},
        {"lateral.confidence", same_lateral_confidence},
        {"readings.size", actual.readings.size() == expected.readings.size()},
    }};
    // NOLINTNEXTLINE(readability-qualified-auto): the array iterator is a class type on some standard libraries
    const auto mismatch = std::ranges::find_if(checks, [](const auto &check) { return !check.second; });
    if (mismatch != checks.end()) {
      const std::string message = std::format("{} differs", mismatch->first);
      return ::testing::AssertionFailure() << message;
    }
    return ::testing::AssertionSuccess();
  }
} // namespace

class concurrent_evaluation : public ::testing::TestWithParam<concurrency_case> {};

// A main-thread reference first (it proves no exception can escape a worker), then THREAD_COUNT workers released
// together evaluate the same const model REPETITIONS times. Each worker writes only its own slots; every assertion
// runs on the main thread after the join.
TEST_P(concurrent_evaluation, SharedModel_EvaluatedFromFourThreads_MatchesSingleThreadBitForBit) {
  const std::unique_ptr<const mf::tire_model> model = load_model(GetParam());
  ASSERT_TRUE(model != nullptr);

  const Eigen::MatrixXd inputs = valid_inputs(*model);
  ASSERT_GT(inputs.rows(), mf::detail::BLOCK_ROWS);
  ASSERT_NE(inputs.rows() % mf::detail::BLOCK_ROWS, 0); // a tail block exists
  ASSERT_TRUE(mf::validate_inputs(*model, inputs).has_value());

  evaluation_results reference;
  ASSERT_NO_THROW(reference = evaluate_all(*model, inputs));
  const mf::coordinate_identification reference_identification = model->find_coordinate_system();

  std::vector<std::vector<evaluation_results>> results(THREAD_COUNT, std::vector<evaluation_results>(REPETITIONS));
  std::vector<mf::coordinate_identification> identifications(THREAD_COUNT);
  std::latch start{THREAD_COUNT};
  {
    std::vector<std::jthread> workers;
    workers.reserve(THREAD_COUNT);
    for (int t = 0; t < THREAD_COUNT; ++t) {
      workers.emplace_back([&shared = *model, &inputs, &results, &identifications, &start, t] {
        const auto thread_index = static_cast<std::size_t>(t);
        start.arrive_and_wait();
        for (std::size_t rep = 0; rep < static_cast<std::size_t>(REPETITIONS); ++rep) {
          results[thread_index][rep] = evaluate_all(shared, inputs);
        }
        identifications[thread_index] = shared.find_coordinate_system();
      });
    }
  } // leaving the scope joins every worker

  for (std::size_t t = 0; t < static_cast<std::size_t>(THREAD_COUNT); ++t) {
    for (std::size_t rep = 0; rep < static_cast<std::size_t>(REPETITIONS); ++rep) {
      const evaluation_results &actual = results[t][rep];
      EXPECT_TRUE(bit_equal(actual.batch, reference.batch)) << "batch, thread " << t << " repetition " << rep;
      EXPECT_TRUE(bit_equal(actual.per_point, reference.per_point))
          << "per_point, thread " << t << " repetition " << rep;
      EXPECT_TRUE(bit_equal(actual.raw_per_point, reference.raw_per_point))
          << "raw_per_point, thread " << t << " repetition " << rep;
      EXPECT_TRUE(bit_equal(actual.characteristics, reference.characteristics))
          << "characteristics, thread " << t << " repetition " << rep;
    }
    EXPECT_TRUE(same_identification(identifications[t], reference_identification)) << "thread " << t;
  }
}

INSTANTIATE_TEST_SUITE_P(
    models, concurrent_evaluation,
    ::testing::Values(concurrency_case{.name = "Pac2002_Default", .tir = "pac2002_S.tir", .options = {}},
                      concurrency_case{.name = "Pac2002_ThrowIfModelFailure",
                                       .tir = "pac2002_S.tir",
                                       .options = {.throw_if_model_failure = true}},
                      concurrency_case{.name = "Mf52_Default", .tir = "mf52_S52.tir", .options = {}},
                      concurrency_case{.name = "Mf52_Uncombined",
                                       .tir = "mf52_S52.tir",
                                       .options = {.mode = mf::slip_mode::UNCOMBINED}}),
    [](const ::testing::TestParamInfo<concurrency_case> &param_info) { return std::string(param_info.param.name); });

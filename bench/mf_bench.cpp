#include <Eigen/Core>
#include <benchmark/benchmark.h>
#include <cmath>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <magic_formula/coordinate_identification.hpp>
#include <magic_formula/coordinate_system.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/io/model_loader.hpp>
#include <magic_formula/io/tir_conversion.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/tire_model.hpp>
#include <numbers>
#include <string>
#include <string_view>

namespace {

  /// PAC2002 property file (cosine combined slip, no ranges).
  constexpr std::string_view PAC2002_TIR = "pac2002_S.tir";
  /// MF 5.2 property file (FITTYP 6).
  constexpr std::string_view MF52_TIR = "mf52_S52.tir";
  /// PAC2002 property file of a consistent ISO set.
  constexpr std::string_view PAC2002_ISO_TIR = "pac2002_S_iso.tir";

  using load_result = std::expected<mf::io::loaded_model, mf::error>;

  /// Loads tests/data/<file_name> with combined slip and the given check policy.
  [[nodiscard]] load_result load_model(std::string_view file_name, bool throw_if_model_failure) {
    const std::filesystem::path path =
        std::filesystem::path{MF_TEST_DATA_DIR} / std::filesystem::path{std::string{file_name}};
    return mf::io::load_tire_model_file(
        path, mf::model_options{.mode = mf::slip_mode::COMBINED, .throw_if_model_failure = throw_if_model_failure});
  }

  /// PAC2002 model without checks, loaded on first use.
  [[nodiscard]] const load_result &pac2002_unchecked() {
    static const load_result loaded = load_model(PAC2002_TIR, false);
    return loaded;
  }

  /// PAC2002 model with throw_if_model_failure, loaded on first use.
  [[nodiscard]] const load_result &pac2002_checked() {
    static const load_result loaded = load_model(PAC2002_TIR, true);
    return loaded;
  }

  /// MF 5.2 model without checks, loaded on first use.
  [[nodiscard]] const load_result &mf52_unchecked() {
    static const load_result loaded = load_model(MF52_TIR, false);
    return loaded;
  }

  /// `points` deterministic operating points, one row per point and one column per input index (column-major).
  [[nodiscard]] Eigen::MatrixXd make_batch_inputs(Eigen::Index points) {
    constexpr double two_pi = 2.0 * std::numbers::pi;
    Eigen::MatrixXd inputs(points, mf::mf5x::INPUT_LENGTH);
    for (Eigen::Index i = 0; i < points; ++i) {
      const double phase = (static_cast<double>(i) + 0.5) / static_cast<double>(points);
      inputs(i, mf::mf5x::FZ) = 1000.0 + (5000.0 * phase);
      inputs(i, mf::mf5x::SA) = 0.2 * std::sin(two_pi * 3.0 * phase);
      inputs(i, mf::mf5x::SR) = 0.15 * std::sin((two_pi * 5.0 * phase) + 0.7);
      inputs(i, mf::mf5x::IA) = 0.05 * std::cos(two_pi * 2.0 * phase);
      inputs(i, mf::mf5x::VX) = 10.0 + (20.0 * phase);
    }
    return inputs;
  }

  /// The same points with one column per point (input_length() x points).
  [[nodiscard]] Eigen::MatrixXd make_point_inputs(Eigen::Index points) {
    Eigen::MatrixXd result = make_batch_inputs(points).transpose();
    return result;
  }

  using runner = void (*)(benchmark::State &, const mf::tire_model &);

  /// Checks the cached model, runs one untimed probe and then the runner.
  void run_with(benchmark::State &state, const load_result &loaded, std::string_view expected_name, runner run) {
    if (!loaded.has_value()) {
      state.SkipWithError(mf::to_string(loaded.error()));
      return;
    }
    const mf::tire_model &model = *loaded->model;
    if (model.name() != expected_name) {
      state.SkipWithError(std::string{"expected model "} + std::string{expected_name} + ", got " +
                          std::string{model.name()});
      return;
    }
    try {
      const Eigen::MatrixXd probe = model.evaluate_batch(make_batch_inputs(static_cast<Eigen::Index>(state.range(0))));
      if (!probe.allFinite()) {
        state.SkipWithError("non-finite outputs for the benchmark inputs");
        return;
      }
    } catch (const mf::model_failure &failure) {
      state.SkipWithError(failure.what());
      return;
    }
    run(state, model);
  }

  /// Per-point evaluation through the Eigen-returning form.
  void run_point_eigen(benchmark::State &state, const mf::tire_model &model) {
    const Eigen::Index points = static_cast<Eigen::Index>(state.range(0));
    const Eigen::MatrixXd inputs = make_point_inputs(points);
    for (auto _ : state) {
      for (Eigen::Index p = 0; p < points; ++p) {
        Eigen::VectorXd y = model.evaluate(inputs.col(p));
        benchmark::DoNotOptimize(y);
      }
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(points));
  }

  /// Per-point evaluation through the raw-pointer form.
  void run_point_raw(benchmark::State &state, const mf::tire_model &model) {
    const Eigen::Index points = static_cast<Eigen::Index>(state.range(0));
    const int input_len = model.input_length();
    const int output_len = model.output_length();
    const Eigen::MatrixXd inputs = make_point_inputs(points);
    Eigen::MatrixXd outputs(output_len, points);
    for (auto _ : state) {
      for (Eigen::Index p = 0; p < points; ++p) {
        model.evaluate(inputs.col(p).data(), input_len, outputs.col(p).data(), output_len);
      }
      double *sink = outputs.data();
      benchmark::DoNotOptimize(sink);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(points));
  }

  /// Batch evaluation through the Eigen-returning form.
  void run_batch_eigen(benchmark::State &state, const mf::tire_model &model) {
    const Eigen::Index points = static_cast<Eigen::Index>(state.range(0));
    const Eigen::MatrixXd inputs = make_batch_inputs(points);
    for (auto _ : state) {
      Eigen::MatrixXd outputs = model.evaluate_batch(inputs);
      benchmark::DoNotOptimize(outputs);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(points));
  }

  /// Batch evaluation through the raw-pointer form (column-major buffers).
  void run_batch_raw(benchmark::State &state, const mf::tire_model &model) {
    const Eigen::Index points = static_cast<Eigen::Index>(state.range(0));
    const int output_len = model.output_length();
    const Eigen::MatrixXd inputs = make_batch_inputs(points);
    Eigen::MatrixXd outputs(points, output_len);
    const int rows = static_cast<int>(points);
    const int len = static_cast<int>(points * output_len);
    for (auto _ : state) {
      model.evaluate_batch(inputs.data(), rows, outputs.data(), len);
      double *sink = outputs.data();
      benchmark::DoNotOptimize(sink);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(points));
  }

  /// PAC2002, per point, Eigen form.
  void BM_Pac2002_EvaluatePointEigen(benchmark::State &state) {
    run_with(state, pac2002_unchecked(), "PAC2002", run_point_eigen);
  }

  /// PAC2002, per point, raw form.
  void BM_Pac2002_EvaluatePointRaw(benchmark::State &state) {
    run_with(state, pac2002_unchecked(), "PAC2002", run_point_raw);
  }

  /// PAC2002, batch, Eigen form.
  void BM_Pac2002_EvaluateBatchEigen(benchmark::State &state) {
    run_with(state, pac2002_unchecked(), "PAC2002", run_batch_eigen);
  }

  /// PAC2002, batch, raw form.
  void BM_Pac2002_EvaluateBatchRaw(benchmark::State &state) {
    run_with(state, pac2002_unchecked(), "PAC2002", run_batch_raw);
  }

  /// PAC2002, batch, raw form, with throw_if_model_failure enabled.
  void BM_Pac2002_EvaluateBatchChecked(benchmark::State &state) {
    run_with(state, pac2002_checked(), "PAC2002", run_batch_raw);
  }

  /// MF 5.2, per point, Eigen form.
  void BM_Mf52_EvaluatePointEigen(benchmark::State &state) {
    run_with(state, mf52_unchecked(), "MF 5.2", run_point_eigen);
  }

  /// MF 5.2, per point, raw form.
  void BM_Mf52_EvaluatePointRaw(benchmark::State &state) {
    run_with(state, mf52_unchecked(), "MF 5.2", run_point_raw);
  }

  /// MF 5.2, batch, Eigen form.
  void BM_Mf52_EvaluateBatchEigen(benchmark::State &state) {
    run_with(state, mf52_unchecked(), "MF 5.2", run_batch_eigen);
  }

  /// MF 5.2, batch, raw form.
  void BM_Mf52_EvaluateBatchRaw(benchmark::State &state) {
    run_with(state, mf52_unchecked(), "MF 5.2", run_batch_raw);
  }

  /// PAC2002 model of pac2002_S_iso.tir (consistent ISO), loaded on first use.
  [[nodiscard]] const load_result &pac2002_iso() {
    static const load_result loaded = load_model(PAC2002_ISO_TIR, false);
    return loaded;
  }

  /// `rows` loaded ISO points (FZ 2000..6000 N, SA +-0.2 rad, SR +-0.1, IA 0, VX 16.6 m/s).
  [[nodiscard]] Eigen::MatrixXd make_table_inputs(Eigen::Index rows) {
    Eigen::MatrixXd inputs(rows, mf::mf5x::INPUT_LENGTH);
    for (Eigen::Index i = 0; i < rows; ++i) {
      const double phase = (static_cast<double>(i) + 0.5) / static_cast<double>(rows);
      inputs(i, mf::mf5x::FZ) = 2000.0 + (4000.0 * phase);
      inputs(i, mf::mf5x::SA) = -0.2 + (0.4 * phase);
      inputs(i, mf::mf5x::SR) = -0.1 + (0.2 * (1.0 - phase));
      inputs(i, mf::mf5x::IA) = 0.0;
      inputs(i, mf::mf5x::VX) = 16.6;
    }
    return inputs;
  }

  /// Unchecked `evaluate_batch` on an ISO table of the consistent ISO model.
  void BM_EvaluateBatch_IsoTable(benchmark::State &state) {
    const load_result &loaded = pac2002_iso();
    if (!loaded.has_value()) {
      state.SkipWithError(mf::to_string(loaded.error()));
      return;
    }
    const mf::tire_model &model = *loaded->model;
    const Eigen::Index rows = static_cast<Eigen::Index>(state.range(0));
    const Eigen::MatrixXd inputs = make_table_inputs(rows);
    for (auto _ : state) {
      Eigen::MatrixXd outputs = model.evaluate_batch(inputs);
      benchmark::DoNotOptimize(outputs);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(rows));
  }

  /// Coordinate-system identification of the model (29 readings from one batch of 55 probe rows).
  void BM_FindCoordinateSystem(benchmark::State &state) {
    const load_result &loaded = pac2002_iso();
    if (!loaded.has_value()) {
      state.SkipWithError(mf::to_string(loaded.error()));
      return;
    }
    const mf::tire_model &model = *loaded->model;
    for (auto _ : state) {
      mf::coordinate_identification identification = model.find_coordinate_system();
      benchmark::DoNotOptimize(identification);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations());
  }

  /// Model conversion to SAE (identification, plan and coefficient sign flips).
  void BM_ConvertCoordinateSystem(benchmark::State &state) {
    const load_result &loaded = pac2002_iso();
    if (!loaded.has_value()) {
      state.SkipWithError(mf::to_string(loaded.error()));
      return;
    }
    const mf::tire_model &model = *loaded->model;
    for (auto _ : state) {
      mf::model_conversion conversion = model.convert_coordinate_system(mf::coordinate_system::SAE);
      benchmark::DoNotOptimize(conversion);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations());
  }

  /// Text conversion of pac2002_S_iso.tir to SAE (parse, load, identification, splicing).
  void BM_ConvertTir(benchmark::State &state) {
    const std::filesystem::path path =
        std::filesystem::path{MF_TEST_DATA_DIR} / std::filesystem::path{std::string{PAC2002_ISO_TIR}};
    std::ifstream stream{path};
    if (!stream.is_open()) {
      state.SkipWithError("cannot open " + path.string());
      return;
    }
    const std::string text{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
    for (auto _ : state) {
      auto converted = mf::io::convert_tir(text, mf::coordinate_system::SAE, std::string{PAC2002_ISO_TIR});
      if (!converted.has_value()) {
        state.SkipWithError(mf::to_string(converted.error()));
        return;
      }
      benchmark::DoNotOptimize(converted);
      benchmark::ClobberMemory();
    }
    state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(text.size()));
  }

} // namespace

BENCHMARK(BM_EvaluateBatch_IsoTable)->ArgName("rows")->Arg(64)->Arg(4096)->Arg(65536);
BENCHMARK(BM_FindCoordinateSystem);
BENCHMARK(BM_ConvertCoordinateSystem);
BENCHMARK(BM_ConvertTir);
BENCHMARK(BM_Pac2002_EvaluatePointEigen)->ArgName("points")->Arg(1)->Arg(8)->Arg(64)->Arg(1024)->Arg(16384);
BENCHMARK(BM_Pac2002_EvaluatePointRaw)->ArgName("points")->Arg(1)->Arg(8)->Arg(64)->Arg(1024)->Arg(16384);
BENCHMARK(BM_Pac2002_EvaluateBatchEigen)->ArgName("points")->Arg(1)->Arg(8)->Arg(64)->Arg(1024)->Arg(16384);
BENCHMARK(BM_Pac2002_EvaluateBatchRaw)->ArgName("points")->Arg(1)->Arg(8)->Arg(64)->Arg(1024)->Arg(16384);
BENCHMARK(BM_Pac2002_EvaluateBatchChecked)->ArgName("points")->Arg(1)->Arg(8)->Arg(64)->Arg(1024)->Arg(16384);
BENCHMARK(BM_Mf52_EvaluatePointEigen)->ArgName("points")->Arg(1)->Arg(8)->Arg(64)->Arg(1024)->Arg(16384);
BENCHMARK(BM_Mf52_EvaluatePointRaw)->ArgName("points")->Arg(1)->Arg(8)->Arg(64)->Arg(1024)->Arg(16384);
BENCHMARK(BM_Mf52_EvaluateBatchEigen)->ArgName("points")->Arg(1)->Arg(8)->Arg(64)->Arg(1024)->Arg(16384);
BENCHMARK(BM_Mf52_EvaluateBatchRaw)->ArgName("points")->Arg(1)->Arg(8)->Arg(64)->Arg(1024)->Arg(16384);

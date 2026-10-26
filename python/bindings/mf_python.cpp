// pybind11 adapter that exposes the magic-formula library to Python as `magic_formula._core`.
// It converts types only: library errors are returned as values (`Error`), never thrown, and nothing is caught here.
// pybind11's dispatcher is the exception boundary (argument conversion failures, std::bad_alloc).

#include <Eigen/Core>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <magic_formula/coordinate_identification.hpp>
#include <magic_formula/coordinate_system.hpp>
#include <magic_formula/detail/text.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/io/model_loader.hpp>
#include <magic_formula/io/tir_document.hpp>
#include <magic_formula/model_checks.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/tire_model.hpp>
#include <magic_formula/version.hpp>
#include <memory>
#include <optional>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace mf::python {
  namespace py = pybind11;

  /// Inputs of `model_handle::evaluate_batch` and `convert`: float64 in Fortran (column-major) order,
  /// the layout of Eigen::MatrixXd.
  /// pybind11 converts any other array-like (C order, other dtypes, nested lists) and copies only when needed.
  using input_array = py::array_t<double, py::array::f_style | py::array::forcecast>;

  /// A recoverable library error as Python sees it (bound as `_core.Error`; read-only attributes).
  struct error_record {
    std::string code;               ///< error_code_name(cause.code), e.g. "INVALID_INPUT"
    std::string message;            ///< cause.message
    std::string text;               ///< model_failure{cause, row}.what(): "[row R: ][<file>:<line>: ]<message>"
    std::string file;               ///< cause.where.file ("" = not applicable)
    std::size_t line = 0;           ///< cause.where.line (0 = not applicable)
    std::string key;                ///< cause.where.key ("" = not applicable)
    std::optional<std::size_t> row; ///< failing batch row; nullopt for load and shape errors
  };

  /// The record of `cause`. `text` reuses the library's own formatting by constructing (never throwing)
  /// `mf::model_failure{cause, row}` and copying `what()`, so the "row R: " prefix is not duplicated.
  [[nodiscard]] error_record to_record(const error &cause, std::optional<std::size_t> row = std::nullopt) {
    const model_failure failure(cause, row);
    return error_record{.code = std::string(error_code_name(cause.code)),
                        .message = cause.message,
                        .text = failure.what(),
                        .file = cause.where.file,
                        .line = cause.where.line,
                        .key = cause.where.key,
                        .row = row};
  }

  /// Moves `matrix` to the heap (std::unique_ptr) and returns a NumPy array of shape (rows, cols) and Fortran strides
  /// {8, 8*rows} that views it and owns it through a py::capsule (the deleter reclaims it in a std::unique_ptr;
  /// release() only after the capsule exists). No copy of the values. 0 rows give an empty (0, cols) array.
  [[nodiscard]] py::array_t<double> to_array(Eigen::MatrixXd matrix) {
    auto owned = std::make_unique<Eigen::MatrixXd>(std::move(matrix));
    const auto rows = static_cast<py::ssize_t>(owned->rows());
    const auto cols = static_cast<py::ssize_t>(owned->cols());
    const auto itemsize = static_cast<py::ssize_t>(sizeof(double));
    double *const data = owned->data();
    const py::capsule owner(owned.get(), [](void *pointer) {
      const std::unique_ptr<Eigen::MatrixXd> reclaimed(static_cast<Eigen::MatrixXd *>(pointer));
    });
    static_cast<void>(owned.release());
    // With 0 rows the data pointer may be null: pybind11 then allocates the empty (0, cols) array itself and drops the
    // capsule, whose deleter frees the empty matrix.
    return py::array_t<double>({rows, cols}, {itemsize, itemsize * rows}, data, owner);
  }

  namespace {
    /// Options of every model the adapter builds: the requested slip mode, never throwing.
    [[nodiscard]] model_options options_for(bool combined) {
      return model_options{.mode = combined ? slip_mode::COMBINED : slip_mode::UNCOMBINED,
                           .throw_if_model_failure = false};
    }

    /// "FZ, SA, SR, IA, VX": the input channel names of `model`, in layout order.
    [[nodiscard]] std::string channel_list(const tire_model &model) {
      std::vector<std::string> names;
      for (const input_channel &channel : model.layout().inputs) {
        names.emplace_back(channel.name);
      }
      return detail::join(names, ", ");
    }

    /// An INVALID_ARGUMENT record without row and location.
    [[nodiscard]] error_record shape_error(std::string message) {
      return to_record(error{.code = error_code::INVALID_ARGUMENT, .message = std::move(message), .where = {}});
    }
  } // namespace

  /// The vote of one axis (bound as `_core.Decision`; read-only attributes).
  struct decision_record {
    /// axis_sign_name: POSITIVE (the ISO side), NEGATIVE or UNDETERMINED.
    std::string sign;
    /// decision_basis_name.
    std::string basis;
    /// Share of the valid vote weight held by the winning side.
    double confidence = 0.0;
    /// Total weight voting for the ISO side.
    double positive_weight = 0.0;
    /// Total weight voting for the SAE side.
    double negative_weight = 0.0;
  };

  /// One evaluated indicator at one probe load (bound as `_core.Reading`; read-only attributes).
  struct reading_record {
    /// Indicator name.
    std::string name;
    /// The physical expectation behind the vote.
    std::string meaning;
    /// Unit of `value`.
    std::string unit;
    /// identification_axis_name: VERTICAL, LATERAL or CHECK.
    std::string axis;
    /// Probe load as a multiple of the reference FZ.
    double load_scale = 1.0;
    /// Probe FZ [N].
    double load = 0.0;
    /// Indicator value in `unit`.
    double value = 0.0;
    /// value / |load|.
    double normalised = 0.0;
    /// reading_status_name.
    std::string status;
    /// +1, -1 or 0 (abstains).
    int vote = 0;
    /// Vote weight of this reading.
    double weight = 0.0;
  };

  /// The coordinate-system identification of a model (bound as `_core.Identification`; read-only attributes).
  struct identification_record {
    /// system_classification_name: ISO, SAE or MIXED.
    std::string classification;
    /// Vertical-axis decision.
    decision_record vertical;
    /// Lateral-axis decision.
    decision_record lateral;
    /// axis_sign_name of the vertical sign used for classification.
    std::string effective_vertical;
    /// axis_sign_name of the lateral sign used for classification.
    std::string effective_lateral;
    /// Every reading, table order then load order.
    std::vector<reading_record> readings;
    /// Reference operating point, in input order.
    std::vector<double> reference_input;
    /// to_string of every identification warning.
    std::vector<std::string> warnings;
  };

  /// The result of `model_handle::evaluate_batch` or `model_handle::evaluate_characteristics_batch` (bound as
  /// `_core.Solution`; read-only attributes).
  struct solution_record {
    /// rows x output_length() (evaluate_batch) or rows x characteristics_length() (evaluate_characteristics_batch).
    /// For characteristics, NaN is a documented value only in SA_AT_PEAK_FY and SR_AT_PEAK_FX (no stationary point, or
    /// no contact); FY_OVER_SA and FX_OVER_SR at zero slip are the tangent slopes DFY_DSA and DFX_DSR, and 0 for rows
    /// without contact.
    py::array_t<double> outputs;
    std::vector<std::string> warnings; ///< "row <index>: <message>" per table warning
  };

  namespace {
    /// The decision as a plain record.
    [[nodiscard]] decision_record to_decision_record(const axis_decision &decision) {
      return decision_record{.sign = std::string(axis_sign_name(decision.sign)),
                             .basis = std::string(decision_basis_name(decision.basis)),
                             .confidence = decision.confidence,
                             .positive_weight = decision.positive_weight,
                             .negative_weight = decision.negative_weight};
    }

    /// The reading as a plain record.
    [[nodiscard]] reading_record to_reading_record(const indicator_reading &reading) {
      return reading_record{.name = std::string(reading.definition.name),
                            .meaning = std::string(reading.definition.meaning),
                            .unit = std::string(reading.definition.unit),
                            .axis = std::string(identification_axis_name(reading.definition.axis)),
                            .load_scale = reading.load_scale,
                            .load = reading.load_n,
                            .value = reading.value,
                            .normalised = reading.normalised,
                            .status = std::string(reading_status_name(reading.status)),
                            .vote = reading.vote,
                            .weight = reading.weight};
    }
  } // namespace

  /// A loaded, immutable tire model and its load diagnostics (bound as `_core.Model`; no Python constructor).
  class model_handle {
  public:
    /// Takes ownership of a successfully loaded model (`loaded.model` is not null).
    explicit model_handle(io::loaded_model loaded) noexcept : loaded_(std::move(loaded)) {}

    /// The wrapped model.
    [[nodiscard]] const tire_model &model() const noexcept {
      return *loaded_.model;
    }

    /// `model().name()`: "PAC2002" or "MF 5.2".
    [[nodiscard]] std::string name() const {
      return std::string(model().name());
    }

    /// (name, unit) of every input channel, in input_index order.
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> inputs() const {
      std::vector<std::pair<std::string, std::string>> result;
      for (const input_channel &channel : model().layout().inputs) {
        result.emplace_back(std::string(channel.name), std::string(channel.unit));
      }
      return result;
    }

    /// (name, unit) of every output channel, in output_index order.
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> outputs() const {
      std::vector<std::pair<std::string, std::string>> result;
      for (const output_channel &channel : model().layout().outputs) {
        result.emplace_back(std::string(channel.name), std::string(channel.unit));
      }
      return result;
    }

    /// (name, unit) of every characteristic channel, in characteristic_index order.
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> characteristics() const {
      std::vector<std::pair<std::string, std::string>> result;
      for (const characteristic_channel &channel : model().layout().characteristics) {
        result.emplace_back(std::string(channel.name), std::string(channel.unit));
      }
      return result;
    }

    /// (key, value) of every limit channel, in limit_index order; absent bounds are -inf / +inf.
    [[nodiscard]] std::vector<std::pair<std::string, double>> limits() const {
      std::vector<std::pair<std::string, double>> result;
      const auto channels = model().layout().limits;
      for (std::size_t i = 0; i < channels.size(); ++i) {
        result.emplace_back(std::string(channels[i].key), model().limits()(static_cast<Eigen::Index>(i)));
      }
      return result;
    }

    /// [MODEL] FITTYP, when present.
    [[nodiscard]] std::optional<int> fittyp() const noexcept {
      return model().metadata().fittyp;
    }

    /// [MODEL] USE_MODE, when present.
    [[nodiscard]] std::optional<int> use_mode() const noexcept {
      return model().metadata().use_mode;
    }

    /// "LEFT" or "RIGHT".
    [[nodiscard]] std::string tyre_side() const {
      return model().metadata().side == ::mf::tyre_side::RIGHT ? "RIGHT" : "LEFT";
    }

    /// `model().options().mode == slip_mode::COMBINED`.
    [[nodiscard]] bool combined() const noexcept {
      return model().options().mode == slip_mode::COMBINED;
    }

    /// `to_string(w)` of every load warning, in load order.
    [[nodiscard]] std::vector<std::string> warnings() const {
      std::vector<std::string> result;
      for (const warning &entry : loaded_.warnings) {
        result.push_back(to_string(entry));
      }
      return result;
    }

    /// Keys present in the file but used by nothing, in file order.
    [[nodiscard]] const std::vector<std::string> &ignored_keys() const noexcept {
      return loaded_.ignored_keys;
    }

    /// `model().find_coordinate_system()` as a plain record (signs, statuses and classification as enumerator names,
    /// warnings as `to_string(warning)`).
    [[nodiscard]] identification_record find_coordinate_system() const {
      const coordinate_identification found = model().find_coordinate_system();
      identification_record record;
      record.classification = std::string(system_classification_name(found.classification));
      record.vertical = to_decision_record(found.vertical);
      record.lateral = to_decision_record(found.lateral);
      record.effective_vertical = std::string(axis_sign_name(found.effective.vertical));
      record.effective_lateral = std::string(axis_sign_name(found.effective.lateral));
      for (const indicator_reading &reading : found.readings) {
        record.readings.push_back(to_reading_record(reading));
      }
      record.reference_input.assign(found.reference_input.data(),
                                    found.reference_input.data() + found.reference_input.size());
      for (const warning &entry : found.warnings) {
        record.warnings.push_back(to_string(entry));
      }
      return record;
    }

    /// A model of the same formulation converted to `target` ("ISO" or "SAE", any case) by coefficient sign flips
    /// (the equations never change), carrying this handle's load warnings and ignored keys; or an INVALID_ARGUMENT
    /// record for an unknown system.
    [[nodiscard]] std::variant<model_handle, error_record> convert_coordinate_system(const std::string &target) const {
      const auto system = parse_coordinate_system(target);
      if (!system.has_value()) {
        return shape_error(std::format("unknown coordinate system '{}' (expected ISO or SAE)", target));
      }
      model_conversion conversion = model().convert_coordinate_system(*system);
      io::loaded_model converted;
      converted.model = std::move(conversion.model);
      converted.warnings = loaded_.warnings;
      converted.ignored_keys = loaded_.ignored_keys;
      return model_handle(std::move(converted));
    }

    /// Converts a data table between ISO and SAE (never used inside evaluation). `kind` is "inputs" (input_length()
    /// columns) or "outputs" (output_length() columns); `source` and `target` are ISO or SAE. Returns a converted copy
    /// (sign flips only: FZ for inputs; FY, MZ and MY for outputs), or an INVALID_ARGUMENT record for an unknown kind
    /// or system or a wrong shape.
    [[nodiscard]] std::variant<py::array_t<double>, error_record> convert(const input_array &values,
                                                                          const std::string &kind,
                                                                          const std::string &source,
                                                                          const std::string &target) const {
      const bool is_inputs = kind == "inputs";
      if (!is_inputs && kind != "outputs") {
        return shape_error(std::format("unknown kind '{}' (expected inputs or outputs)", kind));
      }
      const auto from = parse_coordinate_system(source);
      if (!from.has_value()) {
        return shape_error(std::format("unknown coordinate system '{}' (expected ISO or SAE)", source));
      }
      const auto to = parse_coordinate_system(target);
      if (!to.has_value()) {
        return shape_error(std::format("unknown coordinate system '{}' (expected ISO or SAE)", target));
      }
      const auto columns = static_cast<std::size_t>(is_inputs ? model().input_length() : model().output_length());
      if (values.ndim() != 2) {
        return shape_error(
            std::format("values must be a 2-D array with {} columns, got {} dimension(s)", columns, values.ndim()));
      }
      if (values.shape(1) != static_cast<py::ssize_t>(columns)) {
        return shape_error(std::format("values must have {} columns, got {}", columns, values.shape(1)));
      }
      const Eigen::Map<const Eigen::MatrixXd> map(values.data(), static_cast<Eigen::Index>(values.shape(0)),
                                                  static_cast<Eigen::Index>(values.shape(1)));
      Eigen::MatrixXd copy = map;
      if (is_inputs) {
        convert_inputs(model().layout(), *from, *to, copy);
      } else {
        convert_outputs(model().layout(), *from, *to, copy);
      }
      return to_array(std::move(copy));
    }

    /// Evaluates every row of `points` (rows x input_length(), SI units, columns in input_index order); the raw
    /// inputs go straight to the equations (no axis conversion).
    /// 1. INVALID_ARGUMENT record unless points.ndim() == 2 ("inputs must be a 2-D array with one row per point and
    ///    5 columns (FZ, SA, SR, IA, VX), got <d> dimension(s)"), then unless points.shape(1) == input_length()
    ///    ("inputs must have 5 columns (FZ, SA, SR, IA, VX), got <c>"). Both are checked whatever `check` is.
    /// 2. Eigen::Map<const Eigen::MatrixXd> over points.data() (no copy); GIL released for the evaluation:
    ///    `checked_evaluate_batch` with `check`, else `check_input_shape` and `evaluate_batch`.
    /// 3. A batch_error becomes to_record(cause, index); a result becomes a solution_record, with the load-side
    ///    warning (`load_side_warning` for `loaded_side()`) as "row <index>: <message>".
    [[nodiscard]] std::variant<solution_record, error_record> evaluate_batch(const input_array &points,
                                                                             bool check) const {
      return run_batch(
          points, check,
          [](const tire_model &tire, const Eigen::Map<const Eigen::MatrixXd> &map) {
            return checked_evaluate_batch(tire, map);
          },
          [](const tire_model &tire, const Eigen::Map<const Eigen::MatrixXd> &map) {
            return tire.evaluate_batch(map);
          });
    }

    /// Like `evaluate_batch`, but evaluates the characteristics (rows x characteristics_length(), channel order of
    /// `characteristics()`): the same shape checks, GIL release and load-side warning, with
    /// `checked_evaluate_characteristics_batch` for `check`, else `check_input_shape` and
    /// `evaluate_characteristics_batch`.
    [[nodiscard]] std::variant<solution_record, error_record> evaluate_characteristics_batch(const input_array &points,
                                                                                             bool check) const {
      return run_batch(
          points, check,
          [](const tire_model &tire, const Eigen::Map<const Eigen::MatrixXd> &map) {
            return checked_evaluate_characteristics_batch(tire, map);
          },
          [](const tire_model &tire, const Eigen::Map<const Eigen::MatrixXd> &map) {
            return tire.evaluate_characteristics_batch(map);
          });
    }

  private:
    /// The shared body of the two batch evaluations: shape checks (always), a Map over the points, the GIL released
    /// around `checked_fn(model, map)` with `check`, else `check_input_shape` and `unchecked_fn(model, map)`; a
    /// batch_error becomes to_record(cause, index), a result a solution_record with the load-side warning as
    /// "row <index>: <message>".
    template <typename Checked_T, typename Unchecked_T>
    [[nodiscard]] std::variant<solution_record, error_record> run_batch(const input_array &points, bool check,
                                                                        const Checked_T &checked_fn,
                                                                        const Unchecked_T &unchecked_fn) const {
      constexpr std::string_view dims_fmt = "inputs must be a 2-D array with one row per point and {} columns ({}), "
                                            "got {} dimension(s)";
      constexpr std::string_view columns_fmt = "inputs must have {} columns ({}), got {}";
      if (points.ndim() != 2) {
        return shape_error(std::format(dims_fmt, model().input_length(), channel_list(model()), points.ndim()));
      }
      if (points.shape(1) != static_cast<py::ssize_t>(model().input_length())) {
        return shape_error(std::format(columns_fmt, model().input_length(), channel_list(model()), points.shape(1)));
      }
      const Eigen::Map<const Eigen::MatrixXd> map(points.data(), static_cast<Eigen::Index>(points.shape(0)),
                                                  static_cast<Eigen::Index>(points.shape(1)));
      std::expected<Eigen::MatrixXd, batch_error> result = [&]() -> std::expected<Eigen::MatrixXd, batch_error> {
        const py::gil_scoped_release release;
        if (check) {
          return checked_fn(model(), map);
        }
        const auto shape = check_input_shape(model(), map);
        if (!shape.has_value()) {
          return std::unexpected(shape.error());
        }
        return unchecked_fn(model(), map);
      }();
      if (!result.has_value()) {
        return to_record(result.error().cause, result.error().index);
      }
      solution_record record;
      const std::optional<batch_warning> side =
          load_side_warning(census_fz(model().layout(), map), model().loaded_side());
      if (side.has_value()) {
        record.warnings.push_back(std::format("row {}: {}", side->index, to_string(side->cause)));
      }
      record.outputs = to_array(std::move(*result));
      return record;
    }

    io::loaded_model loaded_;
  };

  /// `io::load_tire_model_file(path, options)` with options = {mode = combined ? COMBINED : UNCOMBINED,
  /// throw_if_model_failure = false}.
  [[nodiscard]] std::variant<model_handle, error_record> load_tir(const std::filesystem::path &path, bool combined) {
    auto loaded = io::load_tire_model_file(path, options_for(combined));
    if (!loaded.has_value()) {
      return to_record(loaded.error());
    }
    return model_handle{std::move(*loaded)};
  }

  /// `io::tir_document::parse(text, file_name)`, then `io::load_tire_model(document, options)` (same options).
  [[nodiscard]] std::variant<model_handle, error_record> parse_tir(const std::string &text,
                                                                   const std::string &file_name, bool combined) {
    const auto document = io::tir_document::parse(text, file_name);
    if (!document.has_value()) {
      return to_record(document.error());
    }
    auto loaded = io::load_tire_model(*document, options_for(combined));
    if (!loaded.has_value()) {
      return to_record(loaded.error());
    }
    return model_handle{std::move(*loaded)};
  }
} // namespace mf::python

PYBIND11_MODULE(_core, m) {
  namespace py = pybind11;
  using mf::python::decision_record;
  using mf::python::error_record;
  using mf::python::identification_record;
  using mf::python::model_handle;
  using mf::python::reading_record;
  using mf::python::solution_record;

  m.doc() = "magic-formula C++ core (private; use the magic_formula package)";

  py::class_<error_record>(m, "Error", "A recoverable library error returned as a value (read-only attributes).")
      .def_readonly("code", &error_record::code, "error code name, e.g. 'INVALID_INPUT'")
      .def_readonly("message", &error_record::message, "message without location")
      .def_readonly("text", &error_record::text, "full text: '[row R: ][<file>:<line>: ]<message>'")
      .def_readonly("file", &error_record::file, "file name; '' when not applicable")
      .def_readonly("line", &error_record::line, "1-based line; 0 when not applicable")
      .def_readonly("key", &error_record::key, "key or channel name; '' when not applicable")
      .def_readonly("row", &error_record::row, "failing batch row; None for load and shape errors");

  py::class_<decision_record>(m, "Decision", "The weighted vote of one axis (read-only attributes).")
      .def_readonly("sign", &decision_record::sign, "POSITIVE (the ISO side), NEGATIVE or UNDETERMINED")
      .def_readonly("basis", &decision_record::basis, "DECIDED, INSUFFICIENT_EVIDENCE or CONFLICTING_EVIDENCE")
      .def_readonly("confidence", &decision_record::confidence, "share of the valid vote weight of the winning side")
      .def_readonly("positive_weight", &decision_record::positive_weight, "vote weight for the ISO side")
      .def_readonly("negative_weight", &decision_record::negative_weight, "vote weight for the SAE side");

  py::class_<reading_record>(m, "Reading", "One identification indicator at one probe load (read-only attributes).")
      .def_readonly("name", &reading_record::name, "indicator name")
      .def_readonly("meaning", &reading_record::meaning, "the physical expectation behind the vote")
      .def_readonly("unit", &reading_record::unit, "unit of value (SI)")
      .def_readonly("axis", &reading_record::axis, "VERTICAL, LATERAL or CHECK")
      .def_readonly("load_scale", &reading_record::load_scale, "probe load as a multiple of the reference FZ")
      .def_readonly("load", &reading_record::load, "probe FZ [N]")
      .def_readonly("value", &reading_record::value, "indicator value in unit")
      .def_readonly("normalised", &reading_record::normalised, "value / |load|")
      .def_readonly("status", &reading_record::status, "VOTE, BELOW_THRESHOLD, NOT_FINITE, CLAMPED or MISSING_CHANNEL")
      .def_readonly("vote", &reading_record::vote, "+1 (ISO side), -1 (SAE side) or 0 (abstains)")
      .def_readonly("weight", &reading_record::weight, "vote weight of this reading");

  py::class_<identification_record>(m, "Identification",
                                    "The coordinate system a coefficient set is expressed in (read-only attributes).")
      .def_readonly("classification", &identification_record::classification, "'ISO', 'SAE' or 'MIXED'")
      .def_readonly("vertical", &identification_record::vertical, "decision of the vertical axis (FZ sign)")
      .def_readonly("lateral", &identification_record::lateral, "decision of the lateral axis (dFy/dSA sign)")
      .def_readonly("effective_vertical", &identification_record::effective_vertical,
                    "vertical sign used for classification: POSITIVE (ISO) or NEGATIVE (SAE)")
      .def_readonly("effective_lateral", &identification_record::effective_lateral,
                    "lateral sign used for classification: POSITIVE (ISO) or NEGATIVE (SAE)")
      .def_readonly("readings", &identification_record::readings, "every indicator reading")
      .def_readonly("reference_input", &identification_record::reference_input,
                    "reference operating point (SI), input order")
      .def_readonly("warnings", &identification_record::warnings, "identification warnings");

  py::class_<solution_record>(m, "Solution",
                              "The result of Model.evaluate_batch or Model.evaluate_characteristics_batch "
                              "(read-only attributes).")
      .def_readonly("outputs", &solution_record::outputs, "(N, n_outputs) float64 array")
      .def_readonly("warnings", &solution_record::warnings, "'row <index>: <message>' per table warning");

  py::class_<model_handle>(m, "Model",
                           "A loaded, immutable tire model (SI units, angles in rad; no axis conversion during "
                           "evaluation).")
      .def_property_readonly("name", &model_handle::name, "model name: 'PAC2002' or 'MF 5.2'")
      .def_property_readonly("inputs", &model_handle::inputs, "(name, unit) of every input channel")
      .def_property_readonly("outputs", &model_handle::outputs, "(name, unit) of every output channel")
      .def_property_readonly("characteristics", &model_handle::characteristics,
                             "(name, unit) of every characteristic channel")
      .def_property_readonly("limits", &model_handle::limits, "(key, value) of every limit; open bounds are -inf/+inf")
      .def_property_readonly("fittyp", &model_handle::fittyp, "[MODEL] FITTYP or None")
      .def_property_readonly("use_mode", &model_handle::use_mode, "[MODEL] USE_MODE or None")
      .def_property_readonly("tyre_side", &model_handle::tyre_side, "'LEFT' or 'RIGHT'")
      .def_property_readonly("combined", &model_handle::combined, "True when combined slip is evaluated")
      .def_property_readonly("warnings", &model_handle::warnings, "load warnings, in load order")
      .def_property_readonly("ignored_keys", &model_handle::ignored_keys, "keys present in the file but unused")
      .def("find_coordinate_system", &model_handle::find_coordinate_system,
           "Identify the coordinate system of the coefficient set (ISO: z up, FZ > 0 loaded, dFy/dSA > 0; SAE: z down, "
           "FZ < 0 loaded, dFy/dSA < 0) from several evaluated indicators per axis. Returns an Identification.")
      .def("convert_coordinate_system", &model_handle::convert_coordinate_system, py::arg("target"),
           "Convert the model to 'ISO' or 'SAE' (any case) by flipping coefficient signs; the equations never change. "
           "Returns a Model, or an Error for an unknown system.")
      .def("convert", &model_handle::convert, py::arg("values"), py::arg("kind"), py::arg("source"), py::arg("target"),
           "Convert an (N, n) array of 'inputs' (FZ flips) or 'outputs' (FY, MZ and MY flip) between 'ISO' and 'SAE' "
           "(SI units). Returns an array or an Error.")
      .def("evaluate_batch", &model_handle::evaluate_batch, py::arg("points"), py::arg("check") = true,
           "Evaluate an (N, n_inputs) array (SI units, angles in rad) in the model's own coordinate system, without "
           "axis conversion. Returns a Solution (outputs and load-side warnings) or an Error for a wrong shape or, "
           "with check=True, an invalid or non-finite point.")
      .def("evaluate_characteristics_batch", &model_handle::evaluate_characteristics_batch, py::arg("points"),
           py::arg("check") = true,
           "Evaluate the characteristics of an (N, n_inputs) float64 F-ordered array of points (SI units, angles in "
           "rad). Returns a Solution whose outputs is the (N, n_characteristics) array (DFY_DSA ... SR_AT_PEAK_FX) or "
           "an Error. NaN is a documented value only in SA_AT_PEAK_FY and SR_AT_PEAK_FX (no stationary point, or no "
           "contact). FY_OVER_SA and FX_OVER_SR at zero slip are the tangent slopes DFY_DSA and DFX_DSR, and 0 for "
           "rows without contact.");

  m.def("version", [] { return std::string(mf::version()); }, "Library version string.");
  m.def("load_tir", &mf::python::load_tir, py::arg("path"), py::arg("combined") = true,
        "Load a .tir file (str or os.PathLike). Returns a Model or an Error.");
  m.def("parse_tir", &mf::python::parse_tir, py::arg("text"), py::arg("file_name") = "", py::arg("combined") = true,
        "Parse .tir text. Returns a Model or an Error.");
}

// pybind11 adapter that exposes the magic-formula library to Python as `magic_formula._core`.
// It converts types only: library errors are returned as values (`Error`), never thrown, and nothing is caught here.
// pybind11's dispatcher is the exception boundary (argument conversion failures, std::bad_alloc).

#include <Eigen/Core>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
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

  /// Inputs of `model_handle::evaluate_batch`: float64 in Fortran (column-major) order, the layout of Eigen::MatrixXd.
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

    /// Evaluates every row of `points` (rows x input_length(), SI units, columns in input_index order).
    /// 1. INVALID_ARGUMENT record unless points.ndim() == 2 ("inputs must be a 2-D array with one row per point and
    ///    5 columns (FZ, SA, SR, IA, VX), got <d> dimension(s)"), then unless points.shape(1) == input_length()
    ///    ("inputs must have 5 columns (FZ, SA, SR, IA, VX), got <c>"). Both are checked whatever `check` is.
    /// 2. Eigen::Map<const Eigen::MatrixXd> over points.data() (no copy); GIL released (py::gil_scoped_release) for:
    ///    check ? checked_evaluate_batch(model(), map) : model().evaluate_batch(map).
    /// 3. A batch_error becomes to_record(cause, index); a result becomes to_array(std::move(matrix)).
    /// Returns a rows x output_length() float64 array in output_index column order.
    [[nodiscard]] std::variant<py::array_t<double>, error_record> evaluate_batch(const input_array &points,
                                                                                 bool check) const {
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
          return checked_evaluate_batch(model(), map);
        }
        return model().evaluate_batch(map);
      }();
      if (!result.has_value()) {
        return to_record(result.error().cause, result.error().index);
      }
      return to_array(std::move(*result));
    }

  private:
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
  using mf::python::error_record;
  using mf::python::model_handle;

  m.doc() = "magic-formula C++ core (private; use the magic_formula package)";

  py::class_<error_record>(m, "Error", "A recoverable library error returned as a value (read-only attributes).")
      .def_readonly("code", &error_record::code, "error code name, e.g. 'INVALID_INPUT'")
      .def_readonly("message", &error_record::message, "message without location")
      .def_readonly("text", &error_record::text, "full text: '[row R: ][<file>:<line>: ]<message>'")
      .def_readonly("file", &error_record::file, "file name; '' when not applicable")
      .def_readonly("line", &error_record::line, "1-based line; 0 when not applicable")
      .def_readonly("key", &error_record::key, "key or channel name; '' when not applicable")
      .def_readonly("row", &error_record::row, "failing batch row; None for load and shape errors");

  py::class_<model_handle>(m, "Model", "A loaded, immutable tire model (SI units, angles in rad, ISO-W signs).")
      .def_property_readonly("name", &model_handle::name, "model name: 'PAC2002' or 'MF 5.2'")
      .def_property_readonly("inputs", &model_handle::inputs, "(name, unit) of every input channel")
      .def_property_readonly("outputs", &model_handle::outputs, "(name, unit) of every output channel")
      .def_property_readonly("limits", &model_handle::limits, "(key, value) of every limit; open bounds are -inf/+inf")
      .def_property_readonly("fittyp", &model_handle::fittyp, "[MODEL] FITTYP or None")
      .def_property_readonly("use_mode", &model_handle::use_mode, "[MODEL] USE_MODE or None")
      .def_property_readonly("tyre_side", &model_handle::tyre_side, "'LEFT' or 'RIGHT'")
      .def_property_readonly("combined", &model_handle::combined, "True when combined slip is evaluated")
      .def_property_readonly("warnings", &model_handle::warnings, "load warnings, in load order")
      .def_property_readonly("ignored_keys", &model_handle::ignored_keys, "keys present in the file but unused")
      .def("evaluate_batch", &model_handle::evaluate_batch, py::arg("inputs"), py::arg("check") = true,
           "Evaluate an (N, n_inputs) array (SI units, angles in rad) into an (N, n_outputs) float64 array; flag "
           "outputs are 0.0/1.0. Returns an Error for a wrong shape or, with check=True, an invalid or non-finite "
           "point.");

  m.def("version", [] { return std::string(mf::version()); }, "Library version string.");
  m.def("load_tir", &mf::python::load_tir, py::arg("path"), py::arg("combined") = true,
        "Load a .tir file (str or os.PathLike). Returns a Model or an Error.");
  m.def("parse_tir", &mf::python::parse_tir, py::arg("text"), py::arg("file_name") = "", py::arg("combined") = true,
        "Parse .tir text. Returns a Model or an Error.");
}

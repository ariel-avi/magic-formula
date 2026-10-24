/// Embind adapter of the browser frontend: exposes the magic-formula library to JavaScript. The adapter holds no tire
/// equations; it only converts between JavaScript values and the library API.
#include <Eigen/Core>
#include <cstddef>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <expected>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/io/model_loader.hpp>
#include <magic_formula/io/tir_document.hpp>
#include <magic_formula/io/tir_editor.hpp>
#include <magic_formula/model_checks.hpp>
#include <magic_formula/version.hpp>
#include <string>
#include <vector>

namespace mf::wasm {
  using emscripten::val;

  namespace {
    [[nodiscard]] val to_js(const error &value) {
      val result = val::object();
      result.set("code", std::string{error_code_name(value.code)});
      result.set("message", value.message);
      result.set("line", static_cast<double>(value.where.line));
      result.set("key", value.where.key);
      return result;
    }

    [[nodiscard]] val to_js(const warning &value) {
      val result = val::object();
      result.set("message", value.message);
      result.set("line", static_cast<double>(value.where.line));
      result.set("key", value.where.key);
      return result;
    }

    [[nodiscard]] val failure(const error &value) {
      val result = val::object();
      result.set("ok", false);
      result.set("error", to_js(value));
      return result;
    }

    [[nodiscard]] val evaluation_failure(const error &value, val row) {
      val result = failure(value);
      result.set("row", row);
      return result;
    }

    [[nodiscard]] val to_js(const io::tir_entry &entry) {
      val result = val::object();
      result.set("section", entry.section);
      result.set("key", entry.key);
      result.set("value", entry.value);
      result.set("quoted", entry.quoted);
      result.set("line", static_cast<double>(entry.line));
      return result;
    }

    [[nodiscard]] val to_js(const tire_model &model) {
      val result = val::object();
      result.set("name", std::string{model.name()});
      const model_metadata &metadata = model.metadata();
      result.set("fittyp", metadata.fittyp ? val(*metadata.fittyp) : val::null());
      result.set("useMode", metadata.use_mode ? val(*metadata.use_mode) : val::null());
      result.set("tyreSide", std::string{metadata.side == tyre_side::LEFT ? "LEFT" : "RIGHT"});

      val inputs = val::array();
      for (const input_channel &channel : model.layout().inputs) {
        val item = val::object();
        item.set("name", std::string{channel.name});
        item.set("unit", std::string{channel.unit});
        item.set("minimum", channel.minimum);
        item.set("minimumInclusive", channel.minimum_inclusive);
        inputs.call<void>("push", item);
      }
      result.set("inputs", inputs);

      val outputs = val::array();
      for (const output_channel &channel : model.layout().outputs) {
        val item = val::object();
        item.set("name", std::string{channel.name});
        item.set("unit", std::string{channel.unit});
        outputs.call<void>("push", item);
      }
      result.set("outputs", outputs);

      val limits = val::array();
      const auto channels = model.layout().limits;
      for (std::size_t i = 0; i < channels.size(); ++i) {
        val item = val::object();
        item.set("key", std::string{channels[i].key});
        std::string role = "VALUE";
        if (channels[i].role == limit_role::LOWER) {
          role = "LOWER";
        } else if (channels[i].role == limit_role::UPPER) {
          role = "UPPER";
        }
        item.set("role", role);
        item.set("value", model.limits()[static_cast<Eigen::Index>(i)]);
        limits.call<void>("push", item);
      }
      result.set("limits", limits);
      return result;
    }

    [[nodiscard]] val to_float64_array(const double *data, std::size_t size) {
      val view{emscripten::typed_memory_view(size, data)};
      return val::global("Float64Array").call<val>("from", view);
    }
  } // namespace

  /// `mf::version()` ("MAJOR.MINOR.PATCH").
  [[nodiscard]] std::string library_version() {
    return std::string{mf::version()};
  }

  /// Parses `text` and tries to build the model. `{ok:false,error}` for a syntax error; otherwise the entries,
  /// warnings, ignored keys and either `model` or `loadError`.
  [[nodiscard]] val inspect(const std::string &text, const std::string &file_name) {
    const auto document = io::tir_document::parse(text, file_name);
    if (!document) {
      return failure(document.error());
    }
    val result = val::object();
    result.set("ok", true);

    val entries = val::array();
    for (const io::tir_entry &entry : document->entries()) {
      entries.call<void>("push", to_js(entry));
    }
    result.set("entries", entries);

    val warnings = val::array();
    const auto loaded = io::load_tire_model(*document);
    val ignored = val::array();
    if (loaded) {
      for (const warning &item : loaded->warnings) {
        warnings.call<void>("push", to_js(item));
      }
      for (const std::string &key : loaded->ignored_keys) {
        ignored.call<void>("push", key);
      }
      result.set("model", to_js(*loaded->model));
      result.set("loadError", val::null());
    } else {
      // loaded->warnings already starts with the parse warnings; on failure the document's are all there is.
      for (const warning &item : document->warnings()) {
        warnings.call<void>("push", to_js(item));
      }
      result.set("model", val::null());
      result.set("loadError", to_js(loaded.error()));
    }
    result.set("warnings", warnings);
    result.set("ignoredKeys", ignored);
    return result;
  }

  /// `io::replace_value`: `{ok:true,text}` or `{ok:false,error}`.
  [[nodiscard]] val replace_value(const std::string &text, const std::string &section, const std::string &key,
                                  const std::string &value) {
    const auto replaced = io::replace_value(text, section, key, value);
    if (!replaced) {
      return failure(replaced.error());
    }
    val result = val::object();
    result.set("ok", true);
    result.set("text", *replaced);
    return result;
  }

  /// Evaluates a batch. `inputs` is a Float64Array, column-major rows x input_length (SI units).
  /// `{ok:true,rows,outputs}` (Float64Array, column-major rows x output_length) or `{ok:false,error,row}`.
  [[nodiscard]] val evaluate(const std::string &text, const val &inputs, bool uncombined) {
    const auto document = io::tir_document::parse(text);
    if (!document) {
      return evaluation_failure(document.error(), val::null());
    }
    model_options options;
    options.mode = uncombined ? slip_mode::UNCOMBINED : slip_mode::COMBINED;
    const auto loaded = io::load_tire_model(*document, options);
    if (!loaded) {
      return evaluation_failure(loaded.error(), val::null());
    }
    const tire_model &model = *loaded->model;

    const std::vector<double> data = emscripten::convertJSArrayToNumberVector<double>(inputs);
    const auto width = static_cast<std::size_t>(model.input_length());
    if (width == 0 || data.size() % width != 0) {
      return evaluation_failure(error{error_code::INVALID_ARGUMENT,
                                      "input length " + std::to_string(data.size()) + " is not a multiple of " +
                                          std::to_string(width) + " inputs",
                                      {}},
                                val::null());
    }
    const auto rows = static_cast<Eigen::Index>(data.size() / width);
    const Eigen::Map<const Eigen::MatrixXd> matrix{data.data(), rows, static_cast<Eigen::Index>(width)};
    const auto outputs = checked_evaluate_batch(model, matrix);
    if (!outputs) {
      return evaluation_failure(outputs.error().cause, val(static_cast<double>(outputs.error().index)));
    }
    val result = val::object();
    result.set("ok", true);
    result.set("rows", static_cast<double>(rows));
    result.set("outputs", to_float64_array(outputs->data(), static_cast<std::size_t>(outputs->size())));
    return result;
  }
} // namespace mf::wasm

EMSCRIPTEN_BINDINGS(mf_wasm) {
  emscripten::function("version", &mf::wasm::library_version);
  emscripten::function("inspect", &mf::wasm::inspect);
  emscripten::function("replaceValue", &mf::wasm::replace_value);
  emscripten::function("evaluate", &mf::wasm::evaluate);
}

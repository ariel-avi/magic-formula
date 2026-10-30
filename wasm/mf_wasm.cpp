/// Embind adapter of the browser frontend: exposes the magic-formula library to JavaScript. The adapter holds no tire
/// equations; it only converts between JavaScript values and the library API.
#include <Eigen/Core>
#include <cstddef>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <expected>
#include <magic_formula/coordinate_identification.hpp>
#include <magic_formula/coordinate_system.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/io/model_loader.hpp>
#include <magic_formula/io/tir_conversion.hpp>
#include <magic_formula/io/tir_document.hpp>
#include <magic_formula/io/tir_editor.hpp>
#include <magic_formula/model_checks.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/sign_transform.hpp>
#include <magic_formula/version.hpp>
#include <string>
#include <utility>
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
        item.set("negatedBy", std::string{sign_transform_name(channel.negated_by)});
        inputs.call<void>("push", item);
      }
      result.set("inputs", inputs);

      val outputs = val::array();
      for (const output_channel &channel : model.layout().outputs) {
        val item = val::object();
        item.set("name", std::string{channel.name});
        item.set("unit", std::string{channel.unit});
        item.set("negatedBy", std::string{sign_transform_name(channel.negated_by)});
        outputs.call<void>("push", item);
      }
      result.set("outputs", outputs);

      val characteristics = val::array();
      for (const characteristic_channel &channel : model.layout().characteristics) {
        val item = val::object();
        item.set("name", std::string{channel.name});
        item.set("unit", std::string{channel.unit});
        characteristics.call<void>("push", item);
      }
      result.set("characteristics", characteristics);

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

    [[nodiscard]] error unknown_system(const std::string &name) {
      std::string message = "unknown coordinate system '" + name + "' (expected ISO or SAE)";
      return error{error_code::INVALID_ARGUMENT, std::move(message), {}};
    }

    [[nodiscard]] val to_js(const axis_decision &value) {
      val result = val::object();
      result.set("sign", std::string{axis_sign_name(value.sign)});
      result.set("basis", std::string{decision_basis_name(value.basis)});
      result.set("confidence", value.confidence);
      result.set("positiveWeight", value.positive_weight);
      result.set("negativeWeight", value.negative_weight);
      return result;
    }

    [[nodiscard]] val to_js(const indicator_reading &value) {
      val result = val::object();
      result.set("name", std::string{value.definition.name});
      result.set("meaning", std::string{value.definition.meaning});
      result.set("unit", std::string{value.definition.unit});
      result.set("axis", std::string{identification_axis_name(value.definition.axis)});
      result.set("loadScale", value.load_scale);
      result.set("load", value.load_n);
      result.set("value", value.value);
      result.set("normalised", value.normalised);
      result.set("status", std::string{reading_status_name(value.status)});
      result.set("vote", value.vote);
      result.set("weight", value.weight);
      return result;
    }

    [[nodiscard]] val to_js(const coordinate_identification &value) {
      val result = val::object();
      result.set("classification", std::string{system_classification_name(value.classification)});
      result.set("vertical", to_js(value.vertical));
      result.set("lateral", to_js(value.lateral));
      val effective = val::object();
      effective.set("vertical", std::string{axis_sign_name(value.effective.vertical)});
      effective.set("lateral", std::string{axis_sign_name(value.effective.lateral)});
      result.set("effective", effective);
      val readings = val::array();
      for (const indicator_reading &item : value.readings) {
        readings.call<void>("push", to_js(item));
      }
      result.set("readings", readings);
      val reference = val::array();
      for (const double item : value.reference_input) {
        reference.call<void>("push", item);
      }
      result.set("referenceInput", reference);
      val warnings = val::array();
      for (const warning &item : value.warnings) {
        warnings.call<void>("push", to_js(item));
      }
      result.set("warnings", warnings);
      return result;
    }

    [[nodiscard]] val to_js(const sign_transforms &value) {
      val result = val::object();
      result.set("vertical", value.vertical);
      result.set("lateral", value.lateral);
      return result;
    }

    [[nodiscard]] val to_js(const io::tir_change &value) {
      val result = val::object();
      result.set("section", value.section);
      result.set("key", value.key);
      result.set("line", static_cast<double>(value.line));
      result.set("before", value.before);
      result.set("after", value.after);
      return result;
    }

    [[nodiscard]] val to_float64_array(const double *data, std::size_t size) {
      val view{emscripten::typed_memory_view(size, data)};
      return val::global("Float64Array").call<val>("from", view);
    }

    /// The loaded model and the raw inputs of one evaluate call (column-major, rows x input_length()).
    struct batch_job {
      io::loaded_model loaded;
      std::vector<double> inputs;
      Eigen::Index rows = 0;
    };

    /// Parses `text`, loads the model in the slip mode of `uncombined` and checks that the inputs hold whole rows.
    /// Returns the job, or the {ok:false,error,row:null} object to return (parse, load, or INVALID_ARGUMENT shape
    /// error).
    [[nodiscard]] std::expected<batch_job, val> prepare_batch(const std::string &text, const val &inputs,
                                                              bool uncombined, bool turn_slip) {
      const auto document = io::tir_document::parse(text);
      if (!document) {
        return std::unexpected(evaluation_failure(document.error(), val::null()));
      }
      model_options options;
      options.mode = uncombined ? slip_mode::UNCOMBINED : slip_mode::COMBINED;
      options.turn_slip = turn_slip;
      auto loaded = io::load_tire_model(*document, options);
      if (!loaded) {
        return std::unexpected(evaluation_failure(loaded.error(), val::null()));
      }
      std::vector<double> data = emscripten::convertJSArrayToNumberVector<double>(inputs);
      const auto width = static_cast<std::size_t>(loaded->model->input_length());
      if (width == 0 || data.size() % width != 0) {
        const std::string message = "input length " + std::to_string(data.size()) + " is not a multiple of " +
                                    std::to_string(width) + " inputs";
        return std::unexpected(evaluation_failure(error{error_code::INVALID_ARGUMENT, message, {}}, val::null()));
      }
      const auto rows = static_cast<Eigen::Index>(data.size() / width);
      return batch_job{.loaded = std::move(*loaded), .inputs = std::move(data), .rows = rows};
    }

    /// {ok:true,rows,outputs,warnings}: outputs as a Float64Array (column-major, rows x output_length()),
    /// warnings = at most the load-side warning (census_fz + load_side_warning).
    [[nodiscard]] val solved_batch(const tire_model &model, const Eigen::Ref<const Eigen::MatrixXd> &inputs,
                                   const Eigen::MatrixXd &outputs) {
      val warnings = val::array();
      const auto side_warning = load_side_warning(census_fz(model.layout(), inputs), model.loaded_side());
      if (side_warning) {
        val entry = val::object();
        entry.set("message", side_warning->cause.message);
        entry.set("key", side_warning->cause.where.key);
        entry.set("row", static_cast<double>(side_warning->index));
        warnings.call<void>("push", entry);
      }
      for (const batch_warning &item : collect_input_warnings(model, inputs)) {
        val entry = val::object();
        entry.set("message", item.cause.message);
        entry.set("key", item.cause.where.key);
        entry.set("row", static_cast<double>(item.index));
        warnings.call<void>("push", entry);
      }
      val result = val::object();
      result.set("ok", true);
      result.set("rows", static_cast<double>(inputs.rows()));
      result.set("outputs", to_float64_array(outputs.data(), static_cast<std::size_t>(outputs.size())));
      result.set("warnings", warnings);
      return result;
    }
  } // namespace

  /// `mf::version()` ("MAJOR.MINOR.PATCH").
  [[nodiscard]] std::string library_version() {
    return std::string{mf::version()};
  }

  /// Parses `text` and tries to build the model. `{ok:false,error}` for a syntax error; otherwise the entries,
  /// warnings, ignored keys and either `model` (with `coordinates`, the coordinate-system identification:
  /// classification, vertical and lateral decisions, effective signs, readings, reference input, warnings) or
  /// `loadError` (with `model` and `coordinates` null).
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
      result.set("coordinates", to_js(loaded->model->find_coordinate_system()));
      result.set("loadError", val::null());
    } else {
      // loaded->warnings already starts with the parse warnings; on failure the document's are all there is.
      for (const warning &item : document->warnings()) {
        warnings.call<void>("push", to_js(item));
      }
      result.set("model", val::null());
      result.set("coordinates", val::null());
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

  /// Evaluates a batch with raw inputs (no conversion). `inputs` is a Float64Array, column-major rows x input_length
  /// (SI units). `{ok:true,rows,outputs,warnings}` (outputs: Float64Array, column-major rows x output_length;
  /// warnings: `[{message,key,row}]`, at most the load-side warning for FZ on the unloaded side) or
  /// `{ok:false,error,row}`.
  [[nodiscard]] val evaluate_turn_slip(const std::string &text, const val &inputs, bool uncombined, bool turn_slip) {
    const auto job = prepare_batch(text, inputs, uncombined, turn_slip);
    if (!job) {
      return job.error();
    }
    const tire_model &model = *job->loaded.model;
    const Eigen::Map<const Eigen::MatrixXd> matrix{job->inputs.data(), job->rows, model.input_length()};
    const auto solved = checked_evaluate_batch(model, matrix);
    if (!solved) {
      return evaluation_failure(solved.error().cause, val(static_cast<double>(solved.error().index)));
    }
    return solved_batch(model, matrix, *solved);
  }

  /// evaluate plus the characteristics of every row (checked_evaluate_batch_with_characteristics: outputs checked
  /// first). `{ok:true,rows,outputs,characteristics,warnings}`: characteristics is a Float64Array, column-major
  /// rows x characteristics_length(), with NaN only in SA_AT_PEAK_FY and SR_AT_PEAK_FX; or `{ok:false,error,row}`.
  [[nodiscard]] val evaluate_with_characteristics_turn_slip(const std::string &text, const val &inputs, bool uncombined,
                                                            bool turn_slip) {
    const auto job = prepare_batch(text, inputs, uncombined, turn_slip);
    if (!job) {
      return job.error();
    }
    const tire_model &model = *job->loaded.model;
    const Eigen::Map<const Eigen::MatrixXd> matrix{job->inputs.data(), job->rows, model.input_length()};
    const auto solved = checked_evaluate_batch_with_characteristics(model, matrix);
    if (!solved) {
      return evaluation_failure(solved.error().cause, val(static_cast<double>(solved.error().index)));
    }
    val result = solved_batch(model, matrix, solved->outputs);
    result.set("characteristics", to_float64_array(solved->characteristics.data(),
                                                   static_cast<std::size_t>(solved->characteristics.size())));
    return result;
  }

  /// Backward-compatible evaluation without turn slip.
  [[nodiscard]] val evaluate(const std::string &text, const val &inputs, bool uncombined) {
    return evaluate_turn_slip(text, inputs, uncombined, false);
  }

  /// Backward-compatible characteristics without turn slip.
  [[nodiscard]] val evaluate_with_characteristics(const std::string &text, const val &inputs, bool uncombined) {
    return evaluate_with_characteristics_turn_slip(text, inputs, uncombined, false);
  }

  /// `io::convert_tir`: converts the model text to `target` ("ISO" or "SAE") by coefficient sign flips.
  /// `{ok:true,text,applied:{vertical,lateral},changes:[{section,key,line,before,after}]}` or `{ok:false,error}`
  /// (INVALID_ARGUMENT for an unknown target).
  [[nodiscard]] val convert_model(const std::string &text, const std::string &file_name, const std::string &target) {
    const auto system = parse_coordinate_system(target);
    if (!system) {
      return failure(unknown_system(target));
    }
    const auto converted = io::convert_tir(text, *system, file_name);
    if (!converted) {
      return failure(converted.error());
    }
    val result = val::object();
    result.set("ok", true);
    result.set("text", converted->text);
    result.set("applied", to_js(converted->applied));
    val changes = val::array();
    for (const io::tir_change &item : converted->changes) {
      changes.call<void>("push", to_js(item));
    }
    result.set("changes", changes);
    return result;
  }
} // namespace mf::wasm

EMSCRIPTEN_BINDINGS(mf_wasm) {
  emscripten::function("version", &mf::wasm::library_version);
  emscripten::function("inspect", &mf::wasm::inspect);
  emscripten::function("replaceValue", &mf::wasm::replace_value);
  emscripten::function("evaluateTurnSlip", &mf::wasm::evaluate_turn_slip);
  emscripten::function("evaluateWithCharacteristicsTurnSlip", &mf::wasm::evaluate_with_characteristics_turn_slip);
  emscripten::function("evaluate", &mf::wasm::evaluate);
  emscripten::function("evaluateWithCharacteristics", &mf::wasm::evaluate_with_characteristics);
  emscripten::function("convertModel", &mf::wasm::convert_model);
}

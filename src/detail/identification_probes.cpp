#include <Eigen/Core>
#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <magic_formula/coordinate_identification.hpp>
#include <magic_formula/detail/identification_probes.hpp>
#include <magic_formula/model_layout.hpp>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace mf::detail {
  namespace {
    constexpr std::string_view CLAMPED_SUFFIX = "_CLAMPED";
    constexpr std::array<double, 1> UNIT_SCALE{1.0};

    /// Positions of the channels one definition needs; -1 where unused.
    struct resolved_channels {
      Eigen::Index fz = -1;
      Eigen::Index input = -1;
      Eigen::Index output = -1;
      bool missing = false;
    };

    /// The rows collected so far and the slots.
    struct plan_state {
      std::vector<Eigen::VectorXd> rows;
      std::vector<probe_slot> slots;
    };

    bool uses_channel(indicator_form form) noexcept {
      return form != indicator_form::OUTPUT_LEVEL;
    }

    bool uses_response(indicator_form form) noexcept {
      return form != indicator_form::INPUT_LEVEL;
    }

    /// Finds FZ, then the channel, then the response; the first absent one marks the result missing.
    resolved_channels resolve_channels(const model_layout &layout, const indicator_definition &definition) noexcept {
      resolved_channels channels;
      const std::optional<std::size_t> fz = find_channel(layout.inputs, "FZ");
      if (!fz.has_value()) {
        channels.missing = true;
        return channels;
      }
      channels.fz = static_cast<Eigen::Index>(*fz);
      if (uses_channel(definition.form)) {
        const std::optional<std::size_t> input = find_channel(layout.inputs, definition.channel);
        if (!input.has_value()) {
          channels.missing = true;
          return channels;
        }
        channels.input = static_cast<Eigen::Index>(*input);
      }
      if (uses_response(definition.form)) {
        const std::optional<std::size_t> output = find_channel(layout.outputs, definition.response);
        if (!output.has_value()) {
          channels.missing = true;
          return channels;
        }
        channels.output = static_cast<Eigen::Index>(*output);
      }
      return channels;
    }

    /// `point` with `offset` added to entry `column`.
    Eigen::VectorXd shifted(const Eigen::VectorXd &point, Eigen::Index column, double offset) {
      Eigen::VectorXd row = point;
      row(column) += offset;
      // Moved into a new vector: a returned (elided) local leaves its destructor call off the closing brace, whose
      // only code is then the exception-cleanup resume that no test can execute.
      // NOLINTNEXTLINE(modernize-return-braced-init-list): braces would pick Eigen's list constructors
      return Eigen::VectorXd(std::move(row));
    }

    /// Appends a row; returns its index.
    Eigen::Index append_row(plan_state &state, Eigen::VectorXd row) {
      state.rows.push_back(std::move(row));
      return static_cast<Eigen::Index>(state.rows.size()) - 1;
    }

    /// Adds the rows of one slot (the channels are all present) and records their positions in `slot`.
    void add_rows(const indicator_definition &definition, const resolved_channels &channels,
                  const Eigen::VectorXd &point, plan_state &state, probe_slot &slot) {
      if (definition.form == indicator_form::INPUT_LEVEL) {
        slot.input_value = point(channels.input);
      } else if (definition.form == indicator_form::OUTPUT_LEVEL) {
        slot.plus_row = append_row(state, point);
      } else {
        slot.plus_row = append_row(state, shifted(point, channels.input, definition.step));
        slot.minus_row = append_row(state, shifted(point, channels.input, -definition.step));
      }
    }

    /// The slot of one (definition, load), with its rows.
    void plan_slot(const Eigen::Ref<const Eigen::VectorXd> &reference, std::size_t index,
                   const indicator_definition &definition, const resolved_channels &channels, double scale,
                   plan_state &state) {
      const double load_n = channels.fz >= 0 ? scale * reference(channels.fz) : 0.0;
      probe_slot slot{.definition = index,
                      .load_scale = scale,
                      .load_n = load_n,
                      .output = channels.output,
                      .missing_channel = channels.missing};
      if (!channels.missing) {
        Eigen::VectorXd point = reference;
        point(channels.fz) = load_n;
        add_rows(definition, channels, point, state, slot);
      }
      state.slots.push_back(slot);
    }

    /// The probe matrix of the collected rows.
    Eigen::MatrixXd rows_to_matrix(const std::vector<Eigen::VectorXd> &rows, Eigen::Index columns) {
      Eigen::MatrixXd matrix(static_cast<Eigen::Index>(rows.size()), columns);
      for (std::size_t i = 0; i < rows.size(); ++i) {
        matrix.row(static_cast<Eigen::Index>(i)) = rows[i].transpose();
      }
      // Moved into a new matrix: see `shifted`.
      // NOLINTNEXTLINE(modernize-return-braced-init-list): braces would pick Eigen's list constructors
      return Eigen::MatrixXd(std::move(matrix));
    }

    /// The output columns whose name ends with "_CLAMPED".
    std::vector<Eigen::Index> clamp_columns_of(const model_layout &layout) {
      std::vector<Eigen::Index> columns;
      for (std::size_t i = 0; i < layout.outputs.size(); ++i) {
        if (layout.outputs[i].name.ends_with(CLAMPED_SUFFIX)) {
          columns.push_back(static_cast<Eigen::Index>(i));
        }
      }
      // Moved into a new vector: see `shifted`.
      return std::vector<Eigen::Index>{std::move(columns)};
    }

    /// True when a clamp column is non-zero on `row`.
    bool row_clamped(const Eigen::Ref<const Eigen::MatrixXd> &outputs, const std::vector<Eigen::Index> &clamp_columns,
                     Eigen::Index row) {
      return std::ranges::any_of(clamp_columns, [&](Eigen::Index column) { return outputs(row, column) != 0.0; });
    }

    /// A value and whether any of its rows was clamped.
    struct slot_value {
      double value = std::numeric_limits<double>::quiet_NaN();
      bool clamped = false;
    };

    /// The value of a slot with rows, by form.
    slot_value read_value(const indicator_definition &definition, const probe_slot &slot,
                          const Eigen::Ref<const Eigen::MatrixXd> &outputs,
                          const std::vector<Eigen::Index> &clamp_columns) {
      if (definition.form == indicator_form::INPUT_LEVEL) {
        return slot_value{.value = slot.input_value, .clamped = false};
      }
      if (definition.form == indicator_form::OUTPUT_LEVEL) {
        return slot_value{.value = outputs(slot.plus_row, slot.output),
                          .clamped = row_clamped(outputs, clamp_columns, slot.plus_row)};
      }
      const double plus = outputs(slot.plus_row, slot.output);
      const double minus = outputs(slot.minus_row, slot.output);
      return slot_value{.value = (plus - minus) / (2.0 * definition.step),
                        .clamped = row_clamped(outputs, clamp_columns, slot.plus_row) ||
                                   row_clamped(outputs, clamp_columns, slot.minus_row)};
    }

    /// The reading of one slot.
    indicator_reading read_slot(const indicator_definition &definition, const probe_slot &slot,
                                const Eigen::Ref<const Eigen::MatrixXd> &outputs,
                                const std::vector<Eigen::Index> &clamp_columns) {
      if (slot.missing_channel) {
        return make_reading(definition, slot.load_scale, slot.load_n, std::numeric_limits<double>::quiet_NaN(), false,
                            true);
      }
      const slot_value value = read_value(definition, slot, outputs, clamp_columns);
      return make_reading(definition, slot.load_scale, slot.load_n, value.value, value.clamped, false);
    }
  } // namespace

  identification_probes plan_probes(const model_layout &layout, const Eigen::Ref<const Eigen::VectorXd> &reference,
                                    std::span<const indicator_definition> indicators) {
    plan_state state;
    for (std::size_t i = 0; i < indicators.size(); ++i) {
      const indicator_definition &definition = indicators[i];
      const resolved_channels channels = resolve_channels(layout, definition);
      const std::span<const double> sweep{IDENTIFICATION_LOAD_SCALES};
      const std::span<const double> single{UNIT_SCALE};
      const std::span<const double> scales = definition.load_sweep ? sweep : single;
      for (const double scale : scales) {
        plan_slot(reference, i, definition, channels, scale, state);
      }
    }
    Eigen::MatrixXd inputs = rows_to_matrix(state.rows, static_cast<Eigen::Index>(layout.inputs.size()));
    return identification_probes{.inputs = std::move(inputs), .slots = std::move(state.slots)};
  }

  std::vector<indicator_reading> read_probes(const model_layout &layout,
                                             std::span<const indicator_definition> indicators,
                                             const identification_probes &probes,
                                             const Eigen::Ref<const Eigen::MatrixXd> &outputs) {
    const std::vector<Eigen::Index> clamp_columns = clamp_columns_of(layout);
    std::vector<indicator_reading> readings;
    readings.reserve(probes.slots.size());
    for (const probe_slot &slot : probes.slots) {
      readings.push_back(read_slot(indicators[slot.definition], slot, outputs, clamp_columns));
    }
    // Moved into a new vector: see `shifted`.
    return std::vector<indicator_reading>{std::move(readings)};
  }
} // namespace mf::detail

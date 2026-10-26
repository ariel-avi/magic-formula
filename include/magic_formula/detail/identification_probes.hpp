#pragma once

#include <Eigen/Core>
#include <cstddef>
#include <limits>
#include <magic_formula/coordinate_identification.hpp>
#include <magic_formula/model_layout.hpp>
#include <span>
#include <vector>

namespace mf::detail {
  /// Where one reading comes from.
  struct probe_slot {
    /// Index into the indicator table.
    std::size_t definition = 0;
    double load_scale = 1.0;
    /// Probe FZ [N].
    double load_n = 0.0;
    /// INPUT_LEVEL: the channel value at the probe.
    double input_value = std::numeric_limits<double>::quiet_NaN();
    /// CENTRAL_DIFFERENCE + row, or the OUTPUT_LEVEL row; -1: none.
    Eigen::Index plus_row = -1;
    /// CENTRAL_DIFFERENCE - row; -1: none.
    Eigen::Index minus_row = -1;
    /// Column of `response`; -1: unused or missing.
    Eigen::Index output = -1;
    /// FZ, channel or response absent: no rows.
    bool missing_channel = false;
  };

  /// The probe matrix and its slots.
  struct identification_probes {
    Eigen::MatrixXd inputs;
    std::vector<probe_slot> slots;
  };

  /// One slot per (definition, load), in table order then IDENTIFICATION_LOAD_SCALES order (one slot at scale 1 when
  /// !load_sweep). Probe point = reference with FZ := scale*reference(FZ); CENTRAL_DIFFERENCE adds two rows with
  /// channel +-step; OUTPUT_LEVEL one row; INPUT_LEVEL none. Channels are found by name (`find_channel`): FZ, then
  /// `channel` in the inputs (INPUT_LEVEL and CENTRAL_DIFFERENCE), then `response` in the outputs (OUTPUT_LEVEL and
  /// CENTRAL_DIFFERENCE); a missing one marks the slot and creates no rows. Probe loads are never moved into
  /// FZMIN/FZMAX.
  [[nodiscard]] identification_probes plan_probes(const model_layout &layout,
                                                  const Eigen::Ref<const Eigen::VectorXd> &reference,
                                                  std::span<const indicator_definition> indicators);

  /// One reading per slot (`make_reading`). clamped = any output named "*_CLAMPED" is non-zero on the slot's rows
  /// (BELOW_VXLOW is not a clamp). Preconditions: `indicators` is the span that was passed to plan_probes for
  /// `probes`; outputs.rows() == probes.inputs.rows(); outputs.cols() == layout.outputs.size().
  [[nodiscard]] std::vector<indicator_reading> read_probes(const model_layout &layout,
                                                           std::span<const indicator_definition> indicators,
                                                           const identification_probes &probes,
                                                           const Eigen::Ref<const Eigen::MatrixXd> &outputs);
} // namespace mf::detail

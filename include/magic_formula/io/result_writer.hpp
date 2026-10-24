#pragma once

#include <Eigen/Core>
#include <magic_formula/model_layout.hpp>
#include <ostream>
#include <string>

namespace mf::io {
  /// "NAME[unit]" of every input channel, then of every output channel, comma-joined, without a trailing newline. For
  /// the MF 5.x models: FZ[N],SA[rad],SR[-],IA[rad],VX[m/s],FX[N],FY[N],MZ[N*m],MX[N*m],MY[N*m],FZ_CLAMPED[-],
  /// SR_CLAMPED[-],SA_CLAMPED[-],IA_CLAMPED[-],BELOW_VXLOW[-].
  [[nodiscard]] std::string results_header(const model_layout &layout);

  /// Writes the `results_header` line, then one CSV line per row r < min(inputs.rows(), outputs.rows()): every input
  /// value, then every output value, each formatted with `std::format("{}")` (shortest text that parses back to the
  /// same double; -0.0 prints as "-0"; the 0.0/1.0 flags print as "0" and "1"), comma-separated. Lines end with '\n'.
  ///
  /// Precondition: `inputs` has one column per input channel and `outputs` one column per output channel of `layout`.
  void write_results(std::ostream &out, const model_layout &layout, const Eigen::Ref<const Eigen::MatrixXd> &inputs,
                     const Eigen::Ref<const Eigen::MatrixXd> &outputs);
} // namespace mf::io

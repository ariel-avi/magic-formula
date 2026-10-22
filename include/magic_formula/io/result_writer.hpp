#pragma once

#include <magic_formula/evaluation_types.hpp>
#include <ostream>
#include <span>
#include <string_view>

namespace mf::io {
  /// CSV header of the CLI output: the inputs in SI, then the five outputs.
  inline constexpr std::string_view RESULTS_HEADER =
      "FZ[N],SA[rad],SR[-],IA[rad],VX[m/s],FX[N],FY[N],MZ[N*m],MX[N*m],MY[N*m]";

  /// Writes `RESULTS_HEADER` and then one CSV line per (input, output) pair, in order. Every line has the ten fields
  /// FZ, SA, SR, IA, VX, FX, FY, MZ, MX, MY, each formatted with `std::format("{}")` (shortest text that parses back
  /// to the same double; -0.0 prints as "-0"). Lines end with '\n'.
  ///
  /// Precondition: `inputs.size() == outputs.size()` (a longer span is ignored beyond the shorter one).
  void write_results(std::ostream &out, std::span<const operating_point> inputs, std::span<const tire_outputs> outputs);
} // namespace mf::io

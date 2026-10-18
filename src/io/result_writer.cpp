#include <format>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/io/result_writer.hpp>
#include <ostream>
#include <ranges>
#include <span>

namespace mf::io {
  void write_results(std::ostream &out, std::span<const operating_point> inputs,
                     std::span<const tire_outputs> outputs) {
    out << RESULTS_HEADER << '\n';
    for (const auto [input, result] : std::views::zip(inputs, outputs)) {
      out << std::format("{},{},{},{},{},{},{},{},{},{}\n", input.fz_n, input.alpha_rad, input.kappa, input.gamma_rad,
                         input.vx_mps, result.fx_n, result.fy_n, result.mz_nm, result.mx_nm, result.my_nm);
    }
  }
} // namespace mf::io

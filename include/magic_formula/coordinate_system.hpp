#pragma once

#include <Eigen/Core>
#include <cstddef>
#include <cstdint>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/sign_transform.hpp>
#include <optional>
#include <string_view>

namespace mf {
  /// The user's tire axis systems (coordinate-systems.md). SA, SR and IA have the same physical meaning in both.
  enum class coordinate_system : std::uint8_t {
    ISO, ///< x forward, y left, z up: a loaded tire has FZ > 0; dFy/dSA > 0 at zero slip
    SAE, ///< x forward, y right, z down: a loaded tire has FZ < 0; dFy/dSA < 0 at zero slip
  };

  /// Enumerator spelling: "ISO" or "SAE".
  [[nodiscard]] std::string_view coordinate_system_name(coordinate_system system) noexcept;

  /// "iso" or "sae" in any ASCII case; empty for anything else, including "ISO-W", "iso_w", "auto" and "".
  [[nodiscard]] std::optional<coordinate_system> parse_coordinate_system(std::string_view text) noexcept;

  /// The data transforms between two systems: {.vertical = from != to, .lateral = false}.
  [[nodiscard]] sign_transforms data_transforms(coordinate_system from, coordinate_system to) noexcept;

  /// Data conversion utility (never used inside evaluation): apply_sign_transforms(layout.inputs,
  /// data_transforms(from, to), inputs). MF 5.x: FZ flips; SA, SR, IA, VX unchanged. Exact; its own inverse.
  /// Precondition: inputs.cols() == layout.inputs.size().
  void convert_inputs(const model_layout &layout, coordinate_system from, coordinate_system to,
                      Eigen::Ref<Eigen::MatrixXd> inputs) noexcept;

  /// The same for outputs: FY, MZ and MY flip; FX, MX and the flags are unchanged. Precondition: outputs.cols() ==
  /// layout.outputs.size().
  void convert_outputs(const model_layout &layout, coordinate_system from, coordinate_system to,
                       Eigen::Ref<Eigen::MatrixXd> outputs) noexcept;

  /// Data conversion of a characteristics matrix (never used inside evaluation):
  /// apply_sign_transforms(layout.characteristics, data_transforms(from, to), characteristics).
  /// MF 5.x: DFY_DSA, FY_OVER_SA and DY flip; the others are unchanged. Precondition: characteristics.cols() ==
  /// layout.characteristics.size().
  void convert_characteristics(const model_layout &layout, coordinate_system from, coordinate_system to,
                               Eigen::Ref<Eigen::MatrixXd> characteristics) noexcept;

  /// FZ sign counts of a table; rows with FZ = +-0 or NaN count as neither sign.
  struct fz_sign_census {
    std::size_t positive = 0;       ///< rows with FZ > 0
    std::size_t negative = 0;       ///< rows with FZ < 0
    std::size_t first_positive = 0; ///< first such row; meaningful when positive > 0
    std::size_t first_negative = 0; ///< first such row; meaningful when negative > 0
  };

  /// Census of the column named "FZ" (`find_channel`); an all-zero census when the layout has no FZ input.
  /// Precondition: inputs.cols() == layout.inputs.size().
  [[nodiscard]] fz_sign_census census_fz(const model_layout &layout,
                                         const Eigen::Ref<const Eigen::MatrixXd> &inputs) noexcept;

  /// At most one warning (key "FZ", index = first offending row) for rows whose FZ sign is the unloaded side of `side`,
  /// the system the rows are in:
  ///   ISO: "FZ < 0 in <n> row(s) of an ISO table: a loaded tire has FZ > 0 in ISO; these rows are lift-off (zero
  ///         forces and moments)<suffix>"
  ///   SAE: "FZ > 0 in <n> row(s) of an SAE table: a loaded tire has FZ < 0 in SAE; these rows are lift-off (zero
  ///         forces and moments)<suffix>"
  /// <suffix> = " (the table mixes FZ signs)" when the table has both signs, else "". Empty when nothing offends.
  [[nodiscard]] std::optional<batch_warning> load_side_warning(const fz_sign_census &census, coordinate_system side);
} // namespace mf

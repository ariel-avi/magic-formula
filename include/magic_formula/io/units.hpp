#pragma once

#include <expected>
#include <magic_formula/error.hpp>
#include <magic_formula/io/tir_reader.hpp>

namespace mf::io {
  /// Checks the [UNITS] section of a .tir file (docs/research/tir-format.md section 4, AD-10 in
  /// docs/research/decisions.md).
  ///
  /// The library works in SI only and does not convert. The six keys LENGTH, FORCE, ANGLE, MASS, TIME and
  /// PRESSURE, when present in [UNITS], must name the SI unit ('meter', 'newton', 'radians', 'kg', 'second',
  /// 'pascal'), compared case-insensitively and ignoring blanks around the text. An absent key, or an absent
  /// [UNITS] section, means SI. MASS is looked up only inside [UNITS] (it is also a number in [INERTIA]). Other
  /// [UNITS] keys are not touched, so they stay unconsumed in `reader`.
  /// @param reader reader over the document; the six keys are marked consumed
  /// @return nothing on success; UNSUPPORTED_UNIT for the first (in the order above) key with another unit, naming the
  ///         key and the value, located at the file, the line of that [UNITS] entry and the key
  [[nodiscard]] std::expected<void, error> check_units(tir_reader &reader);
} // namespace mf::io

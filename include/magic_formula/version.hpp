#pragma once

#include <string_view>

namespace mf {
  /// Returns the library version as "MAJOR.MINOR.PATCH", i.e. the CMake project version (e.g. "0.1.0").
  [[nodiscard]] std::string_view version() noexcept;
} // namespace mf

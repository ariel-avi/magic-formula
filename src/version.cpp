#include <magic_formula/version.hpp>

namespace mf {
  std::string_view version() noexcept {
    return MF_VERSION_STRING;
  }
} // namespace mf

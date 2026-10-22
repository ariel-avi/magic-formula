#pragma once

#include <cstdint>
#include <magic_formula/mf5x_coefficients.hpp>
#include <span>
#include <string_view>

namespace mf::io {
  /// How the loader treats a bound key (see docs/research/tir-format.md).
  enum class binding_role : std::uint8_t {
    /// Must be present (FNOMIN, UNLOADED_RADIUS and the critical keys): absent gives MISSING_KEY.
    REQUIRED,
    /// Scaling factor L*: absent keeps the default 1, silently.
    SCALING,
    /// Pure-slip, overturning, rolling or aligning coefficient (P*, Q*, SSZ*): absent keeps 0 and is listed in the
    /// "defaulted" warning.
    COEFFICIENT,
    /// Combined-slip coefficient (the 20 R* keys): absent selects the PAC2002 friction ellipse or the MF 5.2
    /// default 0.
    COMBINED,
  };

  /// Binds one .tir key to the `mf5x_coefficients` member of the same name.
  struct coefficient_binding {
    /// Upper-case .tir key, equal to the member name.
    std::string_view key;
    /// The coefficient member that receives the value.
    double mf5x_coefficients::*member;
    /// How an absent key is handled.
    binding_role role;
  };

  /// The 113 bindings shared by PAC2002 and MF 5.2, in `mf5x_coefficients` declaration order. Excludes LONGVL
  /// (handled by the loader) and the PAC2002-only camber factors LGAX, LGAY and LGAZ
  /// (`pac2002_camber_bindings`).
  ///
  /// Roles: REQUIRED 9 (FNOMIN, UNLOADED_RADIUS, PCX1, PDX1, PKX1, PCY1, PDY1, PKY1, PKY2), SCALING 22,
  /// COEFFICIENT 62 and COMBINED 20 (RBX1 RBX2 RCX1 REX1 REX2 RHX1 RBY1 RBY2 RBY3 RCY1 REY1 REY2 RHY1 RHY2 RVY1
  /// RVY2 RVY3 RVY4 RVY5 RVY6).
  /// Every key is unique and so is every member.
  [[nodiscard]] std::span<const coefficient_binding> mf5x_common_bindings() noexcept;

  /// The three PAC2002-only camber scaling factors LGAX, LGAY and LGAZ (role SCALING). MF 5.2 has no such keys: the
  /// model forces them to 1.
  [[nodiscard]] std::span<const coefficient_binding> pac2002_camber_bindings() noexcept;

  /// The 70 keys that exist in PAC2002 or MF-Tyre 6.x but not in MF 5.2, in this order:
  ///  - the 30 PAC2002 / 6.x coefficient keys LGAX LGAY LGAZ PKY4-7 PEY5 RBX3 RBY4 QSX4-14 QSY5-8 QDZ10-11 LKYC LKZC
  ///    LMP;
  ///  - the 17 pressure keys INFLPRES NOMPRES PRESMIN PRESMAX PPX1-4 PPY1-5 PPZ1-2 PPMX1 PFZ1;
  ///  - the 20 turn-slip keys PDXP1-3 PKYP1 PDYP1-4 PHYP1-4 PECP1-2 QDTP1 QCRP1-2 QBRP1 QDRP1-2;
  ///  - Q_FYS1-3.
  /// Further keys that MF 5.2 lacks are named by section (`mf52_foreign_sections`). A present one gives one warning
  /// for an MF 5.2 file.
  [[nodiscard]] std::span<const std::string_view> mf52_foreign_keys() noexcept;

  /// The sections of which every key is foreign to MF 5.2: STRUCTURAL, INERTIA and CONTACT_PATCH (the enveloping
  /// section), upper-case. Every key found in one of them joins the foreign-key warning of an MF 5.2 file. Only the
  /// keys inside these sections count: the unit name `MASS` in [UNITS] is not affected.
  [[nodiscard]] std::span<const std::string_view> mf52_foreign_sections() noexcept;
} // namespace mf::io

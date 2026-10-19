#include <array>
#include <magic_formula/io/mf5x_bindings.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <span>
#include <string_view>

namespace mf::io {
  namespace {
    /// One table row. The key text is written next to the member of the same name; the bindings test checks that they
    /// agree and that no member is missing or repeated.
    constexpr coefficient_binding make_binding(std::string_view key, double mf5x_coefficients::*member,
                                               binding_role role) noexcept {
      return coefficient_binding{.key = key, .member = member, .role = role};
    }

    // clang-format off
    /// The common table, in `mf5x_coefficients` declaration order without LONGVL and LGAX/LGAY/LGAZ.
    /// REQUIRED 9 (AD-12), SCALING 22, COEFFICIENT 62, COMBINED 20 (the R* keys: D-07, docs/research/pac2002.md
    /// section 5.5, README.md decision list).
    constexpr std::array<coefficient_binding, 113> COMMON_BINDINGS{
        make_binding("FNOMIN", &mf5x_coefficients::FNOMIN, binding_role::REQUIRED),
        make_binding("UNLOADED_RADIUS", &mf5x_coefficients::UNLOADED_RADIUS, binding_role::REQUIRED),

        make_binding("LFZO", &mf5x_coefficients::LFZO, binding_role::SCALING),
        make_binding("LCX", &mf5x_coefficients::LCX, binding_role::SCALING),
        make_binding("LMUX", &mf5x_coefficients::LMUX, binding_role::SCALING),
        make_binding("LEX", &mf5x_coefficients::LEX, binding_role::SCALING),
        make_binding("LKX", &mf5x_coefficients::LKX, binding_role::SCALING),
        make_binding("LHX", &mf5x_coefficients::LHX, binding_role::SCALING),
        make_binding("LVX", &mf5x_coefficients::LVX, binding_role::SCALING),
        make_binding("LCY", &mf5x_coefficients::LCY, binding_role::SCALING),
        make_binding("LMUY", &mf5x_coefficients::LMUY, binding_role::SCALING),
        make_binding("LEY", &mf5x_coefficients::LEY, binding_role::SCALING),
        make_binding("LKY", &mf5x_coefficients::LKY, binding_role::SCALING),
        make_binding("LHY", &mf5x_coefficients::LHY, binding_role::SCALING),
        make_binding("LVY", &mf5x_coefficients::LVY, binding_role::SCALING),
        make_binding("LTR", &mf5x_coefficients::LTR, binding_role::SCALING),
        make_binding("LRES", &mf5x_coefficients::LRES, binding_role::SCALING),
        make_binding("LXAL", &mf5x_coefficients::LXAL, binding_role::SCALING),
        make_binding("LYKA", &mf5x_coefficients::LYKA, binding_role::SCALING),
        make_binding("LVYKA", &mf5x_coefficients::LVYKA, binding_role::SCALING),
        make_binding("LS", &mf5x_coefficients::LS, binding_role::SCALING),
        make_binding("LMX", &mf5x_coefficients::LMX, binding_role::SCALING),
        make_binding("LVMX", &mf5x_coefficients::LVMX, binding_role::SCALING),
        make_binding("LMY", &mf5x_coefficients::LMY, binding_role::SCALING),

        make_binding("PCX1", &mf5x_coefficients::PCX1, binding_role::REQUIRED),
        make_binding("PDX1", &mf5x_coefficients::PDX1, binding_role::REQUIRED),
        make_binding("PDX2", &mf5x_coefficients::PDX2, binding_role::COEFFICIENT),
        make_binding("PDX3", &mf5x_coefficients::PDX3, binding_role::COEFFICIENT),
        make_binding("PEX1", &mf5x_coefficients::PEX1, binding_role::COEFFICIENT),
        make_binding("PEX2", &mf5x_coefficients::PEX2, binding_role::COEFFICIENT),
        make_binding("PEX3", &mf5x_coefficients::PEX3, binding_role::COEFFICIENT),
        make_binding("PEX4", &mf5x_coefficients::PEX4, binding_role::COEFFICIENT),
        make_binding("PKX1", &mf5x_coefficients::PKX1, binding_role::REQUIRED),
        make_binding("PKX2", &mf5x_coefficients::PKX2, binding_role::COEFFICIENT),
        make_binding("PKX3", &mf5x_coefficients::PKX3, binding_role::COEFFICIENT),
        make_binding("PHX1", &mf5x_coefficients::PHX1, binding_role::COEFFICIENT),
        make_binding("PHX2", &mf5x_coefficients::PHX2, binding_role::COEFFICIENT),
        make_binding("PVX1", &mf5x_coefficients::PVX1, binding_role::COEFFICIENT),
        make_binding("PVX2", &mf5x_coefficients::PVX2, binding_role::COEFFICIENT),
        make_binding("RBX1", &mf5x_coefficients::RBX1, binding_role::COMBINED),
        make_binding("RBX2", &mf5x_coefficients::RBX2, binding_role::COMBINED),
        make_binding("RCX1", &mf5x_coefficients::RCX1, binding_role::COMBINED),
        make_binding("REX1", &mf5x_coefficients::REX1, binding_role::COMBINED),
        make_binding("REX2", &mf5x_coefficients::REX2, binding_role::COMBINED),
        make_binding("RHX1", &mf5x_coefficients::RHX1, binding_role::COMBINED),

        make_binding("PCY1", &mf5x_coefficients::PCY1, binding_role::REQUIRED),
        make_binding("PDY1", &mf5x_coefficients::PDY1, binding_role::REQUIRED),
        make_binding("PDY2", &mf5x_coefficients::PDY2, binding_role::COEFFICIENT),
        make_binding("PDY3", &mf5x_coefficients::PDY3, binding_role::COEFFICIENT),
        make_binding("PEY1", &mf5x_coefficients::PEY1, binding_role::COEFFICIENT),
        make_binding("PEY2", &mf5x_coefficients::PEY2, binding_role::COEFFICIENT),
        make_binding("PEY3", &mf5x_coefficients::PEY3, binding_role::COEFFICIENT),
        make_binding("PEY4", &mf5x_coefficients::PEY4, binding_role::COEFFICIENT),
        make_binding("PKY1", &mf5x_coefficients::PKY1, binding_role::REQUIRED),
        make_binding("PKY2", &mf5x_coefficients::PKY2, binding_role::REQUIRED),
        make_binding("PKY3", &mf5x_coefficients::PKY3, binding_role::COEFFICIENT),
        make_binding("PHY1", &mf5x_coefficients::PHY1, binding_role::COEFFICIENT),
        make_binding("PHY2", &mf5x_coefficients::PHY2, binding_role::COEFFICIENT),
        make_binding("PHY3", &mf5x_coefficients::PHY3, binding_role::COEFFICIENT),
        make_binding("PVY1", &mf5x_coefficients::PVY1, binding_role::COEFFICIENT),
        make_binding("PVY2", &mf5x_coefficients::PVY2, binding_role::COEFFICIENT),
        make_binding("PVY3", &mf5x_coefficients::PVY3, binding_role::COEFFICIENT),
        make_binding("PVY4", &mf5x_coefficients::PVY4, binding_role::COEFFICIENT),
        make_binding("RBY1", &mf5x_coefficients::RBY1, binding_role::COMBINED),
        make_binding("RBY2", &mf5x_coefficients::RBY2, binding_role::COMBINED),
        make_binding("RBY3", &mf5x_coefficients::RBY3, binding_role::COMBINED),
        make_binding("RCY1", &mf5x_coefficients::RCY1, binding_role::COMBINED),
        make_binding("REY1", &mf5x_coefficients::REY1, binding_role::COMBINED),
        make_binding("REY2", &mf5x_coefficients::REY2, binding_role::COMBINED),
        make_binding("RHY1", &mf5x_coefficients::RHY1, binding_role::COMBINED),
        make_binding("RHY2", &mf5x_coefficients::RHY2, binding_role::COMBINED),
        make_binding("RVY1", &mf5x_coefficients::RVY1, binding_role::COMBINED),
        make_binding("RVY2", &mf5x_coefficients::RVY2, binding_role::COMBINED),
        make_binding("RVY3", &mf5x_coefficients::RVY3, binding_role::COMBINED),
        make_binding("RVY4", &mf5x_coefficients::RVY4, binding_role::COMBINED),
        make_binding("RVY5", &mf5x_coefficients::RVY5, binding_role::COMBINED),
        make_binding("RVY6", &mf5x_coefficients::RVY6, binding_role::COMBINED),

        make_binding("QSX1", &mf5x_coefficients::QSX1, binding_role::COEFFICIENT),
        make_binding("QSX2", &mf5x_coefficients::QSX2, binding_role::COEFFICIENT),
        make_binding("QSX3", &mf5x_coefficients::QSX3, binding_role::COEFFICIENT),

        make_binding("QSY1", &mf5x_coefficients::QSY1, binding_role::COEFFICIENT),
        make_binding("QSY2", &mf5x_coefficients::QSY2, binding_role::COEFFICIENT),
        make_binding("QSY3", &mf5x_coefficients::QSY3, binding_role::COEFFICIENT),
        make_binding("QSY4", &mf5x_coefficients::QSY4, binding_role::COEFFICIENT),

        make_binding("QBZ1", &mf5x_coefficients::QBZ1, binding_role::COEFFICIENT),
        make_binding("QBZ2", &mf5x_coefficients::QBZ2, binding_role::COEFFICIENT),
        make_binding("QBZ3", &mf5x_coefficients::QBZ3, binding_role::COEFFICIENT),
        make_binding("QBZ4", &mf5x_coefficients::QBZ4, binding_role::COEFFICIENT),
        make_binding("QBZ5", &mf5x_coefficients::QBZ5, binding_role::COEFFICIENT),
        make_binding("QBZ9", &mf5x_coefficients::QBZ9, binding_role::COEFFICIENT),
        make_binding("QBZ10", &mf5x_coefficients::QBZ10, binding_role::COEFFICIENT),
        make_binding("QCZ1", &mf5x_coefficients::QCZ1, binding_role::COEFFICIENT),
        make_binding("QDZ1", &mf5x_coefficients::QDZ1, binding_role::COEFFICIENT),
        make_binding("QDZ2", &mf5x_coefficients::QDZ2, binding_role::COEFFICIENT),
        make_binding("QDZ3", &mf5x_coefficients::QDZ3, binding_role::COEFFICIENT),
        make_binding("QDZ4", &mf5x_coefficients::QDZ4, binding_role::COEFFICIENT),
        make_binding("QDZ6", &mf5x_coefficients::QDZ6, binding_role::COEFFICIENT),
        make_binding("QDZ7", &mf5x_coefficients::QDZ7, binding_role::COEFFICIENT),
        make_binding("QDZ8", &mf5x_coefficients::QDZ8, binding_role::COEFFICIENT),
        make_binding("QDZ9", &mf5x_coefficients::QDZ9, binding_role::COEFFICIENT),
        make_binding("QEZ1", &mf5x_coefficients::QEZ1, binding_role::COEFFICIENT),
        make_binding("QEZ2", &mf5x_coefficients::QEZ2, binding_role::COEFFICIENT),
        make_binding("QEZ3", &mf5x_coefficients::QEZ3, binding_role::COEFFICIENT),
        make_binding("QEZ4", &mf5x_coefficients::QEZ4, binding_role::COEFFICIENT),
        make_binding("QEZ5", &mf5x_coefficients::QEZ5, binding_role::COEFFICIENT),
        make_binding("QHZ1", &mf5x_coefficients::QHZ1, binding_role::COEFFICIENT),
        make_binding("QHZ2", &mf5x_coefficients::QHZ2, binding_role::COEFFICIENT),
        make_binding("QHZ3", &mf5x_coefficients::QHZ3, binding_role::COEFFICIENT),
        make_binding("QHZ4", &mf5x_coefficients::QHZ4, binding_role::COEFFICIENT),
        make_binding("SSZ1", &mf5x_coefficients::SSZ1, binding_role::COEFFICIENT),
        make_binding("SSZ2", &mf5x_coefficients::SSZ2, binding_role::COEFFICIENT),
        make_binding("SSZ3", &mf5x_coefficients::SSZ3, binding_role::COEFFICIENT),
        make_binding("SSZ4", &mf5x_coefficients::SSZ4, binding_role::COEFFICIENT),
    };

    /// The PAC2002-only camber scaling factors.
    constexpr std::array<coefficient_binding, 3> CAMBER_BINDINGS{
        make_binding("LGAX", &mf5x_coefficients::LGAX, binding_role::SCALING),
        make_binding("LGAY", &mf5x_coefficients::LGAY, binding_role::SCALING),
        make_binding("LGAZ", &mf5x_coefficients::LGAZ, binding_role::SCALING),
    };

    /// Keys of PAC2002 / MF-Tyre 6.x that MF 5.2 does not have (docs/research/mf52.md section 3, "not MF 5.2"): the 30
    /// PAC2002 / 6.x coefficient keys, the pressure set, the turn-slip set and Q_FYS1-3.
    constexpr std::array<std::string_view, 70> FOREIGN_KEYS{
        "LGAX", "LGAY", "LGAZ", "PKY4", "PKY5", "PKY6", "PKY7", "PEY5", "RBX3", "RBY4",
        "QSX4", "QSX5", "QSX6", "QSX7", "QSX8", "QSX9", "QSX10", "QSX11", "QSX12", "QSX13",
        "QSX14", "QSY5", "QSY6", "QSY7", "QSY8", "QDZ10", "QDZ11", "LKYC", "LKZC", "LMP",
        "INFLPRES", "NOMPRES", "PRESMIN", "PRESMAX", "PPX1", "PPX2", "PPX3", "PPX4", "PPY1", "PPY2",
        "PPY3", "PPY4", "PPY5", "PPZ1", "PPZ2", "PPMX1", "PFZ1",
        "PDXP1", "PDXP2", "PDXP3", "PKYP1", "PDYP1", "PDYP2", "PDYP3", "PDYP4", "PHYP1", "PHYP2",
        "PHYP3", "PHYP4", "PECP1", "PECP2", "QDTP1", "QCRP1", "QCRP2", "QBRP1", "QDRP1", "QDRP2",
        "Q_FYS1", "Q_FYS2", "Q_FYS3",
    };

    /// Sections whose every key is foreign to MF 5.2 (docs/research/mf52.md section 3): [STRUCTURAL], [INERTIA] and
    /// the enveloping section [CONTACT_PATCH].
    constexpr std::array<std::string_view, 3> FOREIGN_SECTIONS{"STRUCTURAL", "INERTIA", "CONTACT_PATCH"};
    // clang-format on
  } // namespace

  std::span<const coefficient_binding> mf5x_common_bindings() noexcept {
    return COMMON_BINDINGS;
  }

  std::span<const coefficient_binding> pac2002_camber_bindings() noexcept {
    return CAMBER_BINDINGS;
  }

  std::span<const std::string_view> mf52_foreign_keys() noexcept {
    return FOREIGN_KEYS;
  }

  std::span<const std::string_view> mf52_foreign_sections() noexcept {
    return FOREIGN_SECTIONS;
  }
} // namespace mf::io

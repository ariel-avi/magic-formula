#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <gtest/gtest.h>
#include <magic_formula/io/mf5x_bindings.hpp>
#include <magic_formula/io/tir_document.hpp>
#include <magic_formula/io/tir_reader.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
  using mf::io::binding_role;
  using mf::io::coefficient_binding;
  using mf::io::mf52_foreign_keys;
  using mf::io::mf52_foreign_sections;
  using mf::io::mf5x_common_bindings;
  using mf::io::pac2002_camber_bindings;
  using coeffs = mf::mf5x_coefficients;

  /// Absolute tolerance for small integers that are exactly representable.
  constexpr double VALUE_TOL = 1e-15;

  /// A key written out independently of the table under test, with the member it must bind.
  struct named_member {
    std::string_view name;
    double coeffs::*member;
  };

  constexpr named_member entry(std::string_view name, double coeffs::*member) noexcept {
    return named_member{.name = name, .member = member};
  }

  // clang-format off
  /// The expected common table in `mf5x_coefficients` declaration order (no LONGVL, no LGAX/LGAY/LGAZ).
  constexpr std::array<named_member, 113> COMMON_EXPECTED{
      entry("FNOMIN", &coeffs::FNOMIN), entry("UNLOADED_RADIUS", &coeffs::UNLOADED_RADIUS),
      entry("LFZO", &coeffs::LFZO), entry("LCX", &coeffs::LCX), entry("LMUX", &coeffs::LMUX),
      entry("LEX", &coeffs::LEX), entry("LKX", &coeffs::LKX), entry("LHX", &coeffs::LHX),
      entry("LVX", &coeffs::LVX), entry("LCY", &coeffs::LCY), entry("LMUY", &coeffs::LMUY),
      entry("LEY", &coeffs::LEY), entry("LKY", &coeffs::LKY), entry("LHY", &coeffs::LHY),
      entry("LVY", &coeffs::LVY), entry("LTR", &coeffs::LTR), entry("LRES", &coeffs::LRES),
      entry("LXAL", &coeffs::LXAL), entry("LYKA", &coeffs::LYKA), entry("LVYKA", &coeffs::LVYKA),
      entry("LS", &coeffs::LS), entry("LMX", &coeffs::LMX), entry("LVMX", &coeffs::LVMX),
      entry("LMY", &coeffs::LMY),
      entry("PCX1", &coeffs::PCX1), entry("PDX1", &coeffs::PDX1), entry("PDX2", &coeffs::PDX2),
      entry("PDX3", &coeffs::PDX3), entry("PEX1", &coeffs::PEX1), entry("PEX2", &coeffs::PEX2),
      entry("PEX3", &coeffs::PEX3), entry("PEX4", &coeffs::PEX4), entry("PKX1", &coeffs::PKX1),
      entry("PKX2", &coeffs::PKX2), entry("PKX3", &coeffs::PKX3), entry("PHX1", &coeffs::PHX1),
      entry("PHX2", &coeffs::PHX2), entry("PVX1", &coeffs::PVX1), entry("PVX2", &coeffs::PVX2),
      entry("RBX1", &coeffs::RBX1), entry("RBX2", &coeffs::RBX2), entry("RCX1", &coeffs::RCX1),
      entry("REX1", &coeffs::REX1), entry("REX2", &coeffs::REX2), entry("RHX1", &coeffs::RHX1),
      entry("PCY1", &coeffs::PCY1), entry("PDY1", &coeffs::PDY1), entry("PDY2", &coeffs::PDY2),
      entry("PDY3", &coeffs::PDY3), entry("PEY1", &coeffs::PEY1), entry("PEY2", &coeffs::PEY2),
      entry("PEY3", &coeffs::PEY3), entry("PEY4", &coeffs::PEY4), entry("PKY1", &coeffs::PKY1),
      entry("PKY2", &coeffs::PKY2), entry("PKY3", &coeffs::PKY3), entry("PHY1", &coeffs::PHY1),
      entry("PHY2", &coeffs::PHY2), entry("PHY3", &coeffs::PHY3), entry("PVY1", &coeffs::PVY1),
      entry("PVY2", &coeffs::PVY2), entry("PVY3", &coeffs::PVY3), entry("PVY4", &coeffs::PVY4),
      entry("RBY1", &coeffs::RBY1), entry("RBY2", &coeffs::RBY2), entry("RBY3", &coeffs::RBY3),
      entry("RCY1", &coeffs::RCY1), entry("REY1", &coeffs::REY1), entry("REY2", &coeffs::REY2),
      entry("RHY1", &coeffs::RHY1), entry("RHY2", &coeffs::RHY2), entry("RVY1", &coeffs::RVY1),
      entry("RVY2", &coeffs::RVY2), entry("RVY3", &coeffs::RVY3), entry("RVY4", &coeffs::RVY4),
      entry("RVY5", &coeffs::RVY5), entry("RVY6", &coeffs::RVY6),
      entry("QSX1", &coeffs::QSX1), entry("QSX2", &coeffs::QSX2), entry("QSX3", &coeffs::QSX3),
      entry("QSY1", &coeffs::QSY1), entry("QSY2", &coeffs::QSY2), entry("QSY3", &coeffs::QSY3),
      entry("QSY4", &coeffs::QSY4),
      entry("QBZ1", &coeffs::QBZ1), entry("QBZ2", &coeffs::QBZ2), entry("QBZ3", &coeffs::QBZ3),
      entry("QBZ4", &coeffs::QBZ4), entry("QBZ5", &coeffs::QBZ5), entry("QBZ9", &coeffs::QBZ9),
      entry("QBZ10", &coeffs::QBZ10), entry("QCZ1", &coeffs::QCZ1), entry("QDZ1", &coeffs::QDZ1),
      entry("QDZ2", &coeffs::QDZ2), entry("QDZ3", &coeffs::QDZ3), entry("QDZ4", &coeffs::QDZ4),
      entry("QDZ6", &coeffs::QDZ6), entry("QDZ7", &coeffs::QDZ7), entry("QDZ8", &coeffs::QDZ8),
      entry("QDZ9", &coeffs::QDZ9), entry("QEZ1", &coeffs::QEZ1), entry("QEZ2", &coeffs::QEZ2),
      entry("QEZ3", &coeffs::QEZ3), entry("QEZ4", &coeffs::QEZ4), entry("QEZ5", &coeffs::QEZ5),
      entry("QHZ1", &coeffs::QHZ1), entry("QHZ2", &coeffs::QHZ2), entry("QHZ3", &coeffs::QHZ3),
      entry("QHZ4", &coeffs::QHZ4), entry("SSZ1", &coeffs::SSZ1), entry("SSZ2", &coeffs::SSZ2),
      entry("SSZ3", &coeffs::SSZ3), entry("SSZ4", &coeffs::SSZ4),
  };

  /// The keys that MF 5.2 does not have (docs/research/mf52.md), written out independently of the library.
  constexpr std::array<std::string_view, 70> EXPECTED_FOREIGN{
      // 30 PAC2002 / MF-Tyre 6.x coefficient keys
      "LGAX", "LGAY", "LGAZ", "PKY4", "PKY5", "PKY6", "PKY7", "PEY5", "RBX3", "RBY4",
      "QSX4", "QSX5", "QSX6", "QSX7", "QSX8", "QSX9", "QSX10", "QSX11", "QSX12", "QSX13",
      "QSX14", "QSY5", "QSY6", "QSY7", "QSY8", "QDZ10", "QDZ11", "LKYC", "LKZC", "LMP",
      // 17 pressure keys
      "INFLPRES", "NOMPRES", "PRESMIN", "PRESMAX", "PPX1", "PPX2", "PPX3", "PPX4", "PPY1", "PPY2",
      "PPY3", "PPY4", "PPY5", "PPZ1", "PPZ2", "PPMX1", "PFZ1",
      // 20 turn-slip keys
      "PDXP1", "PDXP2", "PDXP3", "PKYP1", "PDYP1", "PDYP2", "PDYP3", "PDYP4", "PHYP1", "PHYP2",
      "PHYP3", "PHYP4", "PECP1", "PECP2", "QDTP1", "QCRP1", "QCRP2", "QBRP1", "QDRP1", "QDRP2",
      // Q_FYS1-3
      "Q_FYS1", "Q_FYS2", "Q_FYS3",
  };

  /// The sections of which every key is foreign to MF 5.2.
  constexpr std::array<std::string_view, 3> EXPECTED_FOREIGN_SECTIONS{"STRUCTURAL", "INERTIA", "CONTACT_PATCH"};

  /// The REQUIRED keys (the two required model keys and the seven critical coefficients).
  constexpr std::array<std::string_view, 9> EXPECTED_REQUIRED{
      "FNOMIN", "UNLOADED_RADIUS", "PCX1", "PDX1", "PKX1", "PCY1", "PDY1", "PKY1", "PKY2",
  };

  /// The 20 combined-slip R* keys.
  constexpr std::array<std::string_view, 20> EXPECTED_COMBINED{
      "RBX1", "RBX2", "RCX1", "REX1", "REX2", "RHX1", "RBY1", "RBY2", "RBY3", "RCY1",
      "REY1", "REY2", "RHY1", "RHY2", "RVY1", "RVY2", "RVY3", "RVY4", "RVY5", "RVY6",
  };
  // clang-format on

  /// Keys of the common bindings that have `role`.
  std::set<std::string_view> keys_with_role(binding_role role) {
    std::set<std::string_view> keys;
    for (const coefficient_binding &binding : mf5x_common_bindings()) {
      if (binding.role == role) {
        keys.insert(binding.key);
      }
    }
    return keys;
  }

  /// True when the common table binds `key`.
  bool is_common_key(std::string_view key) {
    for (const coefficient_binding &binding : mf5x_common_bindings()) {
      if (binding.key == key) {
        return true;
      }
    }
    return false;
  }

  /// Common bindings followed by the camber bindings.
  std::vector<coefficient_binding> all_bindings() {
    const std::span<const coefficient_binding> common = mf5x_common_bindings();
    const std::span<const coefficient_binding> camber = pac2002_camber_bindings();
    std::vector<coefficient_binding> all(common.begin(), common.end());
    all.insert(all.end(), camber.begin(), camber.end());
    return all;
  }
} // namespace

TEST(mf5x_bindings, Count_Common_Is113) {
  EXPECT_EQ(mf5x_common_bindings().size(), 113U);
}

TEST(mf5x_bindings, Count_Camber_Is3ScalingLga) {
  const std::span<const coefficient_binding> camber = pac2002_camber_bindings();
  ASSERT_EQ(camber.size(), 3U);
  EXPECT_EQ(camber[0].key, "LGAX");
  EXPECT_TRUE(camber[0].member == &coeffs::LGAX);
  EXPECT_EQ(camber[1].key, "LGAY");
  EXPECT_TRUE(camber[1].member == &coeffs::LGAY);
  EXPECT_EQ(camber[2].key, "LGAZ");
  EXPECT_TRUE(camber[2].member == &coeffs::LGAZ);
  for (const coefficient_binding &binding : camber) {
    EXPECT_EQ(binding.role, binding_role::SCALING) << binding.key;
  }
}

TEST(mf5x_bindings, Keys_Unique) {
  const std::vector<coefficient_binding> all = all_bindings();
  std::set<std::string_view> keys;
  for (const coefficient_binding &binding : all) {
    keys.insert(binding.key);
  }
  EXPECT_EQ(keys.size(), all.size());
}

TEST(mf5x_bindings, Members_Unique) {
  // Member pointers have no total order, so compare the addresses of the members inside one object.
  const coeffs object;
  const std::vector<coefficient_binding> all = all_bindings();
  std::set<const double *> addresses;
  for (const coefficient_binding &binding : all) {
    addresses.insert(&(object.*binding.member));
  }
  EXPECT_EQ(addresses.size(), all.size());
}

TEST(mf5x_bindings, Members_PlusLongvl_CoverStruct) {
  const coeffs object;
  const std::vector<coefficient_binding> all = all_bindings();
  std::set<const double *> addresses;
  for (const coefficient_binding &binding : all) {
    addresses.insert(&(object.*binding.member));
  }
  // 113 common + 3 camber + LONGVL fill the struct (which has no padding), and LONGVL is not bound.
  EXPECT_EQ(mf5x_common_bindings().size() + pac2002_camber_bindings().size() + 1U, sizeof(coeffs) / sizeof(double));
  EXPECT_FALSE(addresses.contains(&object.LONGVL));
}

TEST(mf5x_bindings, Keys_MatchMemberNamesAndDeclarationOrder) {
  const std::span<const coefficient_binding> common = mf5x_common_bindings();
  ASSERT_EQ(common.size(), COMMON_EXPECTED.size());
  for (std::size_t index = 0; index < common.size(); ++index) {
    EXPECT_EQ(common[index].key, COMMON_EXPECTED[index].name) << index;
    EXPECT_TRUE(common[index].member == COMMON_EXPECTED[index].member) << common[index].key;
  }
}

TEST(mf5x_bindings, Roles_Common_Are9_22_62_20) {
  EXPECT_EQ(keys_with_role(binding_role::REQUIRED).size(), 9U);
  EXPECT_EQ(keys_with_role(binding_role::SCALING).size(), 22U);
  EXPECT_EQ(keys_with_role(binding_role::COEFFICIENT).size(), 62U);
  EXPECT_EQ(keys_with_role(binding_role::COMBINED).size(), 20U);
}

TEST(mf5x_bindings, Required_EqualsRequiredKeySet) {
  const std::set<std::string_view> expected(EXPECTED_REQUIRED.begin(), EXPECTED_REQUIRED.end());
  EXPECT_EQ(keys_with_role(binding_role::REQUIRED), expected);
}

TEST(mf5x_bindings, Combined_EqualsCombinedSlipKeySet) {
  const std::set<std::string_view> expected(EXPECTED_COMBINED.begin(), EXPECTED_COMBINED.end());
  EXPECT_EQ(keys_with_role(binding_role::COMBINED), expected);
}

TEST(mf5x_bindings, Scaling_AreLKeysAndExcludeLga) {
  const std::set<std::string_view> scaling = keys_with_role(binding_role::SCALING);
  for (const std::string_view key : scaling) {
    EXPECT_TRUE(key.starts_with('L')) << key;
    EXPECT_FALSE(key.starts_with("LGA")) << key;
  }
  EXPECT_FALSE(scaling.contains("LONGVL"));
}

TEST(mf5x_bindings, Coefficient_AreNotScalingNorCombinedKeys) {
  for (const std::string_view key : keys_with_role(binding_role::COEFFICIENT)) {
    const bool is_p_q_or_s = key.starts_with('P') || key.starts_with('Q') || key.starts_with('S');
    EXPECT_TRUE(is_p_q_or_s) << key;
  }
}

TEST(mf5x_bindings, ForeignKeys_EqualMf52Section3) {
  const std::span<const std::string_view> foreign = mf52_foreign_keys();
  EXPECT_EQ(foreign.size(), 70U);
  EXPECT_TRUE(std::ranges::equal(foreign, EXPECTED_FOREIGN));
}

TEST(mf5x_bindings, ForeignKeys_AreUnique) {
  const std::span<const std::string_view> foreign = mf52_foreign_keys();
  const std::set<std::string_view> unique(foreign.begin(), foreign.end());
  EXPECT_EQ(unique.size(), foreign.size());
}

TEST(mf5x_bindings, ForeignSections_EqualMf52Section3) {
  const std::span<const std::string_view> sections = mf52_foreign_sections();
  EXPECT_EQ(sections.size(), 3U);
  EXPECT_TRUE(std::ranges::equal(sections, EXPECTED_FOREIGN_SECTIONS));
}

TEST(mf5x_bindings, ForeignKeys_AreNotCommonKeys) {
  // The common table holds only keys that MF 5.2 has; the foreign LGA* keys live in the camber table.
  for (const std::string_view key : mf52_foreign_keys()) {
    EXPECT_FALSE(is_common_key(key)) << key;
  }
}

TEST(mf5x_bindings, RoundTrip_GeneratedTir_LoadsIntoMembers) {
  const std::vector<coefficient_binding> all = all_bindings();
  std::string text = "[COEFFICIENTS]\n";
  for (std::size_t index = 0; index < all.size(); ++index) {
    text += std::format("{} = {}\n", all[index].key, index + 1);
  }
  const auto document = mf::io::tir_document::parse(text, "generated.tir");
  ASSERT_TRUE(document.has_value());
  mf::io::tir_reader reader(*document);

  coeffs loaded;
  for (const coefficient_binding &binding : all) {
    const auto value = reader.number(binding.key);
    ASSERT_TRUE(value.has_value()) << binding.key;
    const std::optional<double> number = *value;
    ASSERT_TRUE(number.has_value()) << binding.key;
    loaded.*binding.member = *number;
  }

  for (std::size_t index = 0; index < all.size(); ++index) {
    EXPECT_NEAR(loaded.*all[index].member, static_cast<double>(index + 1), VALUE_TOL) << all[index].key;
  }
  EXPECT_NEAR(loaded.LONGVL, 1.0, VALUE_TOL);
  EXPECT_TRUE(reader.unconsumed_keys().empty());
}

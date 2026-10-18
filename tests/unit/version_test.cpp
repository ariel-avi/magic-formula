#include <gtest/gtest.h>
#include <magic_formula/version.hpp>
#include <string_view>

TEST(version, Value_Default_EqualsProjectVersion) {
  EXPECT_FALSE(mf::version().empty());
  EXPECT_EQ(mf::version(), std::string_view{MF_VERSION_STRING});
}

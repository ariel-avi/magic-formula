#include <cstddef>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <ios>
#include <magic_formula/error.hpp>
#include <magic_formula/io/text_file.hpp>
#include <random>
#include <string>
#include <string_view>
#include <system_error>

namespace {
  /// Fixture that owns one unique per-test, per-process directory under the system temp directory and removes it
  /// (with its content) afterwards. `path_` names a file inside it that does not exist until a test writes it.
  class read_text_file : public ::testing::Test {
  protected:
    void SetUp() override {
      const auto *const info = ::testing::UnitTest::GetInstance()->current_test_info();
      std::random_device device;
      const std::string name = std::string("mf_read_text_file_") + info->name() + "_" + std::to_string(device());
      directory_ = std::filesystem::temp_directory_path() / name;
      std::filesystem::create_directories(directory_);
      path_ = directory_ / "content.bin";
    }

    void TearDown() override {
      std::error_code ignored;
      static_cast<void>(std::filesystem::remove_all(directory_, ignored));
    }

    void write_binary(std::string_view content) const {
      std::ofstream output(path_, std::ios::binary | std::ios::trunc);
      output.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    std::filesystem::path directory_;
    std::filesystem::path path_;
  };
} // namespace

TEST_F(read_text_file, ReadTextFile_NonexistentPath_ReturnsIoErrorWithFile) {
  const auto result = mf::io::read_text_file(path_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::IO);
  EXPECT_EQ(result.error().where.file, path_.string());
}

TEST_F(read_text_file, ReadTextFile_Directory_ReturnsIoErrorWithFile) {
  const auto result = mf::io::read_text_file(directory_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::IO);
  EXPECT_EQ(result.error().where.file, directory_.string());
}

// On Linux /proc/self/mem opens, but reading offset 0 (never mapped) fails with EIO, so libstdc++ sets badbit and the
// read-error branch reports it. Where /proc does not exist (macOS, Windows) the open fails; either way the result
// is an IO error naming the path.
TEST_F(read_text_file, ReadTextFile_ReadErrorAfterOpen_ReturnsIoErrorWithFile) {
  const std::filesystem::path unreadable{"/proc/self/mem"};
  const auto result = mf::io::read_text_file(unreadable);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::IO);
  EXPECT_EQ(result.error().where.file, unreadable.string());
}

TEST_F(read_text_file, ReadTextFile_ExistingFile_ReturnsBytesExactly) {
  const std::string content("a\r\nb\0c", 6);
  write_binary(content);
  const auto result = mf::io::read_text_file(path_);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), content);
  EXPECT_EQ(result.value().size(), 6U);
}

TEST_F(read_text_file, ReadTextFile_FileLargerThanOneChunk_ReturnsAllBytes) {
  constexpr std::size_t large_size = 10000;
  std::string content(large_size, 'x');
  content[0] = 'a';
  content[4095] = 'b';
  content[4096] = 'c';
  content[large_size - 1] = 'z';
  write_binary(content);
  const auto result = mf::io::read_text_file(path_);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), content);
}

TEST_F(read_text_file, ReadTextFile_EmptyFile_ReturnsEmptyString) {
  write_binary("");
  const auto result = mf::io::read_text_file(path_);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.value().empty());
}

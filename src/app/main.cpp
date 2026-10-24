#include <cstddef>
#include <iostream>
#include <magic_formula/cli/cli.hpp>
#include <span>
#include <string_view>
#include <vector>

// NOLINTNEXTLINE(bugprone-exception-escape): only an allocation failure can escape, which terminates by design
int main(int argc, char **argv) {
  const std::span<char *> raw(argv, static_cast<std::size_t>(argc));
  const std::vector<std::string_view> args(raw.begin(), raw.end());
  return mf::cli::run(args, std::cout, std::cerr);
}

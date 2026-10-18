---
name: build-and-test
description: Canonical commands to install dependencies, configure, build, run tests (all / by label / by regex), run static checks and benchmarks for magic-formula, plus the exact failure-report template test-runner must return.
when_to_use: Any time code must be built, tested, format/tidy-checked or benchmarked, and whenever a test-runner report is written or read.
allowed-tools: Bash(conan *), Bash(cmake *), Bash(ctest *), Bash(clang-format *), Bash(clang-tidy *), Bash(git ls-files *), Bash(xargs -r clang-format *), Bash(xargs -r clang-tidy *), Bash(./build/release/bench/mf_bench *)
user-invocable: true
argument-hint: "[debug|release|coverage] [unit|module|-R <regex>]"
---

# Build and test

All commands run from the repository root. Presets are defined in `CMakePresets.json`: `debug`, `release`, `coverage` (configure, build and test presets share the name).

## Toolchain prerequisites

| Tool | Minimum | Check |
|------|---------|-------|
| CMake | 4.0 | `cmake --version` |
| Ninja | 1.10 | `ninja --version` |
| Conan | >= 2.9 | `conan --version` |
| Compiler (C++26) | GCC 14 or Clang 19 | `g++ --version` / `clang++ --version` |
| gcovr (coverage only) | 8.x | `gcovr --version` |
| clang-format / clang-tidy | 19 (as CI; 18 may differ) | `clang-format --version` |

The lab templates' `find_program` lists stop at clang-format-17 / clang-tidy-18, so pass `-DCLANG_FORMAT_EXE=<exe>` / `-DCLANG_TIDY_EXE=<exe>` at configure time for versioned binaries (e.g. `clang-format-19`). Clang coverage builds also need `libclang-rt-<N>-dev`.

A missing or too-old tool is reported as `BLOCKED: <tool> <found version> < <required>`; do not try to install or work around it.

First-time Conan setup (once per machine): `conan profile detect --force`.

## Dependencies (Conan)

```bash
# Debug (used by the debug and coverage presets)
conan install . --build=missing -s build_type=Debug -s compiler.cppstd=26
# Release (used by the release preset and benchmarks)
conan install . --build=missing -s build_type=Release -s compiler.cppstd=26
```

## Configure and build

```bash
cmake --preset debug
cmake --build --preset debug
```

Replace `debug` with `release` or `coverage` as needed. Build errors are a FAIL; report the first compiler error per translation unit (file:line + message), not the whole log.

## Tests (CTest)

```bash
ctest --preset debug --output-on-failure            # everything
ctest --preset debug --output-on-failure --no-tests=error -L unit    # unit tests only   (tests/unit/)
ctest --preset debug --output-on-failure --no-tests=error -L module  # module tests only (tests/module/)
ctest --preset debug --output-on-failure --no-tests=error -R '<regex>'   # by test name, e.g. -R 'Pac2002.*'
ctest --preset debug --rerun-failed --output-on-failure # re-run last failures
```

Rules: never pass `--gtest_filter` to hide failing tests, never set `GTEST_SKIP`/`DISABLED_` to make a run pass. A skipped or disabled test counts as a FAIL in the report. Every label (`-L`) or regex (`-R`) run passes `--no-tests=error`. A run that selects **0 tests** (for example a regex that matches nothing, or a test file that was never registered) is a FAIL, never a vacuous PASS.

## Static checks (read-only)

```bash
# File lists include tracked AND new untracked (not ignored) files: workflow changes are uncommitted.
git ls-files --cached --others --exclude-standard -- '*.hpp' '*.cpp' | wc -l        # must be > 0
git ls-files --cached --others --exclude-standard -- '*.hpp' '*.cpp' | xargs -r clang-format --dry-run --Werror
git ls-files --cached --others --exclude-standard -- 'src/*.cpp' | wc -l            # must be > 0 (the pathspec * also matches /, so src/app/main.cpp is included)
git ls-files --cached --others --exclude-standard -- 'src/*.cpp' | xargs -r clang-tidy -p build/debug --quiet
```

CMake targets from the lab templates (configured in the root `CMakeLists.txt`):

- `clang-tidy_all` runs clang-tidy on `mflib` and `mf`. Configure with `-DCLANG_TIDY_EXE="clang-tidy;--warnings-as-errors=*"` to fail on warnings. It checks `.cpp` files only (its `-header-filter` never matches absolute header paths), so the `xargs` command above (which uses the `.clang-tidy` header filter) stays the header check.
- `clang-format_all` formats files IN PLACE. It is for developers only, never for test-runner; test-runner uses the `--dry-run --Werror` command above.

- `xargs -r` (GNU and BSD) skips the command when the list is empty, so nothing runs on empty stdin.
- An **empty file list is never a PASS**. Report it under `### Static checks` as `- clang-format: no files matched (not run)` (or `clang-tidy: …`). A requested check that matched no files makes the status `FAIL`, unless the caller stated that no C++ sources exist yet.
- `clang-tidy` needs `build/debug/compile_commands.json` (produced by `cmake --preset debug`). Never run `clang-format -i` or `clang-tidy --fix` as test-runner; formatting fixes are a developer task.

## Packet-scoped runs (parallel waves)

When several developers work in the same wave of the `feature` workflow, they share one working tree. A shared `build/debug` directory would be configured and built by several runs at once, which races. For a packet-scoped run, use a per-packet binary directory and a test scope limited to the packet:

```bash
# after the Debug conan install above
cmake --preset debug -B build/pkt-<packet-id>
cmake --build build/pkt-<packet-id> --target <packet test targets>   # all targets if none are named
ctest --test-dir build/pkt-<packet-id> --output-on-failure --no-tests=error -R '<packet test regex>'
```

- Only the packet's own targets and tests are in scope. Compile errors in files that belong to other packets of the same wave are reported under `### Build errors` with the note `(outside packet scope)`; they do not count as this packet's failures.
- The full-suite run on `build/debug` (and the coverage gate on `build/coverage`) happens once, in the Verify phase, after all waves have finished.

## Benchmarks

```bash
conan install . --build=missing -s build_type=Release -s compiler.cppstd=26
cmake --preset release && cmake --build --preset release
./build/release/bench/mf_bench --benchmark_format=console
./build/release/bench/mf_bench --benchmark_filter='<regex>'
```

Benchmarks are informational unless the caller gives a threshold; report name, time/iteration and items/s only.

## test-runner report template (exact)

Return exactly this markdown. Omit sections that have nothing to report. Never list passing tests, never paste full logs.

```markdown
## Test report: PASS | FAIL | BLOCKED
**Preset:** debug | release | coverage | build/pkt-<id> | **Scope:** all | unit | module | -R '<regex>'
**Counts:** <passed> passed, <failed> failed, <skipped> skipped / <total> total
**Commands:** `<command 1>`; `<command 2>`

### Build errors
- `src/foo.cpp:42:17` error: <first line of compiler message, verbatim>

### Failed tests
1. `Suite.Test_Name` (tests/unit/foo_test.cpp:88)
   - Expected: `<verbatim, trimmed>`
   - Actual:   `<verbatim, trimmed>`
   - Message:  `<extra gtest output, max 3 lines>`

### Static checks
- clang-format: `<file>` (N lines differ)
- clang-tidy: `<file>:<line>` [<check-name>] <message>

### Coverage (see coverage skill)
- Line: <x>% | Branch: <y>% (target 100/100)
- `src/foo.cpp`: lines 12-15, 40; branches at 51, 77

### Blocked
- <tool / environment problem, verbatim error>
```

Status rules: `PASS` only when build succeeds (errors marked `(outside packet scope)` excepted), at least one test was selected (0 selected = `FAIL`, reported under `### Failed tests` as `- no tests matched <scope> (0/0)`, unless the caller stated that no tests exist yet), every selected test passes, zero skipped/disabled, and every requested check/gate ran on a non-empty file list and passed. `BLOCKED` when the run could not happen (missing tool, Conan failure). Otherwise `FAIL`.

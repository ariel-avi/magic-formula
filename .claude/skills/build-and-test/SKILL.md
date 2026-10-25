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
| Compiler (C++26) | GCC 14 or Clang 19 (Linux); MSVC (C++23 fallback) on Windows; Homebrew LLVM 21 on macOS | `g++ --version` / `clang++ --version` |
| gcovr (coverage only) | 8.x | `gcovr --version` |
| clang-format / clang-tidy | 19 (as CI; 18 may differ) | `clang-format --version` |

The lab templates' `find_program` lists stop at clang-format-17 / clang-tidy-18, so pass `-DCLANG_FORMAT_EXE=<exe>` / `-DCLANG_TIDY_EXE=<exe>` at configure time for versioned binaries (e.g. `clang-format-19`). Clang coverage builds also need `libclang-rt-<N>-dev`.

A missing or too-old tool is reported as `BLOCKED: <tool> <found version> < <required>`; do not try to install or work around it.

First-time Conan setup (once per machine): `conan profile detect --force`.

## Dependencies (Conan)

```bash
# Debug (used by the debug and coverage presets)
conan install . --build=missing -s build_type=Debug -s compiler.cppstd=26
# Release (used by the release preset; Google Benchmark is a default requirement for Release only; -o with_benchmark=True|False overrides)
conan install . --build=missing -s build_type=Release -s compiler.cppstd=26
```

## Windows and macOS

The commands above are the Linux contract; the CI `build-test` matrix (`.github/workflows/ci.yml`) uses these variants. test-runner uses them only when a caller asks for a non-Linux run (the sandbox is Linux only).

- **Windows (MSVC, Developer PowerShell or `ilammy/msvc-dev-cmd`):** `conan install . --build=missing -s build_type=Debug -s compiler.cppstd=23`. Conan's msvc settings know cppstd up to 23 only; CMake has no C++26 flag for MSVC, so `CMakeLists.txt` falls back to 23. `conanfile.py` pins the Ninja generator (needs the MSVC environment on PATH). Then `cmake --preset debug`, `cmake --build --preset debug`, `ctest --preset debug` as usual. Multi-command PowerShell steps only report the last exit code: run each `ctest` as its own step.
- **macOS (Homebrew LLVM 21, not AppleClang):** `CC=/opt/homebrew/opt/llvm@21/bin/clang`, `CXX=.../clang++`, `SDKROOT=$(xcrun --show-sdk-path)`, and pass `-c "tools.build:exelinkflags=<L>" -c "tools.build:sharedlinkflags=<L>"` to `conan install`, with `<L>` = `['-L/opt/homebrew/opt/llvm@21/lib/c++','-L/opt/homebrew/opt/llvm@21/lib/unwind','-lunwind','-Wl,-rpath,/opt/homebrew/opt/llvm@21/lib/c++']`, with `-s compiler.cppstd=26`. Also pass `-c "tools.build:defines=['_LIBCPP_DISABLE_AVAILABILITY']"`: Homebrew's libc++ is built with vendor availability annotations, so libc++ 21 otherwise flags `std::from_chars(double)` as unavailable for the macOS deployment target even though the newer Homebrew `libc++.dylib` is linked.
- The coverage preset and the format/tidy gates are Linux-only. Presets need no per-OS variants.

## Configure and build

```bash
cmake --preset debug
cmake --build --preset debug
```

Replace `debug` with `release` or `coverage` as needed. Build errors are a FAIL; report the first compiler error per translation unit (file:line + message), not the whole log.

`mf_configure_target` (`cmake/MfCompileOptions.cmake`) defines `EIGEN_RUNTIME_NO_MALLOC` on every target in every configuration without `NDEBUG` (the `debug` and `coverage` presets, and an empty `CMAKE_BUILD_TYPE`), never in Release, RelWithDebInfo or MinSizeRel, so tests can assert that code does not use Eigen's heap.

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

- `clang-tidy_all` runs clang-tidy on `mflib` and `mf`. Configure with `-DCLANG_TIDY_EXE="clang-tidy;--warnings-as-errors=*"` to fail on warnings. Project headers are checked too: the target passes no `-header-filter`, so header selection comes from `HeaderFilterRegex` in `.clang-tidy`. The `xargs` command above is the equivalent read-only check.
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

`bench/` builds one executable, `mf_bench` (Google Benchmark, linked with `benchmark_main`), when `MF_BUILD_BENCH` is ON. The `release` preset sets it; `debug` and `coverage` leave it OFF (any preset can opt in with `-DMF_BUILD_BENCH=ON`). `mf_bench` is not a CTest test, is not part of the coverage gate and is not checked by `clang-tidy_all`; `clang-format_all` does format `bench/`.

```bash
conan install . --build=missing -s build_type=Release -s compiler.cppstd=26
cmake --preset release && cmake --build --preset release --target mf_bench
./build/release/bench/mf_bench --benchmark_format=console
./build/release/bench/mf_bench --benchmark_filter='<regex>'
./build/release/bench/mf_bench --benchmark_list_tests=true          # names only
./build/release/bench/mf_bench --benchmark_min_time=0.01s           # quick smoke run (Google Benchmark >= 1.8 needs the "s" suffix)
```

Benchmarks are informational unless the caller gives a threshold; report name, time/iteration and items/s only. A benchmark that prints `ERROR OCCURRED` (SkipWithError) or a non-zero exit is a FAIL.

## WebAssembly module and web app

Prerequisites: git, python3 and network access once (for the project-local emsdk 5.0.7 in `.emsdk/`; nothing system-wide), Node 24 and npm. The bash scripts are macOS/Linux only in the POC (Windows is a follow-up).

```bash
bash wasm/setup-emsdk.sh          # once; idempotent
bash wasm/build.sh                # conan install (conan/profiles/wasm) + cmake --preset wasm + cmake --build --preset wasm
cd web && npm ci
npm run typecheck && npm run test:unit && npm run test:coverage && npm run test:smoke && npm run build
npm run test:e2e                  # Playwright UI tests (Chrome; PLAYWRIGHT_CHANNEL=chromium fallback)
npm run dev                       # http://localhost:5173
```

- The `wasm` preset builds only the target `mf_wasm` and writes `web/src/wasm/mf_wasm.js` (gitignored). It has no test preset; the native `debug`/`release`/`coverage` presets are unaffected.
- `npm test` = unit + smoke. The smoke test needs the built module; the `pretest` hooks fail with "run npm run wasm:build" when it is missing.
- `npm run test:coverage` enforces 100% thresholds on `web/src/domain`.
- Localhost check: start `npm run dev` in the background, then `curl -sf http://localhost:5173/` must return 200; stop the server afterwards.
- `clang-format` covers `wasm/*.cpp`; `clang-tidy` does not (it needs the Emscripten headers).
- `npm run test:e2e` runs `web/e2e/*.spec.ts` against the Vite dev server that `playwright.config.ts` starts (`webServer`, localhost:5173, reused locally if already running). The browser is the installed Google Chrome (channel `chrome`). If it cannot launch: `npx playwright install chromium`, then `PLAYWRIGHT_CHANNEL=chromium npm run test:e2e`. The `pretest:e2e` hook checks the wasm module. `forbidOnly` is on and retries are 0.
- Playwright artifacts (all gitignored): traces in `web/test-results/artifacts/`, screenshots in `web/test-results/screenshots/`, HTML report in `web/playwright-report/`.
- Raw-element check (shadcn components only outside `components/ui/`): `grep -rnE "<(input|select|button|table|textarea)[ >]" web/src --include=*.tsx | grep -v src/components/ui/` must print nothing.
- Playwright results go into the report as passed/failed counts per spec file under `### Failed tests`, naming failing tests only (same style as the Vitest line below).
- Vitest results go into the report template below as counts of passed/failed tests per Vitest project (`unit`, `smoke`) under `### Failed tests`, naming failing tests only.

## Python bindings

`python/` builds the pybind11 extension `magic_formula._core` through scikit-build-core, using the Conan Release toolchain. All commands run from the repository root.

```bash
conan install . --build=missing -s build_type=Release -s compiler.cppstd=26 -o with_benchmark=False
python3.12 -m venv .venv                      # or python3.13
# Linux
CC=gcc-14 CXX=g++-14 .venv/bin/python -m pip install "./python[test]"
# macOS: Homebrew LLVM 21 (the conan install above also needs the macOS flags from "Windows and macOS")
CC=/opt/homebrew/opt/llvm@21/bin/clang CXX=/opt/homebrew/opt/llvm@21/bin/clang++ SDKROOT=$(xcrun --show-sdk-path) \
  .venv/bin/python -m pip install "./python[test]"
.venv/bin/python -m pytest python/tests       # includes the notebook; the 100% pytest-cov gate comes from pyproject.toml
```

- The install is non-editable: reinstall (`pip install "./python[test]"`) after any change under `python/` before running pytest.
- If a rebuild fails on a stale CMake cache, `rm -rf python/build` and reinstall.
- `python/bindings/*.cpp` is covered by `clang-format_all`; a dry run is `clang-format --dry-run --Werror python/bindings/*.cpp`.
- The report template below applies with Scope `pytest`.
- The `feature` workflow's Verify phase runs only ctest and gcovr, so the Python run must be requested explicitly.

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

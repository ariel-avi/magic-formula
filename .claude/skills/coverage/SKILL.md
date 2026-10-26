---
name: coverage
description: How to build with the coverage preset, run the 100% line + branch gate on core sources (src/, include/) with gcovr, report uncovered lines as file:line ranges, and the coverage-exclusion policy.
when_to_use: Running or interpreting the coverage gate; reviewing whether new code is fully covered; deciding whether a coverage exclusion is acceptable.
allowed-tools: Bash(conan *), Bash(cmake *), Bash(ctest *), Bash(gcovr *)
user-invocable: true
---

# Coverage

Target: **100% line AND 100% branch coverage** on core sources: the library under `src/` and the public headers under `include/`. `src/app/` is inside `src/`, so it is gated and is covered by the CLI module test.

`src/cli/` is compiled into `mflib` and covered in-process by tests (`tests/unit/cli_options_test.cpp`, `tests/module/cli_test.cpp`); `src/app/main.cpp` is covered by the CTest process tests in `tests/module/CMakeLists.txt` (`mf_cli_*`), which the coverage target runs. `tests/`, `bench/` and third-party code are excluded from the gate (still built and tested). The gate runs in CI and blocks merge.

## Run the gate

```bash
conan install . --build=missing -s build_type=Debug -s compiler.cppstd=26
cmake --preset coverage
cmake --build --preset coverage
cmake --build --preset coverage --target coverage
```

The `coverage` target (from `cmake/Coverage.cmake`, configured in the root `CMakeLists.txt`) deletes stale `.gcda` files, runs ctest, then runs gcovr from the repository root on `build/coverage` with the `src/` and `include/` filters, `--exclude-unreachable-branches --exclude-throw-branches` and `--fail-under-line 100 --fail-under-branch 100`. It writes `coverage/summary.txt` (line metric), `coverage/index.html` and `coverage/coverage.xml`.

- Exit code non-zero = gate FAIL (a test failed or a threshold was missed).
- The gcov tool is matched to the compiler automatically (`gcov-<major>` for GCC, `llvm-cov-<major> gcov` for Clang). Override with `-DGCOV_EXE=...`, `-DLLVM_COV_EXE=...` or `-DGCOVR_EXE=...` at configure time. The `coverage` preset defines the instrumentation flags (`--coverage` / `-O0 -g`); do not add flags on the command line. Clang coverage builds also need the profile runtime (`libclang-rt-<N>-dev` on Ubuntu); without it the coverage preset fails at link.
- Filters and thresholds live in the root `CMakeLists.txt` `add_coverage_target` call.
- `--exclude-unreachable-branches` drops compiler-generated branches on lines that contain no source code (e.g. closing braces); it does no reachability analysis. Exception-unwinding edges are dropped by `--exclude-throw-branches`. Neither exempts any source branch you wrote; every `if`, `?:`, `&&`, `||`, `switch` case and `std::expected` error path in core must be exercised by a test.

To list uncovered branches without re-running the tests (from the repository root, after a coverage run):

```bash
gcovr --root . --object-directory build/coverage --gcov-executable gcov-14 \
      --filter 'src/' --filter 'include/' \
      --exclude-unreachable-branches --exclude-throw-branches \
      --txt-metric branch --txt build/coverage/branches.txt build/coverage
```

For Clang use `--gcov-executable "llvm-cov-19 gcov"`.

The authoritative gate is gcc-14 (CI). `llvm-cov gcov` (Clang, including Homebrew LLVM 21 on macOS) counts compiler-generated branches that gcc does not, so it reports far lower branch coverage (about 57%, even for trivial files) with lines at 100%; that is toolchain accounting, not missing tests. Run the gate on macOS with Homebrew GCC: `conan install` with `CC=gcc-14 CXX=g++-14`, then `cmake --preset coverage -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14 -B build/cov-gcc` and the matching build and `coverage` target (gcov-14 is picked up automatically).

Use the gcovr version listed in the build-and-test prerequisites. Do not edit thresholds or filters to make a gate pass.

## Reporting uncovered code

Read the `Missing` column of the text reports: `coverage/summary.txt` (lines) and the branch listing above (`build/coverage/branches.txt`). Report ONLY uncovered locations, grouped per file, as compact ranges:

```markdown
### Coverage
- Line: 98.7% | Branch: 96.2% (target 100/100) -> FAIL
- `src/pac2002.cpp`: lines 120-124, 188; branches at 131, 140
- `include/magic_formula/tir_parser.hpp`: line 57
```

No per-file tables for fully covered files, no HTML dumps.

`wasm/` and `web/` lie outside the gcovr gate by layout (not under `src/` or `include/`, not compiled natively), with no exclusion markers; `web/src/domain` has its own 100% Vitest thresholds (a local gate, not CI in the POC).

## Python bindings

`python/` (the pybind11 adapter and the Python layer) is outside the gcovr gate by layout, like `bench/` and `tests/`; this is not an exclusion. The Python package has its own gate: 100% line and branch pytest-cov coverage on `magic_formula`, set in the `python/pyproject.toml` pytest `addopts` (`pytest python/tests` fails below it). The adapter's branches are covered by the pytest suite; the reviewer checks a branch-to-test table for them.

## Exclusion policy

- Exclusions are **forbidden in core** (`src/`, `include/`) by default.
- The only permitted mechanism is a gcovr marker (`// GCOVR_EXCL_LINE`, or a `// GCOVR_EXCL_START` / `// GCOVR_EXCL_STOP` pair) with a justification comment on the same or preceding line: `// GCOVR_EXCL_LINE: <why this cannot be reached by any test>`.
- Every exclusion must be approved by `reviewer` in the PR review and listed in the PR body under "Tests & coverage". Unapproved exclusions are a blocker.
- Not valid justifications: "hard to test", "defensive check", "trivial getter", "logging". Refactor (e.g. inject the dependency, split the function) so it can be tested.
- Changing `--filter`, thresholds, or the preset's flags is an architecture decision (architect + orchestrator), never a developer fix.

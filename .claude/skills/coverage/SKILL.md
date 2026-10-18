---
name: coverage
description: How to build with the coverage preset, run the 100% line + branch gate on core sources (src/, include/) with gcovr, report uncovered lines as file:line ranges, and the coverage-exclusion policy.
when_to_use: Running or interpreting the coverage gate; reviewing whether new code is fully covered; deciding whether a coverage exclusion is acceptable.
allowed-tools: Bash(conan *), Bash(cmake *), Bash(ctest *), Bash(gcovr *)
user-invocable: true
---

# Coverage

Target: **100% line AND 100% branch coverage** on core sources: the library under `src/` and the public headers under `include/`. `app/`, `tests/`, `bench/` and third-party code are excluded from the gate (still built and tested). The gate runs in CI and blocks merge.

## Run the gate

```bash
conan install . --build=missing -s build_type=Debug -s compiler.cppstd=26
cmake --preset coverage
cmake --build --preset coverage
ctest --preset coverage --output-on-failure
mkdir -p coverage
gcovr --root . --object-directory build/coverage \
      --filter 'src/' --filter 'include/' \
      --exclude-unreachable-branches --exclude-throw-branches \
      --fail-under-line 100 --fail-under-branch 100 \
      --txt coverage/summary.txt --txt-metric branch \
      --html-details coverage/index.html \
      --xml coverage/coverage.xml --print-summary
```

- Exit code non-zero = gate FAIL (either threshold missed or tests failed).
- GCC: if the default `gcov` does not match the compiler, add `--gcov-executable gcov-14` (match the GCC major version).
- Clang: add `--gcov-executable "llvm-cov gcov"`. The `coverage` preset defines the instrumentation flags (`--coverage` / `-O0 -g`); do not add flags on the command line.
- `--exclude-unreachable-branches` drops compiler-generated branches on lines that contain no source code (e.g. closing braces); it does no reachability analysis. Exception-unwinding edges are dropped by `--exclude-throw-branches`. Neither exempts any source branch you wrote; every `if`, `?:`, `&&`, `||`, `switch` case and `std::expected` error path in core must be exercised by a test.

Use the gcovr version listed in the build-and-test prerequisites. Do not edit thresholds or filters to make a gate pass.

## Reporting uncovered code

Read the `Missing` column of the text report (`coverage/summary.txt`, plus a line-metric run if needed: rerun with `--txt-metric line`). Report ONLY uncovered locations, grouped per file, as compact ranges:

```markdown
### Coverage
- Line: 98.7% | Branch: 96.2% (target 100/100) -> FAIL
- `src/pac2002.cpp`: lines 120-124, 188; branches at 131, 140
- `include/magic_formula/tir_parser.hpp`: line 57
```

No per-file tables for fully covered files, no HTML dumps.

## Exclusion policy

- Exclusions are **forbidden in core** (`src/`, `include/`) by default.
- The only permitted mechanism is a gcovr marker (`// GCOVR_EXCL_LINE`, or a `// GCOVR_EXCL_START` / `// GCOVR_EXCL_STOP` pair) with a justification comment on the same or preceding line: `// GCOVR_EXCL_LINE: <why this cannot be reached by any test>`.
- Every exclusion must be approved by `reviewer` in the PR review and listed in the PR body under "Tests & coverage". Unapproved exclusions are a blocker.
- Not valid justifications: "hard to test", "defensive check", "trivial getter", "logging". Refactor (e.g. inject the dependency, split the function) so it can be tested.
- Changing `--filter`, thresholds, or the preset's flags is an architecture decision (architect + orchestrator), never a developer fix.

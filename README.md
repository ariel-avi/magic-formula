# magic-formula

A C++26 library and command-line tool for **Pacejka Magic Formula tire models**.

- An abstract base tire-model formulation, with concrete models **PAC2002** and **MF 5.2**. It is designed to be extended (e.g. MF 6.x).
- Batch evaluation with Eigen.
- A CLI that reads a simulation input table (Fz, slip angle, slip ratio, inclination, Vx, …) and a `.tir` tire property file, then prints the tire forces and moments (Fx, Fy, Mz, Mx, My, …) for every input row.
- Verified by unit and module tests, with a 100% line and branch coverage gate on the core library.

## Requirements

CMake >= 4.0, Ninja, Conan 2 (>= 2.9), and GCC >= 14 or Clang >= 19 (C++26). Conan provides GoogleTest; Eigen and Google Benchmark are added with the first code that uses them. Coverage needs gcovr 8.x; GCC needs `gcov-<N>`, Clang needs `llvm-cov` and `libclang-rt-<N>-dev`.

## Build and test

```bash
conan profile detect --force        # once per machine
conan install . --build=missing -s build_type=Debug -s compiler.cppstd=26
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
./build/debug/mf                    # prints "mf 0.1.0"
```

## Code quality and coverage

```bash
cmake --build --preset debug --target clang-format_all   # formats in place
cmake --build --preset debug --target clang-tidy_all
cmake --preset coverage && cmake --build --preset coverage && cmake --build --preset coverage --target coverage
```

The coverage target runs the tests and gcovr with a 100% line and branch gate on `src/` and `include/`; reports are written to `coverage/`.

CI (`.github/workflows/ci.yml`) runs the format/tidy gates, builds and tests with GCC 14 and Clang 19, and runs the coverage gate with GCC 14.

## Usage

The CLI currently prints its version (`mf 0.1.0`). The two-file interface comes with the first model feature:

```bash
mf <simulation-inputs-file> <tire-property-file.tir>
```

See `CLAUDE.md` and `docs/` for the architecture once they are in place.

## Development with Claude Code agents

Development in this repository is driven by a team of Claude Code agents (`.claude/agents/`). Skills (`.claude/skills/`) hold the shared knowledge, and saved workflows (`.claude/workflows/`: `feature`, `review`) chain the agents together. The main session runs as the `orchestrator`. It dispatches architect -> planner -> developers -> test-runner -> reviewer and checks the result against the request. All changes land through pull requests; nothing is committed to `master` directly.

See [`CLAUDE.md`](CLAUDE.md) for the agent roster, the workflow, the commands and the project rules.

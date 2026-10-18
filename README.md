# magic-formula

A C++26 library and command-line tool for **Pacejka Magic Formula tire models**.

- An abstract base tire-model formulation, with concrete models **PAC2002** and **MF 5.2**. It is designed to be extended (e.g. MF 6.x).
- Batch evaluation with Eigen.
- A CLI that reads a simulation input table (Fz, slip angle, slip ratio, inclination, Vx, …) and a `.tir` tire property file, then prints the tire forces and moments (Fx, Fy, Mz, Mx, My, …) for every input row.
- Verified by unit and module tests, with a 100% line and branch coverage gate on the core library.

## Requirements

CMake >= 4.0, Conan 2, and GCC >= 14 or Clang >= 19 (C++26). Conan provides Eigen, GoogleTest and Google Benchmark.

## Build and test

```bash
conan install . --build=missing -s build_type=Debug -s compiler.cppstd=26
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

## Usage

```bash
<cli> <simulation-inputs-file> <tire-property-file.tir>
```

The executable name and the input table format are defined by the architecture. See `CLAUDE.md` and `docs/` once they are in place.

## Development with Claude Code agents

Development in this repository is driven by a team of Claude Code agents (`.claude/agents/`). Skills (`.claude/skills/`) hold the shared knowledge, and saved workflows (`.claude/workflows/`: `feature`, `review`) chain the agents together. The main session runs as the `orchestrator`. It dispatches architect -> planner -> developers -> test-runner -> reviewer and checks the result against the request. All changes land through pull requests; nothing is committed to `master` directly.

See [`CLAUDE.md`](CLAUDE.md) for the agent roster, the workflow, the commands and the project rules.

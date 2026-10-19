# magic-formula

A C++26 library and command-line tool for **Pacejka Magic Formula tire models**.

- An abstract base tire-model formulation, with concrete models **PAC2002** and **MF 5.2**. It is designed to be extended (e.g. MF 6.x).
- Batch evaluation with Eigen.
- A CLI that reads a simulation input table (Fz, slip angle, slip ratio, inclination, Vx, …) and a `.tir` tire property file, then prints the tire forces and moments (Fx, Fy, Mz, Mx, My, …) for every input row.
- Verified by unit and module tests, with a 100% line and branch coverage gate on the core library.

## Requirements

CMake >= 4.0, Ninja, Conan 2 (>= 2.9), and GCC >= 14 or Clang >= 19 (C++26). Windows (MSVC 19.4x, built as C++23) and macOS (Homebrew LLVM 21) are supported too, see "Platform support" below. Conan provides GoogleTest; Eigen and Google Benchmark are added with the first code that uses them. Coverage needs gcovr 8.x; GCC needs `gcov-<N>`, Clang needs `llvm-cov` and `libclang-rt-<N>-dev`.

## Build and test

```bash
conan profile detect --force        # once per machine
conan install . --build=missing -s build_type=Debug -s compiler.cppstd=26
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
./build/debug/mf --version          # prints "mf 0.1.0"
```

## Code quality and coverage

```bash
cmake --build --preset debug --target clang-format_all   # formats in place
cmake --build --preset debug --target clang-tidy_all
cmake --preset coverage && cmake --build --preset coverage && cmake --build --preset coverage --target coverage
```

The coverage target runs the tests and gcovr with a 100% line and branch gate on `src/` and `include/`; reports are written to `coverage/`.

CI (`.github/workflows/ci.yml`) runs the format/tidy gates, builds and tests on Linux (GCC 14, Clang 19), Windows (MSVC, `windows-2025`) and macOS (`macos-15`, Homebrew LLVM 21), and runs the coverage gate with GCC 14 on Linux.

## Platform support

| Platform | Compiler | C++ standard | Notes |
|----------|----------|--------------|-------|
| Linux | GCC >= 14, Clang >= 19 | C++26 | the reference platform; the coverage gate runs here |
| Windows | MSVC (Visual Studio 2022, v143) | C++23 (`/std:c++latest`) | Conan's msvc settings stop at `cppstd=23` and CMake has no C++26 flag for MSVC; the root `CMakeLists.txt` falls back to 23 automatically. Configure from an MSVC developer prompt (Ninja generator). |
| macOS | Homebrew LLVM 21 (`brew install llvm@21`) | C++26 | AppleClang is not supported: its libc++ lacks floating-point `std::from_chars` and `std::views::join_with`. Link against Homebrew's libc++ (flags below). |

Windows (Developer PowerShell for VS 2022):

```powershell
conan profile detect --force
conan install . --build=missing -s build_type=Debug -s compiler.cppstd=23
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

macOS (Homebrew LLVM 21):

```bash
export CC=/opt/homebrew/opt/llvm@21/bin/clang CXX=/opt/homebrew/opt/llvm@21/bin/clang++
export SDKROOT=$(xcrun --show-sdk-path)
LINKFLAGS="['-L/opt/homebrew/opt/llvm@21/lib/c++','-L/opt/homebrew/opt/llvm@21/lib/unwind','-lunwind','-Wl,-rpath,/opt/homebrew/opt/llvm@21/lib/c++']"
conan profile detect --force
conan install . --build=missing -s build_type=Debug -s compiler.cppstd=26 \
  -c "tools.build:exelinkflags=${LINKFLAGS}" -c "tools.build:sharedlinkflags=${LINKFLAGS}" \
  -c "tools.build:defines=['_LIBCPP_DISABLE_AVAILABILITY']"
cmake --preset debug && cmake --build --preset debug && ctest --preset debug --output-on-failure
```

`_LIBCPP_DISABLE_AVAILABILITY` is needed because Homebrew builds libc++ with vendor availability annotations enabled: libc++ 21 would otherwise mark `std::from_chars(double)` unavailable for the macOS deployment target, even though the build links Homebrew's newer `libc++.dylib`.

`.gitattributes` forces LF line endings so test fixtures are byte-identical on every OS.

## CLI

```bash
mf [--uncombined] [--verbose] <inputs-table> <file.tir>
mf --help | -h
mf --version
```

`mf` evaluates the tire model described by `<file.tir>` at every row of `<inputs-table>` and prints Fx, Fy, Mz, Mx and My for every row as CSV on stdout.

| Option | Meaning |
|--------|---------|
| `--uncombined` | print the pure-slip outputs (Fx0, Fy0, Mz0) instead of the combined-slip ones |
| `--verbose` | print the model information and the ignored `.tir` keys on stderr |
| `-h`, `--help` | print the usage text on stdout and exit |
| `--version` | print the version on stdout and exit |

### Models

The model is chosen from the `.tir` file:

- **PAC2002**: `PROPERTY_FILE_FORMAT = 'PAC2002'`.
- **MF 5.2**: `FITTYP = 6`, with `PROPERTY_FILE_FORMAT` absent or one of `'MF_05'`, `'USER'` (TNO MF-Tyre ADAMS user-tire form) or `'MF-TYRE'`.

MF 6.x and every other model give an error (see `docs/research/model-routing.md`). The policy decisions behind the loader and the models (defaults, required keys, units, clamping, low speed, ...) are signed off in [`docs/research/decisions.md`](docs/research/decisions.md).

### Input table

Plain text, one operating point per row:

- A line whose first non-blank character is `#` is a comment; blank lines are ignored.
- Fields are separated by any run of commas, blanks or tabs.
- The first content line is the header. The required columns are `FZ SA SR IA VX`; the optional column is `P` (pressure). Columns may be in any order and in any letter case.
- A column name may carry a unit in brackets; without a unit the SI unit applies: `FZ[N]`, `SA[rad|deg]`, `SR[-]`, `IA[rad|deg]`, `VX[m/s]`, `P[Pa]`.
- `P` is read but ignored with a warning, because PAC2002 and MF 5.2 have no pressure input.
- Every data row has exactly as many numbers as the header has columns.

Example:

```
# Fz[N]  SA[deg]  SR  IA[deg]  Vx[m/s]
FZ[N],SA[deg],SR[-],IA[deg],VX[m/s]
4000,0,0,0,16.7
4000,4,0,0,16.7
3000,2,-0.1,1,25
```

### Output

CSV on stdout. The header line is

```
FZ[N],SA[rad],SR[-],IA[rad],VX[m/s],FX[N],FY[N],MZ[N*m],MX[N*m],MY[N*m]
```

followed by one line per input row, in input order. The inputs are echoed unclamped in SI units (angles in rad), followed by the five outputs in the ISO W-axis sign convention (My is negative when the wheel rolls forward). Nothing is written to stdout unless every row evaluated successfully.

### Diagnostics and exit codes

Diagnostics go to stderr:

- `mf: error: <file>:<line>: <message>` for a failure (`<file>:<line>` is omitted when there is no location).
- `mf: warning: ...` for everything that does not stop the run: load warnings, the ignored `P` column, and one warning per variable when inputs were clamped to the `.tir` ranges or Vx is below `VXLOW` (it names the first table line and the number of rows).
- `mf: info: ...` lines with `--verbose`.

| Exit code | Meaning |
|-----------|---------|
| 0 | success (also `--help` and `--version`) |
| 1 | error: file, model, input table or evaluation problem |
| 2 | usage error (message and usage text on stderr) |

See [`docs/architecture.md`](docs/architecture.md) for the architecture (layers, class hierarchy, evaluation pipeline, error model, and how to extend to MF 6.x) and `CLAUDE.md` for the project layout.

## Development with Claude Code agents

Development in this repository is driven by a team of Claude Code agents (`.claude/agents/`). Skills (`.claude/skills/`) hold the shared knowledge, and saved workflows (`.claude/workflows/`: `feature`, `review`) chain the agents together. The main session runs as the `orchestrator`. It dispatches architect -> planner -> developers -> test-runner -> reviewer and checks the result against the request. All changes land through pull requests; nothing is committed to `master` directly.

See [`CLAUDE.md`](CLAUDE.md) for the agent roster, the workflow, the commands and the project rules.

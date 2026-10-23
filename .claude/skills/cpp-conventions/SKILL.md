---
name: cpp-conventions
description: The C++26 coding standard for magic-formula. Covers naming, namespaces, header layout, error handling (std::expected vs exceptions), ownership, const-correctness, units, Eigen usage rules, GoogleTest/benchmark conventions and clang-format/clang-tidy expectations.
when_to_use: Writing, planning, designing or reviewing any C++ in include/, src/ (incl. src/app/), tests/ or bench/.
user-invocable: true
---

# C++ conventions

Standard: C++26 (`CMAKE_CXX_STANDARD 26`, i.e. `-std=c++26` / `-std=c++2c`), no compiler extensions (`CMAKE_CXX_EXTENSIONS OFF`). Warnings: `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror` on project targets. Code must compile on both GCC >= 14 and Clang >= 19. Use a C++26 feature only if both support it; otherwise use the C++23 equivalent.

## Naming

Aligned with the lab `.clang-tidy` (`readability-identifier-naming`). Class/struct/union naming is a convention checked in review, because the lab config's `TypeCase` key is not a valid clang-tidy option (only `EnumCase`/`TypeAliasCase` are enforced for types); the other rows below are enforced by clang-tidy. A lab follow-up should add `ClassCase`/`StructCase`/`UnionCase`.

| Entity | Style | Example |
|--------|-------|---------|
| Types (class, struct, union, enum, type alias; concepts by convention) | lower_case | `tire_model`, `pac2002_model`, `tir_file`, `slip_inputs` |
| Functions, methods | lower_case | `evaluate_batch`, `vertical_load` |
| Functions that compute one Magic Formula quantity | its symbol in lower_case | `mu_y`, `fx0`, `fy0`, `gxa`, `gyk`, `svyk`, `mzr`, `alpha_eq`, `fy_prime`, `mx`, `my`, `t`, `s` |
| Variables, parameters (also const parameters), public members | lower_case | `fz_n`, `slip_angle_rad` |
| Private / protected members | lower_case + trailing `_` | `coefficients_` |
| Constants: global, namespace-scope, static and member `constexpr` / `const` variables, and enumerators | UPPER_CASE | `PI`, `DEFAULT_EPSILON`, `status::OK` |
| Function-local `constexpr` / `const` variables (`LocalConstantCase`) | lower_case | `const double dfz = ...;`, `constexpr double force_tol_n = 1e-6;` |
| Const references and pointers-to-const | lower_case | `const auto& row` |
| Template parameters | CamelCase + `_T` suffix | `Scalar_T`, `Model_T` |
| Namespaces | lower_case | `mf`, `mf::io`, `mf::detail` |
| Files | snake_case | `tire_model.hpp`, `pac2002_model.cpp`, `pac2002_model_test.cpp` |
| Macros (avoid) | `MF_UPPER_CASE` | `MF_EXPORT` |

Small enums need an explicit underlying type (`enum class status : std::uint8_t`) because of `performance-enum-size`.

Exception to `enum class`: the index enums of a model family are unscoped (`enum input_index : std::uint8_t { FZ, SA, ... }`), because an enumerator is a vector position and is used directly as an index without a cast. The family namespace `mf::<family>` (for example `mf::mf5x`) scopes them, the enumerator names must be unique within that namespace, and the declaration carries a `NOLINTBEGIN/END(cppcoreguidelines-use-enum-class)` with the reason.

A qualifier is added only to tell variants apart (`my_from_shifts`). A single-letter symbol is used as is (`t` for the pneumatic trail, `s` for the moment arm of Fx); locals and parameters in the same namespace then take descriptive names (`terms`, `state`). Functions that return several quantities or are generic helpers keep descriptive names (`vertical_load`, `aligning`, `trail_cosine`, `evaluate_slip_state`, `friction_ellipse_ratios`, `sgn_plus`).

No local variable or parameter reuses the name of a function visible in the same namespace or class (for example a parameter `mu_y` inside `mf::detail`). It reads ambiguously, and MSVC /W4 name-hiding warnings (C4456-C4459) may fire.

Magic Formula coefficients keep their literature names as struct members (`PCX1`, `PDY2`, `QBZ1`, `LMUX`) so they match the `.tir` keys one-to-one. This is the only allowed naming exception. Wrap the member block in a targeted suppression with a reason, and document each member with its meaning and units:

```cpp
// NOLINTBEGIN(readability-identifier-naming): MF coefficient names match the .tir keys
double PCX1;
double PDX1;
// NOLINTEND(readability-identifier-naming)
```

A per-line `// NOLINT(readability-identifier-naming): <reason>` is also allowed.

## Namespaces and layout

- Everything lives in `namespace mf { ... }`. Sub-namespaces by layer: `mf::io` (parsing / file formats), `mf::detail` (internal helpers, not part of the API). The CLI driver lives in `mf::cli`. The index map of a model family (index enums and channel tables) lives in its own layout namespace `mf::<family>`, for example `mf::mf5x`; a new family (MF 6.x) gets `mf::mf6x`.
- Public headers: `include/magic_formula/<name>.hpp`, included as `#include <magic_formula/<name>.hpp>`. They may live in sub-directories that mirror the sub-namespaces: `include/magic_formula/{io,cli,detail}/` (`mf::io`, `mf::cli`, `mf::detail`). `detail/` headers are internal (not API) but stay under `include/` so unit tests can include them. Private headers stay in `src/`. Sources mirror this in `src/{model,io,detail,cli}/`; `src/app/` holds only `main.cpp`.
- Every header starts with `#pragma once`. No include guards and no `using namespace` in headers.
- Include order: clang-format (lab config: LLVM categories, `IncludeBlocks: Regroup`) puts quoted includes first, then ALL `<...>` includes (std, third-party, `<magic_formula/...>`) in one alphabetically sorted block; never hand-order.
- One class per header/source pair unless the types are trivially related (e.g. a POD and its enum).
- Header order: includes, namespace, forward declarations, types, free functions. Keep implementations in `.cpp` unless they are templates or `constexpr`/`inline` on purpose.

## Error handling

- Recoverable errors (parse failures, missing/invalid `.tir` keys, malformed input rows, unsupported model version): return `std::expected<T, mf::error>`, where `mf::error` carries a code, a message and a source location (file, line, key). Check every result; never call `.value()` without checking first.
- Exceptions only at the application boundary `mf::cli::run` (`src/cli/cli.cpp`), which catches `std::exception`, converts errors into a non-zero exit code and a message on stderr. `src/app/main.cpp` is one call with no branches and no `try`. The library throws `mf::model_failure` only from the `evaluate*` functions of a model built with `model_options::throw_if_model_failure`, and otherwise nothing except `std::bad_alloc`. The free check functions (`validate_input(s)`, `check_output(s)`, `check_buffers`, `check_batch_buffers`) return `std::expected`. The evaluation path of a model without the flag performs no checks.
- No `assert` in `src/` or `include/` (its failure branch is never taken, so branch coverage stays below 100%). Invariants come from validated `create()` named constructors and total functions. Never use `[[assume]]` on data that comes from the user or a file.
- Numerical guards (division by near-zero, `Vx -> 0`, `Fz <= 0`) are explicit, named (file-scope `EPSILON_V`, `EPSILON_X`, ...; function-local ones are lower_case, e.g. `epsilon_v`) and tested on both branches.

## Coverage-proof idioms

The 100% line + branch gate (`coverage` skill) runs with `--exclude-unreachable-branches --exclude-throw-branches`. These rules keep every branch reachable by a test (verified with gcov 14):

1. No `assert` in `src/` or `include/`.
2. No `switch` over an `enum class` in core: gcov counts an implicit default edge. Use `if`/`else` chains or `constexpr` lookup tables.
3. Every operand of `&&` / `||` must be drivable both ways by some test. Otherwise use `std::ranges::all_of` / `any_of` with a lambda, or a loop over a table.
4. Put a branch in a reusable, directly tested helper when a call site cannot reach both outcomes.
5. Format optional parts without branches (verbose output, joins).
6. Defaulted special members must not be emitted unused: delete what is not needed, and make sure the rest is used.
7. Every ternary: both sides must be tested.

## Ownership, values, const

- No raw `new`/`delete`, no owning raw pointers. Use values first, then `std::unique_ptr` (factories return `std::unique_ptr<tire_model>`), and `std::shared_ptr` only with a documented reason.
- Prefer value semantics and the Rule of Zero. Polymorphic bases: `virtual ~Base() = default;`, delete copy/move or make them protected (no slicing).
- `const` everything that can be const: locals, parameters by `const&` for non-trivial types, member functions. `constexpr` for pure math helpers and constants.
- `[[nodiscard]]` on every function that returns a value the caller must use (all `std::expected` returns, factories, evaluators).
- `noexcept` on functions that cannot throw (pure math on scalars, getters).
- Use `std::span` for non-owning views, `std::string_view` for read-only strings (never keep one beyond the owner's lifetime), and `enum class` (except the family index enums, see Naming).
- `std::mdspan` is **conditional**: libstdc++ 14 (the GCC 14 minimum) does not ship `<mdspan>` (it arrived in GCC 15). By default use `Eigen::Map` or `std::span` plus explicit strides for multi-dimensional views. Use `std::mdspan` only where the architect asks for it, behind the feature-test macro (`#include <version>`, then `#if defined(__cpp_lib_mdspan)`), with a fallback that compiles on GCC 14, and only if CI builds both paths.
- Use `override` / `final` explicitly. No `virtual` without a reason.
- Undefined behaviour is never allowed: no signed overflow, no uninitialised reads, no out-of-bounds access, no type-punning through `reinterpret_cast`.

## Units

- Inside the library all quantities use SI units: N, m, rad, m/s, Pa, N·m. Angles are in **radians**.
- Unit conversion (e.g. degrees in the input table, units declared in `.tir` `[UNITS]`) happens once, at the io boundary, and is tested.
- Use strong types (`struct newton { double value; };` or a small units header) where the architect specifies them. Otherwise every quantity in a public signature documents its unit in the name or the doc comment (`slip_angle_rad`).

## Eigen

- Kernels are lane-templated and header-defined (`template <typename Lane_T>`), instantiated on `detail::point_lane` (one point, scalar) and `detail::block_lane` (a block of up to `BLOCK_ROWS` points, SIMD). Use `Eigen::Array` for element-wise math and `Eigen::Matrix` only for real linear algebra.
- No dynamic-size Eigen objects (heap allocation) in `compute`, `compute_batch` or the kernels: lanes have a fixed maximum size and raw buffers are viewed with `Eigen::Map`.
- **Never** bind an Eigen expression to `auto`; that keeps dangling temporaries / lazy expressions. Assign to a concrete type (`const Eigen::ArrayXd dfz = (fz - fz0) / fz0;`) or call `.eval()`.
- Watch aliasing: `a = a.transpose()`-style statements need `.eval()`. Matrix products assigned to an operand need `.noalias()` only when provably safe.
- Pass Eigen objects by `const Eigen::Ref<const Eigen::ArrayXd>&` or `const Eigen::ArrayXd&`; never by value. The documented exception: a writable `Eigen::Ref<...>` parameter is passed BY VALUE (the Eigen idiom; it lets an `Eigen::Map` temporary bind).
- Branch-free vector code: use `.select()`, `.min()`, `.max()`, `.cwiseMin()` instead of per-element `if`. Every select predicate is driven both ways by tests, and every form branch is tested on both lane types (branch coverage counts the predicate's outcomes via tests, not via gcov).
- The per-point path and the batch path must agree, and a test asserts it at a 1e-12 scaled relative tolerance (batch results differ from per-point results by a few ULP). Per-point results are bit-identical to the previous implementation on Linux x86-64, provided glibc libm runs its baseline (non-FMA) variants; ctest pins them with `GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX2,-FMA,-FMA4` (`tests/module/per_point_golden_test.cpp`); few-ULP differences are accepted on other platforms (Clang arm64 FMA contraction, other libm/CRT).
- Do not use Eigen's `unsupported/` modules without architect approval.

## Testing (GoogleTest)

- One test file per class/unit: `tests/unit/<unit>_test.cpp`. Module (integration) tests: `tests/module/<feature>_test.cpp`. Registration is automatic: the build skeleton globs `tests/unit/*_test.cpp` and `tests/module/*_test.cpp` and registers each through `mf_add_test` with CTest label `unit` / `module` (target name = file stem). Do not edit CMakeLists.txt to register a test while implementing a packet (`agent-workflow`, "Build-file ownership"); a file that does not follow the naming is never registered, and its packet-scoped run selects 0 tests, which is a FAIL.
- Name tests `TEST(class_name, Behaviour_Condition_Expectation)`, e.g. `TEST(pac2002_model, LateralForce_ZeroSlipAngle_EqualsVerticalShiftOnly)`. Use fixtures (`TEST_F`) for shared setup and parameterised tests (`TEST_P`) for tables of vectors.
- Floating-point checks: `EXPECT_NEAR(actual, expected, tol)` with a named, documented tolerance (function-local `constexpr double force_tol_n = 1e-6; // reference values printed with 9 significant digits`, or `FORCE_TOL_N` at file scope). No `EXPECT_EQ` on computed doubles, with one exception: identity and bit-equal vectors use `EXPECT_EQ(std::bit_cast<std::uint64_t>(a), std::bit_cast<std::uint64_t>(b))` with the vector id in a comment.
- Reference data (`.tir` files, input tables, expected outputs) lives in `tests/data/`. Every file records its origin (source, tool/version, or "hand-computed, see test X") in a header comment or a sibling `README.md`. Find data paths through a CMake-provided define (`MF_TEST_DATA_DIR`), never through relative cwd paths.
- Test helpers live header-only in `tests/support/` (`fixtures.hpp`, namespace `mf::test`) and are included as `"../support/fixtures.hpp"`. `tests/support/` is not globbed as a test.
- Test every error path of every `std::expected` function, and both sides of every numerical guard.
- Never disable or skip tests (`DISABLED_`, `GTEST_SKIP`) to get green; that is a blocker in review.
- Benchmarks (`bench/`): Google Benchmark, `BM_<Unit>_<Scenario>`, use `benchmark::DoNotOptimize` and `benchmark::ClobberMemory`, and parameterise the point/batch count with `->Range(...)`, or with an explicit `->Arg(...)` list when the sizes are not geometric. Pass only non-const lvalues to `DoNotOptimize`: its const-reference overload is deprecated, and `-Werror` rejects the warning. `mf_bench` links `benchmark_main` (no `BENCHMARK_MAIN()`), loads data through `MF_TEST_DATA_DIR`, is formatted by `clang-format_all` and is not checked by `clang-tidy_all`.

## Formatting and static analysis

- `.clang-format` is authoritative (`clang-format --dry-run --Werror` must be clean). Never hand-format against it.
- `.clang-tidy` is a verbatim copy of `ariel-avi/lab/cpp/.clang-tidy` (change the lab first) and is authoritative. Enabled families: `bugprone-*`, `performance-*`, `readability-*`, `modernize-*`, `clang-analyzer-*`, `cppcoreguidelines-*`. Key exclusions: magic numbers, identifier length, trailing return type and `modernize-use-nodiscard` are off. CI runs it with warnings as errors. A `// NOLINT(<check>)` must name the check and give a reason, and the reviewer approves it.
- clang-tidy 19 (CI major) notes:
  - `readability-math-missing-parentheses`: write `a + (b * c)`.
  - `modernize-use-designated-initializers`: initialise aggregates with designated initialisers.
  - `readability-implicit-bool-conversion`: write `p != nullptr`, `opt.has_value()`.
  - `readability-function-cognitive-complexity`: split parsers into helpers.
  - `bugprone-exception-escape`: `noexcept` only on code that does not allocate.
  - `cppcoreguidelines-avoid-const-or-ref-data-members`: hold observers as pointers.
  - `performance-enum-size`: enums use `: std::uint8_t`.
- Headers: `clang-tidy_all` checks project headers too; header selection is left to the config's `HeaderFilterRegex` (the target passes no `-header-filter`).
- Doc comments: `///` on every public function and type; on a data member only when it adds a unit, range or meaning beyond its name and type. Say what the function computes (formulas in Magic Formula symbols are welcome), its units and its preconditions. Code, tests and test data carry no source citations, page or equation numbers, evidence tags or decision/question IDs; the derivation lives in `docs/research/`. At most one plain pointer to a research note per file (file name only).

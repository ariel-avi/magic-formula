---
name: cpp-conventions
description: The C++26 coding standard for magic-formula. Covers naming, namespaces, header layout, error handling (std::expected vs exceptions), ownership, const-correctness, units, Eigen usage rules, GoogleTest/benchmark conventions and clang-format/clang-tidy expectations.
when_to_use: Writing, planning, designing or reviewing any C++ in include/, src/, app/, tests/ or bench/.
user-invocable: true
---

# C++ conventions

Standard: C++26 (`CMAKE_CXX_STANDARD 26`, i.e. `-std=c++26` / `-std=c++2c`), no compiler extensions (`CMAKE_CXX_EXTENSIONS OFF`). Warnings: `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror` on project targets. Code must compile on both GCC >= 14 and Clang >= 19. Use a C++26 feature only if both support it; otherwise use the C++23 equivalent.

## Naming

| Entity | Style | Example |
|--------|-------|---------|
| Types, classes, structs, enums, concepts | PascalCase | `TireModel`, `Pac2002Model`, `TirFile`, `SlipInputs` |
| Functions, methods, variables, parameters | snake_case | `evaluate_batch`, `vertical_load` |
| Data members (private) | snake_case + trailing `_` | `coefficients_` |
| Constants (`constexpr`, `static const`), enumerators | `kPascalCase` | `kPi`, `kDefaultEpsilon`, `Status::kOk` |
| Template parameters | PascalCase | `Scalar`, `Model` |
| Namespaces | lowercase | `mf`, `mf::io`, `mf::detail` |
| Files | snake_case | `tire_model.hpp`, `pac2002_model.cpp`, `pac2002_model_test.cpp` |
| Macros (avoid) | `MF_UPPER_CASE` | `MF_EXPORT` |

Magic Formula coefficients keep their literature names as struct members (`PCX1`, `PDY2`, `QBZ1`, `LMUX`) so they match the `.tir` keys one-to-one. This is the only allowed exception to snake_case. Document each with its meaning and units.

## Namespaces and layout

- Everything lives in `namespace mf { ... }`. Sub-namespaces by layer: `mf::io` (parsing / file formats), `mf::detail` (internal helpers, not part of the API). The CLI may use `mf::app`.
- Public headers: `include/magic_formula/<name>.hpp`, included as `#include <magic_formula/<name>.hpp>`. Private headers stay in `src/`.
- Every header starts with `#pragma once`. No include guards and no `using namespace` in headers.
- Include order (clang-format enforces it): own header, C++ standard library, third-party (Eigen, gtest), project.
- One class per header/source pair unless the types are trivially related (e.g. a POD and its enum).
- Header order: includes, namespace, forward declarations, types, free functions. Keep implementations in `.cpp` unless they are templates or `constexpr`/`inline` on purpose.

## Error handling

- Recoverable errors (parse failures, missing/invalid `.tir` keys, malformed input rows, unsupported model version): return `std::expected<T, mf::Error>`, where `mf::Error` carries a code, a message and a source location (file, line, key). Check every result; never call `.value()` without checking first.
- Exceptions only at the application boundary (`app/`): `main` converts `mf::Error` into a non-zero exit code and a message on stderr. The library throws nothing on its own except for allocation failures and contract violations.
- Contract violations (programmer errors) use `assert`. Never use `[[assume]]` on data that comes from the user or a file.
- Numerical guards (division by near-zero, `Vx -> 0`, `Fz <= 0`) are explicit, named (`kEpsilonV`, `kEpsilonX`, ...) and tested on both branches.

## Ownership, values, const

- No raw `new`/`delete`, no owning raw pointers. Use values first, then `std::unique_ptr` (factories return `std::unique_ptr<TireModel>`), and `std::shared_ptr` only with a documented reason.
- Prefer value semantics and the Rule of Zero. Polymorphic bases: `virtual ~Base() = default;`, delete copy/move or make them protected (no slicing).
- `const` everything that can be const: locals, parameters by `const&` for non-trivial types, member functions. `constexpr` for pure math helpers and constants.
- `[[nodiscard]]` on every function that returns a value the caller must use (all `std::expected` returns, factories, evaluators).
- `noexcept` on functions that cannot throw (pure math on scalars, getters).
- Use `std::span` for non-owning views, `std::string_view` for read-only strings (never keep one beyond the owner's lifetime), and `enum class` only.
- `std::mdspan` is **conditional**: libstdc++ 14 (the GCC 14 minimum) does not ship `<mdspan>` (it arrived in GCC 15). By default use `Eigen::Map` or `std::span` plus explicit strides for multi-dimensional views. Use `std::mdspan` only where the architect asks for it, behind the feature-test macro (`#include <version>`, then `#if defined(__cpp_lib_mdspan)`), with a fallback that compiles on GCC 14, and only if CI builds both paths.
- Use `override` / `final` explicitly. No `virtual` without a reason.
- Undefined behaviour is never allowed: no signed overflow, no uninitialised reads, no out-of-bounds access, no type-punning through `reinterpret_cast`.

## Units

- Inside the library all quantities use SI units: N, m, rad, m/s, Pa, N·m. Angles are in **radians**.
- Unit conversion (e.g. degrees in the input table, units declared in `.tir` `[UNITS]`) happens once, at the io boundary, and is tested.
- Use strong types (`struct Newton { double value; };` or a small units header) where the architect specifies them. Otherwise every quantity in a public signature documents its unit in the name or the doc comment (`slip_angle_rad`).

## Eigen

- Batch evaluation uses `Eigen::ArrayXd` (element-wise math) over all input rows. Use `Eigen::Matrix` only for real linear algebra.
- **Never** bind an Eigen expression to `auto`; that keeps dangling temporaries / lazy expressions. Assign to a concrete type (`const Eigen::ArrayXd dfz = (fz - fz0) / fz0;`) or call `.eval()`.
- Watch aliasing: `a = a.transpose()`-style statements need `.eval()`. Matrix products assigned to an operand need `.noalias()` only when provably safe.
- Pass Eigen objects by `const Eigen::Ref<const Eigen::ArrayXd>&` or `const Eigen::ArrayXd&`; never by value.
- Branch-free vector code: use `.select()`, `.min()`, `.max()`, `.cwiseMin()` instead of per-element `if`. Still test both sides of every `select` (branch coverage counts the predicate's outcomes via tests, not via gcov).
- The scalar path and the batch path must agree, and a test asserts it (`EXPECT_NEAR` per element).
- Do not use Eigen's `unsupported/` modules without architect approval.

## Testing (GoogleTest)

- One test file per class/unit: `tests/unit/<unit>_test.cpp`. Module (integration) tests: `tests/module/<feature>_test.cpp`. Registration is automatic: the build skeleton globs `tests/unit/*_test.cpp` and `tests/module/*_test.cpp` and registers each through `mf_add_test` with CTest label `unit` / `module` (target name = file stem). Do not edit CMakeLists.txt to register a test while implementing a packet (`agent-workflow`, "Build-file ownership"); a file that does not follow the naming is never registered, and its packet-scoped run selects 0 tests, which is a FAIL.
- Name tests `TEST(ClassName, Behaviour_Condition_Expectation)`, e.g. `TEST(Pac2002Model, LateralForce_ZeroSlipAngle_EqualsVerticalShiftOnly)`. Use fixtures (`TEST_F`) for shared setup and parameterised tests (`TEST_P`) for tables of vectors.
- Floating-point checks: `EXPECT_NEAR(actual, expected, tol)` with a named, documented tolerance (`constexpr double kForceTolN = 1e-6; // reference values printed with 9 significant digits`). No `EXPECT_EQ` on computed doubles.
- Reference data (`.tir` files, input tables, expected outputs) lives in `tests/data/`. Every file records its origin (source, tool/version, or "hand-computed, see test X") in a header comment or a sibling `README.md`. Find data paths through a CMake-provided define (`MF_TEST_DATA_DIR`), never through relative cwd paths.
- Test every error path of every `std::expected` function, and both sides of every numerical guard.
- Never disable or skip tests (`DISABLED_`, `GTEST_SKIP`) to get green; that is a blocker in review.
- Benchmarks (`bench/`): Google Benchmark, `BM_<Unit>_<Scenario>`, use `benchmark::DoNotOptimize`, and parameterise the batch size with `->Range(...)`.

## Formatting and static analysis

- `.clang-format` is authoritative (`clang-format --dry-run --Werror` must be clean). Never hand-format against it.
- `.clang-tidy` is authoritative. Expect `bugprone-*`, `modernize-*`, `performance-*`, `readability-*`, `cppcoreguidelines-*` checks with the repo's documented exclusions. A `// NOLINT(<check>)` must name the check and give a reason, and the reviewer approves it.
- Doc comments: `///` on every public declaration. Say what the function computes, its units, its preconditions, and the literature equation it implements (`/// Implements <source>, eq. <n> (see docs/research/<topic>.md)`), when the researcher has supplied one.

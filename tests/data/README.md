# tests/data

## Origin

Every file in this directory (except `reference/`) is **synthetic**: authored for magic-formula from
`docs/research/test-data.md`.
Nothing is copied from, or derived from, any vendor, third-party or reference-implementation file.
`reference/` holds numbers computed by MFeval 4.0 on our synthetic fixture, with their origin and
licence notice in `reference/README.md`. The coefficient values are the hand-derivable fixture "S" of the
research notes; the operating ranges of `pac2002_S_rng.tir` are synthetic values.

Each `.tir` file starts with `$` / `!` origin comment lines, each input table with a `#` origin line.
All files use LF line endings and are authored so that the parser's lexical rules hold,
except for the deliberate lexical variants of `lexical_mix.tir`. CRLF handling is tested inline in the
C++ tests (string literals with `"\r\n"`), not with a file, because git may normalise line endings.

## Tire property files

| File | Purpose | Derivation | Vector ids | Expected load outcome |
|------|---------|------------|------------|-----------------------|
| `pac2002_S.tir` | Fixture S, PAC2002, cosine combined slip, no ranges | 133 assignment lines, `PROPERTY_FILE_FORMAT = 'PAC2002'`, no `FITTYP` | V1-V22 (V11 included), P* | PAC2002, cosine method, no warnings |
| `mf52_S52.tir` | Fixture S52, MF 5.2 (FITTYP 6) | S - `PROPERTY_FILE_FORMAT` + `FITTYP = 6` + `WIDTH = 0.2` + `RIM_RADIUS = 0.2` (135 lines); keeps `LGAX/LGAY/LGAZ` | M1-M11 | MF 5.2, one warning listing LGAX, LGAY, LGAZ |
| `mf52_user_pff.tir` | MF 5.2 in the TNO MF-Tyre ADAMS user-tire form | S52 without `LGAX/LGAY/LGAZ`, plus `PROPERTY_FILE_FORMAT = 'USER'` and `USER_SUB_ID = 815` (hand-written, not copied from any MFeval file) | R-USER | MF 5.2, no warnings; outputs equal `mf52_S52.tir`; `USER_SUB_ID` is an ignored key |
| `pac2002_S_rng.tir` | Operating ranges and clamping | S + `[LONG_SLIP_RANGE]` KPU -1.0..1.0, `[SLIP_ANGLE_RANGE]` ALP -1.5..1.5, `[INCLINATION_ANGLE_RANGE]` CAM -0.2..0.2, `[VERTICAL_FORCE_RANGE]` FZ 200..10000 (141 lines) | V19, P6 | PAC2002 with the four ranges, no warnings |
| `pac2002_S_ell.tir` | Friction-ellipse selection | S without the 20 R* keys (RBX1 RBX2 RCX1 REX1 REX2 RHX1 RBY1 RBY2 RBY3 RCY1 REY1 REY2 RHY1 RHY2 RVY1 RVY2 RVY3 RVY4 RVY5 RVY6; 113 lines) | P1, P16 | PAC2002, friction-ellipse method, one warning |
| `pac2002_R.tir` | Dense fixture R, PAC2002, cosine combined slip (golden) | S with the coefficients of `rich_coefficients()` (`tests/unit/mf5x_kernels_test.cpp`) set to non-zero values (RHY2, QBZ3 and QEZ3 stay 0, every scaling factor L* stays 1), including: PEX1-4, PDX2/3, PKX2/3, PHX1/2, PVX1/2, PEY1-4, PDY2/3, PKY3, PHY1-3, PVY1-4, QBZ2/4/5/9/10, QDZ2-4/6-9, QEZ1/2/4/5, QHZ1-4, REX1/2, RHX1, REY1/2, RHY1, RBX2, RBY2, RBY3, RVY1-6, SSZ1-4, QSX1-3, QSY2-4 (QSY1 stays 0.01) | golden | PAC2002, cosine method, My from the QSY form, no warnings |
| `pac2002_R_ell.tir` | Dense fixture R, friction-ellipse combined method (golden) | `pac2002_S_ell.tir` (no R* keys) with the same dense values for every non-R* coefficient | golden | PAC2002, friction-ellipse method, one warning |
| `pac2002_R_fit5.tir` | Dense fixture R with the My shift form (golden) | `pac2002_R.tir` with `FITTYP = 5` after `PROPERTY_FILE_FORMAT`, `QSY1 = QSY2 = 0` and non-zero `QSY3 = 0.1`, `QSY4 = 0.01` (so the QSY form would give different bits); PHX1/2 and PVX1/2 non-zero | golden | PAC2002, cosine method, My from the shift form, one warning |
| `mf52_R52.tir` | Dense fixture R52, MF 5.2 (golden) | `mf52_S52.tir` with the same dense values as `pac2002_R.tir` (non-zero RVY1-6 and PDY3, so SVyk0 differs from SVyk); keeps `LGAX/LGAY/LGAZ` | golden | MF 5.2, one warning listing LGAX, LGAY, LGAZ |
| `pac2002_S_part.tir` | Partial R* set | S without `RHX1` only (132 lines) | V23 | Load error MISSING_KEY naming RHX1 |
| `mf61_unsupported.tir` | MF-Tyre 6.x is not implemented | `[MDI_HEADER]` + `[MODEL] FITTYP = 61` only | R7 | Load error UNSUPPORTED_MODEL |
| `lexical_mix.tir` | Every lexical variant, same values as S | Same key/value set as `pac2002_S.tir` (plus an unknown quoted `TIRE_DESCRIPTION` and `[INERTIA] MASS`) | L1-L4, L6, L7, L10, L11 | Coefficients and metadata identical to `pac2002_S.tir`, no warnings |

`lexical_mix.tir` covers: `!` header lines; `$` full-line and trailing comments (with and without a
blank before `$`); exponent forms of the S values (`4.0e+003`, `2.0E+001`, `1.0e-002`, `+1.5`, `.1`);
mixed-case section names and keys; a section header with inner blanks; tabs around keys, `=` and
values; whitespace-only and blank lines; `MASS` in `[UNITS]` and `[INERTIA]`; a comment line over 300
characters; a `[SHAPE]` table (`{radial width}` header, no `=`, one trailing comment); a quoted string
containing `$` and `!`; an unquoted `TYRESIDE = LEFT`; `PROPERTY_FILE_FORMAT = 'Pac2002'`,
`USE_MODE = 4.0`; no final newline.

## Input tables

Format: see the CLI documentation. Comments (`#`) appear only on their own lines.

| File | Purpose | Vector ids | Used with | Notes |
|------|---------|------------|-----------|-------|
| `inputs_s_vectors.csv` | Rows V2, V3, V12, V13, V22 (Fz 0), V22 (Fz -100); header with units | V2, V3, V12, V13, V22, M1, M2, M5 | S, S52 | Data lines 4-9 |
| `inputs_degrees.txt` | The same six points, angles in degrees, columns reordered (`fz sr sa[deg] ia[DEG] vx`), blank/tab separated, comments and blank lines between rows | as above | S | Results equal the CSV run within 1e-9 |
| `inputs_pressure.csv` | A `P[Pa]` column | pressure column ignored | S, S52 | One pressure warning |
| `inputs_ranges.csv` | One row inside and one row beyond each S-rng limit | V19 | `pac2002_S_rng.tir` | See line numbers below |
| `inputs_low_speed.csv` | Vx = 0.5 and Vx = 0 | M8 | S, S52 | PAC2002: VXLOW warning; MF 5.2: error at line 4 |
| `inputs_negative_vx.csv` | Negative Vx | negative Vx is rejected | S, S52 | Error at line 4 |
| `inputs_malformed.csv` | Field-count mismatch | - | S | Error at line 4 |
| `inputs_overflow.csv` | Fz = 1e308 overflows the model to a non-finite output: NUMERICAL error with its line and an empty stdout | hand-written, see cli_test Run_OverflowFz_NumericalErrorAtLine | S | Error at line 4 |

CLI outputs carry 15 columns: the five inputs, the five outputs (FX, FY, MZ, MX, MY) and five 0/1 flag columns
(FZ_CLAMPED, SR_CLAMPED, SA_CLAMPED, IA_CLAMPED, BELOW_VXLOW).

### Documented line numbers (1-based, comments and header included)

| File | Line | Meaning |
|------|------|---------|
| `inputs_negative_vx.csv` | 4 | `VX = -1`, evaluation error (INVALID_INPUT, key VX) |
| `inputs_malformed.csv` | 4 | 4 fields instead of 5, SYNTAX error |
| `inputs_low_speed.csv` | 3 | Vx = 0.5, below VXLOW (flagged, PAC2002 warns) |
| `inputs_low_speed.csv` | 4 | Vx = 0, rejected by MF 5.2 (INVALID_INPUT, key VX); accepted by PAC2002 (flagged) |
| `inputs_overflow.csv` | 4 | Fz = 1e308, non-finite output (NUMERICAL) |
| `inputs_ranges.csv` | 4 | inside all S-rng limits |
| `inputs_ranges.csv` | 5 | SR = 10, above KPUMAX |
| `inputs_ranges.csv` | 6 | SA = 2, above ALPMAX |
| `inputs_ranges.csv` | 7 | IA = 0.5, above CAMMAX |
| `inputs_ranges.csv` | 8 | Fz = 20000, above FZMAX |
| `inputs_ranges.csv` | 9 | Fz = 1e-9, below FZMIN |

## Golden per-point results (`golden/`)

`golden/per_point_<model>_<mode>.csv` (model `pac2002`, `pac2002_ell`, `pac2002_R`, `pac2002_R_ell`, `pac2002_R_fit5`, `mf52` or `mf52_R52`, mode `combined` or `uncombined`) hold per-point
results of the previous implementation (branch `refactor/strip-decision-ids-and-citations`, commit `7a3e5c6`). They
are the bit-exactness reference for the single-point evaluation path.

- **Toolchain:** Ubuntu 24.04.5 LTS, GCC 14.2.0 (`g++-14` 14.2.0-4ubuntu2~24.04.1), x86-64 (a `linux/amd64` container),
  CMake 4.4.4, Debug build, `-std=c++26` ISO mode (no extensions), project warning flags, no `-ffast-math`, no
  `-march` flags. On x86-64 with the default target there is no floating-point contraction, so these values are the
  baseline for bit-identical comparison.
- **Generation:** the previous library was built from a `git archive` of the base commit, and a throwaway program
  (not committed, because it targets the removed single-point API) loaded each `.tir` file with the loader and called
  `evaluate(point, mode)` for every grid row. Rows the previous implementation rejected were dropped.
- **Columns:** `TIR` (fixture file in this directory), the inputs `FZ SR SA IA VX` (SI units, rad), the outputs
  `FX FY MZ MX MY`, then the flags `FZ_CLAMPED SR_CLAMPED SA_CLAMPED IA_CLAMPED BELOW_VXLOW` as 0/1. Every floating
  point value is a C99 hexadecimal float (`%a`), so it is bit-exact (including the sign of zero). Inputs are the
  unclamped values given to `evaluate`.
- **Fixtures:** every golden file lists its `.tir` fixtures in the `TIR` column.

  | Golden file | Fixture(s) | Data rows |
  |-------------|------------|-----------|
  | `per_point_pac2002_{combined,uncombined}.csv` | `pac2002_S.tir`, `pac2002_S_rng.tir` | 362 each |
  | `per_point_mf52_{combined,uncombined}.csv` | `mf52_S52.tir` | 179 each |
  | `per_point_pac2002_ell_{combined,uncombined}.csv` | `pac2002_S_ell.tir` (friction-ellipse combined method) | 181 each |
  | `per_point_pac2002_R_{combined,uncombined}.csv` | `pac2002_R.tir` (dense, cosine method) | 181 each |
  | `per_point_pac2002_R_ell_{combined,uncombined}.csv` | `pac2002_R_ell.tir` (dense, friction ellipse) | 181 each |
  | `per_point_pac2002_R_fit5_{combined,uncombined}.csv` | `pac2002_R_fit5.tir` (dense, My from the shift form) | 181 each |
  | `per_point_mf52_R52_{combined,uncombined}.csv` | `mf52_R52.tir` (dense, F'y with SVyk0) | 179 each |

  Only `pac2002_S_rng.tir` has validity ranges, so clamping is exercised for PAC2002 only. The files after the first
  four were captured later from the same base commit and toolchain, with the same grid; the earlier files were
  regenerated in the same run as the dense `R` files and are byte-identical. The S fixtures zero whole coefficient
  families (their golden values are mostly constants for the shifts, camber and trail terms); the dense `R` fixtures
  set the coefficients of `rich_coefficients()` to non-zero values (RHY2, QBZ3 and QEZ3 stay 0, `pac2002_R_ell.tir` has no R* keys, every L* stays 1), so each output column carries 100+ distinct values per file, except MY in
  `per_point_pac2002_R_fit5_{combined,uncombined}.csv` with 59 (the shift form of My depends only on Fz and Vx), and `pac2002_R_fit5.tir` is the only FITTYP-5 fixture.
- **Grid (181 rows per fixture):** nominal points; Fz sweeps (500..9000 N) with pure SA and pure SR; combined slip
  with negative and positive SR, SA and IA; nonzero IA; rows beyond each limit of `pac2002_S_rng.tir` (Fz, SR, SA, IA,
  both sides); Fz = 0, -0.0, negative; Vx below VXLOW (0.5, 0.1, 0.999999, 1e-3), Vx = 1, 0 and -0.0; large loads
  (6e4, 1.2e5, 1e6 N); high speed (100, 300 m/s); 40 pseudo-random rows from a fixed-seed `std::mt19937_64`.
- **Skipped rows:** for `mf52_S52.tir` and `mf52_R52.tir`, the rows with Vx = 0 and Vx = -0.0 (MF 5.2 requires Vx > 0), in both modes.
- **Platform waiver:** on other platforms (Clang on arm64 with floating-point contraction, a different libm or C
  runtime) a few-ulp difference from these values is accepted. On any Linux x86-64 build (guard `__linux__ && __x86_64__` in `per_point_golden_test.cpp`) the values must match exactly; they were captured with GCC 14.
  The batch path is checked against the per-point path with a scaled relative tolerance of 1e-12.

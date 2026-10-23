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

## Numerics

The batch path is checked against the per-point path with a scaled relative tolerance of 1e-12.

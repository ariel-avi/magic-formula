---
name: tire-model-domain
description: Domain primer on Pacejka Magic Formula tire models for magic-formula. Covers inputs/outputs, the Magic Formula equation, pure vs combined slip, the .tir property-file structure, coefficient families, version identification, sign conventions and units. Version-specific details are resolved in the cited `docs/research/` notes, which this primer points to.
when_to_use: Designing, implementing, reviewing or researching anything that touches tire-model equations, coefficients, .tir parsing, input tables, sign conventions or units.
user-invocable: true
---

# Tire model domain primer

**Ground rule:** this primer states only well-established facts. The version-specific and convention-dependent items that used to be marked "to be confirmed" are now resolved in `docs/research/` (start at `README.md`: source register, evidence tags, findings), and each one below points at its note. Do not implement from this file: `docs/research/`, not this primer, is the source of truth for equations, coefficient sets and conventions. Project rules for items that have no primary source are in `docs/research/policies.md`. Items still open for primary evidence are in `docs/research/open-questions.md`. If a point is not covered there, ask `researcher` for a cited finding and record it in `docs/research/<topic>.md`.

## What the Magic Formula is

An empirical (semi-physical) tire model by H. B. Pacejka. A fitted coefficient set reproduces measured steady-state tire forces and moments. The core "magic formula":

```
y(x) = D · sin( C · arctan( B·x − E·(B·x − arctan(B·x)) ) )
Y(X) = y(x) + S_V,     x = X + S_H
```

| Factor | Name | Effect |
|--------|------|--------|
| B | stiffness factor | with C·D sets the slope at the origin (`BCD` = cornering / longitudinal slip stiffness) |
| C | shape factor | sets the curve shape and how much it drops after the peak |
| D | peak factor | peak value (≈ μ·Fz for forces) |
| E | curvature factor | shape near the peak; controls where the peak occurs |
| S_H, S_V | horizontal / vertical shifts | ply-steer, conicity, camber and rolling-resistance offsets |

B, C, D, E, S_H and S_V are themselves functions of the vertical load, the camber (and, in later versions, the pressure), built from the coefficients in the property file and multiplied by scaling factors.

The **normalised vertical load increment** `dfz = (Fz − Fz0') / Fz0'` with `Fz0' = LFZO · FNOMIN` is the standard load variable in MF 5.x/6.x-family formulations.

## Inputs and outputs

| Symbol | Meaning | Library unit |
|--------|---------|--------------|
| Fz | vertical load (positive in compression in the usual tire-model convention) | N |
| κ (kappa, SR) | longitudinal slip ratio, dimensionless; the exact definition and sign are fixed in `docs/research/conventions.md` section 2 | – |
| α (alpha, SA) | slip angle | rad |
| γ (gamma, IA) | inclination (camber) angle | rad |
| Vx | longitudinal speed of the wheel centre | m/s |
| p | inflation pressure (only for formulations that model it; see versions) | Pa |
| Fx | longitudinal force | N |
| Fy | lateral force | N |
| Mz | self-aligning moment | N·m |
| Mx | overturning moment | N·m |
| My | rolling-resistance moment | N·m |

The model input vector is ordered FZ, SA, SR, IA, VX (P is appended after VX for MF 6.x later). The output vector is FX, FY, MZ, MX, MY followed by the flag outputs FZ_CLAMPED, SR_CLAMPED, SA_CLAMPED, IA_CLAMPED and BELOW_VXLOW, which hold 0.0 or 1.0 (1.0 when that input was clamped to the `.tir` range, or Vx is below VXLOW). Each model names the positions with index enums (`pac2002_model::input_index` and `output_index`).

Characteristics vector (separate from the outputs, `characteristics_length()` = 8): DFY_DSA [N/rad] and DFX_DSR [N], the analytic slopes of the evaluated forces (combined or pure per the slip mode); FY_OVER_SA [N/rad] and FX_OVER_SR [N], the secants (evaluated force over the raw slip; at zero slip the row's slope; 0 without contact); DY and DX [N], the pure-slip peak factors; SA_AT_PEAK_FY [rad] and SR_AT_PEAK_FX [-], the first stationary point of the force from 0 on the side of the row's slip. NaN appears only for the two peak slips (no stationary point in the search range, or no contact); every other channel is finite. Sign parity: DFY_DSA and FY_OVER_SA VERTICAL and LATERAL, DY VERTICAL, SA_AT_PEAK_FY LATERAL, the rest none. Derivatives are exact (forward-mode AD of the closed forms), never finite differences. See `docs/research/characteristics.md`.

CLI input table: the first content line (`#` comments and blank lines skipped) is the header with the column names only (FZ SA SR IA VX, optional P); the next content line is the mandatory unit line, one unit per column (FZ N; SA rad|deg; SR -; IA rad|deg; VX m/s; P Pa); the legacy `NAME[unit]` header form is rejected. The CLI option `--system=iso|sae` converts the model (coefficient sign flips) before evaluation, and `--table-system=iso|sae` sets the axis system of the table and of the printed rows (default: the evaluated model's loaded side; nothing is converted by default). Angles may be in degrees; conversion to radians happens once, at the io boundary (see `cpp-conventions`). Full spec: the doc comment of `mf::io::parse_input_table` and the README section "Input table".

## Pure vs combined slip

- **Pure slip:** one slip only. `Fx0(κ)` with α = 0; `Fy0(α)` and `Mz0(α)` with κ = 0. Each is a magic-formula curve.
- **Combined slip:** κ and α act together. The forces are reduced from their pure-slip values by weighting functions (`G_xα`, `G_yκ`), which have their own coefficient families (`RBX…`, `RCX…`, `RBY…`, …). The lateral force also gets a κ-induced side force `S_VyK`.
- **Aligning moment:** `Mz = −t · Fy + Mzr`. Here `t` is the pneumatic trail (a cosine-type magic formula) and `Mzr` the residual moment. In combined slip a further term depends on Fx (coefficients `SSZ…`).
- **Turn slip:** the Pacejka formulations include turn-slip factors (ζ0…ζ8). When turn slip is neglected they are set to 1. This repo neglects turn slip (all ζ = 1, steady state only); see `docs/research/pac2002.md` section 1.
- **Low speed:** expressions divide by Vx (slip definitions) and by B·C·D-type products. Formulations add small ε terms to avoid singularities. The ε value and where it is applied are in `docs/research/conventions.md` sections 5 and 7.

## The .tir property file

The file is plain ASCII. Section headers are written `[SECTION_NAME]` and entries `KEY = value`. Strings are single-quoted (`'PAC2002'`). A `$` starts a comment (full-line or trailing). Lines starting with `!` are comment/metadata lines. Whether `!` is a trailing-comment marker, and the key and section case rules, are fixed in `docs/research/tir-format.md` section 2 (lexical rules, refined in `docs/research/policies.md` section 4).

Common sections (which ones appear, and their contents, vary by version; required keys and defaults per model: `docs/research/tir-format.md` section 3):

| Section | Contents (typical) |
|---------|-------------------|
| `[MDI_HEADER]` | `FILE_TYPE = 'tir'`, `FILE_VERSION`, `FILE_FORMAT = 'ASCII'` |
| `[UNITS]` | `LENGTH`, `FORCE`, `ANGLE`, `MASS`, `TIME` (e.g. `'meter'`, `'newton'`, `'radians'`, `'kg'`, `'second'`) |
| `[MODEL]` | `PROPERTY_FILE_FORMAT`, `FITTYP`, `TYRESIDE`, `LONGVL` (reference speed), `VXLOW`, `USE_MODE` |
| `[DIMENSION]` | `UNLOADED_RADIUS`, `WIDTH`, `ASPECT_RATIO`, `RIM_RADIUS`, `RIM_WIDTH` |
| `[VERTICAL]` | `FNOMIN` (nominal load), `VERTICAL_STIFFNESS`, `VERTICAL_DAMPING`, … |
| `[SHAPE]` | tire cross-section contour (radial/width point pairs) used for contact geometry, not an input-validity range. The `[SHAPE]` table syntax is `docs/research/tir-format.md` section 2, rule 12 |
| `[LONG_SLIP_RANGE]`, `[SLIP_ANGLE_RANGE]`, `[INCLINATION_ANGLE_RANGE]`, `[VERTICAL_FORCE_RANGE]` | validity ranges (`KPUMIN`/`KPUMAX`, `ALPMIN`/`ALPMAX`, `CAMMIN`/`CAMMAX`, `FZMIN`/`FZMAX`). Clamping to them: `docs/research/conventions.md` section 8 |
| `[SCALING_COEFFICIENTS]` | user scaling factors `L…` |
| `[LONGITUDINAL_COEFFICIENTS]` | `P?X…`, `R?X…` |
| `[OVERTURNING_COEFFICIENTS]` | `QSX…` |
| `[LATERAL_COEFFICIENTS]` | `P?Y…`, `R?Y…` |
| `[ROLLING_COEFFICIENTS]` | `QSY…` |
| `[ALIGNING_COEFFICIENTS]` | `Q?Z…`, `SSZ…` |

The values in `[UNITS]` define the units of every dimensional key in the file. The parser must honour them (or reject units it does not support) instead of assuming SI; the policy (SI only, anything else rejected) is `docs/research/tir-format.md` section 4 and `docs/research/policies.md` section 3.

## Coefficient families (naming scheme)

The letters commonly follow this pattern. Letter 1: `P` = force coefficient, `Q` = moment coefficient, `R` = combined-slip coefficient, `S` = combined-slip Mz term (as in `SSZ`). Letter 2: which factor (B, C, D, E, K = stiffness, H = horizontal shift, V = vertical shift; `S` as in `QSX`/`QSY`/`SSZ`). Letter 3: the axis (X, Y, Z). Then an index. The meaning of `S` as letter 1 and letter 2 ("moment arm" in some sources) and the semantics of each coefficient come from `docs/research/mf52.md` section 3 and `docs/research/pac2002.md` section 3, not from this scheme.

| Family | Purpose |
|--------|---------|
| `PCX`, `PDX`, `PEX`, `PKX`, `PHX`, `PVX` | pure longitudinal force Fx0 |
| `RBX`, `RCX`, `REX`, `RHX` | combined-slip weighting of Fx |
| `PCY`, `PDY`, `PEY`, `PKY`, `PHY`, `PVY` | pure lateral force Fy0 |
| `RBY`, `RCY`, `REY`, `RHY`, `RVY` | combined-slip weighting of Fy and the κ-induced side force |
| `QBZ`, `QCZ`, `QDZ`, `QEZ`, `QHZ` | aligning moment (pneumatic trail and residual moment) |
| `SSZ` | combined-slip Mz contribution of Fx |
| `QSX` | overturning moment Mx |
| `QSY` | rolling-resistance moment My |
| `LFZO`, `LCX`, `LMUX`, `LEX`, `LKX`, `LHX`, `LVX`, `LCY`, `LMUY`, `LEY`, `LKY`, `LHY`, `LVY`, `LTR`, `LRES`, `LXAL`, `LYKA`, `LVYKA`, `LS`, `LMX`, `LMY`, … | scaling factors (default 1) |

The exact index ranges (e.g. how many `PKY`, `QSX`, `QDZ` coefficients exist), the presence of camber scaling factors (`LGAX`, `LGAY`, `LGAZ`) and pressure coefficients (`PPX…`, `PPY…`, `PPZ…`), and the per-version formula of every factor all differ between PAC2002, MF 5.2 and MF 6.x. They are resolved in `docs/research/mf52.md` section 3 and `docs/research/pac2002-vs-mf52.md`; take them only from there.

## Versions and identification

- **PAC2002**: the Magic Formula implementation used in ADAMS, close to the MF 5.x formulation. Equations: `docs/research/pac2002.md`; differences from MF 5.2: `docs/research/pac2002-vs-mf52.md`.
- **MF 5.2**: an MF-Tyre (TNO) release of the formulation described in Pacejka's book. Equations (implementation-derived) and coefficient list: `docs/research/mf52.md`; differences from PAC2002: `docs/research/pac2002-vs-mf52.md`.
- **MF 6.1 / 6.2**: later releases that add inflation-pressure dependence and other changes. Out of scope at first, but the architecture must allow adding them (see `docs/research/pac2002-vs-mf52.md` for the version differences).
- Identification: `[MODEL]` holds `FITTYP` (integer) and, in many files, `PROPERTY_FILE_FORMAT` (string). The mapping from `FITTYP` / `PROPERTY_FILE_FORMAT` values to formulations (which numbers mean PAC2002, MF 5.2, MF 6.1, …) and its precedence are in `docs/research/model-routing.md`. An unknown value must give an explicit error, never a silent fallback.

## Sign conventions and units

- The project defines two axis systems (the user's definitions, `docs/research/coordinate-systems.md` section 1). ISO: x forward, y left, z up; a loaded tire has Fz > 0 and dFy/dSA > 0. SAE: x forward, y right, z down; a loaded tire has Fz < 0 and dFy/dSA < 0. SA, SR and IA have the same physical definition in both; Fz, Fy, Mz and My change sign, Fx and Mx do not. The TNO MF-Tyre ISO-W convention (dFy/dSA < 0 with FNOMIN > 0) is not the project's ISO.
- The equations never change and the evaluation never converts. Converting a model between systems means flipping coefficient signs (transform VERTICAL: 15 coefficients plus FZMIN/FZMAX; transform LATERAL: 15 coefficients plus ALPMIN/ALPMAX; no L* ever flips). The tables and their derivation are in `docs/research/coordinate-systems.md`; `mf5x::COEFFICIENT_SIGNS` and the `negated_by` channel metadata are the one tested place where they live. Do not "fix" signs ad hoc.
- The system of a model is identified from its evaluated behaviour (`tire_model::find_coordinate_system`): a robust, per-axis weighted vote of several indicators at several loads, giving ISO, SAE or MIXED with warnings. Raw coefficient signs are never evidence (gauge symmetries). The repository's fixture S family is MIXED (vertical ISO, lateral SAE). FNOMIN is non-zero and its sign is the loaded side; rows with Fz zero or on the opposite side of FNOMIN are lift-off. Cross-model conventions: `docs/research/conventions.md` sections 1-4; project rules: `docs/research/policies.md` section 8.
- Library-internal units are SI with angles in radians. `.tir` units come from `[UNITS]`.
- `TYRESIDE` (left/right) can mirror lateral quantities. Mirroring is not implemented; the policy is in `docs/research/conventions.md` section 9.

## Literature

Sources: the source register in `docs/research/README.md`; locate them through `researcher`, never quote from memory.
Open implementations are used only for cross-checks and never copied; check licences before using any test vector.

## Testing implications

- Reference outputs come from cited sources or independent tools, recorded in `tests/data/` with their origin. Hand-derived values must show their derivation.
- Every model gets: a pure-slip curve check (zero crossing, peak, slope `BCD` at the origin), a combined-slip reduction check, scaling-factor checks (setting `LMUX = 0.5` halves the peak, as far as the formulation is linear in it; see `docs/research/test-data.md` section 4), and guard tests (Fz <= 0, Vx -> 0, out-of-range inputs).

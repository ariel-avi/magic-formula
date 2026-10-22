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

Input files often give angles in degrees. Conversion to radians happens once, at the io boundary (see `cpp-conventions`).

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

- Several axis systems are in use: ISO 8855 / ISO-W (z up, y to the left), SAE J670 (z down, y to the right), and the "adapted" conventions in Pacejka's book and in ADAMS/TYDEX. The signs of Fy, Mz, Mx, α, γ and sometimes κ differ between them.
- Which convention each formulation's coefficients assume, and which convention this repo uses for inputs and outputs (TYDEX W-axis, ISO orientation), is fixed in `docs/research/conventions.md` sections 1-4. Do not "fix" signs ad hoc. Every conversion lives in one tested place.
- Library-internal units are SI with angles in radians. `.tir` units come from `[UNITS]`.
- `TYRESIDE` (left/right) can mirror lateral quantities. Mirroring is not implemented; the policy is in `docs/research/conventions.md` section 9.

## Literature

Sources: the source register in `docs/research/README.md`; locate them through `researcher`, never quote from memory.
Open implementations are used only for cross-checks and never copied; check licences before using any test vector.

## Testing implications

- Reference outputs come from cited sources or independent tools, recorded in `tests/data/` with their origin. Hand-derived values must show their derivation.
- Every model gets: a pure-slip curve check (zero crossing, peak, slope `BCD` at the origin), a combined-slip reduction check, scaling-factor checks (setting `LMUX = 0.5` halves the peak, as far as the formulation is linear in it; see `docs/research/test-data.md` section 4), and guard tests (Fz <= 0, Vx -> 0, out-of-range inputs).

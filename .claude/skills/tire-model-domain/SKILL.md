---
name: tire-model-domain
description: Domain primer on Pacejka Magic Formula tire models for magic-formula. Covers inputs/outputs, the Magic Formula equation, pure vs combined slip, the .tir property-file structure, coefficient families, version identification, sign conventions and units. Version-specific details are marked "to be confirmed by researcher".
when_to_use: Designing, implementing, reviewing or researching anything that touches tire-model equations, coefficients, .tir parsing, input tables, sign conventions or units.
user-invocable: true
---

# Tire model domain primer

**Ground rule:** this primer states only well-established facts. Anything tagged **[TBC]** ("to be confirmed by researcher") is version-specific or convention-dependent and MUST NOT be implemented from this file. Get a cited finding from `researcher` first. Confirmed findings go in `docs/research/<topic>.md` (with citations). That folder, not this primer, is the source of truth for equations and coefficient sets. When a finding is confirmed, update this primer's [TBC] item to point at the research note.

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
| κ (kappa, SR) | longitudinal slip ratio, dimensionless; the exact definition / sign depends on the convention [TBC] | – |
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
- **Turn slip:** the Pacejka formulations include turn-slip factors (ζ0…ζ8). When turn slip is neglected they are set to 1. Whether this repo models turn slip is an architect decision [TBC for each version's exact factor set].
- **Low speed:** expressions divide by Vx (slip definitions) and by B·C·D-type products. Formulations add small ε terms to avoid singularities. The exact ε values and where they go are [TBC].

## The .tir property file

The file is plain ASCII. Section headers are written `[SECTION_NAME]` and entries `KEY = value`. Strings are single-quoted (`'PAC2002'`). A `$` starts a comment (full-line or trailing). Lines starting with `!` are commonly comment/metadata lines in some tools' files [TBC: whether `!` is a comment marker in PAC2002/MF5.2 reference files; the parser rule for `!` lines waits for a researcher note in `docs/research/`]. Keys are matched case-insensitively in practice [TBC: confirm the parser rules against reference files].

Common sections (which ones appear, and their contents, vary by version [TBC]):

| Section | Contents (typical) |
|---------|-------------------|
| `[MDI_HEADER]` | `FILE_TYPE = 'tir'`, `FILE_VERSION`, `FILE_FORMAT = 'ASCII'` |
| `[UNITS]` | `LENGTH`, `FORCE`, `ANGLE`, `MASS`, `TIME` (e.g. `'meter'`, `'newton'`, `'radians'`, `'kg'`, `'second'`) |
| `[MODEL]` | `PROPERTY_FILE_FORMAT`, `FITTYP`, `TYRESIDE`, `LONGVL` (reference speed), `VXLOW`, `USE_MODE` |
| `[DIMENSION]` | `UNLOADED_RADIUS`, `WIDTH`, `ASPECT_RATIO`, `RIM_RADIUS`, `RIM_WIDTH` |
| `[VERTICAL]` | `FNOMIN` (nominal load), `VERTICAL_STIFFNESS`, `VERTICAL_DAMPING`, … |
| `[SHAPE]` | tire cross-section contour (radial/width point pairs) used for contact geometry, not an input-validity range. Usage per version [TBC] |
| `[LONG_SLIP_RANGE]`, `[SLIP_ANGLE_RANGE]`, `[INCLINATION_ANGLE_RANGE]`, `[VERTICAL_FORCE_RANGE]` | validity ranges (`KPUMIN`/`KPUMAX`, `ALPMIN`/`ALPMAX`, `CAMMIN`/`CAMMAX`, `FZMIN`/`FZMAX`). Whether to clamp inputs to them is [TBC] |
| `[SCALING_COEFFICIENTS]` | user scaling factors `L…` |
| `[LONGITUDINAL_COEFFICIENTS]` | `P?X…`, `R?X…` |
| `[OVERTURNING_COEFFICIENTS]` | `QSX…` |
| `[LATERAL_COEFFICIENTS]` | `P?Y…`, `R?Y…` |
| `[ROLLING_COEFFICIENTS]` | `QSY…` |
| `[ALIGNING_COEFFICIENTS]` | `Q?Z…`, `SSZ…` |

The values in `[UNITS]` define the units of every dimensional key in the file. The parser must honour them (or reject units it does not support) instead of assuming SI.

## Coefficient families (naming scheme)

The letters commonly follow this pattern. Letter 1: `P` = force coefficient, `Q` = moment coefficient, `R` = combined-slip coefficient, `S` = combined-slip Mz term (as in `SSZ`). Letter 2: which factor (B, C, D, E, K = stiffness, H = horizontal shift, V = vertical shift; `S` as in `QSX`/`QSY`/`SSZ`). Letter 3: the axis (X, Y, Z). Then an index. [TBC: the meaning of `S` as letter 1 and letter 2 (described as "moment arm" in some sources) is not confirmed across PAC2002/MF5.2; take semantics per coefficient from cited `docs/research/` notes, not from this scheme.]

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

**[TBC]** The exact index ranges (e.g. how many `PKY`, `QSX`, `QDZ` coefficients exist), the presence of camber scaling factors (`LGAX`, `LGAY`, `LGAZ`) and pressure coefficients (`PPX…`, `PPY…`, `PPZ…`), and the per-version formula of every factor all differ between PAC2002, MF 5.2 and MF 6.x. Take them only from a cited `docs/research/` note.

## Versions and identification

- **PAC2002**: the Magic Formula implementation used in MSC ADAMS, close to the MF 5.x formulation. [TBC: its exact equations and coefficient set relative to MF 5.2.]
- **MF 5.2**: an MF-Tyre (TNO) release of the formulation described in Pacejka's book. [TBC: exact equation set and coefficient list.]
- **MF 6.1 / 6.2**: later releases that add inflation-pressure dependence (Besselink, Schmeitz, Pacejka, 2010) and other changes. Out of scope at first, but the architecture must allow adding them.
- Identification: `[MODEL]` holds `FITTYP` (integer) and, in many files, `PROPERTY_FILE_FORMAT` (string). **[TBC]** The mapping from `FITTYP` / `PROPERTY_FILE_FORMAT` values to formulations (which numbers mean PAC2002, MF 5.2, MF 6.1, …) must be confirmed by researcher before the model factory relies on it. An unknown value must give an explicit error, never a silent fallback.

## Sign conventions and units

- Several axis systems are in use: ISO 8855 / ISO-W (z up, y to the left), SAE J670 (z down, y to the right), and the "adapted" conventions in Pacejka's book and in ADAMS/TYDEX. The signs of Fy, Mz, Mx, α, γ and sometimes κ differ between them.
- **[TBC]** Which convention each formulation's coefficients assume, and which convention this repo uses for inputs and outputs. The researcher confirms it, the architect fixes it in the design, and it is documented in `docs/research/sign-conventions.md`. Until then, do not "fix" signs ad hoc. Every conversion lives in one tested place.
- Library-internal units are SI with angles in radians. `.tir` units come from `[UNITS]`.
- `TYRESIDE` (left/right) can mirror lateral quantities. [TBC: exact effect per version.]

## Literature (locate via researcher; do not quote from memory)

- H. B. Pacejka, *Tire and Vehicle Dynamics* (3rd ed., with I. Besselink, 2012), chapter 4 (Magic Formula).
- I. J. M. Besselink, A. J. C. Schmeitz, H. B. Pacejka, "An improved Magic Formula/Swift tyre model that can handle inflation pressure changes", *Vehicle System Dynamics* 48 (sup1), 2010.
- MSC ADAMS/Tire documentation (PAC2002), TNO / Siemens MF-Tyre/MF-Swift user manuals.
- Open implementations, used only for cross-checks and never copied: e.g. MFeval (MATLAB), other open-source MF codes. Check licences before using any test vector.

## Testing implications

- Reference outputs come from cited sources or independent tools, recorded in `tests/data/` with their origin. Hand-derived values must show their derivation.
- Every model gets: a pure-slip curve check (zero crossing, peak, slope `BCD` at the origin), a combined-slip reduction check, scaling-factor checks (setting `LMUX = 0.5` halves the peak, as far as the formulation is linear in it [TBC per factor]), and guard tests (Fz <= 0, Vx -> 0, out-of-range inputs).

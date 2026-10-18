# Test data and vectors

Revision 5 (data-status update after validation; vectors unchanged).

## 1. Data candidates and licence status

| Data | Status | Use |
|---|---|---|
| MSC example (p.48-52) | 'Copyright MSC.Software ... 2004', no licence | values may be cited; not a fixture until the user decides (Q-07) |
| CHRONO bundled .tir files (@39393e3) | headers say 'from passenger car template' or MSC copyright (pactest.tir l.18); feda files are Goodyear-derived | do not copy. Recreate synthetically the cases they show: 'MF_05' with FITTYP 5, and a duplicate [DEFLECTION_LOAD_CURVE] |
| TEASIT fsaettc_obfuscated*.tir | GPL-3 repo, FSAE TTC terms, values rounded by num2str. The _v52 file is an MF 6.1 key set relabelled FITTYP 6 | do not copy |
| MFEVAL sample files and outputs | MFeval 4.0.0 was executed in GNU Octave 10.3 during validation (validation.md). The MFeval example `.tir` is not in the tree. Outputs for the synthetic fixture S52 are committed as `tests/data/reference/mf52_mfeval.csv` (190 rows) | kept by user decision 2026-10-18 (Q-07a resolved, AD-29). BSD-3 notice retained; the wrapper repo's 'solely for use in conjunction with MathWorks products' clause was weighed and the numbers kept; see `tests/data/reference/README.md` |
| Reference outputs | `tests/data/reference/mf52_mfeval.csv` (MFeval, our own fixture S52); no other oracle output is committed (OpenTire summarised only in validation.md; teasit GPL-3, never) | used by `tests/module/mfeval_reference_test.cpp`; kept by user decision 2026-10-18 (Q-07a, AD-29) |

[policy] Use synthetic `.tir` files in `tests/data/` with an origin note ('synthetic, authored for magic-formula'). Hand-derived expectations below. The validation step against independent references has been done (validation.md). Reference-output tests use MFeval-derived numbers on our own synthetic fixture only; the user decided on 2026-10-18 to keep them (Q-07a resolved, AD-29; BSD-3 notice retained). No third-party `.tir` file and no teasit-derived number is committed.

## 2. Fixture S (synthetic PAC2002)

Values are [derived] from the rev-2 vectors and were cross-checked against the rev-2 fixture S: every numeric coefficient value matches, and V1-V22 and M1-M8 recompute by hand. The rev-4 reconstruction had dropped the non-coefficient keys; the full key list is restored here. Every key is listed; a key not listed is absent (no FITTYP, no range sections).

```
[MDI_HEADER]  FILE_TYPE = 'tir'  FILE_VERSION = 3.0  FILE_FORMAT = 'ASCII'
[UNITS]       LENGTH = 'meter'  FORCE = 'newton'  ANGLE = 'radians'  MASS = 'kg'  TIME = 'second'
[MODEL]       PROPERTY_FILE_FORMAT = 'PAC2002'  USE_MODE = 4  VXLOW = 1  LONGVL = 16.6  TYRESIDE = 'LEFT'   (no FITTYP)
[DIMENSION]   UNLOADED_RADIUS = 0.3
[VERTICAL]    FNOMIN = 4000
[SCALING_COEFFICIENTS]  LFZO LCZ LCX LMUX LEX LKX LHX LVX LGAX LCY LMUY LEY LKY LHY LVY LGAY LTR LRES LGAZ LXAL LYKA LVYKA LS LSGKP LSGAL LGYR LMX LVMX LMY = 1 (one line each)
[LONGITUDINAL_COEFFICIENTS]  PCX1 = 1.5  PDX1 = 1.0  PKX1 = 20  RBX1 = 10  RCX1 = 1;  PDX2 PDX3 PEX1 PEX2 PEX3 PEX4 PKX2 PKX3 PHX1 PHX2 PVX1 PVX2 RBX2 REX1 REX2 RHX1 = 0
[OVERTURNING_COEFFICIENTS]   QSX1 QSX2 QSX3 = 0
[LATERAL_COEFFICIENTS]       PCY1 = 1.3  PDY1 = 1.0  PKY1 = -15  PKY2 = 1.5  RBY1 = 10  RCY1 = 1;  PDY2 PDY3 PEY1 PEY2 PEY3 PEY4 PKY3 PHY1 PHY2 PHY3 PVY1 PVY2 PVY3 PVY4 RBY2 RBY3 REY1 REY2 RHY1 RHY2 RVY1 RVY2 RVY3 RVY4 RVY5 RVY6 = 0
[ROLLING_COEFFICIENTS]       QSY1 = 0.01;  QSY2 QSY3 QSY4 = 0
[ALIGNING_COEFFICIENTS]      QBZ1 = 10  QCZ1 = 1.2  QDZ1 = 0.1;  QBZ2 QBZ3 QBZ4 QBZ5 QBZ9 QBZ10 QDZ2 QDZ3 QDZ4 QDZ6 QDZ7 QDZ8 QDZ9 QEZ1 QEZ2 QEZ3 QEZ4 QEZ5 QHZ1 QHZ2 QHZ3 QHZ4 SSZ1 SSZ2 SSZ3 SSZ4 = 0
```

- **PROPERTY_FILE_FORMAT 'PAC2002' is required.** Without it, routing (AD-11, model-routing.md) rejects S, because S has no FITTYP either.
- **R* keys are explicit and non-trivial,** so the full R set is present and the cosine method is selected; G != 1 when both slips are non-zero.
- **Shifts are zero.** All horizontal and vertical shifts are 0, so the odd-symmetry properties hold.

Repository files (synthetic; each carries an origin note):

| File | Content |
|---|---|
| `tests/data/pac2002_S.tir` | fixture S as listed above |
| `tests/data/mf52_S52.tir` | S + FITTYP = 6 + WIDTH 0.2 + RIM_RADIUS 0.2, with no PROPERTY_FILE_FORMAT |
| `tests/data/pac2002_S_rng.tir` | S + synthetic ranges (AD-28): KPUMIN/KPUMAX -1.0 / 1.0, ALPMIN/ALPMAX -1.5 / 1.5, CAMMIN/CAMMAX -0.2 / 0.2, FZMIN/FZMAX 200 / 10000. These are not the MSC example values (licence, Q-07). |
| `tests/data/pac2002_S_ell.tir` | S without any R* key: the friction-ellipse method is selected, with a warning (AD-07). This supersedes the rev-2 expectation 'friction-ellipse fallback not implemented'. |
| `tests/data/pac2002_S_part.tir` | S without RHX1 (V23): loading fails naming RHX1. Rev 2 removed RBX1 instead; rev 4 V23 removes RHX1 (keep RHX1). A key written with value 0 counts as present (AD-07). |

Variants written 'S + KEY = value' in the vector tables are built in the tests from these files or by editing the parameters; a stored file needs its own origin note.

Notes:

- **Omitted sections.** Turn-slip, contact, dynamic and loaded-radius sections are left out.
- **Tolerance policy.** See the note at the start of section 3.

## 3. Vectors

Input column order: Fz, kappa, alpha, gamma, Vx (Vx = 10 unless given).

Tolerances (unless a row states otherwise):

- Closed forms: relative 1e-12 (1e-6 for MF 5.2 vectors, where eps perturbs at the 1e-7 level).
- Printed values: half a unit of the last printed digit.
- Identity and bit-equal vectors (V15, V19, P6, the E clamps): compared with `std::bit_cast<std::uint64_t>` on both sides, with the vector id in a comment. This is the documented exception to 'no EXPECT_EQ on doubles'.

| ID | Fixture | Input | Expected | Basis |
|---|---|---|---|---|
| V1 | S | Fz 4000 | dfz = 0 | (16) |
| V2 | S | 4000, 0.1 | Fx0 = 4000*sin(1.5*atan(4/3)) = 3935.48 N | (18)-(28) |
| V3 | S | 4000, 0, 0.05 | Ky0 = -55384.615 N/rad; By = -10.650888; Fy0 = -2376.40 N | (30)-(39) |
| V4 | S + PVX1 0.01 | 4000, 0 | Fx0 = 40 N (the as-printed S_Vx inside the sine would give 2980.45) | C2 |
| V5 | S | 4000, any, Vx 10 | My_out = -12 N m (eq 87 value +12) | (87), D-11 |
| V6 | S | sweeps | Fx0 odd in kappa; Fy0 and Mz0 odd in alpha | [derived] |
| V7 | S | dFx0/dkappa at 0, h = 1e-7 | 80000 (rel 1e-6) | (25) |
| V8 | S + LMUX 0.5 | 4000, 0.1 | Dx = 2000; Bx = 80/3 | (23), (26) |
| V9 | S + QSX1 0.01, QSX2 0.5, QSX3 0.1 | 4000, 0, 0, 0.05 | Fy = 0; Mx = -18 N m | (86) |
| V10 | S + FITTYP 5, QSY1 0, PHX1 0.001, PVX1 0.01 | 4000 | My_out = -36 N m (eq 88 value +36) | (88), D-11 |
| V11 | S, uncombined | 4000, 0, 0.05 | t = 0.0254434 m; Mz0 = +60.46 N m | (43)-(56) |
| V12 | S | 4000, 0.1, 0.1 | Fx = Fx0(V2)*cos(pi/4) = 2782.80 N | (58)-(66) |
| V13 | S | 4000, 0.1, 0.05 | Fy = -1680.37 N | (67)-(77) |
| V14 | S + RBY1 0, RVY1 0.1, RVY5 1, RVY6 10 | 4000, 0.1, 0.05 | SVyk = 400*sin(atan(1)) = 282.843 N; Fy = -2093.56 N | (75): sin(RVY5*atan(RVY6*kappa)); the rev-2 key-role doubt is resolved |
| V15 | S + PEX1 1.5 | 4000, 0.1 | bit-equal to PEX1 = 1.0; 3603.08 N | D-17 |
| V16 | S + PDX3 10, LGAX 0.5 | 4000, 0.1, 0, 0.1 | Dx = 3900 (PAC2002, gamma_x) | C3 |
| V17 | S + PHY3 0.1 | 4000, 0, 0, 0.1 | Fy0 = -550.02 N; Mz0 = +16.50 N m | D-16 reading A |
| V18 | S + LFZO 1.2 | 4000, 0, 0.05 | dfz = -1/6; Ky0 = -61132.08 N/rad; Dt = 0.03 m | (37), (52), D-23 |
| V19 | S-rng (limits kappa 1.0, alpha 1.5, gamma 0.2, Fz 10000, Fz 200) | kappa 10; alpha 2; gamma 0.5; Fz 20000; Fz 1e-9 | bit-equal to evaluating at the limit (kappa 1.0, alpha 1.5, gamma 0.2, Fz 10000, Fz 200 respectively); one warning per variable | D-03, AD-28 |
| V20 | S | Fz 1e-9 | all outputs finite; abs(Fx0) <= 1e-9 N | [derived] |
| V21 | S + QSY3 0.1, QSY4 0.01 | 4000, Vx 10 | My_out = -1200*(0.01 + 0.1*(10/16.6) + 0.01*(10/16.6)^4) = -85.87 N m | (87) |
| V22 | S | Fz 0 and -100 | every output exactly 0 | D-04 |
| V23 | S + RHX1 omitted | load | error naming RHX1 | D-07 |

MF 5.2 vectors ([impl] equations, mf52.md section 4):

| ID | Fixture | Input | Expected | Basis |
|---|---|---|---|---|
| M1 | S52 | 4000, 0.1 | Fx0 = 3935.48 N (eps effect < 1e-9 relative) | step 4 |
| M2 | S52 | 4000, 0, 0.05 | Fy0 = -2376.40 N | step 5 |
| M3 | S52 + PDX3 10, LGAX 0.5 | 4000, 0.1, 0, 0.1 | Dx = 3600; warning that LGAX is ignored | D-14 |
| M4 | S52 | 4000, 0, 0, 0, Vx 10 | My = -12 N m | step 9 |
| M5 | S52 | 4000, 0, 0.05 | combined Mz = +60.46 N m (cosp = 1 - O(1e-7); Fx = 0) | step 8 |
| M6 | S52 + LFZO 1.2 | 4000, 0, 0.05 | Ky = -61132.08 N/rad; effective trail amplitude Dt*LFZO = 0.03 m | D-23 |
| M7 | S52 + PHY3 0.1 | 4000, 0, 0, 0.1 | Fy0 = -550.02 N; Mz = +16.50 N m | steps 5, 8 |
| M8 | S52 | Vx 0 | row rejected | D-27 |
| M9 | S52 with RBX1 removed | load | warning; RBX1 = 0, so Gxa = 1 | tir-format.md section 3 |
| M10 | S52 and S, each + PDY3 1, RVY1 0.1, RVY5 1, RVY6 10, QCZ1 0 [derived] | 4000, 0.1, 0, 0.1, Vx 10 | PAC2002: Mz = 0. MF 5.2: Mz = 0.03*400*(1 - 0.99)*sin(pi/4) = 0.0848528 N m (relative 1e-6). Pins SVyk0 vs SVyk in F'y | AD-25 |
| M11 | S52 + QCZ1 0 [derived] | 4000, 0, 0.05, 0, Vx 1e-3 | t = Dt*cos(0.05)*Vc/(Vc + 1e-6) with Vc = 1e-3/cos(0.05) and Dt = 0.03 m; observed as t = -Mz/Fy. Pins cos'(alpha) and the Vx -> 0+ behaviour | mf52.md section 4, D-27 |

## 4. Property tests

Relative tolerance 1e-12 for identities and 1e-9 for continuity, unless stated. LFZO = 1 except where a test varies it.

- P1 (ellipse): over the 4 quadrants of kappa, alpha = +/-0.05: sgn(Fx) = sgn(Fx0), sgn(Fy) = sgn(Fy0), and |Fx| <= |Fx0|, |Fy| <= |Fy0|.
- P2 (ellipse): the rewrite equals the printed form with |mu_act| at 1000 random non-degenerate points.
- P3 (both models): at kappa = 0, combined Mz - s*Fx equals Mz0 = -t*Fy0(actual gamma) + Mzr.
- P4: with kappa = 0.1, Mz is continuous across alpha_t = -1e-9, 0, +1e-9 (relies on D-24).
- P5: the t(x) and Mzr(x) kernels are even.
- P6: inputs beyond the limits give outputs equal to those at the limit, with one warning.
- P8: Kx at Fz = FNOMIN equals PKX1*FNOMIN*LKX (SIM20 p.25).
- P9: Ky(Fz) reaches its maximum PKY1*FNOMIN*LKY at Fz = PKY2*FNOMIN (SIM20 p.25).
- P10: max |Fx0 - SVx| over kappa equals PDX1*FNOMIN*LMUX when C > 1.
- P11: Mx follows eq (86) exactly.
- P12: My is linear in Fx with slope R0*Fz*QSY2*LMY/FNOMIN.
- P15: in uncombined mode, Fx = Fx0 and Fy = Fy0.
- P16: at the ellipse degenerate points (kappa_c = 0 and/or alpha* = 0) outputs are finite.

## 5. Parser and routing fixtures (synthetic)

- L1 '2.1e+005'.
- L2 a trailing '$' comment.
- L3 'X = 1.0$c' gives 1.0.
- L4 '!' header lines.
- L5 CRLF line endings, and a last line without a newline.
- L6 MASS in both [UNITS] and [INERTIA].
- L7 a 300-character line.
- L8 a duplicate key gives an error.
- L9 an empty numeric value gives an error.
- L10 mixed-case keys.
- L11 a [SHAPE] table with a '{' header.
- L12 a line without '=' gives an error.
- U1 'mm' gives an error.
- U2 a missing [UNITS] means SI.
- U3 'NEWTON' is accepted.
- R1-R9: one fixture per row of model-routing.md.

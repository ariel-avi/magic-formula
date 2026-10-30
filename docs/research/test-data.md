# Test data and vectors

## 1. Data policy

- MF 5.x property files in `tests/data/` are synthetic, authored for magic-formula, each with an origin note. Their expectations below are hand-derived. `mf61_solver.tir` is a separately identified, user-supplied source-regression fixture; see the MF 6.1 port-data section.
- Reference outputs: `tests/data/reference/mf52_mfeval.csv` (190 rows, computed on fixture S52 by an independent MF 5.2 implementation), used by `tests/module/mfeval_reference_test.cpp`; origin and licence notice in `tests/data/reference/README.md` (policies.md section 6). The validation is summarised in validation.md.
- The MF 6.1 source fixture is the documented exception to the synthetic-property-file policy. Source-regression outputs are distinguished from independent reference outputs.

## 2. Fixture S (synthetic PAC2002)

All values are synthetic; V1-V22 and M1-M8 recompute by hand from them. Every key is listed; a key not listed is absent (no FITTYP, no range sections).

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

- **PROPERTY_FILE_FORMAT 'PAC2002' is required.** Without it, routing (model-routing.md) rejects S, because S has no FITTYP either.
- **R* keys are explicit and non-trivial,** so the full R set is present and the cosine method is selected; G != 1 when both slips are non-zero.
- **Shifts are zero.** All horizontal and vertical shifts are 0, so the odd-symmetry properties hold.

Repository files (synthetic; each carries an origin note):

| File | Content |
|---|---|
| `tests/data/pac2002_S.tir` | fixture S as listed above |
| `tests/data/mf52_S52.tir` | S + FITTYP = 6 + WIDTH 0.2 + RIM_RADIUS 0.2, with no PROPERTY_FILE_FORMAT |
| `tests/data/pac2002_S_rng.tir` | S + synthetic ranges (policies.md section 6): KPUMIN/KPUMAX -1.0 / 1.0, ALPMIN/ALPMAX -1.5 / 1.5, CAMMIN/CAMMAX -0.2 / 0.2, FZMIN/FZMAX 200 / 10000. |
| `tests/data/pac2002_S_ell.tir` | S without any R* key: the friction-ellipse method is selected, with a warning (policies.md section 3, PAC2002 R* keys; pac2002.md section 5.5). |
| `tests/data/pac2002_S_part.tir` | S without RHX1 (V23): loading fails naming RHX1. A key written with value 0 counts as present (policies.md section 3, PAC2002 R* keys; pac2002.md section 5.5). |

Variants written 'S + KEY = value' in the vector tables are built in the tests from these files or by editing the parameters; a stored file needs its own origin note.

Notes:

- **Omitted sections.** Turn-slip, contact, dynamic and loaded-radius sections are left out.
- **Tolerance policy.** See the note at the start of section 3.

## 3. Vectors

Input column order: Fz, kappa, alpha, gamma, Vx (Vx = 10 unless given).

Tolerances (unless a row states otherwise):

- Closed forms: relative 1e-12 (1e-6 for MF 5.2 vectors, where eps perturbs at the 1e-7 level).
- Values given to a few digits: half a unit of the last digit shown.
- Identity and bit-equal vectors (V15, V19, P6, the E clamps): compared with `std::bit_cast<std::uint64_t>` on both sides, with the vector id in a comment. This is the documented exception to 'no EXPECT_EQ on doubles'.

| ID | Fixture | Input | Expected | Equations / rule |
|---|---|---|---|---|
| V1 | S | Fz 4000 | dfz = 0 | load normalisation |
| V2 | S | 4000, 0.1 | Fx0 = 4000*sin(1.5*atan(4/3)) = 3935.48 N | pure Fx0 |
| V3 | S | 4000, 0, 0.05 | Ky0 = -55384.615 N/rad; By = -10.650888; Fy0 = -2376.40 N | pure Fy0 (Ky0, By, Fy0) |
| V4 | S + PVX1 0.01 | 4000, 0 | Fx0 = 40 N (with SVx inside the sine it would be 2980.45) | pac2002.md section 4 |
| V5 | S | 4000, any, Vx 10 | My_out = -12 N m (QSY-form value +12) | My, QSY form; conventions.md section 4 |
| V6 | S | sweeps | Fx0 odd in kappa; Fy0 and Mz0 odd in alpha | odd symmetry of the zero-shift fixture |
| V7 | S | dFx0/dkappa at 0, h = 1e-7 | 80000 (rel 1e-6) | Kx |
| V8 | S + LMUX 0.5 | 4000, 0.1 | Dx = 2000; Bx = 80/3 | mu_x, Bx |
| V9 | S + QSX1 0.01, QSX2 0.5, QSX3 0.1 | 4000, 0, 0, 0.05 | Fy = 0; Mx = -18 N m | Mx equation |
| V10 | S + FITTYP 5, QSY1 0, PHX1 0.001, PVX1 0.01 | 4000 | My_out = -36 N m (SHIFTS-form value +36) | My, SHIFTS form; conventions.md section 4 |
| V11 | S, uncombined | 4000, 0, 0.05 | t = 0.0254434 m; Mz0 = +60.46 N m | pure Mz0 |
| V12 | S | 4000, 0.1, 0.1 | Fx = Fx0(V2)*cos(pi/4) = 2782.80 N | combined Fx (cosine) |
| V13 | S | 4000, 0.1, 0.05 | Fy = -1680.37 N | combined Fy (cosine) |
| V14 | S + RBY1 0, RVY1 0.1, RVY5 1, RVY6 10 | 4000, 0.1, 0.05 | SVyk = 400*sin(atan(1)) = 282.843 N; Fy = -2093.56 N | SVyk: sin(RVY5*atan(RVY6*kappa)) |
| V15 | S + PEX1 1.5 | 4000, 0.1 | bit-equal to PEX1 = 1.0; 3603.08 N | pac2002.md section 4 (E limit) |
| V16 | S + PDX3 10, LGAX 0.5 | 4000, 0.1, 0, 0.1 | Dx = 3900 (PAC2002, gamma_x) | pac2002.md section 4 |
| V17 | S + PHY3 0.1 | 4000, 0, 0, 0.1 | Fy0 = -550.02 N; Mz0 = +16.50 N m | pac2002.md section 5.3 (actual-camber reading) |
| V18 | S + LFZO 1.2 | 4000, 0, 0.05 | dfz = -1/6; Ky0 = -61132.08 N/rad; Dt = 0.03 m | Ky0, Dt; pac2002.md section 5.6 |
| V19 | S-rng (limits kappa 1.0, alpha 1.5, gamma 0.2, Fz 10000, Fz 200) | kappa 10; alpha 2; gamma 0.5; Fz 20000; Fz 1e-9 | bit-equal to evaluating at the limit (kappa 1.0, alpha 1.5, gamma 0.2, Fz 10000, Fz 200 respectively); one warning per variable | conventions.md section 8; policies.md section 6 |
| V20 | S | Fz 1e-9 | all outputs finite; abs(Fx0) <= 1e-9 N | Fz -> 0+ limit |
| V21 | S + QSY3 0.1, QSY4 0.01 | 4000, Vx 10 | My_out = -1200*(0.01 + 0.1*(10/16.6) + 0.01*(10/16.6)^4) = -85.87 N m | My, QSY form |
| V22 | S | Fz 0 and -100 | every output exactly 0 | conventions.md section 8 |
| V23 | S + RHX1 omitted | load | error naming RHX1 | pac2002.md section 5.5 |

Code pointers: the conditioning vectors (V19, V20, V22) are exercised by the pipeline tests (`tests/unit/mf5x_pipeline_test.cpp`, `detail::mf5x_pipeline` in `include/magic_formula/detail/mf5x_kernels.hpp`). The Fz 1e308 row, whose result overflows and is rejected by `mf::check_outputs`, is `tests/data/inputs_overflow.csv`.

MF 5.2 vectors (mf52.md section 4):

| ID | Fixture | Input | Expected | Equations / rule |
|---|---|---|---|---|
| M1 | S52 | 4000, 0.1 | Fx0 = 3935.48 N (eps effect < 1e-9 relative) | step 4 |
| M2 | S52 | 4000, 0, 0.05 | Fy0 = -2376.40 N | step 5 |
| M3 | S52 + PDX3 10, LGAX 0.5 | 4000, 0.1, 0, 0.1 | Dx = 3600; warning that LGAX is ignored | mf52.md section 3 |
| M4 | S52 | 4000, 0, 0, 0, Vx 10 | My = -12 N m | step 9 |
| M5 | S52 | 4000, 0, 0.05 | combined Mz = +60.46 N m (cosp = 1 - O(1e-7); Fx = 0) | step 8 |
| M6 | S52 + LFZO 1.2 | 4000, 0, 0.05 | Ky = -61132.08 N/rad; effective trail amplitude Dt*LFZO = 0.03 m | pac2002.md section 5.6 |
| M7 | S52 + PHY3 0.1 | 4000, 0, 0, 0.1 | Fy0 = -550.02 N; Mz = +16.50 N m | steps 5, 8 |
| M8 | S52 | Vx 0 | row rejected | conventions.md section 5 |
| M9 | S52 with RBX1 removed | load | warning; RBX1 = 0, so Gxa = 1 | tir-format.md section 3 |
| M10 | S52 and S, each + PDY3 1, RVY1 0.1, RVY5 1, RVY6 10, QCZ1 0 | 4000, 0.1, 0, 0.1, Vx 10 | PAC2002: Mz = 0. MF 5.2: Mz = 0.03*400*(1 - 0.99)*sin(pi/4) = 0.0848528 N m (relative 1e-6). Pins SVyk0 vs SVyk in F'y | policies.md section 2 |
| M11 | S52 + QCZ1 0 | 4000, 0, 0.05, 0, Vx 1e-3 | t = Dt*cos(0.05)*Vc/(Vc + 1e-6) with Vc = 1e-3/cos(0.05) and Dt = 0.03 m; observed as t = -Mz/Fy. Pins cos'(alpha) and the Vx -> 0+ behaviour | mf52.md section 4, conventions.md section 5 |

## 4. Property tests

Relative tolerance 1e-12 for identities and 1e-9 for continuity, unless stated. LFZO = 1 except where a test varies it.

- P1 (ellipse): over the 4 quadrants of kappa, alpha = +/-0.05: sgn(Fx) = sgn(Fx0), sgn(Fy) = sgn(Fy0), and |Fx| <= |Fx0|, |Fy| <= |Fy0|.
- P2 (ellipse): the ratio form equals the mu form (pac2002.md section 5.4) at 1000 random non-degenerate points.
- P3 (both models): at kappa = 0, combined Mz - s*Fx equals Mz0 = -t*Fy0(actual gamma) + Mzr.
- P4: with kappa = 0.1, Mz is continuous across alpha_t = -1e-9, 0, +1e-9 (relies on pac2002.md section 5.2).
- P5: the t(x) and Mzr(x) kernels are even.
- P6: inputs beyond the limits give outputs equal to those at the limit, with one warning.
- P8: Kx at Fz = FNOMIN equals PKX1*FNOMIN*LKX (dfz = 0, LFZO = 1).
- P9: Ky0(Fz) reaches its maximum PKY1*FNOMIN*LKY at Fz = PKY2*FNOMIN (LFZO = 1).
- P10: max |Fx0 - SVx| over kappa equals PDX1*FNOMIN*LMUX when C > 1.
- P11: Mx follows the Mx equation (pac2002.md section 3) exactly.
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

## 6. Characteristic vectors

Expected values for the eight characteristics of characteristics.md (DFY_DSA, DFX_DSR, FY_OVER_SA, FX_OVER_SR, DY, DX, SA_AT_PEAK_FY, SR_AT_PEAK_FX). Origin: hand-computed from the closed forms of characteristics.md sections 2 and 4 with an independent scratch script (Python, 70-digit decimal arithmetic, run in isolated mode). The C++ code was never run to produce them, and the script is not committed. The closed forms include eps exactly as the kernel does (Bx = 80000/(6000 + 1e-6), By = Ky0/(5200 + 1e-6)). Every derivative was also checked in the scratch script only against a 1e-25-step central difference of the same high-precision force function at the differentiable points (agreement better than 1e-40 relative) and against one-sided differences at the C17 kink (right and left derivative); no repository test uses finite differences.

Common values (fixture S, Fz 4000, dfz 0, gamma 0, Vx 10, combined cosine unless stated; input order as in section 3: Fz, kappa, alpha, gamma):

- Bx = 80000/(6000 + 1e-6) = 13.3333333311111; Cx = 1.5; Dx = 4000; Kx = 80000.
- Ky0 = Ky = -15*4000*sin(2*atan(1/1.5)) = -55384.6153846154; By = Ky0/(5200 + 1e-6) = -10.6508875719162; Cy = 1.3; Dy = 4000.
- Gxa = 1/sqrt(1 + 100*alpha^2) (independent of kappa); Gyk = 1/sqrt(1 + 100*kappa^2) (independent of alpha); SVyk = 0.
- Pure-slip slopes at zero slip: Fx0'(0) = Bx*Cx*Dx = 6000*Bx = 79999.99998667 and Fy0'(0) = By*Cy*Dy = 5200*By = -55384.61537396. They differ from Kx and Ky by the relative factor CD/(CD + eps) (characteristics.md section 1).
- T = tan(pi/(2*Cy)) = tan(pi/2.6) = 2.63678329558176; peaks on the positive side: SA_AT_PEAK_FY = T/abs(By) = 0.2475646539106, SR_AT_PEAK_FX = sqrt(3)/Bx = 0.1299038105893 (E = 0).

Secants follow characteristics.md section 7: evaluated force divided by the raw slip; at raw slip +-0 the row's tangent channel; rows without contact give exactly +0.0. NaN occurs only in the two peak channels (printed `nan`).

Tolerances: relative 1e-12 for closed forms that include eps exactly as the kernel does; relative 1e-10 for peak slips; the combined-slip peaks of C9, C10 and C10o are roots of the analytic slope, checked by stationarity (slope within 1e-9*abs(K) of 0) and by the sign change as well as by the value.

### 6.1 Vectors

| ID | Fixture | Input (Fz, kappa, alpha, gamma) | Expected (closed form = value) | Tolerance |
|---|---|---|---|---|
| C1 | S | 4000, 0.1, 0, 0 | DFY_DSA = FY_OVER_SA = 5200*By/sqrt(2) = -39162.83710434 (alpha = 0: the secant is the tangent); DFX_DSR = 6000*Bx*cos(1.5*atan(0.1*Bx))/(1 + (0.1*Bx)^2) = 5151.900623800; FX_OVER_SR = 4000*sin(1.5*atan(0.1*Bx))/0.1 = 39354.79640314; SR_AT_PEAK_FX = sqrt(3)/Bx = 0.1299038105893; SA_AT_PEAK_FY = tan(pi/2.6)/abs(By) = 0.2475646539106; DY = DX = 4000 | 1e-12 rel; peaks 1e-10 rel |
| C2 | S | 4000, 0, 0.05, 0 | DFX_DSR = FX_OVER_SR = 6000*Bx/sqrt(1.25) = 71554.17526807 (kappa = 0: the secant is the tangent); DFY_DSA = 5200*By*cos(1.3*atan(0.05*By))/(1 + (0.05*By)^2) = -34707.69217580; FY_OVER_SA = 4000*sin(1.3*atan(0.05*By))/0.05 = -47527.96304983; peaks as C1; DY = DX = 4000 | 1e-12 rel; peaks 1e-10 rel |
| C3 | S | 4000, 0.1, 0.1, 0 (V12) | DFX_DSR = (C1 DFX_DSR)/sqrt(2) = 3642.943867088; FX_OVER_SR = (C1 FX_OVER_SR)/sqrt(2) = 27828.04340887; DFY_DSA = Fy0'(0.1)/sqrt(2) = -8938.282121032; FY_OVER_SA = 4000*sin(1.3*atan(0.1*By))/(0.1*sqrt(2)) = -24701.24501909; peaks as C1 and C2 | 1e-12 rel; peaks 1e-10 rel |
| C4 | S | 4000, 0, 0, 0 | DFX_DSR = FX_OVER_SR = 6000*Bx = 79999.99998667; DFY_DSA = FY_OVER_SA = 5200*By = -55384.61537396 (both secants equal the tangents at zero slip); peaks on the positive sides as C1 | 1e-12 rel; peaks 1e-10 rel |
| C5 | S | 4000, 0, -0.05, 0 | SA_AT_PEAK_FY = -tan(pi/2.6)/abs(By) = -0.2475646539106 (negative side); DFY_DSA and FY_OVER_SA equal the C2 values (even in alpha) | 1e-12 rel; peaks 1e-10 rel |
| C13 | S | 4000, -0.1, 0, 0 | SR_AT_PEAK_FX = -sqrt(3)/Bx = -0.1299038105893; DFX_DSR and FX_OVER_SR equal the C1 values; DFY_DSA = FY_OVER_SA = -39162.83710434 | 1e-12 rel; peaks 1e-10 rel |
| C6 | S + PCY1 1.0 | 4000, 0.1, 0.05, 0 | SA_AT_PEAK_FY = nan (Cy <= 1: the slope never changes sign, characteristics.md section 4); SR_AT_PEAK_FX = 0.1299038105893; remaining channels in section 6.2 | exact (nan); 1e-12 rel; peaks 1e-10 rel |
| C6 | S + PCX1 0.9 | 4000, 0.1, 0.05, 0 | SR_AT_PEAK_FX = nan (Cx <= 1); SA_AT_PEAK_FY = 0.2475646539106; remaining channels in section 6.2 | exact (nan); 1e-12 rel; peaks 1e-10 rel |
| C7 | S-rng | 4000, 0, 2, 0 | alpha clamped to ALPMAX 1.5: DFY_DSA = 0; FY_OVER_SA = Fy(alpha 1.5)/2 = 4000*sin(1.3*atan(1.5*By))/2 = -1849.837475483 (raw alpha 2 in the denominator); DFX_DSR = FX_OVER_SR = 6000*Bx/sqrt(226) = 5321.520841015 (kappa = 0 raw: the secant is the tangent; Gxa(1.5) = 1/sqrt(226)); SA_AT_PEAK_FY = 0.2475646539106 and SR_AT_PEAK_FX = 0.1299038105893 as C1 (inside the range) | 1e-12 rel; peaks 1e-10 rel |
| C8 | S; S-rng + FZMIN -1000, FZMAX 0 | Fz 0 and -100 on S; Fz 100 on S-rng (clamped to 0); each at (0.1, 0.05) and (0, 0) | no contact: all four slopes/secants 0, DY = DX = 0; both secants exactly +0.0 at every row including zero slip; SA_AT_PEAK_FY = SR_AT_PEAK_FX = nan | exact |
| C9 | S + RBY2 5, RBY3 0.02, RVY1 0.05, RVY4 2, RVY5 1, RVY6 10 | 4000, 0.05, 0.05, 0 | DFY_DSA = Fy0'*Gyk + Fy0*(dGyk/dByk)*(dByk/dalpha) + dSVyk/dalpha (characteristics.md section 2.2) = -31436.63872597; FY_OVER_SA = -40824.17919831; DFX_DSR = 31486.30521554; SR_AT_PEAK_FX = sqrt(3)/Bx = 0.1299038105893 (RBX2 = 0); SA_AT_PEAK_FY = 0.3972105943213 (A.3 is not an oracle here because RBY2 and RVY4 are non-zero; the slope at the root is 0 and changes sign) | 1e-12 rel; peak 1e-10 rel |
| C10 | S_ell | 4000, 0.1, 0.05, 0 | DFX_DSR = Fx0'*rx + Fx0*d rx/d kappa and DFY_DSA = Fy0'*ry + Fy0*d ry/d alpha with the Fz-free forms of characteristics.md section 2.3: DFX_DSR = 10599.59198473, DFY_DSA = -27161.23002030; FX_OVER_SR = 35316.04990993; FY_OVER_SA = -30596.45195333; SR_AT_PEAK_FX = 0.1675533315318; SA_AT_PEAK_FY = 0.4281129082399 | 1e-12 rel; peaks 1e-10 rel |
| C10 | S_ell | 4000, 0.05, 0.1, 0 | DFX_DSR = 29331.73571664; DFY_DSA = -14843.97769777; FX_OVER_SR = 33613.08801656; FY_OVER_SA = -32005.27733551; SR_AT_PEAK_FX = 0.2445156130364; SA_AT_PEAK_FY = 0.2983217748151 (same forms as the row above) | 1e-12 rel; peaks 1e-10 rel |
| C10o | S_ell | 4000, 0, 0, 0 | double 0/0 point (both shifted slips 0, kind 1): DFX_DSR = FX_OVER_SR = 6000*Bx = 79999.99998667; DFY_DSA = FY_OVER_SA = 5200*By = -55384.61537396 | 1e-12 rel |
| C14 | S_ell | 4000, 0, 0.05, 0 | single 0/0 point of rx (kind 2): rx_lim = 4000/sqrt(4000^2 + (6000*Bx*sin(0.05))^2) = 0.7072540920832; DFX_DSR = FX_OVER_SR = 6000*Bx*rx_lim = 56580.32735723 (the unit-ratio rule would give 79999.99998667); DFY_DSA = Fy0'(0.05) = -34707.69217580 (ry is identically 1); FY_OVER_SA = Fy0(0.05)/0.05 = -47527.96304983; SR_AT_PEAK_FX = 0.1675533315318 | 1e-12 rel; peaks 1e-10 rel |
| C15 | S_ell | 4000, 0.1, 0, 0 | single 0/0 point of ry (kind 2): ry_lim = 4000/sqrt(4000^2 + (0.1*5200*By)^2) = 0.5854905539184; DFY_DSA = FY_OVER_SA = 5200*By*ry_lim = -32427.16913386; DFX_DSR = Fx0'(0.1) = 5151.900623800 (rx is identically 1); FX_OVER_SR = Fx0(0.1)/0.1 = 39354.79640314; SA_AT_PEAK_FY = 0.4281129082399 | 1e-12 rel; peaks 1e-10 rel |
| C16 | S + PEX1 0.5, PEX4 0.4 | 4000, 0.1, 0, 0 | E+ = 0.5*(1 - 0.4) = 0.3; abs(u) solves (1 - E)*abs(u) + E*atan(abs(u)) = sqrt(3): u = 1.999876569206; SR_AT_PEAK_FX = +u/Bx = 0.1499907427154; DFX_DSR = 6471.907143321; FX_OVER_SR = 38759.79924488 | 1e-12 rel; peak 1e-10 rel |
| C16 | S + PEX1 0.5, PEX4 0.4 | 4000, -0.1, 0, 0 | E- = 0.5*(1 + 0.4) = 0.7; u = 2.886482538696; SR_AT_PEAK_FX = -u/Bx = -0.2164861904383; DFX_DSR = 7341.808190830; FX_OVER_SR = 37481.35068598 | 1e-12 rel; peak 1e-10 rel |
| C17 | S_ell + PVX1 0.01 | 4000, kappa = -SVx/add_epsilon(Kx) = -0.0004999999999937500 (so kc = 0 exactly), 0.05, 0 | genuine kink of rx, right derivative: DFX_DSR = Fx0*mym/(abs(Fx0 - SVx)*abs(sin(alpha_c))) with Fx0 = 0.001259217661803, Fx0 - SVx = -39.99874078234, sin(alpha_c) = 0.04997916927068, mym = 4000: 2.519564294794 (left derivative -2.519564294794); Fx = 0, FX_OVER_SR = Fx/kappa = 0; DFY_DSA = Fy0'(0.05) = -34707.69217580 (ry is identically 1 because kc = 0); FY_OVER_SA = -47527.96304983. Peaks not asserted. Fx0 is the difference of two numbers of size 40, so the numeric value carries a cancellation: evaluate the formula from the plain-lane state (Fx0, SVx, alpha_c as computed by the kernel) | formula on plain-lane state 1e-12 rel; the number above 1e-9 rel |
| C11 | S + PEY1 -0.5 | 4000, 0, 0.05, 0 | Ey = -0.5 on both sides (PEY3 = 0); SA_AT_PEAK_FY = 0.2004969372285; u = By*SA_AT_PEAK_FY = -2.135470336934 reproduces E = (abs(u) - T)/(abs(u) - atan(abs(u))) = -0.5 (T = tan(pi/2.6)); abs(By) includes eps (ignoring it moves E by about 6e-10); DFY_DSA = -37234.83241464; FY_OVER_SA = -48911.84322792 | peak 1e-10 rel; slopes 1e-12 rel |
| C12 | S-rng + ALPMAX 0.1 | 4000, 0, 0.05, 0 | SA_AT_PEAK_FY = nan (the stationary point 0.2475646539106 lies beyond the segment end 0.1); SR_AT_PEAK_FX = 0.1299038105893; DFY_DSA = -34707.69217580 | exact (nan); 1e-12 rel |
| S52 | S52 | every cosine-fixture row above (S and S-derived sets; not the S_ell rows C10, C10o, C14, C15, C17 and not S-rng rows) | bit-equal to S for every channel, point and batch, combined and uncombined. The ellipse rows are PAC2002-only: MF 5.2 is always cosine and cannot select the friction ellipse (missing R* coefficients give 0 with a warning). MF 5.2 runs the same Fx0, Fy0, Gxa, Gyk and SVyk code, with LGA* = 1 as in S) | bit-equal |

### 6.2 All eight channels for the vectors

Values to 13 significant digits; `nan` for a peak without a stationary point or without contact.

| ID | Point (kappa, alpha) | DFY_DSA | DFX_DSR | FY_OVER_SA | FX_OVER_SR | DY | DX | SA_AT_PEAK_FY | SR_AT_PEAK_FX |
|---|---|---|---|---|---|---|---|---|---|
| C1 | S, 0.1, 0 | -39162.83710434 | 5151.900623800 | -39162.83710434 | 39354.79640314 | 4000 | 4000 | 0.2475646539106 | 0.1299038105893 |
| C2 | S, 0, 0.05 | -34707.69217580 | 71554.17526807 | -47527.96304983 | 71554.17526807 | 4000 | 4000 | 0.2475646539106 | 0.1299038105893 |
| C3 | S, 0.1, 0.1 | -8938.282121032 | 3642.943867088 | -24701.24501909 | 27828.04340887 | 4000 | 4000 | 0.2475646539106 | 0.1299038105893 |
| C4 | S, 0, 0 | -55384.61537396 | 79999.99998667 | -55384.61537396 | 79999.99998667 | 4000 | 4000 | 0.2475646539106 | 0.1299038105893 |
| C5 | S, 0, -0.05 | -34707.69217580 | 71554.17526807 | -47527.96304983 | 71554.17526807 | 4000 | 4000 | -0.2475646539106 | 0.1299038105893 |
| C13 | S, -0.1, 0 | -39162.83710434 | 5151.900623800 | -39162.83710434 | 39354.79640314 | 4000 | 4000 | 0.2475646539106 | -0.1299038105893 |
| C6a | S + PCY1 1.0, 0.1, 0.05 | -21766.78012002 | 4608.000003256 | -32199.37887056 | 35199.99999923 | 4000 | 4000 | nan | 0.1299038105893 |
| C6b | S + PCX1 0.9, 0.1, 0.05 | -24542.04449684 | 6170.842201907 | -33607.34496852 | 30729.49406557 | 4000 | 4000 | 0.2475646539106 | nan |
| C7 | S-rng, 0, 2 | 0 | 5321.520841015 | -1849.837475483 | 5321.520841015 | 4000 | 4000 | 0.2475646539106 | 0.1299038105893 |
| C8 | no-contact rows (all C8 rows), any | 0 | 0 | 0 | 0 | 0 | 0 | nan | nan |
| C9 | S + C9 set, 0.05, 0.05 | -31436.63872597 | 31486.30521554 | -40824.17919831 | 55240.83373527 | 4000 | 4000 | 0.3972105943213 | 0.1299038105893 |
| C10a | S_ell, 0.1, 0.05 | -27161.23002030 | 10599.59198473 | -30596.45195333 | 35316.04990993 | 4000 | 4000 | 0.4281129082399 | 0.1675533315318 |
| C10b | S_ell, 0.05, 0.1 | -14843.97769777 | 29331.73571664 | -32005.27733551 | 33613.08801656 | 4000 | 4000 | 0.2983217748151 | 0.2445156130364 |
| C10o | S_ell, 0, 0 | -55384.61537396 | 79999.99998667 | -55384.61537396 | 79999.99998667 | 4000 | 4000 | 0.2475646539106 | 0.1299038105893 |
| C14 | S_ell, 0, 0.05 | -34707.69217580 | 56580.32735723 | -47527.96304983 | 56580.32735723 | 4000 | 4000 | 0.2475646539106 | 0.1675533315318 |
| C15 | S_ell, 0.1, 0 | -32427.16913386 | 5151.900623800 | -32427.16913386 | 39354.79640314 | 4000 | 4000 | 0.4281129082399 | 0.1299038105893 |
| C16p | S + PEX1 0.5, PEX4 0.4, 0.1, 0 | -39162.83710434 | 6471.907143321 | -39162.83710434 | 38759.79924488 | 4000 | 4000 | 0.2475646539106 | 0.1499907427154 |
| C16n | S + PEX1 0.5, PEX4 0.4, -0.1, 0 | -39162.83710434 | 7341.808190830 | -39162.83710434 | 37481.35068598 | 4000 | 4000 | 0.2475646539106 | -0.2164861904383 |
| C11 | S + PEY1 -0.5, 0, 0.05 | -37234.83241464 | 71554.17526807 | -48911.84322792 | 71554.17526807 | 4000 | 4000 | 0.2004969372285 | 0.1299038105893 |
| C12 | S-rng + ALPMAX 0.1, 0, 0.05 | -34707.69217580 | 71554.17526807 | -47527.96304983 | 71554.17526807 | 4000 | 4000 | nan | 0.1299038105893 |
| C17 | S_ell + PVX1 0.01, -0.0004999999999937500, 0.05 | -34707.69217580 | 2.519564294794 | -47527.96304983 | 0 | 4000 | 4000 | - | - |

### 6.3 CLI rows

The rows of `tests/data/inputs_s_vectors.csv` (column order FZ,SA,SR,IA,VX; IA 0, VX 10) with all eight characteristics, for three runs: fixture S combined, fixture S uncombined (`--uncombined`) and fixture S_ell combined (`tests/data/pac2002_S_ell.tir`). Values have 10 significant digits; compare with printed_tol (half a unit of the last digit shown). Under the zero-slip rule no secant is `nan`; the unloaded rows print `0` for the six non-peak channels and `nan` for the two peaks. Uncombined rows are the pure curves (Fx0, Fy0), so the peaks are the pure peaks. Origin: hand-computed from the closed forms.

| Run | Row | DFY_DSA | DFX_DSR | FY_OVER_SA | FX_OVER_SR | DY | DX | SA_AT_PEAK_FY | SR_AT_PEAK_FX |
|---|---|---|---|---|---|---|---|---|---|
| S combined | V2 (SA 0, SR 0.1) | -39162.83710 | 5151.900624 | -39162.83710 | 39354.79640 | 4000 | 4000 | 0.2475646539 | 0.1299038106 |
| S combined | V3 (SA 0.05, SR 0) | -34707.69218 | 71554.17527 | -47527.96305 | 71554.17527 | 4000 | 4000 | 0.2475646539 | 0.1299038106 |
| S combined | V12 (SA 0.1, SR 0.1) | -8938.282121 | 3642.943867 | -24701.24502 | 27828.04341 | 4000 | 4000 | 0.2475646539 | 0.1299038106 |
| S combined | V13 (SA 0.05, SR 0.1) | -24542.04450 | 4608.000003 | -33607.34497 | 35200.00000 | 4000 | 4000 | 0.2475646539 | 0.1299038106 |
| S combined | V22 (Fz 0) | 0 | 0 | 0 | 0 | 0 | 0 | nan | nan |
| S combined | V22 (Fz -100) | 0 | 0 | 0 | 0 | 0 | 0 | nan | nan |
| S uncombined | V2 (SA 0, SR 0.1) | -55384.61537 | 5151.900624 | -55384.61537 | 39354.79640 | 4000 | 4000 | 0.2475646539 | 0.1299038106 |
| S uncombined | V3 (SA 0.05, SR 0) | -34707.69218 | 79999.99999 | -47527.96305 | 79999.99999 | 4000 | 4000 | 0.2475646539 | 0.1299038106 |
| S uncombined | V12 (SA 0.1, SR 0.1) | -12640.63980 | 5151.900624 | -34932.83571 | 39354.79640 | 4000 | 4000 | 0.2475646539 | 0.1299038106 |
| S uncombined | V13 (SA 0.05, SR 0.1) | -34707.69218 | 5151.900624 | -47527.96305 | 39354.79640 | 4000 | 4000 | 0.2475646539 | 0.1299038106 |
| S uncombined | V22 (Fz 0) | 0 | 0 | 0 | 0 | 0 | 0 | nan | nan |
| S uncombined | V22 (Fz -100) | 0 | 0 | 0 | 0 | 0 | 0 | nan | nan |
| S_ell combined | V2 (SA 0, SR 0.1) | -32427.16913 | 5151.900624 | -32427.16913 | 39354.79640 | 4000 | 4000 | 0.4281129082 | 0.1299038106 |
| S_ell combined | V3 (SA 0.05, SR 0) | -34707.69218 | 56580.32736 | -47527.96305 | 56580.32736 | 4000 | 4000 | 0.2475646539 | 0.1675533315 |
| S_ell combined | V12 (SA 0.1, SR 0.1) | -16749.59182 | 15657.19503 | -26292.52881 | 28076.37066 | 4000 | 4000 | 0.4281129082 | 0.2445156130 |
| S_ell combined | V13 (SA 0.05, SR 0.1) | -27161.23002 | 10599.59198 | -30596.45195 | 35316.04991 | 4000 | 4000 | 0.4281129082 | 0.1675533315 |
| S_ell combined | V22 (Fz 0) | 0 | 0 | 0 | 0 | 0 | 0 | nan | nan |
| S_ell combined | V22 (Fz -100) | 0 | 0 | 0 | 0 | 0 | 0 | nan | nan |

## MF 6.1 port data

MF 6.1 fixtures and reference vectors must identify their origin separately from the existing MF 5.x fixtures. The user-supplied movedot-tire-solver MF612 test property file and six stored operating-point cases are source-compatibility inputs, not independent literature oracles. Preserve their rounded-value tolerances and distinguish them from generated full-precision direct-call comparisons.

Regression vectors must isolate the source dispatch corrections by choosing different SA and SR, cover pure/combined evaluation and both turn-slip modes, and exercise pressure/camber effects, RE/RL, no contact, clamping and radius failure. Matrix tests include empty batches and partial final blocks. Coordinate property tests cover each transform and their composition.

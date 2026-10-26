# Tire coordinate systems: ISO, SAE, conversion and identification

Status: the definitions in section 1 are the project owner's (binding). The flip tables (sections 3 and 4) were derived from the implementation and verified numerically (section 9). The TNO ISO-W context (section 2) rests on the sources in section 10; statements marked unverified rest on search snippets. Items marked (open) are listed in open-questions.md. Citations live only in this file (section 10).

## 1. The user's definitions and figures

User message, verbatim:

> - The ISO-W coordinate system is wrong, SA is inverted. Every curve must behave simmetrically: on ISO, FY is negative for negative SA.
> - The equations do NOT change because of coordinate system, ONLY the tire model coefficients' sign change.
> - Update the coordinate system sketch to use a 3D view (threejs) of a simplified tire that shows the convention.
> - Swapping between SAE and ISO should flip the model's related coefficients' sign.
> - Coordinate systems: ISO: iso.png ; SAE: sae.png

Follow-up: "Increase robustness on coordinate system identification. dFy/dSA was a simple suggestion."

| Quantity | ISO (x forward, y left, z up) | SAE (x forward, y right, z down) | ISO -> SAE |
|---|---|---|---|
| Fz of a loaded tire | > 0 | < 0 | flips |
| SA | positive when the wheel travels to the right of its heading | same physical definition | unchanged |
| dFy/dSA at zero slip | > 0 (FY < 0 for SA < 0) | < 0 | flips |
| SR, dFx/dSR | driving positive, dFx/dSR > 0 | same | unchanged |
| IA | top tilted to the right positive | same | unchanged |
| Fx, Mx | | | unchanged |
| Fy, My, Mz, Fz | | | flip |

The figures are not stored in the repository. In words:

- **ISO figure.** A wheel standing on a ground plane, origin at the contact point. x_i points forward along the direction of wheel heading, y_i to the side, z_i up. Fz points up along +z, Mz is an arc about +z, Fx and Fy lie in the ground plane along +x and +y, Mx is an arc about the heading axis and My an arc about +y. The slip angle alpha is drawn in the ground plane between the direction of wheel heading and the direction of wheel travel V. The inclination angle gamma is drawn between +z and the wheel's top line. The wheel spin axis and the angular velocity of the wheel point along the axle.
- **SAE figure.** A wheel with the axes x forward, y to the right and z pointing down. The normal force Fz points down along +z. The tractive force Fx, the overturning moment Mx (about the heading axis), the rolling resistance moment My (about +y), the self-aligning moment Mz (about +z), the lateral force Fy and the slip angle between the wheel plane and the direction of travel are all drawn.

The equations never change: the evaluation always takes the raw inputs and applies the same Magic Formula. A coefficient set belongs to one system; converting it to the other system means flipping coefficient signs (sections 3 and 4), so that the unchanged equations produce the other system's results.

## 2. TNO MF-Tyre ISO-W context

The TNO MF-Tyre "ISO-W" convention (ISO 8855 axes, TYDEX W, tan(alpha) = Vsy/|Vx|, kappa = -Vsx/Vx) has x forward, y left, z up [S1, S2, S13]. A loaded tire has Fz > 0. With that slip-angle definition, PAC2002 files for it carry dFy/dSA < 0 at FNOMIN > 0 [S4, S1]. The user calls this "SA inverted": it is **not** the user's ISO, whose dFy/dSA is > 0 at Fz > 0. The project never uses the name ISO-W for a mode; `parse_coordinate_system` rejects "ISO-W".

Consequence for the repository's own files: the `.tir` fixtures (PKY1 = -15, FNOMIN = 4000) were fitted for the TNO convention. Under the user's definitions their vertical axis says ISO (FNOMIN > 0) and their lateral axis says SAE (dFy/dSA < 0): they are MIXED (section 8).

Other background (sources in section 10):

- **ISO-W** (ISO 8855, TYDEX W): origin at the contact point, x forward in the wheel-plane/road-plane intersection, z up (road normal), y = z x x [S2, S4]. MF-Tyre/MF-Swift uses the ISO convention [S1, S13].
- **SAE** (J670e, J2047; TTC data): x forward, y right, z down [S2]. SAE J670-2008 also allows a z-up variant whose axes equal ISO [S8]. "SAE" in this project means the z-down variant.
- **Adapted SAE** (Pacejka 1st and 2nd editions): SAE axes with the slip-angle sign reversed and Fz reported positive [S6, S7] (unverified). The project defines no mode for it.
- No `.tir` keyword declares the axis system (only TYRESIDE, which selects the vehicle side) [S4]; it is implied by the coefficients.
- The SAE J670 percent slip ratio is braking-positive [S15]. SR in this project is the driving-positive Magic Formula kappa [S1]; a table with J670 slip is a dFx/dSR < 0 case, which the identification reports as a check failure (section 7).
- Do not confuse any of this with TYRESIDE mirroring (y -> -y, an improper map) [S12].

## 3. Transforms A and B

Two elementary, exact transforms act on the coefficient set. Each is a pure sign flip of listed coefficients plus a negate-and-swap of one limit pair.

| | Inputs | Outputs | Limits |
|---|---|---|---|
| **A** (VERTICAL, the full ISO <-> SAE map) | Fz -> -Fz; SA, SR, IA, Vx unchanged | Fy, Mz, My flip; Fx, Mx and the clamp flags unchanged | FZMIN, FZMAX negate and swap |
| **B** (LATERAL, the slip-angle convention) | SA -> -SA; the rest unchanged | all unchanged (flags too) | ALPMIN, ALPMAX negate and swap |

- **Model state** is (s_z, s_y): s_z = sign(FNOMIN) (flipped by A only), s_y = sign(dFy/dSA) at the reference point. s_y flips under **A and B**: under A because Fy flips while SA does not, under B because SA flips while Fy does not. So A takes ISO (+,+) to SAE (-,-), and B toggles only s_y.
- To reach a target: apply A if s_z differs, then B if s_y (after A) still differs. (+,-) -> ISO is {B}; (+,-) -> SAE is {A,B}; (-,+) -> ISO is {A,B}; (-,+) -> SAE is {B}; ISO <-> SAE is {A}.
- **A and B commute**: their coefficient sets are disjoint, and so are their limit pairs. Both are their own inverse, bit-exactly.
- Invariants behind the tables: under A, F'z0 = FNOMIN*LFZO flips with Fz, so dfz = (Fz - F'z0)/F'z0 is invariant. Under B, dfz and every load term are untouched. add_epsilon(d) = d + eps*sgn+(d) is odd for every d != 0; sgn+ is odd for every x != 0; cos, cos(atan(.)), |.| and squares are even; atan and the shaped Magic Formula argument atan(Bx - E(Bx - atan Bx)) are odd in x for fixed B and E.

## 4. The two flip tables

Identical for PAC2002 and MF 5.2.

### 4.1 Transform A (15 flips plus FZMIN/FZMAX)

**Flip:** FNOMIN; PDX1, PDX2; PKX1, PKX2; PVX1, PVX2; QSX1, QSX2, QSX3; QSY2; SSZ1, SSZ2, SSZ3, SSZ4. **Negate-and-swap:** FZMIN' = -FZMAX, FZMAX' = -FZMIN. Everything else unchanged.

| Coefficient(s) | A | Reason |
|---|---|---|
| FNOMIN | flip | F'z0 flips with Fz, so dfz is invariant. The normalisers Fz/FNOMIN (Dt) and Fy/FNOMIN (s, Mx) stay invariant; Fx/FNOMIN (My) flips |
| LFZO | unchanged (> 0) | FNOMIN carries the sign; flipping LFZO fails because FNOMIN appears alone in Dt, s, Mx and My |
| UNLOADED_RADIUS, LONGVL | unchanged | length and speed scales |
| PDX1, PDX2 | flip | Dx = mu_x*Fz must stay invariant (Fx unchanged), so mu_x flips |
| PKX1, PKX2 | flip | Kx = Fz*(PKX1 + PKX2*dfz)*exp(PKX3*dfz)*LKX must stay invariant |
| PVX1, PVX2 | flip | SVx = Fz*(PVX1 + PVX2*dfz)*LVX*LMUX must stay invariant |
| PDX3, PKX3, PCX1, PEX1..PEX4, PHX1, PHX2, RBX*, RCX1, REX*, RHX1 | unchanged | depend only on dfz, kappa and alpha |
| PCY1, PDY*, PEY*, PKY*, PHY*, PVY*, RBY*, RCY1, REY*, RHY*, RVY* | unchanged | mu_y and Ey are invariant; Dy, Ky, SVy, SVyk flip through Fz and F'z0, and SHf = SHy + SVy/Ky is invariant |
| QBZ*, QCZ1, QEZ*, QHZ* | unchanged | Bt, Ct, Et, SHt and Br are invariant (By is invariant) |
| QDZ1..QDZ4 | unchanged | Dt = Fz*(...)*(R0/FNOMIN)*LTR is invariant, so the trail t is invariant |
| QDZ6..QDZ9 | unchanged | Dr = Fz*(...)*R0*LMUY flips through Fz, so Mzr flips |
| SSZ1..SSZ4 | flip | Mz = -t*F'y + Mzr + s*Fx. F'y and Mzr flip, Fx does not, so s must flip; the SSZ2 term carries Fy/FNOMIN (invariant), so SSZ2 flips too |
| QSX1, QSX2, QSX3 | flip | Mx = R0*Fz*(QSX1*LVMX - QSX2*IA + QSX3*Fy/FNOMIN)*LMX. Fz flips and Mx is invariant, so the whole bracket flips |
| QSY1, QSY3, QSY4 | unchanged | My = -R0*Fz*(QSY1 + QSY2*Fx/FNOMIN + QSY3*abs(Vx/LONGVL) + QSY4*(Vx/LONGVL)^4)*LMY: Fz flips, so My flips while the bracket stays invariant |
| QSY2 | flip | Fx/FNOMIN flips, so QSY2 flips to keep the bracket invariant |
| All scaling factors L* (including LGAX, LGAY, LGAZ) | unchanged | see section 6 |
| FZMIN, FZMAX | negate and swap | clamp(-Fz, -FZMAX, -FZMIN) = -clamp(Fz, FZMIN, FZMAX) exactly; the moved flag is identical |
| KPU*, ALP*, CAM*, VXLOW | unchanged | SR, SA, IA and Vx are not transformed |
| eps, sgn+ terms, friction ellipse, cos(alpha) | no change needed | every guarded denominator is invariant or flips; sgn+ arguments are invariant; ellipse ratios use absolute values |

### 4.2 Transform B (15 flips plus ALPMIN/ALPMAX)

**Flip:** PKY1; PHY1, PHY2, PHY3; PEY3, PEY4; QHZ1..QHZ4; QEZ4, QEZ5; QBZ10; RBY3; RHX1. **Negate-and-swap:** ALPMIN' = -ALPMAX, ALPMAX' = -ALPMIN. Everything else unchanged.

| Coefficient(s) | B | Reason |
|---|---|---|
| PHY1, PHY2, PHY3 | flip | alpha_y = SA + SHy must become -alpha_y, so SHy flips |
| PKY1 | flip | Ky flips, so By flips and By*alpha_y is invariant: Fy0 = Dy*sin(C*shaped(By*alpha_y)) + SVy is unchanged |
| PKY2, PKY3 | unchanged | PKY2 is an equivalent alternative to PKY1 (the sin(2 atan) factor is odd in PKY2) but is kept > 0; PKY3 multiplies abs(gy) |
| PEY3, PEY4 | flip | Ey = (PEY1 + PEY2*dfz)*(1 - (PEY3 + PEY4*gy)*sgn+(alpha_y))*LEY: sgn+(alpha_y) flips, so (PEY3 + PEY4*gy) flips to keep Ey invariant |
| PEY1, PEY2, PCY1, PDY1..PDY3, PVY1..PVY4 | unchanged | Cy, Dy and Ey are even in alpha; SVy is the value at alpha_y = 0 |
| QHZ1..QHZ4 | flip | alpha_t = SA + SHt must flip |
| QEZ4, QEZ5 | flip | Et = (...)*(1 + (QEZ4 + QEZ5*gz)*(2/pi)*atan(Bt*Ct*alpha_t)): the atan flips, so (QEZ4 + QEZ5*gz) flips; t is even in alpha_t |
| QBZ1..QBZ5, QCZ1, QDZ*, QEZ1..QEZ3 | unchanged | Bt, Ct and Dt do not depend on SA; the cosine MF is even |
| QBZ10 | flip | Br = QBZ9*LKY/LMUY + QBZ10*By*Cy and By flips, so QBZ10 keeps Br invariant. Mzr = Dr*cos(atan(Br*alpha_r))*cos(SA) is even in alpha_r. QBZ9 flipped instead gives Br' = -Br and is also exact; QBZ10 is canonical because it keeps Br > 0 and QBZ9 unchanged |
| QDZ6..QDZ9 | unchanged | Dr does not depend on SA |
| SHf | follows | SHy flips, SVy is unchanged and Ky flips, so SHf flips and alpha_r flips with no extra flip |
| RHX1 | flip | Gxa: alpha_s = SA + RHX1 must flip; the numerator and denominator cosine MFs are even |
| RBX*, RCX1, REX* | unchanged | Bxa depends on kappa only |
| RBY3 | flip | Byk = RBY1*cos(atan(RBY2*(SA - RBY3)))*LYKA is even once (SA - RBY3) flips |
| RBY1, RBY2, RCY1, REY*, RHY*, RVY* | unchanged | even or SA-free (SVyk contains cos(atan(RVY4*SA)), which is even) |
| SSZ*, QSX*, QSY* | unchanged | s, Mx and My depend on Fy, Fx and IA, which are unchanged |
| alpha_eq (combined Mz) | no change needed | under B, alpha_t flips and (Kx/Ky)^2 is unchanged, so alpha_eq flips; t and Mzr are even |
| Friction ellipse, cos(alpha), cos'(alpha) | no change needed | alpha_c flips but enters squared or as an absolute value; cos is even |
| ALPMIN, ALPMAX | negate and swap | exact clamp mirror; the SA_CLAMPED flag is identical |
| FZMIN/FZMAX, KPU*, CAM*, VXLOW and everything SA-free | unchanged | |

PAC2002 and MF 5.2 share both tables. The formulation differences (cos vs cos' trail, F'y = Fy - SVyk vs Fy - SVyk0, cosine vs friction-ellipse combination, QSY vs shift form of My) add no coefficient. The only formulation-specific item is the code change for the PAC2002 shift form of My (section 5).

### 4.3 Necessity

Every listed flip is necessary. Each table minus one entry was run on PAC2002 cosine, PAC2002 ellipse and MF 5.2, pure and combined slip; every omission leaves a residual far above rounding level, from about 40 to 1e6 in the units of the output (the smallest are PKX2 under A, about 5e2, and QEZ5 under B, about 4e1). Two further checks:

- Omitting QBZ10 from B leaves residuals of 1.6e3 to 2.1e3 N*m in Mz. A table that omits the negate-and-swap of ALPMIN/ALPMAX leaves 2.1e4 in a ranged run.
- Flipping LMY as well under A (wrong) leaves 9.1e7; replacing PKY1 by LKY under B (wrong) leaves 1.2e3.

## 5. Code generalisations and the Ky = 0 caveat

Coefficient flips alone cannot make the property hold. Three sign-agnostic generalisations of the equations are needed; with FNOMIN > 0 each reduces exactly to the earlier behaviour, so ISO-consistent results do not change.

1. **FNOMIN is non-zero.** The load-time check `FNOMIN > 0` becomes "non-zero" (NaN still rejected). Its sign is the loaded side of the equations. LFZO > 0, R0 > 0, LONGVL > 0, PKY2 != 0 and LMUY != 0 stay.
2. **The unloaded rule is relative to sgn(FNOMIN).** `raw_contact = fz*load_sign > 0` and `clamped_contact = fz_clamped*load_sign > 0`, with load_sign = copysign(1, FNOMIN). A row whose Fz is zero, NaN or on the opposite side of FNOMIN gets exactly +0.0 outputs and zero clamp flags, as before. FZMIN and FZMAX may be negative.
3. **PAC2002 shift-form My.** `My = -R0*(SVx + Kx*SHx)` becomes `My = -sgn(FNOMIN)*R0*(SVx + Kx*SHx)`. SVx, Kx and SHx must all stay invariant for Fx, so My cannot flip by coefficients alone; without this factor, A gives My exactly the wrong sign (relative residual 2.0 in every shift-form configuration). The QSY form needs no change because its explicit Fz factor carries the sign. The shift form's selector (QSY1 == 0 and QSY2 == 0) is invariant under the flips.

**Ky = 0 caveat.** `add_epsilon(0) = +eps` on both sides of a transform, so the identity holds only where the guarded denominators are non-zero. The case that matters is Ky = 0 (PKY1 = 0, LKY = 0, or PKY3*abs(gy) = 1) with SVy != 0: the term SVy/add_epsilon(Ky) cannot transform. It appears in alpha_r (used by Mzr) and in alpha_c of the PAC2002 friction ellipse. Measured residuals:

- PAC2002 friction ellipse, combined: up to 2.6e4 N in Fx, with Mz and My of order 1e3 to 1e4, at any SR.
- PAC2002 cosine and MF 5.2, combined: depends on SR. When the kappa term dominates (|SR| >= about 1e-2) the residual is exactly 0. At |SR| <= 1e-12 (including SR = 0) it reaches 2.0e3 N*m in Mz; 1.8 to 16 at 1e-9; 1e-7 to 3e-4 at 1e-6; 4e-13 to 2e-11 at 1e-4.
- Pure slip, every variant: up to 2.0e-3 N*m in Mz (it grows when |SVy| is small).
- Exactly 0 when SVy == 0. Dy == 0 under A leaves at most 7.4e-13 N*m (rounding); Cy == 0, Kx == 0 and Cx*Dx == 0 leave exactly 0.

None needs a guard: a model with Ky = 0 is degenerate, and a sign-aware eps cannot fix it because the sign hint would have to come from PKY1 = 0 itself. The property test draws Ky != 0, and the identification makes readings with a negligible derivative abstain.

Other code that assumed Fz > 0 (default FZ in the web app, the load-side warning wording, FZ sweeps) uses FNOMIN*LFZO with its sign.

## 6. Gauge symmetries and equivalent alternatives

Alternatives that are exact but not canonical:

- A: flip LMUX instead of {PDX1, PDX2, PVX1, PVX2}; LKX instead of {PKX1, PKX2}; LS instead of SSZ1..SSZ4; LMX instead of QSX1..QSX3. Not adopted: scaling factors are user tuning knobs. Never flip LMY (My must flip).
- B: QBZ9 instead of QBZ10; PKY2 instead of PKY1. LKY is not a valid substitute for PKY1 because LKY also scales Bt and Br.
- Canonical choice: the two 15-coefficient tables above, with no L* ever flipped, FNOMIN carrying the vertical sign and PKY1 the lateral sign.

Built-in gauge symmetries (free sign choices of the model itself): any one of LCX, PCX1, LGAX (PAC2002 only; MF 5.2 forces it to 1), LCY, PCY1, LXAL, LYKA, RBX1, RBX2, RCX1, RBY1, RBY2, RCY1 or RVY4 can be flipped on top of either table and the residual stays exactly 0. Reasons: under the sine MF, sin(C*atan(B*x - E(B*x - atan(B*x)))) with D = mu*Fz is invariant under (B, C) -> (-B, -C) as a whole since B = K/(C*D); the cosine weighting functions are even; gamma_x appears only squared.

**Raw coefficient signs are never evidence of the coordinate system.** That includes PCY1, PKY1 alone, RBY1 and RCY1. The sign of Ky comes from the product PKY1*PKY2*LKY*FNOMIN and the sign of Cy is free. The identification uses evaluated outputs only.

## 7. Identification

`tire_model::find_coordinate_system` identifies the two axes separately from the evaluated behaviour, through the public evaluation path, at several loads (0.5, 1 and 1.5 times F'z0 = FNOMIN*LFZO, with the sign of FNOMIN), IA = 0 unless stated. It never throws and never changes the evaluation.

### 7.1 Axes of the indicators

Which readings flip under which transform decides the axis they can vote on.

| Indicator | Flips under A | Flips under B | Axis | Note |
|---|---|---|---|---|
| FZ0 = sign(FNOMIN*LFZO) | yes | no | VERTICAL | the loaded side of the equations |
| dFy/dIA (camber thrust) | yes | no | VERTICAL | independent of the SA convention; a car tire has dFy/dIA < 0 with z up (conventions.md section 3) |
| My at forward rolling | yes | no | VERTICAL | My < 0 with z up (conventions.md section 4); weak |
| dFy/dSA at zero slip | yes | yes | LATERAL | central difference |
| Fy secant at 1, 3, 6 deg | yes | yes | LATERAL | (Fy(+a) - Fy(-a))/2a is the odd part, immune to ply-steer and conicity offsets |
| Fy odd part at 20 deg | yes | yes | LATERAL | the odd part, not the one-sided value |
| dMz/dSA | yes | yes | LATERAL | positive pneumatic trail: opposite sign to dFy/dSA. sign(dMz/dSA * dFy/dSA) is invariant under both transforms, so this is correlated with dFy/dSA, not independent of it |
| dFx/dSR, Fx secant 5 % | no | no | CHECK | invariant under both transforms: cannot vote, and no conversion can change it. A negative value means the SR convention or PKX1 is inconsistent with FNOMIN |

Details:

- **dFy/dIA is vertical.** It does not flip under B, so it carries no information about the SA convention. In the repository files it is exactly 0 (abstains) except in `pac2002_S_iso.tir` and `pac2002_S_sae.tir` (PVY3 = -0.1; +-400 N/rad), because PVY3 = PVY4 = PHY3 = 0 in fixture S.
- **The large-slip indicator uses the odd part.** The one-sided sign of Fy(+a) flips under B in only 61-81% of random sets (a = 0.2 to 0.6 rad) because SHy and SVy offsets shift it. The odd part (Fy(+a) - Fy(-a))/2 flips 100% under A and under B. Its sign equals sign(Ky) in 100% of sets with abs(Cy) < 2 and no lateral shifts, but only 86-94% with wide random shifts and 64-91% with abs(Cy) >= 2 (C*atan passes pi). It is therefore a correlated lateral vote with a moderate weight, not an independent one.
- **dFx/dSR is a CHECK only.** It flipped in 0% of 800 random evaluations under A and B; sgn(FNOMIN)*sgn(dFx/dSR) adds nothing beyond sign(FNOMIN). A negative value produces the warning "SR convention or PKX1 sign inconsistent with FNOMIN" and is never mapped to a transform.
- **Per-dimension thresholds.** A reading abstains when |value| / |probe Fz| is at or below a threshold per indicator, with force slopes, moment slopes and My on different scales. In the repository files (|F'z0| = 4000 N, R0 = 0.3 m) the raw values are dFy/dSA = -5.54e4 N/rad, Fy secant at 3 deg = -4.69e4 N/rad, dMz/dSA = +1.66e3 N*m/rad, dFy/dIA = 0 or +-400 N/rad, dFx/dSR = +8.0e4 N, My = -12 N*m (= -R0*Fz*QSY1); normalised by |F'z0| the force slopes are about 12 to 20 per unit slip, dMz/dSA is 0.4 and My is 0.003. My therefore has its own, much smaller threshold and a low weight.
- **Correlated lateral evidence.** Every lateral reading flips together under A and under B: they are correlated evidence about one sign. The majority guards against offset or shape artefacts and never counts as independent confirmation.
- **Probes in a clamped state are skipped.** A reading whose probe rows set a clamp flag (outputs named `*_CLAMPED`) is skipped, so the evaluated point is the requested one. BELOW_VXLOW is not a clamp.

### 7.2 Constants and vote design

| Name | Axis | Form | Probe | Weight | Threshold | Polarity |
|---|---|---|---|---|---|---|
| FZ0 | VERTICAL | input level | FZ | 1.0 | 0.5 | +1 |
| dFy/dIA | VERTICAL | central difference | IA -> FY, step 1e-3 | 0.25 | 1e-3 | -1 |
| My | VERTICAL | output level | MY | 0.25 | 1e-6 | -1 |
| dFy/dSA | LATERAL | central difference | SA -> FY, step 1e-4 | 1.0 | 1e-3 | +1 |
| Fy secant 1, 3, 6 deg | LATERAL | central difference | SA -> FY, step a | 1/3 each | 1e-3 | +1 |
| Fy odd part 20 deg | LATERAL | central difference | SA -> FY, step 20 deg | 0.5 | 1e-3 | +1 |
| dMz/dSA | LATERAL | central difference | SA -> MZ, step 1e-4 | 0.5 | 1e-5 | -1 |
| dFx/dSR | CHECK | central difference | SR -> FX, step 1e-4 | 0 | 1e-3 | +1 |
| Fx secant 5 % | CHECK | central difference | SR -> FX, step 0.05 | 0 | 1e-3 | +1 |

Swept indicators have one reading per load, the weight shared evenly. The vote of a valid reading is polarity*sign(value); +1 is the ISO side. An axis has total weight W = W+ + W-, confidence max(W+, W-)/W, and is decided when W >= 0.25 and confidence >= 0.6.

- **Weight design.** The lateral axis totals 3.0. The vertical axis totals 1.5, of which FZ0 is 1.0 (1.0/1.5 = 0.667, above the 0.6 confidence), so FZ0 decides the vertical axis: FNOMIN fixes the loaded side of the equations, and the other vertical indicators warn on dissent.
- **Fallback for an undetermined axis:** it takes the other axis's decided sign; ISO when both are undetermined. This avoids a spurious MIXED from a missing axis.
- **Classification:** (+,+) ISO, (-,-) SAE, otherwise MIXED. MIXED carries a warning and a recommended conversion.
- **Warnings:** an undetermined axis (insufficient evidence, or conflicting evidence below the confidence), dissenting indicators named, MIXED, and a failed CHECK.
- **Metamorphic expectations:** for plausible ISO sets, identify(c) = ISO; identify(A(c)) = SAE; identify(B(c)) = MIXED(+,-); identify(A(B(c))) = MIXED(-,+); under A every VERTICAL and LATERAL reading is negated and CHECK readings are unchanged; under B LATERAL readings are negated and VERTICAL and CHECK readings are unchanged.

## 8. Expected results per repository file

| File | Result |
|---|---|
| `pac2002_S.tir`, `mf52_S52.tir`, `mf52_user_pff.tir`, `lexical_mix.tir`, `pac2002_S_ell.tir`, `pac2002_S_rng.tir` | MIXED: vertical ISO (FZ0 and My vote ISO; dFy/dIA abstains because it is exactly 0), lateral SAE (every lateral vote is SAE), both axes decided with confidence 1.00; one mixed warning |
| `pac2002_S_iso.tir` | ISO, confidence 1.00 on both axes, no warnings (dFy/dIA votes ISO) |
| `pac2002_S_sae.tir` | SAE, confidence 1.00 on both axes, no warnings |
| `pac2002_S_part.tir`, `mf61_unsupported.tir` | load errors (a missing RHX1; an unsupported model), not identification fixtures |

The fixture S family has PKY1 = -15 with FNOMIN = 4000, so s_z = + and s_y = sign(PKY1*PKY2*FNOMIN*LKY) = -. B alone converts them to ISO, and A then B converts them to SAE; the property holds exactly for all of them. `pac2002_S_iso.tir` is `pac2002_S.tir` with the LATERAL table applied and PVY3 = -0.1; `pac2002_S_sae.tir` is that file with the VERTICAL table applied.

## 9. Numerical verification

Models were built through `create` and evaluated with the public `evaluate_batch`. Coefficient sets had every coefficient non-zero with random signs: L* in [0.7, 1.3], FNOMIN in [2000, 6000] N, R0 in [0.25, 0.4] m, PKX1 in +-[15, 30], PKY1 in +-[10, 25], PKY2 in +-[1.2, 2.5], other coefficients in +-[0.02, 0.6]. Operating points were combined slip: Fz = +-[0.3, 1.8]*|F'z0| on the FNOMIN side, SA in +-[0.003, 0.3] rad, SR in +-[0.003, 0.3], IA in +-[0.002, 0.1] rad, Vx in [5, 40] m/s. Ranged runs used finite random FZ/KPU/ALP/CAM ranges and VXLOW in [1, 10], with about 72% of rows clamped, Fz on the wrong side on every 7th row, Fz = 0 on every 11th and Vx = 0.5 on every 13th.

| Check | Result |
|---|---|
| A and B, 5 variants (PAC2002 cosine, ellipse, cosine + shift My, ellipse + shift My, MF 5.2) x pure/combined x ranges/no ranges x start side, 300 sets x 300 points | residual 0 (abs and rel) in FX, FY, MZ, MX, MY and the flags |
| A, B and A then B, wider signs (random signs on PCY1, PCX1, QCZ1, RCX1, RCY1, LMUY; signed L*; log-uniform magnitudes; Fz ranges crossing zero; exact zeros in SA, SR, IA), both evaluation paths | residual 0; involution failures 0; commutation failures 0 |
| flip_T(flip_T(c)) == c, ranges included | bit-identical |
| the loadable repository `.tir` files, A, B and A then B | residual 0 |
| shift-form My without the sgn(FNOMIN) factor | My wrong by exactly a factor -1 (relative residual 2.0) |
| B with QBZ9 instead of QBZ10 | residual 0 (equivalent alternative) |
| Ky = 0 | see section 5 |

The residual is exactly 0 on the measured platform (LLVM 21, NEON): every operation is either sign-symmetric in IEEE-754 (negation, abs, even and odd libm functions at exactly negated arguments) or receives bit-identical operands. Other libm and SIMD back ends may differ in the last bits, so the repository property test (`tests/unit/coordinate_conversion_test.cpp`) compares forces and moments at a 1e-12 scaled relative tolerance and compares the clamp flags and the coefficient involution exactly. The test draws at least 50 coefficient sets x 200 points per configuration, with Ky != 0.

## 10. Sources

Located via search. Entries marked (read) were read directly; the others are snippets or unread.

- [S1] (read) Simcenter Tire 2306 Third Party Manual (MF-Tyre/MF-Swift), Conventions, Axis System, printed p. 25: ISO sign convention (Figure 8, image not extracted), kappa = -Vsx/Vx (eq. 1), tan(alpha) = Vsy/Vx (eq. 2); Tire Model Output: wheel-carrier frame, origin at wheel centre, z up. Mirror: 2024.help.altair.com/2024/hwdesktop/hwx/topics/motionview/MFTyre-MFSwift_Help.pdf
- [S2] (read) Altair coordinate systems and output requests (SAE: x forward, y right, z down; Tydex-W: Z parallel to road normal, Y = Z x X; Tydex-C: Y to the vehicle's left): help.altair.com/hwdesktop/hwx/topics/motionview/coordinate_systems_and_output_request_r.htm
- [S3] (read) MathWorks tireData.coordinateTransform and CoordinateSystem property ("ISO", "SAE", "H"; measurement data, not `.tir`): mathworks.com/help/vdynblks/ug/tiredata.coordinatetransform.html
- [S4] (read, secondary copy) PAC2002 Magic-Formula Tire Model documentation, pdfcoffee.com/pac2002-magic-formula-tire-model-pdf-free.html (Axis Systems p. 4: both axis systems have ISO orientation, W-axis origin is the road contact point; Units p. 5: must be valid for the TYDEX W-axis system; Camber p. 45; Left and Right Side Tires pp. 41-42: TYRESIDE). Unofficial mirror: check against the original Adams document.
- [S5] Adams PAC2002 documents and "Using Pacejka '89 and '94" (PAC89 modified SAE, PAC94 SAE, PAC2002 TYDEX W; positive inclination gives negative lateral force for car tires): pdfcoffee PAC2002 copy; scribd.com/document/74263327.
- [S6] Pacejka, Tyre and Vehicle Dynamics, 2nd ed., Ch. 1 and Appendix 1 (SAE J670e axes, slip-angle sign opposite, Fz positive upward). Third edition not checked (open).
- [S7] racer.nl Pacejka reference: lists SAE, adapted SAE, ISO, adapted ISO: racer.nl/reference/pacejka.htm
- [S8] SAE J670-2008 (Z-up and Z-down; comparison with ISO 8855).
- [S9] x-engineer, SAE tire axis system terminology (positive slip angle gives negative lateral force).
- [S10] Cal State FSAE tire thesis (TTC/J2047: left steer gives SA > 0, Fy < 0, Mz > 0; positive slip ratio gives tractive force); sumemura.jp sign-convention note.
- [S11] Camber thrust direction (PAC2002 documentation [S4] read for the car-tire sign): PoliTo lateral tire thesis; WashU FSAE tire modelling paper.
- [S12] Derivation: TYRESIDE mirroring per conventions.md section 9.
- [S13] (read) Functionbay, Axis systems and units, functionbay.com/documentation/onlinehelp/Documents/axissystemsandunits.htm: MF-Tire/MF-Swift 6.1 uses the ISO sign conventions; kappa = -1 is braking at wheel lock.
- [S15] (read via summary) Wikipedia, Slip ratio, en.wikipedia.org/wiki/Slip_ratio, citing SAE J670 (2008): slip ratio % = (1 - Omega*Rc/V)*100%.
- ISO 8855:2011 (side-slip note: negative slip angles with positive lateral forces) and TYDEX 1.3 sections 2.6-2.7: not extracted.

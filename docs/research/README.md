# docs/research: index and findings

These notes define the PAC2002, MF 5.2 and MF 6.1 tire models as this project implements them: equations, conventions, the `.tir` format, model routing, project rules, test data and validation. They are self-contained; apart from characteristics.md, coordinate-systems.md and mf61.md, which carry external source provenance, they refer only to files in this repository. The evaluation orders in pac2002.md section 2 and mf52.md section 4 are authoritative. MF 5.2 shares the PAC2002 forms except the differences listed in pac2002-vs-mf52.md. Where a note and policies.md differ, policies.md is what the code implements.

## Findings

Each row summarises a rule; Details names the note that defines it.

| Item | Rule | Details |
|---|---|---|
| FITTYP meanings | 5 = MF 5.0/5.1; 6 = MF 5.2; 21 = MF-Swift 1.x (based on MF 5.2); 51/52 = MF-MCTyre 1.0/1.1; 60/61/62 = MF 6.0/6.1/6.2; 70 = MF 6.2 with the temperature and velocity extension | model-routing.md |
| Routing | FITTYP 61 selects MF 6.1; 60, 62 and 70 remain unsupported | model-routing.md; policies.md section 3 |
| Range clamp | Fz, kappa, alpha and gamma are limited to [*_RANGE] before evaluation, including raising Fz to FZMIN; one warning per variable; clamp only when both limits of a pair exist | conventions.md section 8; policies.md section 1 |
| Unloaded (Fz*sgn(FNOMIN) <= 0) | all outputs 0, no evaluation | policies.md section 1 |
| alpha_eq | alpha_x,eq = atan(sqrt(tan^2 a + (Kx/Ky)^2 kappa^2)) * sgn+(a), for a = alpha_t, alpha_r; Kx from the pure Fx0 block, Ky from the pure Fy0 block (with the camber reduction); eps-guarded | pac2002.md sections 3 and 5.2 |
| Friction-ellipse signs | ratios use \|mu_act\|, so each force keeps the sign of its pure-slip value | pac2002.md section 5.4 |
| Combined-method choice | all 20 R* keys present: cosine; none: friction ellipse; partial set: load error naming the missing keys; FE_METHOD ignored with a warning | pac2002.md section 5.5; policies.md section 3 |
| Ellipse-branch Mz | combined Mz equations with SVyk = 0, including s*Fx | pac2002.md section 5.4 |
| Moment arm s (both models) | R0*(SSZ1 + SSZ2*Fy/FNOMIN + (SSZ3 + SSZ4*dfz)*gamma)*LS; Fy is the combined output, gamma raw | pac2002.md section 5.1 |
| SVx placement | SVx outside the sine, like SVy in Fy0 | pac2002.md section 4 |
| My | QSY form with QSY2 on the Fx term, times LMY, Vref = LONGVL; My_out = -(QSY-form value); SHIFTS form for FITTYP 5 with QSY1 = QSY2 = 0 | pac2002.md sections 3-4; conventions.md section 4 |
| VXLOW | not applied; rows flagged; CLI warning when Vx < VXLOW | conventions.md section 5; policies.md section 1 |
| .tir lexical rules, [UNITS] | see tir-format.md | tir-format.md; policies.md sections 3-4 |
| MF 5.2 parameter set | as listed; no LGAX, LGAY, LGAZ | mf52.md section 3 |
| MF 5.2 equations | alpha* = alpha, gamma* = gamma, turn slip off | mf52.md section 4 |
| F'y and pure Mz0 | PAC2002: F'y = Fy - SVyk at the actual gamma; Mz0 = -t*Fy0(actual gamma) + Mzr. MF 5.2: F'y = Fy - SVyk0 | pac2002.md section 5.3; mf52.md section 4 |
| E <= 1 | Ex, Ey, Et, Exa and Eyk are clamped to <= 1 | pac2002.md section 4 |
| mu_y, Ky | (1 - PDY3*gamma_y^2); Ky0 with LFZO and LKY | pac2002.md section 3 |
| Scaling in Dr | LRES on the residual term, LMUY on the whole Dr | pac2002.md sections 3-4 |
| Mz_gyr | 0 in steady state (the term contains a time derivative) | pac2002.md section 2 |
| sgn and eps | sgn+(x) = +1 for x >= 0, else -1; eps = 1e-6 added sign-preserving to Cx*Dx, Cy*Dy, Ky (in SHf and Kx/Ky), Kx and Ky in the friction ellipse, and (MF 5.2) Vc | conventions.md section 7; policies.md section 2 |
| Axes | ISO (z up, y left) or SAE (z down, y right), origin at the contact point; the equations never convert | conventions.md section 1; coordinate-systems.md |
| Trail normaliser | PAC2002: R0/FNOMIN. MF 5.2: R0/F'z0 with t multiplied by LFZO, which equals R0/FNOMIN | pac2002.md section 5.6 |
| Reuse in combined Mz | Et (from the pure alpha_t), Bt, Ct, Dt, Br, Cr and Dr are reused unchanged; the combined Mz equations redefine none of them | pac2002.md section 3 |
| Pressure | used by MF 6.1; ignored by PAC2002 and MF 5.2 with one warning | mf61.md; policies.md sections 5 and 10 |
| Uncombined mode | Fx = Fx0, Fy = Fy0, Mz = Mz0 | conventions.md section 10; policies.md section 1 |
| Vx | Vx < 0 rejected; PAC2002 accepts Vx = 0; MF 5.2 requires Vx > 0 | conventions.md section 5; policies.md section 1 |
| Characteristics (tangent and secant stiffness, DY, DX, peak slips) | eight channels per point in a separate vector; the cited meaning of BCD, K and D, and the project definitions of the tangent and the secant | characteristics.md section 1; policies.md section 9 |
| Stiffness parameters vs tangents | the slope of the pure curve at the shifted origin is K*abs(CD)/(abs(CD) + eps), not exactly K; MFeval Kxk/Kya semantics are not determined, so no parity is claimed | characteristics.md section 1 |
| Derivative method | forward-mode differentiation (dual numbers) of the closed-form kernels, exact chain rule, no step size; the closed forms are the oracles | characteristics.md sections 2 and 3 |
| Ellipse derivatives | Fz-free form; at a genuine kink the right derivative; at a 0/0 ratio the value is 1 and the force derivative is F0' times the directional limit of the ratio | characteristics.md sections 2.3-2.5 |
| Peak condition | cos(C*theta) = 0, abs(phi) = tan(pi/(2C)), needs C > 1; E = (abs(u) - T)/(abs(u) - atan(abs(u))); existence conditions and the agreement with the search | characteristics.md section 4 |
| Peak search | first stationary point from 0 on the side of the row's slip, 32-cell scan plus 52 bisections, NaN when none | characteristics.md section 5; policies.md section 9 |
| Characteristic sign parity | DFY_DSA, FY_OVER_SA: VERTICAL and LATERAL; DY: VERTICAL; SA_AT_PEAK_FY: LATERAL (SA != 0); the others none | characteristics.md section 6 |
| Zero-slip secant | at raw slip +-0 the secant is the row's tangent; rows without contact give +0; NaN only in the two peak channels | characteristics.md section 7; policies.md section 9 |

## Files

| File | Content |
|---|---|
| conventions.md | cross-model rules: axes, slip, signs, speed, sgn/eps, limits, TYRESIDE, modes, pressure |
| coordinate-systems.md | the user's ISO and SAE definitions, TNO ISO-W context, conversion by coefficient sign flips (transforms A and B, flip tables, code generalisations, gauge symmetries), robust multi-indicator identification, expected results per fixture, numerical verification |
| characteristics.md | the eight characteristics: definitions, closed-form derivatives (pure, cosine, friction ellipse with kinks and 0/0 points), evaluation method, peak condition and existence, peak search, sign parity, edge cases, sources |
| pac2002.md | PAC2002 evaluation order and equations (authoritative), equation choices, rule details |
| mf61.md | MF 6.1 equations, pressure and turn-slip corrections, radii, coefficients, numerical policies and source-port corrections |
| mf52.md | MF 5.2 status and settings, parameter set, evaluation order and equations (authoritative), behaviours not adopted |
| pac2002-vs-mf52.md | difference table |
| model-routing.md | FITTYP / PROPERTY_FILE_FORMAT routing |
| tir-format.md | lexical rules, sections, [UNITS], required keys and defaults |
| test-data.md | data policy, fixture S, vectors, property tests, parser and routing fixtures, characteristic vectors and CLI rows |
| policies.md | project rules: pipeline, numerics, loading, lexical refinements, CLI, test data, items that may change |
| open-questions.md | open items and settled points |
| validation.md | validation summary: comparisons, method and tolerances, results, differences and dispositions, open items |

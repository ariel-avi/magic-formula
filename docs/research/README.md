# docs/research: index, source register, decisions

Revision 4, 2026-10-18 (researcher). Supersedes revision 2 and the revision-3 draft (neither is kept in the repository).

Revision-2 text was re-read through targeted project-explorer extractions: pac2002 eqs (16)-(85) and their notes, conventions, open-questions, tir-format, test-data and mf52. The rev-3 [CARRY-OVER] markers and the 'no web source read' status are therefore removed. Where rev 2 and rev 3 disagreed, the decision table below settles it; the column 'Was' points to the earlier item. Old IDs map to new ones in open-questions.md section 2.

## Tags

| Tag | Meaning |
|---|---|
| (citation only) | Stated in a read primary source (SIM20, MSC) at the cited page/eq. |
| [derived] | Follows from cited facts. The reasoning is given inline. |
| [impl] | Observed only in an implementation (file:line at a pinned version). Never authoritative on its own. |
| [policy] | Project decision where the sources are silent. The architect may change it and must record the change here. |
| [lead] | Unverified snippet. Not evidence. |

Precedence: primary TNO/Siemens/MSC > peer-reviewed > implementations. Among implementations: MFEVAL (it states it was tuned to match TNO) > OPENTIRE, CHRONO > TEASIT, MFJL > CHRONO8.

## Source register

| ID | Source | Version / commit | Read status | Licence / use |
|---|---|---|---|---|
| SIM20 | Siemens Simcenter MF-Tyre/MF-Swift 2020.2 Help (user-supplied PDF) | 2020.2 | READ PDF 2-48. Cite as p.<printed> [PDF n]; printed = PDF - 4 on body pages | (c) 2020 Siemens, 'All rights reserved' (PDF 2). Cite and paraphrase only; never transcribe tables. |
| MSC | MSC.Software, 'Using the PAC2002 Tire Model' (user-supplied PDF), 54 pp | example header 2004 (p.48) | READ in full (rev 2). Rev 4 re-checked pp.4-5, 13-34, 36 and 40-52 visually. | No licence statement. Cite equations; keep quotes short. |
| MFEVAL | M. Furlan Tassara, MFeval (MATLAB), bundled in mathworks/Simscape-Vehicle-Templates @2c303195, `Libraries/Vehicle/Tire/MFEval/MFeval_4p0` | MFeval 4.0.0 (Contents.m, 17-Mar-2020); wrapper repo mathworks/Simscape-Vehicle-Templates HEAD 2c303195 | READ: `+mfeval/Solver.m` l.167-1640 and 2036-2072; `readTIR.m` in full; `mfeval.m` l.100-139; `doc/changesEquations.html`, `releaseNotes.html`, `equationSource.html`. Executed in GNU Octave 10.3 during the validation (validation.md): 190 output rows for our synthetic fixture are in `tests/data/reference/mf52_mfeval.csv`; MFeval code, adapter and example `.tir` are not in the tree. | `mfeval_license.rights`: BSD-style, 3 conditions, holder name blank. The wrapper's `LICENSE.md` adds 'licensed to you solely for use in conjunction with MathWorks products'. No MFeval code is copied. Numbers it computed for our own fixture are committed provisionally as reference data; which licence applies to them (BSD-style file vs the wrapper's MathWorks-only clause) is a user decision (Q-07), still open. [impl] |
| OPENTIRE | opentire/opentire-python `PAC2002.py`, `PAC2002_Core.py` ('Pacejka 2002 as described in the First Edition', PAC2002.py:15) | 6652c490 | READ: combined Mz, Ky, mu_y, clamps, Mx, My | MIT. [impl] |
| CHRONO | projectchrono/chrono `ChPac02Tire.{cpp,h}` | 39393e39 | READ and verified | BSD-3. Bundled .tir files are not clearly covered. [impl] |
| CHRONO8 | `ChPacejkaTire.cpp` ('Chrono 8.0.0' copy) | not in the 39393e3 tree; provenance unverified | READ: s, alpha_eq, Ky, mu_y, camber | BSD-style header. Lowest weight. [impl] |
| TEASIT | teasit/magic-formula-tyre-library | aeae697 (v2.0.4) | READ. Implements MF 6.1.2 only. | GPL-3: facts only. [impl] |
| MFJL | matheusft/MFeval_julia | a3b3ba4 | partial; superseded wherever MFEVAL was read | MIT. [impl] |
| REV2 | prior notes (revision 2, not kept in the repository) | rev 2 | READ (targeted) | internal |
| TNO62, TNO61, BSP10, ALTAIR-FB, PAC12 | TNO 6.2/6.1 manuals, Besselink et al. 2010, Altair/FunctionBay pages, Pacejka 2012 | - | NOT READ (proxy 403 / DNS failure) | - |
| MF52-EQM | 'MF52 EquationManual' reposts | - | EXCLUDED pending the user's licence decision | - |


## Evidence tier per model

- **PAC2002:** primary (MSC) for every equation except the moment arm s, the VXLOW law and mirroring. Printed defects are repaired with [derived] reasoning plus agreement between implementations.
- **MF 5.2:** identification and parameter set are primary (SIM20). **Every equation is [impl]**: the MFEVAL FITTYP-6 path, commented 'MF5.2 equations from the MF-Tyre equation manual'. The user accepted that tier (AD-26, Q-01 closed in open-questions.md). Each MF 5.2 form equals an MSC PAC2002 form except the items in pac2002-vs-mf52.md.

## Decisions (rev-4 numbering, D-xx)

D-xx are the researcher's decisions and recommendations. The architect's sign-off of every [policy] item, and the changes it made to some D-xx items, are the AD-xx table in decisions.md; where an AD-xx supersedes a D-xx, decisions.md wins.

| D | Item | Decision | Basis | Conf. | Was |
|---|---|---|---|---|---|
| D-01 | FITTYP meanings | 5 = MF-Tyre 5.0/5.1; 6 = MF-Tyre 5.2; 21 = MF-Swift 1.x (based on 5.2); 51/52 = MF-MCTyre 1.0/1.1; 60/61/62 = MF-Tyre 6.0/6.1/6.2; 70 = 6.2 + T&V. 2020.2 accepts only 6, 60, 61, 62, 70 | SIM20 p.36-37 [PDF 40-41] | high | rev-3 S1, OQ-21; rev-2 OQ-V1 |
| D-02 | Routing | see model-routing.md | SIM20 p.36-37; MSC p.2, p.33, p.44 [policy] | medium | S2 |
| D-03 | Range clamp | Fz, kappa, alpha, gamma are limited to [*_RANGE] before MF evaluation, including raising Fz to FZMIN. Warn on first exceedance. Clamp only when both limits of a pair exist | MSC p.45; SIM20 p.24-25. MFEVAL does not raise Fz to FZMIN [impl]; the primary sources win | high (rule) | S3; rev-2 OQ-6, OQ-26 |
| D-04 | Fz <= 0 | all outputs 0, no evaluation | [policy]; MFEVAL sets Fz<0 to 0 and gets zero outputs at Fz = 0 (Solver.m:282, 784, 1069) [impl] | medium | S4 |
| D-05 | alpha_eq | alpha_x,eq = atan(sqrt(tan^2 a + (Kx/Ky)^2 kappa^2)) * sgn+(a), for a = alpha_t, alpha_r. Kx = eq (25); Ky = eq (38), eps-guarded | MSC (84)-(85) repaired [derived]; MFEVAL :1581-1582, OPENTIRE :386, :405, TEASIT [impl] | med-high | S5; rev-2 B2 |
| D-06 | Ellipse signs | rewrite with abs(mu_act) | MSC p.33-34 + [derived]; CHRONO [impl] | high | S6; rev-2 B4 |
| D-07 | Combined-method choice | all 20 R* keys present -> cosine; none present -> ellipse; partial set -> load error naming the missing keys. FE_METHOD is ignored with a warning | MSC p.33 (the two end states); partial set and FE_METHOD are [policy] | medium | keeps rev-2 section 7; replaces rev-3 B8 |
| D-08 | Ellipse-branch Mz | eqs (76)-(85) with S_Vyk = 0, including s*Fx | MSC p.34 | high | S8 |
| D-09 | Moment arm s (both models) | R0*(SSZ1 + SSZ2*Fy/FNOMIN + (SSZ3 + SSZ4*dfz)*gamma)*LS; Fy = combined output, gamma raw | MSC p.17, p.19, p.32 descriptions; MFEVAL :1583 [impl]. OPENTIRE and CHRONO use F'z0 | low-med | rev-3 S9 had F'z0 (changed) |
| D-10 | Eq (18) | S_Vx outside the sine | MSC eq (30) analogue; all implementations | high | S10 |
| D-11 | My | eq (87) with QSY2 on the Fx term, times LMY, Vref = LONGVL. Output My_out = -(MSC value) | MSC p.18, p.32-33; MFEVAL :1506-1508 (FITTYP 6 has a leading minus); TEASIT, CHRONO agree on the sign; OPENTIRE keeps + | form high, sign medium | S11; rev-2 OQ-4, OQ-10 |
| D-12 | VXLOW | not applied; CLI warning when Vx < VXLOW | no primary law (MSC p.45, SIM20 p.27); MFEVAL's law is TNO-style [impl] | policy | S12; rev-2 B3 |
| D-13 | .tir lexical rules, [UNITS] | see tir-format.md | SIM20 p.17, p.24; MSC p.48-52 [policy] | medium | S13 |
| D-14 | MF 5.2 parameter set | SIM20 matrix column 6. LGAX, LGAY and LGAZ occur nowhere in SIM20 | SIM20 p.26-35; text-layer grep of PDF 1-48 | high | S14; rev-3 OQ-4 |
| D-15 | MF 5.2 equations | mf52.md section 4 = MFEVAL FITTYP-6 path with alpha* = alpha, gamma* = gamma, turn slip off | MFEVAL [impl] | [impl] | rev-3 S15 ('not settled') |
| D-16 | F'y and pure Mz0 | PAC2002: F'y = Fy - SVyk at the actual gamma (reading A); Mz0 = -t*Fy0(actual gamma) + Mzr | MSC p.23 has no gamma = 0 mark on Fy0; RHS of (81); OPENTIRE :420, CHRONO :318 | medium | rev-3 B3 recommended (B): reversed. Rev-2 OQ-3, V17 |
| D-17 | E <= 1 | printed for Ex, Ey, Et, Exa, Eyk; implemented as a clamp | MSC (24), (36), (53), (64), (73); clamping is implementation consensus | high (limit printed); clamp is policy | rev-3 B11, OQ-10 |
| D-18 | mu_y, Ky | (1 - PDY3*gamma_y^2); Ky0 per (37), with LFZO and LKY | MSC (35), (37); OPENTIRE and MFEVAL agree; CHRONO's '+' sign and missing LKY are defects | high | rev-3 OQ-13 |
| D-19 | Scaling in eq (56) | lambda_r = LRES, lambda_mugamma = LMUY | MSC p.18; MFEVAL :1263; CHRONO :227-230 | med-high | rev-2 OQ-5 |
| D-20 | Mz_gyr | 0 in steady state | MSC (100) contains dv/dt [derived] | med-high | rev-3 OQ-8 |
| D-21 | sgn and eps | sgn+(x) = +1 for x >= 0, else -1. eps = 1e-6, added sign-preserving to Cx*Dx, Cy*Dy, Ky and (MF 5.2) Vc | [policy]; values per MFEVAL :252, :788-791, :1051-1054, :1142-1145 [impl] | policy | rev-3 conventions section 6 |
| D-22 | Axes | TYDEX W-axis, ISO orientation, origin at the contact point | MSC p.4, p.5, p.16, p.44, p.46-47; SIM20 p.16 Fig. 4.5 | high | rev-3 OQ-9 (axis part) |
| D-23 | Trail normaliser | PAC2002: R0/FNOMIN as printed in (52). MF 5.2: R0/F'z0 with t multiplied by LFZO, which is algebraically R0/FNOMIN | MSC (52); MFEVAL :1177, :1616 [derived] | med-high | rev-2 OQ-29 |
| D-24 | Reuse in combined Mz | E_t (from the pure alpha_t), B_t, C_t, D_t, B_r, C_r, D_r are reused unchanged | MSC (79)-(85) redefine none of them [derived]; MFEVAL :2044-2054, CHRONO [impl] | med-high | rev-2 section 10 'not printed' |
| D-25 | Pressure | ignored for both models, with one warning | MSC p.16, p.19; SIM20 matrix column 6, Fig. 4.9; MFEVAL release notes 4.0.0 | high | rev-2 OQ-22 |
| D-26 | Uncombined mode | Fx = Fx0, Fy = Fy0, Mz = Mz0 | MSC p.43; [policy] for MF 5.2 | medium | rev-2 OQ-16 |
| D-27 | Vx | Vx < 0 is rejected. PAC2002 accepts Vx = 0; MF 5.2 requires Vx > 0 | [policy]; MFEVAL's standstill path is not reproduced | policy | rev-3 conventions section 4 |

## Files

| File | Content |
|---|---|
| conventions.md | cross-model rules: axes, slip, signs, speed, sgn/eps, limits, TYRESIDE, modes, pressure |
| pac2002.md | authoritative PAC2002 evaluation order and equations, printed defects, decision details |
| mf52.md | MF 5.2 status, parameter set, authoritative evaluation order and equations ([impl]) |
| pac2002-vs-mf52.md | difference table |
| model-routing.md | FITTYP / PROPERTY_FILE_FORMAT routing |
| tir-format.md | lexical rules, sections, [UNITS], required keys and defaults |
| test-data.md | data licence status, fixture S, vectors, property tests |
| decisions.md | architect sign-off table (AD-01 to AD-28) for the [policy] items, AD-20 lexical refinements, items still open for primary evidence |
| open-questions.md | open items and the rev-2/rev-3 ID map |
| validation.md | oracle validation summary: oracles, grids, tolerances, results, mismatch classes and dispositions, open items |

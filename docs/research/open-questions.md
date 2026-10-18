# Open questions

Revision 4, updated with the architect's sign-off (AD-xx, decisions.md). Rev-4 IDs are Q-nn (open or closed) and D-nn (researcher decisions, see README). Old IDs are mapped in section 2.

## 1. Open items

| ID | Status | Question | Current position | What resolves it |
|---|---|---|---|---|
| Q-01 | CLOSED (AD-26) | MF 5.2 equations are [impl] only (MFEVAL). Accept this tier under repo rule 4? | Accepted by the user: implemented per mf52.md section 4, keeping the [impl] labels | closed. A primary source (TNO62, BSP10, PAC12, MF52-EQM) would still allow a validation pass |
| Q-02 | implemented per AD-04; open for primary evidence | Moment arm s normaliser, FNOMIN vs F'z0 (no primary) | FNOMIN (D-09); tests use LFZO = 1 | a primary MSC/TNO s equation |
| Q-03 | implemented per AD-05; open for primary evidence | VXLOW law; does TNO scale or flip My with the wheel rolling speed Vr = Vx(1+kappa)? (MFEVAL's My kappa band, Solver.m:1517-1537, mf52.md section 5) | not applied (D-12) | TNO62; an MSC Adams document with the formula |
| Q-05 | implemented per AD-06; open for primary evidence | TYRESIDE mirroring transform; SYMMETRIC | not implemented | no public primary source located |
| Q-06 | implemented per AD-07; open for primary evidence | FE_METHOD semantics | ignored with a warning | MSC Adams documentation of the key |
| Q-07 | PARTLY RESOLVED: Q-07a CLOSED (user, 2026-10-18, AD-29); the other items stay open (user) | Licences: MSC example values, CHRONO data, MFEVAL (BSD-style file vs the wrapper's MathWorks-only clause), MF52-EQM. Sub-item Q-07a: may numbers computed by MFeval 4.0.0 for our own fixture (`tests/data/reference/mf52_mfeval.csv`, 190 rows) be committed and used by `tests/module/mfeval_reference_test.cpp`? | **Q-07a resolved: yes.** User decision 2026-10-18: keep the MFeval outputs computed on our synthetic fixture; the BSD-3 notice (`mfeval_license.rights`) is retained in `tests/data/reference/README.md`. Facts only; no third-party code or `.tir` copied. The MSC example values, Chrono data and MF52-EQM stay out of the tree | the remaining Q-07 items need their own user decision (then record an AD entry) |
| Q-08 | implemented per AD-03; open for primary evidence | eps value: MFEVAL code 1e-6 vs its doc 1e-4 | 1e-6 | TNO source |
| Q-09 | implemented per AD-03; open for primary evidence | alpha_eq at exactly alpha_t = 0: MFEVAL sign(0) = 0 vs sgn+ | sgn+ (continuous) | user acceptance of the single-point deviation |
| Q-10 | implemented per AD-17; open for primary evidence | PAC2002 My output sign: MSC literal + vs ISO - | negative (D-11) | an MSC sign statement. Rev-2 cited 'braking/driving moment' on p.48; rev 4 did not re-check p.48, and that phrase is not on p.16 |
| Q-11 | implemented per D-22 and conventions.md section 3 (no AD); open for primary evidence | Positive gamma direction (no text in either manual) | ISO; consistent with MSC p.44 | TNO62 / TYDEX text |
| Q-12 | implemented per pac2002.md C3 and V16 (no AD); open for primary evidence | eq (23): gamma_x (C3) vs literal raw gamma | gamma_x; V16 pins it | an MSC erratum or the PAC 2002 book |
| Q-13 | implemented per AD-11; open for primary evidence | PROPERTY_FILE_FORMAT: allowed values and case | 'PAC2002', 'MF_05', and (observed in MFEVAL 4.0 examples, gab-s98 and simscape files) 'USER' and 'MF-TYRE', which route by FITTYP like an absent key; case-insensitive | ALTAIR-FB / MSC docs |
| Q-14 | CLOSED (AD-10) | [UNITS] aliases (m, N, rad) | SI only, no aliases: LENGTH 'meter', FORCE 'newton', ANGLE 'radians', MASS 'kg', TIME 'second', PRESSURE 'pascal'; anything else is UNSUPPORTED_UNIT (AD-10) | closed |
| Q-15 | implemented per AD-08; open for primary evidence | TNO estimation for reduced MF 5.2 files | not implemented; zeros with a warning | not documented anywhere |
| Q-16 | implemented per AD-13; open for primary evidence | LONGVL: checklist '1 m/s' vs example 16.6 | required iff QSY3 or QSY4 is non-zero; otherwise an internal placeholder (AD-13) | - |
| Q-17 | CLOSED (AD-11) | Opt-in support for FITTYP 5 / 21 under MF 5.2 | error, no opt-in (AD-11) | closed |
| Q-18 | RESOLVED [impl] | PKY4 default in MFEVAL readTIR | PKY4 = 2: MFEVAL `readTIR.m:200` sets PKY4 = 2 [impl]. The architect read this line directly; a researcher pass should confirm it | closed |
| Q-19 | RESOLVED [impl] | MFEVAL SHf line and Vcy definition | MFEVAL `Solver.m:648`, `:655-656`, `:680-682`: Vsy, Vcy = Vsy, Vc, Vc' = Vc + eps*sgn+(Vc), cos'(alpha) = Vcx/Vc'. `Solver.m:1142-1146`: Kya' = Kya + eps*sgn+(Kya); SHf = SHy + SVy/Kya'. The architect read these lines directly; a researcher pass should confirm them | closed |
| Q-20 | CLOSED (AD-21) | Extra outputs (t, Mzr, Kx, Ky, clamp flags) | none: outputs are Fx, Fy, Mz, Mx, My only (AD-21) | closed |
| Q-21 | open (lowest weight) | CHRONO8 ChPacejkaTire provenance | lowest weight | git history search |
| Q-22 | open (out of scope) | Out-of-scope printed typos (turn slip, (94), bottoming) | out of scope | - |

## 2. ID map

Rev 2:

| Old | Now |
|---|---|
| B1 | D-09, Q-02 |
| B2 | D-05 |
| B3 | D-12, Q-03 |
| B4 | D-06 |
| OQ-M52 | D-15, Q-01 |
| OQ-V1 | D-01 |
| OQ-1 | Q-12 |
| OQ-2 | D-10 |
| OQ-3 | D-16 |
| OQ-4 | D-11 (C7) |
| OQ-5 | D-19 |
| OQ-6, OQ-26 | D-03, D-04 |
| OQ-7 | D-27 |
| OQ-8 | Q-05 |
| OQ-9 | D-22, Q-11 |
| OQ-10 | Q-10 |
| OQ-11 | D-07 |
| OQ-12, OQ-23 | D-21 |
| OQ-13 (lexical) | D-13 |
| OQ-14 | Q-14 |
| OQ-15 | tir-format.md section 3 |
| OQ-16 | D-26 |
| OQ-17 | D-01, model-routing |
| OQ-18 (LYKA/QBZ text typos) | closed: the roles follow from the equations |
| OQ-19, OQ-20 (typos) | Q-22 |
| OQ-21 | closed in rev 2 |
| OQ-22 | D-25 |
| OQ-24 | Q-20 |
| OQ-25 | out of scope |
| OQ-27 | Q-16 |
| OQ-28 | D-08 |
| OQ-29 | D-23 |
| OQ-30 | closed: combined Mz is implementable |
| OQ-31 | closed: MFEVAL cites the Pacejka 3rd ed.; MSC cites the 2002 1st ed. |
| OQ-L1 | Q-07 |
| OQ-R1 | closed |

Rev 3:

| Old | Now |
|---|---|
| S1-S15 | D-01 to D-15 (S9 changed to FNOMIN) |
| OQ-0 | closed: rev-2 notes were read |
| OQ-1 | Q-01 |
| OQ-2 | D-16 (reversed to reading A) |
| OQ-3 | Q-02, D-23 |
| OQ-4 | D-14, Q-12 |
| OQ-5 | Q-03 |
| OQ-6 | D-04 |
| OQ-7 | D-07, Q-06 |
| OQ-8 | D-20 |
| OQ-9 | D-22, Q-10, Q-11 |
| OQ-10 | D-17 |
| OQ-11 | Q-08 |
| OQ-12 | D-15 |
| OQ-13 | D-18 |
| OQ-14 | Q-15 |
| OQ-15 | Q-13 |
| OQ-16 | Q-14 |
| OQ-17 | noted in tir-format.md |
| OQ-18 | Q-16 |
| OQ-19 | Q-17 |
| OQ-20 | Q-07 |
| OQ-21 | D-01 |
| OQ-22 | Q-05 |

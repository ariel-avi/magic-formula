# Open questions

## 1. Open items

| Status | Question | Current position | What resolves it |
|---|---|---|---|
| CLOSED (policies.md section 6) | MF 5.2 equations are [impl] only (MFEVAL). Accept this tier under repo rule 4? | Accepted by the user: implemented per mf52.md section 4, keeping the [impl] labels | closed. A primary source (TNO62, BSP10, PAC12, MF52-EQM) would still allow a validation pass |
| implemented per policies.md section 2; open for primary evidence | Moment arm s normaliser, FNOMIN vs F'z0 (no primary) | FNOMIN (pac2002.md section 5.1); tests use LFZO = 1 | a primary MSC/TNO s equation |
| implemented per policies.md section 1; open for primary evidence | VXLOW law; does TNO scale or flip My with the wheel rolling speed Vr = Vx(1+kappa)? (MFEVAL's My kappa band, Solver.m:1517-1537, mf52.md section 5) | not applied (conventions.md section 5) | TNO62; an MSC Adams document with the formula |
| implemented per policies.md section 3; open for primary evidence | TYRESIDE mirroring transform; SYMMETRIC | not implemented | no public primary source located |
| implemented per policies.md section 3; open for primary evidence | FE_METHOD semantics | ignored with a warning | MSC Adams documentation of the key |
| PARTLY RESOLVED: the MFeval-output item is closed (policies.md section 6); the other items stay open (user) | Licences: MSC example values, CHRONO data, MFEVAL (BSD-style file vs the wrapper's MathWorks-only clause), MF52-EQM. Sub-item: may numbers computed by MFeval 4.0.0 for our own fixture (`tests/data/reference/mf52_mfeval.csv`, 190 rows) be committed and used by `tests/module/mfeval_reference_test.cpp`? | **MFeval-output sub-item resolved: yes.** Keep the MFeval outputs computed on our synthetic fixture; the BSD-3 notice (`mfeval_license.rights`) is retained in `tests/data/reference/README.md`. Facts only; no third-party code or `.tir` copied. The MSC example values, Chrono data and MF52-EQM stay out of the tree | the remaining licence items need a separate decision by the user |
| implemented per policies.md section 2; open for primary evidence | eps value: MFEVAL code 1e-6 vs its doc 1e-4 | 1e-6 | TNO source |
| implemented per policies.md section 2; open for primary evidence | alpha_eq at exactly alpha_t = 0: MFEVAL sign(0) = 0 vs sgn+ | sgn+ (continuous) | user acceptance of the single-point deviation |
| implemented per policies.md section 2; open for primary evidence | PAC2002 My output sign: MSC literal + vs ISO - | negative (conventions.md section 4) | an MSC sign statement. An earlier reading cited 'braking/driving moment' on p.48; p.48 was not re-checked, and that phrase is not on p.16 |
| implemented per conventions.md sections 1 and 3; open for primary evidence | Positive gamma direction (no text in either manual) | ISO; consistent with MSC p.44 | TNO62 / TYDEX text |
| implemented per pac2002.md section 4 and V16; open for primary evidence | eq (23): gamma_x vs literal raw gamma | gamma_x; V16 pins it | an MSC erratum or the PAC 2002 book |
| implemented per policies.md section 3; open for primary evidence | PROPERTY_FILE_FORMAT: allowed values and case | 'PAC2002', 'MF_05', and (observed in MFEVAL 4.0 examples, gab-s98 and simscape files) 'USER' and 'MF-TYRE', which route by FITTYP like an absent key; case-insensitive | ALTAIR-FB / MSC docs |
| CLOSED (policies.md section 3) | [UNITS] aliases (m, N, rad) | SI only, no aliases: LENGTH 'meter', FORCE 'newton', ANGLE 'radians', MASS 'kg', TIME 'second', PRESSURE 'pascal'; anything else is UNSUPPORTED_UNIT | closed |
| implemented per policies.md section 3; open for primary evidence | TNO estimation for reduced MF 5.2 files | not implemented; zeros with a warning | not documented anywhere |
| implemented per policies.md section 3; open for primary evidence | LONGVL: checklist '1 m/s' vs example 16.6 | required iff QSY3 or QSY4 is non-zero; otherwise an internal placeholder | - |
| CLOSED (policies.md section 3) | Opt-in support for FITTYP 5 / 21 under MF 5.2 | error, no opt-in | closed |
| RESOLVED [impl] | PKY4 default in MFEVAL readTIR | PKY4 = 2: MFEVAL `readTIR.m:200` sets PKY4 = 2 [impl]. The architect read this line directly; a researcher pass should confirm it | closed |
| RESOLVED [impl] | MFEVAL SHf line and Vcy definition | MFEVAL `Solver.m:648`, `:655-656`, `:680-682`: Vsy, Vcy = Vsy, Vc, Vc' = Vc + eps*sgn+(Vc), cos'(alpha) = Vcx/Vc'. `Solver.m:1142-1146`: Kya' = Kya + eps*sgn+(Kya); SHf = SHy + SVy/Kya'. The architect read these lines directly; a researcher pass should confirm them | closed |
| CLOSED (policies.md section 1) | Extra outputs (t, Mzr, Kx, Ky, clamp flags) | none: outputs are Fx, Fy, Mz, Mx, My only | closed |
| open (lowest weight) | CHRONO8 ChPacejkaTire provenance | lowest weight | git history search |
| open (out of scope) | Out-of-scope printed typos (turn slip, (94), bottoming) | out of scope | - |

## 2. Settled points

- The text typos in LYKA and QBZ are settled by the equations: the roles of the coefficients follow from them.
- Combined Mz is implementable from the printed equations.
- MFEVAL cites the Pacejka 3rd edition, while MSC cites the 2002 1st edition.

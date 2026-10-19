# Validation against external oracles

Revision 2. Summary of the oracle validation of the PAC2002 and MF 5.2 implementations and of the `.tir` loader. Section 6 lists what stays open.

Revision 2 changes: PFF-USER and TIR-2, found during the validation, are now fixed in the code (section 5 names the code and the tests). The oracle numbers in sections 4.1 to 4.4 were measured **before** those two fixes and were **not re-run** afterwards (section 6). The MF 5.2 verdict is qualified for My (MFE-13). Data kept in the tree: `tests/data/reference/` (README there). The validation plan, adapter scripts, grids and raw outputs are not in the tree; they are cited here by name.

## 1. Verdict and limits

| Area | Oracle | Verdict |
|---|---|---|
| MF 5.2 (FITTYP 6) | MFeval 4.0.0 | PASS: all five channels match within the T1-MFE tolerance (also within 1e-10) on every CORE row, except My on the 513 MFE-13 CORE rows of the example file (215 rows on `mf52_S52.tir`), which are LIT-OPEN and not adopted (section 5) |
| PAC2002 | OpenTire (opentirepython, PAC2002) | PASS: every CORE row matches on all five channels within T1-OT, after the input-convention map |
| MF 5.2, limited cross-check | teasit (MF 6.1.2, 6.1-only coefficients neutralised) | PASS where the models coincide; every difference traced to a named construct |
| `.tir` loader, lexer, routing | Public `.tir` corpus (40 files), spec-derived lexer, tread, MFeval `readTIR` | Lexer PASS (0 defects); routing 240/240 probes as specified (pre-amendment routing). The 1 low-severity diagnostics bug (TIR-2) and the 1 policy gap (PFF-USER, `PROPERTY_FILE_FORMAT = 'USER'`) found here are fixed in the code (section 5); the corpus sweep and the oracle comparisons were not re-run on the fixed code (section 6) |

Limits of the evidence:
- Agreement with MFeval is **not** TNO MF-Tyre 5.2 conformance. Our MF 5.2 equations are [impl]-tier and taken from the same MFeval `Solver.m` (mf52.md section 1, AD-26). The result proves a faithful re-implementation, not independent correctness.
- Neither MF 5.2 fixture exercises QSY2-4, scale factors other than 1, LFZO other than 1, or LGA* other than 1. The synthetic fixture `mf52_S52.tir` has zero curvature and shift coefficients and QSX1-3 = 0, so its Mx is identically 0 on both sides.
- OpenTire has no friction ellipse, no LFZO in `dfz`, and no MF 5.2. The ellipse combined-slip method and the LFZO-normalised `dfz` have no independent oracle.
- All oracle runs used the ISO-W sign convention on both sides, raw alpha and gamma, SI units and TYRESIDE LEFT (no mirroring), so the sign map is the identity (OpenTire My is negated, AD-17).

## 2. Oracles and versions

| Oracle | Version | Licence | Use |
|---|---|---|---|
| MFeval | 4.0.0, from MathWorks Simscape-Vehicle-Templates `2c303195f016cb8da3906c5d92445247ddab87e2`, `MFeval_4p0` | BSD-3 (c) 2020 Marco Furlan Tassara | MF 5.2 primary oracle; run in Octave 10.3.0 |
| OpenTire (opentirepython) | commit `6652c49031071f70cba9080a2837fdd95b7317a7` (2020-06-26), PAC2002 | MIT | PAC2002 secondary oracle (known deviations OT-1 to OT-5); Python 3.13, numpy 2.5 |
| teasit magic-formula-tyre-library | `aeae697c414c367c9681ef7ce15184221fd46ffb`, `+v61` functions | GPL-3.0 | Limited MF 5.2 cross-check only; nothing from it is in the tree |
| tread, MFeval `readTIR`, spec-derived lexer | tread `6e90adaa`, MFeval 4.0 | MIT / BSD-3 / own | Lexical cross-checks of the loader |
| `.tir` corpus | Chrono `39393e3`, tread, MFeval examples, Simscape Data_TIR, teasit, JyNing04, gab-s98 (40 files) | various | Parser and routing sweep; no file is committed |

Our side: `mf` 0.1.0 built with GCC 14.4 (release preset), repo HEAD `990c94c` plus the uncommitted tire-model work.

## 3. Method

- Grids (adapter `make_grid.py`, seed 20261018): blocks G1-G6 plus GP (pressure). G1/G2 are the pure and axis sweeps, G3 combined, G4 special speeds (low, zero, negative), G5 boundary loads, G6 Latin hypercube. Rows are tagged by regime: CORE, SMALLA, LIN, HISLIP, CAMBER, CLAMP, ZERO (Fz <= 0), LOWV (0 < Vx < VXLOW), REV (Vx < 0), EACT (E-clamp active). Only CORE rows gate; other regimes are classified, not counted as failures.
- Sizes: MFeval S52 5741 rows, MFeval example file 7833 rows, OpenTire 6245 rows (S, S_ell) and 7807 rows (others), teasit 9807 rows per fixture. Vx <= 0 rows are run one at a time and must be rejected (AD-02).
- Tolerances: T1-MFE `|o - r| <= 1e-6*scale + 1e-6*max(|o|,|r|)` with `scale = FNOMIN` for forces and `FNOMIN*R0` for moments (a 1e-10 tier is reported). T1-OT and T1-TS use rtol 1e-9 with atol 1e-9*scale. T2-TS (rtol 2e-3, atol 1e-4*scale) applies to combined Fx/Fy and the Mx/My built on them at |SA| <= 0.05.
- MFeval modes: `useMode` 111 (combined, alpha* = alpha, limit checks on, no turn slip) is the gate; 211 (limit checks off) detects E-clamp and low-speed rows; pure runs set alpha = 0 (Fx0, My) or kappa = 0 with SSZ1-4 = 0 (Fy0, Mz0, Mx). A batch-versus-single check on 40 rows gave a difference of 0.
- MFeval adapter specifics: Octave's `mfeval.m` shadows the `+mfeval` package, so the adapter calls `mfeval.readTIR` and `mfeval.Solver.fullSteadyState` directly. A CRLF copy of every `.tir` is fed to MFeval (MFE-10). Missing range pairs are set to FZMIN = 0, FZMAX = Inf, others +/-Inf; keys foreign to MF 5.2 are set to the values giving the mf52.md section 4 form (PKY4 = 2, LKYC = LKZC = 1, others 0).
- OpenTire adapter map: `SA_ot = atan(SA)` and `IA_ot = asin(IA)` (so OpenTire's `tan(alpha)` and `sin(gamma)` equal our raw inputs), `My_ours = -My_OT`, LFZ0 set from LFZO. Unmapped OpenTire is not a valid comparison (OT-5).
- teasit parameters come from our own reader with every 6.1-only key neutralised; an own transcription of mf52.md section 4 with six switches (`explain.py`) reproduces each teasit construct, so every difference is attributed to a named construct (spec reproduces our CLI to 6e-16 x scale, all switches reproduce teasit to 1.5e-15 x scale).

## 4. Results per oracle

### 4.1 MFeval (MF 5.2)

Fixtures: the project-authored `mf52_S52.tir` (4622 CORE rows) and the MFeval example file `MagicFormula52_Paramerters.tir` (BSD-3, FNOMIN 9416.6, R0 0.4; 4496 CORE rows; not in the tree). These numbers were measured with the example file loaded with its `PROPERTY_FILE_FORMAT = 'USER'` line removed (the `*_nopff.tir` copy; see PFF-USER); MFeval ran the unmodified file. They have not been re-run on the unmodified file with the new routing (section 6).

Maximum relative error, CORE rows, example file (combined / pure `--uncombined`):

| Channel | Combined | Pure |
|---|---|---|
| Fx | 1.2e-14 | 1.1e-15 (Fx0) |
| Fy | 4.4e-13 | 4.4e-13 (Fy0) |
| Mz | 2.4e-13 | 9.2e-13 (Mz0, 4376 rows) |
| Mx | 5.6e-14 | 4.8e-14 |
| My | 6.6e-16 | 6.6e-16 |

- Maximum absolute error at most 1.8e-11 N. My is compared over 3983 of the 4496 CORE rows; the other 513 CORE rows differ from MFeval (MFE-13, LIT-OPEN, not adopted) and are not counted as matches.
- Also pass at 1e-10: camber rows (3788), high-slip rows (927), 1361 rows where MFeval's E <= 1 clamps are active (our model always clamps E; these match useMode 111), rows clamped at the range limits with Fz >= FZMIN (1238; Fx, Fy, Mz, Mx). Fz <= 0 rows are zero on both sides. The pressure rows (GP) have zero difference. The unmodified MFeval struct gives results identical to the overridden one.
- `mf52_S52.tir`: Fx, Fy, Mz pass (max relative 1.3e-15, 2.5e-15, 1.7e-11; the Mz value is a near-zero cancellation with absolute error 2e-13 N*m). Mx is identically 0 (no evidence). My passes on 4405 rows (max relative 6.2e-16); the other 215 are MFE-13.

### 4.2 OpenTire (PAC2002)

- CORE rows, LFZO = 1 fixtures (`pac2002_S`, `pac2002_S_rng`, Chrono pactest, mf185, citybus, and a dense variant with non-unit L*, non-zero QSX1-3, QSY2-4, QBZ10, QEZ3, PDX3, RBY3): 28,784 rows per mode, 0 failures at T1-OT in all five channels, combined and uncombined. `pac2002_S_ell` (uncombined): 4961 of 4961.
- Maximum CORE error after the map: Fx 3.1e-7 N (rel 6.7e-7 only near Fx = 0), Fy 4.2e-7 N, Mz 3.1e-8 N*m, Mx 1.7e-8 N*m, My 2.4e-9 N*m. With our AD-03 epsilon added to OpenTire the residual drops to Fx rel <= 1e-15, Fy rel <= 1e-11, Mx abs <= 5e-13, My abs <= 3e-14, Mz abs <= 1.9e-9 (OpenTire's unguarded Kx/Ky in alpha_eq). The residual is therefore the epsilon guard only.
- Calibration against the hand vectors of test-data.md (V2-V21): 20 of 21 pass; V18 `dfz` fails in OpenTire (OT-1).
- Non-CORE: CLAMP rows pass once OpenTire receives AD-15-clamped inputs (residual at most 1.4e-12 N Fx, 1.3e-11 N Fy, 1.2e-9 N*m Mz), which confirms the "evaluate at the limit" semantics. Fz <= 0: 13,680 rows, all outputs exactly 0 (AD-01). Vx < 0: 24 of 24 rejected with exit 1 and the AD-02 message. Outputs are independent of pressure (AD-09). LOWV rows pass (neither side applies VXLOW).

### 4.3 teasit (limited MF 5.2 cross-check)

Gate rows: CORE, IA = 0, not EACT. Fixture (a) is the MFeval example file (2074 pure rows, 585 combined SMALLA rows).

- Agree: Fx0 (2074 of 2074, max rel 6.8e-10), My0 and My (T1/T2, max abs 4.3e-14 N*m; with QSY2-4 active max rel 5.2e-9), combined Fx/Fy/Mx/My at T2 within |SA| <= 0.05 (585 of 585; Fx max 3.08 N).
- Pass except for one catalogued deviation: Fy0 1917 of 2074 (TS-01), Mx0 1931 of 2074 (through Fy0).
- Differ by construction (not coincident, contrary to the validation plan): Mz0 uncombined (9 of 2074 pass at T1; max 11.7 N*m), combined Mz (report-only), and combined Fx/Fy/Mx/My beyond |SA| > 0.05 (report-only).
- Fixture `mf52_S52.tir`: Fx0, Fy0, Mx0, My0 and the T2 combined channels pass; Mz0 fails on 2457 of 2939 rows (T12, T14).
- Calibration on S52: Fx0 3935.4796 (hand 3935.48), Fy0 -2376.3982 (-2376.40), My -12 (-12), Mz 60.4485 (60.46, the expected T12 tan(alpha) effect).

### 4.4 `.tir` corpus sweep

This section describes the **pre-amendment** routing (before the AD-11 amendment for 'USER' and 'MF-TYRE'). It was not re-run. With the amended routing the expected outcome differs: the 'USER' / 'MF-TYRE' files with a supported FITTYP are no longer rejected for their PFF value, so the acceptance count and the rejection breakdown below are stale (expected, not measured).

- 3 of 40 files accepted (`pactest.tir` and `HMMWV_pacejka.tir` route to PAC2002; `fsaettc_obfuscated_v52.tir` routes to MF 5.2). The 37 rejections (pre-amendment) are 13 ANGLE = 'radian' (AD-10), 9 FITTYP 61/62 (AD-11), 6 unknown PFF 'USER'/'MF-TYRE' (AD-11 rule 1, since amended), 4 [SHAPE] without '{' (AD-20), 2 conflicting duplicate keys, and the MFeval MF 5.2 example (PFF 'USER', see PFF-USER). Every rejection exits 1 with empty stdout; accepted files exit 0.
- Routing (pre-amendment): 240 of 240 probes (12 PFF values x 20 FITTYP values) match model-routing.md and AD-11 as they were then, including error precedence and the eq (88) switch for FITTYP 5 + PAC2002 with QSY1 = QSY2 = 0.
- Lexer: 7043 of 7043 entries identical to an independent spec-derived lexer on the 34 lexically valid files; 6303 of 6303 values equal to tread; 6844 of 6844 equal to MFeval `readTIR` (CRLF-adapted); 3000 of 3000 mutation-fuzz files agree in outcome (26 mutation kinds, seed 20261018); `io::parse_number` agrees on 1412 of 1412 strings.

## 5. Mismatches, classes and dispositions

Classes: REF-DEV (the reference deviates from our specification, catalogued), OUT-OF-REGIME (no oracle for that regime), ADAPTER (input transform), LIT-OPEN (literature question open), OUR-BUG. No OUR-BUG affects numerics. Triage result: MFE-13, PFF-USER, TS-12 and TIR-1 were documentation changes (PFF-USER later became a routing-policy change); TIR-2 was a confirmed low-severity diagnostics bug. PFF-USER and TIR-2 are fixed in the code (see their rows); the fixes are covered by unit tests, but the oracle and corpus runs were not repeated.

| Id | Oracle | Channel / rows | Class | Disposition |
|---|---|---|---|---|
| MFE-11 | MFeval | All channels, 0 < Fz < FZMIN (1143 example rows; new id). MFeval raises Fz to FZMIN only in Dt/Dr and applies low-load factors in Mx/My; ours clamps Fz before every kernel. MFeval re-run with Fz := FZMIN equals ours within 1e-6 | REF-DEV | None. Matches the primary rule (MSC p.45, SIM20 p.25; mf52.md section 5). Rows are not compared |
| MFE-12 | MFeval | My with Fz > FZMAX (374 rows; new id). MFeval multiplies by the unclamped Fz (`Solver.m:1454`, `:1508`); ours evaluates at the limit (AD-15). Verified: reference = ours*Fz/FZMAX | REF-DEV | None. Rows are not compared on My |
| MFE-13 | MFeval | My, SR <= VXLOW/\|Vx\| - 1 (S52 215 rows; example 520, of which 513 CORE). MFeval's "Low speed model (Empirically discovered)" (`Solver.m:1517-1537`); reference = ours*f13 for all 735 rows | LIT-OPEN | Not adopted (D-12, AD-05). Recorded in mf52.md sections 1 and 5, conventions.md section 5; Q-03 extended (does TNO scale or flip My with Vr = Vx(1+kappa)?). The comparison classifies these rows as the known deviation or compares against ours*f13; they are not failures. Optional unit test pinning SR = -1 at Vx = LONGVL (non-zero My without taper) is not yet written |
| MFE-8 | MFeval | All channels, LOWV rows (Vx in {1e-3, 0.1, 0.5}; 18 rows per channel on each fixture). MFeval's low-speed model is active for \|Vx\| <= VXLOW; ours only flags and warns. Ours equals MFeval useMode 211 on these rows (My after the MFE-13 factor) | OUT-OF-REGIME | None (AD-05). The reference CSV excludes these rows |
| MFE-10 | MFeval | Oracle parser. `readTIR.m:356` assumes CRLF and drops the last character of each LF line (127 of 284 fields corrupted on S52) | REF-DEV | Adapter feeds CRLF copies; fixed upstream in 4.1+. No output mismatch remains |
| OT-1 | OpenTire | All channels through `dfz`, LFZO != 1 (dense LFZO 1.2 and Chrono sedan LFZO 0.81). OpenTire uses `dfz = (Fz - FNOMIN)/FNOMIN` (no LFZO), key `LFZ0` | REF-DEV | None. Ours follows MSC eq (16); V18 pins it. OpenTire is demoted on LFZO != 1 files; with a `dfz` patch all uncombined rows pass (max rel 1e-12) |
| OT-1b | OpenTire | Combined Mz with LFZO != 1. OpenTire normalises the moment arm by FNOMIN*LFZO; ours by FNOMIN (AD-04, D-09). After the predicted term the residual is 1.2e-9 N*m over 6904 rows | REF-DEV | Q-02 stays open |
| OT-3 | OpenTire | Fx, Fy, Mz. OpenTire has no epsilon; ours uses 1e-6 (AD-03) | REF-DEV | None. Inside T1-OT; the eps variant removes the residual |
| OT-4 (new) | OpenTire | Fx, Fy, Mz at Vx = 0, SA != 0 (142 channel-rows). OpenTire uses `alpha* = tan(SA)*sgn(V)`, so alpha vanishes at Vx = 0; ours accepts Vx = 0 and keeps alpha | REF-DEV | None; add catalogue id OT-4 |
| OT-5 (new, extends OT-2) | OpenTire | Raw (unmapped) OpenTire on any row with SA != 0 or IA != 0: `tan(alpha)` and `sin(gamma)` enter Fy0, Gxa, Byk, DVyk, alpha_t, alpha_r and cos(alpha*). The validation plan's "compare Mz only at \|alpha\| <= 0.1" tier (OT-2) understates this and passes only 30-70% of rows | REF-DEV | Use the adapter map plus T1; the plan's T2-OT-Mz tier is not valid for unmapped OpenTire |
| CLAMP-OT | OpenTire | Rows outside the FZ/KPU/ALP/CAM ranges (OpenTire has no range handling) | OUT-OF-REGIME | None. Clamped-input variant reproduces ours to round-off, confirming AD-15 |
| ZERO-FZ | OpenTire, teasit | Fz <= 0: OpenTire returns negative-load values or NaN, teasit sign-flipped values; ours exactly 0 (AD-01). 13,680 OpenTire rows | OUT-OF-REGIME | None |
| ELL-PI2 | OpenTire | Mz on 110 pure-slip rows at SA = +/-1.7 rad (> pi/2) of `pac2002_S_ell` combined versus OpenTire pure slip (about 5.1%, the closed-form ratio 0.949). Beyond every ALPMAX; OpenTire has no ellipse | OUT-OF-REGIME | None (pac2002.md section 5.2) |
| UNITS-RADIAN / TIR-3 | OpenTire, corpus | `ANGLE = 'radian'` rejected (13 of the 15 PAC2002 files in the public corpus; with 'radians' substituted all load and evaluate finite) | ADAPTER / OUT-OF-REGIME | None. Legitimate by AD-10 (Q-14 closed). Usability note: AD-10 shuts out most real ADAMS/Chrono PAC2002 files; a one-word alias would be a policy change |
| TS-01 | teasit | Fy0 (and Mx0, Mz0 through it): 157 of 2074 T1 rows with \|SA\| < \|SHy\| and sign(SA) != sgn+(SA + SHy). teasit uses `sign(slipangl)` in Ey; ours uses sgn+(alpha_y). Max 7.3e-2 N (rel 2.2e-4) | REF-DEV | None (mf52.md section 4 step 5) |
| TS-02 | teasit | Combined Fx/Fy and Mx/My beyond \|SA\| > 0.05 (max Fx 369 N). teasit uses `alpha* = tan(SA)*sgn(Vx)` in the combined weights; ours uses raw alpha. Passes T2 inside SMALLA | REF-DEV (T2) | None (mf52.md section 1, step 7) |
| TS-03 | teasit | Mz0 and Mz (max 11.7 N*m; S52 up to 890 N*m near \|SA\| = pi/2). teasit uses `alpha* = tan(alpha)` in alpha_t, alpha_r (new id T12). Contradicts the plan, which listed Mz0 as coincident | REF-DEV | None; correct the plan table |
| TS-04 | teasit | Mz0 and Mz, Dr != 0 and alpha != 0 (up to 1.6e-3 N*m isolated). teasit applies cos'(alpha) twice in Mzr (new id T13); ours once (mf52.md steps 6 and 8, D-26) | REF-DEV | None |
| TS-05 | teasit | Mz0, Fx0, Fy0. teasit guards denominators with machine eps; ours with 1e-6 (new id T14; AD-03, Q-08). Fx0 rel 6.8e-10 | REF-DEV | None |
| TS-06 | teasit | Combined Mz, SR != 0 (up to 0.99 N*m on the example file, 1.55 N*m on S52). teasit uses the book linear alpha_eq form (new id T15); ours uses MSC (84)-(85) repaired (D-05) | REF-DEV | None |
| TS-07 | teasit | Fx0, Fx, My via Fx, Mz: EACT rows (636). teasit has no E <= 1 limit (T7); max 3.29e4 N at FZMAX braking where Ex = 1.70 | REF-DEV | None (D-17). Rows excluded from the gate |
| TS-08 | teasit | All channels with IA != 0 (3772 rows per MFeval fixture). teasit implements 6.1 camber (SVyg, Kyg0, no PHY3, gamma* = sin(gamma)) | OUT-OF-REGIME | None. Fx0 failures among camber rows (525) are all EACT |
| TS-09 | teasit | CLAMP rows (1964 on the example file). With inputs pre-clamped per AD-15 the comparable rows show only the classes above | OUT-OF-REGIME | None |
| TS-10 | teasit | ZERO rows and Vx <= 0 (30 rows not run) | OUT-OF-REGIME | None (AD-01, AD-02) |
| PFF-USER (= TS-12 = TIR-1) | MFeval, teasit, corpus | The whole MFeval MF 5.2 example file is rejected: `unknown PROPERTY_FILE_FORMAT 'USER' (expected 'PAC2002' or 'MF_05') (AD-11)`. The file carries PFF = 'USER', USER_SUB_ID = 815, FITTYP = 6. 'USER' occurs in 7 of 40 corpus files, always with a TNO FITTYP (6, 61, 62). With the PFF line removed it loads as MF 5.2 with only the AD-19 (LGAX/LGAY/LGAZ) and AD-16 (USE_MODE 14) warnings | LIT-OPEN | At validation time the code followed AD-11 and rejected the file. **FIXED** by the policy change: 'USER' and 'MF-TYRE' route by FITTYP like an absent PFF; unknown strings and every unsupported FITTYP still error. Code: `KNOWN_FORMATS` (PAC2002, MF_05, USER, MF-TYRE) in `src/io/model_routing.cpp`. Tests (`tests/unit/model_routing_test.cpp`): `Route_UserPffFittyp6_Mf52NoWarning`, `Route_UserPffLowerCaseFittyp6_Mf52NoWarning`, `Route_MfTyrePffFittyp6_Mf52NoWarning`, `Route_UserPffFittyp61_Mf6xNotImplemented`, `Route_UserAndMfTyrePffFittyp5_UnsupportedModel`, `Route_UserAndMfTyrePffOtherFamilies_UnsupportedModel`, `Route_UserPffWithoutFittyp_CannotIdentify`, `Route_UserPffUnrelatedKey_StaysUnconsumed`, `Route_EmptyPff_UnsupportedModel`. Docs: model-routing.md, the AD-11 amendment in decisions.md, Q-13. The oracle runs above still used the `*_nopff.tir` copy; the rerun on the unmodified file is open (section 6) |
| TIR-2 | corpus | Loader diagnostics: no AD-19 warning for foreign pressure (PPX*, PPY*, PPZ*, PPMX1), turn-slip (PDXP1-3, PKYP1, PDYP1-4, PHYP1-4, PECP1-2, QDTP1, QCRP1-2, QBRP1, QDRP1-2), Q_FYS1-3, [STRUCTURAL] and [INERTIA] keys present in an MF 5.2 file; `FOREIGN_KEYS` has 30 of the keys that mf52.md section 3 lists. Numerics unaffected | OUR-BUG (low) | Confirmed at validation time. **FIXED**: `FOREIGN_KEYS` in `src/io/mf5x_bindings.cpp` now has 70 keys (mf52.md section 3) and `FOREIGN_SECTIONS` covers STRUCTURAL, INERTIA and CONTACT_PATCH. Tests (`tests/unit/model_loader_test.cpp`): `Load_Mf52EveryForeignKey_WarnsWithThatKeyAndConsumesIt`, `Load_Mf52PressureTurnSlipStructuralInertia_OneWarningNamesEach`, `Load_Mf52InertiaMassOnly_WarningIsLocatedAtInertiaMassNotUnitsMass`, `Load_Mf52ContactPatchSection_AllKeysWarnedAndConsumed`, `Load_Mf52UnitsMassOnly_NoForeignWarning`, `Load_Pac2002SameKeys_NoAd19WarningAndKeysIgnored` |
| TIR-4 | corpus | 4 Goodyear `feda` files: `[SHAPE]` without '{' gives SYNTAX (AD-20). Behind it FITTYP 5 + MF_05, a partial R* set and TYRESIDE 'UNKNOWN' also reject | OUT-OF-REGIME | None (AD-20, AD-06, AD-07, AD-11). MFeval `readTIR` also fails on these files |
| TIR-5 | corpus | 2 gab-s98 files with conflicting duplicate keys give DUPLICATE_KEY | OUT-OF-REGIME | None (tir-format.md section 2 rule 10) |
| TIR-6 | corpus | 15 files with FITTYP 61/62 or unknown PFF 'USER'/'MF-TYRE' reported first | OUT-OF-REGIME | None (AD-11; MF 6.x not implemented) |
| TIR-7 | corpus | MFeval `readTIR` drops 2 characters per line on LF files; the spec-derived lexer gives no line number for a NUL byte | ADAPTER | CRLF-normalised copies; no mismatch remains |

## 6. What remains open

- **Rerun after the fixes (open)**: PFF-USER and TIR-2 are fixed in the code, but the MFeval and teasit comparisons on the unmodified MFeval example file, and the corpus routing sweep (section 4.4), have **not** been re-run with the new routing. Sections 4.1 to 4.4 describe the pre-fix state and the `*_nopff.tir` derived copies are still what the recorded numbers used. Until the rerun is recorded, the claim "the MFeval example file loads and matches" rests on the PFF-stripped copy.
- **PFF 'USER' / 'MF-TYRE' routing** (PFF-USER, Q-13): fixed (section 5, AD-11 amendment). Only the rerun above is open.
- **Foreign-key warnings** (TIR-2): fixed (section 5). Only the corpus rerun is open.
- **MFE-13 / Q-03**: whether TNO scales or flips My with the wheel rolling speed is unknown; MFeval's law is documented as "empirically discovered" and not adopted.
- **Q-07 (licences), Q-07a resolved**: the MFeval-derived numbers in `tests/data/reference/mf52_mfeval.csv` (used by `tests/module/mfeval_reference_test.cpp`) carry the BSD-3 notice, and the wrapper repository's `LICENSE.md` adds a clause limiting use to use with MathWorks products. The user decided on 2026-10-18 to **keep** these outputs (computed on our own synthetic fixture; BSD-3 notice retained in `tests/data/reference/README.md`; AD-29, open-questions.md Q-07a). The CSV and the test are therefore no longer provisional. The MFeval example `.tir` and all Chrono/tread/teasit files stay out of the tree.
- **Coverage gaps in the evidence**: MF 5.2 QSY2-4, non-unit scale factors, LFZO != 1 and LGA* != 1 have no MFeval comparison; the friction-ellipse method and LFZO-normalised `dfz` have no independent PAC2002 oracle; Vx = 0 behaviour is not independently validated; MF 5.2 has no primary-source validation (Q-01 closed under AD-26).
- **AD-10 usability** (not a bug): 'radian' is common in real ADAMS/Chrono files; an alias would be a policy decision.
- **Plan corrections** to record: new catalogue ids MFE-11, MFE-12, OT-4, T12-T15; Mz0 is not coincident with teasit; OT-2 is replaced by the adapter map; the optional unit test pinning the MFE-13 behaviour.

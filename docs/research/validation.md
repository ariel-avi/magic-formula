# Validation

Summary of the validation of the PAC2002 and MF 5.2 implementations and of the `.tir` loader by comparison with independent implementations (section 2). Two loader items found during the validation, the PROPERTY_FILE_FORMAT 'USER' routing and the foreign-key warnings, are fixed in the code (section 5 names the code and the tests). The numbers in sections 4.1 to 4.4 were measured **before** those fixes and were **not re-run** (section 6). The MF 5.2 verdict is qualified for My (low-speed My taper, section 5). Data kept in the tree: `tests/data/reference/` (origin and licence notice in its README). The comparison scripts, grids and raw outputs are not in the repository.

## 1. Verdict and limits

| Area | Comparison | Verdict |
|---|---|---|
| MF 5.2 (FITTYP 6) | MF 5.2 reference comparison | PASS: all five channels match within the reference tolerance (also within 1e-10) on every CORE row, except My on the 513 CORE rows of the second MF 5.2 parameter set (215 rows on `mf52_S52.tir`); low-speed My taper, not adopted (section 5) |
| PAC2002 | PAC2002 cross-check | PASS: every CORE row matches on all five channels within the tight tolerance, after the input-convention map |
| MF 5.2, limited cross-check | MF 6.1-family cross-check (6.1-only coefficients neutralised) | PASS where the formulations coincide; every difference traced to a named construct |
| `.tir` loader, lexer, routing | Property-file sweep (40 files) | Lexer PASS (0 defects); routing 240/240 probes as specified (routing before the 'USER' and 'MF-TYRE' fix). The 1 low-severity diagnostics bug (the foreign-key warnings) and the 1 policy gap (the 'USER' routing, `PROPERTY_FILE_FORMAT = 'USER'`) found here are fixed in the code (section 5); the sweep and the comparisons were not re-run on the fixed code (section 6) |

Limits of the evidence:
- Agreement in the MF 5.2 reference comparison shows that the code implements mf52.md section 4 faithfully. It is not an independent check of the equation set itself, because that implementation follows the same equations.
- Neither MF 5.2 fixture exercises QSY2-4, scale factors other than 1, LFZO other than 1, or LGA* other than 1. The synthetic fixture `mf52_S52.tir` has zero curvature and shift coefficients and QSX1-3 = 0, so its Mx is identically 0 on both sides.
- The PAC2002 cross-check covers neither the friction-ellipse method nor the LFZO-normalised dfz; those have no independent check.
- All comparisons used the W-axis convention of conventions.md section 1 (x forward, y left, z up, origin at the contact point), raw alpha and gamma, SI units and TYRESIDE LEFT. The PAC2002 cross-check needed a negated My (policies.md section 2).

## 2. Comparisons

- **MF 5.2 reference comparison**: an independent implementation of the MF 5.2 (FITTYP 6) equations. The committed `tests/data/reference/mf52_mfeval.csv` comes from it.
- **PAC2002 cross-check**: an independent PAC2002 implementation that uses tan(alpha) and sin(gamma) where this project uses alpha and gamma. It has no LFZO in dfz, no friction ellipse, no epsilon guard and no range handling.
- **MF 6.1-family cross-check**: an MF 6.1 implementation with every 6.1-only coefficient set neutral, used only where it coincides with MF 5.2.
- **Property-file sweep**: 40 non-synthetic property files (none in the repository), read by the loader and compared with an independently written lexer and two other readers.
- **Second MF 5.2 parameter set**: a non-synthetic MF 5.2 file with non-zero curvature and shift coefficients, not in the repository.

Our side: `mf` 0.1.0 built with GCC 14.4 (release preset), repository commit `990c94c` plus the then uncommitted tire-model work.

## 3. Method

- Grids: blocks G1-G6 plus GP (pressure). G1/G2 are the pure and axis sweeps, G3 combined, G4 special speeds (low, zero, negative), G5 boundary loads, G6 Latin hypercube. Rows are tagged by regime: CORE, SMALLA, LIN, HISLIP, CAMBER, CLAMP, ZERO (Fz <= 0), LOWV (0 < Vx < VXLOW), REV (Vx < 0), EACT (E-clamp active). Only CORE rows gate; other regimes are classified, not counted as failures.
- Sizes: 5741 rows on `mf52_S52.tir` and 7833 on the second MF 5.2 parameter set; PAC2002 cross-check 6245 rows (S, S_ell) and 7807 (others); MF 6.1-family cross-check 9807 rows per fixture. Vx <= 0 rows are run one at a time and must be rejected (policies.md section 1, Vx).
- Tolerances:
  - **reference tolerance**: `|o - r| <= 1e-6*scale + 1e-6*max(|o|,|r|)` with `scale = FNOMIN` for forces and `FNOMIN*R0` for moments (a 1e-10 tolerance is also reported);
  - **tight tolerance**: rtol 1e-9 with atol 1e-9*scale;
  - **small-angle tolerance**: rtol 2e-3, atol 1e-4*scale, applied to combined Fx/Fy and the Mx/My built on them at |SA| <= 0.05.
- MF 5.2 reference comparison modes: the oracle run in combined mode with its limit checks on (alpha* = alpha, no turn slip) is the gate; with its limit checks off it detects EACT and LOWV rows; its pure runs set alpha = 0 (Fx0, My) or kappa = 0 with SSZ1-4 = 0 (Fy0, Mz0, Mx). A batch-versus-single check on 40 rows gave a difference of 0.
- MF 5.2 reference comparison specifics: missing range pairs are set open (FZMIN = 0, FZMAX = Inf, others +/-Inf), and keys foreign to MF 5.2 are set to the values giving the mf52.md section 4 form (PKY4 = 2, LKYC = LKZC = 1, others 0).
- PAC2002 cross-check input map: SA -> atan(SA), IA -> asin(IA), so that the oracle's tan(alpha) and sin(gamma) equal the raw alpha and gamma used here; My negated. A comparison without the map is invalid (section 5, unmapped tan/sin inputs).
- MF 6.1-family cross-check: parameters come from our own reader with every 6.1-only key neutralised. A transcription of mf52.md section 4 with six switches reproduces each construct (our CLI to 6e-16 x scale, the oracle to 1.5e-15 x scale), so every difference is attributed to a named construct.

## 4. Results per comparison

### 4.1 MF 5.2 reference comparison

Fixtures: the project-authored `mf52_S52.tir` (4622 CORE rows) and the second MF 5.2 parameter set (FNOMIN 9416.6, R0 0.4; 4496 CORE rows; not in the tree). Our side loaded the second set with its `PROPERTY_FILE_FORMAT = 'USER'` line removed, because the routing at the time rejected 'USER'; the oracle used the unmodified file. Not re-run (section 6).

Maximum relative error, CORE rows, second parameter set (combined / pure `--uncombined`):

| Channel | Combined | Pure |
|---|---|---|
| Fx | 1.2e-14 | 1.1e-15 (Fx0) |
| Fy | 4.4e-13 | 4.4e-13 (Fy0) |
| Mz | 2.4e-13 | 9.2e-13 (Mz0, 4376 rows) |
| Mx | 5.6e-14 | 4.8e-14 |
| My | 6.6e-16 | 6.6e-16 |

- Maximum absolute error at most 1.8e-11 N. My is compared over 3983 of the 4496 CORE rows; the other 513 CORE rows differ from the oracle (low-speed My taper, OPEN, not adopted) and are not counted as matches.
- Also pass at 1e-10: camber rows (3788), high-slip rows (927), 1361 rows where the oracle's E <= 1 clamps are active (our model always clamps E; these match the oracle with limit checks on), rows clamped at the range limits with Fz >= FZMIN (1238; Fx, Fy, Mz, Mx). Fz <= 0 rows are zero on both sides. The pressure rows (GP) have zero difference. The oracle without the neutral overrides gives identical results.
- `mf52_S52.tir`: Fx, Fy, Mz pass (max relative 1.3e-15, 2.5e-15, 1.7e-11; the Mz value is a near-zero cancellation with absolute error 2e-13 N*m). Mx is identically 0 (no evidence). My passes on 4405 rows (max relative 6.2e-16); the other 215 are the low-speed My taper.

### 4.2 PAC2002 cross-check

- CORE rows, LFZO = 1 fixtures (`pac2002_S`, `pac2002_S_rng`, three non-synthetic PAC2002 parameter sets not in the repository, and a dense synthetic variant with non-unit L*, non-zero QSX1-3, QSY2-4, QBZ10, QEZ3, PDX3, RBY3): 28,784 rows per mode, 0 failures at the tight tolerance in all five channels, combined and uncombined. `pac2002_S_ell` (uncombined): 4961 of 4961.
- Maximum CORE error after the map: Fx 3.1e-7 N (rel 6.7e-7 only near Fx = 0), Fy 4.2e-7 N, Mz 3.1e-8 N*m, Mx 1.7e-8 N*m, My 2.4e-9 N*m. With the epsilon guard of policies.md section 2 added on the oracle side the residual drops to Fx rel <= 1e-15, Fy rel <= 1e-11, Mx abs <= 5e-13, My abs <= 3e-14, Mz abs <= 1.9e-9 (the remaining Mz residual comes from unguarded Kx/Ky in alpha_eq on the oracle side). The residual is therefore the epsilon guard only.
- Calibration against the hand vectors of test-data.md (V2-V21): 20 of 21 pass; V18 (dfz) fails on the oracle side (no LFZO in dfz).
- Non-CORE: CLAMP rows pass once the oracle receives clamped inputs (policies.md section 1) (residual at most 1.4e-12 N Fx, 1.3e-11 N Fy, 1.2e-9 N*m Mz), which confirms the "evaluate at the limit" semantics. Fz <= 0: 13,680 rows, all outputs exactly 0 (policies.md section 1). Vx < 0: 24 of 24 rejected with exit 1 and the negative-Vx message. Outputs are independent of pressure (policies.md section 5). LOWV rows pass (neither side applies VXLOW).

### 4.3 MF 6.1-family cross-check (limited)

Gate rows: CORE, IA = 0, not EACT. Fixture (a) is the second MF 5.2 parameter set (2074 pure rows, 585 combined SMALLA rows).

- Agree: Fx0 (2074 of 2074, max rel 6.8e-10), My0 and My (tight and small-angle tolerances, max abs 4.3e-14 N*m; with QSY2-4 active max rel 5.2e-9), combined Fx/Fy/Mx/My within the small-angle tolerance at |SA| <= 0.05 (585 of 585; Fx max 3.08 N).
- Pass except for one named difference: Fy0 1917 of 2074 (sign function in Ey), Mx0 1931 of 2074 (through Fy0).
- Differ by construction: Mz0 uncombined (9 of 2074 pass at the tight tolerance; max 11.7 N*m), combined Mz (report-only), and combined Fx/Fy/Mx/My beyond |SA| > 0.05 (report-only).
- Fixture `mf52_S52.tir`: Fx0, Fy0, Mx0, My0 and the small-angle combined channels pass; Mz0 fails on 2457 of 2939 rows (tan(alpha) in alpha_t and alpha_r; machine-epsilon guard).
- Calibration on S52: Fx0 3935.4796 (hand 3935.48), Fy0 -2376.3982 (-2376.40), My -12 (-12), Mz 60.4485 (60.46, the expected tan(alpha) effect).

### 4.4 Property-file sweep

This section describes the routing **before the fix** for 'USER' and 'MF-TYRE' (see the routing rule for 'USER' and 'MF-TYRE' in model-routing.md). It was not re-run. With the fixed routing the expected outcome differs: the 'USER' / 'MF-TYRE' files with a supported FITTYP are no longer rejected for their PROPERTY_FILE_FORMAT value, so the acceptance count and the rejection breakdown below are stale (expected, not measured).

- 3 of 40 files accepted (two route to PAC2002, one to MF 5.2). The 37 rejections (before the fix) are 13 ANGLE = 'radian' (policies.md section 3, [UNITS]), 9 FITTYP 61/62 (model-routing.md), 6 unknown PFF 'USER'/'MF-TYRE' (the old rule that rejected every PROPERTY_FILE_FORMAT other than PAC2002 and MF_05, since fixed; model-routing.md), 4 [SHAPE] without '{' (policies.md section 4), 2 conflicting duplicate keys, and the second MF 5.2 parameter set (PFF 'USER', see section 5). Every rejection exits 1 with empty stdout; accepted files exit 0.
- Routing (before the fix): 240 of 240 probes (12 PFF values x 20 FITTYP values) match model-routing.md as it was then, including the order in which errors are reported and the SHIFTS My form switch for FITTYP 5 + PAC2002 with QSY1 = QSY2 = 0.
- Lexer: 7043 of 7043 entries identical to an independently written lexer on the 34 lexically valid files; values equal to those of two other readers (6303 of 6303; and 6844 of 6844 with line endings adapted); 3000 of 3000 mutation-fuzz files agree in outcome (26 mutation kinds); `io::parse_number` agrees on 1412 of 1412 strings.

## 5. Differences, classes and dispositions

Classes: REF-DEV (the oracle differs from this project's definition), OUT-OF-REGIME (no comparison for that regime), ADAPTER (input transform), OPEN (an item in open-questions.md), OUR-BUG. No OUR-BUG affects numerics. Triage result: the low-speed My taper and the 'USER' value led to documentation changes; the 'USER' item then became a routing-policy change. The foreign-key warnings were a confirmed low-severity diagnostics bug. Both loader items are fixed in the code and covered by unit tests; the comparisons were not repeated.

| Difference | Comparison | Channels / rows | Class | Disposition |
|---|---|---|---|---|
| Low load (0 < Fz < FZMIN) | MF 5.2 reference comparison | All channels, 0 < Fz < FZMIN (1143 rows of the second set). The oracle raises Fz to FZMIN only in Dt/Dr and applies low-load factors in Mx/My; ours clamps Fz before every kernel. The oracle re-run with Fz := FZMIN equals ours within 1e-6 | REF-DEV | None. Matches the clamping rule (conventions.md section 8; mf52.md section 5). Rows are not compared |
| My above FZMAX | MF 5.2 reference comparison | My with Fz > FZMAX (374 rows). The oracle multiplies by the unclamped Fz; ours evaluates at the limit (policies.md section 1). Verified: reference = ours*Fz/FZMAX | REF-DEV | None. Rows are not compared on My |
| Low-speed My taper | MF 5.2 reference comparison | My, SR <= VXLOW/\|Vx\| - 1 (S52 215 rows; second set 520, of which 513 CORE). The oracle tapers My for SR <= VXLOW/\|Vx\| - 1 (formula: mf52.md section 5); reference = ours*taper factor for all 735 rows | OPEN | Not adopted (conventions.md section 5; policies.md section 1); open item (open-questions.md). These rows are classified as this known difference, not as failures. An optional unit test pinning SR = -1 at Vx = LONGVL (non-zero My, no taper) is not yet written |
| Low speed (Vx < VXLOW) | MF 5.2 reference comparison | All channels, LOWV rows (Vx in {1e-3, 0.1, 0.5}; 18 rows per channel on each fixture). The oracle's low-speed model is active for \|Vx\| <= VXLOW; ours only flags and warns. Ours equals the oracle with limit checks off on these rows (My after the taper factor) | OUT-OF-REGIME | None (policies.md section 1). The reference CSV excludes these rows |
| Oracle reader line endings | MF 5.2 reference comparison | The oracle's reader drops the last character of each LF line (127 of 284 fields on S52); CRLF copies were fed | REF-DEV | No output mismatch remains |
| dfz without LFZO | PAC2002 cross-check | All channels through dfz, LFZO != 1 (dense variant LFZO 1.2 and a non-synthetic set LFZO 0.81). The oracle uses `dfz = (Fz - FNOMIN)/FNOMIN` (no LFZO) | REF-DEV | None. Ours follows the load normalisation of pac2002.md section 3; V18 pins it. On LFZO != 1 files the oracle is not authoritative; with a dfz patch all uncombined rows pass (max rel 1e-12) |
| Moment-arm normaliser | PAC2002 cross-check | Combined Mz with LFZO != 1. The oracle normalises the moment arm by FNOMIN*LFZO; ours by FNOMIN (policies.md section 2; pac2002.md section 5.1). After the predicted term the residual is 1.2e-9 N*m over 6904 rows | REF-DEV | The open item (open-questions.md) stays open |
| No epsilon guard | PAC2002 cross-check | Fx, Fy, Mz. The oracle has no epsilon; ours uses 1e-6 (policies.md section 2) | REF-DEV | None. Inside the tight tolerance; the eps variant removes the residual |
| Slip angle at Vx = 0 | PAC2002 cross-check | Fx, Fy, Mz at Vx = 0, SA != 0 (142 channel-rows). The oracle uses `alpha* = tan(SA)*sgn(V)`, so alpha vanishes at Vx = 0; ours accepts Vx = 0 and keeps alpha | REF-DEV | None |
| Unmapped tan/sin inputs | PAC2002 cross-check | Raw (unmapped) oracle on any row with SA != 0 or IA != 0: `tan(alpha)` and `sin(gamma)` enter Fy0, Gxa, Byk, DVyk, alpha_t, alpha_r and cos(alpha*). A Mz-only comparison restricted to \|alpha\| <= 0.1 without the input map passes only 30-70% of rows | REF-DEV | Use the input map plus the tight tolerance |
| No range handling | PAC2002 cross-check | Rows outside the FZ/KPU/ALP/CAM ranges (the oracle has no range handling) | OUT-OF-REGIME | None. A clamped-input variant reproduces ours to round-off, confirming the clamping rule (policies.md section 1) |
| Fz <= 0 | PAC2002 cross-check, MF 6.1-family cross-check | Fz <= 0: the PAC2002 cross-check returns negative-load values or NaN, the MF 6.1-family cross-check sign-flipped values; ours exactly 0 (policies.md section 1). 13,680 PAC2002 cross-check rows | OUT-OF-REGIME | None |
| Ellipse beyond pi/2 | PAC2002 cross-check | Mz on 110 pure-slip rows at SA = +/-1.7 rad (> pi/2) of `pac2002_S_ell` combined versus the oracle's pure slip (about 5.1%, the closed-form ratio 0.949). Beyond every ALPMAX; the oracle has no ellipse | OUT-OF-REGIME | None (pac2002.md section 5.2) |
| ANGLE = 'radian' | PAC2002 cross-check, property-file sweep | `ANGLE = 'radian'` rejected (13 of the 15 PAC2002 files in the sweep; with 'radians' substituted all load and evaluate finite) | ADAPTER / OUT-OF-REGIME | None. Legitimate by the [UNITS] rule (policies.md section 3). Usability note: that rule shuts out most non-synthetic PAC2002 files in the sweep; a one-word alias would be a policy change |
| Sign function in Ey | MF 6.1-family cross-check | Fy0 (and Mx0, Mz0 through it): 157 of 2074 tight-tolerance rows with \|SA\| < \|SHy\| and sign(SA) != sgn+(SA + SHy). The oracle uses sign(alpha), with sign(0) = 0, in Ey; ours uses sgn+(alpha_y). Max 7.3e-2 N (rel 2.2e-4) | REF-DEV | None (mf52.md section 4, step 5) |
| tan(alpha) in the combined weights | MF 6.1-family cross-check | Combined Fx/Fy and Mx/My beyond \|SA\| > 0.05 (max Fx 369 N). The oracle uses `alpha* = tan(SA)*sgn(Vx)` in the combined weights; ours uses raw alpha. Passes the small-angle tolerance inside SMALLA | REF-DEV | None (mf52.md section 1; section 4, step 7) |
| tan(alpha) in alpha_t and alpha_r | MF 6.1-family cross-check | Mz0 and Mz (max 11.7 N*m; S52 up to 890 N*m near \|SA\| = pi/2). The oracle uses `alpha* = tan(alpha)` in alpha_t, alpha_r | REF-DEV | None |
| cos'(alpha) applied twice in Mzr | MF 6.1-family cross-check | Mz0 and Mz, Dr != 0 and alpha != 0 (up to 1.6e-3 N*m isolated). The oracle applies cos'(alpha) twice in Mzr; ours once (mf52.md section 4, steps 6 and 8; conventions.md section 6) | REF-DEV | None |
| Machine-epsilon guard | MF 6.1-family cross-check | Mz0, Fx0, Fy0. The oracle guards denominators with machine epsilon; ours with 1e-6 (policies.md section 2). Fx0 rel 6.8e-10 | REF-DEV | None |
| alpha_eq form | MF 6.1-family cross-check | Combined Mz, SR != 0 (up to 0.99 N*m on the second set, 1.55 N*m on S52). The oracle uses a linear alpha_eq form; this project uses the alpha_eq equations of pac2002.md section 3 (section 5.2) | REF-DEV | None |
| No E <= 1 limit | MF 6.1-family cross-check | Fx0, Fx, My via Fx, Mz: EACT rows (636). The oracle has no E <= 1 limit; max 3.29e4 N at FZMAX braking where Ex = 1.70 | REF-DEV | None (pac2002.md section 4, E limit). Rows excluded from the gate |
| MF 6.1 camber terms | MF 6.1-family cross-check | All channels with IA != 0 (3772 rows per fixture). The oracle implements 6.1 camber (SVyg, Kyg0, no PHY3, gamma* = sin(gamma)) | OUT-OF-REGIME | None. Fx0 failures among camber rows (525) are all EACT |
| Clamped rows (MF 6.1-family) | MF 6.1-family cross-check | CLAMP rows (1964 on the second set). With inputs pre-clamped (policies.md section 1) the comparable rows show only the classes above | OUT-OF-REGIME | None |
| Fz <= 0 and Vx <= 0 rows (MF 6.1-family) | MF 6.1-family cross-check | ZERO rows and Vx <= 0 (30 rows not run) | OUT-OF-REGIME | None (policies.md section 1) |
| PROPERTY_FILE_FORMAT 'USER' | MF 5.2 reference comparison, MF 6.1-family cross-check, property-file sweep | The second MF 5.2 parameter set was rejected: `unknown PROPERTY_FILE_FORMAT 'USER' (expected 'PAC2002' or 'MF_05')`. The file carries PFF = 'USER', a USER_SUB_ID key and FITTYP = 6. 'USER' occurs in 7 of the 40 sweep files, always with FITTYP 6, 61 or 62. With the PFF line removed it loads as MF 5.2 with only the foreign-key (LGAX/LGAY/LGAZ) and USE_MODE 14 warnings | OPEN | At validation time the code rejected every PROPERTY_FILE_FORMAT other than PAC2002 and MF_05 and so rejected the file. **FIXED** by the policy change: 'USER' and 'MF-TYRE' route by FITTYP like an absent PFF; unknown strings and every unsupported FITTYP still error. Code: `KNOWN_FORMATS` (PAC2002, MF_05, USER, MF-TYRE) in `src/io/model_routing.cpp`. Tests (`tests/unit/model_routing_test.cpp`): `Route_UserPffFittyp6_Mf52NoWarning`, `Route_UserPffLowerCaseFittyp6_Mf52NoWarning`, `Route_MfTyrePffFittyp6_Mf52NoWarning`, `Route_UserPffFittyp61_Mf6xNotImplemented`, `Route_UserAndMfTyrePffFittyp5_UnsupportedModel`, `Route_UserAndMfTyrePffOtherFamilies_UnsupportedModel`, `Route_UserPffWithoutFittyp_CannotIdentify`, `Route_UserPffUnrelatedKey_StaysUnconsumed`, `Route_EmptyPff_UnsupportedModel`. Docs: model-routing.md (the routing rule for 'USER' and 'MF-TYRE'), open-questions.md. The comparisons above still used the copy with the PFF line removed; the rerun on the unmodified file is open (section 6) |
| Foreign-key warnings | Property-file sweep | Loader diagnostics: no foreign-key warning for foreign pressure (PPX*, PPY*, PPZ*, PPMX1), turn-slip (PDXP1-3, PKYP1, PDYP1-4, PHYP1-4, PECP1-2, QDTP1, QCRP1-2, QBRP1, QDRP1-2), Q_FYS1-3, [STRUCTURAL] and [INERTIA] keys present in an MF 5.2 file; `FOREIGN_KEYS` has 30 of the keys that mf52.md section 3 lists. Numerics unaffected | OUR-BUG (low) | Confirmed at validation time. **FIXED**: `FOREIGN_KEYS` in `src/io/mf5x_bindings.cpp` now has 70 keys (mf52.md section 3) and `FOREIGN_SECTIONS` covers STRUCTURAL, INERTIA and CONTACT_PATCH. Tests (`tests/unit/model_loader_test.cpp`): `Load_Mf52EveryForeignKey_WarnsWithThatKeyAndConsumesIt`, `Load_Mf52PressureTurnSlipStructuralInertia_OneWarningNamesEach`, `Load_Mf52InertiaMassOnly_WarningIsLocatedAtInertiaMassNotUnitsMass`, `Load_Mf52ContactPatchSection_AllKeysWarnedAndConsumed`, `Load_Mf52UnitsMassOnly_NoForeignWarning`, `Load_Pac2002SameKeys_NoForeignKeyWarningAndKeysIgnored` |
| [SHAPE] without '{' | Property-file sweep | 4 files: `[SHAPE]` without '{' gives SYNTAX (policies.md section 4). Behind it FITTYP 5 + MF_05, a partial R* set and TYRESIDE 'UNKNOWN' also reject | OUT-OF-REGIME | None (policies.md sections 3 and 4) |
| Conflicting duplicate keys | Property-file sweep | 2 files with conflicting duplicate keys give DUPLICATE_KEY | OUT-OF-REGIME | None (tir-format.md section 2 rule 10) |
| Unsupported FITTYP or PFF reported first | Property-file sweep | 15 files with FITTYP 61/62 or unknown PFF 'USER'/'MF-TYRE' reported first | OUT-OF-REGIME | None (model-routing.md; MF 6.x not implemented) |
| Reader line endings (sweep) | Property-file sweep | One comparison reader drops characters on LF lines; the independent lexer gives no line number for a NUL byte | ADAPTER | CRLF-normalised copies; no mismatch remains |

## 6. What remains open

- **Rerun after the fixes (open)**: the 'USER' routing and the foreign-key warnings are fixed in the code, but the MF 5.2 reference comparison and the MF 6.1-family cross-check on the unmodified second MF 5.2 parameter set, and the routing sweep (section 4.4), have **not** been re-run with the new routing. Sections 4.1 to 4.4 describe the state before the fixes; the recorded numbers used the copy with the PFF line removed. Until the rerun is recorded, the claim "the second MF 5.2 parameter set loads and matches" rests on the PFF-stripped copy.
- **PROPERTY_FILE_FORMAT 'USER' / 'MF-TYRE' routing** (open-questions.md): fixed (section 5; routing rule for 'USER' and 'MF-TYRE' in model-routing.md). Only the rerun above is open.
- **Foreign-key warnings**: fixed (section 5). Only the sweep rerun is open.
- **Low-speed My taper**: whether My should be tapered or change sign with the wheel rolling speed is open (open-questions.md); the taper is not adopted.
- **Committed reference data**: `tests/data/reference/mf52_mfeval.csv` (used by `tests/module/mfeval_reference_test.cpp`) is kept (policies.md section 6); origin and licence notice in `tests/data/reference/README.md`. No non-synthetic property file is in the repository.
- **Coverage gaps in the evidence**: MF 5.2 QSY2-4, non-unit scale factors, LFZO != 1 and LGA* != 1 have no reference comparison; the friction-ellipse method and the LFZO-normalised dfz have no independent PAC2002 check; Vx = 0 behaviour is not independently validated; MF 5.2 has no check that is independent of its equation set.
- **[UNITS] usability** (not a bug): 'radian' is common in non-synthetic property files; an alias would be a policy decision.

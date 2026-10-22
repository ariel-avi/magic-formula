# Model routing: FITTYP and PROPERTY_FILE_FORMAT

Repo rule: an unknown value gives an explicit error, never a silent fallback.

## Facts

- **SIM20 p.36 [PDF 40]:** FITTYP 5 = MF-Tyre 5.0, 5.1; 6 = MF-Tyre 5.2; 21 = MF-Swift 1.x (based on MF-Tyre 5.2); 51 = MF-MCTyre 1.0; 52 = MF-MCTyre 1.1; 60 = MF-Tyre 6.0; 61 = 6.1; 62 = 6.2; 70 = 6.2 + Temperature & Velocity model.
- **SIM20 p.37 [PDF 41]:** '2020.2 only accepts' 6, 60, 61, 62, 70, and 'will exit with an error for all other values'. FITTYP is selected in [MODEL] (p.36) and is Required (p.26).
- **MSC:** FITTYP matters only when it is 5 and QSY1 = QSY2 = 0, which selects eq (88) (p.33). MSC recommends removing FITTYP to avoid that (p.44). The MSC example has no FITTYP (p.49).
- **PROPERTY_FILE_FORMAT** does not appear among the SIM20 [MODEL] rows, so it is treated as an ADAMS key (open question, open-questions.md).
- **Implementations [impl]:**
  - MFEVAL accepts FITTYP {6, 61, 62} with the error 'not compatible with Magic Formula 5.2, 6.1 or 6.2' (mfeval.m:130-131). It stores PROPERTY_FILE_FORMAT verbatim and never uses it (readTIR.m:478-480).
  - CHRONO accepts only 'PAC2002' and 'MF_05' and ignores FITTYP.
  - TEASIT maps FITTYP {6, 21} to v52 and cannot evaluate it.
- **Observed values [impl data]:**
  - 'PAC2002' (CHRONO bundled files);
  - 'MF_05' with FITTYP 5 (Goodyear-derived feda files);
  - 'USER' with USER_SUB_ID 815 (TNO MF-Tyre ADAMS user subroutine) and FITTYP 6/61/62 in all MFEVAL 4.0 doc/examples files (MagicFormula52/61/62_Paramerters.tir, FSAE_Defaults, PacejkaBook_Defaults, Model_Unstable);
  - 'USER' with USER_SUB_ID 901 and FITTYP 61 (gab-s98);
  - 'MF-TYRE' with FITTYP 62 (simscape car235_80R17.tir).
- [lead] Altair: PROPERTY_FILE_FORMAT is case-insensitive.

## Routing table [policy]

Normalisation: strip quotes and whitespace, compare case-insensitively. FITTYP must parse as an integer, otherwise it is a load error.

| PROPERTY_FILE_FORMAT | FITTYP | Result |
|---|---|---|
| PAC2002 | absent | PAC2002 |
| PAC2002 | 5 | PAC2002, with eq (88) when QSY1 = QSY2 = 0 (MSC p.33) |
| PAC2002 | other integer | PAC2002; warning 'FITTYP <n> ignored for PAC2002 (MSC p.33)' |
| absent, MF_05, USER or MF-TYRE | 6 | MF 5.2 |
| not PAC2002 | 5 | error: MF-Tyre 5.0/5.1 not supported (SIM20 p.37 rejects it too). The message suggests setting PROPERTY_FILE_FORMAT = 'PAC2002' deliberately (MSC p.2) |
| not PAC2002 | 21, 51, 52 | error: not supported (SIM20 p.37) |
| any | 60, 61, 62, 70 | error: MF 6.x not implemented |
| not PAC2002 | absent or other | error: cannot identify the formulation |
| any string other than PAC2002, MF_05, USER or MF-TYRE | any | error: unknown PROPERTY_FILE_FORMAT |

Why PAC2002 takes precedence: a file that declares PAC2002 was written for the ADAMS equations, and MSC uses FITTYP only as a sub-flag. A TNO MF 5.2 file is identified by FITTYP, a Required key. Confidence: medium.

Why 'USER' and 'MF-TYRE' are accepted (open question, open-questions.md): PROPERTY_FILE_FORMAT is an ADAMS dispatch key and not a SIM20 [MODEL] row. 'USER' (with USER_SUB_ID) is the TNO MF-Tyre ADAMS user-tire form, and every MFEVAL 4.0 example file, including the only real MF 5.2 file we hold (FITTYP 6), uses it. 'MF-TYRE' is the other TNO spelling observed. A TNO file is identified by FITTYP (the rationale above), so both values route exactly like an absent PROPERTY_FILE_FORMAT: only the 'not PAC2002' rows apply. Hence 'USER' with FITTYP 5, 21, 51, 52 or absent still errors, and 'USER' with FITTYP 60-70 still gives the MF 6.x error. Accepting every non-PAC2002 string was rejected: a typo would silently route by FITTYP. USER_SUB_ID needs no handling (it loads without a warning). The values stay [impl data]: the open question remains open for primary MSC/Altair evidence.

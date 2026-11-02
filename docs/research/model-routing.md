# Model routing: FITTYP and PROPERTY_FILE_FORMAT

Project rule: an unknown value gives an explicit error, never a silent fallback.

## FITTYP and PROPERTY_FILE_FORMAT values

- FITTYP values: 5 = MF 5.0/5.1; 6 = MF 5.2; 21 = MF-Swift 1.x (based on MF 5.2); 51 = MF-MCTyre 1.0; 52 = MF-MCTyre 1.1; 60 = MF 6.0; 61 = MF 6.1; 62 = MF 6.2; 70 = MF 6.2 with the temperature and velocity extension.
- FITTYP is a [MODEL] key and is required for MF 5.2 and MF 6.1.
- In PAC2002, FITTYP is only a sub-flag: FITTYP = 5 with QSY1 = QSY2 = 0 selects the SHIFTS form of My (pac2002.md section 3); any other integer except 60, 61, 62 and 70 (the version-specific routing rows below) is ignored with a warning.
- PROPERTY_FILE_FORMAT names the formulation family. The known values are 'PAC2002', 'MF_05', 'USER' and 'MF-TYRE'. 'USER' usually comes with a USER_SUB_ID key. 'USER' and 'MF-TYRE' do not name a formulation; files carrying them are identified by FITTYP.

## Routing table

Normalisation: strip quotes and whitespace, compare case-insensitively. FITTYP must parse as an integer, otherwise it is a load error.

| PROPERTY_FILE_FORMAT | FITTYP | Result |
|---|---|---|
| PAC2002 | absent | PAC2002 |
| PAC2002 | 5 | PAC2002, with the SHIFTS My form when QSY1 = QSY2 = 0 |
| PAC2002 | other integer | PAC2002; warning 'FITTYP <n> ignored for PAC2002' |
| absent, MF_05, USER or MF-TYRE | 6 | MF 5.2 |
| not PAC2002 | 5 | error: MF 5.0/5.1 not supported. The message suggests setting PROPERTY_FILE_FORMAT = 'PAC2002', so that evaluating such a file with the PAC2002 equations is an explicit choice |
| not PAC2002 | 21, 51, 52 | error: not supported |
| any known format or absent | 61 | MF 6.1 |
| any | 60, 62, 70 | error: formulation not implemented |
| not PAC2002 | absent or other | error: cannot identify the formulation |
| any string other than PAC2002, MF_05, USER or MF-TYRE | any | error: unknown PROPERTY_FILE_FORMAT |

The version-specific FITTYP 61 and unsupported 60/62/70 rows are checked before PAC2002. Unknown PROPERTY_FILE_FORMAT still errors first, and a non-integer FITTYP errors second.

Why PAC2002 otherwise takes precedence: a file that declares PAC2002 was written for the PAC2002 equations, in which FITTYP is only a sub-flag. An MF 5.2 file is identified by FITTYP, a required key of MF 5.2.

Why 'USER' and 'MF-TYRE' are accepted: PROPERTY_FILE_FORMAT dispatches on the formulation family, and 'USER' and 'MF-TYRE' are generic values that do not name one. Files carrying them are identified by FITTYP (the rationale above), so both values route exactly like an absent PROPERTY_FILE_FORMAT: only the 'not PAC2002' rows apply. Hence 'USER' with FITTYP 5, 21, 51, 52 or absent still errors, 'USER' with FITTYP 61 selects MF 6.1, and 'USER' with FITTYP 60, 62 or 70 still gives the unsupported-formulation error. Accepting every non-PAC2002 string was rejected: a typo would silently route by FITTYP. USER_SUB_ID needs no handling (it loads without a warning). The accepted value set is an open item (open-questions.md).

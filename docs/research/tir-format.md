# .tir property file

## 1. Primary statements

- The file 'typically has the extension .tir (although this is not mandatory)' (SIM20 p.24). Do not require the extension.
- Sections are bracketed names (SIM20 p.24). SIM20 itself spells some inconsistently: [CONTACT-PATCH] vs [CONTACT_PATCH] (p.20 vs p.24, p.29), and [TEMPERATURE_COEFFICIENTS] vs [TVX_COEFFICIENTS] (p.24 vs p.34). Neither matters for v1.
- 'If a required parameter is not specified or has no default value, MF-Tyre/MF-Swift will show an error message' (SIM20 p.24).
- SIM20 gives no grammar. Its quote marks are typographic and are not grammar.
- MSC example (p.48-52):
  - '!' header lines;
  - '$' trailing comments ('LONGVL = 16.6 $Measurement speed', p.49);
  - single-quoted strings (TYRESIDE = 'LEFT');
  - three-digit exponents (2.1e+005).
- Section placement in the MSC example (p.49-52):
  - [MDI_HEADER], [UNITS];
  - [MODEL]: PROPERTY_FILE_FORMAT, USE_MODE, VXLOW, LONGVL, TYRESIDE;
  - [DIMENSION]: UNLOADED_RADIUS, WIDTH, ASPECT_RATIO, RIM_RADIUS, RIM_WIDTH;
  - [VERTICAL]: FNOMIN, VERTICAL_STIFFNESS, VERTICAL_DAMPING, BREFF, DREFF, FREFF. FNOMIN is in [VERTICAL], consistent with SIM20 p.27;
  - ranges [LONG_SLIP_RANGE], [SLIP_ANGLE_RANGE], [INCLINATION_ANGLE_RANGE], [VERTICAL_FORCE_RANGE];
  - [SCALING_COEFFICIENTS], [LONGITUDINAL_COEFFICIENTS], [OVERTURNING_COEFFICIENTS], [LATERAL_COEFFICIENTS], [ROLLING_COEFFICIENTS], [ALIGNING_COEFFICIENTS], and turn-slip, contact, dynamic and loaded-radius sections (REV2 tir-format section 2).
- MASS appears in [UNITS] (a unit symbol, SIM20 p.17) and in [INERTIA] (a value, SIM20 p.27).

## 2. Lexical rules ([policy] unless cited)

1. ASCII/UTF-8. LF or CRLF line endings; a CR is stripped. A final line without a newline is accepted. MFEVAL 4.0.0 drops the last two characters of every line, which is correct only for CRLF (readTIR.m:333-358) [impl defect]. Line length is unlimited.
2. Blank and whitespace-only lines are ignored.
3. A full-line comment starts with '$' or '!' as its first non-blank character. Sources: MSC p.48-49; MFEVAL readTIR.m:378-390; CHRONO :421-424.
4. A trailing comment runs from the first '$' outside single quotes; the whitespace before it is trimmed. '!' is not a trailing marker (MFEVAL :402-413, CHRONO, MFJL agree). CHRONO loses one character when no blank precedes '$' (regression test L3).
5. A section header is `[NAME]`, normalised to upper case. Anything after ']' must be a comment or blank.
6. An assignment splits at the first '='. A non-comment, non-header, non-table line without '=' is a load error (MFEVAL crashes on such lines [impl]).
7. Keys are case-insensitive. MFEVAL and MFJL match case-sensitively; real files use upper case.
8. Values: a single-quoted string (quotes stripped; enumerations compared case-insensitively; MFEVAL keeps the quotes [impl]), or a number parsed in the C locale with a full-match strtod (sign, decimal point, e/E exponent with any number of digits). Trailing junk or an empty value for a numeric key is a load error. MFEVAL turns these into NaN silently [impl].
9. Key lookup does not depend on the section, as in MFEVAL. Exception: MASS is read only from [UNITS] (unit) and [INERTIA] (value); MFEVAL decides by occurrence order (readTIR.m:364-368, 443-447).
10. A duplicate key (case-insensitive, other than MASS) is a load error. MFEVAL silently keeps the last one; CHRONO keeps the first. A duplicate table section gives a warning and the first one wins.
11. Unknown sections and keys are kept, ignored and listed in verbose diagnostics. MFEVAL ignores them silently.
12. Table sections ([SHAPE], [DEFLECTION_LOAD_CURVE], [BOTTOMING_CURVE], ...) hold rows without '='; a '{' header line is skipped. They are parsed raw and not used. MFEVAL skips [SHAPE] only.

## 3. Required keys and defaults

| Item | Rule | Basis |
|---|---|---|
| MF 5.2 required | FITTYP, UNLOADED_RADIUS, WIDTH, RIM_RADIUS, FNOMIN | SIM20 p.26-27 |
| PAC2002 required | FNOMIN, UNLOADED_RADIUS | [policy]: the equations use them |
| Kernel-critical (both models) | PCX1, PDX1, PKX1, PCY1, PDY1, PKY1, PKY2 present, else load error | [policy]: a zero gives zero force or a division by zero. SIM20 p.25 lists PDX1, PKX1, PDY1, PKY1 and PKY2 as 'highly recommended' |
| Validation | FNOMIN > 0, UNLOADED_RADIUS > 0, LFZO > 0 | [policy] |
| Scaling factors L* | default 1 | MSC p.17-18; SIM20 p.26; MFEVAL readTIR.m:122-146 |
| Other P*, Q*, R*, S* | default 0, with one warning listing them. PAC2002 partial R* sets follow pac2002.md section 5.5. MF 5.2: TNO's estimation (SIM20 p.24-25) is not implemented | [policy] |
| TYRESIDE | default LEFT | SIM20 p.19; MFEVAL readTIR.m:36 |
| VXLOW | default 1 (used only for the warning) | MSC p.45 'approximately 1 m/s'; MFEVAL :38 |
| LONGVL | no default; required when QSY3 != 0 or QSY4 != 0 | [policy]. The MSC checklist says '1 m/s' (p.45) while the example uses 16.6 (open question, open-questions.md); MFEVAL defaults to 16.7 |
| *_RANGE | clamp only when both limits of a pair are present | clamp rule in policies.md section 1 |

MFEVAL's readTIR defaults are uncited sample numbers (FITTYP 0, PCX1 1.65, PKY1 -20, QBZ1 10, FZMIN 100, KPU/ALP +/-1.5, CAM +/-0.175; readTIR.m:27-317) and must not be adopted.

## 4. [UNITS]

- SI is the default. Allowed symbols (SIM20 p.17): LENGTH 'meter', FORCE 'newton', ANGLE 'radians', MASS 'kg', TIME 'second' (TEMPERATURE 'kelvin' is FITTYP 70 only). Behaviour for other values or a missing section is not stated.
- [policy] v1:
  - a missing section or key means SI;
  - a present key must equal its SI symbol, compared case-insensitively, otherwise it is a load error naming the key and value;
  - PRESSURE 'pascal' is accepted and ignored;
  - no aliases are accepted (policies.md section 3).
- Why not convert:
  - no primary source says which keys carry which dimension;
  - MFEVAL and TEASIT do no conversion at all (readTIR.m:431-452);
  - CHRONO converts some keys only, never applies its angle factor, and inverts the KN and KPOUND factors.
- No angle-exponent table is needed, because a non-radian ANGLE is rejected. RVY4's exponent is correct: RVY4 multiplies alpha in (76), so its exponent A^-1 holds.

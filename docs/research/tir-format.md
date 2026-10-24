# .tir property file

## 1. File structure

- The file usually has the extension .tir, but the extension is not required.
- Sections are bracketed names.
- A missing required key is a load error (section 3).
- Features that occur in property files: '!' header lines; '$' trailing comments (`LONGVL = 16.6 $Measurement speed`); single-quoted strings (`TYRESIDE = 'LEFT'`); three-digit exponents (`2.1e+005`).
- Typical layout:
  - [MDI_HEADER], [UNITS];
  - [MODEL]: PROPERTY_FILE_FORMAT, USE_MODE, VXLOW, LONGVL, TYRESIDE;
  - [DIMENSION]: UNLOADED_RADIUS, WIDTH, ASPECT_RATIO, RIM_RADIUS, RIM_WIDTH;
  - [VERTICAL]: FNOMIN, VERTICAL_STIFFNESS, VERTICAL_DAMPING, BREFF, DREFF, FREFF;
  - ranges [LONG_SLIP_RANGE], [SLIP_ANGLE_RANGE], [INCLINATION_ANGLE_RANGE], [VERTICAL_FORCE_RANGE];
  - [SCALING_COEFFICIENTS], [LONGITUDINAL_COEFFICIENTS], [OVERTURNING_COEFFICIENTS], [LATERAL_COEFFICIENTS], [ROLLING_COEFFICIENTS], [ALIGNING_COEFFICIENTS], and turn-slip, contact, dynamic and loaded-radius sections.
- MASS appears in [UNITS] (the mass unit) and in [INERTIA] (a mass value).

## 2. Lexical rules

1. ASCII/UTF-8. LF or CRLF line endings; a CR is stripped. A final line without a newline is accepted. Line length is unlimited.
2. Blank and whitespace-only lines are ignored.
3. A full-line comment starts with '$' or '!' as its first non-blank character.
4. A trailing comment runs from the first '$' outside single quotes; the whitespace before it is trimmed. '!' is not a trailing marker. With no blank before '$' the value ends at the '$' (test L3: 'X = 1.0$c' gives 1.0).
5. A section header is `[NAME]`, normalised to upper case. Anything after ']' must be a comment or blank.
6. An assignment splits at the first '='. A non-comment, non-header, non-table line without '=' is a load error.
7. Keys are case-insensitive (files normally use upper case).
8. Values: a single-quoted string (quotes stripped; enumerations compared case-insensitively), or a number parsed in the C locale with a full-match strtod (sign, decimal point, e/E exponent with any number of digits). Trailing junk or an empty value for a numeric key is a load error.
9. Key lookup does not depend on the section. Exception: MASS is read only from [UNITS] (unit) and [INERTIA] (value).
10. A duplicate key (case-insensitive, other than MASS) is a load error. A duplicate table section gives a warning and the first one wins.
11. Unknown sections and keys are kept, ignored and listed in verbose diagnostics.
12. Table sections ([SHAPE], [DEFLECTION_LOAD_CURVE], [BOTTOMING_CURVE], ...) hold rows without '='; a '{' header line is skipped. They are parsed raw and not used.

## 3. Required keys and defaults

| Item | Rule | Reason |
|---|---|---|
| MF 5.2 required | FITTYP, UNLOADED_RADIUS, WIDTH, RIM_RADIUS, FNOMIN | the required keys of the MF 5.2 parameter set (mf52.md section 3) |
| PAC2002 required | FNOMIN, UNLOADED_RADIUS | the equations use them |
| Kernel-critical (both models) | PCX1, PDX1, PKX1, PCY1, PDY1, PKY1, PKY2 present, else load error | a zero gives zero force or a division by zero |
| Validation | FNOMIN > 0, UNLOADED_RADIUS > 0, LFZO > 0 | the equations divide by FNOMIN and F'z0 = FNOMIN*LFZO; R0 scales every moment and the trail, so R0 <= 0 is not physical |
| Scaling factors L* | default 1 | a scale factor of 1 leaves the equation unchanged |
| Other P*, Q*, R*, S* | default 0, with one warning listing them. PAC2002 partial R* sets follow pac2002.md section 5.5. MF 5.2: missing coefficients are not estimated from other ones | policies.md section 3 |
| TYRESIDE | default LEFT | policies.md section 3 |
| VXLOW | default 1 (used only for the flag and the warning) | a typical low-speed threshold of about 1 m/s |
| LONGVL | no default; required when QSY3 != 0 or QSY4 != 0 | LONGVL is the reference speed of the QSY3/QSY4 speed terms; a guessed default would change My silently (open item, open-questions.md) |
| *_RANGE | clamp only when both limits of a pair are present | clamp rule in policies.md section 1 |

## 4. [UNITS]

- SI is the default. The SI unit names are LENGTH 'meter', FORCE 'newton', ANGLE 'radians', MASS 'kg', TIME 'second'. TEMPERATURE is not used.
- Policy:
  - a missing section or key means SI;
  - a present key must equal its SI symbol, compared case-insensitively, otherwise it is a load error naming the key and value;
  - PRESSURE 'pascal' is accepted and ignored;
  - no aliases are accepted (policies.md section 3).
- Why not convert: converting needs the dimension of every key (for example, RVY4 multiplies alpha in DVyk, so it carries 1/angle), and a partial conversion silently mixes units; accepting SI only and rejecting anything else is the safe rule.
- No angle-exponent table is needed, because a non-radian ANGLE is rejected.

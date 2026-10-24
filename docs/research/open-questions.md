# Open questions

## 1. Open items

| Question | Current position | Implemented per |
|---|---|---|
| Moment-arm normaliser: FNOMIN or F'z0 = FNOMIN*LFZO | FNOMIN; the two differ only when LFZO != 1; tests use LFZO = 1 | policies.md section 2; pac2002.md section 5.1 |
| Low-speed treatment below VXLOW, including whether My should be tapered or change sign with the wheel rolling speed Vr = Vx*(1 + kappa) (low-speed My taper: mf52.md section 5; validation.md section 5) | not applied; rows flagged, CLI warns | policies.md section 1; conventions.md section 5 |
| TYRESIDE mirroring transform; SYMMETRIC | no mirroring; LEFT/RIGHT accepted, other values NOT_IMPLEMENTED | policies.md section 3; conventions.md section 9 |
| FE_METHOD semantics | warning, ignored | policies.md section 3; pac2002.md section 5.5 |
| PAC2002 My output sign | negative for a forward-rolling wheel: the QSY form is negated | policies.md section 2; conventions.md section 4 |
| Positive inclination direction | rotation about +x (wheel top toward -y); no sign conversion | conventions.md sections 1 and 3 |
| Camber in mu_x: gamma_x = gamma*LGAX or raw gamma | gamma_x; V16 pins it; identical when LGAX = 1 | pac2002.md section 4 |
| PROPERTY_FILE_FORMAT values and case | 'PAC2002', 'MF_05', 'USER', 'MF-TYRE', case-insensitive; 'USER' and 'MF-TYRE' route by FITTYP like an absent key | policies.md section 3; model-routing.md |
| Estimation of missing MF 5.2 coefficients | not implemented; zeros with a warning | policies.md section 3 |
| alpha_eq sign at exactly alpha_t = 0 (or alpha_r = 0) | sgn+ (continuous): sgn+(0) = +1, a single-point deviation from sgn(0) = 0, which would collapse alpha_eq to 0 there and make Mz discontinuous in alpha when kappa != 0. Closed when the user accepts or rejects this single-point deviation | conventions.md section 7 |
| eps value | 1e-6, applied as d + eps*sgn+(d) | conventions.md section 7; policies.md section 2. Closed when the user confirms the value |
| LONGVL when absent | required iff QSY3 != 0 or QSY4 != 0; otherwise an internal placeholder of 1 m/s | policies.md section 3; tir-format.md section 3 |

## 2. Settled points

- [UNITS] aliases: none; SI only (policies.md section 3).
- FITTYP 5 or 21 under MF 5.2: error, no opt-in (policies.md section 3).
- Extra outputs (t, Mzr, Kx, Ky): none. The output vector is Fx, Fy, Mz, Mx, My plus the five 0/1 flags (policies.md section 1).
- Committed reference outputs are kept (policies.md section 6).
- Out of scope: turn slip and transient behaviour (pac2002.md section 1).

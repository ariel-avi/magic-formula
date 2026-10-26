# Conventions shared by both models

This file holds cross-model rules only. The authoritative evaluation orders are pac2002.md section 2 and mf52.md section 4.

## 1. Axes

- The equations take raw inputs and return outputs in the same axis system; there is no data conversion inside the evaluation. A coefficient set belongs to one system: ISO (x forward, y left, z up; a loaded tire has Fz > 0 and dFy/dSA > 0) or SAE (x forward, y right, z down; a loaded tire has Fz < 0 and dFy/dSA < 0). The origin is the road contact point and x is along the wheel heading in the road plane.
- Property-file coefficients belong to one system. FNOMIN is non-zero and its sign is the loaded side (the sign of Fz of a loaded tire).
- Output in the wheel-centre (C) axis system is out of scope.
- The ISO and SAE definitions, the conversion by coefficient sign flips and the identification of the system: coordinate-systems.md.

## 2. Slip

- V_sx = V_x - Omega*R_e; V_sy = V_y; kappa = -V_sx/V_x; tan(alpha) = V_sy/|V_x|. kappa = -1 is wheel lock.
- CLI inputs: SR = kappa; SA = alpha (rad internally); IA = gamma (rad). SA, SR and IA have the same physical definition in ISO and SAE and are never converted by the evaluation. Degrees are converted once, at the I/O boundary.

## 3. Inclination sign

- Positive gamma is a rotation about +x (the wheel top moves toward -y, to the right of the heading). Its physical definition is the same in ISO and SAE. Whether the user's ISO figure leans the positive angle toward +y is open (open-questions.md).
- Plausibility check: for a car tire, positive inclination gives a negative lateral force at zero slip angle. With z up this is dFy/dIA < 0; in SAE the sign is reversed. The identification uses it as a vertical-axis vote (coordinate-systems.md section 7).
- The IA sign is never converted; only the coefficients change between systems (open item, open-questions.md).

## 4. Output signs

- Fx, Fy, Mx, Mz: the equation values, unchanged. A coefficient set valid in ISO gives Kx > 0, Ky > 0 (dFy/dSA), FNOMIN > 0, Dx > 0, Dy > 0 and QSY1 > 0. In SAE the vertical flips of coordinate-systems.md section 4 apply.
- **My:** My_out = -(QSY-form value) for PAC2002 (also for the SHIFTS form). The PAC2002 SHIFTS form carries the factor sgn(FNOMIN): My_out = -sgn(FNOMIN)*R0*(SVx + Kx*SHx), so My flips under the vertical transform. The MF 5.2 form already carries the minus (mf52.md section 4, step 9), so it is not flipped again.
  - Why: a forward-rolling wheel spins about +y. A rolling-resistance moment (Fz acting ahead of the contact centre at distance e) is (e x) cross (Fz z) = -e*Fz*y, i.e. negative.
  - The QSY form as written gives My > 0 for QSY1 > 0; the output is its negative, so My < 0 for a forward-rolling wheel (open item, open-questions.md).
  - test-data.md gives PAC2002 My expectations in this output sign.

## 5. Speed

- Vx < 0 is rejected for both models.
- PAC2002: Vx enters only the QSY form of My, through |Vx/LONGVL| and (Vx/LONGVL)^4. Vx = 0 is accepted.
- MF 5.2: Vx > 0 is required (policies.md section 1). Its trail factor cos'(alpha) = Vx/(Vc + eps) vanishes at Vx = 0, and the project defines no standstill model, so such rows are rejected.
- **VXLOW.** VXLOW is the speed below which a low-speed treatment would scale forces and moments down. It is not applied: rows with Vx < VXLOW are flagged and the CLI warns. The low-speed tapers considered and not adopted are listed in mf52.md section 5.

## 6. cos(alpha) vs cos'(alpha)

- PAC2002: cos(alpha) in t and Mzr, in pure and combined slip.
- MF 5.2: the combined trail t uses cos'(alpha) = Vx/(Vc + eps), with Vc = sqrt(Vx^2 + Vsy^2) and Vsy = tan(alpha)*|Vx|; Dr uses cos(alpha).
- For Vx > 0, cos'(alpha) = cos(alpha)*Vc/(Vc + eps); the relative difference is eps/Vc.

## 7. sgn and epsilon

- **sgn+(x) = +1 if x >= 0, else -1, everywhere.** Where it matters:
  - Asymmetry terms PEX4*sgn(kappa_x) and PEY3*sgn(alpha_y): no effect, because the arctan argument is 0 whenever the sgn argument is 0.
  - alpha_eq: t and Mzr are even in their argument (Et is held at its pure-slip value computed from the pure alpha_t, pac2002.md section 3), so the sign matters only at exactly alpha_t = 0 or alpha_r = 0. sgn(0) = 0 would collapse alpha_eq to 0 there and make Mz discontinuous in alpha when kappa != 0; sgn+ is continuous. Oddness of alpha_eq itself is not needed, because the outputs are even in it (only sgn(0) = 0 would keep alpha_eq odd). Open item (open-questions.md section 1).
- **eps = 1e-6** (open item, open-questions.md section 1). It is applied as d + eps*sgn+(d) to: Cx*Dx (in Bx); Cy*Dy (in By); Ky (in SHf and in Kx/Ky); Kx and Ky in the friction-ellipse kappa_c and alpha_c; and, for MF 5.2 only, Vc.
- FNOMIN must be non-zero (its sign is the loaded side); LFZO and R0 must be > 0. Validated at load time.

## 8. Limits and load

- Rule: the MF inputs Fz, kappa, alpha and gamma beyond [*_RANGE] are evaluated at the limit, with one warning per variable. The ranges limit only the MF inputs.
- A variable is clamped only when both limits of its pair exist. Reported inputs stay unclamped; each output row carries a clamp flag (policies.md section 1).
- Unloaded: Fz zero, NaN or on the opposite side of FNOMIN gives all outputs 0 (policies.md section 1); the rule is `Fz*sgn(FNOMIN) > 0` on both the raw and the clamped Fz. For FNOMIN > 0 this is Fz <= 0. An SAE model (FNOMIN < 0) treats Fz > 0 as unloaded. FZMIN and FZMAX may be negative.
- On the loaded side, 0 < |Fz| < |FZMIN| (ISO): Fz is raised to FZMIN, so outputs jump as Fz -> 0+.
- An ALPMAX of 1.5708 (pi/2 rounded up) exceeds pi/2 by 3.7e-6, so cos(alpha) is slightly negative at that limit; no monotonicity tests are placed there.

## 9. TYRESIDE

- TYRESIDE is LEFT or RIGHT (default LEFT). A tire mounted on the other side is the mirror image of the measured one (characteristics mirrored with respect to zero slip angle); SYMMETRIC would remove conicity and ply-steer. No mirroring transform is defined here.
- No mirroring is done (the CLI has no mounting side). LEFT and RIGHT are accepted case-insensitively and echoed; absent means LEFT. Any other value (for example SYMMETRIC or 'UNKNOWN') gives an explicit not-implemented error (open item, open-questions.md).

## 10. Modes

- USE_MODE values: 0 = Fz only; 1 = Fx, My; 2 = Fy, Mx, Mz; 3 = longitudinal and lateral, not combined; 4 = combined; 11-15 transient; 21-25 advanced transient. A negative value selects mirroring, and +10 adds relaxation behaviour.
- The library caller or a CLI option selects combined (default, mode-4 semantics) or uncombined (mode 3). The file's USE_MODE is parsed and echoed only. Negative values or values >= 10 give a warning. Modes 1 and 2 are not offered in v1.

## 11. Pressure

Neither model has a pressure input: the PAC2002 and MF 5.2 equations contain no pressure term, and MF 5.2 has no INFLPRES, NOMPRES or PP* parameters. Pressure dependence starts with MF 6.1.

A pressure column in the input table is ignored with one warning.

## 12. Units

SI inside the library, angles in rad. For .tir [UNITS] see tir-format.md section 4.

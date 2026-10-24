# Conventions shared by both models

This file holds cross-model rules only. The authoritative evaluation orders are pac2002.md section 2 and mf52.md section 4.

## 1. Axes

- Outputs Fx, Fy, Mx, My and Mz are given in the W axis system: origin at the road contact point, x forward along the wheel heading in the road plane, y to the left, z up, normal to the road. Fz > 0 is a loaded tire (the road pushes the tire along +z).
- Property-file coefficients are assumed to be valid for this axis system.
- Output in the wheel-centre (C) axis system is out of scope.

## 2. Slip

- V_sx = V_x - Omega*R_e; V_sy = V_y; kappa = -V_sx/V_x; tan(alpha) = V_sy/|V_x|. kappa = -1 is wheel lock.
- CLI inputs: SR = kappa; SA = alpha (rad internally); IA = gamma (rad). No sign conversion. Degrees are converted once, at the I/O boundary.

## 3. Inclination sign

- Positive gamma is a rotation about +x (the wheel top moves toward -y).
- Plausibility check: for a car tire, positive inclination gives a negative lateral force at zero slip angle. A coefficient set that gives a positive force was probably fitted in an axis system with y to the right and z down.
- No camber sign conversion: files are assumed to use this convention (open item, open-questions.md).

## 4. Output signs

- Fx, Fy, Mx, Mz: the equation values, unchanged. A coefficient set valid in this convention gives Kx > 0, Ky < 0, Dx > 0, Dy > 0 and QSY1 > 0.
- **My:** My_out = -(QSY-form value) for PAC2002 (also for the SHIFTS form). The MF 5.2 form already carries the minus (mf52.md section 4, step 9), so it is not flipped again.
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
- FNOMIN, LFZO and R0 must be > 0, validated at load time.

## 8. Limits and load

- Rule: the MF inputs Fz, kappa, alpha and gamma beyond [*_RANGE] are evaluated at the limit, with one warning per variable. The ranges limit only the MF inputs.
- A variable is clamped only when both limits of its pair exist. Reported inputs stay unclamped; each output row carries a clamp flag (policies.md section 1).
- Fz <= 0: all outputs 0 (policies.md section 1).
- 0 < Fz < FZMIN: Fz is raised to FZMIN, so outputs jump as Fz -> 0+.
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

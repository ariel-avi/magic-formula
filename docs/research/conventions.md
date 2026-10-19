# Conventions shared by both models

Revision 4. This file holds cross-model rules only. The authoritative evaluation orders are pac2002.md section 2 and mf52.md section 4. Rev-2 conventions section 6 and rev-3 pac2002 section 2 both claimed that role; this revision settles it.

## 1. Axes (D-22)

- MSC: the W-axis origin is the road contact point; both the C- and W-axis systems 'have the ISO orientation' (p.4). The property file 'must always be valid for the TYDEX W-axis system (ISO oriented)' (p.5). Outputs are 'defined in the W-axis system of TYDEX' (p.16). 'PAC2002 only uses coefficients for the TYDEX W-axis (ISO) system' (p.44). Definitions: p.46-47.
- SIM20: ISO sign conventions, Fig. 4.5 with W axes (Fxw..Mzw) and C axes, both labelled 'reference sidewall position left' (p.16 [PDF 20]). VARINF 1-6 are the contact-point outputs (p.41 [PDF 45]).
- [policy] Output Fx, Fy, Mx, My, Mz in the W (contact-point) ISO frame. C-frame output is out of scope.
- Neither manual spells out the axis vectors. ISO orientation means x forward, y left, z up; the MSC p.4 figure draws F_Zw pointing up (visual reading only).

## 2. Slip

- MSC (1)-(4), p.5-6: V_sx = V_x - Omega*R_e; V_sy = V_y; kappa = -V_sx/V_x; tan(alpha) = V_sy/|V_x|.
- SIM20 (4.1)-(4.2), p.16-17, gives the same definitions; kappa = -1 is wheel lock.
- CLI inputs: SR = kappa; SA = alpha (rad internally); IA = gamma (rad). No sign conversion. Degrees are converted once, at the I/O boundary.

## 3. Inclination sign

- Neither MSC nor SIM20 defines it in words (Q-11).
- [derived] In ISO, positive gamma is a rotation about +x_w (wheel top toward -y). This is consistent with MSC p.44: 'for car tires, positive inclination should result in a negative lateral force at zero slip angle. If positive inclination results in an increase of the lateral force, the coefficient may not be valid for the ISO but for the SAE coordinate system.'
- [policy] No camber sign conversion; files are assumed to be ISO.

## 4. Output signs

- Fx, Fy, Mx, Mz: the equation values, unchanged. MSC checklist (p.45): Kx > 0, Ky < 0, Dx > 0, Dy > 0, qsy1 > 0.
- **My (D-11):** My_out = -(MSC equation value) for PAC2002. MF 5.2 already carries the minus (mf52.md step 9), so it is not flipped again.
  - [derived] Why: in ISO-W a forward-rolling wheel spins about +y. A rolling-resistance moment (Fz acting ahead of the contact centre at distance e) is (e x) cross (Fz z) = -e*Fz*y, i.e. negative.
  - MSC (87) gives My > 0 for qsy1 > 0 and states no sign; p.4-5, p.16 and p.46-47 give only the name and unit.
  - MFEVAL's FITTYP-6 branch has the leading minus, 'From the MF-Tyre equation manual' (Solver.m:1506-1508), and comments 'in the ISO coordinate system, My should be negative' (:1498-1501). TEASIT My.m:20 and CHRONO :1893-1895 agree [impl]. OPENTIRE :492 keeps MSC's + (Q-10).
  - test-data.md gives PAC2002 My expectations in this output sign.

## 5. Speed

- Vx < 0 is rejected for both models (D-27).
- PAC2002: Vx enters only (87), through |Vx/LONGVL| and (Vx/LONGVL)^4 (REV2 conventions l.69). Vx = 0 is accepted.
- MF 5.2: Vx > 0 is required [policy]. MFEVAL's standstill cases are not reproduced: cos'(alpha) = 0, the sgn(Vx) factor in Dr, ualpha = 0 at Vx = 0 (:539), My = 0 at Vx = 0 (:1540), the My kappa band near kappa = -1 (Solver.m:1517-1537).
- **VXLOW (D-12).** MSC p.45: 'Threshold for scaling down forces and moments' (no formula). SIM20 p.27: 'Lower boundary velocity in slip calculation'. Not applied; the CLI warns when Vx < VXLOW.
- MFEVAL's law, for reference only [impl]. It is active only with limit checks on and |Vx| <= VXLOW. With w = 0.5*(1 + cos(pi*Vx/VXLOW)), SVx, SHx, SVyk, t and Mzr are scaled by (1 - w); kappa is scaled by |Vx/VXLOW|; alpha by (|Vx| + |Vsy|)/VXLOW (Solver.m:340-397, 797-798, 1381-1384, 1622-1627).

## 6. cos(alpha) vs cos'(alpha)

- PAC2002: cos(alpha) in (44), (46), (80), (82) (MSC p.23, p.31).
- MF 5.2 [impl]: the combined trail t uses cos'(alpha) = Vx/(Vc + eps), with Vc = sqrt(Vx^2 + Vsy^2) and Vsy = tan(alpha)*|Vx| (Solver.m:647-682, :1614). Dr uses cos(alpha) (:1263).
- [derived] For Vx > 0, cos'(alpha) = cos(alpha)*Vc/(Vc + eps). The relative difference is eps/Vc.

## 7. sgn and epsilon (D-21)

- No primary source defines sgn(0) or an epsilon. SIM20 was searched (none); MSC prints none, including in (79)-(85).
- [policy] **sgn+(x) = +1 if x >= 0, else -1, everywhere.** Where it matters:
  - Asymmetry terms PEX4*sgn(kappa_x) and PEY3*sgn(alpha_y): no effect, because the arctan argument is 0 whenever the sgn argument is 0 [derived].
  - alpha_eq (D-05): t(x) and Mzr(x) are even in x when E_t is the pure-slip value (D-24), so the sign only matters at exactly alpha_t = 0 or alpha_r = 0.
    - sgn(0) = 0 (MFEVAL :1581-1582 unguarded, OPENTIRE np.sign, TEASIT) collapses alpha_eq to 0 at that point. When kappa != 0 this makes Mz discontinuous in alpha there.
    - sgn+ (MFJL, CHRONO8) is continuous.
    - Rev-2 said 'only sgn(0) = 0 keeps alpha_t,eq odd'. That is true but irrelevant, because the outputs are even in alpha_eq.
    - This is an accepted deviation from MFEVAL at a single point (Q-09).
- [policy] **eps = 1e-6**, the MFEVAL code value (Solver.m:252, used for all its epsilons). MFEVAL's own doc (changesEquations Sec. 3) says 1e-4; the code wins (Q-08).
  - Apply as d + eps*sgn+(d) to: Cx*Dx (in Bx), Cy*Dy (in By), Ky (in S_Hf and Kx/Ky) and, for MF 5.2 only, Vc.
  - FNOMIN, LFZO and R0 must be > 0, validated at load time.
  - Do not copy CHRONO's 0.1.

## 8. Limits and load (D-03, D-04, D-17)

- Primary rule: inputs beyond [*_RANGE] are evaluated at the limit, with a warning (MSC p.45). SIM20 p.25 warns on the first exceedance only. SIM20 p.24 groups the ranges as 'Input limitations (only for Magic Formula inputs)'.
- [policy] Clamp a variable only when both limits of its pair exist. Reported inputs stay unclamped; each output row carries a clamp flag.
- Fz <= 0: all outputs 0 [policy]. MFEVAL is equivalent: it sets Fz<0 to 0 and returns zero forces and moments at Fz = 0 [impl].
- 0 < Fz < FZMIN: Fz is raised to FZMIN (primary), so outputs jump as Fz -> 0+ [derived]. MFEVAL keeps Fz and applies low-load factors only in Mx (Fz*(Fz/FZMIN)^2) and My (Fz*(Fz/FZMIN)) (Solver.m:415-422, 1393-1394, 1451-1459). Not adopted [impl].
- ALPMAX = 1.5708 exceeds pi/2 by 3.7e-6, so cos(alpha) is slightly negative at the limit. No monotonicity tests there [derived].

## 9. TYRESIDE

- SIM20 p.19: 'LEFT' or 'RIGHT', default 'LEFT'. A tire mounted on the other side is mirrored. Side code 3 = symmetric, 4 = mirror. The transform is not given.
- MSC p.41: characteristics are 'mirrored with respect to slip angle zero'; SYMMETRIC removes conicity and plysteer. No formula.
- [policy] No mirroring (the CLI has no mounting side). LEFT and RIGHT are accepted case-insensitively and echoed; absent means LEFT. Any other value (SYMMETRIC, CHRONO's 'UNKNOWN') gives an explicit not-implemented error (Q-05).

## 10. Modes

- MSC p.43 USE_MODE: 0 = Fz only; 1 = Fx, My; 2 = Fy, Mx, Mz; 3 = longitudinal and lateral, not combined; 4 = combined; 11-15 transient; 21-25 advanced transient. The p.49 comments add '*-1: mirroring' and '+10: including relaxation behaviour'.
- SIM20: USE_MODE is a 'Tire use mode switch (Adams only)' (p.27); slip-forces modes 0-5 are on p.22.
- [policy] The library caller or a CLI option selects combined (default, mode-4 semantics) or uncombined (mode 3, D-26). The file's USE_MODE is parsed and echoed only. Negative values or values >= 10 give a warning. Modes 1 and 2 are not offered in v1.

## 11. Pressure (D-25)

Neither model has a pressure input:
- MSC eqs (17)-(103) contain none.
- SIM20 column 6 has no INFLPRES, NOMPRES or PP*; pressure dependency arrives with 6.1 (Fig. 4.9, p.36).
- MFEVAL 4.0.0 release note: 'in 5.2 the pressure has no effect'.

A pressure column in the input table is ignored with one warning.

## 12. Units

SI inside the library, angles in rad. For .tir [UNITS] see tir-format.md section 4.

# Tire characteristics: stiffness, peak factors and slip at peak

The eight characteristics computed per operating point for PAC2002 and MF 5.2: the tangent and secant stiffnesses of the planar forces, the peak force factors and the slips at the peaks. This note holds the definitions, the closed-form derivatives, the peak condition and the citations. The project rules that the sources do not fix are in policies.md section 9. Test vectors: test-data.md section 6.

Evidence classes used below:

- **Cited**: stated by a source listed in section 8 (the slide, page or table is named).
- **Derived**: the project's own algebra on the equations of pac2002.md section 3 and mf52.md section 4. No external source states it; the closed forms are the test oracles.
- **Policy**: a project rule where the sources are silent (policies.md section 9).

## 1. Definitions

All channels are evaluated at the operating point of a row. SA is the slip angle alpha (rad), SR the slip ratio kappa (dimensionless). Fx and Fy are the planar forces of the active mode: the combined Fx and Fy in combined mode, the pure-slip Fx0 and Fy0 in uncombined mode (conventions.md section 10). The other inputs of a row (Fz, gamma and the other slip) are held at the row's values.

| Channel | Symbol | Unit | Meaning |
|---|---|---|---|
| DFY_DSA | dFy/dSA | N/rad | Tangent (instantaneous) cornering stiffness: the derivative of the evaluated Fy with respect to SA at the row |
| DFX_DSR | dFx/dSR | N | Tangent (instantaneous) longitudinal stiffness: the derivative of the evaluated Fx with respect to SR at the row |
| FY_OVER_SA | Fy/SA | N/rad | Secant cornering stiffness: the evaluated Fy divided by the raw SA of the row |
| FX_OVER_SR | Fx/SR | N | Secant longitudinal stiffness: the evaluated Fx divided by the raw SR of the row |
| DY | mu_y*Fz | N | Peak lateral force factor: the pure-slip D parameter Dy at the row's load |
| DX | mu_x*Fz | N | Peak longitudinal force factor: the pure-slip D parameter Dx at the row's load |
| SA_AT_PEAK_FY | alpha at the peak of Fy | rad | Slip angle of the first stationary point of Fy(SA) from zero, on the side of the row's SA (section 5) |
| SR_AT_PEAK_FX | kappa at the peak of Fx | - | Slip ratio of the first stationary point of Fx(SR) from zero, on the side of the row's SR (section 5) |

Tangent and secant: a tangent is the derivative at the operating point; a secant is the force divided by the slip, that is the slope of the line from the origin to the operating point. The two coincide only where the force is linear in the slip.

### 1.1 The model stiffness parameters Kx and Ky (cited, derived)

- Cited: for F(x) = D sin[C arctan{Bx - E(Bx - arctan Bx)}] + SV with x = X + SH, "BCD determines the slope near the origin" (TNO slide 7). Kx is the longitudinal slip stiffness and Bx is calculated from Cx, Dx and Kx (TNO slide 8). "BCD represents the cornering stiffness" (Auburn HW6, p. 1). PKX1*FNOMIN is the longitudinal slip stiffness at the nominal load, PKY1*FNOMIN the maximum value of the cornering stiffness and PKY2*FNOMIN the load at which it is reached (Siemens table, as reproduced on the Altair help page). The last agrees with Ky0 = PKY1*F'z0*sin(2*atan(Fz/(PKY2*F'z0)))*LKY of pac2002.md section 3, which peaks at Fz = PKY2*F'z0.
- Repository form: Bx = Kx/(Cx*Dx + eps) and By = Ky/(Cy*Dy + eps) with eps added sign-preserving (pac2002.md section 3; conventions.md section 7).
- Derived: the slope of the pure curve at x = 0 is B*C*D = K*|CD|/(|CD| + eps). It differs from K by the relative factor eps/|CD|, about 1.7e-10 for CD = 6000, so oracles must include eps exactly as the kernel does.
- Derived: DFX_DSR and DFY_DSA equal Kx and Ky (to within eps) only in pure slip at the shifted origin (slip = -SH) or where the combined-slip weighting is 1 and does not depend on the own slip. At a raw slip of 0 with SH != 0 the tangent is the slope at x = SH, which differs from K. In combined slip the tangent differs from K by design (sections 2.2 and 2.3).
- The characteristics DFX_DSR and DFY_DSA are the tangents of the evaluated force. They are not the model parameters Kx and Ky and are not labelled as such.

### 1.2 MFeval Kxk and Kya

Not determined. The MFeval File Exchange page (63618) states for release 2.3.0 that "Slip stiffness (Kxk) is now output number 30"; it gives no formula, and the solver source was not read. Whether MFeval returns the parameter-based stiffness or a tangent at the operating point is open (open-questions.md section 1). No parity with MFeval Kxk or Kya is claimed.

### 1.3 DY and DX

Cited: Dx = mu_x*Fz and Dy = mu_y*Fz (pac2002.md section 3); D is the peak value of the pure curve (Penn State slide 5, y = D sin(...) with D = y_p) and "D represents the peak side force" (Auburn HW6, p. 1). Policy: DY and DX are the pure-slip D parameters at the clamped Fz and the scaled camber (gamma*LGAY and gamma*LGAX; LGAY = LGAX = 1 for MF 5.2), exactly the Dy and Dx the force kernels use. They are not the peaks of the combined force.

## 2. Closed-form derivatives (derived)

Source forms: pac2002.md section 3 (PAC2002) and mf52.md section 4 steps 4 and 5 (MF 5.2). MF 5.2 runs the same Fx0, Fy0, Gxa, Gyk and SVyk forms as PAC2002 (always the cosine method; alpha, not tan(alpha); the same eps placement) with LGAX = LGAY = LGAZ = 1 (mf52.md sections 3 and 4; pac2002-vs-mf52.md), so every closed form below applies to both. Only PAC2002 can select the friction ellipse (pac2002.md section 5.5).

The derivative is taken with respect to the own slip only: d/dkappa for Fx and d/dalpha for Fy, with the other slip, Fz and gamma held at the row. All shift, scale and load terms are independent of the slips.

### 2.1 Sine and cosine Magic Formula

Notation: u = B*x, phi = u - E*(u - atan u), theta = atan(phi), y = D*sin(C*theta) + SV.

- dphi/dx = B*[(1 - E) + E/(1 + u^2)]
- dy/dx = D*C*cos(C*theta)*(dphi/dx)/(1 + phi^2)
- For the cosine form h(B, x) = cos(C*theta): dh/dB = -C*sin(C*theta)*x*[(1 - E) + E/(1 + u^2)]/(1 + phi^2).

E depends on the own slip only through sgn+ (piecewise constant) and through the clamp E <= 1 (pac2002.md section 4), so dE/dx = 0. At x = 0, dphi/dx = B for every E: the slope is continuous across the shifted origin even though E may jump there.

Pure slip:

- dFx0/dkappa is the formula above with (Bx, Cx, Dx, Ex(sgn+(kappa + SHx))) at x = kappa + SHx.
- dFy0/dalpha is the formula above with (By, Cy, Dy, Ey(sgn+(alpha + SHy))) at x = alpha + SHy.
- At x = 0 the slope is B*C*D = K*|CD|/(|CD| + eps), not exactly K (section 1.1).

### 2.2 Cosine method (PAC2002 and MF 5.2)

- Fx = Fx0*Gxa with Gxa = h(Bxa, as)/h(Bxa, SHxa), as = alpha + SHxa, Bxa = RBX1*cos(atan(RBX2*kappa))*LXAL, Cxa = RCX1, Exa = REX1 + REX2*dfz.
- dBxa/dkappa = -RBX1*LXAL*RBX2^2*kappa*(1 + (RBX2*kappa)^2)^(-3/2).
- dG/dB = [h_B(Bxa, as)*h(Bxa, SHxa) - h(Bxa, as)*h_B(Bxa, SHxa)]/h(Bxa, SHxa)^2.
- dFx/dkappa = Fx0'*Gxa + Fx0*(dGxa/dBxa)*(dBxa/dkappa). Gxa depends on kappa only through Bxa.
- Fy = Fy0*Gyk + SVyk with Gyk the form of Gxa with Byk(alpha) = RBY1*cos(atan(RBY2*(alpha - RBY3)))*LYKA, Cyk = RCY1, Eyk = REY1 + REY2*dfz, ks = kappa + SHyk.
- dByk/dalpha = -RBY1*LYKA*RBY2^2*(alpha - RBY3)*(1 + (RBY2*(alpha - RBY3))^2)^(-3/2).
- SVyk = DVyk*sin(RVY5*atan(RVY6*kappa))*LVYKA with DVyk = mu_y*Fz*(RVY1 + RVY2*dfz + RVY3*gamma)*cos(atan(RVY4*alpha)), mu_y at the actual camber. dSVyk/dalpha = -mu_y*Fz*(RVY1 + ...)*RVY4^2*alpha*(1 + (RVY4*alpha)^2)^(-3/2)*sin(RVY5*atan(RVY6*kappa))*LVYKA.
- dFy/dalpha = Fy0'*Gyk + Fy0*(dGyk/dByk)*(dByk/dalpha) + dSVyk/dalpha.

### 2.3 Friction ellipse (PAC2002 only), Fz-free form

Notation, with the definitions of pac2002.md section 5.4 (kc = kappa_c, a* = alpha*):

- kc = kappa + SHx + SVx/Kx and alpha_c = alpha + SHy + SVy/Ky, both with eps-guarded Kx and Ky (Ky is the camber-reduced Ky of the pure Fy0 block, not Ky0); a* = sin(alpha_c).
- g = Fx0 - SVx and h = Fy0 - SVy; mxa = |g|, mya = |h|, mxm = |Dx|, mym = |Dy|, all in N. The 1/|Fz| factors of the printed mu form cancel (the ratios are homogeneous of degree 0), so the Fz-free magnitudes are equivalent to normalising by |Fz| for either sign of Fz. Hence 0 <= rx, ry <= 1, Fx keeps the sign of Fx0 and Fy that of Fy0.
- rx = |kc|*mym/Rx with Rx = sqrt(kc^2*mym^2 + g^2*a*^2); ry = |a*|*mxm/Ry with Ry = sqrt(kc^2*h^2 + a*^2*mxm^2). Fx = Fx0*rx and Fy = Fy0*ry.

Seeds (the derivative is taken along the own slip):

- Along kappa: kc' = 1, g' = Fx0', a*' = 0, h' = 0.
- Along alpha: a*' = cos(alpha_c), h' = Fy0', kc' = 0, g' = 0.
- Dx, Dy, SVx, SVy, SHx, SHy, Kx and Ky do not depend on either slip.

Derivatives:

- Where Rx > 0: d rx/d kappa = mym*a*^2*(sgn+(kc)*g^2 - |kc|*g*Fx0')/Rx^3, and dFx/dkappa = Fx0'*rx + Fx0*(d rx/d kappa).
- Where Ry > 0: d ry/d alpha = mxm*kc^2*(sgn+(a*)*cos(alpha_c)*h^2 - |a*|*h*Fy0')/Ry^3, and dFy/dalpha = Fy0'*ry + Fy0*(d ry/d alpha).
- No 1/Fz and no sgn(g) or sgn(h) appear, because g and h enter only as squares or as g*g' and h*h'.

Corollaries:

- On a* = 0 with kc != 0: rx = 1 and d rx/d kappa = 0, so dFx/dkappa = Fx0'.
- On kc = 0 with a* != 0: ry = 1 and d ry/d alpha = 0, so dFy/dalpha = Fy0'.

### 2.4 Genuine kinks (Rx > 0 or Ry > 0 with the own shifted slip +-0)

|kc| (for rx) and |a*| (for ry) have a genuine kink. The left and right derivatives differ by 2*|F0|*other_max/R, where F0 is the pure force at the kink; this is small but non-zero only when the vertical shift is non-zero (F0 = 0 at the origin when SV = 0). The force is not differentiable there. Policy: the derivative is the right derivative, the one of the sgn+ branch (sgn+(+-0) = +1): with kc' = 1 and a*' = cos(alpha_c) > 0 this is the right derivative in the swept slip. The clamp bounds of the swept slip (section 7) are the other place where only a one-sided value exists.

### 2.5 0/0 points of the ratio (value 1, derivative a limit)

At a point where Rx = 0 (or Ry = 0) the ratio is 0/0 and pac2002.md section 5.4 defines it as 1, so the force equals the pure-slip force there. The derivative of the evaluated force is not the derivative of the constant 1. Write r = |P|/sqrt(P^2 + Q^2) with

- for rx: P = kc*mym and Q = mxa*a*;
- for ry: P = a**mxm and Q = mya*kc.

Then the numerator is |P| (mxm, mym >= 0), the denominator squared is P^2 + Q^2, and R = 0 if and only if P = Q = 0. Along the seed, with P' and Q' the product-rule derivatives:

- dF/ds = F0'*r_lim + F0*r', with r' = 0 (the derivative of the defined constant);
- r_lim = |P'|/sqrt(P'^2 + Q'^2) where P'^2 + Q'^2 > 0, and r_lim = 1 otherwise.

Derivation. Near the point P = P'*e + O(e^2) and Q = Q'*e + O(e^2) for a slip offset e. Because r is homogeneous of degree 0 in (P, Q), r tends to |P'|/sqrt(P'^2 + Q'^2) from both sides. The cases:

- Kind 2 (the own shifted slip and the own actual force are both 0, which needs SV = 0 and slip = -SH, with the other shifted slip non-zero): F0 = 0 exactly, so (F(s0 + e) - F(s0))/e = F0(s0 + e)*r(s0 + e)/e tends to F0'*r_lim. F is differentiable there, and the value assigned to r at the point does not matter. For the own seeds: rx_lim = mym/sqrt(mym^2 + (Fx0'*a*)^2) and ry_lim = mxm/sqrt(mxm^2 + (kc*Fy0')^2) (|cos(alpha_c)| = 1 at a* = 0).
- Kind 1 (both shifted slips are 0): along the own slip the other shifted slip stays 0, so Q' = 0 and r is identically 1; r_lim = 1 and dF/ds = F0', even though F0 may be non-zero.
- P' = Q' = 0 (for example the other axis has mu = 0 and its shifted slip is 0): r is 0/0 along the whole seed line, that is identically 1, so r_lim = 1.
- Any remaining case with F0 != 0 and r_lim != 1 occurs only where F jumps along the seed (other-axis mu = 0 with SV != 0). F is not differentiable there; the rule returns F0'*r_lim, the derivative on both sides of the point.

There is no unit-slope assumption: scaling the seed scales P' and Q' alike, so r_lim is unchanged and dF/ds scales with F0'.

Where it matters. Fixture S has SHx = SVx = SHy = SVy = 0, so with the friction ellipse (S_ell) every row with kappa = 0 and alpha != 0 is a kind-2 point of rx, and every row with alpha = 0 and kappa != 0 is one of ry. These are ordinary pure-slip rows. Example (S_ell, kappa 0, alpha 0.05): rx_lim = 0.7072540921 and DFX_DSR = 56580.32736, against 79999.99999 if the ratio were differentiated as the constant 1 (test-data.md C14).

### 2.6 Corrections to the first derivation

The first draft of the ellipse derivative contained these errors, all corrected above:

- mxa' was written with SVy; the shift is SVx.
- The 1/Fz factors were mixed with the Fz-free magnitudes; the derivation is Fz-free throughout.
- The sgn factors on mxa' and mya' were unnecessary (g and h enter only as squares or g*g', h*h').
- "Ky'" in a*' is the camber-reduced, eps-guarded Ky, not an extra-primed quantity.
- "A 0/0 ratio has derivative 0" is replaced by section 2.5.

## 3. Evaluation method

The derivatives are obtained by exact forward-mode differentiation (dual numbers) of the closed-form kernels. Every elementary operation (addition, multiplication, division, sqrt, atan, sin, cos, exp, abs, min, selects and comparisons) applies its exact derivative rule, so the result is the chain rule applied to the implemented equations: there is no step size, no truncation error and no finite difference anywhere. The result equals the closed forms of section 2 to rounding; the closed forms are the test oracles and are never used by the library.

- The value part of every dual quantity is bit-identical to the plain evaluation, so the characteristics are computed from exactly the forces the model outputs.
- Piecewise operations (abs, sgn+, clamps, min(E, 1), selects) return the derivative of the branch taken. The only place where the derivative of the evaluated force is a limit rather than a chain rule is the ellipse 0/0 set of section 2.5, which is supplied separately and changes only the derivative part.
- The swept input is seeded with derivative 1 when it is inside its clamp range and 0 when it was clamped (the model output is constant there).

## 4. Peak condition and existence

Notation as in section 2.1: u = B*x, T = tan(pi/(2*C)).

### 4.1 Stationary point of the pure curve (cited and derived)

- dy/dx = D*C*cos(C*theta)*(dphi/dx)/(1 + phi^2), and dphi/du = (1 - E) + E/(1 + u^2) > 0 for every finite u when E <= 1. For B != 0 the stationary points of y therefore satisfy cos(C*theta) = 0.
- The first one is C*theta = pi/2, that is |phi| = T. It needs C > 1, because |theta| < pi/2.
- Cited: the peak (x_m, y_m) of y = D sin(C atan(Bx - E(Bx - atan Bx))) has D = y_p and E = (B*x_m - tan(pi/(2C)))/(B*x_m - atan(B*x_m)) "for C > 1" (Penn State slide 5; the same E expression is in the MATLAB code of slide 6; the slides cite Pacejka 2006, Tire and Vehicle Dynamics, 2nd ed.). "B, E and C determine the location of the peak" and "C determines the limit value when x tends to infinity" (TNO slide 7; secondary and not independently verified: the source was located and the statement read from a snippet only, as was the Penn State PDF text beyond the quoted E expression). Primary confirmation of the relation and its equation number is open (open-questions.md section 1).
- The relation inverts as: |u*| solves (1 - E)*|u| + E*atan|u| = T, equivalently E = (|u| - T)/(|u| - atan|u|) (the denominator is non-zero for every u != 0 because t - atan t has the sign of t). For E = 0: |u*| = T. For E = 1: |u*| = tan(T) when T < pi/2.
- Peak slip on the side d = +-1 (d is the sign of x = s + SH), with E taken for that side: s*_d = d*|u*_d|/|B| - SH. This uses sgn(B)/B = 1/|B|, so the result is sign-invariant for B < 0.
- Peak value on side d: F(s*_d) = d*sgn(B)*D + SV for the pure curve; SV is constant and does not move the stationary point. Check on fixture S: By < 0, so the positive side gives Fy = -Dy.

### 4.2 Existence conditions (derived; pure slip, one side, with the E <= 1 clamp of pac2002.md section 4)

1. D != 0 and B != 0 are required. An unloaded row, Ky = 0 and mu = 0 give no peak.
2. C > 1 is required. For C <= 1, |C*theta| < pi/2, so cos(C*theta) > 0; with dphi/du > 0 the slope never changes sign and there is no stationary point.
3. For C > 1 and E <= 1, |phi| increases strictly with |u|, so |phi| = T has at most one solution per side (exactly one for E < 1; for E = 1 see item 5) and it is the first stationary point from x = 0. Further zeros at |C*theta| = 3*pi/2 and beyond (C >= 3) lie farther out; the search takes the first.
4. For E < 1, phi tends to infinity, so the peak always exists.
5. For E = 1, phi = atan(u) < pi/2. The peak exists if and only if T < pi/2, that is C > pi/(2*atan(pi/2)) = 1.5647 (approximately).
6. The slip must also lie inside the search segment (section 5).

For E > 1 (excluded by the clamp) phi would fold back at u^2 = 1/(E - 1) and the inverse E -> u_m would not be unique. The condition "E <= 1 if and only if atan(B*x_m) <= T" is the condition for a fitted E to respect the clamp, given a measured peak location; it is a different question from item 5.

### 4.3 Side asymmetry

Ex has the factor (1 - PEX4*sgn(kx)) with kx = kappa + SHx, and Ey has (1 - (PEY3 + PEY4*gamma)*sgn(ay)) with ay = alpha + SHy (pac2002.md section 3); the code uses sgn+ (conventions.md section 7; mf52.md section 4). The two sides can therefore have different E and different peak slips, E+ for x >= 0 and E- for x < 0. The value of E exactly at x = 0 changes no channel, because phi(0) = 0 and dphi/dx = B whatever E is. Example: PEX1 = 0.5 and PEX4 = 0.4 give E+ = 0.3 and E- = 0.7 (test-data.md C16). No external source on the asymmetry was reached; this rests on the repository notes.

### 4.4 When the closed-form peak and the search agree

The peak search of section 5 starts at the row's raw slip 0 and goes outward on one side, whereas the closed form of section 4.1 is located relative to the shifted origin. They agree if and only if -|u*_-|/|B| < SH < |u*_+|/|B|, that is raw slip 0 lies strictly between the two stationary points. Otherwise, on the positive side, SH >= |u*_+|/|B| gives no stationary point (nan), and SH < -|u*_-|/|B| finds the negative-side point; the negative side mirrors this. Fixtures with SH = 0 always agree.

### 4.5 Scope of the closed form

The closed form of section 4.1 is an oracle for the combined force only where the combined-slip weighting does not depend on the own slip: for Fx when RBX2 = 0; for Fy when RBY2 = 0 and either RVY4 = 0 or DVyk = 0 (the friction ellipse never qualifies except where its ratio is constant). Otherwise the peak is checked by stationarity of the analytic slope plus a sign change around the root (test-data.md C9, C10).

## 5. Peak-slip algorithm (policy)

- **Definition.** The slip angle at the peak of Fy is the first stationary point of the evaluated Fy with respect to SA: the root of the analytic dFy/dSA (section 3) with kappa, Fz and gamma at the row's clamped values and the swept SA not clamped inside the segment. The slip ratio at the peak of Fx is the same for Fx with respect to SR, with SA at the row's clamped value. In uncombined mode the curves are the pure Fy0 and Fx0.
- **Side.** The side is sgn+ of the row's raw slip (+-0 searches the positive side).
- **Segment.** [0, side*min(extent, max(0, side*bound))], with extent pi/2 rad for SA and 1 for SR, and bound = ALPMAX (SA) or KPUMAX (SR) on the positive side and ALPMIN or KPUMIN on the negative side. A bound is infinite when the range is absent (a range is applied only when both limits of its pair exist, conventions.md section 8).
- **Search.** A uniform scan of 32 cells (33 slope evaluations) for the first sign change of the analytic slope (a cell changes sign when the slope at its start is nonzero and the slope at its end is zero or of the opposite sign, so a root exactly on a later cell boundary is found; a slope of exactly 0 at s = 0 followed by a constant sign is not a sign change and gives NaN), then 52 bisection steps in that cell. Sign changes are found with no derivative of the slope, so no second derivative is needed.
- **First, not global.** The first stationary point met moving from 0 outward is returned. Caveat: two stationary points inside one cell (width |segment end|/32) are not resolved.
- **NaN.** No stationary point in the segment (C <= 1, a monotone combined curve, a stationary point beyond the range, an empty segment), and rows without contact.

## 6. Sign parity and ISO/SAE meaning

Parity table (derived; coordinate-systems.md sections 3 and 4). "Negated by" lists the transform under which the channel changes sign when the model and the table are converted: VERTICAL (Fz -> -Fz, transform A) and LATERAL (SA -> -SA, transform B).

| Channel | VERTICAL | LATERAL | Reason |
|---|---|---|---|
| DFY_DSA | yes | yes | Fy is VERTICAL-odd and SA is LATERAL-odd |
| FY_OVER_SA | yes | yes (also at SA = +-0, where it is the tangent) | same as DFY_DSA |
| DY | yes | no | mu_y is unchanged and Fz flips |
| DFX_DSR | no | no | Fx is invariant under both |
| FX_OVER_SR | no | no | same |
| DX | no | no | PDX1, PDX2 and Fz all flip, so Dx is invariant |
| SA_AT_PEAK_FY | no | yes, for SA != 0 (side rule) | the mirrored curve peaks at -SA; at SA = +-0 the positive side is searched in both models |
| SR_AT_PEAK_FX | no | no | same as DFX_DSR |

Facts behind the table:

- Under VERTICAL: Dy flips; Ky0 flips through F'z0, so By = Ky/(Cy*Dy + eps) is invariant; SVy flips with Fz, so Fy0 flips; Dx, Kx and SVx are invariant.
- Under LATERAL: PKY1, PHY and PEY3/PEY4 flip, so the converted Fy0_B(alpha) = Fy0(-alpha); RHX1 flips but cos is even; RBY3 flips, so the converted Byk_B(alpha) = Byk(-alpha); SVy and RVY4 are unchanged. Fx and Fy are LATERAL-even and SA is LATERAL-odd.
- The parity is exact under VERTICAL because eps is sign-matched (conventions.md section 7): By = (-Ky)/(-(Cy*Dy) - eps) is bit-exactly the invariant By, so no 2*eps/|CD| parity break occurs. The parity tests keep the existing relative tolerance 1e-12 for the six non-peak channels and use 1e-10 for the peak slips. NaN equals NaN whatever its sign (LATERAL conversion flips the sign bit of a NaN peak).
- The ellipse ratios use magnitudes, so they are invariant (section 2.3), and the 0/0 limits use |P'| and squares, so they are invariant under both transforms (Fy' = Fy0'*r_lim flips with Fy0').
- Exclusions: sets with Ky = 0 (add_epsilon(0) does not change sign, coordinate-systems.md section 5), exact ellipse kinks (the right derivative of the mirrored model is the left derivative of the original), and SA_AT_PEAK_FY on SA = +-0 rows under plans containing LATERAL. Parity at the 0/0 points of S_ell is compared.

ISO and SAE meaning (project definitions, coordinate-systems.md section 1; conventions.md sections 1-4): in ISO (Fz > 0, dFy/dSA > 0) DY > 0 and DFY_DSA > 0; in SAE both are negative; DX > 0 in both. The positive-side peak is at SA = +|u*|/|By| - SHy with Fy = sgn(By)*Dy + SVy there. No primary source on the Fy-versus-SA sign was reachable (ISO 8855 and SAE J670 are paywalled); this is a project rule, not an external citation.

## 7. Edge cases

| Case | Rule |
|---|---|
| Secant at raw slip +-0 | the secant is the row's own tangent channel (DFY_DSA for FY_OVER_SA, DFX_DSR for FX_OVER_SR), which is 0 where the swept slip was clamped. Elsewhere the secant is the evaluated (clamped, contact-masked) force divided by the raw slip. The limit of F/s as s tends to 0 exists only if F(0) = 0 (SH, SV or SVyk non-zero make the secant diverge), so this is a definition, not a limit |
| No contact (Fz = 0, Fz on the side opposite FNOMIN, or an Fz clamp that ends at 0) | derivatives 0, DY = DX = 0, both secants exactly +0 (including at zero slip), peaks NaN |
| Swept slip clamped to ALPMIN/ALPMAX or KPUMIN/KPUMAX | slope 0 (chain rule through the clamp; the output is constant there); the interior derivative at equality with the limit; the secant is the clamped force divided by the raw slip |
| Genuine ellipse kink (R > 0, own shifted slip +-0) | right derivative (the sgn+ branch) |
| Ellipse 0/0 point | value 1 (the force is the pure force); force derivative F0'*r_lim (section 2.5) |
| E jump at the shifted origin and min(E, 1) | derivative 0 (piecewise constant); the slope is continuous |
| Uncombined mode | the characteristics of the pure Fx0 and Fy0 curves |
| Vx | Fx and Fy do not depend on Vx in MF 5.x, but the Vx domain is still validated as for the outputs |
| Overflow | +-inf is a numerical failure, including a secant at a subnormal slip with a non-zero force |
| NaN | a documented value only in SA_AT_PEAK_FY and SR_AT_PEAK_FX |

Scope note: the coordinate-system identification (coordinate-systems.md section 7) still obtains its indicator derivatives by central differences. It is untouched by the characteristics and could later use DFY_DSA.

## 8. Sources

- TNO: Schmeitz 2013, MF-Tyre/MF-Swift presentation, slides 7 and 8. https://publications.tno.nl/publication/34606988/ORSVMS/schmeitz-2013-mftyre-presentatie.pdf
- Penn State ME481, Notes_08_06, slides 5 and 6 (cite Pacejka 2006, Tire and Vehicle Dynamics, 2nd ed.). https://www.me.psu.edu/sommer/me481/notes_08_06.pdf
- Auburn MECH 4420, HW6, p. 1. https://www.eng.auburn.edu/~dmbevly/mech4420/hw6.pdf
- Siemens MF-Tyre/MF-Swift reduced-input table, as reproduced on the Altair help page. https://help.altair.com/hwdesktop/hwx/topics/motionview/mf-swift_mf-tyre_reduced_tire_input_props_r.htm
- MFeval, MathWorks File Exchange 63618, release notes. https://uk.mathworks.com/matlabcentral/fileexchange/63618-mfeval
- Repository notes: pac2002.md sections 3, 4 and 5.4; mf52.md sections 3 and 4; pac2002-vs-mf52.md; conventions.md sections 7 and 8; coordinate-systems.md sections 1, 3, 4, 5 and 7.

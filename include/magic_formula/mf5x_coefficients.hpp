#pragma once

#include <expected>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>

namespace mf {
  /// The MF 5.x-family coefficient set shared by PAC2002 and MF 5.2 (docs/research/mf52.md). Exactly 117 doubles and
  /// no padding. Member names equal the .tir keys. Defaults: scaling factors L* = 1, every P/Q/R/S coefficient = 0,
  /// LONGVL = 1 m/s as an unused placeholder. The camber factors LGAX, LGAY and LGAZ are PAC2002 keys; MF 5.2 forces
  /// them to 1.
  struct mf5x_coefficients {
    // NOLINTBEGIN(readability-identifier-naming): MF coefficient names match the .tir keys one-to-one

    /// Nominal vertical load Fz0 [N] (> 0).
    double FNOMIN = 0.0;
    /// Unloaded tire radius R0 [m] (> 0).
    double UNLOADED_RADIUS = 0.0;
    /// Reference speed of the rolling-resistance speed terms [m/s] (> 0; placeholder when QSY3 = QSY4 = 0).
    double LONGVL = 1.0;

    /// Scale factor of the nominal load F'z0 = FNOMIN*LFZO (> 0).
    double LFZO = 1.0;
    /// Scale factor of the longitudinal shape factor Cx.
    double LCX = 1.0;
    /// Scale factor of the longitudinal peak friction mu_x.
    double LMUX = 1.0;
    /// Scale factor of the longitudinal curvature factor Ex.
    double LEX = 1.0;
    /// Scale factor of the longitudinal slip stiffness Kx.
    double LKX = 1.0;
    /// Scale factor of the longitudinal horizontal shift SHx.
    double LHX = 1.0;
    /// Scale factor of the longitudinal vertical shift SVx.
    double LVX = 1.0;
    /// Scale factor of the camber influence on Fx (PAC2002 only).
    double LGAX = 1.0;
    /// Scale factor of the lateral shape factor Cy.
    double LCY = 1.0;
    /// Scale factor of the lateral peak friction mu_y (non-zero).
    double LMUY = 1.0;
    /// Scale factor of the lateral curvature factor Ey.
    double LEY = 1.0;
    /// Scale factor of the cornering stiffness Ky.
    double LKY = 1.0;
    /// Scale factor of the lateral horizontal shift SHy.
    double LHY = 1.0;
    /// Scale factor of the lateral vertical shift SVy.
    double LVY = 1.0;
    /// Scale factor of the camber influence on Fy (PAC2002 only).
    double LGAY = 1.0;
    /// Scale factor of the peak pneumatic trail Dt.
    double LTR = 1.0;
    /// Scale factor of the residual moment Dr.
    double LRES = 1.0;
    /// Scale factor of the camber influence on Mz (PAC2002 only).
    double LGAZ = 1.0;
    /// Scale factor of the slip-angle influence on Fx (combined slip).
    double LXAL = 1.0;
    /// Scale factor of the slip-ratio influence on Fy (combined slip).
    double LYKA = 1.0;
    /// Scale factor of the kappa-induced side force SVyk.
    double LVYKA = 1.0;
    /// Scale factor of the moment arm s of Fx.
    double LS = 1.0;
    /// Scale factor of the overturning moment Mx.
    double LMX = 1.0;
    /// Scale factor of the vertical shift of Mx.
    double LVMX = 1.0;
    /// Scale factor of the rolling-resistance moment My.
    double LMY = 1.0;

    /// Longitudinal shape factor Cx [-].
    double PCX1 = 0.0;
    /// Longitudinal peak friction coefficient at the nominal load [-].
    double PDX1 = 0.0;
    /// Variation of the longitudinal peak friction with load [-].
    double PDX2 = 0.0;
    /// Variation of the longitudinal peak friction with camber squared [1/rad^2].
    double PDX3 = 0.0;
    /// Longitudinal curvature factor at the nominal load [-].
    double PEX1 = 0.0;
    /// Variation of the longitudinal curvature factor with load [-].
    double PEX2 = 0.0;
    /// Variation of the longitudinal curvature factor with load squared [-].
    double PEX3 = 0.0;
    /// Longitudinal curvature factor with the sign of the slip [-].
    double PEX4 = 0.0;
    /// Longitudinal slip stiffness at the nominal load, divided by Fz [-].
    double PKX1 = 0.0;
    /// Variation of the longitudinal slip stiffness with load [-].
    double PKX2 = 0.0;
    /// Exponential variation of the longitudinal slip stiffness with load [-].
    double PKX3 = 0.0;
    /// Horizontal shift of Fx at the nominal load [-].
    double PHX1 = 0.0;
    /// Variation of the horizontal shift of Fx with load [-].
    double PHX2 = 0.0;
    /// Vertical shift of Fx at the nominal load, divided by Fz [-].
    double PVX1 = 0.0;
    /// Variation of the vertical shift of Fx with load [-].
    double PVX2 = 0.0;
    /// Slope factor of the combined-slip Fx weighting Gxa [-].
    double RBX1 = 0.0;
    /// Variation of the Gxa slope factor with the slip ratio [-].
    double RBX2 = 0.0;
    /// Shape factor of the combined-slip Fx weighting Gxa [-].
    double RCX1 = 0.0;
    /// Curvature factor of Gxa at the nominal load [-].
    double REX1 = 0.0;
    /// Variation of the Gxa curvature factor with load [-].
    double REX2 = 0.0;
    /// Horizontal shift SHxa of Gxa [-].
    double RHX1 = 0.0;

    /// Lateral shape factor Cy [-].
    double PCY1 = 0.0;
    /// Lateral peak friction coefficient at the nominal load [-].
    double PDY1 = 0.0;
    /// Variation of the lateral peak friction with load [-].
    double PDY2 = 0.0;
    /// Variation of the lateral peak friction with camber squared [1/rad^2].
    double PDY3 = 0.0;
    /// Lateral curvature factor at the nominal load [-].
    double PEY1 = 0.0;
    /// Variation of the lateral curvature factor with load [-].
    double PEY2 = 0.0;
    /// Lateral curvature factor with the sign of the slip angle, constant part [-].
    double PEY3 = 0.0;
    /// Variation of the lateral curvature factor with camber [1/rad].
    double PEY4 = 0.0;
    /// Maximum cornering stiffness divided by the nominal load [1/rad].
    double PKY1 = 0.0;
    /// Load at the maximum cornering stiffness, divided by the nominal load (non-zero) [-].
    double PKY2 = 0.0;
    /// Variation of the cornering stiffness with |camber| [1/rad].
    double PKY3 = 0.0;
    /// Horizontal shift of Fy at the nominal load [-].
    double PHY1 = 0.0;
    /// Variation of the horizontal shift of Fy with load [-].
    double PHY2 = 0.0;
    /// Variation of the horizontal shift of Fy with camber [-].
    double PHY3 = 0.0;
    /// Vertical shift of Fy at the nominal load, divided by Fz [-].
    double PVY1 = 0.0;
    /// Variation of the vertical shift of Fy with load [-].
    double PVY2 = 0.0;
    /// Variation of the vertical shift of Fy with camber [1/rad].
    double PVY3 = 0.0;
    /// Variation of the vertical shift of Fy with camber and load [1/rad].
    double PVY4 = 0.0;
    /// Slope factor of the combined-slip Fy weighting Gyk [-].
    double RBY1 = 0.0;
    /// Variation of the Gyk slope factor with the slip angle [1/rad].
    double RBY2 = 0.0;
    /// Shift of the slip angle in the Gyk slope factor [rad].
    double RBY3 = 0.0;
    /// Shape factor of the combined-slip Fy weighting Gyk [-].
    double RCY1 = 0.0;
    /// Curvature factor of Gyk at the nominal load [-].
    double REY1 = 0.0;
    /// Variation of the Gyk curvature factor with load [-].
    double REY2 = 0.0;
    /// Horizontal shift SHyk of Gyk at the nominal load [-].
    double RHY1 = 0.0;
    /// Variation of the Gyk shift with load [-].
    double RHY2 = 0.0;
    /// Kappa-induced side force DVyk/(mu_y*Fz) at the nominal load [-].
    double RVY1 = 0.0;
    /// Variation of DVyk with load [-].
    double RVY2 = 0.0;
    /// Variation of DVyk with camber [1/rad].
    double RVY3 = 0.0;
    /// Variation of DVyk with the slip angle [1/rad].
    double RVY4 = 0.0;
    /// Variation of SVyk with the slip ratio, factor of the arctangent [-].
    double RVY5 = 0.0;
    /// Variation of SVyk with the slip ratio, slope inside the arctangent [-].
    double RVY6 = 0.0;

    /// Overturning-moment vertical shift Mx/(R0*Fz) at the nominal load [-].
    double QSX1 = 0.0;
    /// Variation of the overturning moment with camber [1/rad].
    double QSX2 = 0.0;
    /// Variation of the overturning moment with the lateral force, divided by FNOMIN [-].
    double QSX3 = 0.0;

    /// Rolling-resistance moment coefficient My/(R0*Fz) [-].
    double QSY1 = 0.0;
    /// Rolling-resistance dependence on Fx, divided by FNOMIN [-].
    double QSY2 = 0.0;
    /// Rolling-resistance dependence on |Vx/LONGVL| [-].
    double QSY3 = 0.0;
    /// Rolling-resistance dependence on (Vx/LONGVL)^4 [-].
    double QSY4 = 0.0;

    /// Pneumatic trail slope factor Bt at the nominal load [1/rad].
    double QBZ1 = 0.0;
    /// Variation of Bt with load [1/rad].
    double QBZ2 = 0.0;
    /// Variation of Bt with load squared [1/rad].
    double QBZ3 = 0.0;
    /// Variation of Bt with camber [1/rad].
    double QBZ4 = 0.0;
    /// Variation of Bt with |camber| [1/rad].
    double QBZ5 = 0.0;
    /// Residual-moment slope factor Br, constant part [1/rad].
    double QBZ9 = 0.0;
    /// Residual-moment slope factor Br, part proportional to By*Cy [-].
    double QBZ10 = 0.0;
    /// Pneumatic trail shape factor Ct [-].
    double QCZ1 = 0.0;
    /// Peak pneumatic trail Dt at the nominal load, divided by R0 [-].
    double QDZ1 = 0.0;
    /// Variation of Dt with load [-].
    double QDZ2 = 0.0;
    /// Variation of Dt with camber [1/rad].
    double QDZ3 = 0.0;
    /// Variation of Dt with camber squared [1/rad^2].
    double QDZ4 = 0.0;
    /// Peak residual moment Dr/(Fz*R0) at the nominal load [-].
    double QDZ6 = 0.0;
    /// Variation of Dr with load [-].
    double QDZ7 = 0.0;
    /// Variation of Dr with camber [1/rad].
    double QDZ8 = 0.0;
    /// Variation of Dr with camber and load [1/rad].
    double QDZ9 = 0.0;
    /// Trail curvature factor Et at the nominal load [-].
    double QEZ1 = 0.0;
    /// Variation of Et with load [-].
    double QEZ2 = 0.0;
    /// Variation of Et with load squared [-].
    double QEZ3 = 0.0;
    /// Et dependence on the sign of the aligning slip, constant part [-].
    double QEZ4 = 0.0;
    /// Et dependence on the sign of the aligning slip, camber part [1/rad].
    double QEZ5 = 0.0;
    /// Horizontal shift SHt of the trail at the nominal load [-].
    double QHZ1 = 0.0;
    /// Variation of SHt with load [-].
    double QHZ2 = 0.0;
    /// Variation of SHt with camber [1/rad].
    double QHZ3 = 0.0;
    /// Variation of SHt with camber and load [1/rad].
    double QHZ4 = 0.0;
    /// Effect of Fx on Mz: nominal value of the moment arm s/R0 [-].
    double SSZ1 = 0.0;
    /// Variation of s/R0 with Fy/FNOMIN [-].
    double SSZ2 = 0.0;
    /// Variation of s/R0 with camber [1/rad].
    double SSZ3 = 0.0;
    /// Variation of s/R0 with load and camber [1/rad].
    double SSZ4 = 0.0;

    // NOLINTEND(readability-identifier-naming)
  };

  /// Everything an MF 5.x-family model is built from (by `mf::io` from a .tir file, or by code).
  struct mf5x_parameters {
    mf5x_coefficients coefficients;
    /// Validity ranges and VXLOW.
    operating_limits limits;
    /// Echoed [MODEL] values (FITTYP also selects the PAC2002 shift form of My).
    model_metadata metadata;
  };

  /// Checks the coefficients that the equations divide by or take as a base. Six independent checks, in this
  /// order: FNOMIN > 0, UNLOADED_RADIUS > 0, LFZO > 0, PKY2 != 0, LMUY != 0, LONGVL > 0. A failure is an
  /// INVALID_VALUE whose `where.key` is the member name (a NaN fails the "> 0" checks).
  [[nodiscard]] std::expected<void, error> validate_coefficients(const mf5x_coefficients &coefficients);
} // namespace mf

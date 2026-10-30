#pragma once
#include <Eigen/Core>
#include <array>
#include <expected>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf6x_layout.hpp>
#include <magic_formula/model_layout.hpp>
#include <string_view>
namespace mf {
  /// MF 6.1 coefficients; scaling factors default to one, unused effects to zero.
  struct mf61_coefficients {
    // NOLINTBEGIN(readability-identifier-naming): MF coefficient names match .tir keys
    /// Nominal speed.
    double LONGVL = 1.0;
    /// Free tyre radius - NEVER MODIFY THIS PARAMETER.
    double UNLOADED_RADIUS = 0.0;
    /// Tyre vertical stiffness.
    double VERTICAL_STIFFNESS = 0.0;
    /// Tyre vertical damping.
    double VERTICAL_DAMPING = 0.0;
    /// Low load stiffness effective rolling radius.
    double BREFF = 0.0;
    /// Peak value of effective rolling radius.
    double DREFF = 0.0;
    /// High load stiffness effective rolling radius.
    double FREFF = 0.0;
    /// Nominal wheel load.
    double FNOMIN = 0.0;
    /// Nominal tyre inflation pressure - NEVER MODIFY THIS PARAMETER.
    double NOMPRES = 0.0;
    /// Scale factor of nominal (rated) load.
    double LFZ0 = 1.0;
    /// Camber w.r.t. spin reduction factor parameter in camber stiffness.
    double PECP1 = 0.0;
    /// Camber w.r.t. spin reduction factor varying with load parameter in camber stiffness.
    double PECP2 = 0.0;
    /// Scale factor of Fx shape factor.
    double LCX = 1.0;
    /// Scale factor of Fx peak friction coefficient.
    double LMUX = 1.0;
    /// Scale factor of Fx curvature factor.
    double LEX = 1.0;
    /// Scale factor of Fx slip stiffness.
    double LKX = 1.0;
    /// Scale factor of Fx horizontal shift.
    double LHX = 1.0;
    /// Scale factor of Fx vertical shift.
    double LVX = 1.0;
    /// Scale factor of Fy shape factor.
    double LCY = 1.0;
    /// Scale factor of Fy peak friction coefficient.
    double LMUY = 1.0;
    /// Scale factor of Fy curvature factor.
    double LEY = 1.0;
    /// Scale factor of Fy cornering stiffness.
    double LKY = 1.0;
    /// Scale factor of Fy horizontal shift.
    double LHY = 1.0;
    /// Scale factor of Fy vertical shift.
    double LVY = 1.0;
    /// Scale factor of peak of pneumatic trail.
    double LTR = 1.0;
    /// Scale factor for offset of residual torque.
    double LRES = 1.0;
    /// Scale factor of alpha influence on Fx.
    double LXAL = 1.0;
    /// Scale factor of kappa influence on Fy.
    double LYKA = 1.0;
    /// Scale factor of kappa induced Fy.
    double LVYKA = 1.0;
    /// Scale factor of moment arm of Fx.
    double LS = 1.0;
    /// Scale factor of camber force stiffness.
    double LKYC = 1.0;
    /// Scale factor of camber torque stiffness.
    double LKZC = 1.0;
    /// LKYY coefficient [-].
    double LKYY = 1.0;
    /// Scale factor of overturning couple.
    double LMX = 1.0;
    /// Scale factor of Mx vertical shift.
    double LVMX = 1.0;
    /// Scale factor of rolling resistance torque.
    double LMY = 1.0;
    /// LMUV coefficient [-].
    double LMUV = 0.0;
    /// Scale factor of Parking Moment.
    double LMP = 1.0;
    /// Shape factor Cfx for longitudinal force.
    double PCX1 = 0.0;
    /// Longitudinal friction Mux at Fznom.
    double PDX1 = 0.0;
    /// Variation of friction Mux with load.
    double PDX2 = 0.0;
    /// Variation of friction Mux with camber squared.
    double PDX3 = 0.0;
    /// Longitudinal curvature Efx at Fznom.
    double PEX1 = 0.0;
    /// Variation of curvature Efx with load.
    double PEX2 = 0.0;
    /// Variation of curvature Efx with load squared.
    double PEX3 = 0.0;
    /// Factor in curvature Efx while driving.
    double PEX4 = 0.0;
    /// Longitudinal slip stiffness Kfx/Fz at Fznom.
    double PKX1 = 0.0;
    /// Variation of slip stiffness Kfx/Fz with load.
    double PKX2 = 0.0;
    /// Exponent in slip stiffness Kfx/Fz with load.
    double PKX3 = 0.0;
    /// Horizontal shift Shx at Fznom.
    double PHX1 = 0.0;
    /// Variation of shift Shx with load.
    double PHX2 = 0.0;
    /// Vertical shift Svx/Fz at Fznom.
    double PVX1 = 0.0;
    /// Variation of shift Svx/Fz with load.
    double PVX2 = 0.0;
    /// Slope factor for combined slip Fx reduction.
    double RBX1 = 0.0;
    /// Variation of slope Fx reduction with kappa.
    double RBX2 = 0.0;
    /// Influence of camber on stiffness for Fx combined.
    double RBX3 = 0.0;
    /// Shape factor for combined slip Fx reduction.
    double RCX1 = 0.0;
    /// Curvature factor of combined Fx.
    double REX1 = 0.0;
    /// Curvature factor of combined Fx with load.
    double REX2 = 0.0;
    /// Shift factor for combined slip Fx reduction.
    double RHX1 = 0.0;
    /// linear influence of inflation pressure on longitudinal slip stiffness.
    double PPX1 = 0.0;
    /// quadratic influence of inflation pressure on longitudinal slip stiffness.
    double PPX2 = 0.0;
    /// linear influence of inflation pressure on peak longitudinal friction.
    double PPX3 = 0.0;
    /// quadratic influence of inflation pressure on peak longitudinal friction.
    double PPX4 = 0.0;
    /// Shape factor Cfy for lateral forces.
    double PCY1 = 0.0;
    /// Lateral friction Muy.
    double PDY1 = 0.0;
    /// Variation of friction Muy with load.
    double PDY2 = 0.0;
    /// Variation of friction Muy with squared camber.
    double PDY3 = 0.0;
    /// Lateral curvature Efy at Fznom.
    double PEY1 = 0.0;
    /// Variation of curvature Efy with load.
    double PEY2 = 0.0;
    /// Zero order camber dependency of curvature Efy.
    double PEY3 = 0.0;
    /// Variation of curvature Efy with camber.
    double PEY4 = 0.0;
    /// Variation of curvature Efy with camber squared.
    double PEY5 = 0.0;
    /// Maximum value of stiffness Kfy/Fznom.
    double PKY1 = 0.0;
    /// Load at which Kfy reaches maximum value.
    double PKY2 = 0.0;
    /// Variation of Kfy/Fznom with camber.
    double PKY3 = 0.0;
    /// Curvature of stiffness Kfy.
    double PKY4 = 2.0;
    /// Peak stiffness variation with camber squared.
    double PKY5 = 0.0;
    /// Fy camber stiffness factor.
    double PKY6 = 0.0;
    /// Vertical load dependency of camber stiffness.
    double PKY7 = 0.0;
    /// Horizontal shift Shy at Fznom.
    double PHY1 = 0.0;
    /// Variation of shift Shy with load.
    double PHY2 = 0.0;
    /// Vertical shift in Svy/Fz at Fznom.
    double PVY1 = 0.0;
    /// Variation of shift Svy/Fz with load.
    double PVY2 = 0.0;
    /// Variation of shift Svy/Fz with camber.
    double PVY3 = 0.0;
    /// Variation of shift Svy/Fz with camber and load.
    double PVY4 = 0.0;
    /// Slope factor for combined Fy reduction.
    double RBY1 = 0.0;
    /// Variation of slope Fy reduction with alpha.
    double RBY2 = 0.0;
    /// Shift term for alpha in slope Fy reduction.
    double RBY3 = 0.0;
    /// Influence of camber on stiffness of Fy combined.
    double RBY4 = 0.0;
    /// Shape factor for combined Fy reduction.
    double RCY1 = 0.0;
    /// Curvature factor of combined Fy.
    double REY1 = 0.0;
    /// Curvature factor of combined Fy with load.
    double REY2 = 0.0;
    /// Shift factor for combined Fy reduction.
    double RHY1 = 0.0;
    /// Shift factor for combined Fy reduction with load.
    double RHY2 = 0.0;
    /// Kappa induced side force Svyk/Muy*Fz at Fznom.
    double RVY1 = 0.0;
    /// Variation of Svyk/Muy*Fz with load.
    double RVY2 = 0.0;
    /// Variation of Svyk/Muy*Fz with camber.
    double RVY3 = 0.0;
    /// Variation of Svyk/Muy*Fz with alpha.
    double RVY4 = 0.0;
    /// Variation of Svyk/Muy*Fz with kappa.
    double RVY5 = 0.0;
    /// Variation of Svyk/Muy*Fz with atan(kappa).
    double RVY6 = 0.0;
    /// influence of inflation pressure on cornering stiffness.
    double PPY1 = 0.0;
    /// influence of inflation pressure on dependency of nominal tyre load on cornering stiffness.
    double PPY2 = 0.0;
    /// linear influence of inflation pressure on lateral peak friction.
    double PPY3 = 0.0;
    /// quadratic influence of inflation pressure on lateral peak friction.
    double PPY4 = 0.0;
    /// Influence of inflation pressure on camber stiffness.
    double PPY5 = 0.0;
    /// Trail slope factor for trail Bpt at Fznom.
    double QBZ1 = 0.0;
    /// Variation of slope Bpt with load.
    double QBZ2 = 0.0;
    /// Variation of slope Bpt with load squared.
    double QBZ3 = 0.0;
    /// Variation of slope Bpt with camber.
    double QBZ4 = 0.0;
    /// Variation of slope Bpt with absolute camber.
    double QBZ5 = 0.0;
    /// Factor for scaling factors of slope factor Br of Mzr.
    double QBZ9 = 0.0;
    /// Factor for dimensionless cornering stiffness of Br of Mzr.
    double QBZ10 = 0.0;
    /// Shape factor Cpt for pneumatic trail.
    double QCZ1 = 0.0;
    /// Peak trail Dpt = Dpt*(Fz/Fznom*R0).
    double QDZ1 = 0.0;
    /// Variation of peak Dpt" with load.
    double QDZ2 = 0.0;
    /// Variation of peak Dpt" with camber.
    double QDZ3 = 0.0;
    /// Variation of peak Dpt" with camber squared.
    double QDZ4 = 0.0;
    /// Peak residual torque Dmr" = Dmr/(Fz*R0).
    double QDZ6 = 0.0;
    /// Variation of peak factor Dmr" with load.
    double QDZ7 = 0.0;
    /// Variation of peak factor Dmr" with camber.
    double QDZ8 = 0.0;
    /// Variation of peak factor Dmr" with camber and load.
    double QDZ9 = 0.0;
    /// Variation of peak factor Dmr with camber squared.
    double QDZ10 = 0.0;
    /// Variation of Dmr with camber squared and load.
    double QDZ11 = 0.0;
    /// Trail curvature Ept at Fznom.
    double QEZ1 = 0.0;
    /// Variation of curvature Ept with load.
    double QEZ2 = 0.0;
    /// Variation of curvature Ept with load squared.
    double QEZ3 = 0.0;
    /// Variation of curvature Ept with sign of Alpha-t.
    double QEZ4 = 0.0;
    /// Variation of Ept with camber and sign Alpha-t.
    double QEZ5 = 0.0;
    /// Trail horizontal shift Sht at Fznom.
    double QHZ1 = 0.0;
    /// Variation of shift Sht with load.
    double QHZ2 = 0.0;
    /// Variation of shift Sht with camber.
    double QHZ3 = 0.0;
    /// Variation of shift Sht with camber and load.
    double QHZ4 = 0.0;
    /// Nominal value of s/R0: effect of Fx on Mz.
    double SSZ1 = 0.0;
    /// Variation of distance s/R0 with Fy/Fznom.
    double SSZ2 = 0.0;
    /// Variation of distance s/R0 with camber.
    double SSZ3 = 0.0;
    /// Variation of distance s/R0 with load and camber.
    double SSZ4 = 0.0;
    /// effect of inflation pressure on length of pneumatic trail.
    double PPZ1 = 0.0;
    /// Influence of inflation pressure on residual aligning torque.
    double PPZ2 = 0.0;
    /// Vertical shift of overturning moment.
    double QSX1 = 0.0;
    /// Camber induced overturning couple.
    double QSX2 = 0.0;
    /// Fy induced overturning couple.
    double QSX3 = 0.0;
    /// Mixed load lateral force and camber on Mx.
    double QSX4 = 0.0;
    /// Load effect on Mx with lateral force and camber.
    double QSX5 = 0.0;
    /// B-factor of load with Mx.
    double QSX6 = 0.0;
    /// Camber with load on Mx.
    double QSX7 = 0.0;
    /// Lateral force with load on Mx.
    double QSX8 = 0.0;
    /// B-factor of lateral force with load on Mx.
    double QSX9 = 0.0;
    /// Vertical force with camber on Mx.
    double QSX10 = 0.0;
    /// B-factor of vertical force with camber on Mx.
    double QSX11 = 0.0;
    /// Camber squared induced overturning moment.
    double QSX12 = 0.0;
    /// Lateral force induced overturning moment.
    double QSX13 = 0.0;
    /// Lateral force induced overturning moment with camber.
    double QSX14 = 0.0;
    /// Influence of inflation pressure on overturning moment.
    double PPMX1 = 0.0;
    /// Rolling resistance torque coefficient.
    double QSY1 = 0.0;
    /// Rolling resistance torque depending on Fx.
    double QSY2 = 0.0;
    /// Rolling resistance torque depending on speed.
    double QSY3 = 0.0;
    /// Rolling resistance torque depending on speed ^4.
    double QSY4 = 0.0;
    /// Rolling resistance torque depending on camber squared.
    double QSY5 = 0.0;
    /// Rolling resistance torque depending on load and camber squared.
    double QSY6 = 0.0;
    /// Rolling resistance torque coefficient load dependency.
    double QSY7 = 0.0;
    /// Rolling resistance torque coefficient pressure dependency.
    double QSY8 = 0.0;
    /// Ratio of free tyre radius with nominal tyre radius.
    double Q_RE0 = 1.0;
    /// Tyre radius increase with speed.
    double Q_V1 = 0.0;
    /// Vertical stiffness increase with speed.
    double Q_V2 = 0.0;
    /// Q_FZ1 coefficient [-].
    double Q_FZ1 = 1.0;
    /// Quadratic term in load vs. deflection.
    double Q_FZ2 = 0.0;
    /// Longitudinal force influence on vertical stiffness.
    double Q_FCX = 0.0;
    /// Lateral force influence on vertical stiffness.
    double Q_FCY = 0.0;
    /// Pressure effect on vertical stiffness.
    double PFZ1 = 0.0;
    /// Peak Fx reduction due to spin parameter.
    double PDXP1 = 0.0;
    /// Peak Fx reduction due to spin with varying load parameter.
    double PDXP2 = 0.0;
    /// Peak Fx reduction due to spin with kappa parameter.
    double PDXP3 = 0.0;
    /// Cornering stiffness reduction due to spin.
    double PKYP1 = 0.0;
    /// Peak Fy reduction due to spin parameter.
    double PDYP1 = 0.0;
    /// Peak Fy reduction due to spin with varying load parameter.
    double PDYP2 = 0.0;
    /// Peak Fy reduction due to spin with alpha parameter.
    double PDYP3 = 0.0;
    /// Peak Fy reduction due to square root of spin parameter.
    double PDYP4 = 0.0;
    /// Fy-alpha curve lateral shift limitation.
    double PHYP1 = 0.0;
    /// Fy-alpha curve maximum lateral shift parameter.
    double PHYP2 = 0.0;
    /// Fy-alpha curve maximum lateral shift varying with load parameter.
    double PHYP3 = 0.0;
    /// Fy-alpha curve maximum lateral shift parameter.
    double PHYP4 = 0.0;
    /// Pneumatic trail reduction factor due to turn slip parameter.
    double QDTP1 = 0.0;
    /// Turning moment at constant turning and zero forward speed parameter.
    double QCRP1 = 0.0;
    /// Turn slip moment (at alpha = 90deg) parameter for increase with spin.
    double QCRP2 = 0.0;
    /// Residual (spin) torque reduction factor parameter due to side slip.
    double QBRP1 = 0.0;
    /// Turn slip moment peak magnitude parameter.
    double QDRP1 = 1.0;
    // NOLINTEND(readability-identifier-naming)
  };
  /// Coefficients, validity limits and metadata of an MF 6.1 tire.
  struct mf61_parameters {
    mf61_coefficients coefficients;
    Eigen::VectorXd limits = default_limits(mf6x::LIMITS);
    model_metadata metadata;
  };
  /// Validates finite coefficient values and the nonzero/positive equation denominators.
  [[nodiscard]] std::expected<void, error> validate_coefficients(const mf61_coefficients &coefficients);
  namespace mf6x {
    /// A .tir key and its coefficient member.
    struct coefficient_entry {
      std::string_view key;
      double mf61_coefficients::*member;
    };
    inline constexpr std::array<coefficient_entry, 187> COEFFICIENTS{{
        {.key = "LONGVL", .member = &mf61_coefficients::LONGVL},
        {.key = "UNLOADED_RADIUS", .member = &mf61_coefficients::UNLOADED_RADIUS},
        {.key = "VERTICAL_STIFFNESS", .member = &mf61_coefficients::VERTICAL_STIFFNESS},
        {.key = "VERTICAL_DAMPING", .member = &mf61_coefficients::VERTICAL_DAMPING},
        {.key = "BREFF", .member = &mf61_coefficients::BREFF},
        {.key = "DREFF", .member = &mf61_coefficients::DREFF},
        {.key = "FREFF", .member = &mf61_coefficients::FREFF},
        {.key = "FNOMIN", .member = &mf61_coefficients::FNOMIN},
        {.key = "NOMPRES", .member = &mf61_coefficients::NOMPRES},
        {.key = "LFZ0", .member = &mf61_coefficients::LFZ0},
        {.key = "PECP1", .member = &mf61_coefficients::PECP1},
        {.key = "PECP2", .member = &mf61_coefficients::PECP2},
        {.key = "LCX", .member = &mf61_coefficients::LCX},
        {.key = "LMUX", .member = &mf61_coefficients::LMUX},
        {.key = "LEX", .member = &mf61_coefficients::LEX},
        {.key = "LKX", .member = &mf61_coefficients::LKX},
        {.key = "LHX", .member = &mf61_coefficients::LHX},
        {.key = "LVX", .member = &mf61_coefficients::LVX},
        {.key = "LCY", .member = &mf61_coefficients::LCY},
        {.key = "LMUY", .member = &mf61_coefficients::LMUY},
        {.key = "LEY", .member = &mf61_coefficients::LEY},
        {.key = "LKY", .member = &mf61_coefficients::LKY},
        {.key = "LHY", .member = &mf61_coefficients::LHY},
        {.key = "LVY", .member = &mf61_coefficients::LVY},
        {.key = "LTR", .member = &mf61_coefficients::LTR},
        {.key = "LRES", .member = &mf61_coefficients::LRES},
        {.key = "LXAL", .member = &mf61_coefficients::LXAL},
        {.key = "LYKA", .member = &mf61_coefficients::LYKA},
        {.key = "LVYKA", .member = &mf61_coefficients::LVYKA},
        {.key = "LS", .member = &mf61_coefficients::LS},
        {.key = "LKYC", .member = &mf61_coefficients::LKYC},
        {.key = "LKZC", .member = &mf61_coefficients::LKZC},
        {.key = "LKYY", .member = &mf61_coefficients::LKYY},
        {.key = "LMX", .member = &mf61_coefficients::LMX},
        {.key = "LVMX", .member = &mf61_coefficients::LVMX},
        {.key = "LMY", .member = &mf61_coefficients::LMY},
        {.key = "LMUV", .member = &mf61_coefficients::LMUV},
        {.key = "LMP", .member = &mf61_coefficients::LMP},
        {.key = "PCX1", .member = &mf61_coefficients::PCX1},
        {.key = "PDX1", .member = &mf61_coefficients::PDX1},
        {.key = "PDX2", .member = &mf61_coefficients::PDX2},
        {.key = "PDX3", .member = &mf61_coefficients::PDX3},
        {.key = "PEX1", .member = &mf61_coefficients::PEX1},
        {.key = "PEX2", .member = &mf61_coefficients::PEX2},
        {.key = "PEX3", .member = &mf61_coefficients::PEX3},
        {.key = "PEX4", .member = &mf61_coefficients::PEX4},
        {.key = "PKX1", .member = &mf61_coefficients::PKX1},
        {.key = "PKX2", .member = &mf61_coefficients::PKX2},
        {.key = "PKX3", .member = &mf61_coefficients::PKX3},
        {.key = "PHX1", .member = &mf61_coefficients::PHX1},
        {.key = "PHX2", .member = &mf61_coefficients::PHX2},
        {.key = "PVX1", .member = &mf61_coefficients::PVX1},
        {.key = "PVX2", .member = &mf61_coefficients::PVX2},
        {.key = "RBX1", .member = &mf61_coefficients::RBX1},
        {.key = "RBX2", .member = &mf61_coefficients::RBX2},
        {.key = "RBX3", .member = &mf61_coefficients::RBX3},
        {.key = "RCX1", .member = &mf61_coefficients::RCX1},
        {.key = "REX1", .member = &mf61_coefficients::REX1},
        {.key = "REX2", .member = &mf61_coefficients::REX2},
        {.key = "RHX1", .member = &mf61_coefficients::RHX1},
        {.key = "PPX1", .member = &mf61_coefficients::PPX1},
        {.key = "PPX2", .member = &mf61_coefficients::PPX2},
        {.key = "PPX3", .member = &mf61_coefficients::PPX3},
        {.key = "PPX4", .member = &mf61_coefficients::PPX4},
        {.key = "PCY1", .member = &mf61_coefficients::PCY1},
        {.key = "PDY1", .member = &mf61_coefficients::PDY1},
        {.key = "PDY2", .member = &mf61_coefficients::PDY2},
        {.key = "PDY3", .member = &mf61_coefficients::PDY3},
        {.key = "PEY1", .member = &mf61_coefficients::PEY1},
        {.key = "PEY2", .member = &mf61_coefficients::PEY2},
        {.key = "PEY3", .member = &mf61_coefficients::PEY3},
        {.key = "PEY4", .member = &mf61_coefficients::PEY4},
        {.key = "PEY5", .member = &mf61_coefficients::PEY5},
        {.key = "PKY1", .member = &mf61_coefficients::PKY1},
        {.key = "PKY2", .member = &mf61_coefficients::PKY2},
        {.key = "PKY3", .member = &mf61_coefficients::PKY3},
        {.key = "PKY4", .member = &mf61_coefficients::PKY4},
        {.key = "PKY5", .member = &mf61_coefficients::PKY5},
        {.key = "PKY6", .member = &mf61_coefficients::PKY6},
        {.key = "PKY7", .member = &mf61_coefficients::PKY7},
        {.key = "PHY1", .member = &mf61_coefficients::PHY1},
        {.key = "PHY2", .member = &mf61_coefficients::PHY2},
        {.key = "PVY1", .member = &mf61_coefficients::PVY1},
        {.key = "PVY2", .member = &mf61_coefficients::PVY2},
        {.key = "PVY3", .member = &mf61_coefficients::PVY3},
        {.key = "PVY4", .member = &mf61_coefficients::PVY4},
        {.key = "RBY1", .member = &mf61_coefficients::RBY1},
        {.key = "RBY2", .member = &mf61_coefficients::RBY2},
        {.key = "RBY3", .member = &mf61_coefficients::RBY3},
        {.key = "RBY4", .member = &mf61_coefficients::RBY4},
        {.key = "RCY1", .member = &mf61_coefficients::RCY1},
        {.key = "REY1", .member = &mf61_coefficients::REY1},
        {.key = "REY2", .member = &mf61_coefficients::REY2},
        {.key = "RHY1", .member = &mf61_coefficients::RHY1},
        {.key = "RHY2", .member = &mf61_coefficients::RHY2},
        {.key = "RVY1", .member = &mf61_coefficients::RVY1},
        {.key = "RVY2", .member = &mf61_coefficients::RVY2},
        {.key = "RVY3", .member = &mf61_coefficients::RVY3},
        {.key = "RVY4", .member = &mf61_coefficients::RVY4},
        {.key = "RVY5", .member = &mf61_coefficients::RVY5},
        {.key = "RVY6", .member = &mf61_coefficients::RVY6},
        {.key = "PPY1", .member = &mf61_coefficients::PPY1},
        {.key = "PPY2", .member = &mf61_coefficients::PPY2},
        {.key = "PPY3", .member = &mf61_coefficients::PPY3},
        {.key = "PPY4", .member = &mf61_coefficients::PPY4},
        {.key = "PPY5", .member = &mf61_coefficients::PPY5},
        {.key = "QBZ1", .member = &mf61_coefficients::QBZ1},
        {.key = "QBZ2", .member = &mf61_coefficients::QBZ2},
        {.key = "QBZ3", .member = &mf61_coefficients::QBZ3},
        {.key = "QBZ4", .member = &mf61_coefficients::QBZ4},
        {.key = "QBZ5", .member = &mf61_coefficients::QBZ5},
        {.key = "QBZ9", .member = &mf61_coefficients::QBZ9},
        {.key = "QBZ10", .member = &mf61_coefficients::QBZ10},
        {.key = "QCZ1", .member = &mf61_coefficients::QCZ1},
        {.key = "QDZ1", .member = &mf61_coefficients::QDZ1},
        {.key = "QDZ2", .member = &mf61_coefficients::QDZ2},
        {.key = "QDZ3", .member = &mf61_coefficients::QDZ3},
        {.key = "QDZ4", .member = &mf61_coefficients::QDZ4},
        {.key = "QDZ6", .member = &mf61_coefficients::QDZ6},
        {.key = "QDZ7", .member = &mf61_coefficients::QDZ7},
        {.key = "QDZ8", .member = &mf61_coefficients::QDZ8},
        {.key = "QDZ9", .member = &mf61_coefficients::QDZ9},
        {.key = "QDZ10", .member = &mf61_coefficients::QDZ10},
        {.key = "QDZ11", .member = &mf61_coefficients::QDZ11},
        {.key = "QEZ1", .member = &mf61_coefficients::QEZ1},
        {.key = "QEZ2", .member = &mf61_coefficients::QEZ2},
        {.key = "QEZ3", .member = &mf61_coefficients::QEZ3},
        {.key = "QEZ4", .member = &mf61_coefficients::QEZ4},
        {.key = "QEZ5", .member = &mf61_coefficients::QEZ5},
        {.key = "QHZ1", .member = &mf61_coefficients::QHZ1},
        {.key = "QHZ2", .member = &mf61_coefficients::QHZ2},
        {.key = "QHZ3", .member = &mf61_coefficients::QHZ3},
        {.key = "QHZ4", .member = &mf61_coefficients::QHZ4},
        {.key = "SSZ1", .member = &mf61_coefficients::SSZ1},
        {.key = "SSZ2", .member = &mf61_coefficients::SSZ2},
        {.key = "SSZ3", .member = &mf61_coefficients::SSZ3},
        {.key = "SSZ4", .member = &mf61_coefficients::SSZ4},
        {.key = "PPZ1", .member = &mf61_coefficients::PPZ1},
        {.key = "PPZ2", .member = &mf61_coefficients::PPZ2},
        {.key = "QSX1", .member = &mf61_coefficients::QSX1},
        {.key = "QSX2", .member = &mf61_coefficients::QSX2},
        {.key = "QSX3", .member = &mf61_coefficients::QSX3},
        {.key = "QSX4", .member = &mf61_coefficients::QSX4},
        {.key = "QSX5", .member = &mf61_coefficients::QSX5},
        {.key = "QSX6", .member = &mf61_coefficients::QSX6},
        {.key = "QSX7", .member = &mf61_coefficients::QSX7},
        {.key = "QSX8", .member = &mf61_coefficients::QSX8},
        {.key = "QSX9", .member = &mf61_coefficients::QSX9},
        {.key = "QSX10", .member = &mf61_coefficients::QSX10},
        {.key = "QSX11", .member = &mf61_coefficients::QSX11},
        {.key = "QSX12", .member = &mf61_coefficients::QSX12},
        {.key = "QSX13", .member = &mf61_coefficients::QSX13},
        {.key = "QSX14", .member = &mf61_coefficients::QSX14},
        {.key = "PPMX1", .member = &mf61_coefficients::PPMX1},
        {.key = "QSY1", .member = &mf61_coefficients::QSY1},
        {.key = "QSY2", .member = &mf61_coefficients::QSY2},
        {.key = "QSY3", .member = &mf61_coefficients::QSY3},
        {.key = "QSY4", .member = &mf61_coefficients::QSY4},
        {.key = "QSY5", .member = &mf61_coefficients::QSY5},
        {.key = "QSY6", .member = &mf61_coefficients::QSY6},
        {.key = "QSY7", .member = &mf61_coefficients::QSY7},
        {.key = "QSY8", .member = &mf61_coefficients::QSY8},
        {.key = "Q_RE0", .member = &mf61_coefficients::Q_RE0},
        {.key = "Q_V1", .member = &mf61_coefficients::Q_V1},
        {.key = "Q_V2", .member = &mf61_coefficients::Q_V2},
        {.key = "Q_FZ1", .member = &mf61_coefficients::Q_FZ1},
        {.key = "Q_FZ2", .member = &mf61_coefficients::Q_FZ2},
        {.key = "Q_FCX", .member = &mf61_coefficients::Q_FCX},
        {.key = "Q_FCY", .member = &mf61_coefficients::Q_FCY},
        {.key = "PFZ1", .member = &mf61_coefficients::PFZ1},
        {.key = "PDXP1", .member = &mf61_coefficients::PDXP1},
        {.key = "PDXP2", .member = &mf61_coefficients::PDXP2},
        {.key = "PDXP3", .member = &mf61_coefficients::PDXP3},
        {.key = "PKYP1", .member = &mf61_coefficients::PKYP1},
        {.key = "PDYP1", .member = &mf61_coefficients::PDYP1},
        {.key = "PDYP2", .member = &mf61_coefficients::PDYP2},
        {.key = "PDYP3", .member = &mf61_coefficients::PDYP3},
        {.key = "PDYP4", .member = &mf61_coefficients::PDYP4},
        {.key = "PHYP1", .member = &mf61_coefficients::PHYP1},
        {.key = "PHYP2", .member = &mf61_coefficients::PHYP2},
        {.key = "PHYP3", .member = &mf61_coefficients::PHYP3},
        {.key = "PHYP4", .member = &mf61_coefficients::PHYP4},
        {.key = "QDTP1", .member = &mf61_coefficients::QDTP1},
        {.key = "QCRP1", .member = &mf61_coefficients::QCRP1},
        {.key = "QCRP2", .member = &mf61_coefficients::QCRP2},
        {.key = "QBRP1", .member = &mf61_coefficients::QBRP1},
        {.key = "QDRP1", .member = &mf61_coefficients::QDRP1},
    }};
  } // namespace mf6x
} // namespace mf

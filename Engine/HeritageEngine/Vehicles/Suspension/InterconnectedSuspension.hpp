#pragma once
#include "SuspensionScalarElements.hpp"

namespace heritage::vehicles::suspension
{

struct AntiRollBarDescription
{
    double rateNmPerRad = 9000.0;
    double cubicRateNmPerRad3 = 0.0;
    double dampingNmPerRadPerSec = 0.0;
    double maximumTorqueNm = 20000.0;
};

inline double evaluateAntiRollTorque(const AntiRollBarDescription& d,
                                     double leftArmAngleRad, double rightArmAngleRad,
                                     double leftArmRateRadps, double rightArmRateRadps)
{
    const double twist = leftArmAngleRad-rightArmAngleRad;
    const double twistRate = leftArmRateRadps-rightArmRateRadps;
    double t = std::max(0.0,d.rateNmPerRad)*twist
             + d.cubicRateNmPerRad3*twist*twist*twist
             + std::max(0.0,d.dampingNmPerRadPerSec)*twistRate;
    return suspClamp(t, -std::abs(d.maximumTorqueNm), std::abs(d.maximumTorqueNm));
}

struct ThirdElementDescription
{
    double heaveSpringRateNPerM = 0.0;
    double heaveDampingNPerMps = 0.0;
    double rollSpringRateNPerM = 0.0;
    double rollDampingNPerMps = 0.0;
};

struct ThirdElementForces
{
    double leftForceN = 0.0;
    double rightForceN = 0.0;
    double heaveForceN = 0.0;
    double rollForceN = 0.0;
};

inline ThirdElementForces evaluateThirdElement(const ThirdElementDescription& d,
                                                double leftTravelM, double rightTravelM,
                                                double leftVelocityMps, double rightVelocityMps)
{
    ThirdElementForces r;
    const double heave = 0.5*(leftTravelM+rightTravelM);
    const double roll = 0.5*(leftTravelM-rightTravelM);
    const double heaveV = 0.5*(leftVelocityMps+rightVelocityMps);
    const double rollV = 0.5*(leftVelocityMps-rightVelocityMps);
    r.heaveForceN = -(d.heaveSpringRateNPerM*heave + d.heaveDampingNPerMps*heaveV);
    r.rollForceN = -(d.rollSpringRateNPerM*roll + d.rollDampingNPerMps*rollV);
    r.leftForceN = 0.5*r.heaveForceN + 0.5*r.rollForceN;
    r.rightForceN = 0.5*r.heaveForceN - 0.5*r.rollForceN;
    return r;
}

struct InerterDescription
{
    double inertanceKg = 0.0; // N/(m/s^2), mechanical inertance
    double maximumForceN = 50000.0;
};

inline double evaluateInerterForce(const InerterDescription& d, double relativeAccelerationMps2)
{
    return suspClamp(-std::max(0.0,d.inertanceKg)*relativeAccelerationMps2,
                     -std::abs(d.maximumForceN), std::abs(d.maximumForceN));
}

// Two-chamber cross-linked hydraulic suspension element. Positive piston travel displaces
// fluid from each wheel cylinder into its node; pressure difference creates cross-coupling.
struct HydraulicInterconnectDescription
{
    double pistonAreaM2 = 0.0015;
    double fluidComplianceM3PerPa = 1.0e-12;
    double crossFlowConductanceM3PerSecPerPa = 2.0e-10;
    double accumulatorPressurePa = 2.0e6;
    double maximumGaugePressurePa = 2.0e7;
};

struct HydraulicInterconnectState
{
    double leftGaugePressurePa = 0.0;
    double rightGaugePressurePa = 0.0;
};

struct HydraulicInterconnectForces
{
    double leftForceN = 0.0;
    double rightForceN = 0.0;
    double crossFlowM3PerSec = 0.0;
};

inline HydraulicInterconnectForces stepHydraulicInterconnect(
    const HydraulicInterconnectDescription& d, HydraulicInterconnectState& s,
    double leftPistonVelocityMps, double rightPistonVelocityMps, double dtSeconds)
{
    HydraulicInterconnectForces r;
    const double area = std::max(1.0e-8,d.pistonAreaM2);
    const double compliance = std::max(1.0e-15,d.fluidComplianceM3PerPa);
    const double conductance = std::max(0.0,d.crossFlowConductanceM3PerSecPerPa);
    const double dt = std::max(0.0,dtSeconds);
    r.crossFlowM3PerSec = conductance*(s.leftGaugePressurePa-s.rightGaugePressurePa);
    const double qL = area*leftPistonVelocityMps-r.crossFlowM3PerSec;
    const double qR = area*rightPistonVelocityMps+r.crossFlowM3PerSec;
    s.leftGaugePressurePa += (qL/compliance)*dt;
    s.rightGaugePressurePa += (qR/compliance)*dt;
    const double maxP = std::max(0.0,d.maximumGaugePressurePa);
    s.leftGaugePressurePa = suspClamp(s.leftGaugePressurePa,-maxP,maxP);
    s.rightGaugePressurePa = suspClamp(s.rightGaugePressurePa,-maxP,maxP);
    r.leftForceN = -(d.accumulatorPressurePa+s.leftGaugePressurePa)*area;
    r.rightForceN = -(d.accumulatorPressurePa+s.rightGaugePressurePa)*area;
    return r;
}

} // namespace heritage::vehicles::suspension

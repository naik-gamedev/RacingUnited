#pragma once
#include "SuspensionScalarElements.hpp"

namespace heritage::vehicles::suspension
{

struct ActiveActuatorDescription
{
    double maximumForceN = 12000.0;
    double maximumForceRateNPerSec = 120000.0;
    double maximumExtensionSpeedMps = 1.5;
    double maximumMechanicalPowerW = 12000.0;
    double efficiencyMotoring = 0.82;
    double efficiencyRegeneration = 0.55;
};

struct ActiveActuatorState
{
    double forceN = 0.0;
    double electricalEnergyJ = 0.0;
    double regeneratedEnergyJ = 0.0;
};

struct ActiveActuatorResult
{
    double forceN = 0.0;
    double mechanicalPowerW = 0.0;
    double electricalPowerW = 0.0;
    bool forceLimited = false;
    bool slewLimited = false;
    bool powerLimited = false;
};

inline ActiveActuatorResult stepActiveActuator(const ActiveActuatorDescription& d,
                                                ActiveActuatorState& s,
                                                double commandedForceN,
                                                double extensionVelocityMps,
                                                double dtSeconds)
{
    ActiveActuatorResult r;
    const double maxF = std::abs(d.maximumForceN);
    double target = suspClamp(commandedForceN,-maxF,maxF);
    r.forceLimited = target != commandedForceN;
    const double dt = std::max(0.0,dtSeconds);
    const double maxDelta = std::max(0.0,d.maximumForceRateNPerSec)*dt;
    const double delta = target-s.forceN;
    if (std::abs(delta)>maxDelta && maxDelta>0.0)
    {
        target = s.forceN+suspSign(delta)*maxDelta;
        r.slewLimited = true;
    }
    const double v = suspClamp(extensionVelocityMps,-std::abs(d.maximumExtensionSpeedMps),
                               std::abs(d.maximumExtensionSpeedMps));
    const double requestedPower = target*v;
    const double maxP = std::abs(d.maximumMechanicalPowerW);
    if (maxP>0.0 && std::abs(requestedPower)>maxP && std::abs(v)>1.0e-8)
    {
        target = suspSign(target)*maxP/std::abs(v);
        r.powerLimited = true;
    }
    s.forceN = target;
    r.forceN = target;
    r.mechanicalPowerW = target*v;
    if (r.mechanicalPowerW>=0.0)
    {
        const double eta = suspClamp(d.efficiencyMotoring,0.05,1.0);
        r.electricalPowerW = r.mechanicalPowerW/eta;
        s.electricalEnergyJ += r.electricalPowerW*dt;
    }
    else
    {
        const double eta = suspClamp(d.efficiencyRegeneration,0.0,1.0);
        r.electricalPowerW = r.mechanicalPowerW*eta;
        s.regeneratedEnergyJ += -r.electricalPowerW*dt;
    }
    return r;
}

struct RideHeightControllerDescription
{
    double positionGainNPerM = 0.0;
    double velocityGainNPerMps = 0.0;
    double skyhookGainNPerMps = 0.0;
    double maximumCommandN = 10000.0;
};

// Optional generic controller; vehicle-specific ECU logic may replace it. Positive travel/velocity
// convention is left to the integration adapter; caller should use the same convention consistently.
inline double evaluateRideHeightCommand(const RideHeightControllerDescription& d,
                                        double targetTravelM, double measuredTravelM,
                                        double unsprungVelocityMps, double sprungVelocityMps)
{
    const double position = d.positionGainNPerM*(targetTravelM-measuredTravelM);
    const double relativeDamping = -d.velocityGainNPerMps*(unsprungVelocityMps-sprungVelocityMps);
    const double skyhook = -d.skyhookGainNPerMps*sprungVelocityMps;
    return suspClamp(position+relativeDamping+skyhook,
                     -std::abs(d.maximumCommandN),std::abs(d.maximumCommandN));
}

struct ActiveAntiRollDescription
{
    double proportionalNmPerRad = 0.0;
    double derivativeNmPerRadPerSec = 0.0;
    double feedForwardNmPerMps2 = 0.0;
    double maximumTorqueNm = 5000.0;
};

inline double evaluateActiveAntiRollTorque(const ActiveAntiRollDescription& d,
                                           double rollErrorRad,double rollRateRadps,
                                           double lateralAccelerationMps2)
{
    const double t = -d.proportionalNmPerRad*rollErrorRad
                     -d.derivativeNmPerRadPerSec*rollRateRadps
                     -d.feedForwardNmPerMps2*lateralAccelerationMps2;
    return suspClamp(t,-std::abs(d.maximumTorqueNm),std::abs(d.maximumTorqueNm));
}

} // namespace heritage::vehicles::suspension

#pragma once
#include "SuspensionScalarElements.hpp"

namespace heritage::vehicles::suspension
{
struct ActiveActuatorDescriptionV2
{
    double maximumForceN=12000.0;
    double maximumForceRateNPerSec=120000.0;
    double maximumExtensionSpeedMps=1.5;
    double maximumMechanicalPowerW=12000.0;
    double stallForceN=15000.0;
    double noLoadSpeedMps=2.0;
    double efficiencyMotoring=0.82;
    double efficiencyRegeneration=0.55;
};
struct ActiveActuatorStateV2
{
    double forceN=0.0;
    double electricalEnergyJ=0.0;
    double regeneratedEnergyJ=0.0;
};
struct ActiveActuatorResultV2
{
    double forceN=0.0;
    double mechanicalPowerW=0.0;
    double electricalPowerW=0.0;
    bool forceLimited=false;
    bool slewLimited=false;
    bool speedLimited=false;
    bool powerLimited=false;
};
inline ActiveActuatorResultV2 stepActiveActuatorV2(const ActiveActuatorDescriptionV2& d,
                                                   ActiveActuatorStateV2& s,double commandForceN,
                                                   double actualExtensionVelocityMps,double dtSeconds)
{
    ActiveActuatorResultV2 r;
    const double dt=std::max(0.0,dtSeconds);
    const double speed=std::abs(actualExtensionVelocityMps);
    const double maxSpeed=std::max(1.0e-6,std::min(std::abs(d.maximumExtensionSpeedMps),std::abs(d.noLoadSpeedMps)));
    double speedForce=std::abs(d.stallForceN)*std::max(0.0,1.0-speed/maxSpeed);
    if(speed>maxSpeed){speedForce=0.0;r.speedLimited=true;}
    const double staticMax=std::abs(d.maximumForceN);
    double attainable=std::min(staticMax,speedForce>0.0?speedForce:0.0);
    if(speed<1.0e-9) attainable=std::min(staticMax,std::abs(d.stallForceN));
    double target=suspClamp(commandForceN,-attainable,attainable);
    r.forceLimited=std::abs(target-commandForceN)>1.0e-9;
    const double maxDelta=std::max(0.0,d.maximumForceRateNPerSec)*dt;
    if(maxDelta>0.0 && std::abs(target-s.forceN)>maxDelta)
    {
        target=s.forceN+suspSign(target-s.forceN)*maxDelta;r.slewLimited=true;
    }
    const double maxP=std::abs(d.maximumMechanicalPowerW);
    const double p=target*actualExtensionVelocityMps;
    if(maxP>0.0 && std::abs(p)>maxP && speed>1.0e-9)
    {
        target=suspSign(target)*maxP/speed;r.powerLimited=true;
    }
    s.forceN=target;r.forceN=target;
    r.mechanicalPowerW=target*actualExtensionVelocityMps;
    if(r.mechanicalPowerW>=0.0)
    {
        r.electricalPowerW=r.mechanicalPowerW/suspClamp(d.efficiencyMotoring,0.05,1.0);
        s.electricalEnergyJ+=r.electricalPowerW*dt;
    }
    else
    {
        r.electricalPowerW=r.mechanicalPowerW*suspClamp(d.efficiencyRegeneration,0.0,1.0);
        s.regeneratedEnergyJ+=-r.electricalPowerW*dt;
    }
    return r;
}
}

namespace heritage::vehicles::suspension
{
struct SemiActiveDamperControllerDescriptionV2
{
    double softValveScale=0.45;
    double firmValveScale=2.5;
    double skyhookWeight=1.0;
    double groundhookWeight=0.25;
};

// Karnopp-style dissipative switching. The semi-active damper never injects energy;
// it only requests a softer/firmer valve state from Damper V3.
inline double evaluateSemiActiveDamperValveV2(const SemiActiveDamperControllerDescriptionV2& d,
                                               double sprungVelocityMps,
                                               double unsprungVelocityMps)
{
    const double relative=sprungVelocityMps-unsprungVelocityMps;
    const bool skyhookFirm=(d.skyhookWeight>0.0) && (sprungVelocityMps*relative>0.0);
    const bool groundhookFirm=(d.groundhookWeight>0.0) && (unsprungVelocityMps*(-relative)>0.0);
    const double demand=(skyhookFirm?std::max(0.0,d.skyhookWeight):0.0)
                       +(groundhookFirm?std::max(0.0,d.groundhookWeight):0.0);
    const double blend=suspClamp(demand,0.0,1.0);
    return std::max(0.05,d.softValveScale+(d.firmValveScale-d.softValveScale)*blend);
}
}

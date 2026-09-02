#pragma once
#include "SuspensionScalarElements.hpp"

namespace heritage::vehicles::suspension
{

enum class SuspensionFailureMode
{
    None,
    Bent,
    Leaking,
    Broken,
    Seized,
    Detached
};

struct SuspensionDamageDescription
{
    double yieldLoadN=18000.0;
    double ultimateLoadN=45000.0;
    double fatigueReferenceLoadN=9000.0;
    double fatigueReferenceCycles=2.0e6;
    double fatigueExponent=5.0;
    double bendRatePerOverloadSecond=0.08;
    double leakRatePerDamageSecond=0.02;
    double wearRatePerMetre=1.0e-8;
};

struct SuspensionDamageState
{
    double fatigueDamage=0.0;   // Miner fraction
    double permanentSet=0.0;    // normalized 0..1 used by geometry adapter
    double leakage=0.0;         // 0..1 damper/air/hydraulic loss
    double wear=0.0;            // 0..1 bushing/joint wear
    double overloadTimeS=0.0;
    SuspensionFailureMode failure=SuspensionFailureMode::None;
};

inline void stepSuspensionDamage(const SuspensionDamageDescription& d,
                                 SuspensionDamageState& s,
                                 double signedLoadN,double relativeTravelM,
                                 double dtSeconds)
{
    const double dt=std::max(0.0,dtSeconds);
    const double load=std::abs(signedLoadN);
    const double yield=std::max(1.0,d.yieldLoadN);
    const double ultimate=std::max(yield+1.0,d.ultimateLoadN);
    if (load>yield)
    {
        s.overloadTimeS+=dt;
        const double over=suspClamp((load-yield)/(ultimate-yield),0.0,2.0);
        s.permanentSet=suspClamp(s.permanentSet+d.bendRatePerOverloadSecond*over*dt,0.0,1.0);
    }
    if (load>=ultimate)
        s.failure=SuspensionFailureMode::Broken;
    else if (s.permanentSet>0.02 && s.failure==SuspensionFailureMode::None)
        s.failure=SuspensionFailureMode::Bent;

    const double ref=std::max(1.0,d.fatigueReferenceLoadN);
    const double cycles=std::max(1.0,d.fatigueReferenceCycles);
    // Damage rate equivalent to one stress reversal per second at the instantaneous amplitude.
    const double fatigueRate=std::pow(load/ref,std::max(1.0,d.fatigueExponent))/cycles;
    s.fatigueDamage=suspClamp(s.fatigueDamage+fatigueRate*dt,0.0,2.0);
    if (s.fatigueDamage>=1.0)
        s.failure=SuspensionFailureMode::Broken;

    s.wear=suspClamp(s.wear+std::abs(relativeTravelM)*std::max(0.0,d.wearRatePerMetre),0.0,1.0);
    const double damageDrive=suspClamp(s.fatigueDamage+0.5*s.permanentSet,0.0,1.0);
    s.leakage=suspClamp(s.leakage+std::max(0.0,d.leakRatePerDamageSecond)*damageDrive*dt,0.0,1.0);
}

inline double damagedStiffnessScale(const SuspensionDamageState& s)
{
    if (s.failure==SuspensionFailureMode::Broken || s.failure==SuspensionFailureMode::Detached) return 0.0;
    return suspClamp(1.0-0.55*s.permanentSet-0.35*s.fatigueDamage,0.05,1.0);
}

inline double damagedDamperScale(const SuspensionDamageState& s)
{
    if (s.failure==SuspensionFailureMode::Broken || s.failure==SuspensionFailureMode::Detached) return 0.0;
    if (s.failure==SuspensionFailureMode::Seized) return 8.0;
    return suspClamp(1.0-s.leakage,0.0,1.0);
}

} // namespace heritage::vehicles::suspension

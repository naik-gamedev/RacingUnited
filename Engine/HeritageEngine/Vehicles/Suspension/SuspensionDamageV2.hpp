#pragma once
#include "SuspensionScalarElements.hpp"
#include <cstdint>

namespace heritage::vehicles::suspension
{

enum SuspensionDamageFlagsV2 : std::uint32_t
{
    DamageNoneV2=0,
    DamageBentV2=1u<<0,
    DamageLeakingV2=1u<<1,
    DamageBrokenV2=1u<<2,
    DamageSeizedV2=1u<<3,
    DamageDetachedV2=1u<<4,
    DamageJointLooseV2=1u<<5
};

struct SuspensionDamageDescriptionV2
{
    double yieldLoadN=18000.0;
    double ultimateLoadN=45000.0;
    double detachLoadN=65000.0;
    double fatigueReferenceLoadN=9000.0;
    double fatigueReferenceCycles=2.0e6;
    double fatigueExponent=5.0;
    double bendRatePerSecond=0.08;
    double sealDamageLoadN=14000.0;
    double sealDamageTemperatureC=150.0;
    double leakGrowthPerSecond=0.05;
    double jointWearPerMetre=1.0e-7;
    double jointLooseWearThreshold=0.65;
    double seizeWearThreshold=0.95;
    double seizeTemperatureC=220.0;
    double brokenDetachDelayS=0.15;
};

struct SuspensionDamageStateV2
{
    double fatigue=0.0;
    double permanentSet=0.0;
    double leakage=0.0;
    double wear=0.0;
    double overloadSeconds=0.0;
    double brokenSeconds=0.0;
    std::uint32_t flags=DamageNoneV2;
};

struct SuspensionDamageResultV2
{
    double stiffnessScale=1.0;
    double dampingScale=1.0;
    double geometrySetScale=0.0;
    double leakageScale=0.0;
    double freePlayScale=1.0;
    bool constraintEnabled=true;
    bool seized=false;
    bool detached=false;
};

inline SuspensionDamageResultV2 stepSuspensionDamageV2(const SuspensionDamageDescriptionV2& d,
                                                        SuspensionDamageStateV2& s,
                                                        double loadN,double relativeTravelM,
                                                        double temperatureC,double dtSeconds)
{
    const double dt=std::max(0.0,dtSeconds);
    const double a=std::abs(loadN);
    const double y=std::max(1.0,d.yieldLoadN);
    const double u=std::max(y+1.0,d.ultimateLoadN);
    if(a>y)
    {
        s.overloadSeconds+=dt;
        const double severity=suspClamp((a-y)/(u-y),0.0,3.0);
        s.permanentSet=suspClamp(s.permanentSet+std::max(0.0,d.bendRatePerSecond)*severity*dt,0.0,1.0);
        if(s.permanentSet>1.0e-4) s.flags|=DamageBentV2;
    }
    const double ref=std::max(1.0,d.fatigueReferenceLoadN);
    const double cycles=std::max(1.0,d.fatigueReferenceCycles);
    s.fatigue=suspClamp(s.fatigue+std::pow(a/ref,std::max(1.0,d.fatigueExponent))*dt/cycles,0.0,4.0);
    if(a>=u || s.fatigue>=1.0) s.flags|=DamageBrokenV2;
    if(a>=std::max(u,d.detachLoadN)) s.flags|=DamageDetachedV2;

    s.wear=suspClamp(s.wear+std::abs(relativeTravelM)*std::max(0.0,d.jointWearPerMetre),0.0,1.0);
    if(s.wear>=d.jointLooseWearThreshold) s.flags|=DamageJointLooseV2;
    if(s.wear>=d.seizeWearThreshold || temperatureC>=d.seizeTemperatureC) s.flags|=DamageSeizedV2;

    const double sealDrive=(a>std::max(1.0,d.sealDamageLoadN)?(a/d.sealDamageLoadN-1.0):0.0)
                         +(temperatureC>d.sealDamageTemperatureC?(temperatureC-d.sealDamageTemperatureC)/50.0:0.0)
                         +0.25*s.fatigue+0.2*s.permanentSet;
    if(sealDrive>0.0)
        s.leakage=suspClamp(s.leakage+std::max(0.0,d.leakGrowthPerSecond)*sealDrive*dt,0.0,1.0);
    if(s.leakage>0.01) s.flags|=DamageLeakingV2;

    if((s.flags&DamageBrokenV2)!=0u)
    {
        s.brokenSeconds+=dt;
        if(s.brokenSeconds>=std::max(0.0,d.brokenDetachDelayS) && (a>0.25*u || s.wear>0.8))
            s.flags|=DamageDetachedV2;
    }

    SuspensionDamageResultV2 r;
    r.detached=(s.flags&DamageDetachedV2)!=0u;
    r.seized=(s.flags&DamageSeizedV2)!=0u && !r.detached;
    const bool broken=(s.flags&DamageBrokenV2)!=0u;
    r.constraintEnabled=!r.detached;
    r.stiffnessScale=r.detached?0.0:(broken?0.03:suspClamp(1.0-0.55*s.permanentSet-0.35*std::min(1.0,s.fatigue),0.05,1.0));
    r.dampingScale=r.detached?0.0:(r.seized?12.0:suspClamp(1.0-s.leakage,0.0,1.0));
    r.geometrySetScale=s.permanentSet;
    r.leakageScale=s.leakage;
    r.freePlayScale=1.0+8.0*s.wear;
    return r;
}

} // namespace heritage::vehicles::suspension

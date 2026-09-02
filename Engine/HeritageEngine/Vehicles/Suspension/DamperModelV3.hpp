#pragma once
#include "SuspensionScalarElements.hpp"
#include <cmath>

namespace heritage::vehicles::suspension
{

struct DamperValveDescriptionV3
{
    double bleedConductanceM3PerSecPerPa=1.0e-12;
    double orificeCoefficientM3PerSecPerSqrtPa=1.0e-8;
    double shimCrackPressurePa=4.0e5;
    double shimFullOpenPressurePa=2.0e6;
    double shimFlowMultiplier=4.0;
    double shimResponseHz=80.0;
};

struct DamperDescriptionV3
{
    double pistonAreaM2=0.00125;
    double rodAreaM2=0.00018;
    double compressionChamberVolumeM3=1.6e-4;
    double reboundChamberVolumeM3=1.9e-4;
    double gasReferenceVolumeM3=8.0e-5;
    double gasReferenceAbsolutePressurePa=1.2e6;
    double gasExponent=1.35;
    double oilBulkModulusPa=1.3e9;
    double oilVaporPressurePa=3000.0;
    double maximumAbsolutePressurePa=4.0e7;
    DamperValveDescriptionV3 compressionValve{};
    DamperValveDescriptionV3 reboundValve{};
    double sealFrictionN=80.0;
    double sealSmoothingVelocityMps=0.003;
    double hydraulicBumpStartCompressionM=0.08;
    double hydraulicBumpRateNPerM=0.0;
    double hydraulicBumpDampingNPerMps=0.0;
    double thermalCapacityJPerK=12000.0;
    double coolingWPerK=30.0;
    double nominalTemperatureC=80.0;
    double fadeStartC=120.0;
    double fadeFullC=190.0;
    double hotViscosityFraction=0.5;
    double aerationBuildRate=2.0;
    double aerationRecoveryRate=1.0;
    double leakConductanceM3PerSecPerPa=0.0;
    double maximumForceN=60000.0;
};

struct DamperStateV3
{
    double compressionPressurePa=1.2e6;
    double reboundPressurePa=1.2e6;
    double gasPressurePa=1.2e6;
    double gasVolumeM3=8.0e-5;
    double compressionShimOpen=0.0;
    double reboundShimOpen=0.0;
    double aeration=0.0;
    double temperatureC=80.0;
    double dissipatedEnergyJ=0.0;
    double leakedOilM3=0.0;
    bool initialized=false;
};

struct DamperResultV3
{
    double forceN=0.0;
    double hydraulicForceN=0.0;
    double gasForceN=0.0;
    double sealForceN=0.0;
    double bumpStopForceN=0.0;
    double compressionPressurePa=0.0;
    double reboundPressurePa=0.0;
    double gasPressurePa=0.0;
    double valveFlowM3ps=0.0;
    double leakageFlowM3ps=0.0;
    double temperatureC=0.0;
    double aeration=0.0;
    double dissipatedPowerW=0.0;
    bool cavitating=false;
    bool forceLimited=false;
};

inline double damperValveFlowV3(const DamperValveDescriptionV3& v,double dp,double shimOpen,double viscosityScale)
{
    const double a=std::abs(dp);
    const double sign=suspSign(dp);
    const double bleed=std::max(0.0,v.bleedConductanceM3PerSecPerPa)*dp;
    const double orifice=std::max(0.0,v.orificeCoefficientM3PerSecPerSqrtPa)*sign*std::sqrt(a);
    const double mult=1.0+std::max(0.0,v.shimFlowMultiplier-1.0)*suspClamp(shimOpen,0.0,1.0);
    return (bleed+orifice*mult)/std::max(0.1,viscosityScale);
}

inline double damperShimTargetV3(const DamperValveDescriptionV3& v,double pressureDeltaPa)
{
    const double a=std::abs(pressureDeltaPa);
    const double p0=std::max(0.0,v.shimCrackPressurePa);
    const double p1=std::max(p0+1.0,v.shimFullOpenPressurePa);
    return suspClamp((a-p0)/(p1-p0),0.0,1.0);
}

inline DamperResultV3 stepDamperV3(const DamperDescriptionV3& d,DamperStateV3& s,
                                   double extensionM,double extensionVelocityMps,
                                   double ambientTemperatureC,double dtSeconds,
                                   double semiActiveValveScale=1.0,double leakageScale=0.0)
{
    if(!s.initialized)
    {
        s.compressionPressurePa=d.gasReferenceAbsolutePressurePa;
        s.reboundPressurePa=d.gasReferenceAbsolutePressurePa;
        s.gasPressurePa=d.gasReferenceAbsolutePressurePa;
        s.gasVolumeM3=std::max(1.0e-7,d.gasReferenceVolumeM3);
        s.temperatureC=d.nominalTemperatureC;
        s.initialized=true;
    }
    DamperResultV3 r;
    const double dt=std::max(0.0,dtSeconds);
    const double ap=std::max(1.0e-8,d.pistonAreaM2);
    const double ar=suspClamp(d.rodAreaM2,0.0,0.95*ap);
    const double aa=ap-ar;
    const double beta=std::max(1.0e6,d.oilBulkModulusPa)*(1.0-0.75*suspClamp(s.aeration,0.0,1.0));
    const double fadeT0=d.fadeStartC, fadeT1=std::max(fadeT0+1.0,d.fadeFullC);
    const double hot=suspClamp((s.temperatureC-fadeT0)/(fadeT1-fadeT0),0.0,1.0);
    const double viscosityScale=1.0+(suspClamp(d.hotViscosityFraction,0.1,1.0)-1.0)*hot;
    const double valveCommand=suspClamp(semiActiveValveScale,0.25,4.0);
    const int substeps=std::max(1,std::min(64,static_cast<int>(std::ceil(dt/0.0001))));
    const double h=substeps>0?dt/static_cast<double>(substeps):0.0;
    double lastFlow=0.0,lastLeak=0.0;

    for(int n=0;n<substeps;++n)
    {
        const double dp=s.compressionPressurePa-s.reboundPressurePa;
        const bool compression=extensionVelocityMps<0.0;
        const DamperValveDescriptionV3& valve=compression?d.compressionValve:d.reboundValve;
        double& shim=compression?s.compressionShimOpen:s.reboundShimOpen;
        const double target=damperShimTargetV3(valve,dp);
        const double alpha=1.0-std::exp(-2.0*3.14159265358979323846*std::max(0.0,valve.shimResponseHz)*h);
        shim += (target-shim)*alpha;
        const double qValve=damperValveFlowV3(valve,dp,shim,viscosityScale*valveCommand);
        const double leakC=std::max(0.0,d.leakConductanceM3PerSecPerPa)*(1.0+100.0*suspClamp(leakageScale,0.0,1.0));
        const double qLeak=leakC*dp;
        const double pistonCompressionVelocity=-extensionVelocityMps;
        const double qCompIn=ap*pistonCompressionVelocity-qValve-qLeak;
        const double qRebIn=-aa*pistonCompressionVelocity+qValve+qLeak;
        const double vc=std::max(1.0e-7,d.compressionChamberVolumeM3+ap*std::max(-0.2,std::min(0.2,extensionM)));
        const double vr=std::max(1.0e-7,d.reboundChamberVolumeM3-aa*std::max(-0.2,std::min(0.2,extensionM)));
        s.compressionPressurePa += beta*qCompIn*h/vc;
        s.reboundPressurePa += beta*qRebIn*h/vr;

        // Rod displacement compresses the gas accumulator and provides monotonic gas preload.
        const double vg0=std::max(1.0e-7,d.gasReferenceVolumeM3);
        s.gasVolumeM3=suspClamp(vg0-ar*extensionM,0.2*vg0,5.0*vg0);
        s.gasPressurePa=d.gasReferenceAbsolutePressurePa*std::pow(vg0/s.gasVolumeM3,suspClamp(d.gasExponent,1.0,1.67));
        // Low-pressure chamber communicates with the gas separator on a fast but finite time scale.
        const double gasRelax=1.0-std::exp(-80.0*h);
        s.reboundPressurePa += (s.gasPressurePa-s.reboundPressurePa)*gasRelax;

        const double vapor=std::max(0.0,d.oilVaporPressurePa);
        bool cav=false;
        if(s.compressionPressurePa<vapor){s.compressionPressurePa=vapor;cav=true;}
        if(s.reboundPressurePa<vapor){s.reboundPressurePa=vapor;cav=true;}
        if(cav) s.aeration=suspClamp(s.aeration+d.aerationBuildRate*h,0.0,1.0);
        else s.aeration=suspClamp(s.aeration-d.aerationRecoveryRate*h,0.0,1.0);
        const double pmax=std::max(vapor+1.0,d.maximumAbsolutePressurePa);
        s.compressionPressurePa=suspClamp(s.compressionPressurePa,vapor,pmax);
        s.reboundPressurePa=suspClamp(s.reboundPressurePa,vapor,pmax);
        s.leakedOilM3+=std::abs(qLeak)*h;
        lastFlow=qValve;lastLeak=qLeak;
    }

    const double pressureForce=s.compressionPressurePa*ap-s.reboundPressurePa*aa;
    const double gasForce=s.gasPressurePa*ar;
    const double seal=-std::max(0.0,d.sealFrictionN)*std::tanh(extensionVelocityMps/std::max(1.0e-6,d.sealSmoothingVelocityMps));
    // Remove static pressure preload from hydraulic channel; gas force remains explicit stored-energy term.
    const double baseline=d.gasReferenceAbsolutePressurePa*(ap-aa);
    r.hydraulicForceN=pressureForce-baseline-gasForce;
    r.gasForceN=gasForce;
    r.sealForceN=seal;
    if(-extensionM>d.hydraulicBumpStartCompressionM)
    {
        const double x=(-extensionM-d.hydraulicBumpStartCompressionM);
        r.bumpStopForceN=std::max(0.0,d.hydraulicBumpRateNPerM)*x
                       +std::max(0.0,d.hydraulicBumpDampingNPerMps)*std::max(0.0,-extensionVelocityMps);
    }
    double f=r.hydraulicForceN+r.gasForceN+r.sealForceN+r.bumpStopForceN;
    const double maxF=std::abs(d.maximumForceN);
    if(maxF>0.0 && std::abs(f)>maxF){f=suspSign(f)*maxF;r.forceLimited=true;}
    r.forceN=f;
    r.compressionPressurePa=s.compressionPressurePa;r.reboundPressurePa=s.reboundPressurePa;r.gasPressurePa=s.gasPressurePa;
    r.valveFlowM3ps=lastFlow;r.leakageFlowM3ps=lastLeak;r.temperatureC=s.temperatureC;r.aeration=s.aeration;
    r.cavitating=s.aeration>0.01;
    // Hydraulic/seal dissipation only; gas/bump elastic storage is not classified as dissipation.
    r.dissipatedPowerW=std::max(0.0,-(r.hydraulicForceN+r.sealForceN)*extensionVelocityMps);
    if(dt>0.0)
    {
        s.dissipatedEnergyJ+=r.dissipatedPowerW*dt;
        const double cooling=std::max(0.0,d.coolingWPerK)*(s.temperatureC-ambientTemperatureC);
        s.temperatureC+=(r.dissipatedPowerW-cooling)*dt/std::max(1.0,d.thermalCapacityJPerK);
        s.temperatureC=suspClamp(s.temperatureC,-50.0,300.0);
    }
    return r;
}

} // namespace heritage::vehicles::suspension

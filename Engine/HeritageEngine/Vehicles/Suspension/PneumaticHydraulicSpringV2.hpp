#pragma once
#include "SuspensionScalarElements.hpp"
#include <cmath>

namespace heritage::vehicles::suspension
{
constexpr double kSuspAirR = 287.05;

struct AirSpringDescriptionV2
{
    double referenceVolumeM3=0.0030;
    double referenceAbsolutePressurePa=6.0e5;
    double referenceTemperatureK=293.15;
    double pistonAreaM2=0.0030;
    double reservoirVolumeM3=0.0;
    double reservoirReferencePressurePa=6.0e5;
    double minimumVolumeM3=1.0e-5;
    double maximumPressurePa=3.0e7;
    double chamberReservoirConductanceKgPerSecPerPa=0.0;
    double leakConductanceKgPerSecPerPa=0.0;
    double compressorMassFlowKgPerSec=0.0;
    double ventMassFlowKgPerSec=0.0;
    double thermalCapacityJPerK=600.0;
    double heatTransferWPerK=8.0;
    double heightControlDeadbandM=0.003;
    double heightControlGain=1.0;
};

struct AirSpringStateV2
{
    double chamberMassKg=0.0;
    double chamberTemperatureK=293.15;
    double reservoirMassKg=0.0;
    double reservoirTemperatureK=293.15;
    double cumulativeCompressorMassKg=0.0;
    double cumulativeLeakMassKg=0.0;
    bool initialized=false;
};

struct AirSpringResultV2
{
    double forceN=0.0;
    double chamberPressurePa=0.0;
    double reservoirPressurePa=0.0;
    double chamberMassKg=0.0;
    double massFlowToReservoirKgps=0.0;
    double leakFlowKgps=0.0;
    double compressorFlowKgps=0.0;
};

inline void initializeAirSpringV2(const AirSpringDescriptionV2& d,AirSpringStateV2& s)
{
    const double t=std::max(150.0,d.referenceTemperatureK);
    const double v=std::max(d.minimumVolumeM3,d.referenceVolumeM3);
    s.chamberMassKg=std::max(1.0e-8,d.referenceAbsolutePressurePa*v/(kSuspAirR*t));
    if(d.reservoirVolumeM3>0.0)
        s.reservoirMassKg=std::max(1.0e-8,d.reservoirReferencePressurePa*d.reservoirVolumeM3/(kSuspAirR*t));
    else s.reservoirMassKg=0.0;
    s.chamberTemperatureK=t; s.reservoirTemperatureK=t; s.initialized=true;
}

inline AirSpringResultV2 stepAirSpringV2(const AirSpringDescriptionV2& d,AirSpringStateV2& s,
                                         double compressionM,double compressionVelocityMps,
                                         double ambientPressurePa,double ambientTemperatureK,
                                         double rideHeightErrorM,double dtSeconds,double leakageScale=0.0)
{
    if(!s.initialized) initializeAirSpringV2(d,s);
    AirSpringResultV2 r;
    const double dt=std::max(0.0,dtSeconds);
    const double area=std::max(1.0e-8,d.pistonAreaM2);
    const double volume=std::max(d.minimumVolumeM3,d.referenceVolumeM3-area*compressionM);
    const double t=std::max(150.0,s.chamberTemperatureK);
    double p=std::max(1.0, s.chamberMassKg*kSuspAirR*t/volume);
    double rp=ambientPressurePa;
    if(d.reservoirVolumeM3>0.0 && s.reservoirMassKg>0.0)
        rp=std::max(1.0,s.reservoirMassKg*kSuspAirR*std::max(150.0,s.reservoirTemperatureK)/d.reservoirVolumeM3);

    const double qToRes=d.reservoirVolumeM3>0.0
        ? d.chamberReservoirConductanceKgPerSecPerPa*(p-rp) : 0.0;
    const double leakConductance=std::max(0.0,d.leakConductanceKgPerSecPerPa)*(1.0+suspClamp(leakageScale,0.0,1.0)*100.0);
    const double qLeak=leakConductance*std::max(0.0,p-ambientPressurePa);
    double qCompressor=0.0;
    const double dead=std::max(0.0,d.heightControlDeadbandM);
    if(rideHeightErrorM>dead)
        qCompressor=std::max(0.0,d.compressorMassFlowKgPerSec)*suspClamp((rideHeightErrorM-dead)*d.heightControlGain,0.0,1.0);
    else if(rideHeightErrorM<-dead)
        qCompressor=-std::max(0.0,d.ventMassFlowKgPerSec)*suspClamp((-rideHeightErrorM-dead)*d.heightControlGain,0.0,1.0);

    if(dt>0.0)
    {
        const double oldMass=s.chamberMassKg;
        s.chamberMassKg=std::max(1.0e-9,s.chamberMassKg+(-qToRes-qLeak+qCompressor)*dt);
        if(d.reservoirVolumeM3>0.0)
            s.reservoirMassKg=std::max(1.0e-9,s.reservoirMassKg+qToRes*dt);
        s.cumulativeLeakMassKg+=std::max(0.0,qLeak)*dt;
        s.cumulativeCompressorMassKg+=std::max(0.0,qCompressor)*dt;

        // First-law lump: -P dV work heats compression; ambient heat transfer cools it.
        const double dVdt=-area*compressionVelocityMps;
        const double workRate=-p*dVdt;
        const double heatRate=std::max(0.0,d.heatTransferWPerK)*(ambientTemperatureK-s.chamberTemperatureK);
        const double cap=std::max(1.0,d.thermalCapacityJPerK);
        s.chamberTemperatureK += (workRate+heatRate)*dt/cap;
        // Added/removed mass mixes toward ambient/compressor temperature without an expensive gas-network solve.
        if(std::abs(s.chamberMassKg-oldMass)>1.0e-12)
        {
            const double mix=suspClamp(std::abs(s.chamberMassKg-oldMass)/std::max(s.chamberMassKg,1.0e-9),0.0,1.0);
            s.chamberTemperatureK += (ambientTemperatureK-s.chamberTemperatureK)*mix;
        }
        if(d.reservoirVolumeM3>0.0)
        {
            const double hr=std::max(0.0,d.heatTransferWPerK)*0.5;
            s.reservoirTemperatureK += hr*(ambientTemperatureK-s.reservoirTemperatureK)*dt/std::max(1.0,d.thermalCapacityJPerK);
        }
        s.chamberTemperatureK=suspClamp(s.chamberTemperatureK,150.0,800.0);
        s.reservoirTemperatureK=suspClamp(s.reservoirTemperatureK,150.0,800.0);
    }

    p=suspClamp(s.chamberMassKg*kSuspAirR*s.chamberTemperatureK/volume,ambientPressurePa,std::max(ambientPressurePa,d.maximumPressurePa));
    if(d.reservoirVolumeM3>0.0)
        rp=s.reservoirMassKg*kSuspAirR*s.reservoirTemperatureK/std::max(1.0e-8,d.reservoirVolumeM3);
    r.forceN=std::max(0.0,(p-ambientPressurePa)*area);
    r.chamberPressurePa=p; r.reservoirPressurePa=rp; r.chamberMassKg=s.chamberMassKg;
    r.massFlowToReservoirKgps=qToRes; r.leakFlowKgps=qLeak; r.compressorFlowKgps=qCompressor;
    return r;
}

struct HydroPneumaticDescriptionV2
{
    double gasReferenceVolumeM3=0.0010;
    double gasPrechargePressurePa=5.0e6;
    double gasReferenceTemperatureK=293.15;
    double pistonAreaM2=0.0015;
    double hydraulicLineComplianceM3PerPa=2.0e-12;
    double hydraulicRestrictorM3PerSecPerPa=2.0e-11;
    double reservoirGaugePressurePa=0.0;
    double leakM3PerSecPerPa=0.0;
    double heatTransferWPerK=5.0;
    double gasThermalCapacityJPerK=400.0;
    double minimumGasVolumeM3=1.0e-5;
    double maximumPressurePa=3.0e7;
};

struct HydroPneumaticStateV2
{
    double displacedFluidM3=0.0;
    double lineGaugePressurePa=0.0;
    double gasTemperatureK=293.15;
    double cumulativeLeakM3=0.0;
};

struct HydroPneumaticResultV2
{
    double forceN=0.0;
    double gasPressurePa=0.0;
    double linePressurePa=0.0;
    double restrictorFlowM3ps=0.0;
    double leakFlowM3ps=0.0;
};

inline HydroPneumaticResultV2 stepHydroPneumaticV2(const HydroPneumaticDescriptionV2& d,
                                                    HydroPneumaticStateV2& s,
                                                    double pistonVelocityMps,double ambientPressurePa,
                                                    double ambientTemperatureK,double dtSeconds,double leakageScale=0.0)
{
    HydroPneumaticResultV2 r;
    const double dt=std::max(0.0,dtSeconds);
    const double area=std::max(1.0e-8,d.pistonAreaM2);
    const double v0=std::max(d.minimumGasVolumeM3,d.gasReferenceVolumeM3);
    const double gasVolume=std::max(d.minimumGasVolumeM3,v0-s.displacedFluidM3);
    const double gamma=1.35;
    const double tempRatio=std::max(150.0,s.gasTemperatureK)/std::max(150.0,d.gasReferenceTemperatureK);
    double gasP=d.gasPrechargePressurePa*std::pow(v0/gasVolume,gamma)*tempRatio;
    gasP=suspClamp(gasP,ambientPressurePa,std::max(ambientPressurePa,d.maximumPressurePa));
    const double lineAbsolute=gasP+s.lineGaugePressurePa;
    const double qRestrict=std::max(0.0,d.hydraulicRestrictorM3PerSecPerPa)*(lineAbsolute-gasP);
    const double qLeak=std::max(0.0,d.leakM3PerSecPerPa)*(1.0+100.0*suspClamp(leakageScale,0.0,1.0))*std::max(0.0,lineAbsolute-ambientPressurePa);
    const double qPiston=area*pistonVelocityMps;
    if(dt>0.0)
    {
        const double compliance=std::max(1.0e-15,d.hydraulicLineComplianceM3PerPa);
        s.lineGaugePressurePa += (qPiston-qRestrict-qLeak)*dt/compliance;
        s.lineGaugePressurePa=suspClamp(s.lineGaugePressurePa,-0.5*gasP,d.maximumPressurePa-gasP);
        s.displacedFluidM3 += qRestrict*dt;
        s.displacedFluidM3=suspClamp(s.displacedFluidM3,-0.5*v0,v0-d.minimumGasVolumeM3);
        s.cumulativeLeakM3+=std::max(0.0,qLeak)*dt;
        const double workRate=gasP*qRestrict;
        const double heatRate=std::max(0.0,d.heatTransferWPerK)*(ambientTemperatureK-s.gasTemperatureK);
        s.gasTemperatureK+=(workRate+heatRate)*dt/std::max(1.0,d.gasThermalCapacityJPerK);
        s.gasTemperatureK=suspClamp(s.gasTemperatureK,150.0,800.0);
    }
    const double vg=std::max(d.minimumGasVolumeM3,v0-s.displacedFluidM3);
    gasP=d.gasPrechargePressurePa*std::pow(v0/vg,gamma)*(s.gasTemperatureK/std::max(150.0,d.gasReferenceTemperatureK));
    const double lp=suspClamp(gasP+s.lineGaugePressurePa,ambientPressurePa,std::max(ambientPressurePa,d.maximumPressurePa));
    r.gasPressurePa=gasP; r.linePressurePa=lp; r.forceN=std::max(0.0,(lp-ambientPressurePa)*area);
    r.restrictorFlowM3ps=qRestrict; r.leakFlowM3ps=qLeak;
    return r;
}

} // namespace heritage::vehicles::suspension

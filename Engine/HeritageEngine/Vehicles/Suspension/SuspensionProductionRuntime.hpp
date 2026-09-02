#pragma once
#include "SuspensionScalarElements.hpp"
#include "DamperModelV3.hpp"
#include "PneumaticHydraulicSpringV2.hpp"
#include "SuspensionComplianceDynamic.hpp"
#include "SuspensionDamageV2.hpp"
#include "ActiveSuspensionV2.hpp"
#include "InterconnectedSuspension.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace heritage::vehicles::suspension
{

enum class SpringHardwareKindV2 : std::uint8_t
{
    ProgressiveCoil,
    DualRateCoil,
    LeafPack,
    Air,
    HydroPneumatic,
    TorsionEquivalent
};

struct SuspensionKinematicSampleV2
{
    // Wheel compression coordinate: positive bump/compression, negative droop.
    double wheelCompressionM=0.0;
    double wheelCompressionVelocityMps=0.0;
    double wheelCompressionAccelerationMps2=0.0;
    // Exact geometry-derived element coordinates/Jacobians. Positive elementCompression compresses the spring/damper.
    double springLengthM=0.30;
    double springCompressionM=0.0;
    double springCompressionVelocityMps=0.0;
    double springMotionRatio=1.0; // d springCompression / d wheelCompression
    double damperCompressionM=0.0;
    double damperCompressionVelocityMps=0.0;
    double damperMotionRatio=1.0;
    double actuatorExtensionVelocityMps=0.0;
    double actuatorExtensionMotionRatio=-1.0; // d actuatorExtension / d wheelCompression
    double bumpStopPenetrationM=0.0;
    double bumpStopVelocityMps=0.0;
    double reboundStopPenetrationM=0.0;
    double reboundStopVelocityMps=0.0;
    SuspVec6 hardpointGeneralizedLoad{};
};

struct SuspensionCornerDescriptionV2
{
    SpringHardwareKindV2 springKind=SpringHardwareKindV2::ProgressiveCoil;
    ProgressiveSpringDescription progressive{};
    DualRateSpringDescription dualRate{};
    LeafFrictionDescription leafFriction{};
    AirSpringDescriptionV2 air{};
    HydroPneumaticDescriptionV2 hydro{};
    TorsionBarDescription torsion{};
    double torsionEffectiveArmM=0.20;
    StopDescription bumpStop{};
    StopDescription reboundStop{};
    DamperDescriptionV3 damper{};
    Compliance6DofDescriptionV2 compliance{};
    SuspensionDamageDescriptionV2 damage{};
    ActiveActuatorDescriptionV2 actuator{};
    RideHeightControllerDescription activeController{};
    SemiActiveDamperControllerDescriptionV2 semiActiveController{};
    bool activeActuatorEnabled=false;
    bool builtInActiveControllerEnabled=false;
    bool semiActiveControllerEnabled=false;
    bool complianceEnabled=false;
    bool damageEnabled=true;
    double maximumSupportForceN=120000.0;
    // Maximum geometry set produced by a bent link/upright/mount. Direction follows
    // the prior hardpoint generalized load and is fed into the next kinematic solve.
    double maximumBentTranslationM=0.020;
    double maximumBentRotationRad=0.080;
};

struct SuspensionCornerStateV2
{
    AirSpringStateV2 air{};
    HydroPneumaticStateV2 hydro{};
    DamperStateV3 damper{};
    Compliance6DofStateV2 compliance{};
    SuspensionDamageStateV2 damage{};
    ActiveActuatorStateV2 actuator{};
    SuspVec6 complianceOffsetsForNextKinematics{};
    SuspVec6 damageOffsetsForNextKinematics{};
    double previousWheelCompressionM=0.0;
    double accumulatedSpringEnergyJ=0.0;
};

struct SuspensionCornerControlV2
{
    double activeForceCommandN=0.0;
    double semiActiveValveScale=1.0;
    double targetRideHeightErrorM=0.0;
    double targetWheelTravelM=0.0;
    double sprungVerticalVelocityMps=0.0;
    double unsprungVerticalVelocityMps=0.0;
};

struct SuspensionCornerTelemetryV2
{
    double wheelCompressionM=0.0;
    double springForceN=0.0;
    double springWheelForceN=0.0;
    double springTangentRateNPerM=0.0;
    double springFrictionForceN=0.0;
    double damperForceN=0.0;
    double damperWheelForceN=0.0;
    double damperTemperatureC=0.0;
    double damperCompressionPressurePa=0.0;
    double damperReboundPressurePa=0.0;
    double damperAeration=0.0;
    double bumpStopWheelForceN=0.0;
    double reboundStopWheelForceN=0.0;
    double activeWheelForceN=0.0;
    double activeMechanicalPowerW=0.0;
    double supportForceN=0.0;
    double generalizedWheelForceN=0.0;
    double airPressurePa=0.0;
    double hydroPressurePa=0.0;
    double complianceStoredEnergyJ=0.0;
    SuspVec6 complianceOffset{};
    std::uint32_t damageFlags=0;
    double damageWear=0.0;
    double damageLeakage=0.0;
    double damagePermanentSet=0.0;
    bool constraintEnabled=true;
    bool forceLimited=false;
};

struct SuspensionCornerResultV2
{
    double generalizedWheelForceN=0.0; // conjugate to positive wheel compression; negative resists bump
    double supportForceN=0.0;          // convenience = -generalizedWheelForceN
    SuspensionCornerTelemetryV2 telemetry{};
};

inline SuspensionCornerResultV2 stepSuspensionCornerV2(const SuspensionCornerDescriptionV2& d,
                                                        SuspensionCornerStateV2& s,
                                                        const SuspensionKinematicSampleV2& k,
                                                        const SuspensionCornerControlV2& control,
                                                        double ambientPressurePa,double ambientTemperatureC,
                                                        double dtSeconds)
{
    SuspensionCornerResultV2 out;
    const double dt=std::max(0.0,dtSeconds);
    const double travelDelta=k.wheelCompressionM-s.previousWheelCompressionM;
    s.previousWheelCompressionM=k.wheelCompressionM;

    SuspensionDamageResultV2 damageResponse;
    if(d.damageEnabled)
        damageResponse=stepSuspensionDamageV2(d.damage,s.damage,0.0,travelDelta,s.damper.temperatureC,0.0);

    double springForce=0.0,tangent=0.0,pressure=0.0;
    double springFrictionForce=0.0;
    switch(d.springKind)
    {
    case SpringHardwareKindV2::ProgressiveCoil:
    {
        auto r=evaluateProgressiveSpring(d.progressive,d.progressive.freeLengthMetres-k.springCompressionM);
        springForce=r.forceNewtons;tangent=r.tangentRateNPerM;s.accumulatedSpringEnergyJ=r.storedEnergyJoules;break;
    }
    case SpringHardwareKindV2::DualRateCoil:
    {
        auto r=evaluateDualRateSpring(d.dualRate,k.springCompressionM);
        springForce=r.forceNewtons;tangent=r.tangentRateNPerM;break;
    }
    case SpringHardwareKindV2::LeafPack:
    {
        auto r=evaluateProgressiveSpring(d.progressive,d.progressive.freeLengthMetres-k.springCompressionM);
        springForce=r.forceNewtons;tangent=r.tangentRateNPerM;s.accumulatedSpringEnergyJ=r.storedEnergyJoules;break;
    }
    case SpringHardwareKindV2::Air:
    {
        auto r=stepAirSpringV2(d.air,s.air,k.springCompressionM,k.springCompressionVelocityMps,
                               ambientPressurePa,ambientTemperatureC+273.15,control.targetRideHeightErrorM,dt,
                               damageResponse.leakageScale);
        springForce=r.forceN;pressure=r.chamberPressurePa;
        const double eps=1.0e-5;
        AirSpringStateV2 copy=s.air;
        auto rp=stepAirSpringV2(d.air,copy,k.springCompressionM+eps,k.springCompressionVelocityMps,
                                ambientPressurePa,ambientTemperatureC+273.15,control.targetRideHeightErrorM,0.0,
                                damageResponse.leakageScale);
        tangent=std::max(0.0,(rp.forceN-springForce)/eps);break;
    }
    case SpringHardwareKindV2::HydroPneumatic:
    {
        auto r=stepHydroPneumaticV2(d.hydro,s.hydro,k.springCompressionVelocityMps,ambientPressurePa,
                                    ambientTemperatureC+273.15,dt,damageResponse.leakageScale);
        springForce=r.forceN;pressure=r.linePressurePa;
        tangent=0.0;break;
    }
    case SpringHardwareKindV2::TorsionEquivalent:
    {
        const double arm=std::max(1.0e-5,std::abs(d.torsionEffectiveArmM));
        const double angle=k.springCompressionM/arm;
        const double torque=evaluateTorsionBarTorque(d.torsion,angle);
        springForce=std::max(0.0,torque/arm);
        tangent=std::max(0.0,d.torsion.rateNmPerRad/(arm*arm));break;
    }
    }

    const double stiffnessScale=damageResponse.stiffnessScale;
    springForce*=stiffnessScale;tangent*=stiffnessScale;
    if(d.springKind==SpringHardwareKindV2::LeafPack)
        springFrictionForce=evaluateLeafInterleafFriction(d.leafFriction,k.springCompressionVelocityMps)*damageResponse.dampingScale;
    double q=-springForce*k.springMotionRatio + springFrictionForce*k.springMotionRatio;

    const double bump=evaluateStopForce(d.bumpStop,k.bumpStopPenetrationM,k.bumpStopVelocityMps);
    const double rebound=evaluateStopForce(d.reboundStop,k.reboundStopPenetrationM,k.reboundStopVelocityMps);
    q-=bump;q+=rebound;

    const double damperExtension=-k.damperCompressionM;
    const double damperExtensionVelocity=-k.damperCompressionVelocityMps;
    const double valveScale=d.semiActiveControllerEnabled
        ? evaluateSemiActiveDamperValveV2(d.semiActiveController,control.sprungVerticalVelocityMps,control.unsprungVerticalVelocityMps)
        : control.semiActiveValveScale;
    auto dr=stepDamperV3(d.damper,s.damper,damperExtension,damperExtensionVelocity,
                         ambientTemperatureC,dt,valveScale,damageResponse.leakageScale);
    const double damperForce=dr.forceN*damageResponse.dampingScale;
    const double damperQ=damperForce*(-k.damperMotionRatio);
    q+=damperQ;

    ActiveActuatorResultV2 ar;
    if(d.activeActuatorEnabled && damageResponse.constraintEnabled)
    {
        double command=control.activeForceCommandN;
        if(d.builtInActiveControllerEnabled)
            command+=evaluateRideHeightCommand(d.activeController,control.targetWheelTravelM,k.wheelCompressionM,
                                               control.unsprungVerticalVelocityMps,control.sprungVerticalVelocityMps);
        ar=stepActiveActuatorV2(d.actuator,s.actuator,command,k.actuatorExtensionVelocityMps,dt);
        q+=ar.forceN*k.actuatorExtensionMotionRatio;
    }

    Compliance6DofResultV2 cr;
    if(d.complianceEnabled)
    {
        cr=stepCompliance6DofV2(d.compliance,s.compliance,k.hardpointGeneralizedLoad,s.damage.wear,dt);
        s.complianceOffsetsForNextKinematics=cr.deflection;
    }
    else s.complianceOffsetsForNextKinematics={};

    // Damage is driven by the actual local element load after forces are known, then immediately maps to physical state.
    if(d.damageEnabled)
    {
        double hardpointPeak=0.0;for(double v:k.hardpointGeneralizedLoad) hardpointPeak=std::max(hardpointPeak,std::abs(v));
        const double structuralLoad=std::max(hardpointPeak,springForce+std::abs(springFrictionForce)+std::abs(damperForce)+bump+rebound+std::abs(ar.forceN));
        damageResponse=stepSuspensionDamageV2(d.damage,s.damage,structuralLoad,travelDelta,s.damper.temperatureC,dt);
        SuspVec6 bent{};
        const double transMag=std::sqrt(k.hardpointGeneralizedLoad[0]*k.hardpointGeneralizedLoad[0]+k.hardpointGeneralizedLoad[1]*k.hardpointGeneralizedLoad[1]+k.hardpointGeneralizedLoad[2]*k.hardpointGeneralizedLoad[2]);
        const double rotMag=std::sqrt(k.hardpointGeneralizedLoad[3]*k.hardpointGeneralizedLoad[3]+k.hardpointGeneralizedLoad[4]*k.hardpointGeneralizedLoad[4]+k.hardpointGeneralizedLoad[5]*k.hardpointGeneralizedLoad[5]);
        if(transMag>1.0e-12) for(std::size_t i=0;i<3;++i) bent[i]=(k.hardpointGeneralizedLoad[i]/transMag)*d.maximumBentTranslationM*damageResponse.geometrySetScale;
        if(rotMag>1.0e-12) for(std::size_t i=3;i<6;++i) bent[i]=(k.hardpointGeneralizedLoad[i]/rotMag)*d.maximumBentRotationRad*damageResponse.geometrySetScale;
        s.damageOffsetsForNextKinematics=bent;
    }
    if(!damageResponse.constraintEnabled) q=0.0;
    const double maxSupport=std::abs(d.maximumSupportForceN);
    if(damageResponse.seized && damageResponse.constraintEnabled && std::abs(k.wheelCompressionVelocityMps)>1.0e-5)
        q-=suspSign(k.wheelCompressionVelocityMps)*maxSupport;
    bool limited=false;
    if(maxSupport>0.0 && std::abs(q)>maxSupport){q=suspSign(q)*maxSupport;limited=true;}

    out.generalizedWheelForceN=q;out.supportForceN=-q;
    auto& t=out.telemetry;
    t.wheelCompressionM=k.wheelCompressionM;t.springForceN=springForce;t.springWheelForceN=-springForce*k.springMotionRatio;
    t.springTangentRateNPerM=tangent*k.springMotionRatio*k.springMotionRatio;t.springFrictionForceN=springFrictionForce;
    t.damperForceN=damperForce;t.damperWheelForceN=damperQ;t.damperTemperatureC=s.damper.temperatureC;
    t.damperCompressionPressurePa=dr.compressionPressurePa;t.damperReboundPressurePa=dr.reboundPressurePa;t.damperAeration=dr.aeration;
    t.bumpStopWheelForceN=-bump;t.reboundStopWheelForceN=rebound;t.activeWheelForceN=ar.forceN*k.actuatorExtensionMotionRatio;
    t.activeMechanicalPowerW=ar.mechanicalPowerW;t.generalizedWheelForceN=q;t.supportForceN=-q;
    t.airPressurePa=d.springKind==SpringHardwareKindV2::Air?pressure:0.0;t.hydroPressurePa=d.springKind==SpringHardwareKindV2::HydroPneumatic?pressure:0.0;
    t.complianceStoredEnergyJ=cr.storedEnergyJ;t.complianceOffset=s.complianceOffsetsForNextKinematics;
    t.damageFlags=s.damage.flags;t.damageWear=s.damage.wear;t.damageLeakage=s.damage.leakage;t.damagePermanentSet=s.damage.permanentSet;
    t.constraintEnabled=damageResponse.constraintEnabled;t.forceLimited=limited||dr.forceLimited||ar.forceLimited||ar.powerLimited||ar.speedLimited;
    return out;
}

inline SuspVec6 suspensionMountOffsetForNextKinematicsV2(const SuspensionCornerStateV2& s)
{
    SuspVec6 r{};for(std::size_t i=0;i<6;++i)r[i]=s.complianceOffsetsForNextKinematics[i]+s.damageOffsetsForNextKinematics[i];return r;
}

struct SuspensionAxleCouplingDescriptionV2
{
    AntiRollBarDescription antiRoll{};
    ThirdElementDescription third{};
    HydraulicInterconnectDescription hydraulic{};
    InerterDescription inerter{};
    ActiveAntiRollDescription activeAntiRoll{};
    bool antiRollEnabled=false;
    bool thirdEnabled=false;
    bool hydraulicEnabled=false;
    bool inerterEnabled=false;
    bool activeAntiRollEnabled=false;
    double leftBarArmM=0.20;
    double rightBarArmM=0.20;
};
struct SuspensionAxleCouplingStateV2{HydraulicInterconnectState hydraulic{};};
struct SuspensionAxleCouplingInputV2
{
    double leftTravelM=0.0,rightTravelM=0.0,leftVelocityMps=0.0,rightVelocityMps=0.0;
    double leftAccelerationMps2=0.0,rightAccelerationMps2=0.0;
    double leftBarAngleRad=0.0,rightBarAngleRad=0.0,leftBarRateRadps=0.0,rightBarRateRadps=0.0;
    double bodyRollErrorRad=0.0,bodyRollRateRadps=0.0,lateralAccelerationMps2=0.0;
};
struct SuspensionAxleCouplingResultV2
{
    double leftGeneralizedForceN=0.0,rightGeneralizedForceN=0.0;
    double antiRollTorqueNm=0.0,activeAntiRollTorqueNm=0.0,thirdHeaveForceN=0.0,thirdRollForceN=0.0;
    double leftHydraulicForceN=0.0,rightHydraulicForceN=0.0;
    double leftInerterForceN=0.0,rightInerterForceN=0.0;
};
inline SuspensionAxleCouplingResultV2 stepSuspensionAxleCouplingV2(const SuspensionAxleCouplingDescriptionV2& d,
                                                                  SuspensionAxleCouplingStateV2& s,
                                                                  const SuspensionAxleCouplingInputV2& in,double dt)
{
    SuspensionAxleCouplingResultV2 r;
    if(d.antiRollEnabled)
    {
        r.antiRollTorqueNm=evaluateAntiRollTorque(d.antiRoll,in.leftBarAngleRad,in.rightBarAngleRad,in.leftBarRateRadps,in.rightBarRateRadps);
        r.leftGeneralizedForceN-=r.antiRollTorqueNm/std::max(1.0e-5,std::abs(d.leftBarArmM));
        r.rightGeneralizedForceN+=r.antiRollTorqueNm/std::max(1.0e-5,std::abs(d.rightBarArmM));
    }
    if(d.thirdEnabled)
    {
        auto f=evaluateThirdElement(d.third,in.leftTravelM,in.rightTravelM,in.leftVelocityMps,in.rightVelocityMps);
        r.thirdHeaveForceN=f.heaveForceN;r.thirdRollForceN=f.rollForceN;
        r.leftGeneralizedForceN+=f.leftForceN;r.rightGeneralizedForceN+=f.rightForceN;
    }
    if(d.hydraulicEnabled)
    {
        auto f=stepHydraulicInterconnect(d.hydraulic,s.hydraulic,in.leftVelocityMps,in.rightVelocityMps,dt);
        r.leftHydraulicForceN=f.leftForceN;r.rightHydraulicForceN=f.rightForceN;
        r.leftGeneralizedForceN+=f.leftForceN;r.rightGeneralizedForceN+=f.rightForceN;
    }
    if(d.inerterEnabled)
    {
        r.leftInerterForceN=evaluateInerterForce(d.inerter,in.leftAccelerationMps2-in.rightAccelerationMps2);
        r.rightInerterForceN=-r.leftInerterForceN;
        r.leftGeneralizedForceN+=r.leftInerterForceN;r.rightGeneralizedForceN+=r.rightInerterForceN;
    }
    if(d.activeAntiRollEnabled)
    {
        r.activeAntiRollTorqueNm=evaluateActiveAntiRollTorque(d.activeAntiRoll,in.bodyRollErrorRad,in.bodyRollRateRadps,in.lateralAccelerationMps2);
        r.leftGeneralizedForceN-=r.activeAntiRollTorqueNm/std::max(1.0e-5,std::abs(d.leftBarArmM));
        r.rightGeneralizedForceN+=r.activeAntiRollTorqueNm/std::max(1.0e-5,std::abs(d.rightBarArmM));
    }
    return r;
}

struct VehicleSuspensionRuntimeV2
{
    static constexpr std::size_t MaxCorners=16;
    static constexpr std::size_t MaxAxles=8;
    std::array<SuspensionCornerStateV2,MaxCorners> corners{};
    std::array<SuspensionAxleCouplingStateV2,MaxAxles> axles{};
    std::size_t cornerCount=0,axleCount=0;
    std::uint64_t stepCounter=0;
};


struct SuspensionAxleMapV2
{
    std::size_t leftCorner=0;
    std::size_t rightCorner=1;
};

struct SuspensionVehicleDescriptionV2
{
    static constexpr std::size_t MaxCorners=VehicleSuspensionRuntimeV2::MaxCorners;
    static constexpr std::size_t MaxAxles=VehicleSuspensionRuntimeV2::MaxAxles;
    std::array<SuspensionCornerDescriptionV2,MaxCorners> corners{};
    std::array<SuspensionAxleCouplingDescriptionV2,MaxAxles> axles{};
    std::array<SuspensionAxleMapV2,MaxAxles> axleMap{};
    std::size_t cornerCount=0;
    std::size_t axleCount=0;
};

struct SuspensionAxleKinematicSampleV2
{
    bool exactBarKinematics=false;
    double leftBarAngleRad=0.0,rightBarAngleRad=0.0,leftBarRateRadps=0.0,rightBarRateRadps=0.0;
    double bodyRollErrorRad=0.0,bodyRollRateRadps=0.0,lateralAccelerationMps2=0.0;
};

struct SuspensionVehicleStepInputV2
{
    std::array<SuspensionKinematicSampleV2,VehicleSuspensionRuntimeV2::MaxCorners> kinematics{};
    std::array<SuspensionCornerControlV2,VehicleSuspensionRuntimeV2::MaxCorners> controls{};
    std::array<SuspensionAxleKinematicSampleV2,VehicleSuspensionRuntimeV2::MaxAxles> axleKinematics{};
    double ambientPressurePa=101325.0;
    double ambientTemperatureC=20.0;
};

struct SuspensionVehicleStepResultV2
{
    std::array<SuspensionCornerResultV2,VehicleSuspensionRuntimeV2::MaxCorners> corners{};
    std::array<SuspensionAxleCouplingResultV2,VehicleSuspensionRuntimeV2::MaxAxles> axles{};
    std::size_t cornerCount=0;
    std::size_t axleCount=0;
    bool valid=false;
};

inline bool validateSuspensionVehicleDescriptionV2(const SuspensionVehicleDescriptionV2& d)
{
    if(d.cornerCount==0 || d.cornerCount>SuspensionVehicleDescriptionV2::MaxCorners ||
       d.axleCount>SuspensionVehicleDescriptionV2::MaxAxles) return false;
    for(std::size_t a=0;a<d.axleCount;++a)
    {
        const auto& m=d.axleMap[a];
        if(m.leftCorner>=d.cornerCount || m.rightCorner>=d.cornerCount || m.leftCorner==m.rightCorner) return false;
    }
    return true;
}

// SUSP24 production force coordinator. Kinematics are solved first by exactly one geometry
// provider per corner. This function then owns the complete physical element force order and
// all axle interconnections. Its generalized force is the single value VehicleSystem applies
// back to the unsprung/wheel support coordinate; no spring/damper/ARB force may be added twice.
inline SuspensionVehicleStepResultV2 stepVehicleSuspensionV2(const SuspensionVehicleDescriptionV2& d,
                                                              VehicleSuspensionRuntimeV2& state,
                                                              const SuspensionVehicleStepInputV2& input,
                                                              double dtSeconds)
{
    SuspensionVehicleStepResultV2 r;
    if(!validateSuspensionVehicleDescriptionV2(d) || !std::isfinite(dtSeconds) || dtSeconds<=0.0) return r;
    state.cornerCount=d.cornerCount;state.axleCount=d.axleCount;
    r.cornerCount=d.cornerCount;r.axleCount=d.axleCount;
    for(std::size_t c=0;c<d.cornerCount;++c)
    {
        r.corners[c]=stepSuspensionCornerV2(d.corners[c],state.corners[c],input.kinematics[c],input.controls[c],
                                            input.ambientPressurePa,input.ambientTemperatureC,dtSeconds);
        if(!std::isfinite(r.corners[c].generalizedWheelForceN)) return SuspensionVehicleStepResultV2{};
    }
    for(std::size_t a=0;a<d.axleCount;++a)
    {
        const auto& map=d.axleMap[a];
        SuspensionAxleCouplingInputV2 ai;
        ai.leftTravelM=input.kinematics[map.leftCorner].wheelCompressionM;
        ai.rightTravelM=input.kinematics[map.rightCorner].wheelCompressionM;
        ai.leftVelocityMps=input.kinematics[map.leftCorner].wheelCompressionVelocityMps;
        ai.rightVelocityMps=input.kinematics[map.rightCorner].wheelCompressionVelocityMps;
        ai.leftAccelerationMps2=input.kinematics[map.leftCorner].wheelCompressionAccelerationMps2;
        ai.rightAccelerationMps2=input.kinematics[map.rightCorner].wheelCompressionAccelerationMps2;
        const auto& ak=input.axleKinematics[a];
        ai.bodyRollErrorRad=ak.bodyRollErrorRad;ai.bodyRollRateRadps=ak.bodyRollRateRadps;ai.lateralAccelerationMps2=ak.lateralAccelerationMps2;
        if(ak.exactBarKinematics)
        {ai.leftBarAngleRad=ak.leftBarAngleRad;ai.rightBarAngleRad=ak.rightBarAngleRad;ai.leftBarRateRadps=ak.leftBarRateRadps;ai.rightBarRateRadps=ak.rightBarRateRadps;}
        else
        {
            ai.leftBarAngleRad=ai.leftTravelM/std::max(1.0e-5,std::abs(d.axles[a].leftBarArmM));
            ai.rightBarAngleRad=ai.rightTravelM/std::max(1.0e-5,std::abs(d.axles[a].rightBarArmM));
            ai.leftBarRateRadps=ai.leftVelocityMps/std::max(1.0e-5,std::abs(d.axles[a].leftBarArmM));
            ai.rightBarRateRadps=ai.rightVelocityMps/std::max(1.0e-5,std::abs(d.axles[a].rightBarArmM));
        }
        r.axles[a]=stepSuspensionAxleCouplingV2(d.axles[a],state.axles[a],ai,dtSeconds);
        auto& left=r.corners[map.leftCorner]; auto& right=r.corners[map.rightCorner];
        left.generalizedWheelForceN+=r.axles[a].leftGeneralizedForceN;
        right.generalizedWheelForceN+=r.axles[a].rightGeneralizedForceN;
        left.supportForceN=-left.generalizedWheelForceN; right.supportForceN=-right.generalizedWheelForceN;
        left.telemetry.generalizedWheelForceN=left.generalizedWheelForceN;
        right.telemetry.generalizedWheelForceN=right.generalizedWheelForceN;
        left.telemetry.supportForceN=left.supportForceN;right.telemetry.supportForceN=right.supportForceN;
    }
    ++state.stepCounter;r.valid=true;return r;
}

} // namespace heritage::vehicles::suspension

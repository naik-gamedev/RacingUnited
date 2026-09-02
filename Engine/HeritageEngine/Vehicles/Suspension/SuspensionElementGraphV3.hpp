#pragma once
#include "SuspensionMath.hpp"
#include "SuspensionScalarElements.hpp"
#include "DamperModelV3.hpp"
#include "PneumaticHydraulicSpringV2.hpp"
#include "SuspensionComplianceDynamic.hpp"
#include "SuspensionDamageV2.hpp"
#include "ActiveSuspensionV2.hpp"
#include "InterconnectedSuspension.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace heritage::vehicles::suspension
{

struct SuspVec3V3
{
    double x=0.0,y=0.0,z=0.0;
};
inline SuspVec3V3 operator+(SuspVec3V3 a,SuspVec3V3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline SuspVec3V3 operator-(SuspVec3V3 a,SuspVec3V3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline SuspVec3V3 operator*(SuspVec3V3 a,double s){return {a.x*s,a.y*s,a.z*s};}
inline double suspDotV3(SuspVec3V3 a,SuspVec3V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline double suspNormV3(SuspVec3V3 a){return std::sqrt(std::max(0.0,suspDotV3(a,a)));}
inline SuspVec3V3 suspNormalizedV3(SuspVec3V3 a)
{
    const double n=suspNormV3(a); return n>1.0e-15?a*(1.0/n):SuspVec3V3{};
}

struct SuspMat3V3
{
    std::array<std::array<double,3>,3> m{{{{1,0,0}},{{0,1,0}},{{0,0,1}}}};
};
inline SuspVec3V3 suspMulV3(const SuspMat3V3& r,SuspVec3V3 v)
{
    return {
        r.m[0][0]*v.x+r.m[0][1]*v.y+r.m[0][2]*v.z,
        r.m[1][0]*v.x+r.m[1][1]*v.y+r.m[1][2]*v.z,
        r.m[2][0]*v.x+r.m[2][1]*v.y+r.m[2][2]*v.z
    };
}

struct SuspensionFramePoseV3
{
    std::uint32_t id=0;
    SuspVec3V3 position{};
    SuspMat3V3 orientation{};
    SuspVec3V3 linearVelocity{};
    SuspVec3V3 angularVelocity{};
    SuspVec3V3 linearAcceleration{};
    SuspVec3V3 angularAcceleration{};
    // Analytic derivatives with respect to positive wheel compression q.
    SuspVec3V3 dPositionDWheel{};
    SuspVec3V3 dAngularDWheel{};
    SuspVec3V3 d2PositionDWheel2{};
    SuspVec3V3 d2AngularDWheel2{};
    bool hasWheelDerivatives=false;
    bool valid=false;
};


struct SuspensionConstraintLoadV3
{
    std::uint32_t elementId=0;
    SuspVec6 generalizedLoad{};
    bool valid=false;
};
struct SuspensionFrameSetV3
{
    static constexpr std::size_t Capacity=24;
    std::array<SuspensionFramePoseV3,Capacity> frames{};
    std::size_t count=0;
    std::array<SuspensionConstraintLoadV3,64> constraintLoads{};
    std::size_t constraintLoadCount=0;
    bool constraintOverridesConsumed=false;

    const SuspensionFramePoseV3* find(std::uint32_t id) const
    {
        for(std::size_t i=0;i<count&&i<Capacity;++i) if(frames[i].valid&&frames[i].id==id) return &frames[i];
        return nullptr;
    }
    const SuspensionConstraintLoadV3* findConstraintLoad(std::uint32_t elementId) const
    {
        for(std::size_t i=0;i<constraintLoadCount&&i<constraintLoads.size();++i)
            if(constraintLoads[i].valid&&constraintLoads[i].elementId==elementId)return &constraintLoads[i];
        return nullptr;
    }
};

struct SuspensionAttachmentV3
{
    std::uint8_t corner=0;
    std::uint32_t frameId=0;
    SuspVec3V3 localPoint{};
};

inline SuspVec3V3 suspensionWorldPointV3(const SuspensionFrameSetV3& frames,const SuspensionAttachmentV3& a)
{
    const auto* f=frames.find(a.frameId);
    return f?f->position+suspMulV3(f->orientation,a.localPoint):SuspVec3V3{};
}

inline SuspVec3V3 suspensionWorldPointVelocityV3(const SuspensionFrameSetV3& frames,const SuspensionAttachmentV3& a)
{
    const auto* f=frames.find(a.frameId); if(!f) return {};
    const SuspVec3V3 r=suspMulV3(f->orientation,a.localPoint);
    const SuspVec3V3 w=f->angularVelocity;
    const SuspVec3V3 cross{w.y*r.z-w.z*r.y,w.z*r.x-w.x*r.z,w.x*r.y-w.y*r.x};
    return f->linearVelocity+cross;
}

inline SuspVec3V3 suspensionCrossV3(SuspVec3V3 a,SuspVec3V3 b)
{
    return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}

inline SuspVec3V3 suspensionWorldPointAccelerationV3(const SuspensionFrameSetV3& frames,const SuspensionAttachmentV3& a)
{
    const auto* f=frames.find(a.frameId); if(!f) return {};
    const SuspVec3V3 r=suspMulV3(f->orientation,a.localPoint);
    return f->linearAcceleration+suspensionCrossV3(f->angularAcceleration,r)+suspensionCrossV3(f->angularVelocity,suspensionCrossV3(f->angularVelocity,r));
}

inline bool suspensionWorldPointWheelDerivativesV3(const SuspensionFrameSetV3& frames,const SuspensionAttachmentV3& a,
                                                     SuspVec3V3& first,SuspVec3V3& second)
{
    const auto* f=frames.find(a.frameId); if(!f||!f->hasWheelDerivatives) return false;
    const SuspVec3V3 r=suspMulV3(f->orientation,a.localPoint);
    first=f->dPositionDWheel+suspensionCrossV3(f->dAngularDWheel,r);
    second=f->d2PositionDWheel2+suspensionCrossV3(f->d2AngularDWheel2,r)+suspensionCrossV3(f->dAngularDWheel,suspensionCrossV3(f->dAngularDWheel,r));
    return true;
}

enum class SuspensionElementKindV3 : std::uint8_t
{
    StructuralLink,
    BallJoint,
    Bushing6Dof,
    CoilSpring,
    DualRateSpring,
    LeafSpring,
    AirSpring,
    HydroPneumaticSpring,
    TorsionEquivalent,
    Damper,
    BumpStop,
    ReboundStop,
    ActiveActuator,
    AntiRollDropLink,
    ThirdElementLink,
    InerterLink,
    HydraulicPistonLink,
    Mount
};

inline constexpr bool suspensionElementCarriesConstraintV3(SuspensionElementKindV3 k)
{
    switch(k)
    {
    case SuspensionElementKindV3::StructuralLink:
    case SuspensionElementKindV3::BallJoint:
    case SuspensionElementKindV3::Bushing6Dof:
    case SuspensionElementKindV3::AntiRollDropLink:
    case SuspensionElementKindV3::Mount:return true;
    default:return false;
    }
}
inline constexpr bool suspensionElementCarriesForceV3(SuspensionElementKindV3 k)
{
    switch(k)
    {
    case SuspensionElementKindV3::CoilSpring:
    case SuspensionElementKindV3::DualRateSpring:
    case SuspensionElementKindV3::LeafSpring:
    case SuspensionElementKindV3::AirSpring:
    case SuspensionElementKindV3::HydroPneumaticSpring:
    case SuspensionElementKindV3::TorsionEquivalent:
    case SuspensionElementKindV3::Damper:
    case SuspensionElementKindV3::BumpStop:
    case SuspensionElementKindV3::ReboundStop:
    case SuspensionElementKindV3::ActiveActuator:
    case SuspensionElementKindV3::InerterLink:return true;
    default:return false;
    }
}

struct SuspensionElementDescriptionV3
{
    std::uint32_t id=0;
    SuspensionElementKindV3 kind=SuspensionElementKindV3::StructuralLink;
    SuspensionAttachmentV3 a{},b{};
    double referenceLengthM=0.0;
    double preloadCompressionM=0.0;
    double maximumForceN=120000.0;
    bool enabled=true;
    bool damageEnabled=true;
    bool complianceEnabled=false;

    ProgressiveSpringDescription progressive{};
    DualRateSpringDescription dualRate{};
    LeafFrictionDescription leafFriction{};
    AirSpringDescriptionV2 air{};
    HydroPneumaticDescriptionV2 hydro{};
    TorsionBarDescription torsion{};
    double torsionEffectiveArmM=0.20;
    DamperDescriptionV3 damper{};
    StopDescription stop{};
    ActiveActuatorDescriptionV2 actuator{};
    InerterDescription inerter{};
    Compliance6DofDescriptionV2 compliance{};
    SuspensionDamageDescriptionV2 damage{};
};

struct SuspensionElementStateV3
{
    AirSpringStateV2 air{};
    HydroPneumaticStateV2 hydro{};
    DamperStateV3 damper{};
    ActiveActuatorStateV2 actuator{};
    Compliance6DofStateV2 compliance{};
    SuspensionDamageStateV2 damage{};
    SuspVec6 complianceFeedback{};
    SuspVec6 permanentSetFeedback{};
    double previousLengthM=0.0;
    double previousPathVelocityMps=0.0;
    double accumulatedEnergyJ=0.0;
    double lastForceN=0.0;
    bool initialized=false;
    bool constraintEnabled=true;
};

struct SuspensionElementGeometryV3
{
    double lengthM=0.0;
    double compressionM=0.0;
    double pathVelocityMps=0.0;
    double pathAccelerationMps2=0.0;
    double dCompressionDWheel=0.0;
    double dLengthDWheel=0.0;
    SuspVec3V3 axis{};
    bool valid=false;
};

struct SuspensionElementControlV3
{
    double activeForceCommandN=0.0;
    double semiActiveValveScale=1.0;
    double targetRideHeightErrorM=0.0;
    double ambientPressurePa=101325.0;
    double ambientTemperatureC=20.0;
};

struct SuspensionElementTelemetryV3
{
    std::uint32_t id=0;
    SuspensionElementKindV3 kind=SuspensionElementKindV3::StructuralLink;
    double lengthM=0.0;
    double compressionM=0.0;
    double motionRatio=0.0;
    double localForceN=0.0;
    double generalizedWheelForceN=0.0;
    double temperatureC=0.0;
    double pressurePa=0.0;
    double wear=0.0;
    double leakage=0.0;
    std::uint32_t damageFlags=0;
    bool constraintEnabled=true;
    bool forceLimited=false;
};

struct SuspensionElementStepResultV3
{
    double generalizedWheelForceN=0.0;
    SuspensionElementTelemetryV3 telemetry{};
};

struct SuspensionConstraintOverrideV3
{
    std::uint32_t elementId=0;
    bool enabled=true;
    SuspVec6 endpointOffset{};
    double freePlayScale=1.0;
};

struct SuspensionConstraintOverrideSetV3
{
    static constexpr std::size_t Capacity=64;
    std::array<SuspensionConstraintOverrideV3,Capacity> entries{};
    std::size_t count=0;
};

inline SuspensionElementStepResultV3 stepSuspensionElementV3(
    const SuspensionElementDescriptionV3& d,SuspensionElementStateV3& s,
    const SuspensionElementGeometryV3& g,const SuspensionElementControlV3& control,
    const SuspVec6& generalizedStructuralLoad,double dtSeconds)
{
    SuspensionElementStepResultV3 out; auto& t=out.telemetry;
    t.id=d.id;t.kind=d.kind;t.lengthM=g.lengthM;t.compressionM=g.compressionM;t.motionRatio=g.dCompressionDWheel;
    if(!d.enabled||!g.valid) return out;
    const double dt=std::max(0.0,dtSeconds);
    if(!s.initialized){s.previousLengthM=g.lengthM;s.previousPathVelocityMps=g.pathVelocityMps;s.initialized=true;}

    double structuralMagnitude=0.0; for(double x:generalizedStructuralLoad) structuralMagnitude=std::max(structuralMagnitude,std::abs(x));
    SuspensionDamageResultV2 damage{};
    if(d.damageEnabled)
        damage=stepSuspensionDamageV2(d.damage,s.damage,structuralMagnitude,0.0,s.damper.temperatureC,0.0);

    double localForce=0.0;
    double pressure=0.0;
    bool limited=false;
    switch(d.kind)
    {
    case SuspensionElementKindV3::CoilSpring:
    {
        const auto r=evaluateProgressiveSpring(d.progressive,d.progressive.freeLengthMetres-g.compressionM-d.preloadCompressionM);
        localForce=r.forceNewtons;s.accumulatedEnergyJ=r.storedEnergyJoules;break;
    }
    case SuspensionElementKindV3::DualRateSpring:
    {
        const auto r=evaluateDualRateSpring(d.dualRate,g.compressionM+d.preloadCompressionM);localForce=r.forceNewtons;break;
    }
    case SuspensionElementKindV3::LeafSpring:
    {
        const auto r=evaluateProgressiveSpring(d.progressive,d.progressive.freeLengthMetres-g.compressionM-d.preloadCompressionM);
        localForce=r.forceNewtons-evaluateLeafInterleafFriction(d.leafFriction,g.pathVelocityMps);s.accumulatedEnergyJ=r.storedEnergyJoules;break;
    }
    case SuspensionElementKindV3::AirSpring:
    {
        const auto r=stepAirSpringV2(d.air,s.air,g.compressionM,g.pathVelocityMps,control.ambientPressurePa,
                                     control.ambientTemperatureC+273.15,control.targetRideHeightErrorM,dt,damage.leakageScale);
        localForce=r.forceN;pressure=r.chamberPressurePa;break;
    }
    case SuspensionElementKindV3::HydroPneumaticSpring:
    {
        const auto r=stepHydroPneumaticV2(d.hydro,s.hydro,g.pathVelocityMps,control.ambientPressurePa,
                                          control.ambientTemperatureC+273.15,dt,damage.leakageScale);
        localForce=r.forceN;pressure=r.linePressurePa;break;
    }
    case SuspensionElementKindV3::TorsionEquivalent:
    {
        const double arm=std::max(1.0e-6,std::abs(d.torsionEffectiveArmM));
        localForce=evaluateTorsionBarTorque(d.torsion,(g.compressionM+d.preloadCompressionM)/arm)/arm;break;
    }
    case SuspensionElementKindV3::Damper:
    {
        const auto r=stepDamperV3(d.damper,s.damper,-g.compressionM,-g.pathVelocityMps,
                                  control.ambientTemperatureC,dt,control.semiActiveValveScale,damage.leakageScale);
        localForce=r.forceN;limited=r.forceLimited;break;
    }
    case SuspensionElementKindV3::BumpStop:
    case SuspensionElementKindV3::ReboundStop:
        localForce=evaluateStopForce(d.stop,std::max(0.0,g.compressionM),g.pathVelocityMps);break;
    case SuspensionElementKindV3::ActiveActuator:
    {
        const auto r=stepActiveActuatorV2(d.actuator,s.actuator,control.activeForceCommandN,-g.pathVelocityMps,dt);
        localForce=r.forceN;limited=r.forceLimited||r.powerLimited||r.speedLimited;break;
    }
    case SuspensionElementKindV3::InerterLink:
    {
        localForce=std::max(0.0,d.inerter.inertanceKg)*g.pathAccelerationMps2;break;
    }
    default:break;
    }

    if(d.damageEnabled)
    {
        const double actualLoad=std::max(structuralMagnitude,std::abs(localForce));
        damage=stepSuspensionDamageV2(d.damage,s.damage,actualLoad,std::abs(g.lengthM-s.previousLengthM),s.damper.temperatureC,dt);
        if(damage.detached || ((s.damage.flags&DamageBrokenV2)!=0u && suspensionElementCarriesForceV3(d.kind))) localForce=0.0;
    }

    if(d.complianceEnabled)
    {
        const auto cr=stepCompliance6DofV2(d.compliance,s.compliance,generalizedStructuralLoad,s.damage.wear,dt);
        s.complianceFeedback=cr.deflection;
    }
    else s.complianceFeedback={};

    SuspVec6 set{};
    if(d.damageEnabled&&damage.geometrySetScale>0.0)
    {
        double tn=std::sqrt(generalizedStructuralLoad[0]*generalizedStructuralLoad[0]+generalizedStructuralLoad[1]*generalizedStructuralLoad[1]+generalizedStructuralLoad[2]*generalizedStructuralLoad[2]);
        double rn=std::sqrt(generalizedStructuralLoad[3]*generalizedStructuralLoad[3]+generalizedStructuralLoad[4]*generalizedStructuralLoad[4]+generalizedStructuralLoad[5]*generalizedStructuralLoad[5]);
        if(tn>1e-12) for(std::size_t i=0;i<3;++i)set[i]=0.02*damage.geometrySetScale*generalizedStructuralLoad[i]/tn;
        if(rn>1e-12) for(std::size_t i=3;i<6;++i)set[i]=0.08*damage.geometrySetScale*generalizedStructuralLoad[i]/rn;
    }
    s.permanentSetFeedback=set;
    const bool brokenConstraint=((s.damage.flags&DamageBrokenV2)!=0u)&&suspensionElementCarriesConstraintV3(d.kind);
    s.constraintEnabled=!d.damageEnabled||(damage.constraintEnabled&&!brokenConstraint);

    const double maxF=std::abs(d.maximumForceN);
    if(maxF>0.0&&std::abs(localForce)>maxF){localForce=suspSign(localForce)*maxF;limited=true;}
    s.lastForceN=localForce;s.previousLengthM=g.lengthM;s.previousPathVelocityMps=g.pathVelocityMps;

    // Positive compression produces a resisting negative generalized wheel force.
    out.generalizedWheelForceN=-localForce*g.dCompressionDWheel;
    t.localForceN=localForce;t.generalizedWheelForceN=out.generalizedWheelForceN;t.temperatureC=s.damper.temperatureC;
    t.pressurePa=pressure;t.wear=s.damage.wear;t.leakage=s.damage.leakage;t.damageFlags=s.damage.flags;
    t.constraintEnabled=s.constraintEnabled;t.forceLimited=limited;
    return out;
}

struct SuspensionElementGraphDescriptionV3
{
    static constexpr std::size_t Capacity=64;
    std::array<SuspensionElementDescriptionV3,Capacity> elements{};
    std::size_t count=0;
};
struct SuspensionElementGraphStateV3
{
    static constexpr std::size_t Capacity=64;
    std::array<SuspensionElementStateV3,Capacity> elements{};
};

inline bool validateSuspensionElementGraphV3(const SuspensionElementGraphDescriptionV3& g)
{
    if(g.count>SuspensionElementGraphDescriptionV3::Capacity)return false;
    for(std::size_t i=0;i<g.count;++i)
    {
        if(g.elements[i].id==0u)return false;
        for(std::size_t j=i+1;j<g.count;++j)if(g.elements[i].id==g.elements[j].id)return false;
    }
    return true;
}

inline SuspensionConstraintOverrideSetV3 buildConstraintOverridesV3(
    const SuspensionElementGraphDescriptionV3& d,const SuspensionElementGraphStateV3& s)
{
    SuspensionConstraintOverrideSetV3 out;
    for(std::size_t i=0;i<d.count&&i<SuspensionConstraintOverrideSetV3::Capacity;++i)
    {
        if(!suspensionElementCarriesConstraintV3(d.elements[i].kind))continue;
        auto& e=out.entries[out.count++];e.elementId=d.elements[i].id;e.enabled=s.elements[i].constraintEnabled;
        for(std::size_t k=0;k<6;++k)e.endpointOffset[k]=s.elements[i].complianceFeedback[k]+s.elements[i].permanentSetFeedback[k];
        e.freePlayScale=1.0+8.0*s.elements[i].damage.wear;
    }
    return out;
}

} // namespace heritage::vehicles::suspension

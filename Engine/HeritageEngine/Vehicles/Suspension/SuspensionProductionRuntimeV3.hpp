#pragma once
#include "SuspensionElementGraphV3.hpp"
#include "SuspensionGeometryJacobianV3.hpp"
#include "InterconnectedSuspension.hpp"
#include "ActiveSuspension.hpp"
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace heritage::vehicles::suspension
{

struct SuspensionCornerGraphDescriptionV3
{
    SuspensionElementGraphDescriptionV3 graph{};
};
struct SuspensionCornerGraphStateV3
{
    SuspensionElementGraphStateV3 graph{};
};
struct SuspensionCornerGraphControlV3
{
    static constexpr std::size_t Capacity=64;
    std::array<SuspensionElementControlV3,Capacity> elements{};
};
struct SuspensionCornerGraphInputV3
{
    SuspensionGeometryProviderV3 provider{};
    SuspensionGeometrySolveRequestV3 geometryRequest{};
};
struct SuspensionCornerGraphResultV3
{
    double generalizedWheelForceN=0.0;
    SuspensionGeometrySampleSetV3 geometry{};
    std::array<SuspensionElementTelemetryV3,64> elements{};
    std::size_t elementCount=0;
    SuspensionConstraintOverrideSetV3 overridesForNextStep{};
    bool valid=false;
};

inline SuspensionCornerGraphResultV3 stepSuspensionCornerGraphV3(
    const SuspensionCornerGraphDescriptionV3& d,SuspensionCornerGraphStateV3& s,
    const SuspensionCornerGraphInputV3& in,const SuspensionCornerGraphControlV3& control,double dtSeconds)
{
    SuspensionCornerGraphResultV3 out;
    if(!validateSuspensionElementGraphV3(d.graph))return out;
    auto req=in.geometryRequest;
    const auto overrides=buildConstraintOverridesV3(d.graph,s.graph);req.constraintOverrides=&overrides;
    out.geometry=evaluateSuspensionGeometryV3(d.graph,in.provider,req);
    if(!out.geometry.converged)return out;
    out.elementCount=d.graph.count;
    for(std::size_t i=0;i<d.graph.count;++i)
    {
        SuspVec6 load{};
        if(const auto* reported=out.geometry.currentFrames.findConstraintLoad(d.graph.elements[i].id))load=reported->generalizedLoad;
        const auto r=stepSuspensionElementV3(d.graph.elements[i],s.graph.elements[i],out.geometry.elements[i],control.elements[i],load,dtSeconds);
        out.generalizedWheelForceN+=r.generalizedWheelForceN;out.elements[i]=r.telemetry;
    }
    out.overridesForNextStep=buildConstraintOverridesV3(d.graph,s.graph);out.valid=true;return out;
}

struct SuspensionRotaryPathV3
{
    std::uint32_t frameId=0;
    SuspVec3V3 localArmDirection{1,0,0};
    SuspVec3V3 referenceDirection{1,0,0};
    SuspVec3V3 axis{0,0,1};
};
struct SuspensionRotarySampleV3
{
    double angleRad=0.0;
    double rateRadps=0.0;
    double dAngleDWheel=0.0;
    bool valid=false;
};
inline SuspensionRotarySampleV3 evaluateSuspensionRotaryPathV3(const SuspensionFrameSetV3& frames,const SuspensionRotaryPathV3& p)
{
    SuspensionRotarySampleV3 out;const auto* f=frames.find(p.frameId);if(!f)return out;
    const auto axis=suspNormalizedV3(p.axis),ref=suspNormalizedV3(p.referenceDirection),cur=suspNormalizedV3(suspMulV3(f->orientation,p.localArmDirection));
    out.angleRad=std::atan2(suspDotV3(axis,suspensionCrossV3(ref,cur)),suspDotV3(ref,cur));
    out.rateRadps=suspDotV3(axis,f->angularVelocity);out.dAngleDWheel=f->hasWheelDerivatives?suspDotV3(axis,f->dAngularDWheel):0.0;
    out.valid=std::isfinite(out.angleRad)&&std::isfinite(out.rateRadps)&&(f->hasWheelDerivatives||std::abs(out.rateRadps)>0.0);return out;
}
inline const SuspensionElementGeometryV3* suspensionFindElementGeometryV3(const SuspensionCornerGraphDescriptionV3& d,const SuspensionCornerGraphResultV3& r,std::uint32_t id)
{
    for(std::size_t i=0;i<d.graph.count;++i)if(d.graph.elements[i].id==id)return &r.geometry.elements[i];
    return nullptr;
}
inline const SuspensionElementStateV3* suspensionFindElementStateV3(const SuspensionCornerGraphDescriptionV3& d,const SuspensionCornerGraphStateV3& s,std::uint32_t id)
{
    for(std::size_t i=0;i<d.graph.count;++i)if(d.graph.elements[i].id==id)return &s.graph.elements[i];
    return nullptr;
}

struct SuspensionAxleGraphDescriptionV3
{
    AntiRollBarDescription antiRoll{};
    ThirdElementDescription third{};
    HydraulicInterconnectDescription hydraulic{};
    ActiveAntiRollDescription activeAntiRoll{};
    double inerterKg=0.0;
    double maximumInerterForceN=50000.0;
    SuspensionRotaryPathV3 leftBarPath{},rightBarPath{};
    std::uint32_t leftDropLinkElementId=0,rightDropLinkElementId=0;
    std::uint32_t leftThirdPathElementId=0,rightThirdPathElementId=0;
    std::uint32_t leftHydraulicPathElementId=0,rightHydraulicPathElementId=0;
    std::uint32_t leftInerterPathElementId=0,rightInerterPathElementId=0;
    bool antiRollEnabled=false,thirdEnabled=false,hydraulicEnabled=false,inerterEnabled=false,activeAntiRollEnabled=false;
};
struct SuspensionAxleGraphStateV3{HydraulicInterconnectState hydraulic{};};
struct SuspensionAxleGraphControlV3
{
    double bodyRollErrorRad=0,bodyRollRateRadps=0,lateralAccelerationMps2=0;
};
struct SuspensionAxleGraphResultV3{double leftGeneralizedForceN=0,rightGeneralizedForceN=0;bool valid=false;};

inline bool suspensionConstraintPathEnabledV3(const SuspensionCornerGraphDescriptionV3& d,const SuspensionCornerGraphStateV3& s,std::uint32_t id)
{
    if(id==0)return true;
    const auto* e=suspensionFindElementStateV3(d,s,id);return e&&e->constraintEnabled;
}

inline SuspensionAxleGraphResultV3 stepSuspensionAxleGraphV3(const SuspensionAxleGraphDescriptionV3& d,
    SuspensionAxleGraphStateV3& s,const SuspensionCornerGraphDescriptionV3& ld,const SuspensionCornerGraphStateV3& ls,const SuspensionCornerGraphResultV3& lr,
    const SuspensionCornerGraphDescriptionV3& rd,const SuspensionCornerGraphStateV3& rs,const SuspensionCornerGraphResultV3& rr,
    const SuspensionAxleGraphControlV3& control,double dtSeconds)
{
    SuspensionAxleGraphResultV3 r;
    if(d.antiRollEnabled||d.activeAntiRollEnabled)
    {
        const auto lbar=evaluateSuspensionRotaryPathV3(lr.geometry.currentFrames,d.leftBarPath),rbar=evaluateSuspensionRotaryPathV3(rr.geometry.currentFrames,d.rightBarPath);
        const bool links=suspensionConstraintPathEnabledV3(ld,ls,d.leftDropLinkElementId)&&suspensionConstraintPathEnabledV3(rd,rs,d.rightDropLinkElementId);
        if(!lbar.valid||!rbar.valid)return r;
        if(links&&d.antiRollEnabled)
        {
            const double t=evaluateAntiRollTorque(d.antiRoll,lbar.angleRad,rbar.angleRad,lbar.rateRadps,rbar.rateRadps);
            r.leftGeneralizedForceN-=t*lbar.dAngleDWheel;r.rightGeneralizedForceN+=t*rbar.dAngleDWheel;
        }
        if(links&&d.activeAntiRollEnabled)
        {
            const double t=evaluateActiveAntiRollTorque(d.activeAntiRoll,control.bodyRollErrorRad,control.bodyRollRateRadps,control.lateralAccelerationMps2);
            r.leftGeneralizedForceN-=t*lbar.dAngleDWheel;r.rightGeneralizedForceN+=t*rbar.dAngleDWheel;
        }
    }
    if(d.thirdEnabled)
    {
        const auto* l=suspensionFindElementGeometryV3(ld,lr,d.leftThirdPathElementId);const auto* q=suspensionFindElementGeometryV3(rd,rr,d.rightThirdPathElementId);if(!l||!q||!l->valid||!q->valid)return r;
        const auto f=evaluateThirdElement(d.third,l->compressionM,q->compressionM,l->pathVelocityMps,q->pathVelocityMps);
        r.leftGeneralizedForceN+=f.leftForceN*l->dCompressionDWheel;r.rightGeneralizedForceN+=f.rightForceN*q->dCompressionDWheel;
    }
    if(d.hydraulicEnabled)
    {
        const auto* l=suspensionFindElementGeometryV3(ld,lr,d.leftHydraulicPathElementId);const auto* q=suspensionFindElementGeometryV3(rd,rr,d.rightHydraulicPathElementId);if(!l||!q||!l->valid||!q->valid)return r;
        const auto f=stepHydraulicInterconnect(d.hydraulic,s.hydraulic,l->pathVelocityMps,q->pathVelocityMps,dtSeconds);
        r.leftGeneralizedForceN+=f.leftForceN*l->dCompressionDWheel;r.rightGeneralizedForceN+=f.rightForceN*q->dCompressionDWheel;
    }
    if(d.inerterEnabled)
    {
        const auto* l=suspensionFindElementGeometryV3(ld,lr,d.leftInerterPathElementId);const auto* q=suspensionFindElementGeometryV3(rd,rr,d.rightInerterPathElementId);if(!l||!q||!l->valid||!q->valid)return r;
        const double relA=l->pathAccelerationMps2-q->pathAccelerationMps2;
        const double fl=suspClamp(-std::max(0.0,d.inerterKg)*relA,-std::abs(d.maximumInerterForceN),std::abs(d.maximumInerterForceN));
        r.leftGeneralizedForceN+=fl*l->dCompressionDWheel;r.rightGeneralizedForceN-=fl*q->dCompressionDWheel;
    }
    r.valid=true;return r;
}

struct SuspensionVehicleGraphDescriptionV3
{
    static constexpr std::size_t MaxCorners=8,MaxAxles=4;
    std::array<SuspensionCornerGraphDescriptionV3,MaxCorners> corners{};
    std::array<SuspensionAxleGraphDescriptionV3,MaxAxles> axles{};
    std::array<std::array<std::uint8_t,2>,MaxAxles> axleCorners{};
    std::size_t cornerCount=0,axleCount=0;
};
struct SuspensionVehicleGraphStateV3
{
    std::array<SuspensionCornerGraphStateV3,SuspensionVehicleGraphDescriptionV3::MaxCorners> corners{};
    std::array<SuspensionAxleGraphStateV3,SuspensionVehicleGraphDescriptionV3::MaxAxles> axles{};
    std::uint64_t stepCounter=0;
};
struct SuspensionVehicleGraphInputV3
{
    std::array<SuspensionCornerGraphInputV3,SuspensionVehicleGraphDescriptionV3::MaxCorners> corners{};
    std::array<SuspensionCornerGraphControlV3,SuspensionVehicleGraphDescriptionV3::MaxCorners> controls{};
    std::array<SuspensionAxleGraphControlV3,SuspensionVehicleGraphDescriptionV3::MaxAxles> axles{};
};
struct SuspensionVehicleGraphResultV3
{
    std::array<SuspensionCornerGraphResultV3,SuspensionVehicleGraphDescriptionV3::MaxCorners> corners{};
    std::array<SuspensionAxleGraphResultV3,SuspensionVehicleGraphDescriptionV3::MaxAxles> axles{};
    bool valid=false;
};
inline SuspensionVehicleGraphResultV3 stepSuspensionVehicleGraphV3(const SuspensionVehicleGraphDescriptionV3& d,
    SuspensionVehicleGraphStateV3& s,const SuspensionVehicleGraphInputV3& in,double dtSeconds)
{
    SuspensionVehicleGraphResultV3 r;if(d.cornerCount>d.MaxCorners||d.axleCount>d.MaxAxles)return r;
    for(std::size_t c=0;c<d.cornerCount;++c)
    {
        r.corners[c]=stepSuspensionCornerGraphV3(d.corners[c],s.corners[c],in.corners[c],in.controls[c],dtSeconds);
        if(!r.corners[c].valid)return r;
    }
    for(std::size_t a=0;a<d.axleCount;++a)
    {
        const auto l=d.axleCorners[a][0],rr=d.axleCorners[a][1];if(l>=d.cornerCount||rr>=d.cornerCount)return r;
        r.axles[a]=stepSuspensionAxleGraphV3(d.axles[a],s.axles[a],d.corners[l],s.corners[l],r.corners[l],d.corners[rr],s.corners[rr],r.corners[rr],in.axles[a],dtSeconds);
        if(!r.axles[a].valid)return r;
        r.corners[l].generalizedWheelForceN+=r.axles[a].leftGeneralizedForceN;
        r.corners[rr].generalizedWheelForceN+=r.axles[a].rightGeneralizedForceN;
    }
    ++s.stepCounter;r.valid=true;return r;
}

} // namespace heritage::vehicles::suspension

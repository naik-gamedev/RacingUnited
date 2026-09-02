#pragma once
#include "SuspensionElementGraphV3.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace heritage::vehicles::suspension
{

struct SuspensionGeometrySolveRequestV3
{
    double wheelCompressionM=0.0;
    double wheelCompressionVelocityMps=0.0;
    double wheelCompressionAccelerationMps2=0.0;
    double steeringAngleRad=0.0;
    const SuspensionConstraintOverrideSetV3* constraintOverrides=nullptr;
    bool probeOnly=false; // provider must not commit warm-start/runtime state for +/- Jacobian probes
};

using SuspensionFrameSolveFnV3 = bool (*)(const void* description,void* state,
                                          const SuspensionGeometrySolveRequestV3& request,
                                          SuspensionFrameSetV3& frames);

struct SuspensionGeometryProviderV3
{
    const void* description=nullptr;
    void* state=nullptr;
    SuspensionFrameSolveFnV3 solve=nullptr;
};

inline bool suspensionElementLengthV3(const SuspensionFrameSetV3& frames,const SuspensionElementDescriptionV3& e,
                                      double& length,SuspVec3V3* axis=nullptr)
{
    const auto* fa=frames.find(e.a.frameId);const auto* fb=frames.find(e.b.frameId);if(!fa||!fb)return false;
    const SuspVec3V3 pa=suspensionWorldPointV3(frames,e.a),pb=suspensionWorldPointV3(frames,e.b);
    const SuspVec3V3 d=pb-pa;length=suspNormV3(d);if(axis)*axis=suspNormalizedV3(d);return std::isfinite(length);
}

inline bool suspensionElementPathVelocityV3(const SuspensionFrameSetV3& frames,const SuspensionElementDescriptionV3& e,
                                            double& velocity)
{
    double len=0.0;SuspVec3V3 axis{};if(!suspensionElementLengthV3(frames,e,len,&axis))return false;
    const auto va=suspensionWorldPointVelocityV3(frames,e.a),vb=suspensionWorldPointVelocityV3(frames,e.b);
    velocity=suspDotV3(vb-va,axis);return std::isfinite(velocity);
}

struct SuspensionGeometryJacobianOptionsV3
{
    double travelProbeM=1.0e-5;
    double minimumProbeM=1.0e-7;
};

struct SuspensionGeometrySampleSetV3
{
    static constexpr std::size_t Capacity=64;
    std::array<SuspensionElementGeometryV3,Capacity> elements{};
    SuspensionFrameSetV3 currentFrames{};
    bool converged=false;
    bool allReferencedFramesPresent=false;
    bool allDamageConstraintLoadsPresent=false;
    bool constraintOverridesConsumed=false;
};

inline SuspensionGeometrySampleSetV3 evaluateSuspensionGeometryV3(
    const SuspensionElementGraphDescriptionV3& graph,const SuspensionGeometryProviderV3& provider,
    const SuspensionGeometrySolveRequestV3& request,const SuspensionGeometryJacobianOptionsV3& options={})
{
    SuspensionGeometrySampleSetV3 out;
    if(!provider.solve||!provider.description||graph.count>SuspensionGeometrySampleSetV3::Capacity)return out;
    SuspensionFrameSetV3 current{};
    if(!provider.solve(provider.description,provider.state,request,current))return out;

    bool needProbe=false;
    for(std::size_t i=0;i<graph.count;++i)
    {
        const auto* a=current.find(graph.elements[i].a.frameId);const auto* b=current.find(graph.elements[i].b.frameId);
        if(!a||!b){needProbe=true;break;}
        if(!a->hasWheelDerivatives||!b->hasWheelDerivatives){needProbe=true;break;}
    }

    SuspensionFrameSetV3 plus{},minus{};
    const double eps=std::max(options.minimumProbeM,std::abs(options.travelProbeM));
    if(needProbe)
    {
        SuspensionGeometrySolveRequestV3 plusReq=request,minusReq=request;
        plusReq.wheelCompressionM+=eps;minusReq.wheelCompressionM-=eps;plusReq.probeOnly=true;minusReq.probeOnly=true;
        if(!provider.solve(provider.description,provider.state,plusReq,plus)||!provider.solve(provider.description,provider.state,minusReq,minus))return out;
    }

    bool all=true;
    for(std::size_t i=0;i<graph.count;++i)
    {
        auto& g=out.elements[i];const auto& e=graph.elements[i];
        double l=0.0;SuspVec3V3 axis{};
        if(!suspensionElementLengthV3(current,e,l,&axis)){all=false;continue;}
        g.lengthM=l;g.axis=axis;

        SuspVec3V3 da{},db{},dda{},ddb{};
        if(suspensionWorldPointWheelDerivativesV3(current,e.a,da,dda)&&suspensionWorldPointWheelDerivativesV3(current,e.b,db,ddb))
        {
            const SuspVec3V3 pa=suspensionWorldPointV3(current,e.a),pb=suspensionWorldPointV3(current,e.b);
            const SuspVec3V3 rel=pb-pa,drel=db-da,ddrel=ddb-dda;
            const double dl=suspDotV3(rel,drel)/std::max(1.0e-12,l);
            const double ddl=(suspDotV3(drel,drel)+suspDotV3(rel,ddrel)-dl*dl)/std::max(1.0e-12,l);
            g.dLengthDWheel=dl;g.dCompressionDWheel=-dl;
            const auto va=suspensionWorldPointVelocityV3(current,e.a),vb=suspensionWorldPointVelocityV3(current,e.b);
            g.pathVelocityMps=-suspDotV3(vb-va,axis);
            const auto aa=suspensionWorldPointAccelerationV3(current,e.a),ab=suspensionWorldPointAccelerationV3(current,e.b);
            const SuspVec3V3 relV=vb-va,relA=ab-aa;
            const double lengthVelocity=suspDotV3(relV,axis);
            const double lengthAcceleration=(suspDotV3(relV,relV)+suspDotV3(pb-pa,relA)-lengthVelocity*lengthVelocity)/std::max(1.0e-12,l);
            g.pathAccelerationMps2=-lengthAcceleration;
            // ddl is retained through the actual frame acceleration path above; provider acceleration
            // includes q_ddot and d2/dq2*q_dot^2 plus steering/body contributions.
            (void)ddl;
        }
        else
        {
            double lp=0.0,lm=0.0;if(!suspensionElementLengthV3(plus,e,lp)||!suspensionElementLengthV3(minus,e,lm)){all=false;continue;}
            g.dLengthDWheel=(lp-lm)/(2.0*eps);g.dCompressionDWheel=-g.dLengthDWheel;
            const double d2Compression=-(lp-2.0*l+lm)/(eps*eps);
            double v=0.0;if(suspensionElementPathVelocityV3(current,e,v))g.pathVelocityMps=-v;else g.pathVelocityMps=g.dCompressionDWheel*request.wheelCompressionVelocityMps;
            g.pathAccelerationMps2=g.dCompressionDWheel*request.wheelCompressionAccelerationMps2+d2Compression*request.wheelCompressionVelocityMps*request.wheelCompressionVelocityMps;
        }
        const double ref=e.referenceLengthM>0.0?e.referenceLengthM:l;
        g.compressionM=ref-l;
        g.valid=std::isfinite(g.lengthM)&&std::isfinite(g.dCompressionDWheel)&&std::isfinite(g.pathVelocityMps)&&std::isfinite(g.pathAccelerationMps2);
        all=all&&g.valid;
    }
    bool loadsOk=true;
    for(std::size_t i=0;i<graph.count;++i)
        if(graph.elements[i].enabled&&graph.elements[i].damageEnabled&&suspensionElementCarriesConstraintV3(graph.elements[i].kind)&&!current.findConstraintLoad(graph.elements[i].id))
            loadsOk=false;
    out.currentFrames=current;out.allReferencedFramesPresent=all;out.allDamageConstraintLoadsPresent=loadsOk;
    out.constraintOverridesConsumed=current.constraintOverridesConsumed||request.constraintOverrides==nullptr||request.constraintOverrides->count==0;
    out.converged=all&&loadsOk&&out.constraintOverridesConsumed;return out;
}
} // namespace heritage::vehicles::suspension

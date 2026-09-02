#pragma once
#include "SuspensionElementGraphV3.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace heritage::vehicles::suspension
{

inline SuspMat3V3 suspTransposeV3(const SuspMat3V3& a)
{
    SuspMat3V3 r;for(std::size_t i=0;i<3;++i)for(std::size_t j=0;j<3;++j)r.m[i][j]=a.m[j][i];return r;
}
inline SuspMat3V3 suspMatMulV3(const SuspMat3V3& a,const SuspMat3V3& b)
{
    SuspMat3V3 r;r.m={{{{0,0,0}},{{0,0,0}},{{0,0,0}}}};
    for(std::size_t i=0;i<3;++i)for(std::size_t j=0;j<3;++j)for(std::size_t k=0;k<3;++k)r.m[i][j]+=a.m[i][k]*b.m[k][j];
    return r;
}
inline SuspMat3V3 suspRotationVectorV3(SuspVec3V3 rv)
{
    const double a=suspNormV3(rv);if(a<1.0e-14)return {};
    const SuspVec3V3 n=rv*(1.0/a);const double c=std::cos(a),s=std::sin(a),t=1.0-c;
    SuspMat3V3 r;
    r.m={{{{t*n.x*n.x+c,t*n.x*n.y-s*n.z,t*n.x*n.z+s*n.y}},
          {{t*n.x*n.y+s*n.z,t*n.y*n.y+c,t*n.y*n.z-s*n.x}},
          {{t*n.x*n.z-s*n.y,t*n.y*n.z+s*n.x,t*n.z*n.z+c}}}};return r;
}
inline SuspVec3V3 suspInvInertiaWorldMulV3(const SuspMat3V3& orientation,SuspVec3V3 inertiaDiag,SuspVec3V3 v)
{
    const auto rt=suspTransposeV3(orientation);const auto local=suspMulV3(rt,v);
    SuspVec3V3 il{local.x/std::max(1.0e-9,inertiaDiag.x),local.y/std::max(1.0e-9,inertiaDiag.y),local.z/std::max(1.0e-9,inertiaDiag.z)};
    return suspMulV3(orientation,il);
}

struct SuspensionRigidBodyDescriptionV3
{
    std::uint32_t frameId=0;
    double massKg=20.0;
    SuspVec3V3 inertiaKgM2{0.5,0.5,0.5};
    bool fixed=false;
};
struct SuspensionRigidBodyStateV3
{
    SuspVec3V3 position{};
    SuspMat3V3 orientation{};
    SuspVec3V3 linearVelocity{};
    SuspVec3V3 angularVelocity{};
    SuspVec3V3 previousPosition{};
    SuspMat3V3 previousOrientation{};
    bool initialized=false;
};
struct SuspensionBodyExternalLoadV3
{
    std::uint32_t frameId=0;
    SuspVec3V3 forceN{};
    SuspVec3V3 torqueNm{};
};
struct SuspensionDegradedDynamicsDescriptionV3
{
    static constexpr std::size_t MaxBodies=24;
    std::array<SuspensionRigidBodyDescriptionV3,MaxBodies> bodies{};
    std::size_t bodyCount=0;
    SuspensionElementGraphDescriptionV3 graph{};
    SuspVec3V3 gravityMps2{0,-9.80665,0};
    std::uint32_t solverIterations=16;
    double maximumInternalDt=0.0005;
};
struct SuspensionDegradedDynamicsStateV3
{
    std::array<SuspensionRigidBodyStateV3,SuspensionDegradedDynamicsDescriptionV3::MaxBodies> bodies{};
    SuspensionElementGraphStateV3 elements{};
};
struct SuspensionDegradedDynamicsInputV3
{
    std::array<SuspensionBodyExternalLoadV3,24> externalLoads{};
    std::size_t externalLoadCount=0;
    std::array<SuspensionElementControlV3,64> elementControls{};
};
struct SuspensionDegradedDynamicsResultV3
{
    SuspensionFrameSetV3 frames{};
    std::array<SuspensionElementTelemetryV3,64> elements{};
    std::size_t elementCount=0;
    double maximumConstraintErrorM=0.0;
    bool valid=false;
};

inline int suspensionFindBodyV3(const SuspensionDegradedDynamicsDescriptionV3& d,std::uint32_t id)
{
    for(std::size_t i=0;i<d.bodyCount;++i)if(d.bodies[i].frameId==id)return static_cast<int>(i);
    return -1;
}
inline SuspVec3V3 suspensionBodyWorldPointV3(const SuspensionRigidBodyStateV3& b,SuspVec3V3 local){return b.position+suspMulV3(b.orientation,local);}
inline SuspVec3V3 suspensionBodyPointVelocityV3(const SuspensionRigidBodyStateV3& b,SuspVec3V3 local)
{
    const auto r=suspMulV3(b.orientation,local);return b.linearVelocity+suspensionCrossV3(b.angularVelocity,r);
}
inline void suspensionApplyOrientationCorrectionV3(SuspensionRigidBodyStateV3& b,SuspVec3V3 delta)
{
    b.orientation=suspMatMulV3(suspRotationVectorV3(delta),b.orientation);
}

inline SuspensionDegradedDynamicsResultV3 stepSuspensionDegradedDynamicsV3(const SuspensionDegradedDynamicsDescriptionV3& d,
    SuspensionDegradedDynamicsStateV3& s,const SuspensionDegradedDynamicsInputV3& in,double dtSeconds)
{
    SuspensionDegradedDynamicsResultV3 out;
    if(d.bodyCount==0||d.bodyCount>d.MaxBodies||!validateSuspensionElementGraphV3(d.graph))return out;
    const double dt=std::max(0.0,dtSeconds);if(dt<=0.0)return out;
    const int substeps=std::max(1,std::min(32,static_cast<int>(std::ceil(dt/std::max(1.0e-6,d.maximumInternalDt)))));const double h=dt/substeps;
    for(std::size_t i=0;i<d.bodyCount;++i)
    {
        if(!s.bodies[i].initialized){s.bodies[i].initialized=true;s.bodies[i].previousPosition=s.bodies[i].position;s.bodies[i].previousOrientation=s.bodies[i].orientation;}
    }

    for(int sub=0;sub<substeps;++sub)
    {
        std::array<SuspVec3V3,24> forces{},torques{};
        for(std::size_t i=0;i<d.bodyCount;++i)if(!d.bodies[i].fixed)forces[i]=d.gravityMps2*d.bodies[i].massKg;
        for(std::size_t k=0;k<in.externalLoadCount&&k<in.externalLoads.size();++k)
        {
            const int bi=suspensionFindBodyV3(d,in.externalLoads[k].frameId);if(bi>=0){forces[bi]=forces[bi]+in.externalLoads[k].forceN;torques[bi]=torques[bi]+in.externalLoads[k].torqueNm;}
        }

        // Evaluate all axial force elements directly in world space. No wheel-motion-ratio abstraction is used in degraded mode.
        for(std::size_t ei=0;ei<d.graph.count;++ei)
        {
            const auto& ed=d.graph.elements[ei];if(!ed.enabled||!suspensionElementCarriesForceV3(ed.kind))continue;
            const int ia=suspensionFindBodyV3(d,ed.a.frameId),ib=suspensionFindBodyV3(d,ed.b.frameId);if(ia<0||ib<0)continue;
            const auto pa=suspensionBodyWorldPointV3(s.bodies[ia],ed.a.localPoint),pb=suspensionBodyWorldPointV3(s.bodies[ib],ed.b.localPoint);const auto rel=pb-pa;const double len=suspNormV3(rel);const auto axis=suspNormalizedV3(rel);
            const auto va=suspensionBodyPointVelocityV3(s.bodies[ia],ed.a.localPoint),vb=suspensionBodyPointVelocityV3(s.bodies[ib],ed.b.localPoint);const double lenV=suspDotV3(vb-va,axis);
            SuspensionElementGeometryV3 g;g.valid=len>1.0e-12;g.lengthM=len;g.compressionM=(ed.referenceLengthM>0?ed.referenceLengthM:len)-len;g.pathVelocityMps=-lenV;g.pathAccelerationMps2=(g.pathVelocityMps-s.elements.elements[ei].previousPathVelocityMps)/h;g.dCompressionDWheel=0;
            SuspVec6 noStructural{};const auto er=stepSuspensionElementV3(ed,s.elements.elements[ei],g,in.elementControls[ei],noStructural,h);out.elements[ei]=er.telemetry;
            const double f=er.telemetry.localForceN;const auto forceOnB=axis*f,forceOnA=forceOnB*(-1.0);forces[ia]=forces[ia]+forceOnA;forces[ib]=forces[ib]+forceOnB;
            const auto ra=pa-s.bodies[ia].position,rb=pb-s.bodies[ib].position;torques[ia]=torques[ia]+suspensionCrossV3(ra,forceOnA);torques[ib]=torques[ib]+suspensionCrossV3(rb,forceOnB);
        }

        // Predict rigid bodies.
        for(std::size_t i=0;i<d.bodyCount;++i)
        {
            auto& bs=s.bodies[i];const auto& bd=d.bodies[i];bs.previousPosition=bs.position;bs.previousOrientation=bs.orientation;if(bd.fixed)continue;
            const double invM=1.0/std::max(1.0e-9,bd.massKg);bs.linearVelocity=bs.linearVelocity+forces[i]*(invM*h);bs.angularVelocity=bs.angularVelocity+suspInvInertiaWorldMulV3(bs.orientation,bd.inertiaKgM2,torques[i])*h;
            bs.position=bs.position+bs.linearVelocity*h;suspensionApplyOrientationCorrectionV3(bs,bs.angularVelocity*h);
        }

        // XPBD-style surviving physical constraints. Broken/detached elements are simply absent.
        double maxError=0.0;
        for(std::uint32_t iteration=0;iteration<std::max(1u,d.solverIterations);++iteration)
        {
            for(std::size_t ei=0;ei<d.graph.count;++ei)
            {
                const auto& ed=d.graph.elements[ei];auto& es=s.elements.elements[ei];if(!ed.enabled||!suspensionElementCarriesConstraintV3(ed.kind)||!es.constraintEnabled)continue;
                const int ia=suspensionFindBodyV3(d,ed.a.frameId),ib=suspensionFindBodyV3(d,ed.b.frameId);if(ia<0||ib<0)continue;
                auto& a=s.bodies[ia];auto& b=s.bodies[ib];const auto& ad=d.bodies[ia];const auto& bd=d.bodies[ib];
                const auto pa=suspensionBodyWorldPointV3(a,ed.a.localPoint),pb=suspensionBodyWorldPointV3(b,ed.b.localPoint);const auto delta=pb-pa;const double dist=suspNormV3(delta);
                const double target=(ed.kind==SuspensionElementKindV3::StructuralLink||ed.kind==SuspensionElementKindV3::AntiRollDropLink)?std::max(0.0,ed.referenceLengthM):0.0;
                const double C=dist-target;maxError=std::max(maxError,std::abs(C));if(dist<1.0e-12&&std::abs(C)<1.0e-12)continue;const auto n=dist>1.0e-12?delta*(1.0/dist):SuspVec3V3{1,0,0};
                const auto ra=pa-a.position,rb=pb-b.position;const auto ran=suspensionCrossV3(ra,n),rbn=suspensionCrossV3(rb,n);
                const double invMa=ad.fixed?0.0:1.0/std::max(1.0e-9,ad.massKg),invMb=bd.fixed?0.0:1.0/std::max(1.0e-9,bd.massKg);
                const auto iRan=ad.fixed?SuspVec3V3{}:suspInvInertiaWorldMulV3(a.orientation,ad.inertiaKgM2,ran);const auto iRbn=bd.fixed?SuspVec3V3{}:suspInvInertiaWorldMulV3(b.orientation,bd.inertiaKgM2,rbn);
                const double w=invMa+invMb+suspDotV3(ran,iRan)+suspDotV3(rbn,iRbn);if(w<1.0e-15)continue;
                double compliance=0.0;if(ed.complianceEnabled){double k=0;for(std::size_t kx=0;kx<3;++kx)k+=std::max(0.0,ed.compliance.stiffness[kx][kx]);k/=3.0;if(k>1.0)compliance=1.0/k;}
                const double lambda=-C/(w+compliance/(h*h));const auto impulse=n*lambda;
                if(!ad.fixed){a.position=a.position-impulse*invMa;suspensionApplyOrientationCorrectionV3(a,suspInvInertiaWorldMulV3(a.orientation,ad.inertiaKgM2,suspensionCrossV3(ra,impulse*(-1.0))));}
                if(!bd.fixed){b.position=b.position+impulse*invMb;suspensionApplyOrientationCorrectionV3(b,suspInvInertiaWorldMulV3(b.orientation,bd.inertiaKgM2,suspensionCrossV3(rb,impulse)));}
                // Constraint reaction drives this component's own fatigue/failure state.
                SuspVec6 load{};load[0]=std::abs(lambda)/(h*h);es.lastForceN=load[0];
                if(ed.damageEnabled)
                {
                    auto damage=stepSuspensionDamageV2(ed.damage,es.damage,load[0],std::abs(C),es.damper.temperatureC,h);
                    const bool broken=(es.damage.flags&DamageBrokenV2)!=0u;es.constraintEnabled=damage.constraintEnabled&&!broken;
                }
            }
        }
        out.maximumConstraintErrorM=std::max(out.maximumConstraintErrorM,maxError);

        // Velocity consistency after projection.
        for(std::size_t i=0;i<d.bodyCount;++i)if(!d.bodies[i].fixed)s.bodies[i].linearVelocity=(s.bodies[i].position-s.bodies[i].previousPosition)*(1.0/h);
    }

    out.frames.count=d.bodyCount;out.frames.constraintOverridesConsumed=true;out.elementCount=d.graph.count;
    for(std::size_t i=0;i<d.bodyCount;++i){auto& f=out.frames.frames[i];const auto& b=s.bodies[i];f.id=d.bodies[i].frameId;f.position=b.position;f.orientation=b.orientation;f.linearVelocity=b.linearVelocity;f.angularVelocity=b.angularVelocity;f.valid=true;}
    for(std::size_t ei=0;ei<d.graph.count&&out.frames.constraintLoadCount<out.frames.constraintLoads.size();++ei)if(suspensionElementCarriesConstraintV3(d.graph.elements[ei].kind))
    {auto& l=out.frames.constraintLoads[out.frames.constraintLoadCount++];l.elementId=d.graph.elements[ei].id;l.generalizedLoad[0]=std::abs(s.elements.elements[ei].lastForceN);l.valid=true;}
    out.valid=true;return out;
}

inline bool suspensionRequiresDegradedDynamicsV3(const SuspensionElementGraphDescriptionV3& d,const SuspensionElementGraphStateV3& s)
{
    for(std::size_t i=0;i<d.count;++i)if(suspensionElementCarriesConstraintV3(d.elements[i].kind)&&!s.elements[i].constraintEnabled)return true;
    return false;
}

} // namespace heritage::vehicles::suspension

#include "LeafSpringLiveAxle.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kEpsilon = 1.0e-6f;
constexpr float kDerivativeStepM = 0.0005f;

heritage::math::Vec3 add(const heritage::math::Vec3& a, const heritage::math::Vec3& b)
{ return {a.x+b.x,a.y+b.y,a.z+b.z}; }
heritage::math::Vec3 sub(const heritage::math::Vec3& a, const heritage::math::Vec3& b)
{ return {a.x-b.x,a.y-b.y,a.z-b.z}; }
heritage::math::Vec3 scale(const heritage::math::Vec3& v,float s)
{ return {v.x*s,v.y*s,v.z*s}; }
float dot(const heritage::math::Vec3& a,const heritage::math::Vec3& b)
{ return a.x*b.x+a.y*b.y+a.z*b.z; }
float len(const heritage::math::Vec3& v){ return std::sqrt(dot(v,v)); }
bool finite(const heritage::math::Vec3& v)
{ return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
heritage::math::Vec3 rotateX(const heritage::math::Vec3& v,float a)
{
    const float c=std::cos(a),s=std::sin(a);
    return {v.x, v.y*c-v.z*s, v.y*s+v.z*c};
}
heritage::math::Vec3 rotateZ(const heritage::math::Vec3& v,float a)
{
    const float c=std::cos(a),s=std::sin(a);
    return {v.x*c-v.y*s, v.x*s+v.y*c, v.z};
}

heritage::math::Vec3 axlePoint(
    const LiveAxleHardpoints& axle,
    const LiveAxleKinematicsOutput& pose,
    const heritage::math::Vec3& reference)
{
    return add(pose.localAxleCenter,
        rotateZ(sub(reference,axle.axleCenter),pose.axleRollRadians));
}

float signedCamberYZ(
    const heritage::math::Vec3& front,
    const heritage::math::Vec3& rear,
    const heritage::math::Vec3& clamp)
{
    const float dy=rear.y-front.y;
    const float dz=rear.z-front.z;
    const float lengthYZ=std::sqrt(dy*dy+dz*dz);
    if(lengthYZ<kEpsilon) return 0.0f;
    // Signed perpendicular distance of the axle clamp from the eye-to-eye chord.
    return (dz*(clamp.y-front.y)-dy*(clamp.z-front.z))/lengthYZ;
}

struct LeafSideState
{
    bool valid=false;
    float compression=0.0f;
    float shackleAngle=0.0f;
    heritage::math::Vec3 rearEye{};
    heritage::math::Vec3 clamp{};
};

bool circleIntersectionsYZ(
    const heritage::math::Vec3& c0,float r0,
    const heritage::math::Vec3& c1,float r1,
    heritage::math::Vec3& a,heritage::math::Vec3& b)
{
    const float dy=c1.y-c0.y,dz=c1.z-c0.z;
    const float d=std::sqrt(dy*dy+dz*dz);
    if(d<kEpsilon || d>r0+r1+1.0e-5f || d<std::abs(r0-r1)-1.0e-5f)
        return false;
    const float x=(r0*r0-r1*r1+d*d)/(2.0f*d);
    const float h2=std::max(r0*r0-x*x,0.0f);
    const float h=std::sqrt(h2);
    const float uy=dy/d,uz=dz/d;
    const float py=c0.y+x*uy,pz=c0.z+x*uz;
    a={c0.x,py-h*uz,pz+h*uy};
    b={c0.x,py+h*uz,pz-h*uy};
    return true;
}

LeafSideState solveSide(
    const LeafSpringLiveAxleHardpoints& h,
    const LiveAxleKinematicsOutput& axlePose,
    bool left)
{
    LeafSideState out;
    const auto& front=left?h.leftLeafFrontEye:h.rightLeafFrontEye;
    const auto& pivot=left?h.leftLeafRearShacklePivot:h.rightLeafRearShacklePivot;
    const auto& restRear=left?h.leftLeafRearEye:h.rightLeafRearEye;
    const auto& restClamp=left?h.leftLeafAxleClamp:h.rightLeafAxleClamp;
    out.clamp=axlePoint(h.axle,axlePose,restClamp);

    const float shackleLength=len(sub(restRear,pivot));
    const float rearLeafSegment=len(sub(restRear,restClamp));
    heritage::math::Vec3 candidateA{},candidateB{};
    if(!circleIntersectionsYZ(pivot,shackleLength,out.clamp,rearLeafSegment,candidateA,candidateB))
        return out;
    // Leaf/shackle motion stays in each side's longitudinal plane.
    candidateA.x=restRear.x;
    candidateB.x=restRear.x;
    out.rearEye=len(sub(candidateA,restRear))<=len(sub(candidateB,restRear))?candidateA:candidateB;

    const float restCamber=signedCamberYZ(front,restRear,restClamp);
    const float currentCamber=signedCamberYZ(front,out.rearEye,out.clamp);
    const float flattenSign=restCamber<=0.0f?1.0f:-1.0f;
    out.compression=(currentCamber-restCamber)*flattenSign;

    const auto restShackle=sub(restRear,pivot);
    const auto liveShackle=sub(out.rearEye,pivot);
    const float restAngle=std::atan2(restShackle.y,restShackle.z);
    const float liveAngle=std::atan2(liveShackle.y,liveShackle.z);
    out.shackleAngle=liveAngle-restAngle;
    out.valid=finite(out.rearEye)&&finite(out.clamp)
        && std::isfinite(out.compression)&&std::isfinite(out.shackleAngle);
    return out;
}

} // namespace

bool validLeafSpringLiveAxleHardpoints(const LeafSpringLiveAxleHardpoints& h)
{
    if(!h.authored || !validLiveAxleHardpoints(h.axle)) return false;
    const heritage::math::Vec3 points[]={
        h.leftLeafFrontEye,h.leftLeafRearShacklePivot,h.leftLeafRearEye,h.leftLeafAxleClamp,
        h.rightLeafFrontEye,h.rightLeafRearShacklePivot,h.rightLeafRearEye,h.rightLeafAxleClamp};
    for(const auto& p:points) if(!finite(p)) return false;
    const auto validSide=[](const heritage::math::Vec3& front,
        const heritage::math::Vec3& pivot,const heritage::math::Vec3& rear,
        const heritage::math::Vec3& clamp)
    {
        return len(sub(rear,pivot))>0.04f
            && len(sub(rear,clamp))>0.10f
            && len(sub(front,clamp))>0.10f
            && len(sub(rear,front))>0.30f
            && std::abs(signedCamberYZ(front,rear,clamp))>0.002f;
    };
    return validSide(h.leftLeafFrontEye,h.leftLeafRearShacklePivot,
            h.leftLeafRearEye,h.leftLeafAxleClamp)
        && validSide(h.rightLeafFrontEye,h.rightLeafRearShacklePivot,
            h.rightLeafRearEye,h.rightLeafAxleClamp);
}

LeafSpringLiveAxleOutput evaluateLeafSpringLiveAxle(
    const LeafSpringLiveAxleHardpoints& h,
    const LeafSpringLiveAxleInput& input)
{
    LeafSpringLiveAxleOutput out;
    if(!validLeafSpringLiveAxleHardpoints(h)) return out;
    out.axle=evaluateLiveAxleKinematics(h.axle,
        {input.leftCompressionM,input.rightCompressionM,input.evaluateLeftWheel,
         input.steeringDegrees,input.staticCamberDegrees,input.staticToeDegrees});
    if(!out.axle.valid) return out;
    const LeafSideState current=solveSide(h,out.axle,input.evaluateLeftWheel);
    if(!current.valid) return out;

    LeafSpringLiveAxleInput derivative=input;
    if(input.evaluateLeftWheel) derivative.leftCompressionM+=kDerivativeStepM;
    else derivative.rightCompressionM+=kDerivativeStepM;
    const auto derivativeAxle=evaluateLiveAxleKinematics(h.axle,
        {derivative.leftCompressionM,derivative.rightCompressionM,derivative.evaluateLeftWheel,
         derivative.steeringDegrees,derivative.staticCamberDegrees,derivative.staticToeDegrees});
    if(!derivativeAxle.valid) return out;
    const LeafSideState plus=solveSide(h,derivativeAxle,input.evaluateLeftWheel);
    if(!plus.valid) return out;

    out.leafCompressionM=current.compression;
    out.leafMotionRatio=(plus.compression-current.compression)/kDerivativeStepM;
    out.shackleTravelRadians=current.shackleAngle;
    out.shackleAngleRadians=current.shackleAngle;
    out.localLeafRearEye=current.rearEye;
    out.localLeafAxleClamp=current.clamp;
    out.valid=std::isfinite(out.leafMotionRatio)
        && std::abs(out.leafMotionRatio)>=0.02f
        && std::abs(out.leafMotionRatio)<=8.0f;
    return out;
}

} // namespace heritage::vehicles

#include "KartChassisKinematics.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kEpsilon = 1.0e-6f;
constexpr float kMaximumIgnoredTravelM = 0.010f;

float radians(float degreesValue) { return degreesValue * (kPi / 180.0f); }
float degrees(float radiansValue) { return radiansValue * (180.0f / kPi); }

bool finite(const heritage::math::Vec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
heritage::math::Vec3 add(const heritage::math::Vec3& a, const heritage::math::Vec3& b)
{ return { a.x + b.x, a.y + b.y, a.z + b.z }; }
heritage::math::Vec3 subtract(const heritage::math::Vec3& a, const heritage::math::Vec3& b)
{ return { a.x - b.x, a.y - b.y, a.z - b.z }; }
heritage::math::Vec3 scale(const heritage::math::Vec3& v, float s)
{ return { v.x * s, v.y * s, v.z * s }; }
float dot(const heritage::math::Vec3& a, const heritage::math::Vec3& b)
{ return a.x*b.x + a.y*b.y + a.z*b.z; }
heritage::math::Vec3 cross(const heritage::math::Vec3& a, const heritage::math::Vec3& b)
{ return { a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x }; }
float length(const heritage::math::Vec3& v) { return std::sqrt(std::max(dot(v,v), 0.0f)); }
heritage::math::Vec3 normalized(const heritage::math::Vec3& v, const heritage::math::Vec3& fallback)
{
    const float m = length(v);
    return m > kEpsilon ? scale(v, 1.0f/m) : fallback;
}
heritage::math::Vec3 rotateAroundAxis(const heritage::math::Vec3& v, const heritage::math::Vec3& axis, float angle)
{
    const float c=std::cos(angle), s=std::sin(angle);
    return add(add(scale(v,c),scale(cross(axis,v),s)),scale(axis,dot(axis,v)*(1.0f-c)));
}
heritage::math::Vec3 rotatePointAroundLine(
    const heritage::math::Vec3& p,
    const heritage::math::Vec3& linePoint,
    const heritage::math::Vec3& axis,
    float angle)
{
    return add(linePoint, rotateAroundAxis(subtract(p,linePoint),axis,angle));
}

heritage::math::Vec3 eulerDegreesFromBasis(
    const heritage::math::Vec3& right,
    const heritage::math::Vec3& up,
    const heritage::math::Vec3& forward)
{
    const float y=std::asin(std::clamp(-right.z,-1.0f,1.0f));
    const float cy=std::cos(y);
    float x=0.0f,z=0.0f;
    if(std::abs(cy)>kEpsilon){x=std::atan2(up.z,forward.z);z=std::atan2(right.y,right.x);}
    else{x=std::atan2(-forward.y,up.y);}
    return {degrees(x),degrees(y),degrees(z)};
}
float horizontalToeDegrees(const heritage::math::Vec3& forward)
{ return degrees(std::atan2(forward.x,forward.z)); }
float camberDegreesFromRight(const heritage::math::Vec3& right)
{ return degrees(std::atan2(right.y,right.x)); }

float distanceSquared(const heritage::math::Vec3& a, const heritage::math::Vec3& b)
{
    const auto d=subtract(a,b);
    return dot(d,d);
}

float pointLineDistance(
    const heritage::math::Vec3& p,
    const heritage::math::Vec3& linePoint,
    const heritage::math::Vec3& unitAxis)
{
    const auto delta=subtract(p,linePoint);
    return length(subtract(delta,scale(unitAxis,dot(delta,unitAxis))));
}

bool validKingpin(
    const heritage::math::Vec3& upper,
    const heritage::math::Vec3& lower,
    const heritage::math::Vec3& wheelCenter)
{
    if(!finite(upper)||!finite(lower)||!finite(wheelCenter)) return false;
    const auto axisVector=subtract(upper,lower);
    const float axisLength=length(axisVector);
    if(axisLength<0.08f||axisLength>0.80f) return false;
    const auto axis=scale(axisVector,1.0f/axisLength);
    if(std::abs(axis.y)<0.45f) return false;
    const float radial=pointLineDistance(wheelCenter,lower,axis);
    return radial>0.015f&&radial<0.45f;
}

KartWheelRole selectRole(const KartChassisHardpoints& h, const heritage::math::Vec3& hint)
{
    struct Candidate { KartWheelRole role; heritage::math::Vec3 center; };
    const Candidate candidates[] = {
        {KartWheelRole::FrontLeft,h.frontLeftWheelCenter},
        {KartWheelRole::FrontRight,h.frontRightWheelCenter},
        {KartWheelRole::RearLeft,h.rearLeftWheelCenter},
        {KartWheelRole::RearRight,h.rearRightWheelCenter}
    };
    float best=1.0e30f;
    KartWheelRole role=KartWheelRole::Unknown;
    for(const Candidate& c:candidates)
    {
        const float d=distanceSquared(hint,c.center);
        if(d<best){best=d;role=c.role;}
    }
    return best<0.25f*0.25f?role:KartWheelRole::Unknown;
}

void applyStaticAlignment(
    float staticCamberDegrees,
    float staticToeDegrees,
    heritage::math::Vec3& right,
    heritage::math::Vec3& up,
    heritage::math::Vec3& forward)
{
    const float toe=radians(staticToeDegrees);
    const float camber=radians(staticCamberDegrees);
    forward=rotateAroundAxis(forward,{0.0f,1.0f,0.0f},toe);
    right=rotateAroundAxis(right,{0.0f,1.0f,0.0f},toe);
    right=rotateAroundAxis(right,forward,camber);
    up=rotateAroundAxis(up,forward,camber);
}

} // namespace

bool validKartChassisHardpoints(const KartChassisHardpoints& h)
{
    if(!h.authored) return false;
    if(!validKingpin(h.frontLeftKingpinUpper,h.frontLeftKingpinLower,h.frontLeftWheelCenter)
        || !validKingpin(h.frontRightKingpinUpper,h.frontRightKingpinLower,h.frontRightWheelCenter))
        return false;
    if(!finite(h.rearAxleBearingLeft)||!finite(h.rearAxleBearingRight)
        ||!finite(h.rearLeftWheelCenter)||!finite(h.rearRightWheelCenter))
        return false;
    const auto axleVector=subtract(h.rearAxleBearingRight,h.rearAxleBearingLeft);
    const float axleLength=length(axleVector);
    if(axleLength<0.25f||axleLength>2.0f) return false;
    const auto axleAxis=scale(axleVector,1.0f/axleLength);
    const float rearTrack=length(subtract(h.rearRightWheelCenter,h.rearLeftWheelCenter));
    if(rearTrack<0.45f||rearTrack>2.0f) return false;
    if(pointLineDistance(h.rearLeftWheelCenter,h.rearAxleBearingLeft,axleAxis)>0.02f
        || pointLineDistance(h.rearRightWheelCenter,h.rearAxleBearingLeft,axleAxis)>0.02f)
        return false;
    if(!(h.frontLeftWheelCenter.x<0.0f&&h.frontRightWheelCenter.x>0.0f
        &&h.rearLeftWheelCenter.x<0.0f&&h.rearRightWheelCenter.x>0.0f))
        return false;
    return true;
}

KartChassisKinematicsOutput evaluateKartChassisKinematics(
    const KartChassisHardpoints& h,
    const KartChassisKinematicsInput& input)
{
    KartChassisKinematicsOutput out;
    if(!validKartChassisHardpoints(h)||!std::isfinite(input.compressionM)
        ||!std::isfinite(input.steeringDegrees)||!finite(input.referenceWheelCenterHint))
        return out;

    out.role=selectRole(h,input.referenceWheelCenterHint);
    if(out.role==KartWheelRole::Unknown) return out;
    out.travelClamped=std::abs(input.compressionM)>kMaximumIgnoredTravelM;

    heritage::math::Vec3 right{1.0f,0.0f,0.0f};
    heritage::math::Vec3 up{0.0f,1.0f,0.0f};
    heritage::math::Vec3 forward{0.0f,0.0f,1.0f};
    applyStaticAlignment(input.staticCamberDegrees,input.staticToeDegrees,right,up,forward);

    if(out.role==KartWheelRole::FrontLeft||out.role==KartWheelRole::FrontRight)
    {
        const bool left=out.role==KartWheelRole::FrontLeft;
        const auto upper=left?h.frontLeftKingpinUpper:h.frontRightKingpinUpper;
        const auto lower=left?h.frontLeftKingpinLower:h.frontRightKingpinLower;
        const auto center=left?h.frontLeftWheelCenter:h.frontRightWheelCenter;
        const auto axis=normalized(subtract(upper,lower),{0.0f,1.0f,0.0f});
        const float steer=radians(input.steeringDegrees);
        out.referenceWheelCenter=center;
        out.localWheelCenter=rotatePointAroundLine(center,lower,axis,steer);
        out.localSteeringAxis=axis;
        out.localSteeringAxisPoint=lower;
        out.steeringJackingM=out.localWheelCenter.y-center.y;
        out.kingpinRadialOffsetM=pointLineDistance(center,lower,axis);
        out.casterDegrees=degrees(std::atan2(-axis.z,std::max(std::abs(axis.y),kEpsilon)));
        out.kingpinInclinationDegrees=degrees(std::atan2(-axis.x,std::max(std::abs(axis.y),kEpsilon)));
        forward=normalized(rotateAroundAxis(forward,axis,steer),{0.0f,0.0f,1.0f});
        right=normalized(rotateAroundAxis(right,axis,steer),{1.0f,0.0f,0.0f});
        up=normalized(cross(forward,right),{0.0f,1.0f,0.0f});
        right=normalized(cross(up,forward),{1.0f,0.0f,0.0f});
    }
    else
    {
        const bool left=out.role==KartWheelRole::RearLeft;
        out.referenceWheelCenter=left?h.rearLeftWheelCenter:h.rearRightWheelCenter;
        out.localWheelCenter=out.referenceWheelCenter;
        out.localSteeringAxis=normalized(
            subtract(h.rearAxleBearingRight,h.rearAxleBearingLeft),{1.0f,0.0f,0.0f});
        out.localSteeringAxisPoint=left?h.rearAxleBearingLeft:h.rearAxleBearingRight;
        out.steeringJackingM=0.0f;
        out.kingpinRadialOffsetM=0.0f;
        // A kart's rear axle is rigid in camber/toe. Static authoring is still
        // honored for unusual historic/experimental layouts, but no travel or
        // steering kinematics can create independent rear-wheel motion.
    }

    out.camberDegrees=camberDegreesFromRight(right);
    out.toeDegrees=horizontalToeDegrees(forward);
    out.localWheelForward=forward;
    out.localWheelRight=right;
    out.localWheelUp=up;
    out.localUprightRotationDegrees=eulerDegreesFromBasis(right,up,forward);
    out.valid=true;
    return out;
}

} // namespace heritage::vehicles

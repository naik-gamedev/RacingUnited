#include "MotorcycleForkKinematics.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kEpsilon = 1.0e-6f;
constexpr float kMinimumStemLengthM = 0.04f;
constexpr float kMaximumForkTravelM = 0.40f;

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
heritage::math::Vec3 rotatePointAroundLine(const heritage::math::Vec3& p, const heritage::math::Vec3& linePoint, const heritage::math::Vec3& axis, float angle)
{ return add(linePoint, rotateAroundAxis(subtract(p,linePoint),axis,angle)); }

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

} // namespace

bool validMotorcycleForkHardpoints(const MotorcycleForkHardpoints& h)
{
    if(!h.authored || !finite(h.steeringStemUpper) || !finite(h.steeringStemLower) || !finite(h.wheelCenter))
        return false;
    const heritage::math::Vec3 stem=subtract(h.steeringStemLower,h.steeringStemUpper);
    const float stemLength=length(stem);
    if(stemLength<kMinimumStemLengthM || stemLength>2.0f) return false;
    const heritage::math::Vec3 axis=scale(stem,1.0f/stemLength);
    const heritage::math::Vec3 radial=subtract(subtract(h.wheelCenter,h.steeringStemLower),scale(axis,dot(subtract(h.wheelCenter,h.steeringStemLower),axis)));
    const float offset=length(radial);
    return offset>0.005f && offset<1.0f;
}

MotorcycleForkKinematicsOutput evaluateMotorcycleForkKinematics(
    const MotorcycleForkHardpoints& h,
    const MotorcycleForkKinematicsInput& input)
{
    MotorcycleForkKinematicsOutput out;
    if(!validMotorcycleForkHardpoints(h) || !std::isfinite(input.compressionM)
        || !std::isfinite(input.steeringDegrees) || !finite(input.suspensionDirection))
        return out;

    heritage::math::Vec3 axis=normalized(subtract(h.steeringStemLower,h.steeringStemUpper),{0.0f,-1.0f,0.0f});
    const heritage::math::Vec3 authoredDirection=normalized(input.suspensionDirection,{0.0f,-1.0f,0.0f});
    if(dot(axis,authoredDirection)<0.0f) axis=scale(axis,-1.0f);
    const float compression=std::clamp(input.compressionM,-kMaximumForkTravelM,kMaximumForkTravelM);
    out.travelClamped=std::abs(compression-input.compressionM)>1.0e-6f;

    // Positive wheel compression moves the axle opposite the fork's extension
    // direction. Steering then rotates the complete sliding axle/fork lower.
    const heritage::math::Vec3 slidCenter=add(h.wheelCenter,scale(axis,-compression));
    const float steer=radians(input.steeringDegrees);
    out.localWheelCenter=rotatePointAroundLine(slidCenter,h.steeringStemLower,axis,steer);

    const float toe=radians(input.staticToeDegrees);
    const float camber=radians(input.staticCamberDegrees);
    heritage::math::Vec3 forward=rotateAroundAxis({0.0f,0.0f,1.0f},{0.0f,1.0f,0.0f},toe);
    heritage::math::Vec3 right=rotateAroundAxis({1.0f,0.0f,0.0f},{0.0f,1.0f,0.0f},toe);
    heritage::math::Vec3 up{0.0f,1.0f,0.0f};
    right=rotateAroundAxis(right,forward,camber);
    up=rotateAroundAxis(up,forward,camber);
    forward=normalized(rotateAroundAxis(forward,axis,steer),{0.0f,0.0f,1.0f});
    right=normalized(rotateAroundAxis(right,axis,steer),{1.0f,0.0f,0.0f});
    up=normalized(cross(forward,right),{0.0f,1.0f,0.0f});
    right=normalized(cross(up,forward),{1.0f,0.0f,0.0f});

    out.valid=true;
    out.forkCompressionM=compression;
    out.springCompressionM=compression;
    out.damperCompressionM=compression;
    out.springMotionRatio=1.0f;
    out.damperMotionRatio=1.0f;
    out.rakeDegreesFromVertical=degrees(std::acos(std::clamp(std::abs(axis.y),0.0f,1.0f)));
    out.wheelbaseDeltaM=out.localWheelCenter.z-h.wheelCenter.z;
    out.camberDegrees=camberDegreesFromRight(right);
    out.toeDegrees=horizontalToeDegrees(forward);
    out.localSteeringAxis=axis;
    out.localSteeringAxisPoint=h.steeringStemLower;
    out.localWheelForward=forward;
    out.localWheelRight=right;
    out.localWheelUp=up;
    out.localUprightRotationDegrees=eulerDegreesFromBasis(right,up,forward);
    return out;
}

} // namespace heritage::vehicles

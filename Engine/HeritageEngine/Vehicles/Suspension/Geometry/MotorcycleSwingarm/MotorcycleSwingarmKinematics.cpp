#include "MotorcycleSwingarmKinematics.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {

constexpr float kPi=3.14159265358979323846f;
constexpr float kEpsilon=1.0e-6f;
constexpr float kMinimumGeometryLength=0.005f;
constexpr float kDerivativeStepM=0.0005f;

float degrees(float r){return r*(180.0f/kPi);}
bool finite(float v){return std::isfinite(v);}
bool finite(const heritage::math::Vec3& v){return finite(v.x)&&finite(v.y)&&finite(v.z);}
heritage::math::Vec3 add(const heritage::math::Vec3&a,const heritage::math::Vec3&b){return{a.x+b.x,a.y+b.y,a.z+b.z};}
heritage::math::Vec3 subtract(const heritage::math::Vec3&a,const heritage::math::Vec3&b){return{a.x-b.x,a.y-b.y,a.z-b.z};}
heritage::math::Vec3 scale(const heritage::math::Vec3&v,float s){return{v.x*s,v.y*s,v.z*s};}
float dot(const heritage::math::Vec3&a,const heritage::math::Vec3&b){return a.x*b.x+a.y*b.y+a.z*b.z;}
heritage::math::Vec3 cross(const heritage::math::Vec3&a,const heritage::math::Vec3&b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float length(const heritage::math::Vec3&v){return std::sqrt(std::max(dot(v,v),0.0f));}
heritage::math::Vec3 normalized(const heritage::math::Vec3&v,const heritage::math::Vec3&fallback){const float m=length(v);return m>kEpsilon?scale(v,1.0f/m):fallback;}
heritage::math::Vec3 rotateAroundAxis(const heritage::math::Vec3&v,const heritage::math::Vec3&a,float q){const float c=std::cos(q),s=std::sin(q);return add(add(scale(v,c),scale(cross(a,v),s)),scale(a,dot(a,v)*(1.0f-c)));}
heritage::math::Vec3 rotatePointAroundLine(const heritage::math::Vec3&p,const heritage::math::Vec3&lp,const heritage::math::Vec3&a,float q){return add(lp,rotateAroundAxis(subtract(p,lp),a,q));}
float wrapPi(float q){while(q>kPi)q-=2*kPi;while(q<-kPi)q+=2*kPi;return q;}

bool solveNearestZeroTrig(float a,float b,float target,float& angle,bool& clamped)
{
    const float mag=std::sqrt(a*a+b*b); if(mag<=kEpsilon)return false;
    float n=target/mag; clamped=n<-1.0f||n>1.0f; n=std::clamp(n,-1.0f,1.0f);
    const float phase=std::atan2(b,a), off=std::acos(n);
    const float p=wrapPi(phase+off), m=wrapPi(phase-off);
    angle=std::abs(p)<=std::abs(m)?p:m; return finite(angle);
}

struct Pose
{
    bool valid=false, clamped=false;
    float armAngle=0.0f, rockerAngle=0.0f;
    heritage::math::Vec3 wheelCenter{}, linkSwing{}, rockerLink{}, shockRocker{};
    float shockCompression=0.0f, dogboneError=0.0f, chainDistance=0.0f;
};

bool solveRockerAngle(const MotorcycleSwingarmHardpoints& h,const heritage::math::Vec3& movingDogbone,float& angle)
{
    const heritage::math::Vec3 axis=normalized(subtract(h.rockerPivotRight,h.rockerPivotLeft),{1,0,0});
    const heritage::math::Vec3 center=h.rockerPivotLeft;
    const heritage::math::Vec3 offset=subtract(h.rockerLinkMount,center);
    const float axial=dot(offset,axis);
    const heritage::math::Vec3 radial=subtract(offset,scale(axis,axial));
    const float radius=length(radial); if(radius<=kMinimumGeometryLength)return false;
    const float dogboneLength=length(subtract(h.linkageSwingarmMount,h.rockerLinkMount));
    if(dogboneLength<=kMinimumGeometryLength)return false;
    const heritage::math::Vec3 q=subtract(subtract(movingDogbone,center),scale(axis,axial));
    const heritage::math::Vec3 e1=scale(radial,1.0f/radius);
    const heritage::math::Vec3 e2=normalized(cross(axis,e1),{0,1,0});
    const float a=dot(e1,q),b=dot(e2,q),amp=std::sqrt(a*a+b*b); if(amp<=kEpsilon)return false;
    const float target=(radius*radius+dot(q,q)-dogboneLength*dogboneLength)/(2.0f*radius);
    const float n=target/amp; if(n<-1.0005f||n>1.0005f)return false;
    const float phase=std::atan2(b,a),delta=std::acos(std::clamp(n,-1.0f,1.0f));
    const float first=wrapPi(phase+delta),second=wrapPi(phase-delta);
    angle=std::abs(first)<=std::abs(second)?first:second;
    return finite(angle)&&std::abs(angle)<2.8f;
}

Pose poseAt(const MotorcycleSwingarmHardpoints& h,const heritage::math::Vec3& suspensionDirection,float compression)
{
    Pose p;
    const heritage::math::Vec3 armAxis=normalized(subtract(h.swingarmPivotRight,h.swingarmPivotLeft),{1,0,0});
    const heritage::math::Vec3 ref=subtract(h.wheelCenter,h.swingarmPivotLeft);
    const heritage::math::Vec3 radial=subtract(ref,scale(armAxis,dot(ref,armAxis)));
    if(length(radial)<=0.05f)return p;
    const heritage::math::Vec3 compDir=scale(normalized(suspensionDirection,{0,-1,0}),-1.0f);
    const float a=dot(compDir,radial),b=dot(compDir,cross(armAxis,radial));
    if(!solveNearestZeroTrig(a,b,compression+a,p.armAngle,p.clamped))return p;
    p.wheelCenter=rotatePointAroundLine(h.wheelCenter,h.swingarmPivotLeft,armAxis,p.armAngle);
    p.linkSwing=rotatePointAroundLine(h.linkageSwingarmMount,h.swingarmPivotLeft,armAxis,p.armAngle);
    if(!solveRockerAngle(h,p.linkSwing,p.rockerAngle))return p;
    const heritage::math::Vec3 rockerAxis=normalized(subtract(h.rockerPivotRight,h.rockerPivotLeft),{1,0,0});
    p.rockerLink=rotatePointAroundLine(h.rockerLinkMount,h.rockerPivotLeft,rockerAxis,p.rockerAngle);
    p.shockRocker=rotatePointAroundLine(h.shockRockerMount,h.rockerPivotLeft,rockerAxis,p.rockerAngle);
    const float refShock=length(subtract(h.shockRockerMount,h.shockChassisMount));
    p.shockCompression=refShock-length(subtract(p.shockRocker,h.shockChassisMount));
    p.dogboneError=length(subtract(p.linkSwing,p.rockerLink))-length(subtract(h.linkageSwingarmMount,h.rockerLinkMount));
    p.chainDistance=length(subtract(p.wheelCenter,h.countershaftCenter));
    p.valid=finite(p.wheelCenter)&&finite(p.shockCompression)&&finite(p.dogboneError)&&std::abs(p.dogboneError)<=0.0005f;
    return p;
}

heritage::math::Vec3 eulerDegreesFromBasis(const heritage::math::Vec3&r,const heritage::math::Vec3&u,const heritage::math::Vec3&f)
{
    const float y=std::asin(std::clamp(-r.z,-1.0f,1.0f)),cy=std::cos(y);float x=0,z=0;
    if(std::abs(cy)>kEpsilon){x=std::atan2(u.z,f.z);z=std::atan2(r.y,r.x);}else{x=std::atan2(-f.y,u.y);} return{degrees(x),degrees(y),degrees(z)};
}

} // namespace

bool validMotorcycleSwingarmHardpoints(const MotorcycleSwingarmHardpoints& h)
{
    if(!h.authored||!finite(h.swingarmPivotLeft)||!finite(h.swingarmPivotRight)||!finite(h.wheelCenter)
        ||!finite(h.linkageSwingarmMount)||!finite(h.rockerPivotLeft)||!finite(h.rockerPivotRight)
        ||!finite(h.rockerLinkMount)||!finite(h.shockChassisMount)||!finite(h.shockRockerMount)||!finite(h.countershaftCenter))return false;
    const heritage::math::Vec3 armAxis=subtract(h.swingarmPivotRight,h.swingarmPivotLeft);
    const heritage::math::Vec3 rockerAxis=subtract(h.rockerPivotRight,h.rockerPivotLeft);
    const float armAxisLength=length(armAxis),rockerAxisLength=length(rockerAxis);
    if(armAxisLength<0.03f||rockerAxisLength<0.03f)return false;
    const heritage::math::Vec3 armUnit=scale(armAxis,1.0f/armAxisLength);
    const float armRadius=length(cross(subtract(h.wheelCenter,h.swingarmPivotLeft),armUnit));
    const float dogbone=length(subtract(h.linkageSwingarmMount,h.rockerLinkMount));
    const float shock=length(subtract(h.shockRockerMount,h.shockChassisMount));
    return armRadius>0.20f&&armRadius<1.5f&&dogbone>0.03f&&dogbone<1.0f&&shock>0.05f&&shock<1.0f
        && length(subtract(h.countershaftCenter,h.wheelCenter))>0.20f;
}

MotorcycleSwingarmKinematicsOutput evaluateMotorcycleSwingarmKinematics(
    const MotorcycleSwingarmHardpoints& h,
    const MotorcycleSwingarmKinematicsInput& input)
{
    MotorcycleSwingarmKinematicsOutput out;
    if(!validMotorcycleSwingarmHardpoints(h)||!finite(input.compressionM)||!finite(input.suspensionDirection))return out;
    const Pose current=poseAt(h,input.suspensionDirection,input.compressionM);
    const Pose plus=poseAt(h,input.suspensionDirection,input.compressionM+kDerivativeStepM);
    const Pose minus=poseAt(h,input.suspensionDirection,input.compressionM-kDerivativeStepM);
    if(!current.valid||!plus.valid||!minus.valid)return out;
    const float shockRatio=(plus.shockCompression-minus.shockCompression)/(2.0f*kDerivativeStepM);
    const float chainRatio=(plus.chainDistance-minus.chainDistance)/(2.0f*kDerivativeStepM);
    if(!finite(shockRatio)||shockRatio<=0.02f||shockRatio>8.0f||!finite(chainRatio))return out;

    // Rear wheel plane remains parallel to the swingarm pivot axis. Rotating a
    // round wheel about that axle merely changes spin phase, so keep its tire
    // forward/up basis chassis-aligned while the axle centre follows the arc.
    const heritage::math::Vec3 right=normalized(subtract(h.swingarmPivotRight,h.swingarmPivotLeft),{1,0,0});
    heritage::math::Vec3 forward=normalized(subtract({0,0,1},scale(right,dot({0,0,1},right))),{0,0,1});
    heritage::math::Vec3 up=normalized(cross(forward,right),{0,1,0});
    forward=normalized(cross(right,up),{0,0,1});

    out.valid=true; out.travelClamped=current.clamped;
    out.swingarmAngleRadians=current.armAngle; out.rockerAngleRadians=current.rockerAngle;
    out.shockCompressionM=current.shockCompression; out.shockMotionRatio=shockRatio;
    out.dogboneLengthErrorM=current.dogboneError; out.chainCenterDistanceMotionRatio=chainRatio;
    out.wheelbaseDeltaM=current.wheelCenter.z-h.wheelCenter.z;
    out.localWheelCenter=current.wheelCenter; out.localLinkageSwingarmMount=current.linkSwing;
    out.localRockerLinkMount=current.rockerLink; out.localShockRockerMount=current.shockRocker;
    out.localWheelRight=right; out.localWheelForward=forward; out.localWheelUp=up;
    out.localUprightRotationDegrees=eulerDegreesFromBasis(right,up,forward);
    return out;
}

} // namespace heritage::vehicles

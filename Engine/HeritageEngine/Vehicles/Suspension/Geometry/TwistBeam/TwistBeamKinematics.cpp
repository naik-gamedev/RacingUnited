#include "TwistBeamKinematics.hpp"
#include <cmath>
namespace heritage::vehicles { namespace {
float dot(const heritage::math::Vec3&a,const heritage::math::Vec3&b){return a.x*b.x+a.y*b.y+a.z*b.z;}
heritage::math::Vec3 sub(const heritage::math::Vec3&a,const heritage::math::Vec3&b){return{a.x-b.x,a.y-b.y,a.z-b.z};}
heritage::math::Vec3 scale(const heritage::math::Vec3&a,float s){return{a.x*s,a.y*s,a.z*s};}
float len(const heritage::math::Vec3&a){return std::sqrt(dot(a,a));}
bool finite(const heritage::math::Vec3&a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
heritage::math::Vec3 norm(const heritage::math::Vec3&a,const heritage::math::Vec3&f){const float l=len(a);return l>1e-7f?scale(a,1.0f/l):f;}
float axisConvention(const SemiTrailingArmHardpoints& arm,const heritage::math::Vec3& beamAxis)
{
    const auto axis=norm(sub(arm.armPivotOuter,arm.armPivotInner),beamAxis);
    return dot(axis,beamAxis)>=0.0f?1.0f:-1.0f;
}
}}
namespace heritage::vehicles {
bool validTwistBeamHardpoints(const TwistBeamHardpoints&h){return h.authored&&validSemiTrailingArmHardpoints(h.leftArm)&&validSemiTrailingArmHardpoints(h.rightArm)&&finite(h.beamLeftAttachment)&&finite(h.beamRightAttachment)&&len(sub(h.beamRightAttachment,h.beamLeftAttachment))>0.25f;}
TwistBeamKinematicsOutput evaluateTwistBeamKinematics(const TwistBeamHardpoints&h,const TwistBeamKinematicsInput&i)
{
    TwistBeamKinematicsOutput o;if(!validTwistBeamHardpoints(h))return o;
    auto l=evaluateSemiTrailingArmKinematics(h.leftArm,{i.leftCompressionM,i.suspensionDirection,i.staticCamberDegrees,i.staticToeDegrees});
    auto r=evaluateSemiTrailingArmKinematics(h.rightArm,{i.rightCompressionM,i.suspensionDirection,i.staticCamberDegrees,i.staticToeDegrees});
    if(!l.valid||!r.valid)return o;
    const auto beamAxis=norm(sub(h.beamRightAttachment,h.beamLeftAttachment),{1,0,0});
    const float ls=axisConvention(h.leftArm,beamAxis),rs=axisConvention(h.rightArm,beamAxis);
    const float leftAngle=l.armRotationRadians*ls,rightAngle=r.armRotationRadians*rs;
    const float leftRatio=l.armAngularMotionRatioRadPerM*ls,rightRatio=r.armAngularMotionRatioRadPerM*rs;
    o.valid=true;o.travelClamped=l.travelClamped||r.travelClamped;o.arm=i.leftSide?l:r;
    o.beamTwistRadians=leftAngle-rightAngle;
    o.beamTwistRateRadiansPerSecond=leftRatio*i.leftCompressionVelocityMps-rightRatio*i.rightCompressionVelocityMps;
    o.beamAngularMotionRatioRadPerM=i.leftSide?leftRatio:-rightRatio;
    o.beamSpanM=len(sub(h.beamRightAttachment,h.beamLeftAttachment));return o;
}
} // namespace heritage::vehicles

#include "SemiTrailingArmKinematics.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {
constexpr float kPi=3.14159265358979323846f;
constexpr float kEps=1.0e-7f;
constexpr float kMin=0.01f;
constexpr float kDerivative=0.0005f;
float radians(float d){return d*kPi/180.0f;} float degrees(float r){return r*180.0f/kPi;}
heritage::math::Vec3 add(const heritage::math::Vec3&a,const heritage::math::Vec3&b){return{a.x+b.x,a.y+b.y,a.z+b.z};}
heritage::math::Vec3 sub(const heritage::math::Vec3&a,const heritage::math::Vec3&b){return{a.x-b.x,a.y-b.y,a.z-b.z};}
heritage::math::Vec3 scale(const heritage::math::Vec3&a,float s){return{a.x*s,a.y*s,a.z*s};}
float dot(const heritage::math::Vec3&a,const heritage::math::Vec3&b){return a.x*b.x+a.y*b.y+a.z*b.z;}
heritage::math::Vec3 cross(const heritage::math::Vec3&a,const heritage::math::Vec3&b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float len(const heritage::math::Vec3&a){return std::sqrt(dot(a,a));}
bool finite(const heritage::math::Vec3&a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
heritage::math::Vec3 norm(const heritage::math::Vec3&a,const heritage::math::Vec3&f){float l=len(a);return l>kEps?scale(a,1.0f/l):f;}
heritage::math::Vec3 rot(const heritage::math::Vec3&v,const heritage::math::Vec3&axis,float a){float c=std::cos(a),s=std::sin(a);return add(add(scale(v,c),scale(cross(axis,v),s)),scale(axis,dot(axis,v)*(1-c)));}
heritage::math::Vec3 rotPoint(const heritage::math::Vec3&p,const heritage::math::Vec3&o,const heritage::math::Vec3&a,float r){return add(o,rot(sub(p,o),a,r));}
float wrap(float a){while(a>kPi)a-=2*kPi;while(a<-kPi)a+=2*kPi;return a;}

bool solveAngle(const SemiTrailingArmHardpoints&h,const heritage::math::Vec3&susp,float compression,float&angle,bool&clamped){
    const auto axis=norm(sub(h.armPivotOuter,h.armPivotInner),{1,0,0});
    const auto ref=sub(h.wheelCenter,h.armPivotInner);
    const auto radial=sub(ref,scale(axis,dot(ref,axis)));
    if(len(radial)<0.05f)return false;
    const auto cdir=scale(norm(susp,{0,-1,0}),-1.0f);
    const float A=dot(cdir,radial),B=dot(cdir,cross(axis,radial));
    const float mag=std::sqrt(A*A+B*B); if(mag<kEps)return false;
    float q=(compression+A)/mag; clamped=q<-1.0f||q>1.0f;q=std::clamp(q,-1.0f,1.0f);
    const float phase=std::atan2(B,A),off=std::acos(q);
    const float a=wrap(phase+off),b=wrap(phase-off);angle=std::abs(a)<std::abs(b)?a:b;return true;
}

struct Pose{bool valid=false,clamped=false;float angle=0;heritage::math::Vec3 wheel{},spring{},damper{};};
Pose pose(const SemiTrailingArmHardpoints&h,const heritage::math::Vec3&susp,float c){Pose p;if(!solveAngle(h,susp,c,p.angle,p.clamped))return p;auto axis=norm(sub(h.armPivotOuter,h.armPivotInner),{1,0,0});p.wheel=rotPoint(h.wheelCenter,h.armPivotInner,axis,p.angle);p.spring=rotPoint(h.springLowerMount,h.armPivotInner,axis,p.angle);p.damper=rotPoint(h.damperLowerMount,h.armPivotInner,axis,p.angle);p.valid=finite(p.wheel)&&finite(p.spring)&&finite(p.damper);return p;}
float compressionAt(const SemiTrailingArmHardpoints&h,const heritage::math::Vec3&s,float c,bool spring){auto p=pose(h,s,c);if(!p.valid)return 0;const auto upper=spring?h.springUpperMount:h.damperUpperMount;const auto lower=spring?h.springLowerMount:h.damperLowerMount;const auto moved=spring?p.spring:p.damper;return len(sub(lower,upper))-len(sub(moved,upper));}
float angleAt(const SemiTrailingArmHardpoints&h,const heritage::math::Vec3&s,float c){auto p=pose(h,s,c);return p.valid?p.angle:0;}
float derivative(float c,const auto&fn){return(fn(c+kDerivative)-fn(c-kDerivative))/(2*kDerivative);}
heritage::math::Vec3 euler(const heritage::math::Vec3&r,const heritage::math::Vec3&u,const heritage::math::Vec3&f){float y=std::asin(std::clamp(-r.z,-1.0f,1.0f)),cy=std::cos(y),x=0,z=0;if(std::abs(cy)>kEps){x=std::atan2(u.z,f.z);z=std::atan2(r.y,r.x);}else x=std::atan2(-f.y,u.y);return{degrees(x),degrees(y),degrees(z)};}
float toe(const heritage::math::Vec3&f){return degrees(std::atan2(f.x,f.z));} float camber(const heritage::math::Vec3&r){return degrees(std::atan2(r.y,r.x));}
}

bool validSemiTrailingArmHardpoints(const SemiTrailingArmHardpoints&h){
    if(!h.authored||!finite(h.armPivotInner)||!finite(h.armPivotOuter)||!finite(h.wheelCenter)||!finite(h.springUpperMount)||!finite(h.springLowerMount)||!finite(h.damperUpperMount)||!finite(h.damperLowerMount))return false;
    const auto axis=sub(h.armPivotOuter,h.armPivotInner);if(len(axis)<0.05f)return false;const auto u=norm(axis,{1,0,0});
    if(len(cross(sub(h.wheelCenter,h.armPivotInner),u))<0.05f)return false;
    return len(sub(h.springLowerMount,h.springUpperMount))>kMin&&len(sub(h.damperLowerMount,h.damperUpperMount))>kMin;
}

SemiTrailingArmKinematicsOutput evaluateSemiTrailingArmKinematics(const SemiTrailingArmHardpoints&h,const SemiTrailingArmKinematicsInput&i){
    SemiTrailingArmKinematicsOutput o;if(!validSemiTrailingArmHardpoints(h)||!std::isfinite(i.compressionM))return o;auto p=pose(h,i.suspensionDirection,i.compressionM);if(!p.valid)return o;
    const auto axis=norm(sub(h.armPivotOuter,h.armPivotInner),{1,0,0});
    float t=radians(i.staticToeDegrees),c=radians(i.staticCamberDegrees);heritage::math::Vec3 f{std::sin(t),0,std::cos(t)},r{std::cos(t),0,-std::sin(t)},u{0,1,0};r=rot(r,f,c);u=rot(u,f,c);f=rot(f,axis,p.angle);r=rot(r,axis,p.angle);u=rot(u,axis,p.angle);f=norm(f,{0,0,1});r=norm(r,{1,0,0});u=norm(cross(f,r),{0,1,0});r=norm(cross(u,f),{1,0,0});
    auto af=[&](float x){return angleAt(h,i.suspensionDirection,x);};auto sf=[&](float x){return compressionAt(h,i.suspensionDirection,x,true);};auto df=[&](float x){return compressionAt(h,i.suspensionDirection,x,false);};
    o.valid=true;o.travelClamped=p.clamped;o.armRotationRadians=p.angle;o.armAngularMotionRatioRadPerM=derivative(i.compressionM,af);o.localWheelCenter=p.wheel;o.localWheelForward=f;o.localWheelRight=r;o.localWheelUp=u;o.camberDegrees=camber(r);o.toeDegrees=toe(f);o.bumpSteerDegrees=o.toeDegrees-i.staticToeDegrees;o.springCompressionM=sf(i.compressionM);o.springMotionRatio=std::clamp(std::abs(derivative(i.compressionM,sf)),0.02f,8.0f);o.damperCompressionM=df(i.compressionM);o.damperMotionRatio=std::clamp(std::abs(derivative(i.compressionM,df)),0.02f,8.0f);o.localUprightRotationDegrees=euler(r,u,f);return o;
}

} // namespace heritage::vehicles

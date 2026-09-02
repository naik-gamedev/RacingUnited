#include "MultiLinkKinematics.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace heritage::vehicles {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kEpsilon = 1.0e-7f;
constexpr float kMinimumLinkLength = 0.02f;
constexpr float kPoseDerivativeTranslationM = 0.00005f;
constexpr float kPoseDerivativeRotationRad = 0.0001f;
constexpr float kMotionRatioDerivativeStepM = 0.0005f;
constexpr float kRackDerivativeStepM = 0.0005f;

float radians(float degreesValue) { return degreesValue * (kPi / 180.0f); }
float degrees(float radiansValue) { return radiansValue * (180.0f / kPi); }

heritage::math::Vec3 add(const heritage::math::Vec3& a, const heritage::math::Vec3& b)
{ return { a.x + b.x, a.y + b.y, a.z + b.z }; }
heritage::math::Vec3 subtract(const heritage::math::Vec3& a, const heritage::math::Vec3& b)
{ return { a.x - b.x, a.y - b.y, a.z - b.z }; }
heritage::math::Vec3 scale(const heritage::math::Vec3& a, float s)
{ return { a.x * s, a.y * s, a.z * s }; }
float dot(const heritage::math::Vec3& a, const heritage::math::Vec3& b)
{ return a.x*b.x + a.y*b.y + a.z*b.z; }
heritage::math::Vec3 cross(const heritage::math::Vec3& a, const heritage::math::Vec3& b)
{ return { a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x }; }
float lengthSquared(const heritage::math::Vec3& a) { return dot(a,a); }
float length(const heritage::math::Vec3& a) { return std::sqrt(lengthSquared(a)); }
bool finite(const heritage::math::Vec3& a)
{ return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z); }
heritage::math::Vec3 normalized(const heritage::math::Vec3& a, const heritage::math::Vec3& fallback)
{
    const float l=length(a); return l>kEpsilon?scale(a,1.0f/l):fallback;
}

heritage::math::Vec3 rotateByVector(const heritage::math::Vec3& value, const heritage::math::Vec3& rotationVector)
{
    const float angle=length(rotationVector);
    if(angle<=kEpsilon) return value;
    const heritage::math::Vec3 axis=scale(rotationVector,1.0f/angle);
    const float c=std::cos(angle), s=std::sin(angle);
    return add(add(scale(value,c),scale(cross(axis,value),s)),scale(axis,dot(axis,value)*(1.0f-c)));
}

heritage::math::Vec3 eulerDegreesFromBasis(
    const heritage::math::Vec3& right,
    const heritage::math::Vec3& up,
    const heritage::math::Vec3& forward)
{
    const float y=std::asin(std::clamp(-right.z,-1.0f,1.0f));
    const float cy=std::cos(y);
    float x=0.0f,z=0.0f;
    if(std::abs(cy)>1.0e-6f){x=std::atan2(up.z,forward.z);z=std::atan2(right.y,right.x);}else{x=std::atan2(-forward.y,up.y);}
    return {degrees(x),degrees(y),degrees(z)};
}

float toeDegreesFromForward(const heritage::math::Vec3& f)
{ return degrees(std::atan2(f.x,f.z)); }
float camberDegreesFromRight(const heritage::math::Vec3& r)
{ return degrees(std::atan2(r.y,r.x)); }

struct Pose
{
    bool valid=false;
    bool clamped=false;
    heritage::math::Vec3 translation{}; // wheel centre delta from reference
    heritage::math::Vec3 rotationVector{};
    heritage::math::Vec3 wheelCenter{};
    heritage::math::Vec3 forward{0,0,1};
    heritage::math::Vec3 right{1,0,0};
    heritage::math::Vec3 up{0,1,0};
};

std::array<heritage::math::Vec3,5> outerPoints(const MultiLinkHardpoints& h)
{ return {h.link1Outer,h.link2Outer,h.link3Outer,h.link4Outer,h.toeLinkOuter}; }
std::array<heritage::math::Vec3,5> innerPoints(const MultiLinkHardpoints& h, float rackOffset)
{
    heritage::math::Vec3 rackAxis=normalized(subtract(h.steeringRackAxisEnd,h.steeringRackAxisStart),{1,0,0});
    return {h.link1Inner,h.link2Inner,h.link3Inner,h.link4Inner,add(h.toeLinkInner,scale(rackAxis,rackOffset))};
}

heritage::math::Vec3 transformUprightPoint(const MultiLinkHardpoints& h,const Pose& p,const heritage::math::Vec3& ref)
{
    return add(add(h.wheelCenter,p.translation),rotateByVector(subtract(ref,h.wheelCenter),p.rotationVector));
}

void referenceBasis(float staticCamberDegrees,float staticToeDegrees,
                    heritage::math::Vec3& f,heritage::math::Vec3& r,heritage::math::Vec3& u)
{
    const float toe=radians(staticToeDegrees), camber=radians(staticCamberDegrees);
    const float ct=std::cos(toe), st=std::sin(toe);
    f={st,0.0f,ct}; r={ct,0.0f,-st}; u={0,1,0};
    const heritage::math::Vec3 axis=f;
    const heritage::math::Vec3 rv=scale(axis,camber);
    r=rotateByVector(r,rv); u=rotateByVector(u,rv);
}

bool solveLinear6(float a[6][6],float b[6],float x[6])
{
    float m[6][7]{};
    for(int r=0;r<6;++r){for(int c=0;c<6;++c)m[r][c]=a[r][c];m[r][6]=b[r];}
    for(int c=0;c<6;++c){
        int pivot=c; float best=std::abs(m[c][c]);
        for(int r=c+1;r<6;++r){float v=std::abs(m[r][c]);if(v>best){best=v;pivot=r;}}
        if(best<1.0e-9f)return false;
        if(pivot!=c)for(int k=c;k<7;++k)std::swap(m[c][k],m[pivot][k]);
        const float inv=1.0f/m[c][c]; for(int k=c;k<7;++k)m[c][k]*=inv;
        for(int r=0;r<6;++r){if(r==c)continue;const float q=m[r][c];for(int k=c;k<7;++k)m[r][k]-=q*m[c][k];}
    }
    for(int i=0;i<6;++i)x[i]=m[i][6]; return true;
}

struct SolveContext
{
    std::array<heritage::math::Vec3,5> inner{};
    std::array<heritage::math::Vec3,5> outer{};
    std::array<float,5> lengths{};
    heritage::math::Vec3 compressionDirection{0,1,0};
};

Pose poseFromState(const MultiLinkHardpoints& h,const MultiLinkKinematicsInput& input,const float state[6])
{
    Pose p; p.translation={state[0],state[1],state[2]};p.rotationVector={state[3],state[4],state[5]};
    p.wheelCenter=add(h.wheelCenter,p.translation);
    heritage::math::Vec3 f,r,u;referenceBasis(input.staticCamberDegrees,input.staticToeDegrees,f,r,u);
    p.forward=rotateByVector(f,p.rotationVector);p.right=rotateByVector(r,p.rotationVector);p.up=rotateByVector(u,p.rotationVector);
    p.valid=finite(p.wheelCenter)&&finite(p.forward)&&finite(p.right)&&finite(p.up);
    return p;
}

void residuals(const MultiLinkHardpoints& h,const SolveContext& ctx,const Pose& p,float requestedCompressionM,float out[6])
{
    for(int i=0;i<5;++i){
        const heritage::math::Vec3 moved=transformUprightPoint(h,p,ctx.outer[i]);
        out[i]=length(subtract(moved,ctx.inner[i]))-ctx.lengths[i];
    }
    out[5]=dot(p.translation,ctx.compressionDirection)-requestedCompressionM;
}

Pose solvePoseCore(const MultiLinkHardpoints& h,const MultiLinkKinematicsInput& input,float compressionM,float rackOffset,const Pose* seed)
{
    SolveContext ctx;ctx.outer=outerPoints(h);ctx.inner=innerPoints(h,rackOffset);
    const auto refInner=innerPoints(h,0.0f);
    for(int i=0;i<5;++i)ctx.lengths[i]=length(subtract(ctx.outer[i],refInner[i]));
    ctx.compressionDirection=scale(normalized(input.suspensionDirection,{0,-1,0}),-1.0f);
    float state[6]{ctx.compressionDirection.x*compressionM,ctx.compressionDirection.y*compressionM,ctx.compressionDirection.z*compressionM,0,0,0};
    if(seed && seed->valid){state[0]=seed->translation.x;state[1]=seed->translation.y;state[2]=seed->translation.z;state[3]=seed->rotationVector.x;state[4]=seed->rotationVector.y;state[5]=seed->rotationVector.z;}
    Pose best;float bestError=1.0e9f;
    for(int iter=0;iter<36;++iter){
        Pose p=poseFromState(h,input,state);if(!p.valid)break;
        float r[6]{};residuals(h,ctx,p,compressionM,r);
        float error=0;for(float v:r)error+=std::abs(v);
        if(error<bestError){best=p;bestError=error;}
        float maxAbs=0;for(float v:r)maxAbs=std::max(maxAbs,std::abs(v));
        if(maxAbs<5.0e-6f){p.valid=true;p.clamped=false;return p;}
        float j[6][6]{};
        for(int c=0;c<6;++c){
            float probeState[6];for(int k=0;k<6;++k)probeState[k]=state[k];
            const float step=c<3?kPoseDerivativeTranslationM:kPoseDerivativeRotationRad;
            probeState[c]+=step;
            Pose q=poseFromState(h,input,probeState);if(!q.valid){best.valid=false;return best;}
            float qr[6]{};residuals(h,ctx,q,compressionM,qr);
            for(int row=0;row<6;++row)j[row][c]=(qr[row]-r[row])/step;
        }
        float rhs[6];for(int i=0;i<6;++i)rhs[i]=-r[i];float delta[6]{};
        if(!solveLinear6(j,rhs,delta))break;
        for(int i=0;i<3;++i)delta[i]=std::clamp(delta[i],-0.025f,0.025f);
        for(int i=3;i<6;++i)delta[i]=std::clamp(delta[i],-0.12f,0.12f);
        for(int i=0;i<6;++i)state[i]+=delta[i];
        for(int i=3;i<6;++i)state[i]=std::clamp(state[i],-1.0f,1.0f);
    }
    best.clamped=true;best.valid=best.valid&&bestError<0.0030f;return best;
}

Pose solvePose(const MultiLinkHardpoints& h,const MultiLinkKinematicsInput& input,float compressionM,float rackOffset)
{
    if(std::abs(rackOffset)<=1.0e-8f) return solvePoseCore(h,input,compressionM,0.0f,nullptr);
    const Pose passive=solvePoseCore(h,input,compressionM,0.0f,nullptr);
    return solvePoseCore(h,input,compressionM,rackOffset,passive.valid?&passive:nullptr);
}

float solveRackDisplacement(const MultiLinkHardpoints& h,const MultiLinkKinematicsInput& input)
{
    if(std::abs(input.steeringDegrees)<0.0001f)return 0.0f;
    MultiLinkKinematicsInput restInput=input;restInput.compressionM=0.0f;
    const Pose minus=solvePose(h,restInput,0.0f,-kRackDerivativeStepM);
    const Pose plus=solvePose(h,restInput,0.0f,kRackDerivativeStepM);
    if(!minus.valid||!plus.valid)return 0.0f;
    const float derivative=(toeDegreesFromForward(plus.forward)-toeDegreesFromForward(minus.forward))/(2.0f*kRackDerivativeStepM);
    if(std::abs(derivative)<5.0f)return 0.0f;
    return std::clamp(input.steeringDegrees/derivative,-0.10f,0.10f);
}

void actuatorState(const MultiLinkHardpoints& h,const MultiLinkKinematicsInput& input,float rackOffset,
                   const Pose& pose,const heritage::math::Vec3& upper,const heritage::math::Vec3& lower,
                   float& compression,float& ratio)
{
    const float restLength=length(subtract(lower,upper));
    const heritage::math::Vec3 movedLower=transformUprightPoint(h,pose,lower);
    compression=restLength-length(subtract(movedLower,upper));
    const Pose plus=solvePose(h,input,input.compressionM+kMotionRatioDerivativeStepM,rackOffset);
    const Pose minus=solvePose(h,input,input.compressionM-kMotionRatioDerivativeStepM,rackOffset);
    if(plus.valid&&minus.valid){
        const float plusComp=restLength-length(subtract(transformUprightPoint(h,plus,lower),upper));
        const float minusComp=restLength-length(subtract(transformUprightPoint(h,minus,lower),upper));
        ratio=std::abs((plusComp-minusComp)/(2.0f*kMotionRatioDerivativeStepM));
    }else ratio=1.0f;
    ratio=std::clamp(ratio,0.02f,8.0f);
}

heritage::math::Vec3 virtualSteeringAxis(const MultiLinkHardpoints& h,const MultiLinkKinematicsInput& input,float compression,float rackOffset,const Pose& pose)
{
    const Pose probe=solvePose(h,input,compression,rackOffset+kRackDerivativeStepM);
    if(!probe.valid)return {0,1,0};
    const heritage::math::Vec3 df=subtract(probe.forward,pose.forward);
    const heritage::math::Vec3 dr=subtract(probe.right,pose.right);
    heritage::math::Vec3 omega=add(cross(pose.forward,df),cross(pose.right,dr));
    return normalized(omega,{0,1,0});
}

} // namespace

bool validMultiLinkHardpoints(const MultiLinkHardpoints& h)
{
    if(!h.authored)return false;
    const std::array<heritage::math::Vec3,17> points={h.link1Inner,h.link1Outer,h.link2Inner,h.link2Outer,h.link3Inner,h.link3Outer,h.link4Inner,h.link4Outer,h.toeLinkInner,h.toeLinkOuter,h.wheelCenter,h.springUpperMount,h.springLowerMount,h.damperUpperMount,h.damperLowerMount,h.steeringRackAxisStart,h.steeringRackAxisEnd};
    for(const auto& p:points)if(!finite(p))return false;
    const auto inner=innerPoints(h,0.0f);const auto outer=outerPoints(h);
    for(int i=0;i<5;++i)if(length(subtract(outer[i],inner[i]))<kMinimumLinkLength)return false;
    if(length(subtract(h.springLowerMount,h.springUpperMount))<kMinimumLinkLength||length(subtract(h.damperLowerMount,h.damperUpperMount))<kMinimumLinkLength)return false;
    if(length(subtract(h.steeringRackAxisEnd,h.steeringRackAxisStart))<kMinimumLinkLength)return false;
    const heritage::math::Vec3 spreadA=subtract(h.link2Outer,h.link1Outer);
    const heritage::math::Vec3 spreadB=subtract(h.link3Outer,h.link1Outer);
    return length(cross(spreadA,spreadB))>0.0001f;
}

MultiLinkKinematicsOutput evaluateMultiLinkKinematics(const MultiLinkHardpoints& h,const MultiLinkKinematicsInput& input)
{
    MultiLinkKinematicsOutput out;if(!validMultiLinkHardpoints(h))return out;
    const float rackOffset=solveRackDisplacement(h,input);
    const Pose pose=solvePose(h,input,input.compressionM,rackOffset);if(!pose.valid)return out;
    const Pose passive=solvePose(h,input,input.compressionM,0.0f);
    out.valid=true;out.travelClamped=pose.clamped;out.localWheelCenter=pose.wheelCenter;out.localWheelForward=pose.forward;out.localWheelRight=pose.right;out.localWheelUp=pose.up;
    out.toeDegrees=toeDegreesFromForward(pose.forward);out.camberDegrees=camberDegreesFromRight(pose.right);
    out.bumpSteerDegrees=passive.valid?toeDegreesFromForward(passive.forward)-input.staticToeDegrees:0.0f;
    out.steeringRackDisplacementM=rackOffset;
    out.localSteeringAxis=virtualSteeringAxis(h,input,input.compressionM,rackOffset,pose);
    out.localSteeringAxisPoint=pose.wheelCenter;
    out.casterDegrees=degrees(std::atan2(-out.localSteeringAxis.z,std::max(std::abs(out.localSteeringAxis.y),1.0e-6f)));
    out.kingpinInclinationDegrees=degrees(std::atan2(out.localSteeringAxis.x,std::max(std::abs(out.localSteeringAxis.y),1.0e-6f)));
    actuatorState(h,input,rackOffset,pose,h.springUpperMount,h.springLowerMount,out.springCompressionM,out.springMotionRatio);
    actuatorState(h,input,rackOffset,pose,h.damperUpperMount,h.damperLowerMount,out.damperCompressionM,out.damperMotionRatio);
    out.localUprightRotationDegrees=eulerDegreesFromBasis(out.localWheelRight,out.localWheelUp,out.localWheelForward);
    return out;
}

} // namespace heritage::vehicles

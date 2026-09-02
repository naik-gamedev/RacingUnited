#pragma once
#include "LegacyAndSpecialKinematics.hpp"
#include <array>

namespace heritage::vehicles::suspension
{

// Generic double-link/girder/Hossack front carrier. Steering is a transform about steerAxis;
// suspension motion is then solved as a four-bar in the bike longitudinal/vertical plane.
// Telelever is represented by the same carrier plus its separate spring/damper link authority.
struct MotorcycleLinkFrontDescription
{
    SpecialVec3 upperChassisPivot{};
    SpecialVec3 lowerChassisPivot{};
    SpecialVec3 restUpperCarrierJoint{};
    SpecialVec3 restLowerCarrierJoint{};
    SpecialVec3 restWheelCentre{};
    SpecialVec3 steerAxis{0.0,1.0,0.0};
    SpecialVec3 steerOrigin{};
    double minimumTravelM=-0.12;
    double maximumTravelM=0.12;
};

struct MotorcycleLinkFrontState
{
    SpecialVec3 wheelCentre{};
    SpecialVec3 upperCarrierJoint{};
    SpecialVec3 lowerCarrierJoint{};
    double upperArmAngleRad=0.0;
    double lowerArmAngleRad=0.0;
    double steeringAngleRad=0.0;
    bool valid=false;
};

inline MotorcycleLinkFrontState solveMotorcycleLinkFront(
    const MotorcycleLinkFrontDescription& d,double requestedVerticalTravelM,double steeringAngleRad)
{
    MotorcycleLinkFrontState r;
    const SpecialVec3 ua=d.restUpperCarrierJoint-d.upperChassisPivot;
    const SpecialVec3 la=d.restLowerCarrierJoint-d.lowerChassisPivot;
    const SpecialVec3 carrier=d.restUpperCarrierJoint-d.restLowerCarrierJoint;
    const double carrierLen=specialLength(carrier);
    if(specialLength(ua)<1e-5||specialLength(la)<1e-5||carrierLen<1e-5) return r;

    // Mechanism is solved in x-y assuming bike z is lateral. This is deliberate: the entire
    // linkage can then be steered around an arbitrary 3D steer axis as one rigid assembly.
    const double targetY=d.restWheelCentre.y+std::max(d.minimumTravelM,std::min(d.maximumTravelM,requestedVerticalTravelM));
    double au=0.0,al=0.0;
    auto rotZ=[](SpecialVec3 v,double a){const double c=std::cos(a),s=std::sin(a);return SpecialVec3{c*v.x-s*v.y,s*v.x+c*v.y,v.z};};
    for(int i=0;i<16;++i)
    {
        SpecialVec3 u=d.upperChassisPivot+rotZ(ua,au);
        SpecialVec3 l=d.lowerChassisPivot+rotZ(la,al);
        SpecialVec3 c=u-l;
        const double e0=specialLength(c)-carrierLen;
        const double wheelYOffset=0.5*((u.y-d.restUpperCarrierJoint.y)+(l.y-d.restLowerCarrierJoint.y));
        const double e1=(d.restWheelCentre.y+wheelYOffset)-targetY;
        if(std::max(std::abs(e0),std::abs(e1))<1e-8) break;
        const double h=1e-5;
        auto eval=[&](double a0,double a1){
            SpecialVec3 uu=d.upperChassisPivot+rotZ(ua,a0);
            SpecialVec3 ll=d.lowerChassisPivot+rotZ(la,a1);
            const double q0=specialLength(uu-ll)-carrierLen;
            const double q1=(d.restWheelCentre.y+0.5*((uu.y-d.restUpperCarrierJoint.y)+(ll.y-d.restLowerCarrierJoint.y)))-targetY;
            return std::array<double,2>{q0,q1};};
        const auto fu=eval(au+h,al),fl=eval(au,al+h);
        const double j00=(fu[0]-e0)/h,j10=(fu[1]-e1)/h,j01=(fl[0]-e0)/h,j11=(fl[1]-e1)/h;
        const double det=j00*j11-j01*j10;
        if(std::abs(det)<1e-12) break;
        const double du=(-e0*j11+j01*e1)/det;
        const double dl=(-j00*e1+j10*e0)/det;
        au+=std::max(-0.08,std::min(0.08,du));
        al+=std::max(-0.08,std::min(0.08,dl));
    }
    SpecialVec3 u=d.upperChassisPivot+rotZ(ua,au);
    SpecialVec3 l=d.lowerChassisPivot+rotZ(la,al);
    const SpecialVec3 translation=(u-d.restUpperCarrierJoint+l-d.restLowerCarrierJoint)*0.5;
    SpecialVec3 wheel=d.restWheelCentre+translation;
    const SpecialVec3 axis=specialNormalized(d.steerAxis);
    r.upperCarrierJoint=d.steerOrigin+rotateAxis(u-d.steerOrigin,axis,steeringAngleRad);
    r.lowerCarrierJoint=d.steerOrigin+rotateAxis(l-d.steerOrigin,axis,steeringAngleRad);
    r.wheelCentre=d.steerOrigin+rotateAxis(wheel-d.steerOrigin,axis,steeringAngleRad);
    r.upperArmAngleRad=au;r.lowerArmAngleRad=al;r.steeringAngleRad=steeringAngleRad;
    r.valid=std::isfinite(r.wheelCentre.x)&&std::isfinite(r.wheelCentre.y)&&std::isfinite(r.wheelCentre.z);
    return r;
}

} // namespace heritage::vehicles::suspension

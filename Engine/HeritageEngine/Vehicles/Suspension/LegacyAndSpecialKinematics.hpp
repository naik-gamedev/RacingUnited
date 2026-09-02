#pragma once
#include <algorithm>
#include <cmath>

namespace heritage::vehicles::suspension
{

struct SpecialVec3 { double x=0.0,y=0.0,z=0.0; };
inline SpecialVec3 operator+(SpecialVec3 a,SpecialVec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline SpecialVec3 operator-(SpecialVec3 a,SpecialVec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline SpecialVec3 operator*(SpecialVec3 a,double s){return {a.x*s,a.y*s,a.z*s};}
inline double specialDot(SpecialVec3 a,SpecialVec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline SpecialVec3 specialCross(SpecialVec3 a,SpecialVec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline double specialLength(SpecialVec3 a){return std::sqrt(specialDot(a,a));}
inline SpecialVec3 specialNormalized(SpecialVec3 a){const double l=specialLength(a);return l>1e-12?a*(1.0/l):SpecialVec3{};}

struct SwingAxleDescription
{
    SpecialVec3 differentialPivot{};
    SpecialVec3 restWheelCentre{};
    SpecialVec3 hingeAxis{1.0,0.0,0.0};
    SpecialVec3 restWheelUp{0.0,1.0,0.0};
    double minimumAngleRad=-0.7;
    double maximumAngleRad=0.7;
};

struct SwingAxleState
{
    SpecialVec3 wheelCentre{};
    SpecialVec3 wheelUp{};
    double axleAngleRad=0.0;
    double scrubMetres=0.0;
    bool valid=false;
};

inline SpecialVec3 rotateAxis(SpecialVec3 v,SpecialVec3 axis,double angle)
{
    axis=specialNormalized(axis);
    const double c=std::cos(angle),s=std::sin(angle);
    return v*c+specialCross(axis,v)*s+axis*(specialDot(axis,v)*(1.0-c));
}

// Requested vertical travel is solved on the rigid half-shaft arc. Camber follows the axle,
// reproducing the characteristic swing-axle jacking/camber migration instead of a lookup curve.
inline SwingAxleState solveSwingAxle(const SwingAxleDescription& d,double requestedVerticalTravelM)
{
    SwingAxleState r;
    const SpecialVec3 arm=d.restWheelCentre-d.differentialPivot;
    const SpecialVec3 axis=specialNormalized(d.hingeAxis);
    if (specialLength(arm)<1e-6 || specialLength(axis)<0.5) return r;
    const double targetY=d.restWheelCentre.y+requestedVerticalTravelM;
    // bounded scalar Newton on hinge rotation
    double a=0.0;
    for(int i=0;i<12;++i)
    {
        const SpecialVec3 p=d.differentialPivot+rotateAxis(arm,axis,a);
        const double e=p.y-targetY;
        if(std::abs(e)<1e-8) break;
        const SpecialVec3 tangent=specialCross(axis,p-d.differentialPivot);
        if(std::abs(tangent.y)<1e-9) break;
        a-=e/tangent.y;
        a=std::max(d.minimumAngleRad,std::min(d.maximumAngleRad,a));
    }
    r.axleAngleRad=a;
    r.wheelCentre=d.differentialPivot+rotateAxis(arm,axis,a);
    r.wheelUp=specialNormalized(rotateAxis(d.restWheelUp,axis,a));
    const SpecialVec3 delta=r.wheelCentre-(d.restWheelCentre+SpecialVec3{0.0,requestedVerticalTravelM,0.0});
    r.scrubMetres=std::sqrt(delta.x*delta.x+delta.z*delta.z);
    r.valid=std::isfinite(r.wheelCentre.x)&&std::isfinite(r.wheelCentre.y)&&std::isfinite(r.wheelCentre.z);
    return r;
}

struct SlidingPillarDescription
{
    SpecialVec3 restWheelCentre{};
    SpecialVec3 pillarAxis{0.0,1.0,0.0};
    SpecialVec3 wheelUp{0.0,1.0,0.0};
    double minimumTravelM=-0.15;
    double maximumTravelM=0.15;
};

inline SwingAxleState solveSlidingPillar(const SlidingPillarDescription& d,double requestedTravelM)
{
    SwingAxleState r;
    const SpecialVec3 axis=specialNormalized(d.pillarAxis);
    if(specialLength(axis)<0.5) return r;
    const double x=std::max(d.minimumTravelM,std::min(d.maximumTravelM,requestedTravelM));
    r.wheelCentre=d.restWheelCentre+axis*x;
    r.wheelUp=specialNormalized(d.wheelUp);
    r.valid=true;
    return r;
}

// Integration aliases: these are not duplicate physics solvers.
// - Chapman strut -> existing MacPherson/strut provider with rear steering disabled.
// - Pure trailing arm -> SUSP13 semi_trailing_arm_v1 with sweep angle = 0.
// - De Dion -> SUSP08 rigid/live axle linkage provider with driveline/differential unsprung mass disabled.
// - 3/4/5-link independent -> SUSP14 multi_link_v1 with corresponding rigid constraints.

} // namespace heritage::vehicles::suspension

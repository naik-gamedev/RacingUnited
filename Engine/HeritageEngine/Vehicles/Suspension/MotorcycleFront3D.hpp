#pragma once
#include "MultiLinkKinematics.hpp"
#include "SuspensionMath.hpp"
#include "SuspensionScalarElements.hpp"
#include <array>

namespace heritage::vehicles::suspension
{

struct MotorcycleAArm3DDescription
{
    MultiLinkVec3 upperChassisFront{};
    MultiLinkVec3 upperChassisRear{};
    MultiLinkVec3 lowerChassisFront{};
    MultiLinkVec3 lowerChassisRear{};
    MultiLinkVec3 restUpperCarrierJoint{};
    MultiLinkVec3 restLowerCarrierJoint{};
    MultiLinkVec3 restWheelCentre{};
    MultiLinkVec3 restForward{0.0,0.0,1.0};
    MultiLinkVec3 travelAxis{0.0,1.0,0.0};
    double minimumTravelM=-0.15;
    double maximumTravelM=0.15;
    int maxIterations=18;
    double toleranceM=1.0e-7;
};

struct MotorcycleAArm3DState
{
    MultiLinkVec3 upperCarrierJoint{};
    MultiLinkVec3 lowerCarrierJoint{};
    MultiLinkVec3 wheelCentre{};
    MultiLinkVec3 steeringAxis{};
    MultiLinkVec3 forward{};
    double steeringAngleRad=0.0;
    double maxConstraintErrorM=0.0;
    int iterations=0;
    bool converged=false;
};

inline MultiLinkVec3 motoProjectPlane(const MultiLinkVec3& v,const MultiLinkVec3& n)
{
    return v-n*dot(v,n);
}

inline MultiLinkVec3 motoRotateAxis(const MultiLinkVec3& v,const MultiLinkVec3& axis,double a)
{
    const MultiLinkVec3 n=normalized(axis);
    const double c=std::cos(a),s=std::sin(a);
    return v*c+cross(n,v)*s+n*dot(n,v)*(1.0-c);
}

inline MultiLinkVec3 motoTransportForward(const MotorcycleAArm3DDescription& d,
                                           const MultiLinkVec3& upper,const MultiLinkVec3& lower)
{
    const MultiLinkVec3 restAxis=normalized(d.restUpperCarrierJoint-d.restLowerCarrierJoint);
    const MultiLinkVec3 axis=normalized(upper-lower);
    MultiLinkVec3 f0=normalized(motoProjectPlane(d.restForward,restAxis));
    if(lengthSquared(f0)<1.0e-8) f0=normalized(cross(restAxis,MultiLinkVec3{1,0,0}));
    const MultiLinkVec3 c=cross(restAxis,axis);
    const double cs=suspClamp(dot(restAxis,axis),-1.0,1.0);
    const double sn=length(c);
    if(sn<1.0e-10) return normalized(motoProjectPlane(f0,axis));
    const MultiLinkVec3 ra=c/sn;
    const double angle=std::atan2(sn,cs);
    return normalized(motoProjectPlane(motoRotateAxis(f0,ra,angle),axis));
}

inline MultiLinkVec3 motoWheelFromJoints(const MotorcycleAArm3DDescription& d,
                                         const MultiLinkVec3& upper,const MultiLinkVec3& lower)
{
    const MultiLinkVec3 restMid=(d.restUpperCarrierJoint+d.restLowerCarrierJoint)*0.5;
    const MultiLinkVec3 mid=(upper+lower)*0.5;
    const MultiLinkVec3 restAxis=normalized(d.restUpperCarrierJoint-d.restLowerCarrierJoint);
    const MultiLinkVec3 axis=normalized(upper-lower);
    const MultiLinkVec3 restF=motoTransportForward(d,d.restUpperCarrierJoint,d.restLowerCarrierJoint);
    const MultiLinkVec3 f=motoTransportForward(d,upper,lower);
    const MultiLinkVec3 restSide=normalized(cross(restAxis,restF));
    const MultiLinkVec3 side=normalized(cross(axis,f));
    const MultiLinkVec3 off=d.restWheelCentre-restMid;
    return mid + axis*dot(off,restAxis) + f*dot(off,restF) + side*dot(off,restSide);
}

inline std::array<double,6> motoResidual3D(const MotorcycleAArm3DDescription& d,
                                          const std::array<double,6>& x,double targetTravel)
{
    const MultiLinkVec3 u{x[0],x[1],x[2]},l{x[3],x[4],x[5]};
    const double uF=length(d.restUpperCarrierJoint-d.upperChassisFront);
    const double uR=length(d.restUpperCarrierJoint-d.upperChassisRear);
    const double lF=length(d.restLowerCarrierJoint-d.lowerChassisFront);
    const double lR=length(d.restLowerCarrierJoint-d.lowerChassisRear);
    const double carrier=length(d.restUpperCarrierJoint-d.restLowerCarrierJoint);
    const MultiLinkVec3 wheel=motoWheelFromJoints(d,u,l);
    const MultiLinkVec3 axis=normalized(d.travelAxis);
    return {
        length(u-d.upperChassisFront)-uF,
        length(u-d.upperChassisRear)-uR,
        length(l-d.lowerChassisFront)-lF,
        length(l-d.lowerChassisRear)-lR,
        length(u-l)-carrier,
        dot(wheel-d.restWheelCentre,axis)-targetTravel};
}

inline MotorcycleAArm3DState solveMotorcycleAArm3D(const MotorcycleAArm3DDescription& d,
                                                    double requestedTravelM,double steeringAngleRad,
                                                    const MotorcycleAArm3DState* warm=nullptr)
{
    MotorcycleAArm3DState out;
    const double target=suspClamp(requestedTravelM,d.minimumTravelM,d.maximumTravelM);
    std::array<double,6> x{
        d.restUpperCarrierJoint.x,d.restUpperCarrierJoint.y,d.restUpperCarrierJoint.z,
        d.restLowerCarrierJoint.x,d.restLowerCarrierJoint.y,d.restLowerCarrierJoint.z};
    if(warm&&warm->converged)
        x={warm->upperCarrierJoint.x,warm->upperCarrierJoint.y,warm->upperCarrierJoint.z,
           warm->lowerCarrierJoint.x,warm->lowerCarrierJoint.y,warm->lowerCarrierJoint.z};
    for(int it=0;it<std::max(1,d.maxIterations);++it)
    {
        const auto r=motoResidual3D(d,x,target);
        double maxe=0.0;for(double v:r)maxe=std::max(maxe,std::abs(v));
        out.maxConstraintErrorM=maxe;out.iterations=it+1;
        if(maxe<=std::max(1.0e-10,d.toleranceM)){out.converged=true;break;}
        SuspMat6 j{};
        for(std::size_t c=0;c<6;++c)
        {
            const double h=2.0e-7;
            auto xp=x,xm=x;xp[c]+=h;xm[c]-=h;
            const auto rp=motoResidual3D(d,xp,target),rm=motoResidual3D(d,xm,target);
            for(std::size_t rr=0;rr<6;++rr) j[rr][c]=(rp[rr]-rm[rr])/(2.0*h);
        }
        SuspVec6 b{};for(std::size_t i=0;i<6;++i)b[i]=-r[i];
        SuspVec6 dx{};
        // Levenberg-like diagonal if mechanism is near a singular posture.
        SuspMat6 normal{};SuspVec6 rhs{};
        for(std::size_t rr=0;rr<6;++rr) for(std::size_t c=0;c<6;++c)
        {
            rhs[c]+=j[rr][c]*b[rr];
            for(std::size_t k=0;k<6;++k)normal[c][k]+=j[rr][c]*j[rr][k];
        }
        for(std::size_t i=0;i<6;++i)normal[i][i]+=1.0e-12;
        if(!suspSolveLinear6(normal,rhs,dx))break;
        double norm=0.0;for(double v:dx)norm+=v*v;norm=std::sqrt(norm);
        if(norm>0.025)for(double& v:dx)v*=0.025/norm;
        for(std::size_t i=0;i<6;++i)x[i]+=dx[i];
    }
    out.upperCarrierJoint={x[0],x[1],x[2]};out.lowerCarrierJoint={x[3],x[4],x[5]};
    out.steeringAxis=normalized(out.upperCarrierJoint-out.lowerCarrierJoint);
    const MultiLinkVec3 baseWheel=motoWheelFromJoints(d,out.upperCarrierJoint,out.lowerCarrierJoint);
    const MultiLinkVec3 baseForward=motoTransportForward(d,out.upperCarrierJoint,out.lowerCarrierJoint);
    const MultiLinkVec3 origin=(out.upperCarrierJoint+out.lowerCarrierJoint)*0.5;
    out.wheelCentre=origin+motoRotateAxis(baseWheel-origin,out.steeringAxis,steeringAngleRad);
    out.forward=normalized(motoRotateAxis(baseForward,out.steeringAxis,steeringAngleRad));
    out.steeringAngleRad=steeringAngleRad;
    if(!std::isfinite(out.wheelCentre.x)||!std::isfinite(out.wheelCentre.y)||!std::isfinite(out.wheelCentre.z))out.converged=false;
    return out;
}

} // namespace heritage::vehicles::suspension

#pragma once
#include "SuspensionScalarElements.hpp"
#include <array>

namespace heritage::vehicles::suspension
{

struct Compliance6DofDescription
{
    std::array<double,6> stiffness{{1e7,1e7,1e7,1e5,1e5,1e5}}; // N/m then Nm/rad
    std::array<double,6> damping{{1e4,1e4,1e4,1e3,1e3,1e3}};
    std::array<double,6> freePlay{{0,0,0,0,0,0}};
    std::array<double,6> maximumDeflection{{0.03,0.03,0.03,0.1,0.1,0.1}};
};

struct Compliance6DofState
{
    std::array<double,6> deflection{{0,0,0,0,0,0}};
};

inline double complianceDeadband(double x,double play)
{
    const double p=std::max(0.0,play);
    if (x>p) return x-p;
    if (x<-p) return x+p;
    return 0.0;
}

// Quasi-static+viscous compliance owner for bushes, top mounts, subframes, joints and bearings.
// The rigid-body constraint solver supplies generalized load and relative velocity per axis.
inline std::array<double,6> solveCompliance6Dof(const Compliance6DofDescription& d,
                                                Compliance6DofState& s,
                                                const std::array<double,6>& generalizedLoad,
                                                const std::array<double,6>& relativeVelocity,
                                                double wearScale=0.0)
{
    std::array<double,6> reaction{};
    const double wear=suspClamp(wearScale,0.0,1.0);
    for (std::size_t i=0;i<6;++i)
    {
        const double k=std::max(1.0,d.stiffness[i])*(1.0-0.85*wear);
        const double c=std::max(0.0,d.damping[i])*(1.0-0.70*wear);
        const double play=d.freePlay[i]*(1.0+5.0*wear);
        double desired=generalizedLoad[i]/k;
        const double limit=std::max(0.0,d.maximumDeflection[i]);
        desired=suspClamp(desired,-limit,limit);
        s.deflection[i]=desired;
        const double elastic=complianceDeadband(s.deflection[i],play)*k;
        reaction[i]=-(elastic+c*relativeVelocity[i]);
    }
    return reaction;
}

struct JointClearanceDescription
{
    double radialClearanceM=0.0;
    double axialClearanceM=0.0;
    double angularClearanceRad=0.0;
    double impactRateNPerM=5e7;
    double impactDampingNPerMps=1e4;
};

inline double evaluateJointClearanceAxis(double displacement,double velocity,
                                         double clearance,double rate,double damping)
{
    const double engaged=complianceDeadband(displacement,std::max(0.0,clearance));
    if (engaged==0.0) return 0.0;
    return -(std::max(0.0,rate)*engaged+std::max(0.0,damping)*velocity);
}

} // namespace heritage::vehicles::suspension

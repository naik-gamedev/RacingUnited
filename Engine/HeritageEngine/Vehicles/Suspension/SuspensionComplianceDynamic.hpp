#pragma once
#include "SuspensionMath.hpp"
#include "SuspensionScalarElements.hpp"

namespace heritage::vehicles::suspension
{

struct Compliance6DofDescriptionV2
{
    SuspMat6 stiffness = suspDiagonalMat6({1.0e7,1.0e7,1.0e7,1.0e5,1.0e5,1.0e5});
    SuspMat6 damping = suspDiagonalMat6({1.0e4,1.0e4,1.0e4,1.0e3,1.0e3,1.0e3});
    SuspVec6 generalizedMass{{2.0,2.0,2.0,0.05,0.05,0.05}};
    SuspVec6 freePlay{{0,0,0,0,0,0}};
    SuspVec6 maximumDeflection{{0.03,0.03,0.03,0.12,0.12,0.12}};
    SuspVec6 coulombFriction{{0,0,0,0,0,0}};
    SuspVec6 hysteresisStrength{{0,0,0,0,0,0}};
    double hysteresisRate = 30.0;
    double wearStiffnessLoss = 0.85;
    double wearDampingLoss = 0.70;
    double wearPlayMultiplier = 5.0;
    double maximumInternalDt = 0.00025;
};

struct Compliance6DofStateV2
{
    SuspVec6 deflection{};
    SuspVec6 velocity{};
    SuspVec6 hysteresis{};
};

struct Compliance6DofResultV2
{
    SuspVec6 reaction{};
    SuspVec6 deflection{};
    SuspVec6 velocity{};
    double storedEnergyJ = 0.0;
    double dissipatedPowerW = 0.0;
    bool limited = false;
};

inline SuspVec6 complianceEngagedDeflection(const Compliance6DofDescriptionV2& d,
                                             const Compliance6DofStateV2& s,
                                             double wear)
{
    SuspVec6 q{};
    for (std::size_t i=0;i<6;++i)
    {
        const double p = std::max(0.0,d.freePlay[i])*(1.0 + std::max(0.0,d.wearPlayMultiplier)*wear);
        const double x = s.deflection[i];
        q[i] = x > p ? x-p : (x < -p ? x+p : 0.0);
    }
    return q;
}

inline Compliance6DofResultV2 stepCompliance6DofV2(const Compliance6DofDescriptionV2& d,
                                                     Compliance6DofStateV2& s,
                                                     const SuspVec6& generalizedLoad,
                                                     double wearScale,
                                                     double dtSeconds)
{
    Compliance6DofResultV2 out;
    const double dt = std::max(0.0,dtSeconds);
    const double wear = suspClamp(wearScale,0.0,1.0);
    if (dt <= 0.0)
    {
        out.deflection=s.deflection; out.velocity=s.velocity;
        return out;
    }
    const double maxDt=std::max(1.0e-6,d.maximumInternalDt);
    const int steps=std::max(1,std::min(64,static_cast<int>(std::ceil(dt/maxDt))));
    const double h=dt/static_cast<double>(steps);
    const double kScale=std::max(0.02,1.0-d.wearStiffnessLoss*wear);
    const double cScale=std::max(0.02,1.0-d.wearDampingLoss*wear);

    for (int step=0;step<steps;++step)
    {
        const SuspVec6 engaged=complianceEngagedDeflection(d,s,wear);
        SuspVec6 elastic=suspMatVec(d.stiffness,engaged);
        SuspVec6 viscous=suspMatVec(d.damping,s.velocity);
        for (std::size_t i=0;i<6;++i)
        {
            elastic[i]*=kScale;
            viscous[i]*=cScale;
            const double hr=std::max(0.0,d.hysteresisRate);
            const double target=std::tanh(100.0*s.velocity[i]);
            s.hysteresis[i] += (target-s.hysteresis[i])*(1.0-std::exp(-hr*h));
            const double friction=std::max(0.0,d.coulombFriction[i])*std::tanh(s.velocity[i]/1.0e-4);
            const double hyster=d.hysteresisStrength[i]*s.hysteresis[i];
            const double m=std::max(1.0e-6,d.generalizedMass[i]);
            const double a=(generalizedLoad[i]-elastic[i]-viscous[i]-friction-hyster)/m;
            s.velocity[i]+=a*h;
            s.deflection[i]+=s.velocity[i]*h;
            const double lim=std::max(0.0,d.maximumDeflection[i]);
            if (lim>0.0 && std::abs(s.deflection[i])>lim)
            {
                s.deflection[i]=suspSign(s.deflection[i])*lim;
                if (s.velocity[i]*s.deflection[i]>0.0) s.velocity[i]=0.0;
                out.limited=true;
            }
            if (!suspFinite(s.deflection[i]) || !suspFinite(s.velocity[i]))
            {
                s.deflection[i]=0.0; s.velocity[i]=0.0; s.hysteresis[i]=0.0;
                out.limited=true;
            }
        }
    }

    const SuspVec6 engaged=complianceEngagedDeflection(d,s,wear);
    SuspVec6 elastic=suspMatVec(d.stiffness,engaged);
    SuspVec6 viscous=suspMatVec(d.damping,s.velocity);
    for (std::size_t i=0;i<6;++i)
    {
        elastic[i]*=kScale; viscous[i]*=cScale;
        const double friction=std::max(0.0,d.coulombFriction[i])*std::tanh(s.velocity[i]/1.0e-4);
        const double hyster=d.hysteresisStrength[i]*s.hysteresis[i];
        out.reaction[i]=-(elastic[i]+viscous[i]+friction+hyster);
        out.dissipatedPowerW += std::max(0.0,(viscous[i]+friction)*s.velocity[i]);
        out.storedEnergyJ += 0.5*engaged[i]*elastic[i];
    }
    out.deflection=s.deflection; out.velocity=s.velocity;
    return out;
}

} // namespace heritage::vehicles::suspension

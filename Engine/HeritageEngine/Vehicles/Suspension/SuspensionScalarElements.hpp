#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace heritage::vehicles::suspension
{

inline double suspClamp(double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); }
inline double suspSign(double v) { return (v > 0.0) - (v < 0.0); }
inline bool suspFinite(double v) { return std::isfinite(v); }

struct ProgressiveSpringDescription
{
    double freeLengthMetres = 0.30;
    double preloadNewtons = 0.0;
    double linearRateNPerM = 30000.0;
    double quadraticRateNPerM2 = 0.0;
    double cubicRateNPerM3 = 0.0;
    double maximumCompressionMetres = 0.25;
};

struct ProgressiveSpringResult
{
    double compressionMetres = 0.0;
    double forceNewtons = 0.0;
    double tangentRateNPerM = 0.0;
    double storedEnergyJoules = 0.0;
};

inline ProgressiveSpringResult evaluateProgressiveSpring(
    const ProgressiveSpringDescription& d, double currentLengthMetres)
{
    ProgressiveSpringResult r;
    r.compressionMetres = suspClamp(d.freeLengthMetres - currentLengthMetres, 0.0,
                                    std::max(0.0, d.maximumCompressionMetres));
    const double x = r.compressionMetres;
    r.forceNewtons = std::max(0.0, d.preloadNewtons
        + d.linearRateNPerM * x
        + d.quadraticRateNPerM2 * x*x
        + d.cubicRateNPerM3 * x*x*x);
    r.tangentRateNPerM = std::max(0.0, d.linearRateNPerM
        + 2.0*d.quadraticRateNPerM2*x
        + 3.0*d.cubicRateNPerM3*x*x);
    r.storedEnergyJoules = std::max(0.0, d.preloadNewtons*x
        + 0.5*d.linearRateNPerM*x*x
        + (1.0/3.0)*d.quadraticRateNPerM2*x*x*x
        + 0.25*d.cubicRateNPerM3*x*x*x*x);
    return r;
}

// Main + helper/tender spring in series until helper binds solid, then main spring alone.
struct DualRateSpringDescription
{
    double mainRateNPerM = 80000.0;
    double helperRateNPerM = 10000.0;
    double helperBindCompressionMetres = 0.025;
    double preloadNewtons = 0.0;
};

struct DualRateSpringResult
{
    double forceNewtons = 0.0;
    double tangentRateNPerM = 0.0;
    bool helperBound = false;
};

inline DualRateSpringResult evaluateDualRateSpring(const DualRateSpringDescription& d,
                                                    double totalCompressionMetres)
{
    DualRateSpringResult r;
    const double x = std::max(0.0, totalCompressionMetres);
    const double km = std::max(1.0, d.mainRateNPerM);
    const double kh = std::max(1.0, d.helperRateNPerM);
    const double kSeries = (km * kh) / (km + kh);
    const double bind = std::max(0.0, d.helperBindCompressionMetres);
    // helper compression while in series is F/kh = (kSeries*x)/kh
    const double totalAtBind = bind * kh / std::max(1.0, kSeries);
    if (x <= totalAtBind || bind <= 0.0)
    {
        r.forceNewtons = std::max(0.0, d.preloadNewtons + kSeries*x);
        r.tangentRateNPerM = kSeries;
        r.helperBound = false;
    }
    else
    {
        const double fBind = kSeries * totalAtBind;
        r.forceNewtons = std::max(0.0, d.preloadNewtons + fBind + km*(x-totalAtBind));
        r.tangentRateNPerM = km;
        r.helperBound = true;
    }
    return r;
}

struct StopDescription
{
    double engagementMetres = 0.0;
    double linearRateNPerM = 0.0;
    double cubicRateNPerM3 = 2.0e8;
    double dampingNPerMps = 1000.0;
    double maximumForceNewtons = 1.0e6;
};

inline double evaluateStopForce(const StopDescription& d, double penetrationMetres,
                                double penetrationVelocityMps)
{
    const double x = std::max(0.0, penetrationMetres - std::max(0.0, d.engagementMetres));
    if (x <= 0.0) return 0.0;
    double f = std::max(0.0, d.linearRateNPerM)*x
             + std::max(0.0, d.cubicRateNPerM3)*x*x*x;
    if (penetrationVelocityMps > 0.0)
        f += std::max(0.0, d.dampingNPerMps)*penetrationVelocityMps;
    return suspClamp(f, 0.0, std::max(0.0, d.maximumForceNewtons));
}

struct TorsionBarDescription
{
    double neutralAngleRadians = 0.0;
    double rateNmPerRad = 800.0;
    double preloadTorqueNm = 0.0;
    double maximumTorqueNm = 10000.0;
};

inline double evaluateTorsionBarTorque(const TorsionBarDescription& d, double angleRadians)
{
    const double theta = angleRadians - d.neutralAngleRadians;
    const double t = d.preloadTorqueNm + std::max(0.0, d.rateNmPerRad)*theta;
    return suspClamp(t, -std::abs(d.maximumTorqueNm), std::abs(d.maximumTorqueNm));
}

struct PneumaticSpringDescription
{
    double referenceVolumeM3 = 0.0025;
    double referenceAbsolutePressurePa = 6.0e5;
    double pistonAreaM2 = 0.0030;
    double reservoirVolumeM3 = 0.0;
    double ambientAbsolutePressurePa = 101325.0;
    double polytropicExponent = 1.30;
    double minimumVolumeM3 = 1.0e-5;
    double maximumPressurePa = 3.0e7;
};

struct PneumaticSpringResult
{
    double absolutePressurePa = 0.0;
    double forceNewtons = 0.0;
    double tangentRateNPerM = 0.0;
};

// Positive compression decreases chamber volume by area*x. Reservoir shares pressure.
inline PneumaticSpringResult evaluatePneumaticSpring(const PneumaticSpringDescription& d,
                                                      double compressionMetres)
{
    PneumaticSpringResult r;
    const double area = std::max(1.0e-8, d.pistonAreaM2);
    const double v0 = std::max(d.minimumVolumeM3, d.referenceVolumeM3 + std::max(0.0, d.reservoirVolumeM3));
    const double v = std::max(d.minimumVolumeM3, v0 - area*compressionMetres);
    const double gamma = suspClamp(d.polytropicExponent, 1.0, 1.67);
    const double p0 = std::max(d.ambientAbsolutePressurePa, d.referenceAbsolutePressurePa);
    const double p = suspClamp(p0*std::pow(v0/v, gamma), d.ambientAbsolutePressurePa,
                               std::max(d.ambientAbsolutePressurePa, d.maximumPressurePa));
    r.absolutePressurePa = p;
    r.forceNewtons = std::max(0.0, (p-d.ambientAbsolutePressurePa)*area);
    r.tangentRateNPerM = gamma*p*area*area/v;
    return r;
}

// Hydropneumatic gas sphere: liquid displacement compresses nitrogen sphere.
struct HydroPneumaticSpringDescription
{
    double gasReferenceVolumeM3 = 0.0010;
    double gasPrechargePressurePa = 5.0e6;
    double hydraulicPistonAreaM2 = 0.0015;
    double gasExponent = 1.35;
    double ambientPressurePa = 101325.0;
    double minimumGasVolumeM3 = 1.0e-5;
};

inline PneumaticSpringResult evaluateHydroPneumaticSpring(
    const HydroPneumaticSpringDescription& d, double pistonCompressionMetres)
{
    PneumaticSpringDescription p;
    p.referenceVolumeM3 = d.gasReferenceVolumeM3;
    p.referenceAbsolutePressurePa = d.gasPrechargePressurePa;
    p.pistonAreaM2 = d.hydraulicPistonAreaM2;
    p.reservoirVolumeM3 = 0.0;
    p.ambientAbsolutePressurePa = d.ambientPressurePa;
    p.polytropicExponent = d.gasExponent;
    p.minimumVolumeM3 = d.minimumGasVolumeM3;
    p.maximumPressurePa = 1.0e8;
    return evaluatePneumaticSpring(p, pistonCompressionMetres);
}

struct LeafFrictionDescription
{
    double coulombForceNewtons = 0.0;
    double smoothingVelocityMps = 0.005;
};

inline double evaluateLeafInterleafFriction(const LeafFrictionDescription& d, double velocityMps)
{
    const double vs = std::max(1.0e-6, d.smoothingVelocityMps);
    return -std::max(0.0, d.coulombForceNewtons)*std::tanh(velocityMps/vs);
}

} // namespace heritage::vehicles::suspension

#pragma once
#include "SuspensionScalarElements.hpp"
#include <array>

namespace heritage::vehicles::suspension
{

struct DamperCurvePoint
{
    double speedMps = 0.0;
    double forceNewtons = 0.0; // magnitude, positive
};

struct DamperCurve
{
    static constexpr std::size_t MaxPoints = 8;
    std::array<DamperCurvePoint, MaxPoints> points{};
    std::size_t count = 0;
};

inline double sampleDamperCurve(const DamperCurve& c, double speedMagnitudeMps)
{
    const double v = std::max(0.0, speedMagnitudeMps);
    if (c.count == 0) return 0.0;
    if (c.count == 1 || v <= c.points[0].speedMps) return std::max(0.0, c.points[0].forceNewtons);
    const std::size_t n = std::min(c.count, DamperCurve::MaxPoints);
    for (std::size_t i=1; i<n; ++i)
    {
        if (v <= c.points[i].speedMps)
        {
            const auto& a = c.points[i-1];
            const auto& b = c.points[i];
            const double dv = std::max(1.0e-12, b.speedMps-a.speedMps);
            const double t = suspClamp((v-a.speedMps)/dv, 0.0, 1.0);
            return std::max(0.0, a.forceNewtons + (b.forceNewtons-a.forceNewtons)*t);
        }
    }
    return std::max(0.0, c.points[n-1].forceNewtons);
}

struct DamperDescriptionV2
{
    DamperCurve compression{};
    DamperCurve rebound{};
    double lowSpeedSealFrictionNewtons = 0.0;
    double sealSmoothingVelocityMps = 0.003;
    double gasSpringForceNewtons = 0.0;
    double bumpPositionGain = 0.0;
    double reboundPositionGain = 0.0;
    double nominalTemperatureC = 80.0;
    double fadeStartTemperatureC = 120.0;
    double fadeFullTemperatureC = 180.0;
    double hotForceFraction = 0.55;
    double thermalCapacityJPerK = 14000.0;
    double coolingWPerK = 35.0;
    double cavitationStartReboundMps = 1.2;
    double cavitationFullReboundMps = 3.0;
    double cavitatedForceFraction = 0.45;
    double maximumForceNewtons = 50000.0;
};

struct DamperStateV2
{
    double temperatureC = 80.0;
    double dissipatedEnergyJ = 0.0;
    double cavitation = 0.0;
};

struct DamperResultV2
{
    double forceNewtons = 0.0; // force on first endpoint; always opposes extension velocity except gas force
    double hydraulicForceNewtons = 0.0;
    double thermalMultiplier = 1.0;
    double positionMultiplier = 1.0;
    double cavitationMultiplier = 1.0;
};

inline DamperResultV2 stepDamperV2(const DamperDescriptionV2& d, DamperStateV2& s,
                                   double extensionVelocityMps,
                                   double normalizedPosition,
                                   double ambientTemperatureC,
                                   double dtSeconds,
                                   double semiActiveScale = 1.0)
{
    DamperResultV2 r;
    const bool extending = extensionVelocityMps > 0.0;
    const double v = std::abs(extensionVelocityMps);
    const double base = sampleDamperCurve(extending ? d.rebound : d.compression, v);
    const double p = suspClamp(normalizedPosition, 0.0, 1.0);
    r.positionMultiplier = extending
        ? std::max(0.0, 1.0 + d.reboundPositionGain*(1.0-p))
        : std::max(0.0, 1.0 + d.bumpPositionGain*p);

    const double t0 = d.fadeStartTemperatureC;
    const double t1 = std::max(t0+1.0, d.fadeFullTemperatureC);
    const double hotBlend = suspClamp((s.temperatureC-t0)/(t1-t0), 0.0, 1.0);
    r.thermalMultiplier = 1.0 + (suspClamp(d.hotForceFraction,0.0,1.0)-1.0)*hotBlend;

    if (extending)
    {
        const double c0 = d.cavitationStartReboundMps;
        const double c1 = std::max(c0+0.01, d.cavitationFullReboundMps);
        s.cavitation = suspClamp((v-c0)/(c1-c0), 0.0, 1.0);
    }
    else
    {
        // compression restores pressure reserve.
        s.cavitation = std::max(0.0, s.cavitation - 4.0*std::max(0.0,dtSeconds));
    }
    r.cavitationMultiplier = 1.0 + (suspClamp(d.cavitatedForceFraction,0.0,1.0)-1.0)*s.cavitation;

    const double valve = suspClamp(semiActiveScale, 0.05, 4.0);
    const double hydraulicMagnitude = base*r.positionMultiplier*r.thermalMultiplier*r.cavitationMultiplier*valve;
    const double seal = std::max(0.0,d.lowSpeedSealFrictionNewtons)
                      * std::tanh(v/std::max(1.0e-6,d.sealSmoothingVelocityMps));
    const double passiveMagnitude = hydraulicMagnitude + seal;
    r.hydraulicForceNewtons = -suspSign(extensionVelocityMps)*passiveMagnitude;
    r.forceNewtons = suspClamp(r.hydraulicForceNewtons + d.gasSpringForceNewtons,
                               -std::abs(d.maximumForceNewtons), std::abs(d.maximumForceNewtons));

    const double dissipatedPower = std::max(0.0, -r.hydraulicForceNewtons*extensionVelocityMps);
    const double dt = std::max(0.0, dtSeconds);
    s.dissipatedEnergyJ += dissipatedPower*dt;
    const double capacity = std::max(1.0,d.thermalCapacityJPerK);
    const double cooling = std::max(0.0,d.coolingWPerK)*(s.temperatureC-ambientTemperatureC);
    s.temperatureC += (dissipatedPower-cooling)*dt/capacity;
    if (!suspFinite(s.temperatureC)) s.temperatureC = d.nominalTemperatureC;
    return r;
}

// Passive/semi-active command helper. It can only change damping magnitude and can never
// command a force that adds energy to the suspension DOF.
inline double semiActiveScaleForTargetForce(double passiveForceAtScale1,
                                             double extensionVelocityMps,
                                             double desiredForceNewtons,
                                             double minimumScale,
                                             double maximumScale)
{
    if (std::abs(passiveForceAtScale1) < 1.0e-9 || std::abs(extensionVelocityMps) < 1.0e-9)
        return suspClamp(1.0, minimumScale, maximumScale);
    // Desired hydraulic force must oppose velocity.
    const double desiredPassive = (desiredForceNewtons*extensionVelocityMps <= 0.0)
        ? desiredForceNewtons : 0.0;
    const double scale = std::abs(desiredPassive/passiveForceAtScale1);
    return suspClamp(scale, minimumScale, maximumScale);
}

} // namespace heritage::vehicles::suspension

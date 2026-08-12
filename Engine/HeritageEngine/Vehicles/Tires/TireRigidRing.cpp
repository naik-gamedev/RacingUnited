#include "TireRigidRing.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles::tires {
namespace {

constexpr VehicleScalar kPi = 3.14159265358979323846;
constexpr VehicleScalar kEpsilon = 1.0e-9;

bool finiteValue(VehicleScalar value)
{
    return std::isfinite(static_cast<double>(value));
}

VehicleScalar frequencyFromStiffness(
    VehicleScalar stiffness,
    VehicleScalar massOrInertia)
{
    if (stiffness <= kEpsilon || massOrInertia <= kEpsilon)
        return 0.0;
    return std::sqrt(stiffness / massOrInertia) / (2.0 * kPi);
}

VehicleScalar effectiveFrequency(
    VehicleScalar identifiedHz,
    VehicleScalar stiffness,
    VehicleScalar massOrInertia)
{
    if (identifiedHz > 0.1)
        return identifiedHz;
    return frequencyFromStiffness(stiffness, massOrInertia);
}

VehicleScalar stiffnessFromFrequency(
    VehicleScalar frequencyHz,
    VehicleScalar massOrInertia)
{
    if (frequencyHz <= 0.1 || massOrInertia <= kEpsilon)
        return 0.0;
    const VehicleScalar w = 2.0 * kPi * frequencyHz;
    return massOrInertia * w * w;
}

VehicleScalar lowSpeedDamping(
    const TireRigidRingDescription& d,
    VehicleScalar base,
    VehicleScalar forwardSpeedMps)
{
    const VehicleScalar threshold = std::max(
        d.lowSpeedThresholdMps, VehicleScalar{0.05});
    const VehicleScalar t = std::clamp(
        std::abs(forwardSpeedMps) / threshold,
        VehicleScalar{0.0}, VehicleScalar{1.0});
    const VehicleScalar lowSpeedBlend = VehicleScalar{1.0} - t;
    return std::max(
        base + d.residualDampingRatio
            + lowSpeedBlend * d.lowSpeedAdditionalDampingRatio
                * std::max(d.lowSpeedDampingScale, VehicleScalar{0.0}),
        VehicleScalar{0.0});
}

// Exact state transition for x'' + 2*zeta*w*x' + w^2*(x-target)=0 over a
// constant-target interval. It applies equally to translation and rotation.
void advanceSecondOrder(
    VehicleScalar target,
    VehicleScalar frequencyHz,
    VehicleScalar dampingRatio,
    VehicleScalar deltaTimeSeconds,
    VehicleScalar& position,
    VehicleScalar& velocity)
{
    if (deltaTimeSeconds <= 0.0 || frequencyHz <= 0.01)
    {
        position = target;
        velocity = 0.0;
        return;
    }

    const VehicleScalar w = 2.0 * kPi * frequencyHz;
    const VehicleScalar zeta = std::max(dampingRatio, VehicleScalar{0.0});
    const VehicleScalar y0 = position - target;
    const VehicleScalar v0 = velocity;
    const VehicleScalar dt = deltaTimeSeconds;

    VehicleScalar y1 = y0;
    VehicleScalar v1 = v0;

    if (zeta < 1.0 - 1.0e-6)
    {
        const VehicleScalar a = zeta * w;
        const VehicleScalar b = w * std::sqrt(std::max(
            VehicleScalar{1.0} - zeta * zeta,
            VehicleScalar{1.0e-12}));
        const VehicleScalar e = std::exp(-a * dt);
        const VehicleScalar c = std::cos(b * dt);
        const VehicleScalar s = std::sin(b * dt);
        y1 = e * (y0 * c + (v0 + a * y0) / b * s);
        v1 = e * (v0 * c - (a * v0 + w * w * y0) / b * s);
    }
    else if (zeta <= 1.0 + 1.0e-6)
    {
        const VehicleScalar e = std::exp(-w * dt);
        const VehicleScalar c = v0 + w * y0;
        y1 = e * (y0 + c * dt);
        v1 = e * (v0 - w * c * dt);
    }
    else
    {
        const VehicleScalar root = std::sqrt(zeta * zeta - 1.0);
        const VehicleScalar r1 = -w * (zeta - root);
        const VehicleScalar r2 = -w * (zeta + root);
        const VehicleScalar denom = r1 - r2;
        const VehicleScalar a1 = (v0 - r2 * y0) / denom;
        const VehicleScalar a2 = y0 - a1;
        const VehicleScalar e1 = std::exp(r1 * dt);
        const VehicleScalar e2 = std::exp(r2 * dt);
        y1 = a1 * e1 + a2 * e2;
        v1 = a1 * r1 * e1 + a2 * r2 * e2;
    }

    position = target + y1;
    velocity = v1;
}

void clampMode(
    VehicleScalar maximumOffset,
    VehicleScalar& position,
    VehicleScalar& velocity)
{
    const VehicleScalar limit = std::max(maximumOffset, VehicleScalar{0.001});
    if (position > limit)
    {
        position = limit;
        velocity = std::min(velocity, VehicleScalar{0.0});
    }
    else if (position < -limit)
    {
        position = -limit;
        velocity = std::max(velocity, VehicleScalar{0.0});
    }
}

} // namespace

bool validTireRigidRingDescription(const TireRigidRingDescription& d)
{
    if (!d.enabled)
        return true;

    const VehicleScalar fLong = effectiveFrequency(
        d.longitudinalFrequencyHz, d.longitudinalStiffnessNPerM, d.beltMassKg);
    const VehicleScalar fLat = effectiveFrequency(
        d.lateralFrequencyHz, d.lateralStiffnessNPerM, d.beltMassKg);
    const VehicleScalar fRad = d.radialFrequencyHz > 0.1
        ? d.radialFrequencyHz : fLong;

    const bool yawRequested = d.yawFrequencyHz > 0.1
        || d.yawStiffnessNmPerRad > 1.0;
    const bool windupRequested = d.windupFrequencyHz > 0.1;
    const VehicleScalar fYaw = yawRequested
        ? effectiveFrequency(
            d.yawFrequencyHz,
            d.yawStiffnessNmPerRad,
            d.beltDiametralInertiaKgM2)
        : 0.0;

    return finiteValue(d.longitudinalStiffnessNPerM)
        && finiteValue(d.lateralStiffnessNPerM)
        && finiteValue(d.yawStiffnessNmPerRad)
        && finiteValue(d.beltMassKg)
        && finiteValue(d.beltDiametralInertiaKgM2)
        && finiteValue(d.beltPolarInertiaKgM2)
        && d.longitudinalStiffnessNPerM >= 1000.0
        && d.lateralStiffnessNPerM >= 1000.0
        && d.beltMassKg > 0.05
        && d.beltMassKg < 250.0
        && finiteValue(fLong) && fLong >= 1.0 && fLong <= 250.0
        && finiteValue(fLat) && fLat >= 1.0 && fLat <= 250.0
        && finiteValue(fRad) && fRad >= 1.0 && fRad <= 250.0
        && (!yawRequested || (d.beltDiametralInertiaKgM2 > 1.0e-5
            && finiteValue(fYaw) && fYaw >= 1.0 && fYaw <= 250.0))
        && (!windupRequested || (d.beltPolarInertiaKgM2 > 1.0e-5
            && d.windupFrequencyHz >= 1.0 && d.windupFrequencyHz <= 250.0))
        && finiteValue(d.longitudinalDampingRatio)
        && finiteValue(d.lateralDampingRatio)
        && finiteValue(d.radialDampingRatio)
        && finiteValue(d.yawDampingRatio)
        && finiteValue(d.windupDampingRatio)
        && d.longitudinalDampingRatio >= 0.0 && d.longitudinalDampingRatio <= 5.0
        && d.lateralDampingRatio >= 0.0 && d.lateralDampingRatio <= 5.0
        && d.radialDampingRatio >= 0.0 && d.radialDampingRatio <= 5.0
        && d.yawDampingRatio >= 0.0 && d.yawDampingRatio <= 5.0
        && d.windupDampingRatio >= 0.0 && d.windupDampingRatio <= 5.0
        && finiteValue(d.residualDampingRatio)
        && finiteValue(d.lowSpeedAdditionalDampingRatio)
        && finiteValue(d.lowSpeedDampingScale)
        && finiteValue(d.lowSpeedThresholdMps)
        && finiteValue(d.maximumLongitudinalOffsetM)
        && finiteValue(d.maximumLateralOffsetM)
        && finiteValue(d.maximumRadialOffsetM)
        && finiteValue(d.maximumYawAngleRadians)
        && finiteValue(d.maximumWindupAngleRadians)
        && d.maximumYawAngleRadians > 0.001 && d.maximumYawAngleRadians <= 1.0
        && d.maximumWindupAngleRadians > 0.001 && d.maximumWindupAngleRadians <= 1.0;
}

TireRigidRingOutput advanceTireRigidRing(
    const TireRigidRingDescription& d,
    const TireRigidRingInput& input,
    TireRigidRingState& state)
{
    TireRigidRingOutput out;
    if (!d.enabled || !validTireRigidRingDescription(d)
        || !finiteValue(input.deltaTimeSeconds)
        || !finiteValue(input.forwardSpeedMps)
        || !finiteValue(input.roadRadialOffsetM)
        || !finiteValue(input.longitudinalForceN)
        || !finiteValue(input.lateralForceN)
        || !finiteValue(input.inflationPressurePa)
        || !finiteValue(input.referencePressurePa)
        || !finiteValue(input.thermalStiffnessScale)
        || !finiteValue(input.aligningMomentNm)
        || !finiteValue(input.longitudinalReactionMomentNm))
    {
        return out;
    }

    if (!state.initialized)
    {
        state.initialized = true;
        state.radialOffsetM = input.roadRadialOffsetM;
    }

    // TIRE32: structural displacement is the physical visual authority.
    // The imported [STRUCTURAL] stiffness is referenced to its dataset pressure.
    // Live gauge pressure changes pneumatic carcass tension; temperature supplies
    // the already-simulated carcass stiffness scale.  This reduced-order pressure
    // law is intentionally parameter-driven rather than using radial deflection as
    // a proxy for lateral compliance.
    const VehicleScalar referencePressurePa = input.referencePressurePa > 20000.0
        ? input.referencePressurePa : input.inflationPressurePa;
    const VehicleScalar pressureRatio = referencePressurePa > 20000.0
        ? std::clamp(input.inflationPressurePa / referencePressurePa,
            VehicleScalar{0.20}, VehicleScalar{3.0})
        : VehicleScalar{1.0};
    const VehicleScalar pressureStiffnessScale = std::sqrt(pressureRatio);
    const VehicleScalar thermalStiffnessScale = std::clamp(
        input.thermalStiffnessScale > 0.0
            ? input.thermalStiffnessScale : VehicleScalar{1.0},
        VehicleScalar{0.50}, VehicleScalar{1.50});
    const VehicleScalar structuralStiffnessScale = std::clamp(
        pressureStiffnessScale * thermalStiffnessScale,
        VehicleScalar{0.30}, VehicleScalar{2.00});

    const VehicleScalar longStiffness = d.longitudinalStiffnessNPerM
        * structuralStiffnessScale;
    const VehicleScalar latStiffness = d.lateralStiffnessNPerM
        * structuralStiffnessScale;
    const VehicleScalar modalFrequencyScale = std::sqrt(structuralStiffnessScale);
    const VehicleScalar baseLongFrequency = effectiveFrequency(
        d.longitudinalFrequencyHz, d.longitudinalStiffnessNPerM, d.beltMassKg);
    const VehicleScalar baseLatFrequency = effectiveFrequency(
        d.lateralFrequencyHz, d.lateralStiffnessNPerM, d.beltMassKg);
    const VehicleScalar fLong = baseLongFrequency * modalFrequencyScale;
    const VehicleScalar fLat = baseLatFrequency * modalFrequencyScale;
    const VehicleScalar fRad = (d.radialFrequencyHz > 0.1
        ? d.radialFrequencyHz : baseLongFrequency) * modalFrequencyScale;

    const VehicleScalar zLong = lowSpeedDamping(
        d, d.longitudinalDampingRatio, input.forwardSpeedMps);
    const VehicleScalar zLat = lowSpeedDamping(
        d, d.lateralDampingRatio, input.forwardSpeedMps);
    const VehicleScalar zRad = lowSpeedDamping(
        d, d.radialDampingRatio, input.forwardSpeedMps);
    const VehicleScalar zYaw = lowSpeedDamping(
        d, d.yawDampingRatio, input.forwardSpeedMps);
    const VehicleScalar zWindup = lowSpeedDamping(
        d, d.windupDampingRatio, input.forwardSpeedMps);

    const VehicleScalar targetLong = std::clamp(
        input.longitudinalForceN / std::max(longStiffness, kEpsilon),
        -d.maximumLongitudinalOffsetM,
        d.maximumLongitudinalOffsetM);
    const VehicleScalar targetLat = std::clamp(
        input.lateralForceN / std::max(latStiffness, kEpsilon),
        -d.maximumLateralOffsetM,
        d.maximumLateralOffsetM);
    const VehicleScalar targetRad = std::clamp(
        input.roadRadialOffsetM,
        -d.maximumRadialOffsetM,
        d.maximumRadialOffsetM);

    advanceSecondOrder(targetLong, fLong, zLong, input.deltaTimeSeconds,
        state.longitudinalOffsetM, state.longitudinalVelocityMps);
    advanceSecondOrder(targetLat, fLat, zLat, input.deltaTimeSeconds,
        state.lateralOffsetM, state.lateralVelocityMps);
    advanceSecondOrder(targetRad, fRad, zRad, input.deltaTimeSeconds,
        state.radialOffsetM, state.radialVelocityMps);

    const bool yawEnabled = d.yawFrequencyHz > 0.1
        || d.yawStiffnessNmPerRad > 1.0;
    if (yawEnabled)
    {
        const VehicleScalar yawStiffness = d.yawStiffnessNmPerRad > 1.0
            ? d.yawStiffnessNmPerRad
            : stiffnessFromFrequency(
                d.yawFrequencyHz, d.beltDiametralInertiaKgM2);
        const VehicleScalar yawFrequency = effectiveFrequency(
            d.yawFrequencyHz, yawStiffness, d.beltDiametralInertiaKgM2);
        const VehicleScalar targetYaw = yawStiffness > 1.0e-6
            ? std::clamp(
                input.aligningMomentNm / yawStiffness,
                -d.maximumYawAngleRadians,
                d.maximumYawAngleRadians)
            : VehicleScalar{0.0};
        advanceSecondOrder(targetYaw, yawFrequency, zYaw,
            input.deltaTimeSeconds,
            state.yawAngleRadians, state.yawAngularVelocityRadPerS);
        clampMode(d.maximumYawAngleRadians,
            state.yawAngleRadians, state.yawAngularVelocityRadPerS);
    }
    else
    {
        state.yawAngleRadians = 0.0;
        state.yawAngularVelocityRadPerS = 0.0;
    }

    const bool windupEnabled = d.windupFrequencyHz > 0.1
        && d.beltPolarInertiaKgM2 > 1.0e-5;
    if (windupEnabled)
    {
        const VehicleScalar windupStiffness = stiffnessFromFrequency(
            d.windupFrequencyHz, d.beltPolarInertiaKgM2);
        const VehicleScalar targetWindup = windupStiffness > 1.0e-6
            ? std::clamp(
                input.longitudinalReactionMomentNm / windupStiffness,
                -d.maximumWindupAngleRadians,
                d.maximumWindupAngleRadians)
            : VehicleScalar{0.0};
        advanceSecondOrder(targetWindup, d.windupFrequencyHz, zWindup,
            input.deltaTimeSeconds,
            state.windupAngleRadians, state.windupAngularVelocityRadPerS);
        clampMode(d.maximumWindupAngleRadians,
            state.windupAngleRadians, state.windupAngularVelocityRadPerS);
    }
    else
    {
        state.windupAngleRadians = 0.0;
        state.windupAngularVelocityRadPerS = 0.0;
    }

    clampMode(d.maximumLongitudinalOffsetM,
        state.longitudinalOffsetM, state.longitudinalVelocityMps);
    clampMode(d.maximumLateralOffsetM,
        state.lateralOffsetM, state.lateralVelocityMps);
    clampMode(d.maximumRadialOffsetM,
        state.radialOffsetM, state.radialVelocityMps);

    out.valid = true;
    out.longitudinalOffsetM = state.longitudinalOffsetM;
    out.longitudinalVelocityMps = state.longitudinalVelocityMps;
    out.lateralOffsetM = state.lateralOffsetM;
    out.lateralVelocityMps = state.lateralVelocityMps;
    out.radialOffsetM = state.radialOffsetM;
    out.radialVelocityMps = state.radialVelocityMps;
    out.yawAngleRadians = state.yawAngleRadians;
    out.yawAngularVelocityRadPerS = state.yawAngularVelocityRadPerS;
    out.windupAngleRadians = state.windupAngleRadians;
    out.windupAngularVelocityRadPerS = state.windupAngularVelocityRadPerS;
    return out;
}

void relaxTireRigidRingAirborne(
    const TireRigidRingDescription& d,
    VehicleScalar dt,
    TireRigidRingState& state)
{
    if (!state.initialized || dt <= 0.0)
        return;

    TireRigidRingInput input;
    input.deltaTimeSeconds = dt;
    input.forwardSpeedMps = 0.0;
    input.roadRadialOffsetM = 0.0;
    advanceTireRigidRing(d, input, state);

    if (std::abs(state.longitudinalOffsetM) < 1.0e-7
        && std::abs(state.lateralOffsetM) < 1.0e-7
        && std::abs(state.radialOffsetM) < 1.0e-7
        && std::abs(state.longitudinalVelocityMps) < 1.0e-6
        && std::abs(state.lateralVelocityMps) < 1.0e-6
        && std::abs(state.radialVelocityMps) < 1.0e-6
        && std::abs(state.yawAngleRadians) < 1.0e-7
        && std::abs(state.yawAngularVelocityRadPerS) < 1.0e-6
        && std::abs(state.windupAngleRadians) < 1.0e-7
        && std::abs(state.windupAngularVelocityRadPerS) < 1.0e-6)
    {
        state = {};
    }
}

} // namespace heritage::vehicles::tires

#include "TireContactGeometry.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles::tires {
namespace {

constexpr VehicleScalar kEpsilon = 1.0e-9;
constexpr VehicleScalar kPi = 3.14159265358979323846;

bool finiteValue(VehicleScalar value)
{
    return std::isfinite(static_cast<double>(value));
}

VehicleScalar clampDeflection(
    const TireContactGeometryDescription& d,
    VehicleScalar deflectionM,
    VehicleScalar freeRadiusM)
{
    const VehicleScalar minimumLoadedRadius = d.rimRadiusM > 0.01
        ? d.rimRadiusM + 0.002
        : freeRadiusM * 0.55;
    const VehicleScalar maximumDeflection = std::max(
        freeRadiusM - minimumLoadedRadius,
        VehicleScalar{0.0});
    return std::clamp(deflectionM, VehicleScalar{0.0}, maximumDeflection);
}

} // namespace

bool validTireContactGeometryDescription(
    const TireContactGeometryDescription& d)
{
    if (!finiteValue(d.unloadedRadiusM)
        || d.unloadedRadiusM <= 0.05
        || d.unloadedRadiusM > 2.5
        || !finiteValue(d.nominalLoadN)
        || d.nominalLoadN < 100.0
        || d.nominalLoadN > 250000.0
        || !finiteValue(d.verticalStiffnessNPerM)
        || d.verticalStiffnessNPerM < 1000.0
        || d.verticalStiffnessNPerM > 10000000.0
        || !finiteValue(d.nominalWidthM)
        || d.nominalWidthM <= 0.03
        || d.nominalWidthM > 1.5
        || !finiteValue(d.rimRadiusM)
        || d.rimRadiusM < 0.0
        || d.rimRadiusM >= d.unloadedRadiusM
        || !finiteValue(d.referenceSpeedMps)
        || d.referenceSpeedMps <= 0.1
        || d.referenceSpeedMps > 200.0)
    {
        return false;
    }

    if (d.useMagicFormulaEffectiveRadius)
    {
        if (!finiteValue(d.bReff) || d.bReff <= 0.0 || d.bReff > 100.0
            || !finiteValue(d.dReff) || d.dReff < 0.0 || d.dReff > 2.0
            || !finiteValue(d.fReff) || d.fReff < -2.0 || d.fReff > 2.0
            || !finiteValue(d.qRe0) || d.qRe0 <= 0.5 || d.qRe0 > 1.5
            || !finiteValue(d.qV1) || d.qV1 < -0.10 || d.qV1 > 0.10)
        {
            return false;
        }
    }

    if (d.useMagicFormulaContactLength)
    {
        if (!finiteValue(d.qRa1) || d.qRa1 < 0.0 || d.qRa1 > 10.0
            || !finiteValue(d.qRa2) || d.qRa2 < -10.0 || d.qRa2 > 10.0)
        {
            return false;
        }
    }

    return finiteValue(d.qRb1) && finiteValue(d.qRb2);
}

TireContactGeometryOutput evaluateTireContactGeometry(
    const TireContactGeometryDescription& d,
    const TireContactGeometryInput& input)
{
    TireContactGeometryOutput out;
    if (!validTireContactGeometryDescription(d)
        || !finiteValue(input.normalLoadN)
        || !finiteValue(input.wheelAngularVelocityRadPerS)
        || !finiteValue(input.inflationPressurePa)
        || !finiteValue(input.verticalDeflectionM))
    {
        return out;
    }

    const VehicleScalar fz = std::max(input.normalLoadN, VehicleScalar{0.0});
    const VehicleScalar omega = input.wheelAngularVelocityRadPerS;
    const VehicleScalar speedRatio =
        omega * d.unloadedRadiusM / d.referenceSpeedMps;

    // Public MF rolling-radius relation: the free rolling radius grows with
    // rotational speed before the separate load-dependent reduction is
    // applied. Q_RE0=1 and Q_V1=0 are the neutral values.
    VehicleScalar freeRadius = d.unloadedRadiusM;
    if (d.useMagicFormulaEffectiveRadius)
    {
        freeRadius = d.unloadedRadiusM
            * (d.qRe0 + d.qV1 * speedRatio * speedRatio);
    }
    freeRadius = std::clamp(
        freeRadius,
        d.rimRadiusM + VehicleScalar{0.002},
        d.unloadedRadiusM * VehicleScalar{1.20});

    VehicleScalar deflection = 0.0;
    if (fz > kEpsilon)
    {
        deflection = input.verticalDeflectionKnown
            ? std::max(input.verticalDeflectionM, VehicleScalar{0.0})
            : fz / d.verticalStiffnessNPerM;
    }
    deflection = clampDeflection(d, deflection, freeRadius);

    out.freeRollingRadiusM = freeRadius;
    out.verticalDeflectionM = deflection;
    out.loadedRadiusM = freeRadius - deflection;
    out.normalizedDeflection = deflection
        / std::max(d.unloadedRadiusM, VehicleScalar{0.05});

    VehicleScalar effectiveRadius = out.loadedRadiusM;
    if (fz > kEpsilon && d.useMagicFormulaEffectiveRadius)
    {
        const VehicleScalar loadRatio = fz / d.nominalLoadN;
        const VehicleScalar nominalDeflection =
            d.nominalLoadN / d.verticalStiffnessNPerM;
        effectiveRadius = freeRadius - nominalDeflection
            * (d.dReff * std::atan(d.bReff * loadRatio)
                + d.fReff * loadRatio);
    }
    out.effectiveRollingRadiusM = std::clamp(
        effectiveRadius,
        out.loadedRadiusM,
        out.freeRollingRadiusM);

    if (fz > kEpsilon && deflection > kEpsilon)
    {
        VehicleScalar contactLength = 0.0;
        if (d.useMagicFormulaContactLength)
        {
            const VehicleScalar normalized = std::max(
                out.normalizedDeflection, VehicleScalar{0.0});
            const VehicleScalar halfLength = d.unloadedRadiusM
                * (d.qRa1 * std::sqrt(normalized)
                    + d.qRa2 * normalized);
            contactLength = 2.0 * std::max(halfLength, VehicleScalar{0.0});
        }
        else
        {
            // Neutral finite-footprint fallback: circular chord generated by
            // the current radial deflection. This is geometry, not a claim of
            // proprietary SWIFT enveloping parity.
            const VehicleScalar chordSquared = std::max(
                VehicleScalar{2.0} * freeRadius * deflection
                    - deflection * deflection,
                VehicleScalar{0.0});
            contactLength = 2.0 * std::sqrt(chordSquared);
        }

        contactLength = std::clamp(
            contactLength,
            VehicleScalar{0.001},
            d.unloadedRadiusM * VehicleScalar{1.50});

        // A pneumatic footprint carries most of Fz through inflation pressure.
        // Use that first-order physical area to reconstruct an elliptical width.
        // Structural carcass effects and the Q_RB relation stay isolated for the
        // later rigid-ring/contact implementation rather than being guessed here.
        const VehicleScalar pressure = std::max(
            input.inflationPressurePa, VehicleScalar{50000.0});
        const VehicleScalar targetArea = fz / pressure;
        VehicleScalar contactWidth = 4.0 * targetArea
            / std::max(kPi * contactLength, VehicleScalar{0.001});
        contactWidth = std::clamp(
            contactWidth,
            d.nominalWidthM * VehicleScalar{0.15},
            d.nominalWidthM);

        out.contactPatchLengthM = contactLength;
        out.contactPatchWidthM = contactWidth;
        out.contactPatchAreaM2 = kPi * VehicleScalar{0.25}
            * contactLength * contactWidth;
    }

    out.valid = true;
    return out;
}

} // namespace heritage::vehicles::tires

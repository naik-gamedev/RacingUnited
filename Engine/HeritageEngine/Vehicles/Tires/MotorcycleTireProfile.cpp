#include "MotorcycleTireProfile.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles::tires {
namespace {

constexpr VehicleScalar kEpsilon = 1.0e-9;

bool finiteValue(VehicleScalar value)
{
    return std::isfinite(static_cast<double>(value));
}

} // namespace

bool validMotorcycleTireProfile(
    const MotorcycleTireProfileDescription& d,
    VehicleScalar unloadedRadiusM)
{
    if (!finiteValue(d.tireWidthM)
        || !finiteValue(d.mcContourA)
        || !finiteValue(d.mcContourB)
        || !finiteValue(unloadedRadiusM))
    {
        return false;
    }

    const VehicleScalar a = d.mcContourA * d.tireWidthM;
    const VehicleScalar b = d.mcContourB * d.tireWidthM;
    return d.tireWidthM > 0.02
        && d.tireWidthM < 0.80
        && d.mcContourA > 0.01
        && d.mcContourA <= 2.0
        && d.mcContourB > 0.01
        && d.mcContourB <= 2.0
        && unloadedRadiusM > b + 0.01
        && unloadedRadiusM < 1.5
        && a > kEpsilon
        && b > kEpsilon;
}

MotorcycleTireContactGeometry evaluateMotorcycleTireProfile(
    const MotorcycleTireProfileDescription& d,
    VehicleScalar unloadedRadiusM,
    VehicleScalar camberAngleRadians)
{
    MotorcycleTireContactGeometry out;
    if (!validMotorcycleTireProfile(d, unloadedRadiusM)
        || !finiteValue(camberAngleRadians))
    {
        return out;
    }

    const VehicleScalar a = d.mcContourA * d.tireWidthM;
    const VehicleScalar b = d.mcContourB * d.tireWidthM;
    const VehicleScalar baseRadius = unloadedRadiusM - b;
    const VehicleScalar sinGamma = std::sin(camberAngleRadians);
    const VehicleScalar cosGamma = std::cos(camberAngleRadians);

    // The road normal expressed in the tire cross-section has components
    // (sin(gamma), cos(gamma)). The support point of x^2/a^2 + y^2/b^2 = 1
    // opposite that normal gives the lowest contour point on a flat road.
    const VehicleScalar support = std::sqrt(
        a * a * sinGamma * sinGamma
        + b * b * cosGamma * cosGamma);
    if (support <= kEpsilon)
        return out;

    out.valid = true;
    out.lateralSemiAxisM = a;
    out.radialSemiAxisM = b;
    out.crownBaseRadiusM = baseRadius;
    out.lateralContactOffsetM = -(a * a * sinGamma) / support;
    out.radialContactOffsetM = -(b * b * cosGamma) / support;

    // Distance from wheel centre to the tangent road plane. At gamma=0 this
    // is exactly the unloaded radius; at lean it follows the rounded crown.
    out.centerToRoadM = baseRadius * cosGamma + support;
    out.centerToRoadM = std::max(out.centerToRoadM, 0.0);
    return out;
}

} // namespace heritage::vehicles::tires

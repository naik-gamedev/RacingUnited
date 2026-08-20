#include "SurfaceMaterialProperties.hpp"

#include "../CollisionSystem.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::physics {
namespace {

bool finite(double value)
{
    return std::isfinite(value);
}

SurfaceDeformableProperties sandProperties()
{
    SurfaceDeformableProperties p;
    p.enabled = true;
    p.densityKgM3 = 1580.0;
    p.initialLooseDepthM = 0.40;
    p.initialMoisture = 0.05;
    p.bekkerKc = 900.0;
    p.bekkerKphi = 1.20e6;
    p.sinkageExponent = 1.10;
    p.cohesionPa = 350.0;
    p.frictionAngleDegrees = 34.0;
    p.shearDeformationModulusM = 0.030;
    p.compactionStiffnessGain = 2.4;
    p.compactionShearGain = 0.35;
    p.plasticRutFraction = 0.72;
    p.compactionRateHz = 0.55;
    p.looseDepthLossPerCompactionM = 0.09;
    p.mfBaseFrictionScale = 0.08;
    p.baseStiffnessScale = 0.24;
    p.rollingResistanceScale = 3.0;
    p.relaxationScale = 2.2;
    return p;
}

SurfaceDeformableProperties mudProperties()
{
    SurfaceDeformableProperties p;
    p.enabled = true;
    p.densityKgM3 = 1500.0;
    p.initialLooseDepthM = 0.30;
    p.initialMoisture = 0.88;
    p.bekkerKc = 4500.0;
    p.bekkerKphi = 0.55e6;
    p.sinkageExponent = 1.18;
    p.cohesionPa = 6500.0;
    p.frictionAngleDegrees = 16.0;
    p.shearDeformationModulusM = 0.038;
    p.compactionStiffnessGain = 1.4;
    p.compactionShearGain = 0.10;
    p.plasticRutFraction = 0.85;
    p.compactionRateHz = 0.40;
    p.looseDepthLossPerCompactionM = 0.04;
    p.mfBaseFrictionScale = 0.06;
    p.baseStiffnessScale = 0.18;
    p.rollingResistanceScale = 4.2;
    p.relaxationScale = 2.5;
    return p;
}

SurfaceDeformableProperties softSoilProperties()
{
    SurfaceDeformableProperties p;
    p.enabled = true;
    p.densityKgM3 = 1680.0;
    p.initialLooseDepthM = 0.32;
    p.initialMoisture = 0.30;
    p.bekkerKc = 3200.0;
    p.bekkerKphi = 1.00e6;
    p.sinkageExponent = 1.05;
    p.cohesionPa = 3200.0;
    p.frictionAngleDegrees = 27.0;
    p.shearDeformationModulusM = 0.022;
    p.compactionStiffnessGain = 2.8;
    p.compactionShearGain = 0.40;
    p.plasticRutFraction = 0.72;
    p.compactionRateHz = 0.85;
    p.looseDepthLossPerCompactionM = 0.07;
    p.mfBaseFrictionScale = 0.10;
    p.baseStiffnessScale = 0.28;
    p.rollingResistanceScale = 3.1;
    p.relaxationScale = 2.1;
    return p;
}

SurfaceDeformableProperties deepSnowProperties()
{
    SurfaceDeformableProperties p;
    p.enabled = true;
    p.densityKgM3 = 320.0;
    p.initialLooseDepthM = 0.48;
    p.initialMoisture = 0.08;
    p.bekkerKc = 600.0;
    p.bekkerKphi = 0.30e6;
    p.sinkageExponent = 1.25;
    p.cohesionPa = 1200.0;
    p.frictionAngleDegrees = 20.0;
    p.shearDeformationModulusM = 0.045;
    p.compactionStiffnessGain = 4.0;
    p.compactionShearGain = 0.55;
    p.plasticRutFraction = 0.92;
    p.compactionRateHz = 1.25;
    p.looseDepthLossPerCompactionM = 0.15;
    p.mfBaseFrictionScale = 0.07;
    p.baseStiffnessScale = 0.18;
    p.rollingResistanceScale = 4.8;
    p.relaxationScale = 2.8;
    return p;
}

SurfaceHydrologyProperties hydrologyProperties(SurfaceMaterial material)
{
    SurfaceHydrologyProperties p;
    switch (material)
    {
    case SurfaceMaterial::Gravel:
        p.infiltrationCapacityMmPerHour = 28.0;
        p.flowRoughness = 0.060;
        p.depressionStorageMm = 1.20;
        break;
    case SurfaceMaterial::Dirt:
        p.infiltrationCapacityMmPerHour = 6.0;
        p.flowRoughness = 0.085;
        p.depressionStorageMm = 1.80;
        break;
    case SurfaceMaterial::Grass:
        p.infiltrationCapacityMmPerHour = 14.0;
        p.flowRoughness = 0.110;
        p.depressionStorageMm = 2.50;
        break;
    case SurfaceMaterial::Mud:
        p.infiltrationCapacityMmPerHour = 0.8;
        p.flowRoughness = 0.140;
        p.depressionStorageMm = 3.0;
        break;
    case SurfaceMaterial::Sand:
        p.infiltrationCapacityMmPerHour = 40.0;
        p.flowRoughness = 0.090;
        p.depressionStorageMm = 2.0;
        break;
    case SurfaceMaterial::SoftSoil:
        p.infiltrationCapacityMmPerHour = 9.0;
        p.flowRoughness = 0.120;
        p.depressionStorageMm = 2.5;
        break;
    case SurfaceMaterial::Snow:
    case SurfaceMaterial::DeepSnow:
        p.infiltrationCapacityMmPerHour = 1.0;
        p.flowRoughness = 0.100;
        p.depressionStorageMm = 2.0;
        break;
    case SurfaceMaterial::Ice:
        p.infiltrationCapacityMmPerHour = 0.0;
        p.flowRoughness = 0.010;
        p.depressionStorageMm = 0.05;
        break;
    case SurfaceMaterial::Kerb:
        p.infiltrationCapacityMmPerHour = 0.03;
        p.flowRoughness = 0.025;
        p.depressionStorageMm = 0.15;
        break;
    case SurfaceMaterial::PaintedLine:
        p.infiltrationCapacityMmPerHour = 0.01;
        p.flowRoughness = 0.012;
        p.depressionStorageMm = 0.08;
        break;
    case SurfaceMaterial::Default:
    case SurfaceMaterial::Asphalt:
    default:
        // Dense conventional asphalt: almost all meaningful removal comes
        // from camber/runoff, drains and evaporation, not rapid absorption.
        p.infiltrationCapacityMmPerHour = 0.15;
        p.flowRoughness = 0.020;
        p.depressionStorageMm = 0.20;
        break;
    }
    return p;
}

} // namespace

bool deformableSurfaceMaterial(SurfaceMaterial material)
{
    return material == SurfaceMaterial::Mud
        || material == SurfaceMaterial::Sand
        || material == SurfaceMaterial::SoftSoil
        || material == SurfaceMaterial::DeepSnow;
}

double defaultSurfaceTemperatureC(SurfaceMaterial material)
{
    return material == SurfaceMaterial::Snow
            || material == SurfaceMaterial::Ice
            || material == SurfaceMaterial::DeepSnow
        ? -5.0
        : 20.0;
}

SurfaceMaterialProperties defaultSurfaceMaterialProperties(SurfaceMaterial material)
{
    SurfaceMaterialProperties result;
    result.hydrology = hydrologyProperties(material);
    switch (material)
    {
    case SurfaceMaterial::Mud:
        result.deformable = mudProperties();
        break;
    case SurfaceMaterial::Sand:
        result.deformable = sandProperties();
        break;
    case SurfaceMaterial::SoftSoil:
        result.deformable = softSoilProperties();
        break;
    case SurfaceMaterial::DeepSnow:
        result.deformable = deepSnowProperties();
        break;
    default:
        result.deformable.enabled = false;
        break;
    }
    result.authoredSurfaceTemperatureC = defaultSurfaceTemperatureC(material);
    return result;
}

bool validSurfaceDeformableProperties(const SurfaceDeformableProperties& p)
{
    if (!p.enabled)
        return true;

    const double values[] = {
        p.densityKgM3,
        p.initialLooseDepthM,
        p.initialMoisture,
        p.bekkerKc,
        p.bekkerKphi,
        p.sinkageExponent,
        p.cohesionPa,
        p.frictionAngleDegrees,
        p.shearDeformationModulusM,
        p.compactionStiffnessGain,
        p.compactionShearGain,
        p.plasticRutFraction,
        p.compactionRateHz,
        p.looseDepthLossPerCompactionM,
        p.mfBaseFrictionScale,
        p.baseStiffnessScale,
        p.rollingResistanceScale,
        p.relaxationScale
    };
    for (double value : values)
    {
        if (!finite(value))
            return false;
    }

    return p.densityKgM3 >= 25.0 && p.densityKgM3 <= 5000.0
        && p.initialLooseDepthM >= 0.0 && p.initialLooseDepthM <= 2.0
        && p.initialMoisture >= 0.0 && p.initialMoisture <= 1.0
        && p.bekkerKc >= 0.0 && p.bekkerKc <= 1.0e8
        && p.bekkerKphi >= 0.0 && p.bekkerKphi <= 1.0e9
        && p.sinkageExponent >= 0.20 && p.sinkageExponent <= 4.0
        && p.cohesionPa >= 0.0 && p.cohesionPa <= 2.0e6
        && p.frictionAngleDegrees >= 0.0 && p.frictionAngleDegrees <= 60.0
        && p.shearDeformationModulusM >= 0.001 && p.shearDeformationModulusM <= 1.0
        && p.compactionStiffnessGain >= 0.0 && p.compactionStiffnessGain <= 20.0
        && p.compactionShearGain >= 0.0 && p.compactionShearGain <= 10.0
        && p.plasticRutFraction >= 0.0 && p.plasticRutFraction <= 1.0
        && p.compactionRateHz >= 0.0 && p.compactionRateHz <= 20.0
        && p.looseDepthLossPerCompactionM >= 0.0
        && p.looseDepthLossPerCompactionM <= 2.0
        && p.mfBaseFrictionScale >= 0.0 && p.mfBaseFrictionScale <= 1.0
        && p.baseStiffnessScale >= 0.0 && p.baseStiffnessScale <= 2.0
        && p.rollingResistanceScale >= 0.0 && p.rollingResistanceScale <= 20.0
        && p.relaxationScale >= 0.1 && p.relaxationScale <= 20.0;
}

bool validSurfaceMaterialProperties(const SurfaceMaterialProperties& value)
{
    const SurfaceHydrologyProperties& h = value.hydrology;
    const bool validHydrology = finite(h.infiltrationCapacityMmPerHour)
        && h.infiltrationCapacityMmPerHour >= 0.0
        && h.infiltrationCapacityMmPerHour <= 1000.0
        && finite(h.drainageCapacityMmPerHour)
        && h.drainageCapacityMmPerHour >= 0.0
        && h.drainageCapacityMmPerHour <= 100000.0
        && finite(h.flowRoughness)
        && h.flowRoughness >= 0.001 && h.flowRoughness <= 1.0
        && finite(h.depressionStorageMm)
        && h.depressionStorageMm >= 0.0
        && h.depressionStorageMm <= 100.0;
    return validHydrology
        && validSurfaceDeformableProperties(value.deformable)
        && (!value.hasAuthoredSurfaceTemperature
            || (finite(value.authoredSurfaceTemperatureC)
                && value.authoredSurfaceTemperatureC >= -100.0
                && value.authoredSurfaceTemperatureC <= 150.0));
}

SurfaceDeformableProperties blendSurfaceDeformableProperties(
    const SurfaceDeformableProperties* values,
    const double* weights,
    std::size_t count)
{
    SurfaceDeformableProperties out;
    if (!values || !weights || count == 0)
        return out;

    double total = 0.0;
    bool authored = false;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!values[i].enabled || weights[i] <= 0.0 || !std::isfinite(weights[i]))
            continue;
        total += weights[i];
        authored = authored || values[i].authored;
    }
    if (total <= 1.0e-12)
        return out;

    const auto mix = [&](double SurfaceDeformableProperties::*member) {
        double result = 0.0;
        for (std::size_t i = 0; i < count; ++i)
        {
            if (!values[i].enabled || weights[i] <= 0.0 || !std::isfinite(weights[i]))
                continue;
            result += values[i].*member * (weights[i] / total);
        }
        return result;
    };

    out.enabled = true;
    out.authored = authored;
    out.densityKgM3 = mix(&SurfaceDeformableProperties::densityKgM3);
    out.initialLooseDepthM = mix(&SurfaceDeformableProperties::initialLooseDepthM);
    out.initialMoisture = mix(&SurfaceDeformableProperties::initialMoisture);
    out.bekkerKc = mix(&SurfaceDeformableProperties::bekkerKc);
    out.bekkerKphi = mix(&SurfaceDeformableProperties::bekkerKphi);
    out.sinkageExponent = mix(&SurfaceDeformableProperties::sinkageExponent);
    out.cohesionPa = mix(&SurfaceDeformableProperties::cohesionPa);
    out.frictionAngleDegrees = mix(&SurfaceDeformableProperties::frictionAngleDegrees);
    out.shearDeformationModulusM = mix(&SurfaceDeformableProperties::shearDeformationModulusM);
    out.compactionStiffnessGain = mix(&SurfaceDeformableProperties::compactionStiffnessGain);
    out.compactionShearGain = mix(&SurfaceDeformableProperties::compactionShearGain);
    out.plasticRutFraction = mix(&SurfaceDeformableProperties::plasticRutFraction);
    out.compactionRateHz = mix(&SurfaceDeformableProperties::compactionRateHz);
    out.looseDepthLossPerCompactionM = mix(&SurfaceDeformableProperties::looseDepthLossPerCompactionM);
    out.mfBaseFrictionScale = mix(&SurfaceDeformableProperties::mfBaseFrictionScale);
    out.baseStiffnessScale = mix(&SurfaceDeformableProperties::baseStiffnessScale);
    out.rollingResistanceScale = mix(&SurfaceDeformableProperties::rollingResistanceScale);
    out.relaxationScale = mix(&SurfaceDeformableProperties::relaxationScale);
    return out;
}

} // namespace heritage::physics

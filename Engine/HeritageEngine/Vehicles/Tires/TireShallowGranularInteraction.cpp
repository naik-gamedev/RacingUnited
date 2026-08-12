#include "TireShallowGranularInteraction.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles::tires {
namespace {

constexpr VehicleScalar kPi = 3.1415926535897932384626433832795;
constexpr VehicleScalar kGravity = 9.81;

bool finiteValue(VehicleScalar value)
{
    return std::isfinite(static_cast<double>(value));
}

VehicleScalar clamp01(VehicleScalar value)
{
    return std::clamp(value, VehicleScalar{0.0}, VehicleScalar{1.0});
}

VehicleScalar radians(VehicleScalar degrees)
{
    return degrees * (kPi / VehicleScalar{180.0});
}

VehicleScalar degrees(VehicleScalar radiansValue)
{
    return radiansValue * (VehicleScalar{180.0} / kPi);
}

VehicleScalar signOf(VehicleScalar value)
{
    if (value > VehicleScalar{1.0e-8}) return VehicleScalar{1.0};
    if (value < VehicleScalar{-1.0e-8}) return VehicleScalar{-1.0};
    return VehicleScalar{0.0};
}

struct SurfacePreset
{
    VehicleScalar looseLayerDepthM = 0.0;
    VehicleScalar densityKgM3 = 0.0;
    VehicleScalar cohesionPa = 0.0;
    VehicleScalar frictionAngleRadians = 0.0;
    VehicleScalar shearDeformationModulusM = 0.01;

    // Reduced-order Bekker-like pressure/sinkage relationship:
    // pressure = sinkageModulus * sinkage^sinkageExponent.
    // TIRE15 can replace this with full terrain kc/kphi/b/n data once a
    // persistent SurfaceField exists.
    VehicleScalar sinkageModulusPaPerMExponent = 1.0;
    VehicleScalar sinkageExponent = 1.0;

    VehicleScalar baseFrictionScale = 0.45;
    VehicleScalar baseStiffnessScale = 0.60;
    VehicleScalar rollingResistanceScale = 1.50;
    VehicleScalar relaxationScale = 1.45;
    VehicleScalar wetShearLoss = 0.15;
    VehicleScalar wetSinkageGain = 0.20;
};

SurfacePreset gravelPreset()
{
    SurfacePreset value;
    value.looseLayerDepthM = 0.030;
    value.densityKgM3 = 1750.0;
    value.cohesionPa = 500.0;
    value.frictionAngleRadians = radians(38.0);
    value.shearDeformationModulusM = 0.014;
    value.sinkageModulusPaPerMExponent = 22.0e6;
    value.sinkageExponent = 1.10;
    value.baseFrictionScale = 0.40;
    value.baseStiffnessScale = 0.55;
    value.rollingResistanceScale = 1.55;
    value.relaxationScale = 1.45;
    value.wetShearLoss = 0.12;
    value.wetSinkageGain = 0.20;
    return value;
}

SurfacePreset dirtPreset()
{
    SurfacePreset value;
    value.looseLayerDepthM = 0.015;
    value.densityKgM3 = 1650.0;
    value.cohesionPa = 2500.0;
    value.frictionAngleRadians = radians(32.0);
    value.shearDeformationModulusM = 0.010;
    value.sinkageModulusPaPerMExponent = 30.0e6;
    value.sinkageExponent = 1.00;
    value.baseFrictionScale = 0.44;
    value.baseStiffnessScale = 0.58;
    value.rollingResistanceScale = 1.70;
    value.relaxationScale = 1.55;
    value.wetShearLoss = 0.25;
    value.wetSinkageGain = 0.35;
    return value;
}

SurfacePreset blendPreset(VehicleScalar gravelWeight, VehicleScalar dirtWeight)
{
    const VehicleScalar total = std::max(
        gravelWeight + dirtWeight, VehicleScalar{1.0e-9});
    gravelWeight /= total;
    dirtWeight /= total;
    const SurfacePreset gravel = gravelPreset();
    const SurfacePreset dirt = dirtPreset();
    const auto blend = [&](VehicleScalar SurfacePreset::*member) {
        return gravel.*member * gravelWeight + dirt.*member * dirtWeight;
    };

    SurfacePreset out;
    out.looseLayerDepthM = blend(&SurfacePreset::looseLayerDepthM);
    out.densityKgM3 = blend(&SurfacePreset::densityKgM3);
    out.cohesionPa = blend(&SurfacePreset::cohesionPa);
    out.frictionAngleRadians = blend(&SurfacePreset::frictionAngleRadians);
    out.shearDeformationModulusM = blend(&SurfacePreset::shearDeformationModulusM);
    out.sinkageModulusPaPerMExponent = blend(
        &SurfacePreset::sinkageModulusPaPerMExponent);
    out.sinkageExponent = blend(&SurfacePreset::sinkageExponent);
    out.baseFrictionScale = blend(&SurfacePreset::baseFrictionScale);
    out.baseStiffnessScale = blend(&SurfacePreset::baseStiffnessScale);
    out.rollingResistanceScale = blend(&SurfacePreset::rollingResistanceScale);
    out.relaxationScale = blend(&SurfacePreset::relaxationScale);
    out.wetShearLoss = blend(&SurfacePreset::wetShearLoss);
    out.wetSinkageGain = blend(&SurfacePreset::wetSinkageGain);
    return out;
}

VehicleScalar fallbackContactArea(const TireShallowGranularInput& input)
{
    if (input.contactPatchAreaM2 > VehicleScalar{1.0e-5})
        return input.contactPatchAreaM2;
    if (input.contactPatchLengthM > VehicleScalar{0.01}
        && input.contactPatchWidthM > VehicleScalar{0.01})
    {
        return input.contactPatchLengthM * input.contactPatchWidthM;
    }

    // Pneumatic tires on a load-bearing substrate tend to operate around a
    // pressure scale comparable to their inflation pressure. Use a bounded
    // engineering fallback only when TIRE04 has not produced footprint area.
    const VehicleScalar referencePressurePa = VehicleScalar{170000.0};
    return std::clamp(
        input.normalLoadN / referencePressurePa,
        VehicleScalar{0.006}, VehicleScalar{0.050});
}

VehicleScalar fallbackContactLength(
    const TireShallowGranularInput& input,
    VehicleScalar area)
{
    if (input.contactPatchLengthM > VehicleScalar{0.01})
        return input.contactPatchLengthM;
    const VehicleScalar width = input.contactPatchWidthM > VehicleScalar{0.02}
        ? input.contactPatchWidthM : VehicleScalar{0.20};
    return std::clamp(area / width, VehicleScalar{0.04}, VehicleScalar{0.30});
}

VehicleScalar fallbackContactWidth(
    const TireShallowGranularInput& input,
    VehicleScalar area,
    VehicleScalar length)
{
    if (input.contactPatchWidthM > VehicleScalar{0.02})
        return input.contactPatchWidthM;
    return std::clamp(area / std::max(length, VehicleScalar{0.02}),
        VehicleScalar{0.08}, VehicleScalar{0.45});
}

VehicleScalar treadDepthRatio(const TireShallowGranularInput& input)
{
    const VehicleScalar usable = std::max(
        input.initialTreadDepthM - input.minimumTreadDepthM,
        VehicleScalar{1.0e-6});
    return clamp01(
        (input.currentAverageTreadDepthM - input.minimumTreadDepthM) / usable);
}

VehicleScalar treadEffectiveness(
    const TireShallowGranularDescription& description,
    const TireShallowGranularInput& input)
{
    const VehicleScalar geometry = clamp01(
        VehicleScalar{0.35} * clamp01(description.treadAggressiveness)
        + VehicleScalar{0.30} * clamp01(description.treadEdgeDensity)
        + VehicleScalar{0.20} * clamp01(description.openVoidRatio)
        + VehicleScalar{0.15} * clamp01(description.bulldozingCoupling));
    const VehicleScalar depth = std::pow(
        treadDepthRatio(input),
        std::max(description.treadDepthEffectExponent, VehicleScalar{0.05}));
    const VehicleScalar depthEffect = description.minimumWornTreadEffectiveness
        + (VehicleScalar{1.0} - description.minimumWornTreadEffectiveness) * depth;
    return clamp01((VehicleScalar{0.35} + VehicleScalar{0.65} * geometry) * depthEffect);
}

} // namespace

bool validTireShallowGranularDescription(
    const TireShallowGranularDescription& d)
{
    return finiteValue(d.treadAggressiveness)
        && d.treadAggressiveness >= 0.0 && d.treadAggressiveness <= 1.0
        && finiteValue(d.treadEdgeDensity)
        && d.treadEdgeDensity >= 0.0 && d.treadEdgeDensity <= 1.0
        && finiteValue(d.openVoidRatio)
        && d.openVoidRatio >= 0.0 && d.openVoidRatio <= 1.0
        && finiteValue(d.granularShearCoupling)
        && d.granularShearCoupling >= 0.0 && d.granularShearCoupling <= 2.0
        && finiteValue(d.bulldozingCoupling)
        && d.bulldozingCoupling >= 0.0 && d.bulldozingCoupling <= 2.0
        && finiteValue(d.plowingCoupling)
        && d.plowingCoupling >= 0.0 && d.plowingCoupling <= 2.0
        && finiteValue(d.minimumWornTreadEffectiveness)
        && d.minimumWornTreadEffectiveness >= 0.0
        && d.minimumWornTreadEffectiveness <= 1.0
        && finiteValue(d.treadDepthEffectExponent)
        && d.treadDepthEffectExponent > 0.0 && d.treadDepthEffectExponent <= 4.0
        && finiteValue(d.maximumSinkageM)
        && d.maximumSinkageM >= 0.0 && d.maximumSinkageM <= 0.20
        && finiteValue(d.maximumGranularForceRatio)
        && d.maximumGranularForceRatio >= 0.0
        && d.maximumGranularForceRatio <= 2.0
        && finiteValue(d.maximumPlowingForceRatio)
        && d.maximumPlowingForceRatio >= 0.0
        && d.maximumPlowingForceRatio <= 2.0
        && finiteValue(d.minimumBaseFrictionScale)
        && finiteValue(d.maximumBaseFrictionScale)
        && d.minimumBaseFrictionScale >= 0.0
        && d.minimumBaseFrictionScale <= d.maximumBaseFrictionScale
        && d.maximumBaseFrictionScale <= 2.0;
}

TireShallowGranularOutput evaluateTireShallowGranular(
    const TireShallowGranularDescription& d,
    const TireShallowGranularInput& input)
{
    TireShallowGranularOutput out;
    if (!d.enabled || !validTireShallowGranularDescription(d) || !input.grounded)
        return out;

    using heritage::physics::SurfaceMaterial;
    VehicleScalar gravel = 0.0;
    VehicleScalar dirt = 0.0;
    VehicleScalar wetness = clamp01(input.surfaceWetness);
    if (input.footprintSurfaceBlendValid)
    {
        gravel = clamp01(input.footprintGravelFraction);
        dirt = clamp01(input.footprintDirtFraction);
        wetness = clamp01(input.footprintAverageWetness);
    }
    else if (input.surfaceMaterial == SurfaceMaterial::Gravel)
    {
        gravel = 1.0;
    }
    else if (input.surfaceMaterial == SurfaceMaterial::Dirt)
    {
        dirt = 1.0;
    }

    const VehicleScalar granularFraction = std::clamp(
        gravel + dirt, VehicleScalar{0.0}, VehicleScalar{1.0});
    if (granularFraction <= VehicleScalar{1.0e-6})
        return out;

    const SurfacePreset surface = blendPreset(gravel, dirt);
    const VehicleScalar load = std::max(input.normalLoadN, VehicleScalar{0.0});
    if (load <= VehicleScalar{1.0})
        return out;

    const VehicleScalar area = fallbackContactArea(input);
    const VehicleScalar length = fallbackContactLength(input, area);
    const VehicleScalar width = fallbackContactWidth(input, area, length);
    const VehicleScalar pressure = load / std::max(area, VehicleScalar{1.0e-6});

    const VehicleScalar wetSinkageMultiplier = VehicleScalar{1.0}
        + surface.wetSinkageGain * wetness;
    const VehicleScalar rawSinkage = std::pow(
        std::max(pressure / std::max(
            surface.sinkageModulusPaPerMExponent,
            VehicleScalar{1.0}), VehicleScalar{0.0}),
        VehicleScalar{1.0} / std::max(surface.sinkageExponent, VehicleScalar{0.2}))
        * wetSinkageMultiplier;
    const VehicleScalar sinkageLimit = std::min(
        surface.looseLayerDepthM,
        std::max(d.maximumSinkageM, VehicleScalar{0.0}));
    const VehicleScalar sinkage = std::clamp(
        rawSinkage, VehicleScalar{0.0}, sinkageLimit) * granularFraction;
    const VehicleScalar sinkageFraction = surface.looseLayerDepthM > VehicleScalar{1.0e-6}
        ? clamp01(sinkage / surface.looseLayerDepthM)
        : VehicleScalar{0.0};

    const VehicleScalar effectiveCohesion = surface.cohesionPa
        * (VehicleScalar{1.0} - VehicleScalar{0.35} * wetness);
    const VehicleScalar effectiveFrictionAngle = surface.frictionAngleRadians
        * (VehicleScalar{1.0} - VehicleScalar{0.12} * wetness);
    const VehicleScalar maximumShearStress = std::max(
        effectiveCohesion + pressure * std::tan(effectiveFrictionAngle),
        VehicleScalar{0.0});
    const VehicleScalar grossSoilShearCapacity = maximumShearStress
        * area * granularFraction;

    const VehicleScalar tread = treadEffectiveness(d, input);
    const VehicleScalar wetShearScale = std::clamp(
        VehicleScalar{1.0} - surface.wetShearLoss * wetness,
        VehicleScalar{0.45}, VehicleScalar{1.0});
    const VehicleScalar usableSoilCapacity = std::min(
        grossSoilShearCapacity
            * d.granularShearCoupling * tread * wetShearScale,
        load * d.maximumGranularForceRatio * granularFraction);

    const VehicleScalar longitudinalShearDisplacement = length
        * std::min(std::abs(input.slipRatio), VehicleScalar{2.5});
    const VehicleScalar clampedSlipAngle = std::clamp(
        input.slipAngleRadians, radians(VehicleScalar{-65.0}), radians(VehicleScalar{65.0}));
    const VehicleScalar lateralShearDisplacement = length
        * std::min(std::abs(std::tan(clampedSlipAngle)), VehicleScalar{3.5});
    const VehicleScalar shearModulus = std::max(
        surface.shearDeformationModulusM, VehicleScalar{0.001});
    const VehicleScalar longMobilization = clamp01(
        VehicleScalar{1.0} - std::exp(-longitudinalShearDisplacement / shearModulus));
    const VehicleScalar latMobilization = clamp01(
        VehicleScalar{1.0} - std::exp(-lateralShearDisplacement / shearModulus));

    const VehicleScalar longitudinalDirection = signOf(
        std::abs(input.longitudinalSlipVelocityMps) > VehicleScalar{1.0e-5}
            ? input.longitudinalSlipVelocityMps : input.slipRatio);
    const VehicleScalar lateralDirection = -signOf(
        std::abs(input.lateralSlipVelocityMps) > VehicleScalar{1.0e-5}
            ? input.lateralSlipVelocityMps : input.slipAngleRadians);

    const VehicleScalar longitudinalShearForce = longitudinalDirection
        * usableSoilCapacity * longMobilization;
    const VehicleScalar lateralShearForce = lateralDirection
        * usableSoilCapacity * latMobilization;

    // Clean-room passive-wedge bulldozing approximation. The functional form
    // follows the expected density*g*width*sinkage^2 scaling and uses the
    // passive earth-pressure coefficient Kp = tan^2(45deg + phi/2). It is not
    // claimed to reproduce Altair's proprietary implementation of Hegedus.
    const VehicleScalar passiveCoefficient = std::pow(
        std::tan(kPi * VehicleScalar{0.25} + effectiveFrictionAngle * VehicleScalar{0.5}),
        VehicleScalar{2.0});
    const VehicleScalar passiveWedgeForce = (
        VehicleScalar{0.5} * surface.densityKgM3 * kGravity
            * passiveCoefficient * width * sinkage * sinkage
        + VehicleScalar{2.0} * effectiveCohesion * width * sinkage
            * std::sqrt(std::max(passiveCoefficient, VehicleScalar{0.0})))
        * granularFraction;
    const VehicleScalar lateralBulldozing = lateralDirection
        * std::min(
            passiveWedgeForce * d.bulldozingCoupling * latMobilization,
            load * d.maximumGranularForceRatio * VehicleScalar{0.35});

    // A shallow load-bearing layer also consumes longitudinal work by
    // compacting/displacing particles. W*z/R is a compact reduced-order form
    // for compaction rolling resistance; the passive wedge adds the material
    // that must be pushed forward at finite sinkage.
    const VehicleScalar compactionResistance = load * sinkage
        / std::max(input.unloadedRadiusM, VehicleScalar{0.10});
    const VehicleScalar rawPlowing = (
        compactionResistance + passiveWedgeForce * VehicleScalar{0.45})
        * d.plowingCoupling;
    const VehicleScalar plowingDrag = std::min(
        std::max(rawPlowing, VehicleScalar{0.0}),
        load * d.maximumPlowingForceRatio * granularFraction);

    // As the tread reaches the load-bearing base, direct rubber/base response
    // grows somewhat. The loose material never disappears entirely in TIRE14.
    const VehicleScalar baseEngagement = VehicleScalar{0.72}
        + VehicleScalar{0.28} * sinkageFraction;
    const VehicleScalar localBaseFriction = std::clamp(
        surface.baseFrictionScale * baseEngagement
            * (VehicleScalar{1.0} - VehicleScalar{0.10} * wetness),
        d.minimumBaseFrictionScale,
        d.maximumBaseFrictionScale);
    const VehicleScalar localBaseStiffness = std::clamp(
        surface.baseStiffnessScale
            * (VehicleScalar{1.0} - VehicleScalar{0.12} * wetness),
        VehicleScalar{0.20}, VehicleScalar{1.0});

    out.valid = true;
    out.gravelFraction = gravel;
    out.dirtFraction = dirt;
    out.granularSurfaceFraction = granularFraction;
    out.surfaceWetness = wetness;
    out.looseLayerDepthM = surface.looseLayerDepthM;
    out.sinkageM = sinkage;
    out.sinkageFraction = sinkageFraction;
    out.contactPressurePa = pressure;
    out.effectiveCohesionPa = effectiveCohesion;
    out.effectiveFrictionAngleDegrees = degrees(effectiveFrictionAngle);
    out.longitudinalShearDisplacementM = longitudinalShearDisplacement;
    out.lateralShearDisplacementM = lateralShearDisplacement;
    out.longitudinalShearMobilization = longMobilization;
    out.lateralShearMobilization = latMobilization;
    out.soilShearCapacityN = grossSoilShearCapacity;
    out.treadEffectiveness = tread;
    out.longitudinalShearForceN = longitudinalShearForce;
    out.lateralShearForceN = lateralShearForce;
    out.lateralBulldozingForceN = lateralBulldozing;
    out.plowingDragN = plowingDrag;
    out.compactionPowerW = plowingDrag * std::abs(input.forwardSpeedMps);
    out.additionalContactCapacityN = std::min(
        usableSoilCapacity + std::abs(lateralBulldozing),
        load * d.maximumGranularForceRatio * granularFraction);
    out.frictionScale = VehicleScalar{1.0}
        + granularFraction * (localBaseFriction - VehicleScalar{1.0});
    out.stiffnessScale = VehicleScalar{1.0}
        + granularFraction * (localBaseStiffness - VehicleScalar{1.0});
    out.rollingResistanceScale = VehicleScalar{1.0}
        + granularFraction * (surface.rollingResistanceScale - VehicleScalar{1.0});
    out.relaxationScale = VehicleScalar{1.0}
        + granularFraction * (surface.relaxationScale - VehicleScalar{1.0});
    return out;
}

} // namespace heritage::vehicles::tires

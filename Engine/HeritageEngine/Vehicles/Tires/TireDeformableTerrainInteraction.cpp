#include "TireDeformableTerrainInteraction.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles::tires {
namespace {

constexpr VehicleScalar kPi = 3.1415926535897932384626433832795;
constexpr VehicleScalar kGravity = 9.80665;

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

using TerrainPreset = heritage::physics::SurfaceDeformableProperties;

TerrainPreset presetFor(heritage::physics::SurfaceMaterial material)
{
    return heritage::physics::defaultSurfaceMaterialProperties(material).deformable;
}

TerrainPreset blendPresets(
    VehicleScalar mud,
    VehicleScalar sand,
    VehicleScalar soil,
    VehicleScalar snow)
{
    const TerrainPreset values[] = {
        presetFor(heritage::physics::SurfaceMaterial::Mud),
        presetFor(heritage::physics::SurfaceMaterial::Sand),
        presetFor(heritage::physics::SurfaceMaterial::SoftSoil),
        presetFor(heritage::physics::SurfaceMaterial::DeepSnow)
    };
    const double weights[] = { mud, sand, soil, snow };
    return heritage::physics::blendSurfaceDeformableProperties(
        values, weights, 4u);
}

VehicleScalar fallbackArea(const TireDeformableTerrainInput& input)
{
    if (input.contactPatchAreaM2 > VehicleScalar{1.0e-5})
        return input.contactPatchAreaM2;
    if (input.contactPatchLengthM > VehicleScalar{0.01}
        && input.contactPatchWidthM > VehicleScalar{0.01})
    {
        return input.contactPatchLengthM * input.contactPatchWidthM;
    }
    return std::clamp(
        input.normalLoadN / VehicleScalar{180000.0},
        VehicleScalar{0.010}, VehicleScalar{0.060});
}

VehicleScalar fallbackLength(const TireDeformableTerrainInput& input, VehicleScalar area)
{
    if (input.contactPatchLengthM > VehicleScalar{0.01})
        return input.contactPatchLengthM;
    const VehicleScalar width = input.contactPatchWidthM > VehicleScalar{0.03}
        ? input.contactPatchWidthM : VehicleScalar{0.20};
    return std::clamp(area / width, VehicleScalar{0.06}, VehicleScalar{0.40});
}

VehicleScalar fallbackWidth(
    const TireDeformableTerrainInput& input,
    VehicleScalar area,
    VehicleScalar length)
{
    if (input.contactPatchWidthM > VehicleScalar{0.03})
        return input.contactPatchWidthM;
    return std::clamp(area / std::max(length, VehicleScalar{0.04}),
        VehicleScalar{0.08}, VehicleScalar{0.50});
}

VehicleScalar treadEffectiveness(
    const TireDeformableTerrainDescription& d,
    const TireDeformableTerrainInput& input)
{
    const VehicleScalar usableInitial = std::max(
        input.initialTreadDepthM - input.minimumTreadDepthM,
        VehicleScalar{1.0e-5});
    const VehicleScalar usableCurrent = std::max(
        input.currentAverageTreadDepthM - input.minimumTreadDepthM,
        VehicleScalar{0.0});
    const VehicleScalar depthFraction = clamp01(usableCurrent / usableInitial);
    const VehicleScalar depthEffect = std::pow(
        depthFraction, d.treadDepthEffectExponent);
    const VehicleScalar architecture = clamp01(
        VehicleScalar{0.36} * d.treadAggressiveness
        + VehicleScalar{0.22} * d.treadEdgeDensity
        + VehicleScalar{0.24} * d.openVoidRatio
        + VehicleScalar{0.18} * d.flotationCoupling);
    return std::clamp(
        d.minimumWornTreadEffectiveness
            + (VehicleScalar{1.0} - d.minimumWornTreadEffectiveness)
                * depthEffect * (VehicleScalar{0.45} + VehicleScalar{0.55} * architecture),
        VehicleScalar{0.0}, VehicleScalar{1.0});
}

void materialFractions(
    const TireDeformableTerrainInput& input,
    VehicleScalar& mud,
    VehicleScalar& sand,
    VehicleScalar& soil,
    VehicleScalar& snow)
{
    mud = sand = soil = snow = 0.0;
    if (input.footprintSurfaceBlendValid)
    {
        mud = clamp01(input.footprintMudFraction);
        sand = clamp01(input.footprintSandFraction);
        soil = clamp01(input.footprintSoftSoilFraction);
        snow = clamp01(input.footprintDeepSnowFraction);
        return;
    }

    using heritage::physics::SurfaceMaterial;
    switch (input.surfaceMaterial)
    {
    case SurfaceMaterial::Mud: mud = 1.0; break;
    case SurfaceMaterial::Sand: sand = 1.0; break;
    case SurfaceMaterial::SoftSoil: soil = 1.0; break;
    case SurfaceMaterial::DeepSnow: snow = 1.0; break;
    default: break;
    }
}

} // namespace

bool validTireDeformableTerrainDescription(
    const TireDeformableTerrainDescription& d)
{
    return finiteValue(d.treadAggressiveness)
        && d.treadAggressiveness >= 0.0 && d.treadAggressiveness <= 2.0
        && finiteValue(d.treadEdgeDensity)
        && d.treadEdgeDensity >= 0.0 && d.treadEdgeDensity <= 2.0
        && finiteValue(d.openVoidRatio)
        && d.openVoidRatio >= 0.0 && d.openVoidRatio <= 1.0
        && finiteValue(d.soilShearCoupling)
        && d.soilShearCoupling >= 0.0 && d.soilShearCoupling <= 2.0
        && finiteValue(d.bulldozingCoupling)
        && d.bulldozingCoupling >= 0.0 && d.bulldozingCoupling <= 2.0
        && finiteValue(d.plowingCoupling)
        && d.plowingCoupling >= 0.0 && d.plowingCoupling <= 2.0
        && finiteValue(d.flotationCoupling)
        && d.flotationCoupling >= 0.0 && d.flotationCoupling <= 2.0
        && finiteValue(d.minimumWornTreadEffectiveness)
        && d.minimumWornTreadEffectiveness >= 0.0
        && d.minimumWornTreadEffectiveness <= 1.0
        && finiteValue(d.treadDepthEffectExponent)
        && d.treadDepthEffectExponent > 0.0 && d.treadDepthEffectExponent <= 4.0
        && finiteValue(d.maximumSinkageM)
        && d.maximumSinkageM >= 0.0 && d.maximumSinkageM <= 1.0
        && finiteValue(d.maximumTerrainForceRatio)
        && d.maximumTerrainForceRatio >= 0.0 && d.maximumTerrainForceRatio <= 3.0
        && finiteValue(d.maximumPlowingForceRatio)
        && d.maximumPlowingForceRatio >= 0.0 && d.maximumPlowingForceRatio <= 2.0
        && finiteValue(d.minimumMfFrictionScale)
        && finiteValue(d.maximumMfFrictionScale)
        && d.minimumMfFrictionScale >= 0.0
        && d.minimumMfFrictionScale <= d.maximumMfFrictionScale
        && d.maximumMfFrictionScale <= 1.0;
}

heritage::physics::SurfaceFieldInitialState deformableTerrainInitialSurfaceState(
    const heritage::physics::SurfaceDeformableProperties& properties,
    VehicleScalar surfaceWetness)
{
    heritage::physics::SurfaceFieldInitialState state;
    if (!properties.enabled
        || !heritage::physics::validSurfaceDeformableProperties(properties))
    {
        return state;
    }
    state.looseDepthM = static_cast<float>(properties.initialLooseDepthM);
    state.compaction = 0.0f;
    const VehicleScalar wetness = clamp01(surfaceWetness);
    state.moisture = static_cast<float>(std::clamp(
        static_cast<VehicleScalar>(properties.initialMoisture)
            + wetness * VehicleScalar{0.35},
        VehicleScalar{0.0}, VehicleScalar{1.0}));
    state.rutDepthM = 0.0f;
    return state;
}

heritage::physics::SurfaceFieldInitialState deformableTerrainInitialSurfaceState(
    heritage::physics::SurfaceMaterial material,
    VehicleScalar surfaceWetness)
{
    return deformableTerrainInitialSurfaceState(
        presetFor(material), surfaceWetness);
}

TireDeformableTerrainOutput evaluateTireDeformableTerrain(
    const TireDeformableTerrainDescription& d,
    const TireDeformableTerrainInput& input)
{
    TireDeformableTerrainOutput out;
    if (!d.enabled || !validTireDeformableTerrainDescription(d) || !input.grounded)
        return out;

    VehicleScalar mud = 0.0;
    VehicleScalar sand = 0.0;
    VehicleScalar soil = 0.0;
    VehicleScalar snow = 0.0;
    materialFractions(input, mud, sand, soil, snow);
    const VehicleScalar terrainFraction = clamp01(mud + sand + soil + snow);
    if (terrainFraction <= VehicleScalar{1.0e-6})
        return out;

    const VehicleScalar load = std::max(input.normalLoadN, VehicleScalar{0.0});
    if (load <= VehicleScalar{1.0})
        return out;

    TerrainPreset surface = input.surfacePropertiesValid
        ? input.surfaceProperties
        : blendPresets(mud, sand, soil, snow);
    if (!surface.enabled
        || !heritage::physics::validSurfaceDeformableProperties(surface))
    {
        surface = blendPresets(mud, sand, soil, snow);
    }
    const VehicleScalar area = fallbackArea(input);
    const VehicleScalar length = fallbackLength(input, area);
    const VehicleScalar width = fallbackWidth(input, area, length);
    const VehicleScalar pressure = load / std::max(area, VehicleScalar{1.0e-6});
    const VehicleScalar effectiveB = std::clamp(
        std::min(width, length), VehicleScalar{0.03}, VehicleScalar{0.60});

    const VehicleScalar fieldCompaction = input.surfaceField.valid
        ? clamp01(input.surfaceField.compaction) : VehicleScalar{0.0};
    const VehicleScalar fieldMoisture = input.surfaceField.valid
        ? clamp01(input.surfaceField.moisture)
        : clamp01(surface.initialMoisture + input.surfaceWetness * VehicleScalar{0.35});
    const VehicleScalar existingRut = input.surfaceField.valid
        ? std::max(static_cast<VehicleScalar>(input.surfaceField.rutDepthM), VehicleScalar{0.0})
        : VehicleScalar{0.0};
    const VehicleScalar looseDepth = input.surfaceField.valid
        ? std::max(static_cast<VehicleScalar>(input.surfaceField.looseDepthM), VehicleScalar{0.0})
        : surface.initialLooseDepthM;

    // Compaction stiffens the pressure-sinkage response. Very wet material is
    // softened except for cohesive mud, where suction/cohesion remains but the
    // friction angle is strongly reduced.
    const VehicleScalar moistureSoftening = std::clamp(
        VehicleScalar{1.0} - VehicleScalar{0.45} * fieldMoisture,
        VehicleScalar{0.45}, VehicleScalar{1.0});
    const VehicleScalar compactionStiffening = VehicleScalar{1.0}
        + surface.compactionStiffnessGain * fieldCompaction;
    const VehicleScalar effectiveKc = surface.bekkerKc * compactionStiffening
        * moistureSoftening;
    const VehicleScalar effectiveKphi = surface.bekkerKphi * compactionStiffening
        * moistureSoftening;
    const VehicleScalar pressureModulus = std::max(
        effectiveKc / effectiveB + effectiveKphi, VehicleScalar{1.0});
    VehicleScalar elasticSinkage = std::pow(
        std::max(pressure / pressureModulus, VehicleScalar{0.0}),
        VehicleScalar{1.0} / std::max(surface.sinkageExponent, VehicleScalar{0.25}));

    // A broad/low-pressure pneumatic tire floats better than a narrow tire at
    // the same load. This modest bounded term complements the pressure term
    // without pretending the tread mesh itself is a finite-element tire.
    const VehicleScalar flotation = std::clamp(
        d.flotationCoupling * width / VehicleScalar{0.30},
        VehicleScalar{0.0}, VehicleScalar{0.65});
    elasticSinkage *= VehicleScalar{1.0} - VehicleScalar{0.35} * flotation;
    elasticSinkage *= VehicleScalar{1.0} - VehicleScalar{0.55} * fieldCompaction;

    const VehicleScalar availableDepth = std::max(
        looseDepth - existingRut * VehicleScalar{0.25}, VehicleScalar{0.0});
    elasticSinkage = std::clamp(
        elasticSinkage, VehicleScalar{0.0},
        std::min(availableDepth, d.maximumSinkageM));
    const VehicleScalar totalSinkage = std::clamp(
        existingRut + elasticSinkage,
        VehicleScalar{0.0}, d.maximumSinkageM);

    VehicleScalar effectiveCohesion = surface.cohesionPa
        * (VehicleScalar{1.0} + VehicleScalar{0.40} * fieldCompaction);
    VehicleScalar effectivePhi = radians(surface.frictionAngleDegrees)
        * (VehicleScalar{1.0} - VehicleScalar{0.55} * fieldMoisture)
        * (VehicleScalar{1.0} + surface.compactionShearGain * fieldCompaction);
    effectivePhi = std::clamp(effectivePhi, radians(4.0), radians(45.0));
    if (mud > VehicleScalar{0.0})
        effectiveCohesion *= VehicleScalar{0.75} + VehicleScalar{0.35} * fieldMoisture;

    const VehicleScalar maximumShearStress = std::max(
        effectiveCohesion + pressure * std::tan(effectivePhi), VehicleScalar{0.0});
    const VehicleScalar grossShearCapacity = maximumShearStress * area * terrainFraction;
    const VehicleScalar tread = treadEffectiveness(d, input);
    const VehicleScalar usableShearCapacity = std::min(
        grossShearCapacity * d.soilShearCoupling * tread,
        load * d.maximumTerrainForceRatio * terrainFraction);

    const VehicleScalar clampedSlipAngle = std::clamp(
        input.slipAngleRadians, radians(-70.0), radians(70.0));
    const VehicleScalar longitudinalShearDisplacement = length
        * std::min(std::abs(input.slipRatio), VehicleScalar{3.0});
    const VehicleScalar lateralShearDisplacement = length
        * std::min(std::abs(std::tan(clampedSlipAngle)), VehicleScalar{4.0});
    const VehicleScalar shearModulus = std::max(
        surface.shearDeformationModulusM, VehicleScalar{0.002});
    const VehicleScalar longMob = clamp01(
        VehicleScalar{1.0} - std::exp(-longitudinalShearDisplacement / shearModulus));
    const VehicleScalar latMob = clamp01(
        VehicleScalar{1.0} - std::exp(-lateralShearDisplacement / shearModulus));

    const VehicleScalar longitudinalDirection = signOf(
        std::abs(input.longitudinalSlipVelocityMps) > VehicleScalar{1.0e-5}
            ? input.longitudinalSlipVelocityMps : input.slipRatio);
    const VehicleScalar lateralDirection = -signOf(
        std::abs(input.lateralSlipVelocityMps) > VehicleScalar{1.0e-5}
            ? input.lateralSlipVelocityMps : input.slipAngleRadians);

    const VehicleScalar longitudinalTerrainForce = longitudinalDirection
        * usableShearCapacity * longMob;
    const VehicleScalar lateralTerrainForce = lateralDirection
        * usableShearCapacity * latMob;

    const VehicleScalar passiveCoefficient = std::pow(
        std::tan(kPi * VehicleScalar{0.25} + effectivePhi * VehicleScalar{0.5}),
        VehicleScalar{2.0});
    const VehicleScalar passiveWedge = (
        VehicleScalar{0.5} * surface.densityKgM3 * kGravity
            * passiveCoefficient * width * totalSinkage * totalSinkage
        + VehicleScalar{2.0} * effectiveCohesion * width * totalSinkage
            * std::sqrt(std::max(passiveCoefficient, VehicleScalar{0.0})))
        * terrainFraction;
    const VehicleScalar bulldozing = lateralDirection * std::min(
        passiveWedge * d.bulldozingCoupling * latMob,
        load * d.maximumTerrainForceRatio * VehicleScalar{0.55} * terrainFraction);

    const VehicleScalar compactionResistance = load * totalSinkage
        / std::max(input.unloadedRadiusM, VehicleScalar{0.10});
    const VehicleScalar plowing = std::min(
        (compactionResistance + passiveWedge * VehicleScalar{0.55})
            * d.plowingCoupling,
        load * d.maximumPlowingForceRatio * terrainFraction);

    const VehicleScalar pressureRatio = std::clamp(
        pressure / VehicleScalar{180000.0}, VehicleScalar{0.0}, VehicleScalar{3.0});
    const VehicleScalar shearActivity = clamp01(
        VehicleScalar{0.5} * (longMob + latMob));
    const VehicleScalar compactionRate = surface.compactionRateHz
        * pressureRatio * (VehicleScalar{1.0} + VehicleScalar{0.6} * shearActivity)
        * (VehicleScalar{1.0} - fieldCompaction) * terrainFraction;
    const VehicleScalar rutTarget = std::max(
        existingRut,
        totalSinkage * surface.plasticRutFraction * terrainFraction);
    const VehicleScalar looseLossRate = surface.looseDepthLossPerCompactionM
        * compactionRate;
    const VehicleScalar displacedVolumeRate = std::max(
        plowing / std::max(surface.densityKgM3 * kGravity, VehicleScalar{1.0}),
        VehicleScalar{0.0}) * std::max(std::abs(input.forwardSpeedMps), VehicleScalar{0.2});

    const VehicleScalar compactedMf = surface.mfBaseFrictionScale
        * (VehicleScalar{1.0} + VehicleScalar{0.75} * fieldCompaction);
    const VehicleScalar localMfScale = std::clamp(
        compactedMf,
        d.minimumMfFrictionScale,
        d.maximumMfFrictionScale);

    out.valid = true;
    out.terrainSurfaceFraction = terrainFraction;
    out.mudFraction = mud;
    out.sandFraction = sand;
    out.softSoilFraction = soil;
    out.deepSnowFraction = snow;
    out.contactPressurePa = pressure;
    out.elasticSinkageM = elasticSinkage;
    out.persistentRutDepthM = existingRut;
    out.totalSinkageM = totalSinkage * terrainFraction;
    out.compaction = fieldCompaction;
    out.moisture = fieldMoisture;
    out.looseDepthM = looseDepth;
    out.effectiveCohesionPa = effectiveCohesion;
    out.effectiveFrictionAngleDegrees = degrees(effectivePhi);
    out.shearCapacityN = grossShearCapacity;
    out.longitudinalShearMobilization = longMob;
    out.lateralShearMobilization = latMob;
    out.treadEffectiveness = tread;
    out.longitudinalTerrainForceN = longitudinalTerrainForce;
    out.lateralTerrainForceN = lateralTerrainForce;
    out.lateralBulldozingForceN = bulldozing;
    out.plowingDragN = plowing;
    out.compactionPowerW = plowing * std::abs(input.forwardSpeedMps);
    out.additionalContactCapacityN = std::min(
        usableShearCapacity + std::abs(bulldozing),
        load * d.maximumTerrainForceRatio * terrainFraction);
    out.mfFrictionScale = VehicleScalar{1.0}
        + terrainFraction * (localMfScale - VehicleScalar{1.0});
    out.stiffnessScale = VehicleScalar{1.0}
        + terrainFraction * (surface.baseStiffnessScale - VehicleScalar{1.0});
    out.rollingResistanceScale = VehicleScalar{1.0}
        + terrainFraction * (surface.rollingResistanceScale - VehicleScalar{1.0});
    out.relaxationScale = VehicleScalar{1.0}
        + terrainFraction * (surface.relaxationScale - VehicleScalar{1.0});
    out.rutDepthTargetM = rutTarget;
    out.compactionRatePerSecond = std::max(compactionRate, VehicleScalar{0.0});
    out.looseDepthLossRateMps = std::max(looseLossRate, VehicleScalar{0.0});
    out.displacedVolumeRateM3ps = displacedVolumeRate;
    return out;
}

heritage::physics::SurfaceFieldUpdate tireDeformableTerrainFieldUpdate(
    const TireDeformableTerrainDescription&,
    const TireDeformableTerrainInput& input,
    const TireDeformableTerrainOutput& output,
    VehicleScalar deltaTimeSeconds)
{
    heritage::physics::SurfaceFieldUpdate update;
    update.material = input.surfaceMaterial;
    update.initialState = input.surfacePropertiesValid
        ? deformableTerrainInitialSurfaceState(
            input.surfaceProperties, input.surfaceWetness)
        : deformableTerrainInitialSurfaceState(
            input.surfaceMaterial, input.surfaceWetness);
    if (!output.valid || deltaTimeSeconds <= VehicleScalar{0.0})
        return update;

    const VehicleScalar dt = std::max(deltaTimeSeconds, VehicleScalar{0.0});
    update.rutDepthTargetM = static_cast<float>(output.rutDepthTargetM);
    const VehicleScalar existingRut = input.surfaceField.valid
        ? static_cast<VehicleScalar>(input.surfaceField.rutDepthM)
        : VehicleScalar{0.0};
    const VehicleScalar remainingPlasticRut = std::max(
        output.rutDepthTargetM - existingRut, VehicleScalar{0.0});
    update.rutDepthDeltaM = static_cast<float>(
        remainingPlasticRut * std::min(dt * VehicleScalar{4.0}, VehicleScalar{1.0}));
    update.compactionDelta = static_cast<float>(
        output.compactionRatePerSecond * dt);
    update.looseDepthDeltaM = static_cast<float>(
        -output.looseDepthLossRateMps * dt);
    update.longitudinalShearHistoryDeltaM = static_cast<float>(
        std::abs(input.longitudinalSlipVelocityMps) * dt
        * output.terrainSurfaceFraction);
    update.lateralShearHistoryDeltaM = static_cast<float>(
        std::abs(input.lateralSlipVelocityMps) * dt
        * output.terrainSurfaceFraction);
    update.displacedVolumeDeltaM3 = static_cast<float>(
        output.displacedVolumeRateM3ps * dt);
    const VehicleScalar traversalDistance = std::abs(input.forwardSpeedMps) * dt;
    const VehicleScalar footprintLength = std::max(
        input.contactPatchLengthM, VehicleScalar{0.10});
    update.passProgressDelta = static_cast<float>(
        traversalDistance / footprintLength * output.terrainSurfaceFraction);
    update.countPass = false;
    return update;
}

} // namespace heritage::vehicles::tires

// CLEAN03B wheel-substep phase: 04_TireStructureAndTerrainSupport
// Advance rigid-ring structure and preview granular/deformable support sinkage before suspension resolution.
// This file is intentionally included inside VehicleSystem::simulateWheelSubstep().
// It preserves the validated lexical scope and statement order while making phase ownership explicit.

    tires::TireRigidRingOutput rigidRingOutput;
    if (wheel.tireModel.rigidRing.enabled)
    {
        tires::TireRigidRingInput ringInput;
        ringInput.deltaTimeSeconds = substepDeltaTime;
        ringInput.forwardSpeedMps = previousLongitudinalSpeed;
        ringInput.roadRadialOffsetM = hitGround
            ? wheel.cachedRoadEnvelopeOffsetM : VehicleScalar{0.0};
        ringInput.longitudinalForceN = hitGround
            ? previousLongitudinalTireForce : VehicleScalar{0.0};
        ringInput.lateralForceN = hitGround
            ? previousLateralTireForce : VehicleScalar{0.0};
        ringInput.inflationPressurePa = state.tireInflationPressurePa > 20000.0
            ? state.tireInflationPressurePa
            : wheel.tireModel.inflationPressurePa;
        ringInput.referencePressurePa = wheel.tireModel.thermal.enabled
            ? wheel.tireModel.thermal.referenceGaugePressurePa
            : wheel.tireModel.inflationPressurePa;
        ringInput.thermalStiffnessScale = state.tireThermalStiffnessScale > 0.0
            ? state.tireThermalStiffnessScale : VehicleScalar{1.0};
        ringInput.aligningMomentNm = hitGround
            ? previousAligningTorque : VehicleScalar{0.0};
        ringInput.longitudinalReactionMomentNm = hitGround
            ? -previousLongitudinalTireForce * previousEffectiveRollingRadius
            : VehicleScalar{0.0};
        rigidRingOutput = tires::advanceTireRigidRing(
            wheel.tireModel.rigidRing,
            ringInput,
            wheel.rigidRingState);
    }
    else
    {
        wheel.rigidRingState = {};
    }

    state.tireEnvelopeRoadOffset = wheel.cachedRoadEnvelopeOffsetM;
    state.tireEnvelopeSlopeDegrees = degrees(
        wheel.cachedRoadEnvelopeSlopeRadians);
    state.tireEnvelopeCrossSlopeDegrees = degrees(
        wheel.cachedRoadEnvelopeCrossSlopeRadians);
    state.tireEnvelopeValidSamples = static_cast<VehicleScalar>(
        wheel.cachedRoadEnvelopeValidSamples);
    state.tireFootprintTotalSamples = static_cast<VehicleScalar>(
        wheel.cachedRoadEnvelopeTotalSamples);
    state.tireFootprintSupportedFraction =
        wheel.cachedRoadEnvelopeSupportedFraction;
    state.tireFootprintRoughnessRange =
        wheel.cachedRoadEnvelopeRoughnessRangeM;
    state.tireFootprintSurfaceFriction =
        wheel.cachedFootprintFrictionMultiplier;
    state.tireFootprintSurfaceSpread =
        wheel.cachedFootprintFrictionSpread;
    state.tireFootprintRefined = wheel.cachedFootprintRefined;
    state.tireVisualSupportGridValid = wheel.cachedVisualSupportGridValid;
    state.tireVisualSupportHalfLengthM = wheel.cachedVisualSupportHalfLengthM;
    state.tireVisualSupportHalfWidthM = wheel.cachedVisualSupportHalfWidthM;
    state.tireVisualSupportHeightResidualM =
        wheel.cachedVisualSupportHeightResidualM;
    if (rigidRingOutput.valid)
    {
        state.tireRingRadialOffset = rigidRingOutput.radialOffsetM;
        state.tireRingRadialVelocity = rigidRingOutput.radialVelocityMps;
        state.tireRingLongitudinalOffset =
            rigidRingOutput.longitudinalOffsetM;
        state.tireRingLongitudinalVelocity =
            rigidRingOutput.longitudinalVelocityMps;
        state.tireRingLateralOffset = rigidRingOutput.lateralOffsetM;
        state.tireRingLateralVelocity = rigidRingOutput.lateralVelocityMps;
        state.tireRingYawDegrees = degrees(rigidRingOutput.yawAngleRadians);
        state.tireRingYawRateDegreesPerSecond = degrees(
            rigidRingOutput.yawAngularVelocityRadPerS);
        state.tireRingWindupDegrees = degrees(
            rigidRingOutput.windupAngleRadians);
        state.tireRingWindupRateDegreesPerSecond = degrees(
            rigidRingOutput.windupAngularVelocityRadPerS);
    }
    else
    {
        state.tireRingRadialOffset = 0.0;
        state.tireRingRadialVelocity = 0.0;
        state.tireRingLongitudinalOffset = 0.0;
        state.tireRingLongitudinalVelocity = 0.0;
        state.tireRingLateralOffset = 0.0;
        state.tireRingLateralVelocity = 0.0;
        state.tireRingYawDegrees = 0.0;
        state.tireRingYawRateDegreesPerSecond = 0.0;
        state.tireRingWindupDegrees = 0.0;
        state.tireRingWindupRateDegreesPerSecond = 0.0;
    }

    const VehicleScalar structuralRoadOffset = rigidRingOutput.valid
        ? rigidRingOutput.radialOffsetM
        : wheel.cachedRoadEnvelopeOffsetM;
    const VehicleScalar structuralRoadVelocity = rigidRingOutput.valid
        ? rigidRingOutput.radialVelocityMps
        : VehicleScalar{0.0};

    // TIRE14 shallow-granular support preview. The provider uses the previous
    // 1000 Hz load/slip/footprint state (or nominal fallbacks on first
    // contact) to estimate how far the tire penetrates the loose top layer
    // before the load-bearing base supports it. This one-substep lag avoids a
    // circular vertical solve while remaining far below the time scale of
    // gravel/dirt sinkage changes.
    tires::TireShallowGranularOutput shallowGranularSupport{};
    if (hitGround && wheel.tireModel.shallowGranularSurface.enabled)
    {
        tires::TireShallowGranularInput granularSupportInput;
        granularSupportInput.grounded = true;
        granularSupportInput.surfaceMaterial = hit.surfaceMaterial;
        granularSupportInput.surfaceWetness = hitSurfaceConditions.wetness;
        granularSupportInput.footprintSurfaceBlendValid =
            wheel.cachedFootprintMaterialBlendValid;
        granularSupportInput.footprintGravelFraction =
            wheel.cachedFootprintGravelFraction;
        granularSupportInput.footprintDirtFraction =
            wheel.cachedFootprintDirtFraction;
        granularSupportInput.footprintAverageWetness =
            wheel.cachedFootprintAverageWetness;
        granularSupportInput.normalLoadN = previousNormalForce > VehicleScalar{1.0}
            ? previousNormalForce
            : std::max(wheel.tireModel.nominalLoad, VehicleScalar{1.0});
        granularSupportInput.nominalLoadN = wheel.tireModel.nominalLoad;
        granularSupportInput.forwardSpeedMps = previousLongitudinalSpeed;
        granularSupportInput.longitudinalSlipVelocityMps =
            previousSlipRatio * std::max(
                std::abs(previousLongitudinalSpeed), VehicleScalar{0.5});
        granularSupportInput.lateralSlipVelocityMps = previousLateralSpeed;
        granularSupportInput.slipRatio = previousSlipRatio;
        granularSupportInput.slipAngleRadians = previousSlipAngleRadians;
        granularSupportInput.unloadedRadiusM = physicalSupportRadiusM;
        granularSupportInput.contactPatchLengthM = previousContactPatchLength;
        granularSupportInput.contactPatchWidthM = previousContactPatchWidth;
        granularSupportInput.contactPatchAreaM2 = previousContactPatchArea;
        granularSupportInput.currentAverageTreadDepthM = preContactWear.valid
            ? preContactWear.averageTreadDepthM
            : wheel.tireModel.wear.initialTreadDepthM;
        granularSupportInput.initialTreadDepthM =
            wheel.tireModel.wear.initialTreadDepthM;
        granularSupportInput.minimumTreadDepthM =
            wheel.tireModel.wear.minimumTreadDepthM;
        shallowGranularSupport = tires::evaluateTireShallowGranular(
            wheel.tireModel.shallowGranularSurface,
            granularSupportInput);
    }
    const VehicleScalar granularSinkageM = shallowGranularSupport.valid
        ? shallowGranularSupport.sinkageM : VehicleScalar{0.0};

    // TIRE15 persistent deformable-terrain support preview. Unlike TIRE14's
    // shallow layer, the SurfaceField remembers plastic rut/compaction state
    // at this world location. Use the previous substep load for the support
    // preview to avoid a circular vertical solve, exactly as the shallow
    // granular bridge does.
    tires::TireDeformableTerrainOutput deformableTerrainSupport{};
    heritage::physics::SurfaceFieldSample deformableTerrainFieldSupport{};
    const heritage::physics::SurfaceMaterial supportTerrainMaterial =
        dominantDeformableTerrainMaterial(
            hit.surfaceMaterial,
            wheel.cachedFootprintMudFraction,
            wheel.cachedFootprintSandFraction,
            wheel.cachedFootprintSoftSoilFraction,
            wheel.cachedFootprintDeepSnowFraction);
    heritage::physics::SurfaceDeformableProperties supportTerrainProperties =
        heritage::physics::defaultSurfaceMaterialProperties(
            supportTerrainMaterial).deformable;
    if (wheel.cachedFootprintDeformablePropertiesValid)
    {
        supportTerrainProperties = wheel.cachedFootprintDeformableProperties;
    }
    else if (hit.surfaceMaterial == supportTerrainMaterial
        && hit.surfaceProperties.deformable.enabled)
    {
        supportTerrainProperties = hit.surfaceProperties.deformable;
    }
    const VehicleScalar supportTerrainWetness =
        wheel.cachedFootprintMaterialBlendValid
            ? wheel.cachedFootprintAverageWetness
            : hitSurfaceConditions.wetness;
    if (hitGround && wheel.tireModel.deformableTerrainSurface.enabled
        && deformableTerrainSurfaceMaterial(supportTerrainMaterial))
    {
        const auto initialField = tires::deformableTerrainInitialSurfaceState(
            supportTerrainProperties, supportTerrainWetness);
        deformableTerrainFieldSupport = surfaces.sampleDeformable(
            hit.point, supportTerrainMaterial, initialField);

        tires::TireDeformableTerrainInput terrainSupportInput;
        terrainSupportInput.grounded = true;
        terrainSupportInput.surfaceMaterial = supportTerrainMaterial;
        terrainSupportInput.surfaceWetness = supportTerrainWetness;
        terrainSupportInput.surfaceProperties = supportTerrainProperties;
        terrainSupportInput.surfacePropertiesValid =
            supportTerrainProperties.enabled
            && heritage::physics::validSurfaceDeformableProperties(
                supportTerrainProperties);
        terrainSupportInput.footprintSurfaceBlendValid =
            wheel.cachedFootprintMaterialBlendValid;
        terrainSupportInput.footprintMudFraction = wheel.cachedFootprintMudFraction;
        terrainSupportInput.footprintSandFraction = wheel.cachedFootprintSandFraction;
        terrainSupportInput.footprintSoftSoilFraction =
            wheel.cachedFootprintSoftSoilFraction;
        terrainSupportInput.footprintDeepSnowFraction =
            wheel.cachedFootprintDeepSnowFraction;
        terrainSupportInput.surfaceField = deformableTerrainFieldSupport;
        terrainSupportInput.normalLoadN = previousNormalForce > VehicleScalar{1.0}
            ? previousNormalForce
            : std::max(wheel.tireModel.nominalLoad, VehicleScalar{1.0});
        terrainSupportInput.nominalLoadN = wheel.tireModel.nominalLoad;
        terrainSupportInput.forwardSpeedMps = previousLongitudinalSpeed;
        terrainSupportInput.longitudinalSlipVelocityMps =
            previousSlipRatio * std::max(
                std::abs(previousLongitudinalSpeed), VehicleScalar{0.5});
        terrainSupportInput.lateralSlipVelocityMps = previousLateralSpeed;
        terrainSupportInput.slipRatio = previousSlipRatio;
        terrainSupportInput.slipAngleRadians = previousSlipAngleRadians;
        terrainSupportInput.unloadedRadiusM = physicalSupportRadiusM;
        terrainSupportInput.contactPatchLengthM = previousContactPatchLength;
        terrainSupportInput.contactPatchWidthM = previousContactPatchWidth;
        terrainSupportInput.contactPatchAreaM2 = previousContactPatchArea;
        terrainSupportInput.currentAverageTreadDepthM = preContactWear.valid
            ? preContactWear.averageTreadDepthM
            : wheel.tireModel.wear.initialTreadDepthM;
        terrainSupportInput.initialTreadDepthM = wheel.tireModel.wear.initialTreadDepthM;
        terrainSupportInput.minimumTreadDepthM = wheel.tireModel.wear.minimumTreadDepthM;
        deformableTerrainSupport = tires::evaluateTireDeformableTerrain(
            wheel.tireModel.deformableTerrainSurface, terrainSupportInput);
    }
    const VehicleScalar deformableTerrainSinkageM = deformableTerrainSupport.valid
        ? deformableTerrainSupport.totalSinkageM : VehicleScalar{0.0};
    const VehicleScalar surfaceSupportSinkageM =
        granularSinkageM + deformableTerrainSinkageM;


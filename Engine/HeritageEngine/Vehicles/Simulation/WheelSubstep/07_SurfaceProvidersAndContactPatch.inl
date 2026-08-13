// CLEAN03B wheel-substep phase: 07_SurfaceProvidersAndContactPatch
// Build surface/provider state, thermal/wet/winter/granular/deformable inputs, transient slip, and turn-slip contact-patch state.
// This file is intentionally included inside VehicleSystem::simulateWheelSubstep().
// It preserves the validated lexical scope and statement order while making phase ownership explicit.

    SurfaceProfile surface = surfaceProfile(
        hit.surfaceMaterial,
        static_cast<float>(hitSurfaceConditions.wetness),
        vehicle.surface);
    if (wheel.cachedFootprintSurfaceValid)
    {
        // TIRE06 split-surface approximation: one MF force evaluation remains
        // the performance baseline, but its surface modifiers are the area-
        // sampled footprint average rather than only the centre ray material.
        surface.frictionMultiplier = static_cast<float>(
            wheel.cachedFootprintFrictionMultiplier);
        surface.stiffnessMultiplier = static_cast<float>(
            wheel.cachedFootprintStiffnessMultiplier);
        surface.rollingResistanceMultiplier = static_cast<float>(
            wheel.cachedFootprintRollingResistanceMultiplier);
        surface.relaxationMultiplier = static_cast<float>(
            wheel.cachedFootprintRelaxationMultiplier);
    }

    // TIRE12/TIRE13/TIRE14 dedicated surface providers replace the legacy
    // scalar wet, winter and gravel/dirt profiles before applying explicit
    // mechanisms. The cached provider base is area weighted: hard-wet samples
    // are restored to dry coefficients, while snow/ice and gravel/dirt samples
    // are neutralized for their dedicated providers. Grass remains legacy until
    // the broader soft/deformable terrain work.
    if (wheel.tireModel.wetSurface.enabled
        || wheel.tireModel.winterSurface.enabled
        || wheel.tireModel.shallowGranularSurface.enabled
        || wheel.tireModel.deformableTerrainSurface.enabled)
    {
        if (wheel.cachedFootprintSurfaceValid)
        {
            surface.frictionMultiplier = static_cast<float>(
                wheel.cachedFootprintProviderBaseFrictionMultiplier);
            surface.stiffnessMultiplier = static_cast<float>(
                wheel.cachedFootprintProviderBaseStiffnessMultiplier);
            surface.rollingResistanceMultiplier = static_cast<float>(
                wheel.cachedFootprintProviderBaseRollingResistanceMultiplier);
            surface.relaxationMultiplier = static_cast<float>(
                wheel.cachedFootprintProviderBaseRelaxationMultiplier);
        }
        else
        {
            surface = providerBaseSurfaceProfile(
                hit.surfaceMaterial, static_cast<float>(hitSurfaceConditions.wetness), vehicle.surface,
                wheel.tireModel.wetSurface.enabled,
                wheel.tireModel.winterSurface.enabled,
                wheel.tireModel.shallowGranularSurface.enabled,
                wheel.tireModel.deformableTerrainSurface.enabled);
        }
    }

    tires::TireWetSurfaceInput wetInput;
    if (wheel.tireModel.wetSurface.enabled)
    {
        wetInput.grounded = true;
        wetInput.surfaceMaterial = hit.surfaceMaterial;
        wetInput.surfaceWetness = hitSurfaceConditions.wetness;
        wetInput.footprintSurfaceBlendValid =
            wheel.cachedFootprintMaterialBlendValid;
        wetInput.footprintCleanHardFraction =
            wheel.cachedFootprintCleanHardFraction;
        wetInput.footprintAverageWetness =
            wheel.cachedFootprintAverageWetness;
        wetInput.wheelRotationDegrees = state.wheelRotationDegrees;
        wetInput.normalLoadN = suspensionForce;
        wetInput.inflationPressurePa = dynamicInflationPressurePa;
        wetInput.referencePressurePa =
            wheel.tireModel.referenceInflationPressurePa;
        wetInput.forwardSpeedMps = structuralLongitudinalSpeed;
        wetInput.longitudinalSlipVelocityMps =
            circumferentialSpeed - structuralLongitudinalSpeed;
        wetInput.lateralSlipVelocityMps = structuralLateralSpeed;
        wetInput.contactPatchLengthM = state.tireContactPatchLength;
        wetInput.contactPatchWidthM = state.tireContactPatchWidth;
        wetInput.contactPatchAreaM2 = state.tireContactPatchArea;
        wetInput.currentAverageTreadDepthM = wearBefore.valid
            ? wearBefore.averageTreadDepthM
            : wheel.tireModel.wear.initialTreadDepthM;
        wetInput.initialTreadDepthM = wheel.tireModel.wear.initialTreadDepthM;
        wetInput.minimumTreadDepthM = wheel.tireModel.wear.minimumTreadDepthM;
        wetInput.bulkTreadTemperatureC = thermalBefore.valid
            ? thermalBefore.treadTemperatureC : VehicleScalar{20.0};
        wetBefore = tires::evaluateTireWetSurface(
            wheel.tireModel.wetSurface, wheel.tireModel.wear,
            wetInput, wheel.wearState);
        writeWetSurfaceTelemetry(wetBefore);
    }

    tires::TireWinterSurfaceInput winterInput;
    if (wheel.tireModel.winterSurface.enabled)
    {
        winterInput.grounded = true;
        winterInput.surfaceMaterial = hit.surfaceMaterial;
        winterInput.surfaceWetness = hitSurfaceConditions.wetness;
        winterInput.footprintSurfaceBlendValid =
            wheel.cachedFootprintMaterialBlendValid;
        winterInput.footprintSnowFraction = wheel.cachedFootprintSnowFraction;
        winterInput.footprintIceFraction = wheel.cachedFootprintIceFraction;
        winterInput.footprintAverageWetness = wheel.cachedFootprintAverageWetness;
        winterInput.wheelRotationDegrees = state.wheelRotationDegrees;
        winterInput.normalLoadN = suspensionForce;
        winterInput.nominalLoadN = wheel.tireModel.nominalLoad;
        winterInput.inflationPressurePa = dynamicInflationPressurePa;
        winterInput.referencePressurePa =
            wheel.tireModel.referenceInflationPressurePa;
        winterInput.forwardSpeedMps = structuralLongitudinalSpeed;
        winterInput.longitudinalSlipVelocityMps =
            circumferentialSpeed - structuralLongitudinalSpeed;
        winterInput.lateralSlipVelocityMps = structuralLateralSpeed;
        winterInput.currentAverageTreadDepthM = wearBefore.valid
            ? wearBefore.averageTreadDepthM
            : wheel.tireModel.wear.initialTreadDepthM;
        winterInput.initialTreadDepthM = wheel.tireModel.wear.initialTreadDepthM;
        winterInput.minimumTreadDepthM = wheel.tireModel.wear.minimumTreadDepthM;
        // TIRE15B: winter physics consumes the same authored/live road
        // temperature that the tire thermal system sees. Footprint sampling
        // averages local values across a refined patch when available.
        winterInput.surfaceTemperatureC = wheel.cachedFootprintMaterialBlendValid
            ? wheel.cachedFootprintAverageSurfaceTemperatureC
            : hitSurfaceConditions.surfaceTemperatureC;
        winterInput.bulkTreadTemperatureC = thermalBefore.valid
            ? thermalBefore.treadTemperatureC : VehicleScalar{20.0};
        winterBefore = tires::evaluateTireWinterSurface(
            wheel.tireModel.winterSurface, wheel.tireModel.wear,
            winterInput, wheel.wearState);
        writeWinterSurfaceTelemetry(winterBefore);
    }

    tires::TireShallowGranularInput shallowGranularInput;
    if (wheel.tireModel.shallowGranularSurface.enabled)
    {
        shallowGranularInput.grounded = true;
        shallowGranularInput.surfaceMaterial = hit.surfaceMaterial;
        shallowGranularInput.surfaceWetness = hitSurfaceConditions.wetness;
        shallowGranularInput.footprintSurfaceBlendValid =
            wheel.cachedFootprintMaterialBlendValid;
        shallowGranularInput.footprintGravelFraction =
            wheel.cachedFootprintGravelFraction;
        shallowGranularInput.footprintDirtFraction =
            wheel.cachedFootprintDirtFraction;
        shallowGranularInput.footprintAverageWetness =
            wheel.cachedFootprintAverageWetness;
        shallowGranularInput.normalLoadN = suspensionForce;
        shallowGranularInput.nominalLoadN = wheel.tireModel.nominalLoad;
        shallowGranularInput.forwardSpeedMps = structuralLongitudinalSpeed;
        shallowGranularInput.longitudinalSlipVelocityMps =
            circumferentialSpeed - structuralLongitudinalSpeed;
        shallowGranularInput.lateralSlipVelocityMps = structuralLateralSpeed;
        shallowGranularInput.slipRatio = state.slipRatio;
        shallowGranularInput.slipAngleRadians = slipAngleRadians;
        shallowGranularInput.unloadedRadiusM = physicalSupportRadiusM;
        shallowGranularInput.contactPatchLengthM = state.tireContactPatchLength;
        shallowGranularInput.contactPatchWidthM = state.tireContactPatchWidth;
        shallowGranularInput.contactPatchAreaM2 = state.tireContactPatchArea;
        shallowGranularInput.currentAverageTreadDepthM = wearBefore.valid
            ? wearBefore.averageTreadDepthM
            : wheel.tireModel.wear.initialTreadDepthM;
        shallowGranularInput.initialTreadDepthM =
            wheel.tireModel.wear.initialTreadDepthM;
        shallowGranularInput.minimumTreadDepthM =
            wheel.tireModel.wear.minimumTreadDepthM;
        shallowGranularBefore = tires::evaluateTireShallowGranular(
            wheel.tireModel.shallowGranularSurface,
            shallowGranularInput);
        writeShallowGranularTelemetry(shallowGranularBefore);
    }

    tires::TireDeformableTerrainInput deformableTerrainInput;
    heritage::physics::SurfaceMaterial activeTerrainMaterial =
        heritage::physics::SurfaceMaterial::Default;
    if (wheel.tireModel.deformableTerrainSurface.enabled)
    {
        deformableTerrainInput.grounded = true;
        deformableTerrainInput.surfaceMaterial = hit.surfaceMaterial;
        deformableTerrainInput.surfaceWetness = supportTerrainWetness;
        deformableTerrainInput.surfaceProperties = supportTerrainProperties;
        deformableTerrainInput.surfacePropertiesValid =
            supportTerrainProperties.enabled
            && heritage::physics::validSurfaceDeformableProperties(
                supportTerrainProperties);
        deformableTerrainInput.footprintSurfaceBlendValid =
            wheel.cachedFootprintMaterialBlendValid;
        deformableTerrainInput.footprintMudFraction =
            wheel.cachedFootprintMudFraction;
        deformableTerrainInput.footprintSandFraction =
            wheel.cachedFootprintSandFraction;
        deformableTerrainInput.footprintSoftSoilFraction =
            wheel.cachedFootprintSoftSoilFraction;
        deformableTerrainInput.footprintDeepSnowFraction =
            wheel.cachedFootprintDeepSnowFraction;
        activeTerrainMaterial = dominantDeformableTerrainMaterial(
            hit.surfaceMaterial,
            wheel.cachedFootprintMudFraction,
            wheel.cachedFootprintSandFraction,
            wheel.cachedFootprintSoftSoilFraction,
            wheel.cachedFootprintDeepSnowFraction);
        if (deformableTerrainSurfaceMaterial(activeTerrainMaterial))
        {
            const auto initialField = tires::deformableTerrainInitialSurfaceState(
                supportTerrainProperties, supportTerrainWetness);
            deformableTerrainField = surfaces.sampleDeformable(
                hit.point, activeTerrainMaterial, initialField);
            deformableTerrainInput.surfaceField = deformableTerrainField;
            // The footprint fractions preserve partial hard/deformable contact;
            // use the dominant deformable material only for persistent field
            // properties and state lookup.
            deformableTerrainInput.surfaceMaterial = activeTerrainMaterial;
        }
        deformableTerrainInput.normalLoadN = suspensionForce;
        deformableTerrainInput.nominalLoadN = wheel.tireModel.nominalLoad;
        deformableTerrainInput.forwardSpeedMps = structuralLongitudinalSpeed;
        deformableTerrainInput.longitudinalSlipVelocityMps =
            circumferentialSpeed - structuralLongitudinalSpeed;
        deformableTerrainInput.lateralSlipVelocityMps = structuralLateralSpeed;
        deformableTerrainInput.slipRatio = state.slipRatio;
        deformableTerrainInput.slipAngleRadians = slipAngleRadians;
        deformableTerrainInput.unloadedRadiusM = physicalSupportRadiusM;
        deformableTerrainInput.contactPatchLengthM = state.tireContactPatchLength;
        deformableTerrainInput.contactPatchWidthM = state.tireContactPatchWidth;
        deformableTerrainInput.contactPatchAreaM2 = state.tireContactPatchArea;
        deformableTerrainInput.currentAverageTreadDepthM = wearBefore.valid
            ? wearBefore.averageTreadDepthM
            : wheel.tireModel.wear.initialTreadDepthM;
        deformableTerrainInput.initialTreadDepthM =
            wheel.tireModel.wear.initialTreadDepthM;
        deformableTerrainInput.minimumTreadDepthM =
            wheel.tireModel.wear.minimumTreadDepthM;
        deformableTerrainBefore = tires::evaluateTireDeformableTerrain(
            wheel.tireModel.deformableTerrainSurface, deformableTerrainInput);
        writeDeformableTerrainTelemetry(
            deformableTerrainBefore, deformableTerrainField);
    }

    const VehicleScalar dedicatedFrictionScale = combineDedicatedSurfaceScale(
        wetBefore.valid ? wetBefore.frictionScale : VehicleScalar{1.0},
        winterBefore.valid ? winterBefore.frictionScale : VehicleScalar{1.0},
        shallowGranularBefore.valid
            ? shallowGranularBefore.frictionScale : VehicleScalar{1.0},
        deformableTerrainBefore.valid
            ? deformableTerrainBefore.mfFrictionScale : VehicleScalar{1.0},
        VehicleScalar{0.01}, VehicleScalar{1.0});
    const VehicleScalar dedicatedStiffnessScale = combineDedicatedSurfaceScale(
        wetBefore.valid ? wetBefore.stiffnessScale : VehicleScalar{1.0},
        winterBefore.valid ? winterBefore.stiffnessScale : VehicleScalar{1.0},
        shallowGranularBefore.valid
            ? shallowGranularBefore.stiffnessScale : VehicleScalar{1.0},
        deformableTerrainBefore.valid
            ? deformableTerrainBefore.stiffnessScale : VehicleScalar{1.0},
        VehicleScalar{0.03}, VehicleScalar{1.0});
    const VehicleScalar dedicatedRelaxationScale = combineDedicatedSurfaceScale(
        wetBefore.valid ? wetBefore.relaxationScale : VehicleScalar{1.0},
        winterBefore.valid ? winterBefore.relaxationScale : VehicleScalar{1.0},
        shallowGranularBefore.valid
            ? shallowGranularBefore.relaxationScale : VehicleScalar{1.0},
        deformableTerrainBefore.valid
            ? deformableTerrainBefore.relaxationScale : VehicleScalar{1.0},
        VehicleScalar{1.0}, VehicleScalar{5.0});
    const VehicleScalar dedicatedRollingResistanceScale = combineDedicatedSurfaceScale(
        wetBefore.valid ? wetBefore.rollingResistanceScale : VehicleScalar{1.0},
        winterBefore.valid ? winterBefore.rollingResistanceScale : VehicleScalar{1.0},
        shallowGranularBefore.valid
            ? shallowGranularBefore.rollingResistanceScale : VehicleScalar{1.0},
        deformableTerrainBefore.valid
            ? deformableTerrainBefore.rollingResistanceScale : VehicleScalar{1.0},
        VehicleScalar{1.0}, VehicleScalar{6.0});

    tires::TireSlipDynamicsDescription slipDynamics;
    const VehicleScalar unloadedRadiusM = wheel.tireModel.magicFormulaUsesLegacySeed
        ? description.radius
        : wheel.tireModel.magicFormula.unloadedRadiusM;
    const VehicleScalar nominalLoadN = wheel.tireModel.magicFormulaUsesLegacySeed
        ? wheel.tireModel.nominalLoad
        : wheel.tireModel.magicFormula.nominalLoadN;
    const VehicleScalar lateralCamberSensitivity =
        wheel.tireModel.magicFormulaUsesLegacySeed
            ? VehicleScalar{0.0}
            : wheel.tireModel.magicFormula.pKy3;
    slipDynamics.longitudinalRelaxationLengthM =
        tires::magicFormulaLongitudinalRelaxationLengthM(
            wheel.tireModel.slipDynamicsCoefficients,
            suspensionForce,
            nominalLoadN,
            unloadedRadiusM,
            wheel.tireModel.longitudinalRelaxationLength)
        * surface.relaxationMultiplier
        * dedicatedRelaxationScale;
    slipDynamics.lateralRelaxationLengthM =
        tires::magicFormulaLateralRelaxationLengthM(
            wheel.tireModel.slipDynamicsCoefficients,
            suspensionForce,
            nominalLoadN,
            unloadedRadiusM,
            radians(state.camberAngleDegrees),
            lateralCamberSensitivity,
            wheel.tireModel.lateralRelaxationLength)
        * surface.relaxationMultiplier
        * dedicatedRelaxationScale;
    slipDynamics.minimumTransportSpeedMps = 0.50;
    tires::TireSlipDynamicsState slipState;
    slipState.longitudinalSlip = state.relaxedSlipRatio;
    slipState.slipAngleRadians = radians(state.relaxedSlipAngleDegrees);
    tires::integrateTireSlipDynamics(
        slipDynamics,
        state.slipRatio,
        slipAngleRadians,
        structuralLongitudinalSpeed,
        substepDeltaTime,
        slipState);
    state.relaxedSlipRatio = slipState.longitudinalSlip;
    state.relaxedSlipAngleDegrees = degrees(slipState.slipAngleRadians);

    const VehicleScalar relaxedAngleRadians = std::clamp(
        slipState.slipAngleRadians,
        radians(VehicleScalar{-75.0}),
        radians(VehicleScalar{75.0}));

    const heritage::math::Vec3 chassisAngularVelocityRadians = {
        static_cast<float>(radians(chassisAngularVelocityDegrees.x)),
        static_cast<float>(radians(chassisAngularVelocityDegrees.y)),
        static_cast<float>(radians(chassisAngularVelocityDegrees.z))
    };
    const VehicleScalar chassisYawRateRadiansPerSecond = dot(
        chassisAngularVelocityRadians, state.contactNormal);
    const VehicleScalar wheelYawRateRadiansPerSecond =
        chassisYawRateRadiansPerSecond + steeringYawRateRadiansPerSecond;

    tires::TireContactPatchDescription contactPatchDescription;
    tires::TireContactPatchInput contactPatchInput;
    contactPatchInput.wheelYawRateRadiansPerSecond =
        wheelYawRateRadiansPerSecond;
    contactPatchInput.forwardSpeedMps = structuralLongitudinalSpeed;
    contactPatchInput.normalLoadN = suspensionForce;
    contactPatchInput.effectiveFriction = std::max(
        wheel.tireModel.peakFriction * surface.frictionMultiplier,
        VehicleScalar{0.0});
    contactPatchInput.unloadedRadiusM =
        activeMagicFormula.unloadedRadiusM > 0.05
            ? activeMagicFormula.unloadedRadiusM
            : static_cast<VehicleScalar>(description.radius);
    contactPatchInput.zeroSpeedTurnMomentCoefficient =
        activeMagicFormula.qCrP1;
    contactPatchInput.parkingMomentScale = activeMagicFormula.lMp;
    const tires::TireContactPatchOutput contactPatch =
        tires::integrateTireContactPatch(
            contactPatchDescription,
            contactPatchInput,
            substepDeltaTime,
            wheel.contactPatchState);

    state.turnSlipPerM = contactPatch.turnSlipPerM;
    state.contactPatchTwistDegrees = degrees(
        contactPatch.torsionalTwistRadians);
    state.parkingTurnMoment = contactPatch.parkingTurnMomentNm;

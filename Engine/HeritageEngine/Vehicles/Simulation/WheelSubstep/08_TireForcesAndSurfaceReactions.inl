// CLEAN03B wheel-substep phase: 08_TireForcesAndSurfaceReactions
// Evaluate wear/contamination and tire forces, then add bounded granular/deformable/fluid reactions and publish force telemetry.
// This file is intentionally included inside VehicleSystem::simulateWheelSubstep().
// It preserves the validated lexical scope and statement order while making phase ownership explicit.

    if (wheel.tireModel.wear.enabled)
    {
        wearReadInput.grounded = true;
        wearReadInput.wheelRotationDegrees = state.wheelRotationDegrees;
        wearReadInput.normalLoadN = suspensionForce;
        wearReadInput.nominalLoadN = nominalLoadN;
        wearReadInput.camberAngleRadians = radians(state.camberAngleDegrees);
        wearReadInput.inflationPressurePa = dynamicInflationPressurePa;
        wearReadInput.referencePressurePa =
            wheel.tireModel.referenceInflationPressurePa;
        wearReadInput.bulkTreadTemperatureC = thermalBefore.valid
            ? thermalBefore.treadTemperatureC : VehicleScalar{20.0};
        wearBefore = tires::evaluateTireWearState(
            wheel.tireModel.wear, wheel.tireModel.thermal,
            wearReadInput, wheel.wearState);
        writeWearTelemetry(wearBefore);
    }

    tires::TireWearOutput wearAfter = wearBefore;

    // TIRE15C samples one shared, world-owned rubber state before this contact
    // modifies it. Deposited rubber can improve a dry racing line, wet rubber
    // can be less favorable, and loose rubber/marbles reduce grip and feed the
    // existing TIRE11 tread-pickup channel.
    const heritage::physics::rubber::TrackRubberSample trackRubberBefore =
        surfaces.sampleTrackRubber(
            hit.point, hit.surfaceMaterial,
            static_cast<float>(hitSurfaceConditions.wetness));

    tires::TireContaminationInput contaminationInput;
    if (wheel.tireModel.contamination.enabled)
    {
        contaminationInput.grounded = true;
        contaminationInput.surfaceMaterial = hit.surfaceMaterial;
        contaminationInput.surfaceWetness = hitSurfaceConditions.wetness;
        contaminationInput.footprintSurfaceBlendValid =
            wheel.cachedFootprintMaterialBlendValid;
        contaminationInput.footprintGrassFraction =
            wheel.cachedFootprintGrassFraction;
        contaminationInput.footprintDirtFraction =
            wheel.cachedFootprintDirtFraction;
        contaminationInput.footprintGravelFraction =
            wheel.cachedFootprintGravelFraction;
        contaminationInput.footprintMudFraction =
            wheel.cachedFootprintMudFraction;
        contaminationInput.footprintSandFraction =
            wheel.cachedFootprintSandFraction;
        contaminationInput.footprintSoftSoilFraction =
            wheel.cachedFootprintSoftSoilFraction;
        contaminationInput.footprintDeepSnowFraction =
            wheel.cachedFootprintDeepSnowFraction;
        contaminationInput.footprintCleanHardFraction =
            wheel.cachedFootprintCleanHardFraction;
        contaminationInput.footprintAverageWetness =
            wheel.cachedFootprintAverageWetness;
        contaminationInput.surfaceRubberDebrisFraction =
            static_cast<VehicleScalar>(trackRubberBefore.pickupAvailability);
        contaminationInput.wheelRotationDegrees = state.wheelRotationDegrees;
        contaminationInput.normalLoadN = suspensionForce;
        contaminationInput.nominalLoadN = nominalLoadN;
        contaminationInput.camberAngleRadians = radians(state.camberAngleDegrees);
        contaminationInput.inflationPressurePa = dynamicInflationPressurePa;
        contaminationInput.referencePressurePa =
            wheel.tireModel.referenceInflationPressurePa;
        contaminationInput.forwardSpeedMps = structuralLongitudinalSpeed;
        contaminationInput.longitudinalSlipVelocityMps =
            circumferentialSpeed - structuralLongitudinalSpeed;
        contaminationInput.lateralSlipVelocityMps = structuralLateralSpeed;
        contaminationInput.bulkTreadTemperatureC = thermalBefore.valid
            ? thermalBefore.treadTemperatureC : VehicleScalar{20.0};
        contaminationBefore = tires::evaluateTireContamination(
            wheel.tireModel.contamination, wheel.tireModel.wear,
            contaminationInput, wheel.wearState);
        writeContaminationTelemetry(contaminationBefore);
    }

    TireContactInput tireInput;
    tireInput.normalLoad = suspensionForce;
    tireInput.longitudinalSlip = state.relaxedSlipRatio;
    tireInput.slipAngleRadians = relaxedAngleRadians;
    tireInput.camberAngleRadians = radians(state.camberAngleDegrees);
    tireInput.forwardSpeedMps = structuralLongitudinalSpeed;
    tireInput.turnSlipPerM = contactPatch.turnSlipPerM;
    tireInput.contactPatchTurnMomentNm = contactPatch.parkingTurnMomentNm;
    tireInput.wheelRadiusM = contactGeometryDescription.unloadedRadiusM;
    tireInput.inflationPressurePa = dynamicInflationPressurePa;
    tireInput.frictionMultiplier = surface.frictionMultiplier
        * (thermalBefore.valid ? thermalBefore.frictionScale : VehicleScalar{1.0})
        * (wearBefore.valid ? wearBefore.contactFrictionScale : VehicleScalar{1.0})
        * (contaminationBefore.valid
            ? contaminationBefore.contactFrictionScale : VehicleScalar{1.0})
        * dedicatedFrictionScale
        * static_cast<VehicleScalar>(trackRubberBefore.contactFrictionScale);
    tireInput.stiffnessMultiplier = surface.stiffnessMultiplier
        * (thermalBefore.valid ? thermalBefore.stiffnessScale : VehicleScalar{1.0})
        * dedicatedStiffnessScale;
    const TireForceResult tireResult = evaluateAdvancedRoadTire(
        wheel.tireModel,
        tireInput);

    VehicleScalar longitudinalForce = tireResult.longitudinalForce;
    // TIRE03 now owns torsional parking behavior in TireContactPatch. Keep
    // this separate translational standstill/creep stabilizer until a future
    // brush/contact-mass state can replace it without regressing parked and
    // turn-then-brake stability. The final friction-circle clamp still bounds
    // it by the tire's current normal load and grip.
    const VehicleScalar lowSpeedBlend = smoothStep01(
        (std::abs(structuralLongitudinalSpeed) - kLowSpeedTireBlendStart)
        / (kLowSpeedTireBlendEnd - kLowSpeedTireBlendStart));
    const VehicleScalar lowSpeedLateralForce =
        -vehicle.description.lateralStiffness * structuralLateralSpeed;
    VehicleScalar lateralForce = lowSpeedLateralForce
        + (tireResult.lateralForce - lowSpeedLateralForce)
            * lowSpeedBlend;
    const VehicleScalar pressureServiceability = std::clamp(
        dynamicInflationPressurePa
            / std::max(wheel.tireModel.referenceInflationPressurePa,
                VehicleScalar{1.0}),
        VehicleScalar{0.0}, VehicleScalar{1.0});
    const VehicleScalar flatTireRollingResistanceScale =
        VehicleScalar{1.0}
            + VehicleScalar{4.0}
                * (VehicleScalar{1.0} - pressureServiceability)
                * (VehicleScalar{1.0} - pressureServiceability);
    const VehicleScalar rollingResistanceForce =
        -vehicle.description.rollingResistance
        * surface.rollingResistanceMultiplier
        * (contaminationBefore.valid
            ? contaminationBefore.rollingResistanceScale : VehicleScalar{1.0})
        * dedicatedRollingResistanceScale
        * static_cast<VehicleScalar>(trackRubberBefore.rollingResistanceScale)
        * flatTireRollingResistanceScale
        * longitudinalSpeed;
    longitudinalForce += rollingResistanceForce;

    // Preserve the contact-force safety bound after the legacy rolling
    // resistance term is applied. Rolling resistance is kept separate
    // from pure/combined-slip telemetry, but it must not push the final
    // contact vector beyond the tire's available friction force.
    const VehicleScalar forceLimit = tireResult.effectiveFriction * suspensionForce;
    const VehicleScalar finalMagnitude = std::sqrt(
        longitudinalForce * longitudinalForce
        + lateralForce * lateralForce);
    if (finalMagnitude > forceLimit && finalMagnitude > kVectorEpsilon)
    {
        const VehicleScalar scaleToLimit = forceLimit / finalMagnitude;
        longitudinalForce *= scaleToLimit;
        lateralForce *= scaleToLimit;
    }

    // TIRE14: granular shear and lateral bulldozing are additional terrain
    // reactions around the load-bearing MF6.2 base contact. They are applied
    // after the MF friction-circle clamp, then bounded by the independently
    // calculated soil shear/passive-wedge capacity. Longitudinal plowing is a
    // dissipative terrain force and is added after that contact-force bound.
    if (shallowGranularBefore.valid)
    {
        longitudinalForce += shallowGranularBefore.longitudinalShearForceN;
        lateralForce += shallowGranularBefore.lateralShearForceN
            + shallowGranularBefore.lateralBulldozingForceN;

        const VehicleScalar granularForceLimit = std::max(
            forceLimit + shallowGranularBefore.additionalContactCapacityN,
            VehicleScalar{0.0});
        const VehicleScalar granularMagnitude = std::sqrt(
            longitudinalForce * longitudinalForce
            + lateralForce * lateralForce);
        if (granularMagnitude > granularForceLimit
            && granularMagnitude > kVectorEpsilon)
        {
            const VehicleScalar scaleToGranularLimit =
                granularForceLimit / granularMagnitude;
            longitudinalForce *= scaleToGranularLimit;
            lateralForce *= scaleToGranularLimit;
        }

        if (shallowGranularBefore.plowingDragN > 0.0)
        {
            const VehicleScalar plowingDirection =
                structuralLongitudinalSpeed > 0.05
                    ? VehicleScalar{-1.0}
                    : (structuralLongitudinalSpeed < -0.05
                        ? VehicleScalar{1.0} : VehicleScalar{0.0});
            longitudinalForce +=
                plowingDirection * shallowGranularBefore.plowingDragN;
        }
    }

    // TIRE15: on fully deformable terrain the terrain reaction is the primary
    // source of tractive force. MF6.2/SWIFT still supplies the pneumatic tire
    // state and a deliberately reduced residual interface contribution, while
    // the pressure-sinkage/shear model adds soil reaction and plastic plowing.
    if (deformableTerrainBefore.valid)
    {
        longitudinalForce += deformableTerrainBefore.longitudinalTerrainForceN;
        lateralForce += deformableTerrainBefore.lateralTerrainForceN
            + deformableTerrainBefore.lateralBulldozingForceN;

        const VehicleScalar terrainForceLimit = std::max(
            forceLimit + deformableTerrainBefore.additionalContactCapacityN,
            VehicleScalar{0.0});
        const VehicleScalar terrainMagnitude = std::sqrt(
            longitudinalForce * longitudinalForce
            + lateralForce * lateralForce);
        if (terrainMagnitude > terrainForceLimit
            && terrainMagnitude > kVectorEpsilon)
        {
            const VehicleScalar scaleToTerrainLimit =
                terrainForceLimit / terrainMagnitude;
            longitudinalForce *= scaleToTerrainLimit;
            lateralForce *= scaleToTerrainLimit;
        }

        if (deformableTerrainBefore.plowingDragN > 0.0)
        {
            const VehicleScalar plowingDirection =
                structuralLongitudinalSpeed > 0.05
                    ? VehicleScalar{-1.0}
                    : (structuralLongitudinalSpeed < -0.05
                        ? VehicleScalar{1.0} : VehicleScalar{0.0});
            longitudinalForce +=
                plowingDirection * deformableTerrainBefore.plowingDragN;
        }
    }

    // Water-plowing drag is a fluid force, not pavement friction, so apply it
    // after the MF/friction-circle bound. It remains finite and directionally
    // opposes longitudinal motion even as pavement contact approaches zero.
    if (wetBefore.valid && wetBefore.hydrodynamicDragN > 0.0)
    {
        const VehicleScalar waterDirection = structuralLongitudinalSpeed > 0.05
            ? VehicleScalar{-1.0}
            : (structuralLongitudinalSpeed < -0.05
                ? VehicleScalar{1.0} : VehicleScalar{0.0});
        longitudinalForce += waterDirection * wetBefore.hydrodynamicDragN;
    }

    state.effectiveFriction = tireResult.effectiveFriction;
    state.gripUtilization = tireResult.gripUtilization;
    state.pureLongitudinalForce = tireResult.pureLongitudinalForce;
    state.pureLateralForce = tireResult.pureLateralForce;
    state.combinedSlipScale = tireResult.combinedSlipScale;
    state.pneumaticTrail = tireResult.pneumaticTrail;
    state.aligningTorque = tireResult.aligningTorque;
    state.overturningMoment = tireResult.overturningMoment;
    state.rollingResistanceMoment = tireResult.rollingResistanceMoment;
    state.residualAligningTorque = tireResult.residualAligningTorque;
    state.longitudinalSlipStiffness = tireResult.longitudinalSlipStiffness;
    state.corneringStiffness = tireResult.corneringStiffness;
    state.camberStiffness = tireResult.camberStiffness;
    state.normalizedTurnSlip = tireResult.normalizedTurnSlip;
    state.turnSlipMoment = tireResult.turnSlipMoment;
    state.turnSlipLongitudinalReduction =
        tireResult.turnSlipLongitudinalReduction;
    state.turnSlipLateralReduction = tireResult.turnSlipLateralReduction;
    state.turnSlipCorneringReduction = tireResult.turnSlipCorneringReduction;
    state.turnSlipTrailReduction = tireResult.turnSlipTrailReduction;
    state.motorcycleContourValid = tireResult.motorcycleContourValid;
    state.motorcycleContactLateralOffset =
        tireResult.motorcycleContactLateralOffset;
    state.motorcycleCenterToRoad = tireResult.motorcycleCenterToRoad;
    state.longitudinalForce = longitudinalForce;
    state.lateralForce = lateralForce;

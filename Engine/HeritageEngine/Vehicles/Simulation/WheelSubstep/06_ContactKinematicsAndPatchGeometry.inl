// CLEAN03B wheel-substep phase: 06_ContactKinematicsAndPatchGeometry
// Refresh upright/contact kinematics, derive wheel basis and local velocities, and solve physical contact geometry/slip inputs.
// This file is intentionally included inside VehicleSystem::simulateWheelSubstep().
// It preserves the validated lexical scope and statement order while making phase ownership explicit.

    // Contact resolution may have changed suspension compression. Refresh
    // the authoritative upright pose before constructing the tire basis.
    updateUprightGeometry();

    if (state.grounded && state.steeringAxisPointValid)
    {
        const SteeringGroundGeometry steeringGeometry =
            evaluateSteeringGroundGeometry(
                state.worldSteeringAxisPoint,
                state.worldSteeringAxis,
                state.contactPoint,
                state.contactNormal,
                state.worldWheelForward,
                state.worldWheelRight);
        state.steeringGroundGeometryValid = steeringGeometry.valid;
        state.steeringAxisGroundPointWorld =
            steeringGeometry.steeringAxisGroundPointWorld;
        state.signedScrubRadiusM = steeringGeometry.signedScrubRadiusM;
        state.scrubRadiusMagnitudeM =
            steeringGeometry.scrubRadiusMagnitudeM;
        state.mechanicalTrailM = steeringGeometry.mechanicalTrailM;
    }

    // Refresh velocities because earlier wheels in this same 1 ms substep
    // may already have applied impulses to the chassis.
    bodies.linearVelocity(vehicle.description.chassisBody, chassisLinearVelocity);
    bodies.angularVelocityDegrees(
        vehicle.description.chassisBody,
        chassisAngularVelocityDegrees);
    const heritage::math::Vec3 contactOffsetFromCenterOfMass = add(
        wheelRayOffsetFromCenterOfMass,
        scale(suspensionDirection, hit.distance));
    const heritage::math::Vec3 contactVelocity = pointVelocityFromOffset(
        chassisLinearVelocity,
        chassisAngularVelocityDegrees,
        contactOffsetFromCenterOfMass);

    heritage::math::Vec3 wheelForward = state.worldWheelForward;
    wheelForward = subtract(
        wheelForward,
        scale(state.contactNormal, dot(wheelForward, state.contactNormal)));
    wheelForward = normalized(wheelForward, { 0.0f, 0.0f, 1.0f });
    heritage::math::Vec3 wheelRight = normalized(
        cross(state.contactNormal, wheelForward),
        { 1.0f, 0.0f, 0.0f });

    const VehicleScalar longitudinalSpeed = dot(contactVelocity, wheelForward);
    const VehicleScalar lateralSpeed = dot(contactVelocity, wheelRight);
    state.longitudinalSpeed = longitudinalSpeed;
    state.lateralSpeed = lateralSpeed;
    const VehicleScalar structuralLongitudinalSpeed = longitudinalSpeed
        + (rigidRingOutput.valid
            ? rigidRingOutput.longitudinalVelocityMps : VehicleScalar{0.0});
    const VehicleScalar structuralLateralSpeed = lateralSpeed
        + (rigidRingOutput.valid
            ? rigidRingOutput.lateralVelocityMps : VehicleScalar{0.0});

    const tires::MagicFormula62Parameters activeMagicFormula =
        wheel.tireModel.magicFormulaUsesLegacySeed
            ? seededMagicFormula62Parameters(wheel.tireModel, description.radius)
            : wheel.tireModel.magicFormula;

    tires::TireContactGeometryDescription contactGeometryDescription =
        wheel.tireModel.contactGeometry;
    const VehicleScalar nominalUnloadedRadiusM =
        activeMagicFormula.unloadedRadiusM > 0.05
            ? activeMagicFormula.unloadedRadiusM
            : static_cast<VehicleScalar>(description.radius);
    contactGeometryDescription.unloadedRadiusM = std::max(
        nominalUnloadedRadiusM - currentTreadRadiusLossM,
        VehicleScalar{0.05});
    contactGeometryDescription.nominalLoadN =
        contactGeometryDescription.nominalLoadN >= 100.0
            ? contactGeometryDescription.nominalLoadN
            : std::max(activeMagicFormula.nominalLoadN, VehicleScalar{100.0});
    contactGeometryDescription.verticalStiffnessNPerM =
        contactGeometryDescription.verticalStiffnessNPerM >= 1000.0
            ? contactGeometryDescription.verticalStiffnessNPerM
            : std::max(
                static_cast<VehicleScalar>(description.tireRadialStiffness),
                VehicleScalar{1000.0});
    if (contactGeometryDescription.nominalWidthM <= 0.03)
    {
        contactGeometryDescription.nominalWidthM =
            description.fitment.tireWidthMm > 30.0f
                ? static_cast<VehicleScalar>(description.fitment.tireWidthMm) * 0.001
                : VehicleScalar{0.20};
    }
    if (contactGeometryDescription.rimRadiusM <= 0.01)
    {
        const VehicleScalar rimDiameterIn = description.fitment.tireRimDiameterIn > 1.0f
            ? static_cast<VehicleScalar>(description.fitment.tireRimDiameterIn)
            : static_cast<VehicleScalar>(description.fitment.rimDiameterIn);
        contactGeometryDescription.rimRadiusM = rimDiameterIn > 1.0
            ? rimDiameterIn * VehicleScalar{0.0254 * 0.5}
            : contactGeometryDescription.unloadedRadiusM * VehicleScalar{0.60};
    }
    contactGeometryDescription.rimRadiusM = std::clamp(
        contactGeometryDescription.rimRadiusM,
        VehicleScalar{0.01},
        contactGeometryDescription.unloadedRadiusM - VehicleScalar{0.002});
    contactGeometryDescription.referenceSpeedMps =
        contactGeometryDescription.referenceSpeedMps > 0.1
            ? contactGeometryDescription.referenceSpeedMps
            : std::max(activeMagicFormula.referenceSpeedMps, VehicleScalar{0.1});

    tires::TireContactGeometryInput contactGeometryInput;
    contactGeometryInput.normalLoadN = suspensionForce;
    contactGeometryInput.wheelAngularVelocityRadPerS =
        state.wheelAngularVelocity;
    const VehicleScalar dynamicInflationPressurePa = thermalBefore.valid
        ? thermalBefore.inflationPressurePa
        : wheel.tireModel.inflationPressurePa;
    contactGeometryInput.inflationPressurePa = dynamicInflationPressurePa;
    contactGeometryInput.verticalDeflectionKnown =
        description.effectiveUnsprungMass > 0.001f;
    contactGeometryInput.verticalDeflectionM = state.tireDeflection;
    const tires::TireContactGeometryOutput contactGeometry =
        tires::evaluateTireContactGeometry(
            contactGeometryDescription, contactGeometryInput);
    if (contactGeometry.valid)
    {
        state.tireFreeRollingRadius = contactGeometry.freeRollingRadiusM;
        state.tireLoadedRadius = contactGeometry.loadedRadiusM;
        state.tireEffectiveRollingRadius =
            contactGeometry.effectiveRollingRadiusM;
        state.tireContactPatchLength = contactGeometry.contactPatchLengthM;
        state.tireContactPatchWidth = contactGeometry.contactPatchWidthM;
        state.tireContactPatchArea = contactGeometry.contactPatchAreaM2;
    }
    const VehicleScalar effectiveRollingRadius =
        state.tireEffectiveRollingRadius > 0.05
            ? state.tireEffectiveRollingRadius
            : static_cast<VehicleScalar>(description.radius);

    wheel.flexibleRingDemandSeconds = std::max(
        wheel.flexibleRingDemandSeconds - substepDeltaTime,
        VehicleScalar{0.0});

    // TIRE44 — physics-owned dynamic carcass.
    //
    // The previous TIRE41 runtime path prescribed tireDeflection as a radial
    // vertex target and immediately solved an equilibrium field during the
    // render bridge. That can make a supported lower sidewall curl inward: a
    // scalar loaded-radius loss is not the same thing as the 3D carcass shape.
    //
    // The dynamic lattice below receives no prescribed vertical-deformation
    // geometry. Suspension/contact physics already placed the wheel centre.
    // Road-envelope collider points then become unilateral contact boundaries;
    // pressure, carcass stiffness, neighbour tension and moving rigid-ring
    // anchors determine the displacement state over time.
    if (wheel.flexibleRingDemandSeconds > VehicleScalar{0.0})
    {
        wheel.flexibleRingAccumulatorSeconds += substepDeltaTime;
    }
    else
    {
        wheel.flexibleRingAccumulatorSeconds = 0.0;
    }
    const VehicleScalar flexibleRingRateScale =
        wheel.flexibleRingDevelopmentTuning.enabled
            ? std::clamp(
                wheel.flexibleRingDevelopmentTuning.structuralRateScale,
                VehicleScalar{0.25}, VehicleScalar{8.0})
            : VehicleScalar{1.0};
    const VehicleScalar kFlexibleRingStepSeconds =
        VehicleScalar{0.008} / flexibleRingRateScale; // 125 Hz at production default
    if (wheel.flexibleRingDemandSeconds > VehicleScalar{0.0}
        && (!wheel.flexibleRingState.initialized
            || wheel.flexibleRingAccumulatorSeconds + VehicleScalar{1.0e-12}
                >= kFlexibleRingStepSeconds))
    {
        const VehicleScalar carcassDeltaTime = wheel.flexibleRingState.initialized
            ? std::min(
                wheel.flexibleRingAccumulatorSeconds,
                kFlexibleRingStepSeconds * VehicleScalar{2.0})
            : kFlexibleRingStepSeconds;
        wheel.flexibleRingAccumulatorSeconds = 0.0;

        tires::TireFlexibleRingFieldDescription carcassDescription;
        carcassDescription.unloadedRadiusM =
            contactGeometryDescription.unloadedRadiusM;
        carcassDescription.rimRadiusM =
            contactGeometryDescription.rimRadiusM;
        carcassDescription.sectionWidthM =
            contactGeometryDescription.nominalWidthM;
        carcassDescription.maximumDeflectionM = std::clamp(
            static_cast<VehicleScalar>(description.maximumTireDeflection),
            VehicleScalar{0.015},
            std::max(
                contactGeometryDescription.unloadedRadiusM
                    - contactGeometryDescription.rimRadiusM,
                VehicleScalar{0.015}));
        carcassDescription.referencePressurePa =
            wheel.tireModel.referenceInflationPressurePa > 20000.0
                ? wheel.tireModel.referenceInflationPressurePa
                : VehicleScalar{220000.0};
        carcassDescription.verticalStiffnessNPerM =
            contactGeometryDescription.verticalStiffnessNPerM;

        tires::TireFlexibleRingDynamicsInput carcassInput;
        carcassInput.deltaTimeSeconds = carcassDeltaTime;
        carcassInput.grounded = state.grounded;
        carcassInput.inflationPressurePa = dynamicInflationPressurePa;
        carcassInput.thermalStiffnessScale =
            state.tireThermalStiffnessScale > 0.0
                ? state.tireThermalStiffnessScale : VehicleScalar{1.0};
        carcassInput.normalLoadN = state.normalForce;
        // TIRE45J: feed the last completed physical tire-force state into the
        // carcass presentation gate. The current 1 kHz substep computes MF6.2
        // forces later in phase 08, so these values are intentionally 1 ms old.
        // That preserves phase ordering while restoring real cornering flex.
        carcassInput.lateralForceN = state.grounded
            ? previousLateralTireForce : VehicleScalar{0.0};
        carcassInput.aligningMomentNm = state.grounded
            ? previousAligningTorque : VehicleScalar{0.0};
        carcassInput.slipAngleRadians = state.grounded
            ? previousSlipAngleRadians : VehicleScalar{0.0};
        carcassInput.forwardSpeedMps = structuralLongitudinalSpeed;
        carcassInput.wheelAngularVelocityRadPerS = state.wheelAngularVelocity;
        carcassInput.ringLongitudinalOffsetM = state.tireRingLongitudinalOffset;
        carcassInput.ringLateralOffsetM = state.tireRingLateralOffset;
        carcassInput.ringYawRadians = radians(state.tireRingYawDegrees);
        carcassInput.ringWindupRadians = radians(state.tireRingWindupDegrees);
        carcassInput.contactPatchTwistRadians =
            radians(state.contactPatchTwistDegrees);
        carcassInput.flatSpotDepthM =
            state.tireFlatSpotDepthMm * VehicleScalar{0.001};
        carcassInput.flatSpotSector = state.tireFlatSpotSector;
        carcassInput.wheelRotationRadians = radians(state.wheelRotationDegrees);
        carcassInput.developmentTuning =
            wheel.flexibleRingDevelopmentTuning.enabled
                ? &wheel.flexibleRingDevelopmentTuning : nullptr;

        const heritage::math::Vec3 fieldForward = normalized(
            state.worldWheelForward, { 0.0f, 0.0f, 1.0f });
        const heritage::math::Vec3 fieldRight = normalized(
            state.worldWheelRight, { 1.0f, 0.0f, 0.0f });
        const heritage::math::Vec3 fieldDown = scale(
            normalized(state.worldWheelUp, { 0.0f, 1.0f, 0.0f }),
            VehicleScalar{-1.0});

        const auto appendCarcassRoadSample = [&carcassInput,
            &state, &fieldForward, &fieldDown, &fieldRight](
                bool queried,
                bool supported,
                VehicleScalar queryForwardM,
                VehicleScalar queryLateralM,
                const heritage::math::Vec3& pointWorld,
                const heritage::math::Vec3& normalWorld) {
            if (carcassInput.roadSampleCount
                >= tires::TireFlexibleRingMaximumRoadSamples)
            {
                return;
            }
            auto& target = carcassInput.roadSamples[
                carcassInput.roadSampleCount++];
            target.queried = queried;
            target.supported = supported;
            target.queryForwardM = queryForwardM;
            target.queryLateralM = queryLateralM;
            if (!supported)
                return;

            const heritage::math::Vec3 relative = subtract(
                pointWorld, state.worldCenter);
            target.pointForwardM = dot(relative, fieldForward);
            target.pointDownM = dot(relative, fieldDown);
            target.pointLateralM = dot(relative, fieldRight);
            const heritage::math::Vec3 normal = normalized(
                normalWorld, state.contactNormal);
            target.normalForward = dot(normal, fieldForward);
            target.normalDown = dot(normal, fieldDown);
            target.normalLateral = dot(normal, fieldRight);
        };

        // The 1 kHz centre support hit is always the first true collision
        // boundary. Additional TIRE06 road-envelope queries are appended below;
        // on a refined footprint explicit misses are retained to preserve road
        // edges and partial support.
        if (state.grounded)
        {
            appendCarcassRoadSample(
                true, true, VehicleScalar{0.0}, VehicleScalar{0.0},
                state.contactPoint, state.contactNormal);
        }
        for (std::size_t sampleIndex = 0;
             sampleIndex < wheel.carcassRoadSampleCount; ++sampleIndex)
        {
            const auto& source = wheel.carcassRoadSamples[sampleIndex];
            if (std::abs(source.queryForwardM) <= VehicleScalar{1.0e-7}
                && std::abs(source.queryLateralM) <= VehicleScalar{1.0e-7})
            {
                continue; // centre already came from the current 1 kHz hit
            }
            // TIRE45B: the cached envelope sample describes shape around
            // the centre query. Re-anchor that shape to THIS substep's live
            // centre contact. This prevents forward vehicle travel between
            // envelope refreshes from masquerading as carcass deformation.
            const heritage::math::Vec3 reanchoredPointWorld = add(
                state.contactPoint,
                source.pointDeltaFromCenterContactWorld);
            appendCarcassRoadSample(
                source.queried,
                source.supported,
                source.queryForwardM,
                source.queryLateralM,
                reanchoredPointWorld,
                source.normalWorld);
        }

        wheel.flexibleRingOutput = tires::advanceTireFlexibleRingDynamics(
            carcassDescription,
            carcassInput,
            wheel.flexibleRingState);
    }

    VehicleScalar differentialLockTorque = 0.0;
    if (vehicle.powertrain.differentialMode == DifferentialMode::Locked
        && description.driveFactor > 0.0f)
    {
        constexpr VehicleScalar lockingStrength = 180.0;
        differentialLockTorque = -(
            state.wheelAngularVelocity - driveline.drivenOmega)
            * lockingStrength;
    }

    const VehicleScalar beltSpinVelocity = rigidRingOutput.valid
        ? rigidRingOutput.windupAngularVelocityRadPerS
        : VehicleScalar{0.0};
    const VehicleScalar circumferentialSpeed =
        (state.wheelAngularVelocity + beltSpinVelocity) * effectiveRollingRadius;
    constexpr VehicleScalar kSlipRegularizationSpeedMps = 0.50;
    const VehicleScalar dominantRollingSpeed = std::max(
        std::abs(structuralLongitudinalSpeed), std::abs(circumferentialSpeed));
    const VehicleScalar slipDenominator = std::sqrt(
        dominantRollingSpeed * dominantRollingSpeed
        + kSlipRegularizationSpeedMps * kSlipRegularizationSpeedMps);
    state.slipRatio = std::clamp(
        (circumferentialSpeed - structuralLongitudinalSpeed) / slipDenominator,
        -5.0,
        5.0);
    const VehicleScalar lateralReferenceSpeed = std::sqrt(
        structuralLongitudinalSpeed * structuralLongitudinalSpeed
        + kSlipRegularizationSpeedMps * kSlipRegularizationSpeedMps);
    const VehicleScalar beltYawAngle = rigidRingOutput.valid
        ? rigidRingOutput.yawAngleRadians : VehicleScalar{0.0};
    const VehicleScalar slipAngleRadians = std::atan2(
        structuralLateralSpeed, lateralReferenceSpeed) - beltYawAngle;
    state.slipAngleDegrees = degrees(slipAngleRadians);

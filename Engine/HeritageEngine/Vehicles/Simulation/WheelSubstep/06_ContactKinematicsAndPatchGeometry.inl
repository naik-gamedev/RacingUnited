// CLEAN03B wheel-substep phase: 06_ContactKinematicsAndPatchGeometry
// Refresh upright/contact kinematics, derive wheel basis and local velocities, and solve physical contact geometry/slip inputs.
// This file is intentionally included inside VehicleSystem::simulateWheelSubstep().
// It preserves the validated lexical scope and statement order while making phase ownership explicit.

    // Contact resolution may have changed suspension compression. Refresh
    // the authoritative upright pose before constructing the tire basis.
    updateUprightGeometry();

    // TIRE17C7/VIS10: reference-quality visual collider neighborhood. The
    // existing immutable static-scene BVH selects exact nearby creator
    // collision triangles; the GPU later applies them per tire mesh vertex.
    // This is deliberately independent from the 3x3 force-envelope provider.
    // It solves visual curb/vertical-face penetration without turning the tire
    // into hundreds of CPU physics particles.
    constexpr VehicleScalar kVisualColliderRefreshHz = 500.0;
    constexpr VehicleScalar kVisualColliderRefreshPeriodS =
        VehicleScalar{1.0} / kVisualColliderRefreshHz;
    wheel.visualColliderQueryAccumulatorSeconds +=
        static_cast<VehicleScalar>(substepDeltaTime);
    if (wheel.visualColliderQueryAccumulatorSeconds >= kVisualColliderRefreshPeriodS
        || state.tireVisualColliderTriangleCount == 0)
    {
        wheel.visualColliderQueryAccumulatorSeconds = std::fmod(
            wheel.visualColliderQueryAccumulatorSeconds,
            kVisualColliderRefreshPeriodS);
        const heritage::math::Vec3 visualWheelCenterWorld = add(
            wheelRayOriginWorld,
            scale(suspensionDirection, state.suspensionLength));
        const VehicleScalar authoredWidthM = description.fitment.tireWidthMm > 30.0f
            ? static_cast<VehicleScalar>(description.fitment.tireWidthMm) * 0.001
            : VehicleScalar{0.20};
        const float queryHalfExtent = static_cast<float>(
            std::max(
                static_cast<VehicleScalar>(description.radius) + authoredWidthM * 0.55
                    + VehicleScalar{0.10},
                VehicleScalar{0.40}));

        std::vector<heritage::physics::StaticSceneTriangle> nearbyTriangles;
        collisions.nearbyStaticSceneTriangles(
            visualWheelCenterWorld,
            queryHalfExtent,
            TireVisualColliderTriangleLimit,
            nearbyTriangles);

        state.tireVisualColliderTriangleCount = static_cast<std::uint32_t>(
            (std::min)(nearbyTriangles.size(), TireVisualColliderTriangleLimit));
        state.tireVisualColliderTrianglesValid =
            state.tireVisualColliderTriangleCount > 0;
        for (std::size_t triangleIndex = 0;
             triangleIndex < state.tireVisualColliderTriangleCount;
             ++triangleIndex)
        {
            const auto& sourceTriangle = nearbyTriangles[triangleIndex];
            heritage::math::Vec3 normal = normalized(
                sourceTriangle.normal, { 0.0f, 1.0f, 0.0f });
            const heritage::math::Vec3 towardWheel = subtract(
                visualWheelCenterWorld, sourceTriangle.a);
            if (dot(normal, towardWheel) < 0.0)
                normal = scale(normal, -1.0);

            auto& destination = state.tireVisualColliderTriangles[triangleIndex];
            destination.a = sourceTriangle.a;
            destination.b = sourceTriangle.b;
            destination.c = sourceTriangle.c;
            destination.normal = normal;
        }
    }

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


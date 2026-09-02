// CLEAN03B wheel-substep phase: 00_PrepareWheelAndSupportQuery
// Establish per-wheel references, previous-state snapshot, support ray, and zeroed telemetry state.
// This file is intentionally included inside VehicleSystem::simulateWheelSubstep().
// It preserves the validated lexical scope and statement order while making phase ownership explicit.

    const Quaternion chassisRotation = quaternionFromEulerDegrees(
        chassisPose.rotationDegrees);

    WheelRecord& wheel = vehicle.wheels[wheelIndex];
    WheelState& state = wheel.state;
    const WheelDescription& description = wheel.description;
    const heritage::math::Vec3 flexedLocalMount =
        applyChassisSectionTwistToPoint(
            description.localMount,
            vehicle.chassisFlex.torsionAxisLocalY,
            chassisSectionTwistRadiansValue);
    const heritage::math::Vec3 flexedCenterlineOffsetLocal =
        applyChassisSectionTwistToVector(
            wheelCenterlineFitmentOffsetLocal(description),
            chassisSectionTwistRadiansValue);

    // SUSP08: a live axle is one rigid unsprung member, so each wheel needs
    // the opposite wheel's compression from the common pre-loop snapshot.
    // Prefer the authored axle id; the geometric fallback exists for native
    // test/prototype wheels created without VehicleDefinition metadata.
    bool pairedCompressionValid = false;
    float pairedCompressionM = 0.0f;
    bool pairedCompressionVelocityValid = false;
    float pairedCompressionVelocityMps = 0.0f;
    if (description.suspensionProvider == SuspensionProviderKind::LiveAxleV1
        || description.suspensionProvider == SuspensionProviderKind::LeafSpringLiveAxleV1
        || description.suspensionProvider == SuspensionProviderKind::TwistBeamV1)
    {
        std::size_t partner = vehicle.wheels.size();
        float bestScore = 1.0e9f;
        for (std::size_t index = 0; index < vehicle.wheels.size(); ++index)
        {
            if (index == wheelIndex
                || vehicle.wheels[index].description.suspensionProvider
                    != description.suspensionProvider)
                continue;
            const WheelDescription& candidate = vehicle.wheels[index].description;
            const bool sameAuthoredAxle = !description.suspensionAxleId.empty()
                && candidate.suspensionAxleId == description.suspensionAxleId;
            const float dz = std::abs(candidate.localMount.z - description.localMount.z);
            const bool oppositeSide = candidate.localMount.x * description.localMount.x <= 0.0f;
            if (!sameAuthoredAxle && !(dz < 0.08f && oppositeSide))
                continue;
            const float score = sameAuthoredAxle ? dz : dz + 0.25f;
            if (score < bestScore)
            {
                bestScore = score;
                partner = index;
            }
        }
        if (partner < vehicle.wheels.size()
            && partner < vehicle.suspensionCompressionScratch.size())
        {
            pairedCompressionValid = true;
            pairedCompressionM = static_cast<float>(
                vehicle.suspensionCompressionScratch[partner]);
            pairedCompressionVelocityValid = true;
            pairedCompressionVelocityMps = static_cast<float>(
                vehicle.wheels[partner].state.compressionVelocity);
        }
    }

    // SUSP05: hardpoint providers already solve the real wheel-centre path.
    // Use the previous 1 kHz compression/steer state to move this substep's
    // support query laterally/longitudinally with that path. Travel along the
    // suspension axis remains owned by the ray/unsprung solve below, avoiding
    // double-counting bump/droop. This one-substep predictor is the same bounded
    // explicit ordering used by motorcycle crown contact and is effectively
    // continuous at the native 1 kHz vehicle rate.
    const SuspensionSupportOffsetOutput suspensionSupportOffset =
        evaluateSuspensionSupportOffset(
            suspensionGeometryDescription(description),
            { static_cast<float>(state.compression),
              static_cast<float>(state.steerAngleDegrees),
              description.localSuspensionDirection,
              pairedCompressionValid,
              pairedCompressionM,
              pairedCompressionVelocityValid,
              static_cast<float>(state.compressionVelocity),
              pairedCompressionVelocityMps });
    const heritage::math::Vec3 flexedSuspensionSupportOffsetLocal =
        applyChassisSectionTwistToVector(
            suspensionSupportOffset.valid
                ? suspensionSupportOffset.localTransverseOffset
                : heritage::math::Vec3{},
            chassisSectionTwistRadiansValue);
    const heritage::math::Vec3 flexedWheelRayLocalMount = add(
        add(flexedLocalMount, flexedCenterlineOffsetLocal),
        flexedSuspensionSupportOffsetLocal);
    const heritage::math::Vec3 flexedSuspensionDirectionLocal =
        applyChassisSectionTwistToVector(
            description.localSuspensionDirection,
            chassisSectionTwistRadiansValue);
    const bool wasGrounded = state.grounded;
    const WheelContactStatus previousContactStatus =
        state.contactStatus;
    const VehicleScalar previousSlipRatio = state.slipRatio;
    const VehicleScalar previousSlipAngleRadians = radians(state.slipAngleDegrees);
    const VehicleScalar previousLongitudinalSpeed = state.longitudinalSpeed;
    const VehicleScalar previousLateralSpeed = state.lateralSpeed;
    const VehicleScalar previousNormalForce = state.normalForce;
    const VehicleScalar previousLongitudinalTireForce = state.longitudinalForce;
    const VehicleScalar previousLateralTireForce = state.lateralForce;
    const VehicleScalar previousAligningTorque = state.aligningTorque;
    const VehicleScalar previousContactPatchLength = state.tireContactPatchLength;
    const VehicleScalar previousContactPatchWidth = state.tireContactPatchWidth;
    const VehicleScalar previousContactPatchArea = state.tireContactPatchArea;
    const VehicleScalar previousEffectiveRollingRadius =
        state.tireEffectiveRollingRadius > 0.05
            ? state.tireEffectiveRollingRadius
            : static_cast<VehicleScalar>(description.radius);
    // TIRE46 motorcycle contact authority uses the prior 1 kHz wheel-right
    // basis to place the support ray under the rounded crown. Upright geometry
    // is refreshed later in this same substep; a one-substep-old basis avoids a
    // circular support/kinematics solve and is effectively continuous at 1 kHz.
    const heritage::math::Vec3 previousWorldWheelRight = normalized(
        state.worldWheelRight, { 1.0f, 0.0f, 0.0f });

    // TIRE10: query the material-fixed tread field before the support ray is
    // converted into a hub datum. This is what makes a real flat spot a
    // geometric radius variation rather than merely a friction/visual effect.
    // The read is state-only; wear still advances later after contact forces
    // and dissipated energy are known. A one-substep-old camber/pressure datum
    // is sufficient for the three-band weighting and avoids a circular solve.
    const tires::TireThermalOutput preContactThermal =
        tires::evaluateTireThermalState(
            wheel.tireModel.thermal, wheel.thermalState);
    tires::TireWearInput preContactWearInput;
    preContactWearInput.grounded = false;
    preContactWearInput.wheelRotationDegrees = state.wheelRotationDegrees;
    preContactWearInput.nominalLoadN = wheel.tireModel.nominalLoad;
    preContactWearInput.camberAngleRadians = radians(state.camberAngleDegrees);
    preContactWearInput.inflationPressurePa = preContactThermal.valid
        ? preContactThermal.inflationPressurePa
        : wheel.tireModel.inflationPressurePa;
    preContactWearInput.referencePressurePa =
        wheel.tireModel.referenceInflationPressurePa;
    preContactWearInput.bulkTreadTemperatureC = preContactThermal.valid
        ? preContactThermal.treadTemperatureC : VehicleScalar{20.0};
    const tires::TireWearOutput preContactWear = tires::evaluateTireWearState(
        wheel.tireModel.wear, wheel.tireModel.thermal,
        preContactWearInput, wheel.wearState);
    const VehicleScalar currentTreadRadiusLossM = preContactWear.valid
        ? std::clamp(
            preContactWear.contactTreadRadiusLossM,
            VehicleScalar{0.0},
            std::max(
                static_cast<VehicleScalar>(description.radius)
                    - VehicleScalar{0.05},
                VehicleScalar{0.0}))
        : VehicleScalar{0.0};
    const VehicleScalar wornUnloadedRadiusM = std::max(
        static_cast<VehicleScalar>(description.radius) - currentTreadRadiusLossM,
        VehicleScalar{0.05});
    VehicleScalar physicalSupportRadiusM = wornUnloadedRadiusM;
    tires::MotorcycleTireContactGeometry motorcycleSupportContour;
    if (wheel.tireModel.provider == TireProviderKind::MagicFormula62Motorcycle)
    {
        motorcycleSupportContour = tires::evaluateMotorcycleTireProfile(
            wheel.tireModel.motorcycleProfile,
            wornUnloadedRadiusM,
            radians(state.camberAngleDegrees));
        if (motorcycleSupportContour.valid)
        {
            // The rounded motorcycle crown is now contact geometry authority,
            // not telemetry-only. Lean changes both centre-to-road support
            // height and the lateral location of the actual road query.
            physicalSupportRadiusM = std::max(
                motorcycleSupportContour.centerToRoadM, VehicleScalar{0.05});
        }
    }

    // Keep the vehicle solver centred on its own chassis. The offset is
    // authoritative for high-rate local calculations; mountWorld exists
    // only at the collision/rigid-body boundary. This avoids subtracting
    // two large world coordinates just to recover a ~1 metre lever arm.
    const heritage::math::Vec3 mountOffset =
        rotateVector(chassisRotation, flexedLocalMount);
    const heritage::math::Vec3 mountOffsetFromCenterOfMass = rotateVector(
        chassisRotation,
        subtract(flexedLocalMount, chassisCenterOfMassLocal));
    heritage::math::Vec3 wheelRayOffset =
        rotateVector(chassisRotation, flexedWheelRayLocalMount);
    heritage::math::Vec3 wheelRayOffsetFromCenterOfMass = rotateVector(
        chassisRotation,
        subtract(flexedWheelRayLocalMount, chassisCenterOfMassLocal));
    if (motorcycleSupportContour.valid)
    {
        const heritage::math::Vec3 crownOffset = scale(
            previousWorldWheelRight,
            motorcycleSupportContour.lateralContactOffsetM);
        wheelRayOffset = add(wheelRayOffset, crownOffset);
        wheelRayOffsetFromCenterOfMass = add(
            wheelRayOffsetFromCenterOfMass, crownOffset);
    }
    const heritage::math::Vec3 mountWorld = add(
        chassisPose.position,
        mountOffset);
    const heritage::math::Vec3 wheelRayOriginWorld = add(
        chassisPose.position,
        wheelRayOffset);
    const heritage::math::Vec3 suspensionDirection = normalized(
        rotateVector(
            chassisRotation,
            flexedSuspensionDirectionLocal),
        { 0.0f, -1.0f, 0.0f });

    // SUSP11: a kart wheel hub has no conventional suspension travel. Keep the
    // hub rigidly attached to the flexing frame/solid axle and let the pneumatic
    // tire carry vertical compliance. Front kingpin steering jacking has already
    // moved wheelRayOriginWorld above; fixing suspension length here prevents a
    // hidden raycast spring from cancelling that physical jacking mechanism.
    const bool kartRigidSupport = description.suspensionProvider
        == SuspensionProviderKind::KartChassisFlexV1;
    const VehicleScalar minimumLength = kartRigidSupport
        ? static_cast<VehicleScalar>(description.restLength)
        : static_cast<VehicleScalar>(description.restLength
            - description.maximumCompression);
    const VehicleScalar maximumLength = kartRigidSupport
        ? static_cast<VehicleScalar>(description.restLength)
        : static_cast<VehicleScalar>(description.restLength
            + description.maximumDroop);
    const VehicleScalar rayDistance = maximumLength + description.radius;

    heritage::physics::CollisionQueryFilter filter;
    filter.layerMask = 0xffffffffu;
    filter.includeTriggers = false;
    filter.ignoredBody = vehicle.description.chassisBody;
    heritage::physics::RaycastHit hit;
    const bool hitGround = collisions.raycast(
        wheelRayOriginWorld,
        suspensionDirection,
        static_cast<float>(rayDistance),
        filter,
        bodies,
        hit);
    const heritage::physics::RaycastQueryDiagnostics rayDiagnostics =
        collisions.lastRaycastDiagnostics();
    const heritage::physics::SurfaceLocalConditions hitSurfaceConditions =
        hitGround
            ? surfaces.localConditions(
                hit.point,
                hit.surfaceMaterial,
                hit.surfaceWetness,
                hit.surfaceProperties)
            : heritage::physics::SurfaceLocalConditions{};

    state.grounded = false;
    state.contactStatus = WheelContactStatus::NoSupportHit;
    state.rayCandidateCount = rayDiagnostics.colliderCandidateCount
        + rayDiagnostics.staticTriangleCandidateCount;
    state.rayExactTestCount = rayDiagnostics.exactTestCount;
    state.staticTriangleCandidateCount =
        rayDiagnostics.staticTriangleCandidateCount;
    state.staticSceneLoaded = rayDiagnostics.staticSceneLoaded;
    state.originInsideStaticSceneBounds =
        rayDiagnostics.originInsideStaticSceneHorizontalBounds;
    state.rayBoundsOverlapStaticScene =
        rayDiagnostics.rayBoundsOverlapStaticScene;
    state.selectedHitWasStaticTriangle =
        rayDiagnostics.selectedHitWasStaticTriangle;
    state.rawSupportDistance = hitGround
        ? hit.distance - physicalSupportRadiusM
        : 0.0f;
    state.suspensionBottomed = false;
    state.bottomOutPenetration = 0.0f;
    state.normalForce = 0.0f;
    state.longitudinalForce = 0.0f;
    state.lateralForce = 0.0f;
    state.appliedDriveTorque = 0.0f;
    state.appliedBrakeTorque = 0.0f;
    state.serviceBrakeTorque = 0.0f;
    state.handbrakeTorque = 0.0f;
    state.antiLockActive = false;
    state.tractionControlActive = false;
    state.compressionVelocity = 0.0f;
    state.suspensionSpringForce = 0.0f;
    state.suspensionDampingForce = 0.0f;
    state.suspensionBumpStopForce = 0.0f;
    state.suspensionDroopStopForce = 0.0f;
    state.suspensionUnclampedForce = 0.0f;
    state.antiRollBarForce = antiRollBarForceN;
    state.damperDissipationWatts = 0.0f;
    state.leafInterleafFrictionForceN = 0.0f;
    state.leafInterleafDissipationWatts = 0.0f;
    state.leafAxleWrapJackingForceN = 0.0f;
    state.motorcycleChainJackingForceN = 0.0f;
    state.twistBeamCouplingForceN = 0.0f;
    state.twistBeamDissipationWatts = 0.0f;
    state.unsprungVelocity = 0.0f;
    state.tireDeflection = 0.0f;
    state.tireDeflectionVelocity = 0.0f;
    state.tireRadialDissipationWatts = 0.0f;
    state.tireFreeRollingRadius = physicalSupportRadiusM;
    state.tireLoadedRadius = physicalSupportRadiusM;
    state.tireEffectiveRollingRadius = physicalSupportRadiusM;
    state.tireContactPatchLength = 0.0f;
    state.tireContactPatchWidth = 0.0f;
    state.tireContactPatchArea = 0.0f;
    state.tireEnvelopeRoadOffset = 0.0f;
    state.tireEnvelopeSlopeDegrees = 0.0f;
    state.tireEnvelopeValidSamples = 0.0f;
    state.tireRingRadialOffset = 0.0f;
    state.tireRingRadialVelocity = 0.0f;
    state.tireRingLongitudinalOffset = 0.0f;
    state.tireRingLongitudinalVelocity = 0.0f;
    state.tireRingLateralOffset = 0.0f;
    state.tireRingLateralVelocity = 0.0f;
    state.longitudinalSpeed = 0.0f;
    state.lateralSpeed = 0.0f;
    state.slipRatio = 0.0f;
    state.slipAngleDegrees = 0.0f;
    state.effectiveFriction = 0.0f;
    state.gripUtilization = 0.0f;
    state.pureLongitudinalForce = 0.0f;
    state.pureLateralForce = 0.0f;
    state.combinedSlipScale = 1.0f;
    state.pneumaticTrail = 0.0f;
    state.aligningTorque = 0.0f;
    state.overturningMoment = 0.0f;
    state.rollingResistanceMoment = 0.0f;
    state.residualAligningTorque = 0.0f;
    state.longitudinalSlipStiffness = 0.0f;
    state.corneringStiffness = 0.0f;
    state.camberStiffness = 0.0f;
    state.motorcycleContourValid = false;
    state.motorcycleContactLateralOffset = 0.0f;
    state.motorcycleCenterToRoad = 0.0f;
    state.contactCollider = heritage::physics::InvalidCollider;
    state.surfaceMaterial = heritage::physics::SurfaceMaterial::Default;
    state.surfaceWetness = 0.0f;
    state.surfaceTemperatureC = 20.0f;
    state.tireTrackDepositedRubber = 0.0f;
    state.tireTrackLooseRubber = 0.0f;
    state.tireTrackRubberFrictionScale = 1.0f;
    state.tireTrackRubberPassCount = 0.0f;
    state.contactNormal = { 0.0f, 1.0f, 0.0f };
    state.steeringAxisPointValid = false;
    state.steeringGroundGeometryValid = false;
    state.worldSteeringAxisPoint = {};
    state.steeringAxisGroundPointWorld = {};
    state.signedScrubRadiusM = 0.0;
    state.scrubRadiusMagnitudeM = 0.0;
    state.mechanicalTrailM = 0.0;

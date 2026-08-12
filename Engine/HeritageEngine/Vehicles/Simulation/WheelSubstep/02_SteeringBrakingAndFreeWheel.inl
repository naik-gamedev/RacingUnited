// CLEAN03B wheel-substep phase: 02_SteeringBrakingAndFreeWheel
// Resolve per-wheel steering/upright geometry, driver-aid modulation, actuation torques, and free-wheel fallback.
// This file is intentionally included inside VehicleSystem::simulateWheelSubstep().
// It preserves the validated lexical scope and statement order while making phase ownership explicit.

    const float steerFactorMagnitude = std::abs(
        description.steerFactor);
    const float steerFactorSign = signOrZero(description.steerFactor);
    state.steerAngleDegrees = vehicle.currentSteerCenterDegrees
        * description.steerFactor;
    if (steerFactorMagnitude > 0.0001f
        && steering.centerMagnitudeDegrees > 0.001f
        && vehicle.detectedWheelbase > 0.01f
        && vehicle.detectedSteerTrack > 0.01f)
    {
        const float wheelTurnSign = steering.centerSign * steerFactorSign;
        // Native convention is -LEFT / +RIGHT. Therefore the inside wheel
        // is on negative X for a left turn and positive X for a right turn.
        const bool turningRight = wheelTurnSign > 0.0f;
        const bool isInsideWheel = turningRight
            ? description.localMount.x > steering.axleCenterX
            : description.localMount.x < steering.axleCenterX;
        const float ackermannMagnitude = isInsideWheel
            ? steering.innerMagnitudeDegrees
            : steering.outerMagnitudeDegrees;
        state.steerAngleDegrees = wheelTurnSign
            * ackermannMagnitude
            * steerFactorMagnitude;
    }

    VehicleScalar steeringYawRateRadiansPerSecond = 0.0;
    if (wheel.steerRateInitialized && substepDeltaTime > 0.0)
    {
        steeringYawRateRadiansPerSecond = radians(
            (state.steerAngleDegrees - wheel.previousSteerAngleDegrees)
            / substepDeltaTime);
    }
    else
    {
        wheel.steerRateInitialized = true;
    }
    wheel.previousSteerAngleDegrees = state.steerAngleDegrees;

    SuspensionGeometryOutput currentGeometryOutput;
    const auto updateUprightGeometry = [&]() {
        currentGeometryOutput = evaluateSuspensionGeometry(
            suspensionGeometryDescription(description),
            { static_cast<float>(state.compression),
              static_cast<float>(state.steerAngleDegrees),
              description.localSuspensionDirection });
        const SuspensionGeometryOutput& geometryOutput =
            currentGeometryOutput;
        state.suspensionKinematicsValid = geometryOutput.kinematicsValid;
        state.suspensionTravelClamped = geometryOutput.travelClamped;
        state.bumpSteerDegrees = geometryOutput.bumpSteerDegrees;
        state.strutCompression = geometryOutput.strutCompressionM;
        state.instantaneousMotionRatio = geometryOutput.springMotionRatio;
        state.camberAngleDegrees = geometryOutput.camberDegrees;
        state.toeAngleDegrees = geometryOutput.toeDegrees;
        state.localUprightRotationDegrees =
            geometryOutput.localUprightRotationDegrees;
        const heritage::math::Vec3 flexedSteeringAxis =
            applyChassisSectionTwistToVector(
                geometryOutput.localSteeringAxis,
                chassisSectionTwistRadiansValue);
        const heritage::math::Vec3 flexedWheelForward =
            applyChassisSectionTwistToVector(
                geometryOutput.localWheelForward,
                chassisSectionTwistRadiansValue);
        const heritage::math::Vec3 flexedWheelRight =
            applyChassisSectionTwistToVector(
                geometryOutput.localWheelRight,
                chassisSectionTwistRadiansValue);
        const heritage::math::Vec3 flexedWheelUp =
            applyChassisSectionTwistToVector(
                geometryOutput.localWheelUp,
                chassisSectionTwistRadiansValue);
        state.worldSteeringAxis = normalized(
            rotateVector(chassisRotation, flexedSteeringAxis),
            { 0.0f, 1.0f, 0.0f });
        state.steeringAxisPointValid = geometryOutput.steeringAxisPointValid;
        if (state.steeringAxisPointValid)
        {
            const heritage::math::Vec3 flexedSteeringAxisPoint =
                applyChassisSectionTwistToPoint(
                    geometryOutput.localSteeringAxisPoint,
                    vehicle.chassisFlex.torsionAxisLocalY,
                    chassisSectionTwistRadiansValue);
            state.worldSteeringAxisPoint = add(
                chassisPose.position,
                rotateVector(chassisRotation, flexedSteeringAxisPoint));
        }
        state.worldWheelForward = normalized(
            rotateVector(chassisRotation, flexedWheelForward),
            { 0.0f, 0.0f, 1.0f });
        state.worldWheelRight = normalized(
            rotateVector(chassisRotation, flexedWheelRight),
            { 1.0f, 0.0f, 0.0f });
        state.worldWheelUp = normalized(
            rotateVector(chassisRotation, flexedWheelUp),
            { 0.0f, 1.0f, 0.0f });
        // localUprightRotationDegrees is intentionally an FP32 local/render-facing
        // geometry vector. Keep the high-rate flex state in VehicleScalar/FP64 and
        // narrow explicitly only at this established Vec3 boundary.
        state.localUprightRotationDegrees.z += static_cast<float>(
            degrees(chassisSectionTwistRadiansValue));
    };
    const auto currentSuspensionModelDescription = [&]() {
        SuspensionModelDescription model =
            suspensionModelDescription(description);
        if (currentGeometryOutput.kinematicsValid)
        {
            if (description.suspensionProvider
                    == SuspensionProviderKind::MacPhersonStrutV1)
            {
                model.motionRatio = std::clamp(
                    static_cast<VehicleScalar>(
                        currentGeometryOutput.springMotionRatio),
                    0.05,
                    5.0);
            }
            else if (description.suspensionProvider
                    == SuspensionProviderKind::TrailingArmTorsionBarV1)
            {
                model.motionRatio = std::clamp(
                    static_cast<VehicleScalar>(
                        currentGeometryOutput.damperMotionRatio),
                    -5.0,
                    5.0);
            }
        }
        return model;
    };
    const float gearDirection = signOrZero(vehicle.selectedGearRatio);
    const VehicleScalar drivenSlip =
        previousSlipRatio * static_cast<VehicleScalar>(gearDirection);
    VehicleScalar tractionTargetModulation = 1.0;
    if (vehicle.driverAids.tractionControlEnabled
        && description.driveFactor > 0.0f
        && vehicle.throttle > 0.01f
        && std::abs(previousLongitudinalSpeed)
            >= vehicle.driverAids.minimumActivationSpeed
        && drivenSlip > vehicle.driverAids.tractionControlTargetSlip)
    {
        tractionTargetModulation = std::clamp(
            static_cast<VehicleScalar>(
                vehicle.driverAids.tractionControlTargetSlip)
                / std::max(static_cast<VehicleScalar>(drivenSlip), 0.001),
            0.05,
            1.0);
        state.tractionControlActive = true;
        ++vehicle.tractionControlActiveWheelCount;
    }
    state.tractionControlModulation = moveTowardsScalar(
        state.tractionControlModulation,
        tractionTargetModulation,
        static_cast<VehicleScalar>(vehicle.driverAids.modulationRate)
            * static_cast<VehicleScalar>(substepDeltaTime));

    const VehicleScalar driveTorque = vehicle.outputTorque
        * vehicle.driveSharesScratch[wheelIndex]
        * state.tractionControlModulation;

    VehicleScalar antiLockTargetModulation = 1.0;
    const VehicleScalar brakingDirection = previousLongitudinalSpeed > 0.0001
        ? 1.0
        : (previousLongitudinalSpeed < -0.0001 ? -1.0 : 0.0);
    const VehicleScalar brakingSlip =
        -previousSlipRatio * brakingDirection;
    if (vehicle.driverAids.antiLockBrakesEnabled
        && vehicle.brake > 0.01f
        && description.brakeFactor > 0.0f
        && std::abs(previousLongitudinalSpeed)
            >= vehicle.driverAids.minimumActivationSpeed
        && brakingSlip > vehicle.driverAids.antiLockTargetSlip)
    {
        antiLockTargetModulation = std::clamp(
            static_cast<VehicleScalar>(
                vehicle.driverAids.antiLockTargetSlip)
                / std::max(brakingSlip, 0.001),
            0.05,
            1.0);
        state.antiLockActive = true;
        ++vehicle.antiLockActiveWheelCount;
    }
    state.antiLockModulation = moveTowardsScalar(
        state.antiLockModulation,
        antiLockTargetModulation,
        static_cast<VehicleScalar>(vehicle.driverAids.modulationRate)
            * static_cast<VehicleScalar>(substepDeltaTime));

    const VehicleScalar serviceBrakeTorque =
        vehicle.description.maximumBrakeForce
        * vehicle.brake
        * (description.brakeFactor / driveline.totalBrakeFactor)
        * description.radius
        * state.antiLockModulation;
    const VehicleScalar handbrakeTorque =
        vehicle.driverAids.maximumHandbrakeTorque
        * vehicle.handbrake
        * (description.handbrakeFactor / driveline.totalHandbrakeFactor);
    const VehicleScalar brakeTorqueMagnitude =
        serviceBrakeTorque + handbrakeTorque;
    state.appliedDriveTorque = driveTorque;
    state.appliedBrakeTorque = brakeTorqueMagnitude;
    state.serviceBrakeTorque = serviceBrakeTorque;
    state.handbrakeTorque = handbrakeTorque;

    const auto advanceFreeWheelRotation = [&]() {
        const VehicleScalar projectedAngularVelocity = state.wheelAngularVelocity
            + (driveTorque / wheel.tireModel.wheelInertia)
                * substepDeltaTime;
        const VehicleScalar brakeTorque = brakeTorqueMagnitude > 0.0
            ? std::clamp(
                -projectedAngularVelocity * wheel.tireModel.wheelInertia
                    / substepDeltaTime,
                -brakeTorqueMagnitude,
                brakeTorqueMagnitude)
            : 0.0f;
        const VehicleScalar angularAcceleration = (driveTorque + brakeTorque)
            / wheel.tireModel.wheelInertia;
        state.wheelAngularVelocity += angularAcceleration * substepDeltaTime;
        state.wheelAngularVelocity *= std::exp(-0.35f * substepDeltaTime);
        state.relaxedSlipRatio *= std::exp(-4.0f * substepDeltaTime);
        state.relaxedSlipAngleDegrees *= std::exp(-4.0f * substepDeltaTime);
        wheel.contactPatchState.torsionalTwistRadians = 0.0;
        state.turnSlipPerM = 0.0;
        state.normalizedTurnSlip = 0.0;
        state.contactPatchTwistDegrees = 0.0;
        state.parkingTurnMoment = 0.0;
        state.turnSlipMoment = 0.0;
        state.turnSlipLongitudinalReduction = 1.0;
        state.turnSlipLateralReduction = 1.0;
        state.turnSlipCorneringReduction = 1.0;
        state.turnSlipTrailReduction = 1.0;
        state.wheelRotationDegrees += degrees(
            state.wheelAngularVelocity * substepDeltaTime);
    };


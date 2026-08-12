// CLEAN03B wheel-substep phase: 10_ApplyForcesAndIntegrateWheel
// Apply tire impulse to the chassis and integrate wheel torque, brake constraint, angular velocity, and visual rotation.
// This file is intentionally included inside VehicleSystem::simulateWheelSubstep().
// It preserves the validated lexical scope and statement order while making phase ownership explicit.

    const heritage::math::Vec3 tireForce = add(
        scale(wheelForward, longitudinalForce),
        scale(wheelRight, lateralForce));
    bodies.applyImpulseAtPoint(
        vehicle.description.chassisBody,
        scale(tireForce, substepDeltaTime),
        state.contactPoint);

    const VehicleScalar tireReactionTorque = -longitudinalForce
        * effectiveRollingRadius;
    const VehicleScalar torqueWithoutBrake = driveTorque
        + differentialLockTorque
        + tireReactionTorque;
    const VehicleScalar projectedAngularVelocity = state.wheelAngularVelocity
        + (torqueWithoutBrake / wheel.tireModel.wheelInertia)
            * substepDeltaTime;
    // A brake is a bounded constraint, not a torque that is allowed to
    // overshoot zero and reverse the wheel every millisecond. Clamp the
    // requested reaction to exactly the torque needed to stop this step.
    const VehicleScalar brakeTorque = brakeTorqueMagnitude > 0.0
        ? std::clamp(
            -projectedAngularVelocity * wheel.tireModel.wheelInertia
                / substepDeltaTime,
            -brakeTorqueMagnitude,
            brakeTorqueMagnitude)
        : 0.0f;
    const VehicleScalar netWheelTorque = torqueWithoutBrake + brakeTorque;
    state.wheelAngularVelocity += (netWheelTorque
        / wheel.tireModel.wheelInertia) * substepDeltaTime;
    state.wheelAngularVelocity = std::clamp(
        state.wheelAngularVelocity,
        -4000.0,
        4000.0);
    if (brakeTorqueMagnitude > 0.0f
        && std::abs(state.wheelAngularVelocity) < 0.03f
        && std::abs(longitudinalSpeed) < 0.10f)
    {
        state.wheelAngularVelocity = 0.0f;
    }
    state.wheelRotationDegrees += degrees(
        state.wheelAngularVelocity * substepDeltaTime);
    if (std::abs(state.wheelRotationDegrees) > 36000.0f)
        state.wheelRotationDegrees = std::fmod(
            state.wheelRotationDegrees,
            360.0f);

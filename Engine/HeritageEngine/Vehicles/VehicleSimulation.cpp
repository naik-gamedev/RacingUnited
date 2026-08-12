#include "VehicleSystem.hpp"
#include "VehicleSystemInternal.hpp"
#include "Tires/TireSlipDynamics.hpp"
#include "Tires/TireContactPatch.hpp"
#include "../Physics/Surfaces/SurfaceWorld.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::vehicles {
using namespace vehicle_system_detail;

void VehicleSystem::simulate(
    heritage::physics::RigidBodySystem& bodies,
    const heritage::physics::CollisionSystem& collisions,
    heritage::physics::SurfaceWorld& surfaces,
    float worldDeltaTime,
    const heritage::math::Vec3& gravity)
{
    if (!finiteFloat(worldDeltaTime) || worldDeltaTime <= 0.0f
        || !finiteVec3(gravity))
        return;

    removeInvalidBodies(bodies);
    for (Slot& slot : m_slots)
    {
        if (!slot.alive)
            continue;

        Record& vehicle = slot.record;
        vehicle.lastHighRateStepCount = 0;

        bool chassisSleeping = false;
        bodies.sleeping(vehicle.description.chassisBody, chassisSleeping);
        if (vehicle.parkedResting && !chassisSleeping)
        {
            // A reset, impulse, collision, or authored pose change woke the
            // chassis. The parked lock is no longer authoritative.
            vehicle.parkedResting = false;
            vehicle.parkedRestRequiresBrake = false;
            vehicle.parkedRestBrakeInput = 0.0f;
            vehicle.parkedRestHandbrakeInput = 0.0f;
            vehicle.restTimer = 0.0f;
        }
        if (chassisSleeping)
        {
            const bool requiredBrakeReleased =
                vehicle.brake + 0.001f < vehicle.parkedRestBrakeInput
                || vehicle.handbrake + 0.001f
                    < vehicle.parkedRestHandbrakeInput;
            const float requestedSteerCenterDegrees =
                vehicle.description.maximumSteerAngleDegrees * vehicle.steering;
            const bool steeringMotionRequested = std::abs(
                requestedSteerCenterDegrees - vehicle.currentSteerCenterDegrees)
                > 0.01f;
            // Steering remains an active high-rate tire/suspension operation even
            // while the chassis itself is otherwise parkable. In particular,
            // TIRE03 parking torsion and the visual upright must continue to move
            // while the service brake is held. A sleeping chassis therefore wakes
            // whenever the commanded road-wheel angle differs from the current
            // steering state.
            const bool shouldWake = vehicle.throttle > 0.001f
                || steeringMotionRequested
                || (vehicle.parkedResting
                    && vehicle.parkedRestRequiresBrake
                    && requiredBrakeReleased);
            if (!shouldWake)
            {
                vehicle.highRateAccumulator = 0.0;
                vehicle.speed = 0.0f;
                captureDynamicsLabFrame(vehicle, bodies, worldDeltaTime);
                continue;
            }

            bodies.wake(vehicle.description.chassisBody);
            vehicle.parkedResting = false;
            vehicle.parkedRestRequiresBrake = false;
            vehicle.parkedRestBrakeInput = 0.0f;
            vehicle.parkedRestHandbrakeInput = 0.0f;
            vehicle.restTimer = 0.0f;
        }

        const double highRateDelta = 1.0
            / static_cast<double>(vehicle.description.highRateHertz);
        vehicle.highRateAccumulator += static_cast<double>(worldDeltaTime);

        while (vehicle.highRateAccumulator + 1.0e-12 >= highRateDelta
            && vehicle.lastHighRateStepCount < kMaximumHighRateStepsPerWorldStep)
        {
            vehicle.highRateAccumulator -= highRateDelta;
            if (vehicle.highRateAccumulator < 0.0)
                vehicle.highRateAccumulator = 0.0;
            simulateVehicleSubstep(
                vehicle,
                bodies,
                collisions,
                surfaces,
                static_cast<float>(highRateDelta));
            ++vehicle.lastHighRateStepCount;
            ++vehicle.totalHighRateStepCount;
        }

        // A malformed rate or a debugger stall must not create an unbounded
        // high-rate backlog inside one vehicle.
        const double maximumBacklog = highRateDelta * 2.0;
        if (vehicle.highRateAccumulator > maximumBacklog)
            vehicle.highRateAccumulator = std::fmod(
                vehicle.highRateAccumulator,
                highRateDelta);

        heritage::math::Vec3 linearVelocity{};
        heritage::math::Vec3 angularVelocityDegrees{};
        if (bodies.linearVelocity(
                vehicle.description.chassisBody,
                linearVelocity))
        {
            vehicle.speed = length(linearVelocity);
        }
        bodies.angularVelocityDegrees(
            vehicle.description.chassisBody,
            angularVelocityDegrees);

        // A tire is a static-friction contact while it is parked, not merely a
        // velocity-dependent force curve. High-rate suspension/tire impulses
        // intentionally wake the rigid body, so the generic collision-island
        // sleep timer cannot settle a raycast-supported vehicle by itself.
        // Detect a physically supportable rest state here, then let the rigid
        // body sleep until propulsion, brake release on a slope, an impact, or
        // an authored transform wakes it.
        const bool allWheelsGrounded = !vehicle.wheels.empty()
            && vehicle.groundedWheelCount == vehicle.wheels.size();
        heritage::math::Vec3 weightedNormal{};
        VehicleScalar normalLoadTotal = 0.0;
        VehicleScalar availableBrakeHoldForce = 0.0;
        VehicleScalar maximumWheelSpeed = 0.0;
        for (const WheelRecord& wheel : vehicle.wheels)
        {
            maximumWheelSpeed = std::max(
                maximumWheelSpeed,
                std::abs(wheel.state.wheelAngularVelocity));
            if (!wheel.state.grounded || wheel.state.normalForce <= 0.0f)
                continue;

            weightedNormal = add(
                weightedNormal,
                scale(wheel.state.contactNormal, wheel.state.normalForce));
            normalLoadTotal += wheel.state.normalForce;
            if (wheel.state.appliedBrakeTorque > 0.0f)
            {
                const VehicleScalar tireHoldLimit = wheel.state.effectiveFriction
                    * wheel.state.normalForce;
                const VehicleScalar brakeHoldLimit = wheel.state.appliedBrakeTorque
                    / std::max(static_cast<VehicleScalar>(wheel.description.radius), 0.01);
                availableBrakeHoldForce += std::min(
                    tireHoldLimit,
                    brakeHoldLimit);
            }
        }

        float chassisMass = 0.0f;
        float gravityFactor = 0.0f;
        bodies.mass(vehicle.description.chassisBody, chassisMass);
        bodies.gravityFactor(vehicle.description.chassisBody, gravityFactor);

        // TIRE15C5: one analytical wake evaluation per world step and vehicle.
        // The 1000 Hz wheel loop remains tire/contact-owned; aerodynamic rubber
        // migration is a world-scale field operation and does not belong there.
        if (vehicle.speed > 9.5f || length(linearVelocity) > 9.5f)
        {
            heritage::physics::RigidBodyPose wakePose;
            if (bodies.pose(vehicle.description.chassisBody, wakePose))
            {
                const Quaternion wakeRotation = quaternionFromEulerDegrees(
                    wakePose.rotationDegrees);
                heritage::math::Vec3 wakeForward = normalized(
                    rotateVector(wakeRotation, { 0.0f, 0.0f, 1.0f }),
                    { 0.0f, 0.0f, 1.0f });
                const heritage::math::Vec3 wakeUp = normalized(
                    rotateVector(wakeRotation, { 0.0f, 1.0f, 0.0f }),
                    { 0.0f, 1.0f, 0.0f });
                if (dot(linearVelocity, wakeForward) < 0.0f)
                    wakeForward = scale(wakeForward, -1.0f);

                float minWheelX = 0.0f;
                float maxWheelX = 0.0f;
                float minWheelZ = 0.0f;
                float maxWheelZ = 0.0f;
                float maximumTireWidth = 0.20f;
                bool firstWheel = true;
                for (const WheelRecord& wheel : vehicle.wheels)
                {
                    if (firstWheel)
                    {
                        minWheelX = maxWheelX = wheel.description.localMount.x;
                        minWheelZ = maxWheelZ = wheel.description.localMount.z;
                        firstWheel = false;
                    }
                    else
                    {
                        minWheelX = std::min(minWheelX, wheel.description.localMount.x);
                        maxWheelX = std::max(maxWheelX, wheel.description.localMount.x);
                        minWheelZ = std::min(minWheelZ, wheel.description.localMount.z);
                        maxWheelZ = std::max(maxWheelZ, wheel.description.localMount.z);
                    }
                    const float authoredWidth = static_cast<float>(
                        wheel.tireModel.contactGeometry.nominalWidthM);
                    if (std::isfinite(authoredWidth) && authoredWidth > 0.05f)
                        maximumTireWidth = std::max(maximumTireWidth, authoredWidth);
                }

                const float wheelTrack = firstWheel ? 1.4f : std::max(maxWheelX - minWheelX, 0.0f);
                const float wheelbase = firstWheel ? 2.4f : std::max(maxWheelZ - minWheelZ, 0.0f);
                const float vehicleWidth = std::clamp(
                    wheelTrack + maximumTireWidth + 0.22f, 0.45f, 3.2f);
                const float vehicleLength = std::clamp(
                    wheelbase + std::max(1.05f, wheelbase * 0.46f), 1.4f, 8.0f);
                const float longitudinalCenter = firstWheel
                    ? 0.0f : (minWheelZ + maxWheelZ) * 0.5f;
                const heritage::math::Vec3 wakeCenterOffset = rotateVector(
                    wakeRotation, { 0.0f, 0.0f, longitudinalCenter });
                const heritage::math::Vec3 wakeCenter = add(
                    wakePose.position, wakeCenterOffset);

                heritage::physics::rubber::TrackRubberWakeInput wake;
                wake.forward = wakeForward;
                wake.up = wakeUp;
                wake.deltaTimeSeconds = worldDeltaTime;
                wake.speedMps = length(linearVelocity);
                wake.vehicleWidthM = vehicleWidth;
                wake.vehicleLengthM = vehicleLength;
                // Until active aerodynamic authoring is promoted, the wake model
                // uses a conservative production-car baseline. This is an explicit
                // seam, not a hidden claim that every vehicle has identical aero.
                wake.rideHeightM = 0.16f;
                wake.normalLoadN = static_cast<float>(normalLoadTotal);
                wake.referenceWeightN = std::max(
                    chassisMass * std::abs(gravityFactor) * 9.80665f, 1.0f);
                wake.aeroWakeFactor = 1.0f;
                surfaces.applyTrackRubberWake(wakeCenter, wake);
            }
        }
        const heritage::math::Vec3 supportNormal = normalized(
            weightedNormal,
            { 0.0f, 1.0f, 0.0f });
        const heritage::math::Vec3 bodyGravity = scale(gravity, gravityFactor);
        const heritage::math::Vec3 predictedLinearVelocity = add(
            linearVelocity,
            scale(bodyGravity, worldDeltaTime));
        const float predictedSpeed = length(predictedLinearVelocity);
        vehicle.speed = predictedSpeed;
        const heritage::math::Vec3 tangentialGravity = subtract(
            bodyGravity,
            scale(supportNormal, dot(bodyGravity, supportNormal)));
        const float gravityMagnitude = length(bodyGravity);
        const float tangentialGravityMagnitude = length(tangentialGravity);
        const float flatSlopeLimit = gravityMagnitude * std::sin(
            radians(kVehicleRestFlatSlopeDegrees));
        const bool effectivelyFlat = tangentialGravityMagnitude
            <= flatSlopeLimit + 0.001f;
        const float requiredHoldForce = chassisMass
            * tangentialGravityMagnitude;
        const bool brakesCanHold = availableBrakeHoldForce
            >= requiredHoldForce * 1.05f;
        const bool quietEnough = predictedSpeed <= kVehicleRestLinearSpeed
            && length(angularVelocityDegrees)
                <= kVehicleRestAngularSpeedDegrees
            && maximumWheelSpeed <= kVehicleRestWheelSpeed;
        const float requestedSteerCenterDegrees =
            vehicle.description.maximumSteerAngleDegrees * vehicle.steering;
        const bool steeringSettled = std::abs(
            requestedSteerCenterDegrees - vehicle.currentSteerCenterDegrees)
            <= 0.01f;
        const bool canRest = allWheelsGrounded
            && normalLoadTotal > 0.0f
            && vehicle.throttle <= 0.001f
            && steeringSettled
            && quietEnough
            && (effectivelyFlat || brakesCanHold);
        vehicle.restCandidate = canRest;
        vehicle.requiredHoldForce = requiredHoldForce;
        vehicle.availableBrakeHoldForce = static_cast<float>(
            availableBrakeHoldForce);

        if (canRest)
            vehicle.restTimer += worldDeltaTime;
        else
            vehicle.restTimer = 0.0f;

        if (vehicle.restTimer >= kVehicleRestDelaySeconds
            && bodies.setSleeping(vehicle.description.chassisBody, true))
        {
            vehicle.speed = 0.0f;
            vehicle.parkedResting = true;
            vehicle.parkedRestRequiresBrake = !effectivelyFlat;
            vehicle.parkedRestBrakeInput = vehicle.brake;
            vehicle.parkedRestHandbrakeInput = vehicle.handbrake;
        }
    }
}

VehicleSystem::SteeringSubstepState VehicleSystem::updateSteeringSubstep(
    Record& vehicle,
    float chassisSpeed,
    float substepDeltaTime)
{
    vehicle.targetSteerCenterDegrees =
        vehicle.description.maximumSteerAngleDegrees * vehicle.steering;

    const float speedBlend = std::clamp(
        chassisSpeed / vehicle.description.highSpeedReferenceMps,
        0.0f,
        1.0f);
    vehicle.currentSteeringRateFactor =
        1.0f + (vehicle.description.highSpeedSteeringRateFactor - 1.0f)
        * speedBlend;
    const bool returningToCenter = std::abs(vehicle.steering) < 0.0001f;
    const float steeringRate = returningToCenter
        ? vehicle.description.steeringReturnRateDegreesPerSecond
        : vehicle.description.steeringRateDegreesPerSecond
            * vehicle.currentSteeringRateFactor;
    vehicle.currentSteerCenterDegrees = moveTowards(
        vehicle.currentSteerCenterDegrees,
        vehicle.targetSteerCenterDegrees,
        steeringRate * substepDeltaTime);

    float steeredWeight = 0.0f;
    float steeredZ = 0.0f;
    float steeredMinX = (std::numeric_limits<float>::max)();
    float steeredMaxX = (std::numeric_limits<float>::lowest)();
    float referenceWeight = 0.0f;
    float referenceZ = 0.0f;
    for (const WheelRecord& wheel : vehicle.wheels)
    {
        // Ackermann belongs to the upright/steering geometry. A spacer or ET
        // change moves the tire centerline but does not move the steering pivots.
        const heritage::math::Vec3& referenceMount =
            wheel.description.localMount;
        const float steerWeight = std::abs(wheel.description.steerFactor);
        if (steerWeight > 0.0001f)
        {
            steeredWeight += steerWeight;
            steeredZ += referenceMount.z * steerWeight;
            steeredMinX = std::min(
                steeredMinX,
                referenceMount.x);
            steeredMaxX = std::max(
                steeredMaxX,
                referenceMount.x);
        }
        else
        {
            referenceWeight += 1.0f;
            referenceZ += referenceMount.z;
        }
    }

    SteeringSubstepState result;
    vehicle.detectedWheelbase = 0.0f;
    vehicle.detectedSteerTrack = 0.0f;
    if (steeredWeight > 0.0001f)
    {
        result.axleCenterX = 0.5f * (steeredMinX + steeredMaxX);
        vehicle.detectedSteerTrack = std::max(
            0.0f,
            steeredMaxX - steeredMinX);
        if (referenceWeight > 0.0001f)
        {
            const float steeringAxleZ = steeredZ / steeredWeight;
            const float referenceAxleZ = referenceZ / referenceWeight;
            vehicle.detectedWheelbase = std::abs(
                steeringAxleZ - referenceAxleZ);
        }
    }

    result.centerMagnitudeDegrees = std::abs(
        vehicle.currentSteerCenterDegrees);
    const AckermannSolution ackermann = solveAckermann(
        vehicle.currentSteerCenterDegrees,
        vehicle.detectedWheelbase,
        vehicle.detectedSteerTrack,
        vehicle.description.ackermannPercent);
    result.innerMagnitudeDegrees = ackermann.innerMagnitudeDegrees;
    result.outerMagnitudeDegrees = ackermann.outerMagnitudeDegrees;
    result.centerSign = signOrZero(vehicle.currentSteerCenterDegrees);

    vehicle.innerSteerAngleDegrees =
        result.centerSign * result.innerMagnitudeDegrees;
    vehicle.outerSteerAngleDegrees =
        result.centerSign * result.outerMagnitudeDegrees;
    return result;
}

VehicleSystem::DrivelineSubstepState VehicleSystem::updateDrivelineSubstep(
    Record& vehicle,
    float substepDeltaTime)
{
    if (vehicle.shifting)
    {
        vehicle.shiftTimeRemaining = std::max(
            0.0f,
            vehicle.shiftTimeRemaining - substepDeltaTime);
        if (vehicle.shiftTimeRemaining <= 0.0f)
        {
            vehicle.currentGear = vehicle.requestedGear;
            vehicle.shifting = false;
        }
    }

    vehicle.selectedGearRatio = vehicle.shifting
        ? 0.0f
        : selectedGearRatio(vehicle.powertrain, vehicle.currentGear);

    float drivenWeight = 0.0f;
    VehicleScalar drivenOmega = 0.0;
    float drivenRadius = 0.0f;
    VehicleScalar minimumDrivenOmega =
        (std::numeric_limits<VehicleScalar>::max)();
    VehicleScalar maximumDrivenOmega =
        (std::numeric_limits<VehicleScalar>::lowest)();
    for (const WheelRecord& wheel : vehicle.wheels)
    {
        if (wheel.description.driveFactor <= 0.0f)
            continue;
        const float weight = wheel.description.driveFactor;
        drivenWeight += weight;
        drivenOmega += wheel.state.wheelAngularVelocity * weight;
        drivenRadius += wheel.description.radius * weight;
        const VehicleScalar absoluteOmega = std::abs(
            wheel.state.wheelAngularVelocity);
        minimumDrivenOmega = std::min(minimumDrivenOmega, absoluteOmega);
        maximumDrivenOmega = std::max(maximumDrivenOmega, absoluteOmega);
    }
    if (drivenWeight <= 0.0f)
        drivenWeight = 1.0f;
    drivenOmega /= drivenWeight;
    drivenRadius /= drivenWeight;
    if (drivenRadius <= 0.01f)
        drivenRadius = 0.35f;

    vehicle.drivenWheelSpeedDifferenceRpm = static_cast<float>(
        maximumDrivenOmega >= minimumDrivenOmega
        ? (maximumDrivenOmega - minimumDrivenOmega)
            * (60.0 / (2.0 * static_cast<VehicleScalar>(kPi)))
        : 0.0);
    vehicle.wheelCoupledRpm = static_cast<float>(std::abs(
        drivenOmega
        * static_cast<VehicleScalar>(vehicle.selectedGearRatio)
        * static_cast<VehicleScalar>(vehicle.powertrain.finalDriveRatio))
        * (60.0 / (2.0 * static_cast<VehicleScalar>(kPi))));

    const bool drivelineConnected = !vehicle.shifting
        && std::abs(vehicle.selectedGearRatio) > 0.0001f;
    float clutchTarget = 0.0f;
    if (drivelineConnected)
    {
        const float launchBlend = std::clamp(
            vehicle.wheelCoupledRpm
                / std::max(vehicle.powertrain.idleRpm * 1.35f, 1.0f),
            0.0f,
            1.0f);
        clutchTarget = vehicle.throttle > 0.02f
            ? 0.25f + 0.75f * launchBlend
            : 1.0f;
    }
    vehicle.clutchEngagement = moveTowards(
        vehicle.clutchEngagement,
        clutchTarget,
        vehicle.powertrain.clutchEngagementRate * substepDeltaTime);

    const float freeRevTarget = vehicle.powertrain.idleRpm
        + vehicle.throttle
            * (vehicle.powertrain.redlineRpm - vehicle.powertrain.idleRpm)
            * 0.96f;
    const float coupledTarget = std::max(
        vehicle.powertrain.idleRpm,
        vehicle.wheelCoupledRpm);
    const float engineTarget = drivelineConnected
        ? freeRevTarget
            + (coupledTarget - freeRevTarget) * vehicle.clutchEngagement
        : freeRevTarget;
    const float engineResponse = 1.0f - std::exp(
        -vehicle.powertrain.engineResponse * substepDeltaTime);
    vehicle.engineRpm += (engineTarget - vehicle.engineRpm) * engineResponse;
    vehicle.engineRpm = std::clamp(
        vehicle.engineRpm,
        vehicle.powertrain.idleRpm,
        vehicle.powertrain.redlineRpm + 750.0f);
    vehicle.clutchSlipRpm = drivelineConnected
        ? vehicle.engineRpm - vehicle.wheelCoupledRpm
        : vehicle.engineRpm;

    const float torqueFactor = engineTorqueCurveFactor(
        vehicle.engineRpm,
        vehicle.powertrain.idleRpm,
        vehicle.powertrain.redlineRpm);
    const bool revLimiter = vehicle.engineRpm
        >= vehicle.powertrain.redlineRpm;
    const float combustionTorque = revLimiter
        ? 0.0f
        : vehicle.powertrain.maximumTorque
            * torqueFactor
            * vehicle.throttle;
    const float engineBrakeBlend = std::clamp(
        (vehicle.engineRpm - vehicle.powertrain.idleRpm)
            / std::max(
                vehicle.powertrain.redlineRpm
                    - vehicle.powertrain.idleRpm,
                1.0f),
        0.0f,
        1.0f);
    const float engineBrakeTorque = vehicle.powertrain.engineBrakingTorque
        * (1.0f - vehicle.throttle)
        * engineBrakeBlend;
    vehicle.engineTorque = combustionTorque - engineBrakeTorque;
    vehicle.outputTorque = drivelineConnected
        ? vehicle.engineTorque
            * vehicle.selectedGearRatio
            * vehicle.powertrain.finalDriveRatio
            * vehicle.powertrain.drivetrainEfficiency
            * vehicle.clutchEngagement
        : 0.0f;
    const float maximumOutputTorque = vehicle.description.maximumDriveForce
        * drivenRadius;
    vehicle.outputTorque = std::clamp(
        vehicle.outputTorque,
        -maximumOutputTorque,
        maximumOutputTorque);

    vehicle.driveSharesScratch.assign(vehicle.wheels.size(), 0.0);
    VehicleScalar driveShareTotal = 0.0;
    const VehicleScalar averageDrivenAbsoluteOmega = std::abs(drivenOmega);
    for (std::size_t wheelIndex = 0;
        wheelIndex < vehicle.wheels.size();
        ++wheelIndex)
    {
        const WheelRecord& wheel = vehicle.wheels[wheelIndex];
        if (wheel.description.driveFactor <= 0.0f)
            continue;

        VehicleScalar share = wheel.description.driveFactor;
        if (vehicle.powertrain.differentialMode
            == DifferentialMode::LimitedSlip)
        {
            const VehicleScalar wheelSpeed = std::abs(
                wheel.state.wheelAngularVelocity);
            const VehicleScalar speedError =
                (averageDrivenAbsoluteOmega - wheelSpeed)
                / std::max(averageDrivenAbsoluteOmega + 1.0, 1.0);
            const VehicleScalar bias =
                vehicle.powertrain.differentialBiasRatio;
            const VehicleScalar multiplier = std::clamp(
                1.0 + speedError * (bias - 1.0),
                1.0 / bias,
                bias);
            share *= multiplier;
        }
        vehicle.driveSharesScratch[wheelIndex] = share;
        driveShareTotal += share;
    }
    if (driveShareTotal > 0.0f)
    {
        for (VehicleScalar& share : vehicle.driveSharesScratch)
            share /= driveShareTotal;
    }

    float totalBrakeFactor = 0.0f;
    float totalHandbrakeFactor = 0.0f;
    for (const WheelRecord& wheel : vehicle.wheels)
    {
        totalBrakeFactor += wheel.description.brakeFactor;
        totalHandbrakeFactor += wheel.description.handbrakeFactor;
    }
    if (totalBrakeFactor <= 0.0f)
        totalBrakeFactor = 1.0f;
    if (totalHandbrakeFactor <= 0.0f)
        totalHandbrakeFactor = 1.0f;

    DrivelineSubstepState result;
    result.drivenOmega = drivenOmega;
    result.totalBrakeFactor = totalBrakeFactor;
    result.totalHandbrakeFactor = totalHandbrakeFactor;
    return result;
}


void VehicleSystem::prepareAntiRollBarForces(Record& vehicle)
{
    vehicle.antiRollForcesScratch.assign(vehicle.wheels.size(), 0.0);
    for (AntiRollBarRecord& bar : vehicle.antiRollBars)
    {
        const auto& description = bar.description;
        if (!description.enabled
            || description.leftWheelIndex >= vehicle.wheels.size()
            || description.rightWheelIndex >= vehicle.wheels.size())
        {
            bar.state = {};
            continue;
        }

        const WheelState& left =
            vehicle.wheels[description.leftWheelIndex].state;
        const WheelState& right =
            vehicle.wheels[description.rightWheelIndex].state;
        bar.state = evaluateSuspensionAntiRollBar(
            description,
            { left.compression,
              right.compression,
              left.compressionVelocity,
              right.compressionVelocity });
        vehicle.antiRollForcesScratch[description.leftWheelIndex] +=
            bar.state.leftWheelForceN;
        vehicle.antiRollForcesScratch[description.rightWheelIndex] +=
            bar.state.rightWheelForceN;
    }
}

void VehicleSystem::updateChassisFlexSubstep(
    Record& vehicle,
    float substepDeltaTime)
{
    if (!vehicle.chassisFlex.enabled
        || !validChassisTorsionalComplianceDescription(vehicle.chassisFlex))
    {
        vehicle.chassisFlexState = {};
        return;
    }

    const VehicleScalar span = vehicle.chassisFlex.frontReferenceLocalZ
        - vehicle.chassisFlex.rearReferenceLocalZ;
    VehicleScalar frontRollMomentNm = 0.0;
    VehicleScalar rearRollMomentNm = 0.0;

    // Use the previous high-rate wheel reactions as the load driving the first
    // structural torsion mode. A 1 ms history is deliberate: all corners are
    // sampled from the same instant, avoiding wheel-iteration-order coupling.
    // Gross roll remains on the rigid body; only front/rear reaction mismatch
    // twists the virtual suspension pickup frames relative to one another.
    for (const WheelRecord& wheel : vehicle.wheels)
    {
        if (!wheel.state.grounded || wheel.state.normalForce <= 0.0)
            continue;
        const VehicleScalar rollMomentNm =
            static_cast<VehicleScalar>(wheel.description.localMount.x)
            * wheel.state.normalForce;
        const VehicleScalar frontWeight = std::clamp(
            (static_cast<VehicleScalar>(wheel.description.localMount.z)
                - vehicle.chassisFlex.rearReferenceLocalZ) / span,
            0.0,
            1.0);
        frontRollMomentNm += rollMomentNm * frontWeight;
        rearRollMomentNm += rollMomentNm * (1.0 - frontWeight);
    }

    integrateChassisTorsionalCompliance(
        vehicle.chassisFlex,
        frontRollMomentNm,
        rearRollMomentNm,
        static_cast<VehicleScalar>(substepDeltaTime),
        vehicle.chassisFlexState);
}

void VehicleSystem::simulateVehicleSubstep(
    Record& vehicle,
    heritage::physics::RigidBodySystem& bodies,
    const heritage::physics::CollisionSystem& collisions,
    heritage::physics::SurfaceWorld& surfaces,
    float substepDeltaTime)
{
    heritage::physics::RigidBodyPose pose;
    heritage::math::Vec3 centerOfMassLocal{};
    heritage::math::Vec3 linearVelocity{};
    heritage::math::Vec3 angularVelocityDegrees{};
    if (!bodies.pose(vehicle.description.chassisBody, pose)
        || !bodies.centerOfMassLocal(
            vehicle.description.chassisBody,
            centerOfMassLocal)
        || !bodies.linearVelocity(vehicle.description.chassisBody, linearVelocity)
        || !bodies.angularVelocityDegrees(
            vehicle.description.chassisBody,
            angularVelocityDegrees))
    {
        return;
    }

    // The high-rate vehicle step is intentionally orchestrated as explicit
    // stages. Each stage retains the previous numerical order: steering,
    // driveline, then wheel contact/suspension/tire work.
    const SteeringSubstepState steering = updateSteeringSubstep(
        vehicle,
        length(linearVelocity),
        substepDeltaTime);
    const DrivelineSubstepState driveline = updateDrivelineSubstep(
        vehicle,
        substepDeltaTime);

    vehicle.antiLockActiveWheelCount = 0;
    vehicle.tractionControlActiveWheelCount = 0;
    updateChassisFlexSubstep(vehicle, substepDeltaTime);
    prepareAntiRollBarForces(vehicle);
    std::size_t groundedCount = 0;
    for (std::size_t wheelIndex = 0;
        wheelIndex < vehicle.wheels.size();
        ++wheelIndex)
    {
        simulateWheelSubstep(
            vehicle,
            wheelIndex,
            steering,
            driveline,
            pose,
            centerOfMassLocal,
            linearVelocity,
            angularVelocityDegrees,
            bodies,
            collisions,
            surfaces,
            substepDeltaTime,
            wheelIndex < vehicle.antiRollForcesScratch.size()
                ? vehicle.antiRollForcesScratch[wheelIndex]
                : 0.0,
            chassisSectionTwistRadians(
                vehicle.chassisFlex,
                vehicle.chassisFlexState,
                vehicle.wheels[wheelIndex].description.localMount.z));
        if (vehicle.wheels[wheelIndex].state.grounded)
            ++groundedCount;
    }

    vehicle.groundedWheelCount = groundedCount;
    captureDynamicsLabFrame(vehicle, bodies, substepDeltaTime);
}


} // namespace heritage::vehicles

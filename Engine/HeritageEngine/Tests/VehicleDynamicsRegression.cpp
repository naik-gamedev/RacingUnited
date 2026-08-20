#include "PhysicsRegressionCommon.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>

namespace heritage::tests {

bool parkedVehicleStaysQuiet()
{
    PrototypeWorld world;
    if (!createPrototypeWorld(world, 1000.0f))
    {
        std::cerr << "Could not create the 1000 Hz prototype world.\n";
        return false;
    }

    const StabilitySample sample = sampleStability(world, 4.0f, 3.0f);
    printSample("parked_1000hz", sample);

    const Vec3 displacement{
        sample.endPosition.x - sample.startPosition.x,
        sample.endPosition.y - sample.startPosition.y,
        sample.endPosition.z - sample.startPosition.z
    };
    return horizontalMagnitude(displacement) <= 0.010f
        && std::abs(displacement.y) <= 0.010f
        && sample.maximumHorizontalSpeed <= 0.020f
        && sample.maximumVerticalSpeed <= 0.030f
        && sample.maximumAngularSpeedDegrees <= 0.75f
        && sample.verticalPositionSpan <= 0.010f
        && sample.minimumGroundedWheels == 4;
}

bool flatRestSleepsAndThrottleWakes()
{
    PrototypeWorld world;
    if (!createPrototypeWorld(world, 1000.0f))
    {
        std::cerr << "Could not create the flat rest/wake world.\n";
        return false;
    }

    world.bodies.setAllowSleep(world.chassis, true);
    const StabilitySample sample = sampleStability(world, 3.0f, 1.0f);
    printSample("flat_rest", sample);

    world.vehicles.setInputs(world.vehicle, 0.35f, 0.0f, 0.0f, 0.0f);
    stepWorld(world);
    bool sleepingAfterThrottle = true;
    world.bodies.sleeping(world.chassis, sleepingAfterThrottle);
    std::cout << "flat_rest throttle_woke_body="
        << (!sleepingAfterThrottle ? "true" : "false") << '\n';

    return sample.sleepingAtEnd && !sleepingAfterThrottle;
}

bool brakeHeldSteeringWakesAndTracks()
{
    PrototypeWorld world;
    if (!createPrototypeWorld(world, 1000.0f))
    {
        std::cerr << "Could not create the brake-held steering wake world.\n";
        return false;
    }

    world.bodies.setAllowSleep(world.chassis, true);
    world.vehicles.setInputs(world.vehicle, 0.0f, 1.0f, 0.0f, 0.0f);
    const int settleSteps = static_cast<int>(std::round(3.0f / kWorldDeltaTime));
    for (int index = 0; index < settleSteps; ++index)
        stepWorld(world);

    bool sleepingBeforeSteer = false;
    world.bodies.sleeping(world.chassis, sleepingBeforeSteer);

    WheelState before{};
    if (!world.vehicles.wheelState(world.vehicle, 0, before))
        return false;

    // INPUT08 regression: the old parked-rest wake gate ignored steering
    // commands smaller than 0.01 road-wheel degree. 0.0001 normalized input
    // requests only about 0.0038 degree on the prototype car, so it must still
    // wake and become visible when the user has chosen zero deadzone.
    world.vehicles.setInputs(world.vehicle, 0.0f, 1.0f, 0.0001f, 0.0f);
    stepWorld(world);

    bool sleepingAfterMicroSteer = true;
    world.bodies.sleeping(world.chassis, sleepingAfterMicroSteer);
    WheelState afterMicroSteer{};
    if (!world.vehicles.wheelState(world.vehicle, 0, afterMicroSteer))
        return false;

    // Preserve the original large-command controller regression too.
    world.vehicles.setInputs(world.vehicle, 0.0f, 1.0f, 0.75f, 0.0f);
    stepWorld(world);

    bool sleepingAfterSteer = true;
    world.bodies.sleeping(world.chassis, sleepingAfterSteer);

    const int steerSteps = static_cast<int>(std::round(0.20f / kWorldDeltaTime));
    for (int index = 0; index < steerSteps; ++index)
        stepWorld(world);

    WheelState afterRight{};
    if (!world.vehicles.wheelState(world.vehicle, 0, afterRight))
        return false;

    world.vehicles.setInputs(world.vehicle, 0.0f, 1.0f, -0.75f, 0.0f);
    const int reverseSteps = static_cast<int>(std::round(0.40f / kWorldDeltaTime));
    for (int index = 0; index < reverseSteps; ++index)
        stepWorld(world);

    WheelState afterLeft{};
    if (!world.vehicles.wheelState(world.vehicle, 0, afterLeft))
        return false;

    std::cout
        << "brake_held_steering sleeping_before="
        << (sleepingBeforeSteer ? "true" : "false")
        << " woke_on_micro_steer="
        << (!sleepingAfterMicroSteer ? "true" : "false")
        << " micro_deg=" << afterMicroSteer.steerAngleDegrees
        << " woke_on_steer=" << (!sleepingAfterSteer ? "true" : "false")
        << " before_deg=" << before.steerAngleDegrees
        << " right_deg=" << afterRight.steerAngleDegrees
        << " left_deg=" << afterLeft.steerAngleDegrees
        << '\n';

    return sleepingBeforeSteer
        && !sleepingAfterMicroSteer
        && std::abs(afterMicroSteer.steerAngleDegrees) > 0.0005f
        && !sleepingAfterSteer
        && afterRight.steerAngleDegrees > before.steerAngleDegrees + 2.0f
        && afterLeft.steerAngleDegrees < -2.0f;
}

bool highRateSuspensionAgreesWithNativeRate()
{
    PrototypeWorld nativeRate;
    PrototypeWorld highRateInsideWorldRate;
    if (!createPrototypeWorld(nativeRate, 1000.0f)
        || !createPrototypeWorld(highRateInsideWorldRate, 1000.0f))
    {
        std::cerr << "Could not create the rate-comparison worlds.\n";
        return false;
    }

    const StabilitySample nativeRateSample = sampleStability(
        nativeRate,
        0.0f,
        2.0f,
        1.0f / 1000.0f);
    const StabilitySample highRateInsideWorldRateSample = sampleStability(
        highRateInsideWorldRate,
        0.0f,
        2.0f);
    printSample("drop_native_1000hz_world", nativeRateSample);
    printSample("drop_1000hz_inside_120hz_world", highRateInsideWorldRateSample);

    const float finalHeightDifference = std::abs(
        nativeRateSample.endPosition.y
            - highRateInsideWorldRateSample.endPosition.y);
    const float verticalSpeedDifference = std::abs(
        nativeRateSample.maximumVerticalSpeed
            - highRateInsideWorldRateSample.maximumVerticalSpeed);
    const heritage::vehicles::VehicleScalar suspensionSpeedDifference = std::abs(
        nativeRateSample.maximumSuspensionVelocity
            - highRateInsideWorldRateSample.maximumSuspensionVelocity);
    std::cout
        << "rate_comparison final_height_difference_m="
        << finalHeightDifference
        << " peak_vertical_speed_difference_mps="
        << verticalSpeedDifference
        << " peak_suspension_speed_difference_mps="
        << suspensionSpeedDifference
        << '\n';

    return finalHeightDifference <= 0.020f
        && verticalSpeedDifference <= 0.20f
        && suspensionSpeedDifference <= 0.15f;
}

bool parkingBrakeHoldsOnSlope()
{
    PrototypeWorld world;
    if (!createPrototypeWorld(world, 1000.0f)
        || !replaceFloorWithSlope(world, 5.0f))
    {
        std::cerr << "Could not create the parking-brake slope world.\n";
        return false;
    }

    world.bodies.setAllowSleep(world.chassis, true);
    world.vehicles.setInputs(world.vehicle, 0.0f, 0.0f, 0.0f, 1.0f);
    const StabilitySample sample = sampleStability(world, 5.0f, 3.0f);
    printSample("parked_5deg_handbrake", sample);
    printWheelStates(world, "parked_5deg_handbrake");
    VehicleRestState restState;
    if (world.vehicles.restState(world.vehicle, restState))
    {
        std::cout
            << "parked_5deg_handbrake rest_candidate="
            << (restState.candidate ? "true" : "false")
            << " quiet_time_s=" << restState.quietTimeSeconds
            << " required_hold_n=" << restState.requiredHoldForce
            << " available_brake_hold_n="
            << restState.availableBrakeHoldForce
            << '\n';
    }

    const Vec3 displacement{
        sample.endPosition.x - sample.startPosition.x,
        sample.endPosition.y - sample.startPosition.y,
        sample.endPosition.z - sample.startPosition.z
    };
    const bool held = horizontalMagnitude(displacement) <= 0.020f
        && sample.maximumHorizontalSpeed <= 0.030f
        && sample.sleepingAtEnd;

    world.vehicles.setInputs(world.vehicle, 0.0f, 0.0f, 0.0f, 0.0f);
    stepWorld(world);
    bool sleepingAfterRelease = true;
    world.bodies.sleeping(world.chassis, sleepingAfterRelease);
    std::cout << "parked_5deg_handbrake release_woke_body="
        << (!sleepingAfterRelease ? "true" : "false") << '\n';
    return held && !sleepingAfterRelease;
}

bool unbrakedVehicleRollsOnSlope()
{
    PrototypeWorld world;
    if (!createPrototypeWorld(world, 1000.0f)
        || !replaceFloorWithSlope(world, 5.0f))
    {
        std::cerr << "Could not create the unbraked slope world.\n";
        return false;
    }

    world.bodies.setAllowSleep(world.chassis, true);
    world.vehicles.setInputs(world.vehicle, 0.0f, 0.0f, 0.0f, 0.0f);
    const StabilitySample sample = sampleStability(world, 2.0f, 2.0f);
    printSample("unbraked_5deg", sample);

    const Vec3 displacement{
        sample.endPosition.x - sample.startPosition.x,
        sample.endPosition.y - sample.startPosition.y,
        sample.endPosition.z - sample.startPosition.z
    };
    return horizontalMagnitude(displacement) >= 0.050f
        && !sample.sleepingAtEnd;
}

bool turnThenBrakeRemainsStableAtLowSpeed()
{
    PrototypeWorld world;
    if (!createPrototypeWorld(world, 1000.0f))
    {
        std::cerr << "Could not create the turn-and-brake world.\n";
        return false;
    }

    const int settleSteps = static_cast<int>(std::round(2.0f / kWorldDeltaTime));
    for (int index = 0; index < settleSteps; ++index)
        stepWorld(world);

    world.vehicles.setInputs(world.vehicle, 0.75f, 0.0f, 0.42f, 0.0f);
    const int cornerSteps = static_cast<int>(std::round(2.0f / kWorldDeltaTime));
    for (int index = 0; index < cornerSteps; ++index)
        stepWorld(world);

    world.vehicles.setInputs(world.vehicle, 0.65f, 0.0f, 0.0f, 0.0f);
    const int straightenSteps = static_cast<int>(std::round(
        0.75f / kWorldDeltaTime));
    for (int index = 0; index < straightenSteps; ++index)
        stepWorld(world);

    world.vehicles.setInputs(world.vehicle, 0.0f, 1.0f, 0.0f, 0.0f);
    const int brakeSteps = static_cast<int>(std::round(5.0f / kWorldDeltaTime));
    int lowSpeedYawReversals = 0;
    int lowSpeedRollReversals = 0;
    float previousSignificantYawSign = 0.0f;
    float previousSignificantRollSign = 0.0f;
    float maximumLowSpeedYawRate = 0.0f;
    float maximumLowSpeedRollRate = 0.0f;
    heritage::vehicles::VehicleScalar maximumLowSpeedRearLateralSpeed = 0.0;
    float finalSpeed = 0.0f;
    for (int index = 0; index < brakeSteps; ++index)
    {
        stepWorld(world);

        Vec3 linearVelocity{};
        Vec3 angularVelocityDegrees{};
        world.bodies.linearVelocity(world.chassis, linearVelocity);
        world.bodies.angularVelocityDegrees(
            world.chassis,
            angularVelocityDegrees);
        RigidBodyPose brakingPose;
        world.bodies.pose(world.chassis, brakingPose);
        const float brakingYawRadians = brakingPose.rotationDegrees.y
            * 3.14159265358979323846f / 180.0f;
        const Vec3 chassisForward{
            std::sin(brakingYawRadians),
            0.0f,
            std::cos(brakingYawRadians)
        };
        const float localRollRate = angularVelocityDegrees.x * chassisForward.x
            + angularVelocityDegrees.z * chassisForward.z;
        finalSpeed = magnitude(linearVelocity);
        if (finalSpeed > 2.0f)
            continue;

        maximumLowSpeedYawRate = std::max(
            maximumLowSpeedYawRate,
            std::abs(angularVelocityDegrees.y));
        maximumLowSpeedRollRate = std::max(
            maximumLowSpeedRollRate,
            std::abs(localRollRate));
        for (std::size_t wheelIndex = 2; wheelIndex < 4; ++wheelIndex)
        {
            WheelState state;
            if (world.vehicles.wheelState(world.vehicle, wheelIndex, state))
            {
                maximumLowSpeedRearLateralSpeed = std::max(
                    maximumLowSpeedRearLateralSpeed,
                    std::abs(state.lateralSpeed));
            }
        }

        if (std::abs(angularVelocityDegrees.y) >= 0.35f)
        {
            const float yawSign = angularVelocityDegrees.y > 0.0f
                ? 1.0f
                : -1.0f;
            if (previousSignificantYawSign != 0.0f
                && yawSign != previousSignificantYawSign)
            {
                ++lowSpeedYawReversals;
            }
            previousSignificantYawSign = yawSign;
        }

        if (std::abs(localRollRate) >= 0.35f)
        {
            const float rollSign = localRollRate > 0.0f
                ? 1.0f
                : -1.0f;
            if (previousSignificantRollSign != 0.0f
                && rollSign != previousSignificantRollSign)
            {
                ++lowSpeedRollReversals;
            }
            previousSignificantRollSign = rollSign;
        }
    }

    std::cout
        << "turn_then_brake final_speed_mps=" << finalSpeed
        << " max_low_speed_yaw_degps=" << maximumLowSpeedYawRate
        << " max_low_speed_local_roll_degps=" << maximumLowSpeedRollRate
        << " max_rear_lateral_speed_mps="
        << maximumLowSpeedRearLateralSpeed
        << " low_speed_yaw_reversals=" << lowSpeedYawReversals
        << " low_speed_roll_reversals=" << lowSpeedRollReversals
        << '\n';

    return finalSpeed <= 0.15f
        && maximumLowSpeedYawRate <= 0.10f
        && lowSpeedYawReversals <= 1
        && maximumLowSpeedRollRate <= 0.25f
        && lowSpeedRollReversals <= 1;
}


bool dynamicsLabCapturesHighRateTelemetry()
{
    PrototypeWorld world;
    if (!createPrototypeWorld(world, 1000.0f))
    {
        std::cerr << "Could not create the dynamics-lab world.\n";
        return false;
    }
    if (!world.vehicles.startDynamicsLabCapture(
            world.vehicle, 1.0f, 1000.0f))
    {
        std::cerr << "Could not start the dynamics lab: "
            << world.vehicles.lastError() << '\n';
        return false;
    }

    world.vehicles.setInputs(world.vehicle, 0.55f, 0.0f, 0.15f, 0.0f);
    for (int index = 0; index < 120; ++index)
        stepWorld(world);

    heritage::vehicles::DynamicsLabSummary summary;
    if (!world.vehicles.dynamicsLabSummary(world.vehicle, summary))
        return false;

    std::vector<float> speedSeries;
    std::vector<float> wheelLoadSeries;
    std::vector<float> damperPowerSeries;
    std::vector<float> unsprungVelocitySeries;
    std::vector<float> tireDeflectionSeries;
    std::vector<float> camberSeries;
    std::vector<float> toeSeries;
    const bool speedWorked = world.vehicles.dynamicsLabMetricSeries(
        world.vehicle,
        heritage::vehicles::DynamicsLabMetric::SpeedKph,
        0,
        64,
        speedSeries);
    const bool wheelLoadWorked = world.vehicles.dynamicsLabMetricSeries(
        world.vehicle,
        heritage::vehicles::DynamicsLabMetric::WheelNormalForceNewtons,
        0,
        64,
        wheelLoadSeries);
    const bool damperPowerWorked = world.vehicles.dynamicsLabMetricSeries(
        world.vehicle,
        heritage::vehicles::DynamicsLabMetric::WheelDamperDissipationWatts,
        0,
        64,
        damperPowerSeries);
    const bool unsprungVelocityWorked = world.vehicles.dynamicsLabMetricSeries(
        world.vehicle,
        heritage::vehicles::DynamicsLabMetric::WheelUnsprungVelocityMps,
        0,
        64,
        unsprungVelocitySeries);
    const bool tireDeflectionWorked = world.vehicles.dynamicsLabMetricSeries(
        world.vehicle,
        heritage::vehicles::DynamicsLabMetric::WheelTireDeflectionMillimeters,
        0,
        64,
        tireDeflectionSeries);
    const bool camberWorked = world.vehicles.dynamicsLabMetricSeries(
        world.vehicle,
        heritage::vehicles::DynamicsLabMetric::WheelCamberDegrees,
        0,
        64,
        camberSeries);
    const bool toeWorked = world.vehicles.dynamicsLabMetricSeries(
        world.vehicle,
        heritage::vehicles::DynamicsLabMetric::WheelToeDegrees,
        0,
        64,
        toeSeries);

    std::cout
        << "dynamics_lab samples=" << summary.sampleCount
        << " capacity=" << summary.sampleCapacity
        << " duration_s=" << summary.durationSeconds
        << " capture_hz=" << summary.requestedCaptureHertz
        << " peak_speed_kph=" << summary.peakSpeedKph
        << " peak_suspension_speed_mps="
        << summary.peakAbsoluteSuspensionVelocityMps
        << " peak_unsprung_speed_mps="
        << summary.peakAbsoluteUnsprungVelocityMps
        << " peak_tire_deflection_mm="
        << summary.peakTireDeflectionMillimeters
        << " speed_plot_points=" << speedSeries.size()
        << " load_plot_points=" << wheelLoadSeries.size()
        << '\n';

    return summary.captureComplete
        && !summary.recording
        && summary.sampleCount == 1000
        && summary.sampleCapacity == 1000
        && summary.wheelCount == 4
        && std::abs(summary.durationSeconds - 1.0) <= 0.001
        && summary.peakSpeedKph > 0.1f
        && speedWorked
        && wheelLoadWorked
        && damperPowerWorked
        && unsprungVelocityWorked
        && tireDeflectionWorked
        && camberWorked
        && toeWorked
        && summary.peakAbsoluteUnsprungVelocityMps > 0.0f
        && summary.peakTireDeflectionMillimeters > 0.0f
        && !speedSeries.empty()
        && speedSeries.size() <= 64
        && wheelLoadSeries.size() == speedSeries.size()
        && damperPowerSeries.size() == speedSeries.size()
        && unsprungVelocitySeries.size() == speedSeries.size()
        && tireDeflectionSeries.size() == speedSeries.size()
        && camberSeries.size() == speedSeries.size()
        && toeSeries.size() == speedSeries.size();
}


bool steeringDirectionAndAckermannAreSymmetric()
{
    PrototypeWorld world;
    if (!createPrototypeWorld(world, 1000.0f))
        return false;

    // Set a fast but deterministic road-wheel response so this test measures
    // sign and geometry rather than keyboard-feel tuning.
    if (!world.vehicles.setSteeringGeometry(
            world.vehicle,
            1.0f,
            720.0f,
            720.0f,
            1.0f,
            40.0f))
    {
        return false;
    }

    const auto settleSteering = [&](float input) {
        world.vehicles.setInputs(world.vehicle, 0.0f, 0.0f, input, 1.0f);
        for (int step = 0; step < 20; ++step)
            stepWorld(world, 1.0f / 120.0f);
    };

    WheelState leftFl;
    WheelState leftFr;
    settleSteering(-1.0f); // native convention: negative = left
    const bool leftRead = world.vehicles.wheelState(world.vehicle, 0, leftFl)
        && world.vehicles.wheelState(world.vehicle, 1, leftFr);

    WheelState rightFl;
    WheelState rightFr;
    settleSteering(1.0f); // native convention: positive = right
    const bool rightRead = world.vehicles.wheelState(world.vehicle, 0, rightFl)
        && world.vehicles.wheelState(world.vehicle, 1, rightFr);

    const bool leftDirection = leftRead
        && leftFl.steerAngleDegrees < -1.0
        && leftFr.steerAngleDegrees < -1.0
        && leftFl.worldWheelForward.x < -0.01f
        && leftFr.worldWheelForward.x < -0.01f;
    const bool rightDirection = rightRead
        && rightFl.steerAngleDegrees > 1.0
        && rightFr.steerAngleDegrees > 1.0
        && rightFl.worldWheelForward.x > 0.01f
        && rightFr.worldWheelForward.x > 0.01f;

    // On a left turn the left wheel is inside; on a right turn the right wheel
    // is inside. Ideal Ackermann therefore gives that wheel the larger lock.
    const bool insideWheelCorrect = leftDirection && rightDirection
        && std::abs(leftFl.steerAngleDegrees)
            > std::abs(leftFr.steerAngleDegrees)
        && std::abs(rightFr.steerAngleDegrees)
            > std::abs(rightFl.steerAngleDegrees);

    const bool symmetric = insideWheelCorrect
        && std::abs(
            std::abs(leftFl.steerAngleDegrees)
            - std::abs(rightFr.steerAngleDegrees)) < 0.05
        && std::abs(
            std::abs(leftFr.steerAngleDegrees)
            - std::abs(rightFl.steerAngleDegrees)) < 0.05;

    std::cout
        << "steering left_FL_FR_deg="
        << leftFl.steerAngleDegrees << ',' << leftFr.steerAngleDegrees
        << " right_FL_FR_deg="
        << rightFl.steerAngleDegrees << ',' << rightFr.steerAngleDegrees
        << " left_forward_x="
        << leftFl.worldWheelForward.x << ',' << leftFr.worldWheelForward.x
        << " right_forward_x="
        << rightFl.worldWheelForward.x << ',' << rightFr.worldWheelForward.x
        << '\n';
    return leftDirection && rightDirection && insideWheelCorrect && symmetric;
}

} // namespace heritage::tests

#pragma once

#include "../../VehiclePrecision.hpp"

#include <cstddef>

namespace heritage::vehicles {

// Reusable left/right suspension coupling. The bar itself is represented as a
// torsional spring/damper with explicit lever arms, so the same mechanism can
// couple MacPherson, wishbone, trailing-arm, live-axle or other suspension
// providers without embedding anti-roll behavior inside any one geometry type.
struct SuspensionAntiRollBarDescription
{
    std::size_t leftWheelIndex = 0;
    std::size_t rightWheelIndex = 1;
    bool enabled = true;
    VehicleScalar torsionalStiffnessNmPerRad = 900.0;
    VehicleScalar torsionalDampingNmsPerRad = 35.0;
    VehicleScalar leftLeverArmM = 0.20;
    VehicleScalar rightLeverArmM = 0.20;
    VehicleScalar leftLinkMotionRatio = 1.0;
    VehicleScalar rightLinkMotionRatio = 1.0;
    VehicleScalar maximumWheelForceN = 12000.0;
};

struct SuspensionAntiRollBarInput
{
    VehicleScalar leftCompressionM = 0.0;
    VehicleScalar rightCompressionM = 0.0;
    VehicleScalar leftCompressionVelocityMps = 0.0;
    VehicleScalar rightCompressionVelocityMps = 0.0;
};

struct SuspensionAntiRollBarOutput
{
    VehicleScalar twistRadians = 0.0;
    VehicleScalar twistRateRadiansPerSecond = 0.0;
    VehicleScalar elasticTorqueNm = 0.0;
    VehicleScalar dampingTorqueNm = 0.0;
    VehicleScalar totalTorqueNm = 0.0;
    VehicleScalar leftWheelForceN = 0.0;
    VehicleScalar rightWheelForceN = 0.0;
};

bool validSuspensionAntiRollBarDescription(
    const SuspensionAntiRollBarDescription& description);

SuspensionAntiRollBarOutput evaluateSuspensionAntiRollBar(
    const SuspensionAntiRollBarDescription& description,
    const SuspensionAntiRollBarInput& input);

} // namespace heritage::vehicles

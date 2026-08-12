#pragma once

#include "../../../Core/Math/Math.hpp"
#include "../../VehiclePrecision.hpp"

namespace heritage::vehicles {

// Current steering-axis intersection with the local road plane. Positive
// mechanical trail means the contact patch trails behind the steering-axis
// ground intersection in the wheel's forward direction.
struct SteeringGroundGeometry
{
    bool valid = false;
    heritage::math::Vec3 steeringAxisGroundPointWorld{};
    VehicleScalar signedScrubRadiusM = 0.0;
    VehicleScalar scrubRadiusMagnitudeM = 0.0;
    VehicleScalar mechanicalTrailM = 0.0;
};

SteeringGroundGeometry evaluateSteeringGroundGeometry(
    const heritage::math::Vec3& steeringAxisPointWorld,
    const heritage::math::Vec3& steeringAxisDirectionWorld,
    const heritage::math::Vec3& contactPointWorld,
    const heritage::math::Vec3& contactNormalWorld,
    const heritage::math::Vec3& wheelForwardWorld,
    const heritage::math::Vec3& wheelRightWorld);

} // namespace heritage::vehicles

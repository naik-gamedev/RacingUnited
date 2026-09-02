#pragma once

#include "../../Geometry/LiveAxle/LiveAxleKinematics.hpp"

namespace heritage::vehicles {

// SUSP09 adds a semi-elliptic leaf pack and rear shackle on top of SUSP08's
// single rigid axle body. The linkage core still owns axle translation/roll;
// this descendant owns the compliant leaf coordinate and its leverage.
struct LeafSpringLiveAxleHardpoints
{
    bool authored = false;
    LiveAxleHardpoints axle;
    heritage::math::Vec3 leftLeafFrontEye{};
    heritage::math::Vec3 leftLeafRearShacklePivot{};
    heritage::math::Vec3 leftLeafRearEye{};
    heritage::math::Vec3 leftLeafAxleClamp{};
    heritage::math::Vec3 rightLeafFrontEye{};
    heritage::math::Vec3 rightLeafRearShacklePivot{};
    heritage::math::Vec3 rightLeafRearEye{};
    heritage::math::Vec3 rightLeafAxleClamp{};
};

struct LeafSpringLiveAxleInput
{
    float leftCompressionM = 0.0f;
    float rightCompressionM = 0.0f;
    bool evaluateLeftWheel = true;
    float steeringDegrees = 0.0f;
    float staticCamberDegrees = 0.0f;
    float staticToeDegrees = 0.0f;
};

struct LeafSpringLiveAxleOutput
{
    bool valid = false;
    LiveAxleKinematicsOutput axle;
    float leafCompressionM = 0.0f;
    float leafMotionRatio = 1.0f;
    float shackleAngleRadians = 0.0f;
    float shackleTravelRadians = 0.0f;
    heritage::math::Vec3 localLeafRearEye{};
    heritage::math::Vec3 localLeafAxleClamp{};
};

bool validLeafSpringLiveAxleHardpoints(
    const LeafSpringLiveAxleHardpoints& hardpoints);

LeafSpringLiveAxleOutput evaluateLeafSpringLiveAxle(
    const LeafSpringLiveAxleHardpoints& hardpoints,
    const LeafSpringLiveAxleInput& input);

} // namespace heritage::vehicles

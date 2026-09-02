#pragma once

#include "../../../../Core/Math/Math.hpp"

namespace heritage::vehicles {

// SUSP10: chassis-local conventional/telescopic motorcycle fork geometry.
// The stem points define the physical steering/fork slide axis and wheelCenter
// carries the authored axle offset/trail. Positive compression slides the axle
// upward along that axis; steering rotates the complete lower fork/axle package
// around the same physical steering axis.
struct MotorcycleForkHardpoints
{
    bool authored = false;
    heritage::math::Vec3 steeringStemUpper{};
    heritage::math::Vec3 steeringStemLower{};
    heritage::math::Vec3 wheelCenter{};
};

struct MotorcycleForkKinematicsInput
{
    float compressionM = 0.0f;
    float steeringDegrees = 0.0f;
    heritage::math::Vec3 suspensionDirection{ 0.0f, -1.0f, 0.0f };
    float staticCamberDegrees = 0.0f;
    float staticToeDegrees = 0.0f;
};

struct MotorcycleForkKinematicsOutput
{
    bool valid = false;
    bool travelClamped = false;
    float forkCompressionM = 0.0f;
    float springCompressionM = 0.0f;
    float springMotionRatio = 1.0f;
    float damperCompressionM = 0.0f;
    float damperMotionRatio = 1.0f;
    float rakeDegreesFromVertical = 0.0f;
    float wheelbaseDeltaM = 0.0f;
    float camberDegrees = 0.0f;
    float toeDegrees = 0.0f;
    heritage::math::Vec3 localSteeringAxis{ 0.0f, -1.0f, 0.0f };
    heritage::math::Vec3 localSteeringAxisPoint{};
    heritage::math::Vec3 localWheelCenter{};
    heritage::math::Vec3 localWheelForward{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 localWheelRight{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 localWheelUp{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 localUprightRotationDegrees{};
};

bool validMotorcycleForkHardpoints(const MotorcycleForkHardpoints& hardpoints);

MotorcycleForkKinematicsOutput evaluateMotorcycleForkKinematics(
    const MotorcycleForkHardpoints& hardpoints,
    const MotorcycleForkKinematicsInput& input);

} // namespace heritage::vehicles

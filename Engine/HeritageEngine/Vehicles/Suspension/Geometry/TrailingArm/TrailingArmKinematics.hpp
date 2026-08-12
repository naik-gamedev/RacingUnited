#pragma once

#include "../../../../Core/Math/Math.hpp"

namespace heritage::vehicles {

// Chassis-local reference hardpoints for one independent trailing-arm corner.
// The arm rotates as one rigid assembly about the line through its two chassis
// pivots. The wheel centre and damper lower eye are arm-fixed; the damper upper
// eye is chassis-fixed. A transverse torsion bar keyed to the arm shares the
// same arm rotation and therefore acquires real angular spring travel.
struct TrailingArmHardpoints
{
    bool authored = false;
    heritage::math::Vec3 armPivotInner{};
    heritage::math::Vec3 armPivotOuter{};
    heritage::math::Vec3 wheelCenter{};
    heritage::math::Vec3 damperUpperMount{};
    heritage::math::Vec3 damperLowerMount{};
};

struct TrailingArmKinematicsInput
{
    // Positive compression moves the wheel opposite the authored suspension
    // direction. static camber/toe describe the reference upright orientation.
    float compressionM = 0.0f;
    heritage::math::Vec3 suspensionDirection{ 0.0f, -1.0f, 0.0f };
    float staticCamberDegrees = 0.0f;
    float staticToeDegrees = 0.0f;
};

struct TrailingArmKinematicsOutput
{
    bool valid = false;
    bool travelClamped = false;
    float armRotationRadians = 0.0f;
    float torsionBarTwistRadians = 0.0f;
    float torsionBarAngularMotionRatioRadPerM = 0.0f;
    float referenceTorsionBarAngularMotionRatioRadPerM = 0.0f;
    float damperCompressionM = 0.0f;
    float damperMotionRatio = 1.0f;
    float camberDegrees = 0.0f;
    float toeDegrees = 0.0f;
    heritage::math::Vec3 localWheelCenter{};
    heritage::math::Vec3 localWheelForward{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 localWheelRight{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 localWheelUp{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 localUprightRotationDegrees{};
};

bool validTrailingArmHardpoints(const TrailingArmHardpoints& hardpoints);

TrailingArmKinematicsOutput evaluateTrailingArmKinematics(
    const TrailingArmHardpoints& hardpoints,
    const TrailingArmKinematicsInput& input);

} // namespace heritage::vehicles

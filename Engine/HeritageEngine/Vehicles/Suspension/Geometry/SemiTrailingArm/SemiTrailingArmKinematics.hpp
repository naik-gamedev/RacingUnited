#pragma once

#include "../../../../Core/Math/Math.hpp"

namespace heritage::vehicles {

// SUSP13: one rigid semi-trailing arm. The pivot axis may be swept in plan and
// elevation; that geometry naturally generates camber/toe migration and wheel
// scrub as the arm rotates. Coil spring and damper have independent arm-fixed
// lower eyes and chassis-fixed upper eyes.
struct SemiTrailingArmHardpoints
{
    bool authored = false;
    heritage::math::Vec3 armPivotInner{};
    heritage::math::Vec3 armPivotOuter{};
    heritage::math::Vec3 wheelCenter{};
    heritage::math::Vec3 springUpperMount{};
    heritage::math::Vec3 springLowerMount{};
    heritage::math::Vec3 damperUpperMount{};
    heritage::math::Vec3 damperLowerMount{};
};

struct SemiTrailingArmKinematicsInput
{
    float compressionM = 0.0f;
    heritage::math::Vec3 suspensionDirection{0.0f,-1.0f,0.0f};
    float staticCamberDegrees = 0.0f;
    float staticToeDegrees = 0.0f;
};

struct SemiTrailingArmKinematicsOutput
{
    bool valid = false;
    bool travelClamped = false;
    float armRotationRadians = 0.0f;
    float armAngularMotionRatioRadPerM = 0.0f; // signed d(theta)/d(compression)
    float bumpSteerDegrees = 0.0f;
    float camberDegrees = 0.0f;
    float toeDegrees = 0.0f;
    float springCompressionM = 0.0f;
    float springMotionRatio = 1.0f;
    float damperCompressionM = 0.0f;
    float damperMotionRatio = 1.0f;
    heritage::math::Vec3 localWheelCenter{};
    heritage::math::Vec3 localWheelForward{0,0,1};
    heritage::math::Vec3 localWheelRight{1,0,0};
    heritage::math::Vec3 localWheelUp{0,1,0};
    heritage::math::Vec3 localUprightRotationDegrees{};
};

bool validSemiTrailingArmHardpoints(const SemiTrailingArmHardpoints& hardpoints);
SemiTrailingArmKinematicsOutput evaluateSemiTrailingArmKinematics(
    const SemiTrailingArmHardpoints& hardpoints,
    const SemiTrailingArmKinematicsInput& input);

} // namespace heritage::vehicles

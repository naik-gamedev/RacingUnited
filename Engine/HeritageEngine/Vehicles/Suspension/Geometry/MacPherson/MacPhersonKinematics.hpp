#pragma once

#include "../../../../Core/Math/Math.hpp"

namespace heritage::vehicles {

// Chassis-local reference hardpoints for a MacPherson strut corner. All eight
// points describe the vehicle at authored/static ride height. The provider
// treats the upright as rigid, the lower arm as a rigid triangle hinged about
// its two chassis pivots, the strut top and tie-rod inner as chassis-fixed, and
// the steering axis as the line from lower ball joint to strut top mount.
struct MacPhersonHardpoints
{
    bool authored = false;
    heritage::math::Vec3 strutTopMount{};
    heritage::math::Vec3 strutUprightMount{};
    heritage::math::Vec3 lowerArmInnerFront{};
    heritage::math::Vec3 lowerArmInnerRear{};
    heritage::math::Vec3 lowerBallJoint{};
    heritage::math::Vec3 tieRodInner{};
    heritage::math::Vec3 tieRodOuter{};
    heritage::math::Vec3 wheelCenter{};
};

struct MacPhersonKinematicsInput
{
    // Positive compression moves the lower ball joint opposite the authored
    // suspension direction. Steering is the commanded road-wheel rotation;
    // passive bump steer from the tie rod is solved in addition to it.
    float compressionM = 0.0f;
    float steeringDegrees = 0.0f;
    heritage::math::Vec3 suspensionDirection{ 0.0f, -1.0f, 0.0f };
    float staticCamberDegrees = 0.0f;
    float staticToeDegrees = 0.0f;
    bool casterOverrideEnabled = false;
    float staticCasterDegrees = 0.0f;
};

struct MacPhersonKinematicsOutput
{
    bool valid = false;
    bool travelClamped = false;
    float bumpSteerDegrees = 0.0f;
    float camberDegrees = 0.0f;
    float toeDegrees = 0.0f;
    float strutCompressionM = 0.0f;
    float springMotionRatio = 1.0f;
    heritage::math::Vec3 localSteeringAxis{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 localSteeringAxisPoint{};
    heritage::math::Vec3 localWheelCenter{};
    heritage::math::Vec3 localWheelForward{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 localWheelRight{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 localWheelUp{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 localUprightRotationDegrees{};
};

bool validMacPhersonHardpoints(const MacPhersonHardpoints& hardpoints);

MacPhersonKinematicsOutput evaluateMacPhersonKinematics(
    const MacPhersonHardpoints& hardpoints,
    const MacPhersonKinematicsInput& input);

} // namespace heritage::vehicles

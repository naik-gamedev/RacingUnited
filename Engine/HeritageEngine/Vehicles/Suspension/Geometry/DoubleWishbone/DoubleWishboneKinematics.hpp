#pragma once

#include "../../../../Core/Math/Math.hpp"

namespace heritage::vehicles {

// Chassis-local reference hardpoints for a conventional unequal-length
// double-wishbone corner. Each arm is a rigid triangle hinged about its two
// chassis pivots. The upright is rigid between upper/lower ball joints; the
// tie rod resolves passive bump steer and commanded steering rotates about the
// instantaneous ball-joint steering axis. The damper lower eye is lower-arm
// fixed and its upper eye chassis-fixed, so motion ratio is geometry-derived.
struct DoubleWishboneHardpoints
{
    bool authored = false;
    heritage::math::Vec3 upperArmInnerFront{};
    heritage::math::Vec3 upperArmInnerRear{};
    heritage::math::Vec3 upperBallJoint{};
    heritage::math::Vec3 lowerArmInnerFront{};
    heritage::math::Vec3 lowerArmInnerRear{};
    heritage::math::Vec3 lowerBallJoint{};
    heritage::math::Vec3 tieRodInner{};
    heritage::math::Vec3 tieRodOuter{};
    heritage::math::Vec3 wheelCenter{};
    heritage::math::Vec3 damperUpperMount{};
    heritage::math::Vec3 damperLowerMount{};
};

struct DoubleWishboneKinematicsInput
{
    float compressionM = 0.0f;
    float steeringDegrees = 0.0f;
    heritage::math::Vec3 suspensionDirection{ 0.0f, -1.0f, 0.0f };
    float staticCamberDegrees = 0.0f;
    float staticToeDegrees = 0.0f;
};

struct DoubleWishboneKinematicsOutput
{
    bool valid = false;
    bool travelClamped = false;
    float bumpSteerDegrees = 0.0f;
    float camberDegrees = 0.0f;
    float toeDegrees = 0.0f;
    float casterDegrees = 0.0f;
    float kingpinInclinationDegrees = 0.0f;
    float upperArmRotationRadians = 0.0f;
    float lowerArmRotationRadians = 0.0f;
    float damperCompressionM = 0.0f;
    float damperMotionRatio = 1.0f;
    float springMotionRatio = 1.0f;
    heritage::math::Vec3 localSteeringAxis{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 localSteeringAxisPoint{};
    heritage::math::Vec3 localWheelCenter{};
    heritage::math::Vec3 localWheelForward{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 localWheelRight{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 localWheelUp{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 localUprightRotationDegrees{};
};

// Linkage-only validation/evaluation is shared by direct-acting and
// pushrod/rocker descendants. It intentionally ignores the two direct-damper
// hardpoints while preserving the same A-arm/upright/tie-rod geometry.
bool validDoubleWishboneLinkageHardpoints(
    const DoubleWishboneHardpoints& hardpoints);

bool validDoubleWishboneHardpoints(const DoubleWishboneHardpoints& hardpoints);

DoubleWishboneKinematicsOutput evaluateDoubleWishboneLinkageKinematics(
    const DoubleWishboneHardpoints& hardpoints,
    const DoubleWishboneKinematicsInput& input);

DoubleWishboneKinematicsOutput evaluateDoubleWishboneKinematics(
    const DoubleWishboneHardpoints& hardpoints,
    const DoubleWishboneKinematicsInput& input);

} // namespace heritage::vehicles

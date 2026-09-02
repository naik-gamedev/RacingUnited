#pragma once

#include "../DoubleWishbone/DoubleWishboneKinematics.hpp"

namespace heritage::vehicles {

// SUSP07: pushrod/rocker actuation layered on the SUSP06 unequal-length
// double-wishbone linkage. Wheel/upright/tie-rod kinematics remain owned by the
// shared wishbone solver. The pushrod is lower-arm fixed and drives a rigid
// rocker around a chassis-fixed axis. Spring and damper can use independent
// rocker/chassis attachment points, so their motion ratios may differ and vary
// non-linearly through wheel travel.
struct PushrodDoubleWishboneHardpoints
{
    bool authored = false;
    DoubleWishboneHardpoints wishbone{};
    heritage::math::Vec3 pushrodLowerArmMount{};
    heritage::math::Vec3 rockerPivotFront{};
    heritage::math::Vec3 rockerPivotRear{};
    heritage::math::Vec3 rockerPushrodMount{};
    heritage::math::Vec3 springChassisMount{};
    heritage::math::Vec3 springRockerMount{};
    heritage::math::Vec3 damperChassisMount{};
    heritage::math::Vec3 damperRockerMount{};
};

struct PushrodDoubleWishboneKinematicsOutput
{
    bool valid = false;
    bool travelClamped = false;
    DoubleWishboneKinematicsOutput linkage{};
    float rockerAngleRadians = 0.0f;
    float springCompressionM = 0.0f;
    float springMotionRatio = 1.0f;
    float damperCompressionM = 0.0f;
    float damperMotionRatio = 1.0f;
    float pushrodLengthErrorM = 0.0f;
    heritage::math::Vec3 localPushrodLowerMount{};
    heritage::math::Vec3 localRockerPushrodMount{};
    heritage::math::Vec3 localSpringRockerMount{};
    heritage::math::Vec3 localDamperRockerMount{};
};

bool validPushrodDoubleWishboneHardpoints(
    const PushrodDoubleWishboneHardpoints& hardpoints);

PushrodDoubleWishboneKinematicsOutput evaluatePushrodDoubleWishboneKinematics(
    const PushrodDoubleWishboneHardpoints& hardpoints,
    const DoubleWishboneKinematicsInput& input);

} // namespace heritage::vehicles

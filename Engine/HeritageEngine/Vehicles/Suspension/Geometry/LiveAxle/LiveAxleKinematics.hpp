#pragma once

#include "../../../../Core/Math/Math.hpp"

namespace heritage::vehicles {

// Chassis-local reference geometry for a rigid live axle. Both wheel centres
// belong to one axle body. The axle is laterally located by a Panhard rod and
// longitudinally located by left/right trailing links; coil/leaf force elements
// can attach independently at each side through spring/damper hardpoints.
struct LiveAxleHardpoints
{
    bool authored = false;
    heritage::math::Vec3 axleCenter{};
    heritage::math::Vec3 leftWheelCenter{};
    heritage::math::Vec3 rightWheelCenter{};
    heritage::math::Vec3 panhardChassisMount{};
    heritage::math::Vec3 panhardAxleMount{};
    heritage::math::Vec3 leftTrailingChassisMount{};
    heritage::math::Vec3 leftTrailingAxleMount{};
    heritage::math::Vec3 rightTrailingChassisMount{};
    heritage::math::Vec3 rightTrailingAxleMount{};
    heritage::math::Vec3 leftSpringChassisMount{};
    heritage::math::Vec3 leftSpringAxleMount{};
    heritage::math::Vec3 rightSpringChassisMount{};
    heritage::math::Vec3 rightSpringAxleMount{};
    heritage::math::Vec3 leftDamperChassisMount{};
    heritage::math::Vec3 leftDamperAxleMount{};
    heritage::math::Vec3 rightDamperChassisMount{};
    heritage::math::Vec3 rightDamperAxleMount{};
};

struct LiveAxleKinematicsInput
{
    float leftCompressionM = 0.0f;
    float rightCompressionM = 0.0f;
    bool evaluateLeftWheel = true;
    float steeringDegrees = 0.0f;
    float staticCamberDegrees = 0.0f;
    float staticToeDegrees = 0.0f;
};

struct LiveAxleKinematicsOutput
{
    bool valid = false;
    bool travelClamped = false;
    float axleRollRadians = 0.0f;
    float lateralShiftM = 0.0f;
    float longitudinalShiftM = 0.0f;
    float springCompressionM = 0.0f;
    float springMotionRatio = 1.0f;
    float damperCompressionM = 0.0f;
    float damperMotionRatio = 1.0f;
    float camberDegrees = 0.0f;
    float toeDegrees = 0.0f;
    heritage::math::Vec3 localAxleCenter{};
    heritage::math::Vec3 localWheelCenter{};
    heritage::math::Vec3 localWheelForward{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 localWheelRight{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 localWheelUp{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 localUprightRotationDegrees{};
};

bool validLiveAxleHardpoints(const LiveAxleHardpoints& hardpoints);

LiveAxleKinematicsOutput evaluateLiveAxleKinematics(
    const LiveAxleHardpoints& hardpoints,
    const LiveAxleKinematicsInput& input);

} // namespace heritage::vehicles

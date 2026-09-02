#pragma once

#include "../../../../Core/Math/Math.hpp"

namespace heritage::vehicles {

// SUSP12: generic five-link independent suspension. The wheel carrier is a
// rigid body constrained by five fixed-length links; requested wheel travel is
// the sixth scalar constraint. Link 5 is the toe/steering link. Its chassis
// pickup may translate along an authored rack axis for steerable front layouts,
// while a fixed rack naturally produces passive bump steer on rear layouts.
struct MultiLinkHardpoints
{
    bool authored = false;
    heritage::math::Vec3 link1Inner{};
    heritage::math::Vec3 link1Outer{};
    heritage::math::Vec3 link2Inner{};
    heritage::math::Vec3 link2Outer{};
    heritage::math::Vec3 link3Inner{};
    heritage::math::Vec3 link3Outer{};
    heritage::math::Vec3 link4Inner{};
    heritage::math::Vec3 link4Outer{};
    heritage::math::Vec3 toeLinkInner{};
    heritage::math::Vec3 toeLinkOuter{};
    heritage::math::Vec3 wheelCenter{};
    heritage::math::Vec3 springUpperMount{};
    heritage::math::Vec3 springLowerMount{};
    heritage::math::Vec3 damperUpperMount{};
    heritage::math::Vec3 damperLowerMount{};
    heritage::math::Vec3 steeringRackAxisStart{};
    heritage::math::Vec3 steeringRackAxisEnd{};
};

struct MultiLinkKinematicsInput
{
    float compressionM = 0.0f;
    float steeringDegrees = 0.0f;
    heritage::math::Vec3 suspensionDirection{ 0.0f, -1.0f, 0.0f };
    float staticCamberDegrees = 0.0f;
    float staticToeDegrees = 0.0f;
};

struct MultiLinkKinematicsOutput
{
    bool valid = false;
    bool travelClamped = false;
    float bumpSteerDegrees = 0.0f;
    float camberDegrees = 0.0f;
    float toeDegrees = 0.0f;
    float casterDegrees = 0.0f;
    float kingpinInclinationDegrees = 0.0f;
    float steeringRackDisplacementM = 0.0f;
    float springCompressionM = 0.0f;
    float springMotionRatio = 1.0f;
    float damperCompressionM = 0.0f;
    float damperMotionRatio = 1.0f;
    heritage::math::Vec3 localSteeringAxis{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 localSteeringAxisPoint{};
    heritage::math::Vec3 localWheelCenter{};
    heritage::math::Vec3 localWheelForward{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 localWheelRight{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 localWheelUp{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 localUprightRotationDegrees{};
};

bool validMultiLinkHardpoints(const MultiLinkHardpoints& hardpoints);

MultiLinkKinematicsOutput evaluateMultiLinkKinematics(
    const MultiLinkHardpoints& hardpoints,
    const MultiLinkKinematicsInput& input);

} // namespace heritage::vehicles

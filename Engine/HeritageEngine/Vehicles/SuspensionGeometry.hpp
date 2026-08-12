#pragma once

#include "SuspensionModel.hpp"
#include "Suspension/Geometry/MacPherson/MacPhersonKinematics.hpp"
#include "Suspension/Geometry/TrailingArm/TrailingArmKinematics.hpp"

#include "../Core/Math/Math.hpp"

namespace heritage::vehicles {

// Healthy upright kinematics evaluated independently from spring/damper force.
// The current provider uses authored travel curves. Future hardpoint providers
// can replace those curves while retaining the same authoritative pose output.
struct SuspensionGeometryDescription
{
    SuspensionProviderKind provider = SuspensionProviderKind::LinearRaycastV1;
    // A point on the steering axis. WheelSystem supplies the authored/reference
    // mount for virtual/legacy providers; hardpoint providers replace it with
    // their actual moving linkage point.
    heritage::math::Vec3 localSteeringAxisPoint{};
    heritage::math::Vec3 localSteeringAxis{ 0.0f, 1.0f, 0.0f };
    float staticCamberDegrees = 0.0f;
    float camberGainDegreesPerM = 0.0f;
    float camberProgressionDegreesPerM2 = 0.0f;
    float staticToeDegrees = 0.0f;
    bool casterOverrideEnabled = false;
    float staticCasterDegrees = 0.0f;
    float toeGainDegreesPerM = 0.0f;
    float toeProgressionDegreesPerM2 = 0.0f;
    MacPhersonHardpoints macPherson;
    TrailingArmHardpoints trailingArm;
};

struct SuspensionGeometryInput
{
    float compressionM = 0.0f;
    float steeringDegrees = 0.0f;
    heritage::math::Vec3 localSuspensionDirection{ 0.0f, -1.0f, 0.0f };
};

struct SuspensionGeometryOutput
{
    float camberDegrees = 0.0f;
    float toeDegrees = 0.0f;
    heritage::math::Vec3 localSteeringAxis{ 0.0f, 1.0f, 0.0f };
    bool steeringAxisPointValid = false;
    heritage::math::Vec3 localSteeringAxisPoint{};
    heritage::math::Vec3 localWheelForward{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 localWheelRight{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 localWheelUp{ 0.0f, 1.0f, 0.0f };
    // Intrinsic X-Y-Z Euler angles matching Entity.SetLocalRotation.
    heritage::math::Vec3 localUprightRotationDegrees{};
    // Hardpoint providers can move the wheel centre laterally/longitudinally
    // through travel and expose the instantaneous spring motion ratio.
    bool kinematicsValid = true;
    bool travelClamped = false;
    float bumpSteerDegrees = 0.0f;
    float strutCompressionM = 0.0f;
    float springMotionRatio = 1.0f;
    // Separate-damper and rotational-spring mechanisms expose their own
    // generalized coordinates. MacPherson keeps both ratios equal because the
    // spring/damper share the strut.
    float damperCompressionM = 0.0f;
    float damperMotionRatio = 1.0f;
    float springTwistRadians = 0.0f;
    float springAngularMotionRatioRadPerM = 0.0f;
    float referenceSpringAngularMotionRatioRadPerM = 0.0f;
    heritage::math::Vec3 localWheelCenter{};
};

SuspensionGeometryOutput evaluateSuspensionGeometry(
    const SuspensionGeometryDescription& description,
    const SuspensionGeometryInput& input);

} // namespace heritage::vehicles

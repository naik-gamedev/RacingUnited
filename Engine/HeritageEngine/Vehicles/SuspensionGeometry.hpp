#pragma once

#include "SuspensionModel.hpp"

#include "../Core/Math/Math.hpp"

namespace heritage::vehicles {

// Healthy upright kinematics evaluated independently from spring/damper force.
// The current provider uses authored travel curves. Future hardpoint providers
// can replace those curves while retaining the same authoritative pose output.
struct SuspensionGeometryDescription
{
    SuspensionProviderKind provider = SuspensionProviderKind::LinearRaycastV1;
    heritage::math::Vec3 localSteeringAxis{ 0.0f, 1.0f, 0.0f };
    float staticCamberDegrees = 0.0f;
    float camberGainDegreesPerM = 0.0f;
    float camberProgressionDegreesPerM2 = 0.0f;
    float staticToeDegrees = 0.0f;
    float toeGainDegreesPerM = 0.0f;
    float toeProgressionDegreesPerM2 = 0.0f;
};

struct SuspensionGeometryInput
{
    float compressionM = 0.0f;
    float steeringDegrees = 0.0f;
};

struct SuspensionGeometryOutput
{
    float camberDegrees = 0.0f;
    float toeDegrees = 0.0f;
    heritage::math::Vec3 localSteeringAxis{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 localWheelForward{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 localWheelRight{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 localWheelUp{ 0.0f, 1.0f, 0.0f };
    // Intrinsic X-Y-Z Euler angles matching Entity.SetLocalRotation.
    heritage::math::Vec3 localUprightRotationDegrees{};
};

SuspensionGeometryOutput evaluateSuspensionGeometry(
    const SuspensionGeometryDescription& description,
    const SuspensionGeometryInput& input);

} // namespace heritage::vehicles

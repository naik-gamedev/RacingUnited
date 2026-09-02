#pragma once

#include "../../../../Core/Math/Math.hpp"

namespace heritage::vehicles {

// SUSP11: a racing kart has no conventional wheel suspension. The complete
// chassis-local package carries both front kingpins and the rigid rear axle.
// Front steering rotates each spindle around its physical inclined kingpin,
// producing real caster/KPI jacking. Rear wheel centres remain one rigid axle.
// Vertical compliance is owned by tire radial compliance plus the existing
// chassis_torsional_mode_v1 frame mode, never by hidden coil springs.
struct KartChassisHardpoints
{
    bool authored = false;
    heritage::math::Vec3 frontLeftKingpinUpper{};
    heritage::math::Vec3 frontLeftKingpinLower{};
    heritage::math::Vec3 frontLeftWheelCenter{};
    heritage::math::Vec3 frontRightKingpinUpper{};
    heritage::math::Vec3 frontRightKingpinLower{};
    heritage::math::Vec3 frontRightWheelCenter{};
    heritage::math::Vec3 rearAxleBearingLeft{};
    heritage::math::Vec3 rearAxleBearingRight{};
    heritage::math::Vec3 rearLeftWheelCenter{};
    heritage::math::Vec3 rearRightWheelCenter{};
};

enum class KartWheelRole
{
    Unknown = 0,
    FrontLeft,
    FrontRight,
    RearLeft,
    RearRight
};

struct KartChassisKinematicsInput
{
    float compressionM = 0.0f;
    float steeringDegrees = 0.0f;
    heritage::math::Vec3 referenceWheelCenterHint{};
    float staticCamberDegrees = 0.0f;
    float staticToeDegrees = 0.0f;
};

struct KartChassisKinematicsOutput
{
    bool valid = false;
    bool travelClamped = false;
    KartWheelRole role = KartWheelRole::Unknown;
    float casterDegrees = 0.0f;
    float kingpinInclinationDegrees = 0.0f;
    float steeringJackingM = 0.0f;
    float kingpinRadialOffsetM = 0.0f;
    float camberDegrees = 0.0f;
    float toeDegrees = 0.0f;
    heritage::math::Vec3 referenceWheelCenter{};
    heritage::math::Vec3 localSteeringAxis{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 localSteeringAxisPoint{};
    heritage::math::Vec3 localWheelCenter{};
    heritage::math::Vec3 localWheelForward{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 localWheelRight{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 localWheelUp{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 localUprightRotationDegrees{};
};

bool validKartChassisHardpoints(const KartChassisHardpoints& hardpoints);

KartChassisKinematicsOutput evaluateKartChassisKinematics(
    const KartChassisHardpoints& hardpoints,
    const KartChassisKinematicsInput& input);

} // namespace heritage::vehicles

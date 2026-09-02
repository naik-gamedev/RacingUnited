#pragma once

#include "../../../../Core/Math/Math.hpp"

namespace heritage::vehicles {

// SUSP10: motorcycle rear suspension. The wheel and linkage pickup rotate on a
// rigid swingarm. A fixed-length dogbone drives a chassis-pivoted rocker and
// shock, producing an actual rising/falling-rate shaft coordinate. The
// countershaft point is retained so chain-line length variation can expose the
// physical anti-squat/jacking virtual-work ratio.
struct MotorcycleSwingarmHardpoints
{
    bool authored = false;
    heritage::math::Vec3 swingarmPivotLeft{};
    heritage::math::Vec3 swingarmPivotRight{};
    heritage::math::Vec3 wheelCenter{};
    heritage::math::Vec3 linkageSwingarmMount{};
    heritage::math::Vec3 rockerPivotLeft{};
    heritage::math::Vec3 rockerPivotRight{};
    heritage::math::Vec3 rockerLinkMount{};
    heritage::math::Vec3 shockChassisMount{};
    heritage::math::Vec3 shockRockerMount{};
    heritage::math::Vec3 countershaftCenter{};
};

struct MotorcycleSwingarmKinematicsInput
{
    float compressionM = 0.0f;
    heritage::math::Vec3 suspensionDirection{ 0.0f, -1.0f, 0.0f };
};

struct MotorcycleSwingarmKinematicsOutput
{
    bool valid = false;
    bool travelClamped = false;
    float swingarmAngleRadians = 0.0f;
    float rockerAngleRadians = 0.0f;
    float shockCompressionM = 0.0f;
    float shockMotionRatio = 1.0f;
    float dogboneLengthErrorM = 0.0f;
    // d(countershaft-to-axle distance)/d(wheel compression). Combined with
    // wheel/rear-sprocket radii this becomes the chain anti-squat force ratio.
    float chainCenterDistanceMotionRatio = 0.0f;
    float wheelbaseDeltaM = 0.0f;
    heritage::math::Vec3 localWheelCenter{};
    heritage::math::Vec3 localLinkageSwingarmMount{};
    heritage::math::Vec3 localRockerLinkMount{};
    heritage::math::Vec3 localShockRockerMount{};
    heritage::math::Vec3 localWheelForward{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 localWheelRight{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 localWheelUp{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 localUprightRotationDegrees{};
};

bool validMotorcycleSwingarmHardpoints(const MotorcycleSwingarmHardpoints& hardpoints);

MotorcycleSwingarmKinematicsOutput evaluateMotorcycleSwingarmKinematics(
    const MotorcycleSwingarmHardpoints& hardpoints,
    const MotorcycleSwingarmKinematicsInput& input);

} // namespace heritage::vehicles

#pragma once

#include "../../../Core/Math/Math.hpp"
#include "../../VehiclePrecision.hpp"

namespace heritage::vehicles {

// FITMENT01: immutable/reference component geometry plus the current installed
// wheel setup. Suspension pickup points remain chassis data; changing ET or a
// spacer moves the tire/wheel centerline relative to that reference geometry.
struct WheelFitmentDescription
{
    bool enabled = false;
    float referenceOffsetEtMm = 0.0f;
    float installedOffsetEtMm = 0.0f;
    float spacerThicknessMm = 0.0f;
    float rimDiameterIn = 0.0f;
    float rimWidthIn = 0.0f;
    float tireWidthMm = 0.0f;
    float tireAspectRatio = 0.0f;
    float tireRimDiameterIn = 0.0f;
};

struct WheelFitmentResolved
{
    bool valid = false;
    VehicleScalar outwardCenterlineDeltaM = 0.0;
    VehicleScalar nominalTireRadiusM = 0.0;
};

// Human-facing alignment setup. Camber and toe use symmetric vehicle setup
// conventions in Lua; by the time they reach native code they are already the
// signed per-corner local values consumed by suspension kinematics.
struct WheelAlignmentSetup
{
    float camberDegrees = 0.0f;
    float toeDegrees = 0.0f;
    bool casterOverrideEnabled = false;
    float casterDegrees = 0.0f;
};

bool validWheelFitmentDescription(const WheelFitmentDescription& value);
WheelFitmentResolved resolveWheelFitment(const WheelFitmentDescription& value);
heritage::math::Vec3 wheelCenterlineOffsetLocal(
    const heritage::math::Vec3& referenceSuspensionMount,
    const WheelFitmentDescription& fitment);
bool validWheelAlignmentSetup(const WheelAlignmentSetup& value);

} // namespace heritage::vehicles

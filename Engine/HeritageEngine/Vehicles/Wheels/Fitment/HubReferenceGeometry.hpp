#pragma once

#include "WheelFitment.hpp"

#include "../../../Core/Math/Math.hpp"
#include "../../VehiclePrecision.hpp"

namespace heritage::vehicles {

// FITMENT02: explicit reference datums for a wheel installation. The authored
// reference wheel center stays immutable. Positive wheel ET places the hub
// mounting face outward from the wheel centerline. A spacer moves only the
// installed mounting plane; it never relocates suspension hardpoints.
struct WheelHubReferenceGeometry
{
    bool valid = false;
    heritage::math::Vec3 outwardAxisLocal{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 referenceWheelCenterLocal{};
    heritage::math::Vec3 referenceHubFaceCenterLocal{};
    heritage::math::Vec3 installedMountFaceCenterLocal{};
    heritage::math::Vec3 installedWheelCenterLocal{};
    heritage::math::Vec3 installedInnerTirePlaneLocal{};
    heritage::math::Vec3 installedOuterTirePlaneLocal{};
    VehicleScalar referenceOffsetEtM = 0.0;
    VehicleScalar installedOffsetEtM = 0.0;
    VehicleScalar spacerThicknessM = 0.0;
    VehicleScalar tireHalfWidthM = 0.0;
    // Positive distances measured from the chassis-side reference hub face.
    VehicleScalar inboardTireExtensionFromReferenceHubM = 0.0;
    VehicleScalar outboardTireExtensionFromReferenceHubM = 0.0;
};

heritage::math::Vec3 wheelOutwardAxisFromReferenceCenter(
    const heritage::math::Vec3& referenceWheelCenterLocal);

WheelHubReferenceGeometry resolveWheelHubReferenceGeometry(
    const heritage::math::Vec3& referenceWheelCenterLocal,
    const heritage::math::Vec3& outwardAxisLocal,
    const WheelFitmentDescription& fitment);

WheelHubReferenceGeometry resolveWheelHubReferenceGeometry(
    const heritage::math::Vec3& referenceWheelCenterLocal,
    const WheelFitmentDescription& fitment);

} // namespace heritage::vehicles

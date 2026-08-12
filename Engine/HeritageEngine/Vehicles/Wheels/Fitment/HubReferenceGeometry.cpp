#include "HubReferenceGeometry.hpp"

#include <cmath>

namespace heritage::vehicles {
namespace {

constexpr VehicleScalar kMillimetersPerMeter = 1000.0;
constexpr VehicleScalar kVectorEpsilon = 1.0e-12;

VehicleScalar dot(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return static_cast<VehicleScalar>(left.x) * right.x
        + static_cast<VehicleScalar>(left.y) * right.y
        + static_cast<VehicleScalar>(left.z) * right.z;
}

VehicleScalar lengthSquared(const heritage::math::Vec3& value)
{
    return dot(value, value);
}

heritage::math::Vec3 scale(
    const heritage::math::Vec3& value,
    VehicleScalar scalar)
{
    return {
        static_cast<float>(static_cast<VehicleScalar>(value.x) * scalar),
        static_cast<float>(static_cast<VehicleScalar>(value.y) * scalar),
        static_cast<float>(static_cast<VehicleScalar>(value.z) * scalar) };
}

heritage::math::Vec3 add(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return { left.x + right.x, left.y + right.y, left.z + right.z };
}

heritage::math::Vec3 subtract(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return { left.x - right.x, left.y - right.y, left.z - right.z };
}

heritage::math::Vec3 normalized(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& fallback)
{
    const VehicleScalar squared = lengthSquared(value);
    if (!std::isfinite(squared) || squared <= kVectorEpsilon)
        return fallback;
    return scale(value, 1.0 / std::sqrt(squared));
}

bool finite(const heritage::math::Vec3& value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

} // namespace

heritage::math::Vec3 wheelOutwardAxisFromReferenceCenter(
    const heritage::math::Vec3& referenceWheelCenterLocal)
{
    // Heritage native vehicle coordinates are +X right. This convenience path
    // is appropriate for ordinary left/right road wheels. Centerline vehicles
    // (motorcycles, single-track prototypes) should provide an explicit axis.
    if (referenceWheelCenterLocal.x < -1.0e-5f)
        return { -1.0f, 0.0f, 0.0f };
    return { 1.0f, 0.0f, 0.0f };
}

WheelHubReferenceGeometry resolveWheelHubReferenceGeometry(
    const heritage::math::Vec3& referenceWheelCenterLocal,
    const heritage::math::Vec3& outwardAxisLocal,
    const WheelFitmentDescription& fitment)
{
    WheelHubReferenceGeometry result;
    if (!finite(referenceWheelCenterLocal)
        || !finite(outwardAxisLocal)
        || !validWheelFitmentDescription(fitment))
    {
        return result;
    }

    const heritage::math::Vec3 outward = normalized(
        outwardAxisLocal,
        wheelOutwardAxisFromReferenceCenter(referenceWheelCenterLocal));
    if (lengthSquared(outward) <= kVectorEpsilon)
        return result;

    result.outwardAxisLocal = outward;
    result.referenceWheelCenterLocal = referenceWheelCenterLocal;

    if (!fitment.enabled)
    {
        result.referenceHubFaceCenterLocal = referenceWheelCenterLocal;
        result.installedMountFaceCenterLocal = referenceWheelCenterLocal;
        result.installedWheelCenterLocal = referenceWheelCenterLocal;
        result.installedInnerTirePlaneLocal = referenceWheelCenterLocal;
        result.installedOuterTirePlaneLocal = referenceWheelCenterLocal;
        result.valid = true;
        return result;
    }

    result.referenceOffsetEtM =
        static_cast<VehicleScalar>(fitment.referenceOffsetEtMm)
        / kMillimetersPerMeter;
    result.installedOffsetEtM =
        static_cast<VehicleScalar>(fitment.installedOffsetEtMm)
        / kMillimetersPerMeter;
    result.spacerThicknessM =
        static_cast<VehicleScalar>(fitment.spacerThicknessMm)
        / kMillimetersPerMeter;
    result.tireHalfWidthM =
        static_cast<VehicleScalar>(fitment.tireWidthMm)
        / (2.0 * kMillimetersPerMeter);

    // Positive ET means the mounting face is toward the outside of the vehicle
    // relative to the wheel centerline.
    result.referenceHubFaceCenterLocal = add(
        referenceWheelCenterLocal,
        scale(outward, result.referenceOffsetEtM));

    // A spacer moves the wheel-side mounting plane outward from the chassis hub.
    result.installedMountFaceCenterLocal = add(
        result.referenceHubFaceCenterLocal,
        scale(outward, result.spacerThicknessM));
    result.installedWheelCenterLocal = subtract(
        result.installedMountFaceCenterLocal,
        scale(outward, result.installedOffsetEtM));

    result.installedInnerTirePlaneLocal = subtract(
        result.installedWheelCenterLocal,
        scale(outward, result.tireHalfWidthM));
    result.installedOuterTirePlaneLocal = add(
        result.installedWheelCenterLocal,
        scale(outward, result.tireHalfWidthM));

    result.inboardTireExtensionFromReferenceHubM = -dot(
        subtract(
            result.installedInnerTirePlaneLocal,
            result.referenceHubFaceCenterLocal),
        outward);
    result.outboardTireExtensionFromReferenceHubM = dot(
        subtract(
            result.installedOuterTirePlaneLocal,
            result.referenceHubFaceCenterLocal),
        outward);

    result.valid = finite(result.referenceHubFaceCenterLocal)
        && finite(result.installedMountFaceCenterLocal)
        && finite(result.installedWheelCenterLocal)
        && finite(result.installedInnerTirePlaneLocal)
        && finite(result.installedOuterTirePlaneLocal)
        && std::isfinite(result.inboardTireExtensionFromReferenceHubM)
        && std::isfinite(result.outboardTireExtensionFromReferenceHubM);
    return result;
}

WheelHubReferenceGeometry resolveWheelHubReferenceGeometry(
    const heritage::math::Vec3& referenceWheelCenterLocal,
    const WheelFitmentDescription& fitment)
{
    return resolveWheelHubReferenceGeometry(
        referenceWheelCenterLocal,
        wheelOutwardAxisFromReferenceCenter(referenceWheelCenterLocal),
        fitment);
}

} // namespace heritage::vehicles

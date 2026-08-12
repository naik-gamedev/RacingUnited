#include "WheelFitment.hpp"
#include "HubReferenceGeometry.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {

constexpr VehicleScalar kMillimetersPerMeter = 1000.0;
constexpr VehicleScalar kMillimetersPerInch = 25.4;

bool finite(float value)
{
    return std::isfinite(value);
}

} // namespace

bool validWheelFitmentDescription(const WheelFitmentDescription& value)
{
    if (!value.enabled)
        return true;

    return finite(value.referenceOffsetEtMm)
        && value.referenceOffsetEtMm >= -250.0f
        && value.referenceOffsetEtMm <= 250.0f
        && finite(value.installedOffsetEtMm)
        && value.installedOffsetEtMm >= -250.0f
        && value.installedOffsetEtMm <= 250.0f
        && finite(value.spacerThicknessMm)
        && value.spacerThicknessMm >= 0.0f
        && value.spacerThicknessMm <= 200.0f
        && finite(value.rimDiameterIn)
        && value.rimDiameterIn >= 4.0f
        && value.rimDiameterIn <= 60.0f
        && finite(value.rimWidthIn)
        && value.rimWidthIn >= 2.0f
        && value.rimWidthIn <= 30.0f
        && finite(value.tireWidthMm)
        && value.tireWidthMm >= 50.0f
        && value.tireWidthMm <= 1000.0f
        && finite(value.tireAspectRatio)
        && value.tireAspectRatio >= 10.0f
        && value.tireAspectRatio <= 100.0f
        && finite(value.tireRimDiameterIn)
        && value.tireRimDiameterIn >= 4.0f
        && value.tireRimDiameterIn <= 60.0f
        && std::abs(value.rimDiameterIn - value.tireRimDiameterIn) <= 0.05f;
}

WheelFitmentResolved resolveWheelFitment(const WheelFitmentDescription& value)
{
    WheelFitmentResolved result;
    if (!validWheelFitmentDescription(value))
        return result;
    if (!value.enabled)
    {
        result.valid = true;
        return result;
    }

    result.outwardCenterlineDeltaM = (
        static_cast<VehicleScalar>(value.referenceOffsetEtMm)
        - static_cast<VehicleScalar>(value.installedOffsetEtMm)
        + static_cast<VehicleScalar>(value.spacerThicknessMm))
        / kMillimetersPerMeter;

    const VehicleScalar sidewallMm =
        static_cast<VehicleScalar>(value.tireWidthMm)
        * (static_cast<VehicleScalar>(value.tireAspectRatio) / 100.0);
    const VehicleScalar rimMm =
        static_cast<VehicleScalar>(value.tireRimDiameterIn)
        * kMillimetersPerInch;
    result.nominalTireRadiusM =
        (rimMm + sidewallMm * 2.0) / (2.0 * kMillimetersPerMeter);
    result.valid = result.nominalTireRadiusM > 0.01
        && result.nominalTireRadiusM <= 5.0;
    return result;
}

heritage::math::Vec3 wheelCenterlineOffsetLocal(
    const heritage::math::Vec3& referenceSuspensionMount,
    const WheelFitmentDescription& fitment)
{
    const WheelHubReferenceGeometry hub = resolveWheelHubReferenceGeometry(
        referenceSuspensionMount,
        fitment);
    if (!hub.valid || !fitment.enabled)
        return {};

    // FITMENT02 makes the mounting-face/centerline relationship the single
    // geometric source of truth. The returned vector is still downstream of
    // the upright/hub and never relocates suspension or steering hardpoints.
    return {
        hub.installedWheelCenterLocal.x - hub.referenceWheelCenterLocal.x,
        hub.installedWheelCenterLocal.y - hub.referenceWheelCenterLocal.y,
        hub.installedWheelCenterLocal.z - hub.referenceWheelCenterLocal.z };
}

bool validWheelAlignmentSetup(const WheelAlignmentSetup& value)
{
    return finite(value.camberDegrees)
        && std::abs(value.camberDegrees) <= 45.0f
        && finite(value.toeDegrees)
        && std::abs(value.toeDegrees) <= 20.0f
        && finite(value.casterDegrees)
        && value.casterDegrees >= -20.0f
        && value.casterDegrees <= 30.0f;
}

} // namespace heritage::vehicles

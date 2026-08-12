#include "PhysicsRegressionCommon.hpp"
#include "../Vehicles/Wheels/Fitment/WheelFitment.hpp"
#include "../Vehicles/Wheels/Fitment/HubReferenceGeometry.hpp"
#include "../Vehicles/Wheels/Fitment/ScrubRadiusGeometry.hpp"

#include <cmath>
#include <iostream>

namespace heritage::tests {
namespace {

bool nearlyEqual(double a, double b, double tolerance = 1.0e-5)
{
    return std::abs(a - b) <= tolerance;
}

} // namespace

bool wheelFitmentAndAlignmentAreReferenceSafe()
{
    heritage::vehicles::WheelFitmentDescription stock;
    stock.enabled = true;
    stock.referenceOffsetEtMm = 28.0f;
    stock.installedOffsetEtMm = 28.0f;
    stock.spacerThicknessMm = 0.0f;
    stock.rimDiameterIn = 17.0f;
    stock.rimWidthIn = 7.0f;
    stock.tireWidthMm = 205.0f;
    stock.tireAspectRatio = 40.0f;
    stock.tireRimDiameterIn = 17.0f;

    const auto stockResolved = heritage::vehicles::resolveWheelFitment(stock);
    const Vec3 leftReference{ -0.7185f, 0.85f, 1.221f };
    const Vec3 rightReference{ 0.7185f, 0.85f, 1.221f };
    const Vec3 leftStockOffset = heritage::vehicles::wheelCenterlineOffsetLocal(
        leftReference, stock);
    const Vec3 rightStockOffset = heritage::vehicles::wheelCenterlineOffsetLocal(
        rightReference, stock);

    heritage::vehicles::WheelFitmentDescription modified = stock;
    modified.installedOffsetEtMm = 15.0f;
    modified.spacerThicknessMm = 10.0f;
    const auto modifiedResolved = heritage::vehicles::resolveWheelFitment(modified);
    const Vec3 leftModifiedOffset = heritage::vehicles::wheelCenterlineOffsetLocal(
        leftReference, modified);
    const Vec3 rightModifiedOffset = heritage::vehicles::wheelCenterlineOffsetLocal(
        rightReference, modified);
    const auto stockHub = heritage::vehicles::resolveWheelHubReferenceGeometry(
        rightReference, stock);
    const auto modifiedHub = heritage::vehicles::resolveWheelHubReferenceGeometry(
        rightReference, modified);

    const auto syntheticSteeringGeometry =
        heritage::vehicles::evaluateSteeringGroundGeometry(
            { 0.700f, 0.500f, 1.200f },
            { 0.0f, 1.0f, 0.0f },
            { 0.723f, 0.0f, 1.180f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f },
            { 1.0f, 0.0f, 0.0f });

    heritage::vehicles::WheelFitmentDescription invalid = modified;
    invalid.tireRimDiameterIn = 18.0f;

    heritage::vehicles::WheelAlignmentSetup alignment;
    alignment.camberDegrees = -2.5f;
    alignment.toeDegrees = -0.12f;
    alignment.casterOverrideEnabled = true;
    alignment.casterDegrees = 5.5f;

    heritage::vehicles::WheelAlignmentSetup fineAlignment;
    fineAlignment.camberDegrees = 0.82f;
    fineAlignment.toeDegrees = -0.037f;
    fineAlignment.casterOverrideEnabled = true;
    fineAlignment.casterDegrees = 3.2f;

    heritage::vehicles::WheelAlignmentSetup extremePositive = fineAlignment;
    extremePositive.camberDegrees = 44.999f;
    heritage::vehicles::WheelAlignmentSetup extremeNegative = fineAlignment;
    extremeNegative.camberDegrees = -44.999f;
    heritage::vehicles::WheelAlignmentSetup invalidExtreme = fineAlignment;
    invalidExtreme.camberDegrees = 45.01f;

    heritage::vehicles::SuspensionGeometryDescription geometry;
    geometry.provider = heritage::vehicles::SuspensionProviderKind::LinearRaycastV1;
    geometry.localSteeringAxis = { 0.0f, 1.0f, 0.0f };
    geometry.staticCamberDegrees = alignment.camberDegrees;
    geometry.staticToeDegrees = alignment.toeDegrees;
    geometry.casterOverrideEnabled = true;
    geometry.staticCasterDegrees = alignment.casterDegrees;
    const auto upright = heritage::vehicles::evaluateSuspensionGeometry(
        geometry,
        { 0.0f, 12.0f, { 0.0f, -1.0f, 0.0f } });

    PrototypeWorld world;
    bool integrationPassed = createPrototypeWorld(world, 1000.0f);
    heritage::vehicles::SteeringState referenceSteering;
    heritage::vehicles::SteeringState modifiedSteering;
    WheelState referenceLeft;
    WheelState referenceRight;
    WheelState modifiedLeft;
    WheelState modifiedRight;
    heritage::vehicles::WheelFitmentGeometryState referenceLeftGeometry;
    heritage::vehicles::WheelFitmentGeometryState referenceRightGeometry;
    heritage::vehicles::WheelFitmentGeometryState modifiedLeftGeometry;
    heritage::vehicles::WheelFitmentGeometryState modifiedRightGeometry;
    if (integrationPassed)
    {
        integrationPassed = world.vehicles.setWheelFitment(
            world.vehicle, 0, stock)
            && world.vehicles.setWheelFitment(world.vehicle, 1, stock);
    }
    if (integrationPassed)
    {
        for (int step = 0; step < 8; ++step)
            stepWorld(world);
        integrationPassed = world.vehicles.steeringState(
            world.vehicle, referenceSteering)
            && world.vehicles.wheelState(world.vehicle, 0, referenceLeft)
            && world.vehicles.wheelState(world.vehicle, 1, referenceRight)
            && world.vehicles.wheelFitmentGeometry(
                world.vehicle, 0, referenceLeftGeometry)
            && world.vehicles.wheelFitmentGeometry(
                world.vehicle, 1, referenceRightGeometry);
    }
    if (integrationPassed)
    {
        integrationPassed = world.vehicles.setWheelFitment(
            world.vehicle, 0, modified)
            && world.vehicles.setWheelFitment(world.vehicle, 1, modified);
    }
    if (integrationPassed)
    {
        for (int step = 0; step < 8; ++step)
            stepWorld(world);
        integrationPassed = world.vehicles.steeringState(
            world.vehicle, modifiedSteering)
            && world.vehicles.wheelState(world.vehicle, 0, modifiedLeft)
            && world.vehicles.wheelState(world.vehicle, 1, modifiedRight)
            && world.vehicles.wheelFitmentGeometry(
                world.vehicle, 0, modifiedLeftGeometry)
            && world.vehicles.wheelFitmentGeometry(
                world.vehicle, 1, modifiedRightGeometry);
    }
    const double referenceVisualTrack =
        referenceRight.worldCenter.x - referenceLeft.worldCenter.x;
    const double modifiedVisualTrack =
        modifiedRight.worldCenter.x - modifiedLeft.worldCenter.x;
    integrationPassed = integrationPassed
        && nearlyEqual(
            referenceSteering.detectedSteerTrack,
            modifiedSteering.detectedSteerTrack,
            1.0e-5)
        && nearlyEqual(
            modifiedVisualTrack - referenceVisualTrack,
            0.046,
            2.0e-3)
        && referenceLeftGeometry.steeringGroundGeometryValid
        && referenceRightGeometry.steeringGroundGeometryValid
        && modifiedLeftGeometry.steeringGroundGeometryValid
        && modifiedRightGeometry.steeringGroundGeometryValid
        && nearlyEqual(referenceLeftGeometry.signedScrubRadiusM, 0.0, 2.0e-3)
        && nearlyEqual(referenceRightGeometry.signedScrubRadiusM, 0.0, 2.0e-3)
        && nearlyEqual(
            modifiedLeftGeometry.signedScrubRadiusM
                - referenceLeftGeometry.signedScrubRadiusM,
            -0.023,
            2.0e-3)
        && nearlyEqual(
            modifiedRightGeometry.signedScrubRadiusM
                - referenceRightGeometry.signedScrubRadiusM,
            0.023,
            2.0e-3);

    const double nominalRadius = (17.0 * 25.4 + 2.0 * 205.0 * 0.40)
        / 2000.0;
    const bool passed = stockResolved.valid
        && nearlyEqual(stockResolved.outwardCenterlineDeltaM, 0.0)
        && nearlyEqual(stockResolved.nominalTireRadiusM, nominalRadius)
        && nearlyEqual(leftStockOffset.x, 0.0)
        && nearlyEqual(rightStockOffset.x, 0.0)
        && modifiedResolved.valid
        && nearlyEqual(modifiedResolved.outwardCenterlineDeltaM, 0.023)
        && nearlyEqual(leftModifiedOffset.x, -0.023, 1.0e-4)
        && nearlyEqual(rightModifiedOffset.x, 0.023, 1.0e-4)
        && nearlyEqual(leftModifiedOffset.y, 0.0)
        && nearlyEqual(rightModifiedOffset.y, 0.0)
        && nearlyEqual(leftModifiedOffset.z, 0.0)
        && nearlyEqual(rightModifiedOffset.z, 0.0)
        && stockHub.valid
        && modifiedHub.valid
        && nearlyEqual(stockHub.referenceHubFaceCenterLocal.x, 0.7465, 1.0e-4)
        && nearlyEqual(stockHub.installedWheelCenterLocal.x, 0.7185, 1.0e-4)
        && nearlyEqual(modifiedHub.installedMountFaceCenterLocal.x, 0.7565, 1.0e-4)
        && nearlyEqual(modifiedHub.installedWheelCenterLocal.x, 0.7415, 1.0e-4)
        && nearlyEqual(
            modifiedHub.inboardTireExtensionFromReferenceHubM,
            0.1075,
            1.0e-4)
        && nearlyEqual(
            modifiedHub.outboardTireExtensionFromReferenceHubM,
            0.0975,
            1.0e-4)
        && syntheticSteeringGeometry.valid
        && nearlyEqual(syntheticSteeringGeometry.signedScrubRadiusM, 0.023, 1.0e-6)
        && nearlyEqual(syntheticSteeringGeometry.mechanicalTrailM, 0.020, 1.0e-6)
        && !heritage::vehicles::validWheelFitmentDescription(invalid)
        && heritage::vehicles::validWheelAlignmentSetup(alignment)
        && heritage::vehicles::validWheelAlignmentSetup(fineAlignment)
        && heritage::vehicles::validWheelAlignmentSetup(extremePositive)
        && heritage::vehicles::validWheelAlignmentSetup(extremeNegative)
        && !heritage::vehicles::validWheelAlignmentSetup(invalidExtreme)
        && nearlyEqual(fineAlignment.camberDegrees, 0.82, 1.0e-6)
        && upright.kinematicsValid
        && nearlyEqual(upright.camberDegrees, -2.5, 1.0e-4)
        && nearlyEqual(upright.toeDegrees, -0.12, 1.0e-4)
        && upright.localSteeringAxis.z < -0.05f
        && integrationPassed;

    std::cout
        << "fitment stock_radius_m=" << stockResolved.nominalTireRadiusM
        << " outward_delta_m=" << modifiedResolved.outwardCenterlineDeltaM
        << " left_centerline_delta_m=" << leftModifiedOffset.x
        << " right_centerline_delta_m=" << rightModifiedOffset.x
        << " caster_axis_z=" << upright.localSteeringAxis.z
        << " steering_track_m=" << modifiedSteering.detectedSteerTrack
        << " installed_track_delta_m="
        << (modifiedVisualTrack - referenceVisualTrack)
        << " hub_face_x_m=" << modifiedHub.referenceHubFaceCenterLocal.x
        << " inboard_extent_mm="
        << modifiedHub.inboardTireExtensionFromReferenceHubM * 1000.0
        << " outboard_extent_mm="
        << modifiedHub.outboardTireExtensionFromReferenceHubM * 1000.0
        << " left_scrub_delta_mm="
        << (modifiedLeftGeometry.signedScrubRadiusM
            - referenceLeftGeometry.signedScrubRadiusM) * 1000.0
        << " right_scrub_delta_mm="
        << (modifiedRightGeometry.signedScrubRadiusM
            - referenceRightGeometry.signedScrubRadiusM) * 1000.0
        << '\n';
    return passed;
}

} // namespace heritage::tests

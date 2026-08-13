#include "LuaEntityTireFlexibleRingBridge.hpp"

#include "../../../../Vehicles/Tires/TireFlexibleRingField.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::modules::lua_binding_detail {

TireFlexibleRingPresentationField solveTireFlexibleRingPresentationField(
    const heritage::vehicles::WheelState& wheelState,
    const heritage::vehicles::TireModelDescription* tireModel,
    float tireRadiusM,
    float rimRadiusM,
    float tireHalfWidthM,
    float maximumCompressionM,
    const std::array<float, heritage::entities::TireVisualContactSampleCount>&
        directContactCompressionM,
    const std::array<float, heritage::entities::TireVisualContactSampleCount>&
        directContactForwardDisplacementM,
    const std::array<float, heritage::entities::TireVisualContactSampleCount>&
        directContactDownDisplacementM,
    const std::array<float, heritage::entities::TireVisualContactSampleCount>&
        directContactLateralDisplacementM)
{
    static_assert(
        heritage::vehicles::tires::TireFlexibleRingContactCount
            == heritage::entities::TireVisualContactSampleCount);
    static_assert(
        heritage::vehicles::tires::TireFlexibleRingFieldCount
            == heritage::entities::TireVisualDeformationFieldCount);

    heritage::vehicles::tires::TireFlexibleRingFieldDescription description;
    description.unloadedRadiusM = tireRadiusM;
    description.rimRadiusM = std::clamp(
        static_cast<double>(rimRadiusM),
        0.015,
        static_cast<double>(tireRadiusM - 0.005f));
    description.sectionWidthM = tireHalfWidthM * 2.0;
    description.maximumDeflectionM = maximumCompressionM;
    description.referencePressurePa = tireModel
        ? (tireModel->thermal.referenceGaugePressurePa > 20000.0
            ? tireModel->thermal.referenceGaugePressurePa
            : tireModel->inflationPressurePa)
        : 220000.0;
    description.verticalStiffnessNPerM = tireModel
        && tireModel->contactGeometry.verticalStiffnessNPerM > 1000.0
        ? tireModel->contactGeometry.verticalStiffnessNPerM
        : 220000.0;

    heritage::vehicles::tires::TireFlexibleRingFieldInput input;
    input.grounded = wheelState.grounded;
    input.verticalDeflectionM = wheelState.tireDeflection;
    input.contactPatchLengthM = wheelState.tireContactPatchLength;
    input.contactPatchWidthM = wheelState.tireContactPatchWidth;
    input.normalLoadN = wheelState.normalForce;
    input.inflationPressurePa = wheelState.tireInflationPressurePa;
    input.ringLongitudinalOffsetM = wheelState.tireRingLongitudinalOffset;
    input.ringLateralOffsetM = wheelState.tireRingLateralOffset;
    input.ringYawRadians = wheelState.tireRingYawDegrees
        * 3.14159265358979323846 / 180.0;
    input.ringWindupRadians = wheelState.tireRingWindupDegrees
        * 3.14159265358979323846 / 180.0;
    input.flatSpotDepthM = wheelState.tireFlatSpotDepthMm * 0.001;
    input.flatSpotSector = wheelState.tireFlatSpotSector;
    input.wheelRotationRadians = wheelState.wheelRotationDegrees
        * 3.14159265358979323846 / 180.0;
    for (std::size_t index = 0; index < directContactCompressionM.size(); ++index)
    {
        input.directContactCompressionM[index] = directContactCompressionM[index];
        input.directContactForwardDisplacementM[index] =
            directContactForwardDisplacementM[index];
        input.directContactDownDisplacementM[index] =
            directContactDownDisplacementM[index];
        input.directContactLateralDisplacementM[index] =
            directContactLateralDisplacementM[index];
    }

    const auto native = heritage::vehicles::tires::evaluateTireFlexibleRingField(
        description, input);
    TireFlexibleRingPresentationField output;
    output.valid = native.valid;
    for (std::size_t index = 0; index < output.forwardDisplacementM.size(); ++index)
    {
        output.forwardDisplacementM[index] = static_cast<float>(
            native.forwardDisplacementM[index]);
        output.downDisplacementM[index] = static_cast<float>(
            native.downDisplacementM[index]);
        output.lateralDisplacementM[index] = static_cast<float>(
            native.lateralDisplacementM[index]);
    }
    return output;
}

} // namespace heritage::modules::lua_binding_detail

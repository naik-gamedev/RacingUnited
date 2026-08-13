#pragma once

#include "../../../Entities/EntityRegistry.hpp"
#include "../../../../Vehicles/VehicleSystem.hpp"

#include <array>

namespace heritage::modules::lua_binding_detail {

struct TireFlexibleRingPresentationField
{
    bool valid = false;
    std::array<float, heritage::entities::TireVisualDeformationFieldCount>
        forwardDisplacementM{};
    std::array<float, heritage::entities::TireVisualDeformationFieldCount>
        downDisplacementM{};
    std::array<float, heritage::entities::TireVisualDeformationFieldCount>
        lateralDisplacementM{};
};

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
        directContactLateralDisplacementM);

} // namespace heritage::modules::lua_binding_detail

#pragma once

#include "../../../VehiclePrecision.hpp"

namespace heritage::vehicles {

struct LeafSpringAxleWrapDescription
{
    VehicleScalar stiffnessNmPerRad = 16000.0;
    VehicleScalar dampingNmsPerRad = 1200.0;
    VehicleScalar inertiaKgM2 = 5.0;
    VehicleScalar maximumAngleRadians = 0.22;
};

struct LeafSpringAxleWrapState
{
    VehicleScalar angleRadians = 0.0;
    VehicleScalar rateRadiansPerSecond = 0.0;
};

struct LeafSpringAxleWrapInput
{
    VehicleScalar reactionTorqueNm = 0.0;
    VehicleScalar deltaTimeSeconds = 0.001;
};

bool validLeafSpringAxleWrapDescription(
    const LeafSpringAxleWrapDescription& description);

LeafSpringAxleWrapState advanceLeafSpringAxleWrap(
    const LeafSpringAxleWrapDescription& description,
    const LeafSpringAxleWrapState& state,
    const LeafSpringAxleWrapInput& input);

} // namespace heritage::vehicles

#pragma once

#include "VehicleMassPropertiesEstimator.hpp"

#include <string>
#include <vector>

namespace heritage::vehicles {

// Reusable installed-component mass primitive. The inertia is the component's
// diagonal inertia about its own COM, expressed in vehicle-local axes.
//
// MASS01 intentionally keeps the runtime rigid body on a diagonal tensor. A
// future full-tensor provider can extend this contract without changing how
// wheels, bumpers, batteries, wings, cages, cargo, etc. declare their mass.
struct VehicleMassComponent
{
    std::string stableId;
    VehicleScalar massKg = 0.0;
    heritage::math::Vec3 centerLocal{};
    heritage::math::Vec3 inertiaAtCenterKgM2{};
};

struct VehicleAccumulatedMassProperties
{
    bool valid = false;
    VehicleScalar totalMassKg = 0.0;
    heritage::math::Vec3 centerOfMassLocal{};
    heritage::math::Vec3 inertiaLocalKgM2{};
};

VehicleAccumulatedMassProperties accumulateVehicleMassProperties(
    const std::vector<VehicleMassComponent>& components);

} // namespace heritage::vehicles

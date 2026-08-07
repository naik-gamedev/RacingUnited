#pragma once

#include <string>

#include "VehicleDefinition.hpp"
#include "VehicleSystem.hpp"

namespace heritage::vehicles {

struct VehicleDefinitionLoadSettings
{
    VehicleDescription vehicle;
};

// Adapter from an immutable compiled topology into a particular native solver.
// Step 29K supports raycast_wheel_v1; future providers plug in beside it rather
// than adding vehicle-category branches here.
class VehicleDefinitionLoader
{
public:
    static VehicleHandle create(
        const CompiledVehicleDefinition& definition,
        const VehicleDefinitionLoadSettings& settings,
        const heritage::physics::RigidBodySystem& bodies,
        VehicleSystem& vehicles,
        std::string& errorMessage);
};

} // namespace heritage::vehicles

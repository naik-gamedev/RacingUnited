#pragma once

#include "../../VehiclePrecision.hpp"
#include "../../../Core/Math/Math.hpp"

#include <string>

namespace heritage::vehicles {

enum class VehicleMassClass
{
    Unknown = 0,
    RoadCar,
    Formula,
    Kart,
    Atv,
    Motorcycle,
    Truck
};

const char* vehicleMassClassId(VehicleMassClass value);
bool parseVehicleMassClass(const std::string& value, VehicleMassClass& result);

// MASS01 deliberately estimates only the rigid-body properties that the solver
// actually consumes. Values remain replaceable by measured/CAD/component data.
struct VehicleMassPropertiesEstimateInput
{
    VehicleScalar totalMassKg = 1200.0;
    VehicleScalar wheelbaseM = 2.50;
    VehicleScalar frontTrackM = 1.50;
    VehicleScalar rearTrackM = 1.50;
    VehicleScalar centerOfMassHeightM = 0.50;
    VehicleScalar frontStaticLoadFraction = 0.50;
    VehicleScalar leftStaticLoadFraction = 0.50;
    VehicleMassClass massClass = VehicleMassClass::Unknown;
};

struct VehicleMassPropertiesEstimate
{
    bool valid = false;
    VehicleScalar totalMassKg = 0.0;
    heritage::math::Vec3 centerOfMassLocal{};
    // Heritage local axes: X right, Y up, Z forward. Therefore the diagonal
    // entries correspond to pitch inertia (about X), yaw (about Y), roll (Z).
    heritage::math::Vec3 inertiaLocalKgM2{};
    VehicleScalar frontStaticMassKg = 0.0;
    VehicleScalar rearStaticMassKg = 0.0;
    VehicleScalar leftStaticMassKg = 0.0;
    VehicleScalar rightStaticMassKg = 0.0;
    std::string provenance;
    VehicleScalar confidence = 0.0;
};

VehicleMassPropertiesEstimate estimateVehicleMassProperties(
    const VehicleMassPropertiesEstimateInput& input);

} // namespace heritage::vehicles

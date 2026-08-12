#pragma once

#include "ChassisTorsionalCompliance.hpp"

#include <string>

namespace heritage::vehicles {

enum class ChassisConstructionKind
{
    Unknown = 0,
    ClosedUnibody,
    OpenUnibody,
    LadderFrame,
    SpaceFrame,
    CarbonMonocoque
};

const char* chassisConstructionKindId(ChassisConstructionKind value);
bool parseChassisConstructionKind(
    const std::string& value,
    ChassisConstructionKind& result);

struct ChassisFlexEstimateInput
{
    VehicleScalar massKg = 1200.0;
    VehicleScalar wheelbaseM = 2.50;
    VehicleScalar frontTrackM = 1.50;
    VehicleScalar rearTrackM = 1.50;
    VehicleScalar centerOfMassHeightM = 0.50;
    int modelYear = 2000;
    ChassisConstructionKind construction = ChassisConstructionKind::Unknown;
};

struct ChassisFlexEstimate
{
    bool valid = false;
    ChassisTorsionalComplianceDescription description;
    std::string provenance;
    VehicleScalar confidence = 0.0;
};

ChassisFlexEstimate estimateChassisFlex(
    const ChassisFlexEstimateInput& input);

} // namespace heritage::vehicles

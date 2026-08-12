#pragma once

#include "ChassisTorsionalCompliance.hpp"

namespace heritage::vehicles {

struct ChassisFlexDiagnostics
{
    VehicleScalar twistDegrees = 0.0;
    VehicleScalar twistRateDegreesPerSecond = 0.0;
    VehicleScalar elasticEnergyJ = 0.0;
    VehicleScalar kineticEnergyJ = 0.0;
    VehicleScalar frontSectionTwistDegrees = 0.0;
    VehicleScalar rearSectionTwistDegrees = 0.0;
};

ChassisFlexDiagnostics evaluateChassisFlexDiagnostics(
    const ChassisTorsionalComplianceDescription& description,
    const ChassisTorsionalComplianceState& state);

} // namespace heritage::vehicles

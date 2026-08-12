#pragma once

#include "../VehiclePrecision.hpp"

namespace heritage::vehicles::tires {

// MF-Swift 6.2 motorcycle tires use two dimensionless contour parameters,
// MC_CONTOUR_A and MC_CONTOUR_B. The public 6.2 manual defines them as the
// lateral and radial ellipse semi-axes divided by tire width. Heritage keeps
// that vocabulary so measured/fitted motorcycle data can be mapped directly.
struct MotorcycleTireProfileDescription
{
    VehicleScalar tireWidthM = 0.180;
    VehicleScalar mcContourA = 0.50;
    VehicleScalar mcContourB = 0.50;
};

struct MotorcycleTireContactGeometry
{
    bool valid = false;
    VehicleScalar lateralSemiAxisM = 0.0;
    VehicleScalar radialSemiAxisM = 0.0;
    VehicleScalar crownBaseRadiusM = 0.0;
    VehicleScalar lateralContactOffsetM = 0.0;
    VehicleScalar radialContactOffsetM = 0.0;
    VehicleScalar centerToRoadM = 0.0;
};

bool validMotorcycleTireProfile(
    const MotorcycleTireProfileDescription& description,
    VehicleScalar unloadedRadiusM);

// Evaluates the support point of the public MF-Swift motorcycle elliptical
// contour against a locally flat road plane at the supplied inclination.
// Positive camber leans the wheel toward its +lateral axis.
MotorcycleTireContactGeometry evaluateMotorcycleTireProfile(
    const MotorcycleTireProfileDescription& description,
    VehicleScalar unloadedRadiusM,
    VehicleScalar camberAngleRadians);

} // namespace heritage::vehicles::tires

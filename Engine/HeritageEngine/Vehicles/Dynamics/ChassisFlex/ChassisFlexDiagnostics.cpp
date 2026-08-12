#include "ChassisFlexDiagnostics.hpp"

namespace heritage::vehicles {

ChassisFlexDiagnostics evaluateChassisFlexDiagnostics(
    const ChassisTorsionalComplianceDescription& description,
    const ChassisTorsionalComplianceState& state)
{
    constexpr VehicleScalar kPi = 3.141592653589793238462643383279502884;
    const VehicleScalar degreesPerRadian = 180.0 / kPi;
    const VehicleScalar stiffnessNmPerRad =
        description.torsionalRigidityNmPerDegree * degreesPerRadian;

    ChassisFlexDiagnostics result;
    result.twistDegrees = state.twistRadians * degreesPerRadian;
    result.twistRateDegreesPerSecond =
        state.twistRateRadiansPerSecond * degreesPerRadian;
    result.elasticEnergyJ = 0.5 * stiffnessNmPerRad
        * state.twistRadians * state.twistRadians;
    result.kineticEnergyJ = 0.5 * description.effectiveTorsionalInertiaKgM2
        * state.twistRateRadiansPerSecond * state.twistRateRadiansPerSecond;
    result.frontSectionTwistDegrees = chassisSectionTwistRadians(
        description,
        state,
        description.frontReferenceLocalZ) * degreesPerRadian;
    result.rearSectionTwistDegrees = chassisSectionTwistRadians(
        description,
        state,
        description.rearReferenceLocalZ) * degreesPerRadian;
    return result;
}

} // namespace heritage::vehicles

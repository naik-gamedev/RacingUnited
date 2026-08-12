#include "ChassisFlexEstimator.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {

bool finite(VehicleScalar value)
{
    return std::isfinite(value);
}

VehicleScalar baseRigidityNmPerDegree(ChassisConstructionKind construction)
{
    switch (construction)
    {
    case ChassisConstructionKind::ClosedUnibody: return 10000.0;
    case ChassisConstructionKind::OpenUnibody: return 4500.0;
    case ChassisConstructionKind::LadderFrame: return 6500.0;
    case ChassisConstructionKind::SpaceFrame: return 18000.0;
    case ChassisConstructionKind::CarbonMonocoque: return 35000.0;
    case ChassisConstructionKind::Unknown:
    default: return 8500.0;
    }
}

} // namespace

const char* chassisConstructionKindId(ChassisConstructionKind value)
{
    switch (value)
    {
    case ChassisConstructionKind::ClosedUnibody: return "closed_unibody";
    case ChassisConstructionKind::OpenUnibody: return "open_unibody";
    case ChassisConstructionKind::LadderFrame: return "ladder_frame";
    case ChassisConstructionKind::SpaceFrame: return "space_frame";
    case ChassisConstructionKind::CarbonMonocoque: return "carbon_monocoque";
    case ChassisConstructionKind::Unknown:
    default: return "unknown";
    }
}

bool parseChassisConstructionKind(
    const std::string& value,
    ChassisConstructionKind& result)
{
    if (value == "closed_unibody")
        result = ChassisConstructionKind::ClosedUnibody;
    else if (value == "open_unibody")
        result = ChassisConstructionKind::OpenUnibody;
    else if (value == "ladder_frame")
        result = ChassisConstructionKind::LadderFrame;
    else if (value == "space_frame")
        result = ChassisConstructionKind::SpaceFrame;
    else if (value == "carbon_monocoque")
        result = ChassisConstructionKind::CarbonMonocoque;
    else if (value == "unknown")
        result = ChassisConstructionKind::Unknown;
    else
        return false;
    return true;
}

ChassisFlexEstimate estimateChassisFlex(
    const ChassisFlexEstimateInput& input)
{
    ChassisFlexEstimate result;
    if (!finite(input.massKg) || input.massKg < 50.0 || input.massKg > 100000.0
        || !finite(input.wheelbaseM) || input.wheelbaseM < 0.5 || input.wheelbaseM > 15.0
        || !finite(input.frontTrackM) || input.frontTrackM < 0.3 || input.frontTrackM > 6.0
        || !finite(input.rearTrackM) || input.rearTrackM < 0.3 || input.rearTrackM > 6.0
        || !finite(input.centerOfMassHeightM)
        || input.centerOfMassHeightM < 0.05 || input.centerOfMassHeightM > 5.0
        || input.modelYear < 1880 || input.modelYear > 2200)
    {
        return result;
    }

    // This is deliberately an evidence-poor engineering estimate, not a model
    // lookup. Construction sets the broad stiffness class; era, wheelbase and
    // mass only nudge it within that class. Better measured/photo/CAD evidence
    // should replace the output without changing the compliance mechanism.
    const VehicleScalar eraFactor = std::clamp(
        0.72 + static_cast<VehicleScalar>(input.modelYear - 1990) * 0.012,
        0.60,
        1.35);
    const VehicleScalar wheelbaseFactor = std::pow(
        2.50 / input.wheelbaseM,
        0.60);
    const VehicleScalar massFactor = std::pow(
        input.massKg / 1200.0,
        0.18);
    const VehicleScalar trackAverage = 0.5 * (
        input.frontTrackM + input.rearTrackM);
    const VehicleScalar trackFactor = std::pow(
        trackAverage / 1.50,
        0.12);

    result.description.enabled = true;
    result.description.torsionalRigidityNmPerDegree = std::clamp(
        baseRigidityNmPerDegree(input.construction)
            * eraFactor * wheelbaseFactor * massFactor * trackFactor,
        1200.0,
        120000.0);
    result.description.effectiveTorsionalInertiaKgM2 = std::clamp(
        0.08 * input.massKg * input.wheelbaseM * input.wheelbaseM,
        25.0,
        25000.0);
    result.description.frontReferenceLocalZ = 0.5 * input.wheelbaseM;
    result.description.rearReferenceLocalZ = -0.5 * input.wheelbaseM;
    result.description.torsionAxisLocalY = std::clamp(
        input.centerOfMassHeightM * 0.70,
        0.15,
        1.50);

    // About 0.35 critical damping keeps the structural mode responsive but
    // non-ringing. Convert creator-facing Nm/deg stiffness to Nm/rad first.
    constexpr VehicleScalar kPi = 3.141592653589793238462643383279502884;
    const VehicleScalar stiffnessNmPerRad =
        result.description.torsionalRigidityNmPerDegree * (180.0 / kPi);
    const VehicleScalar criticalDamping = 2.0 * std::sqrt(
        stiffnessNmPerRad
        * result.description.effectiveTorsionalInertiaKgM2);
    result.description.torsionalDampingNmsPerRad = 0.35 * criticalDamping;
    result.description.maximumTwistDegrees =
        input.construction == ChassisConstructionKind::CarbonMonocoque
            ? 0.35 : 1.25;

    result.provenance = std::string("estimated_chassis_flex_")
        + chassisConstructionKindId(input.construction) + "_v1";
    result.confidence = input.construction == ChassisConstructionKind::Unknown
        ? 0.10 : 0.18;
    result.valid = validChassisTorsionalComplianceDescription(
        result.description);
    return result;
}

} // namespace heritage::vehicles

#include "VehicleMassPropertiesEstimator.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {

bool finite(VehicleScalar value)
{
    return std::isfinite(value);
}

struct RadiusFactors
{
    VehicleScalar pitchFromWheelbase = 0.43;
    VehicleScalar yawFromWheelbase = 0.48;
    VehicleScalar rollFromTrack = 0.50;
    VehicleScalar confidence = 0.16;
};

RadiusFactors factorsFor(VehicleMassClass massClass)
{
    // Radius-of-gyration factors are intentionally broad engineering priors,
    // not lookup data for any named vehicle. They preserve plausible relative
    // pitch/yaw/roll resistance until component/CAD/measured inertia replaces
    // the estimate. The road-car factors were selected to stay close to the
    // existing compact-hatch collision-envelope inertia, avoiding a hidden
    // handling rewrite merely because MASS01 makes inertia explicit.
    switch (massClass)
    {
    case VehicleMassClass::RoadCar: return { 0.43, 0.48, 0.50, 0.20 };
    case VehicleMassClass::Formula: return { 0.40, 0.44, 0.42, 0.18 };
    case VehicleMassClass::Kart: return { 0.38, 0.42, 0.36, 0.16 };
    case VehicleMassClass::Atv: return { 0.40, 0.44, 0.45, 0.14 };
    case VehicleMassClass::Motorcycle: return { 0.36, 0.40, 0.32, 0.10 };
    case VehicleMassClass::Truck: return { 0.45, 0.50, 0.54, 0.12 };
    case VehicleMassClass::Unknown:
    default: return {};
    }
}

} // namespace

const char* vehicleMassClassId(VehicleMassClass value)
{
    switch (value)
    {
    case VehicleMassClass::RoadCar: return "road_car";
    case VehicleMassClass::Formula: return "formula";
    case VehicleMassClass::Kart: return "kart";
    case VehicleMassClass::Atv: return "atv";
    case VehicleMassClass::Motorcycle: return "motorcycle";
    case VehicleMassClass::Truck: return "truck";
    case VehicleMassClass::Unknown:
    default: return "unknown";
    }
}

bool parseVehicleMassClass(const std::string& value, VehicleMassClass& result)
{
    if (value == "road_car" || value == "car")
        result = VehicleMassClass::RoadCar;
    else if (value == "formula" || value == "indycar" || value == "sprint_car")
        result = VehicleMassClass::Formula;
    else if (value == "kart" || value == "go_kart")
        result = VehicleMassClass::Kart;
    else if (value == "atv")
        result = VehicleMassClass::Atv;
    else if (value == "motorcycle")
        result = VehicleMassClass::Motorcycle;
    else if (value == "truck")
        result = VehicleMassClass::Truck;
    else if (value == "unknown" || value == "custom" || value.empty())
        result = VehicleMassClass::Unknown;
    else
        return false;
    return true;
}

VehicleMassPropertiesEstimate estimateVehicleMassProperties(
    const VehicleMassPropertiesEstimateInput& input)
{
    VehicleMassPropertiesEstimate result;
    if (!finite(input.totalMassKg)
        || input.totalMassKg < 20.0 || input.totalMassKg > 1000000.0
        || !finite(input.wheelbaseM)
        || input.wheelbaseM < 0.30 || input.wheelbaseM > 30.0
        || !finite(input.frontTrackM)
        || input.frontTrackM < 0.20 || input.frontTrackM > 10.0
        || !finite(input.rearTrackM)
        || input.rearTrackM < 0.20 || input.rearTrackM > 10.0
        || !finite(input.centerOfMassHeightM)
        || input.centerOfMassHeightM < 0.02 || input.centerOfMassHeightM > 8.0
        || !finite(input.frontStaticLoadFraction)
        || input.frontStaticLoadFraction < 0.05
        || input.frontStaticLoadFraction > 0.95
        || !finite(input.leftStaticLoadFraction)
        || input.leftStaticLoadFraction < 0.05
        || input.leftStaticLoadFraction > 0.95)
    {
        return result;
    }

    const RadiusFactors factors = factorsFor(input.massClass);
    const VehicleScalar averageTrack = 0.5 * (
        input.frontTrackM + input.rearTrackM);

    // With the authored axle midpoint at local Z=0, static axle load directly
    // determines longitudinal COM. Positive Heritage Z is forward.
    const VehicleScalar comZ = (
        input.frontStaticLoadFraction - 0.5) * input.wheelbaseM;
    // Positive X is right, so a larger left static load means COM moved left.
    const VehicleScalar comX = (
        0.5 - input.leftStaticLoadFraction) * averageTrack;

    const VehicleScalar pitchRadius = std::max(
        0.05, factors.pitchFromWheelbase * input.wheelbaseM);
    const VehicleScalar yawRadius = std::max(
        0.05, factors.yawFromWheelbase * input.wheelbaseM);
    const VehicleScalar rollRadius = std::max(
        0.05, factors.rollFromTrack * averageTrack);

    const VehicleScalar pitchInertia = input.totalMassKg
        * pitchRadius * pitchRadius;
    const VehicleScalar yawInertia = input.totalMassKg
        * yawRadius * yawRadius;
    const VehicleScalar rollInertia = input.totalMassKg
        * rollRadius * rollRadius;

    if (!finite(pitchInertia) || !finite(yawInertia) || !finite(rollInertia)
        || pitchInertia <= 0.0 || yawInertia <= 0.0 || rollInertia <= 0.0)
    {
        return result;
    }

    result.totalMassKg = input.totalMassKg;
    result.centerOfMassLocal = {
        static_cast<float>(comX),
        static_cast<float>(input.centerOfMassHeightM),
        static_cast<float>(comZ)
    };
    result.inertiaLocalKgM2 = {
        static_cast<float>(pitchInertia),
        static_cast<float>(yawInertia),
        static_cast<float>(rollInertia)
    };
    result.frontStaticMassKg = input.totalMassKg
        * input.frontStaticLoadFraction;
    result.rearStaticMassKg = input.totalMassKg - result.frontStaticMassKg;
    result.leftStaticMassKg = input.totalMassKg
        * input.leftStaticLoadFraction;
    result.rightStaticMassKg = input.totalMassKg - result.leftStaticMassKg;
    result.provenance = std::string("estimated_mass_properties_")
        + vehicleMassClassId(input.massClass) + "_v1";
    result.confidence = factors.confidence;
    result.valid = true;
    return result;
}

} // namespace heritage::vehicles

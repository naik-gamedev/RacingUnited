#include "TireCarcassDevelopmentLab.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

namespace heritage::vehicles::tires {
namespace {

constexpr VehicleScalar kPi = 3.14159265358979323846;
constexpr VehicleScalar kTwoPi = 2.0 * kPi;
constexpr std::size_t kGlobalParameterCount = 32;
constexpr std::size_t kStationBankCount = 5;
constexpr std::size_t kBandBankCount = 5;
constexpr std::size_t kStationParameterCount =
    TireFlexibleRingFieldStations * kStationBankCount;
constexpr std::size_t kBandParameterCount =
    TireFlexibleRingFieldBands * kBandBankCount;
constexpr std::size_t kParameterCount =
    kGlobalParameterCount + kStationParameterCount + kBandParameterCount;

const std::array<const char*, TireCarcassDevelopmentGroupCount> kGroupNames{
    "CORE / INERTIA",
    "PRESSURE / THERMAL",
    "ROAD CONTACT",
    "RIM / BEAD",
    "RIGID-RING ANCHORS",
    "SOLVER / NUMERICS",
    "STATION FOUNDATION",
    "STATION CONTACT",
    "STATION DAMPING",
    "STATION ANCHOR",
    "STATION COUPLING",
    "BAND FOUNDATION",
    "BAND CONTACT",
    "BAND DAMPING",
    "BAND ANCHOR",
    "BAND COUPLING"
};

struct GlobalInfo
{
    const char* key;
    const char* label;
    std::size_t group;
    VehicleScalar minimum;
    VehicleScalar maximum;
    VehicleScalar defaultValue;
    bool integer;
};

const std::array<GlobalInfo, kGlobalParameterCount> kGlobals{{
    {"mass_scale", "Effective carcass/belt mass", 0, 0.05, 20.0, 1.0, false},
    {"foundation_scale", "Rim-to-carcass foundation stiffness", 0, 0.0, 20.0, 1.0, false},
    {"circumferential_scale", "Circumferential neighbour stiffness", 0, 0.0, 20.0, 1.0, false},
    {"second_neighbor_scale", "Second-neighbour belt stiffness", 0, 0.0, 20.0, 1.0, false},
    {"lateral_scale", "Lateral carcass coupling", 0, 0.0, 20.0, 1.0, false},
    {"damping_scale", "Structural damping", 0, 0.0, 10.0, 1.0, false},
    {"velocity_retention", "Post-step velocity retention", 0, 0.0, 1.0, 0.86, false},
    {"pressure_exponent", "Pressure -> stiffness exponent", 1, 0.0, 2.0, 0.50, false},
    {"pneumatic_minimum", "Minimum pneumatic stiffness scale", 1, 0.005, 2.0, 0.10, false},
    {"pneumatic_maximum", "Maximum pneumatic stiffness scale", 1, 0.05, 8.0, 2.25, false},
    {"thermal_influence", "Thermal stiffness influence", 1, 0.0, 2.0, 1.0, false},
    {"contact_scale", "Road contact constraint stiffness", 2, 0.0, 40.0, 1.0, false},
    {"association_forward", "Road-sample forward association weight", 2, 0.10, 10.0, 1.0, false},
    {"association_lateral", "Road-sample lateral association weight", 2, 0.10, 10.0, 1.0, false},
    {"ground_station_threshold", "Lowest station eligibility threshold", 2, -1.0, 1.0, -0.10, false},
    {"contact_slop_mm", "Road contact compliance/slop", 2, 0.0, 10.0, 0.0, false},
    {"rim_contact_scale", "Internal rim/flange contact stiffness", 3, 0.0, 40.0, 1.0, false},
    {"flange_height_scale", "Flange-height envelope", 3, 0.10, 4.0, 1.0, false},
    {"flange_clearance_scale", "Flange clearance", 3, 0.0, 4.0, 1.0, false},
    {"shoulder_allowance_scale", "Shoulder rim-clearance allowance", 3, 0.0, 4.0, 1.0, false},
    {"longitudinal_anchor", "Rigid-ring longitudinal anchor", 4, 0.0, 4.0, 1.0, false},
    {"lateral_anchor", "Rigid-ring lateral anchor", 4, 0.0, 4.0, 1.0, false},
    {"yaw_anchor", "Rigid-ring yaw anchor", 4, 0.0, 4.0, 1.0, false},
    {"windup_anchor", "Rigid-ring wind-up anchor", 4, 0.0, 4.0, 1.0, false},
    {"contact_twist_anchor", "Contact-patch twist anchor", 4, 0.0, 4.0, 1.0, false},
    {"poisson_bulge", "Structural sidewall Poisson bulge", 4, 0.0, 4.0, 0.0, false},
    {"flat_spot_scale", "Flat-spot structural anchor", 4, 0.0, 4.0, 1.0, false},
    {"radial_compression_scale", "Compression driving Poisson response", 4, 0.0, 4.0, 1.0, false},
    {"lower_hemisphere_anchor", "Lower-hemisphere anchor concentration", 4, 0.0, 3.0, 1.0, false},
    {"maximum_magnitude", "Catastrophe guard distance", 5, 0.10, 5.0, 1.0, false},
    {"structural_rate", "Structural solver rate multiplier", 5, 0.25, 8.0, 1.0, false},
    {"implicit_iterations", "Implicit Jacobi iterations", 5, 1.0, 32.0, 8.0, true}
}};

VehicleScalar clampFinite(VehicleScalar value, VehicleScalar fallback)
{
    return std::isfinite(static_cast<double>(value)) ? value : fallback;
}

std::uint64_t splitmix64(std::uint64_t value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

VehicleScalar unitRandom(std::uint64_t seed, std::uint64_t stream)
{
    const std::uint64_t bits = splitmix64(seed ^ (stream * 0x9e3779b97f4a7c15ULL));
    return static_cast<VehicleScalar>((bits >> 11) * (1.0 / 9007199254740992.0));
}

std::size_t fieldIndex(std::size_t station, std::size_t band)
{
    return station * TireFlexibleRingFieldBands + band;
}

VehicleScalar radialDisplacement(
    const TireFlexibleRingFieldOutput& field,
    std::size_t station,
    std::size_t band)
{
    const VehicleScalar theta = kTwoPi
        * static_cast<VehicleScalar>(station)
        / static_cast<VehicleScalar>(TireFlexibleRingFieldStations);
    const std::size_t index = fieldIndex(station, band);
    return field.forwardDisplacementM[index] * std::cos(theta)
        + field.downDisplacementM[index] * std::sin(theta);
}

VehicleScalar structuralRestRadialScale(std::size_t band)
{
    const VehicleScalar absoluteWidth = std::abs(
        TireFlexibleRingWidthCoordinates[band]);
    const VehicleScalar shoulderT = std::clamp(
        (absoluteWidth - VehicleScalar{0.80}) / VehicleScalar{0.20},
        VehicleScalar{0.0}, VehicleScalar{1.0});
    const VehicleScalar shoulder = shoulderT * shoulderT
        * (VehicleScalar{3.0} - VehicleScalar{2.0} * shoulderT);
    return VehicleScalar{1.0}
        - VehicleScalar{0.055} * shoulder * shoulder;
}


void setFlatRoadSamples(
    TireFlexibleRingDynamicsInput& input,
    const TireFlexibleRingFieldDescription& description,
    VehicleScalar roadDownM,
    bool partialEdge,
    VehicleScalar bankRadians)
{
    input.roadSampleCount = 0;
    const VehicleScalar halfWidth = description.sectionWidthM * VehicleScalar{0.5};
    const VehicleScalar spanForward = description.unloadedRadiusM * VehicleScalar{0.42};
    const VehicleScalar cosB = std::cos(bankRadians);
    const VehicleScalar sinB = std::sin(bankRadians);
    for (int fi = -2; fi <= 2; ++fi)
    {
        for (int bi = -2; bi <= 2; ++bi)
        {
            if (input.roadSampleCount >= TireFlexibleRingMaximumRoadSamples)
                return;
            auto& sample = input.roadSamples[input.roadSampleCount++];
            sample.queried = true;
            const VehicleScalar forward = spanForward * VehicleScalar{0.5 * fi};
            const VehicleScalar lateral = halfWidth * VehicleScalar{0.48 * bi};
            sample.queryForwardM = forward;
            sample.queryLateralM = lateral;
            sample.supported = !partialEdge || lateral <= VehicleScalar{0.015};
            if (!sample.supported)
                continue;
            sample.pointForwardM = forward;
            sample.pointDownM = roadDownM + lateral * std::tan(bankRadians);
            sample.pointLateralM = lateral;
            sample.normalForward = 0.0;
            sample.normalDown = -cosB;
            sample.normalLateral = sinB;
        }
    }
}

TireFlexibleRingDynamicsInput scenarioInput(
    const TireCarcassSyntheticInput& synthetic,
    TireCarcassSyntheticScenario scenario,
    const TireFlexibleRingDevelopmentTuning& tuning)
{
    TireFlexibleRingDynamicsInput input;
    input.deltaTimeSeconds = VehicleScalar{0.008}
        / std::clamp(tuning.structuralRateScale,
            VehicleScalar{0.25}, VehicleScalar{8.0});
    input.grounded = scenario != TireCarcassSyntheticScenario::AirborneRelaxation;
    input.inflationPressurePa = synthetic.referencePressurePa;
    input.thermalStiffnessScale = 1.0;
    input.normalLoadN = synthetic.normalLoadN;
    input.developmentTuning = &tuning;

    switch (scenario)
    {
    case TireCarcassSyntheticScenario::HardAcceleration:
        input.ringLongitudinalOffsetM = 0.025;
        break;
    case TireCarcassSyntheticScenario::HardBraking:
        input.ringLongitudinalOffsetM = -0.025;
        break;
    case TireCarcassSyntheticScenario::HardCornering:
        input.ringLateralOffsetM = 0.020;
        input.ringYawRadians = 0.035;
        break;
    case TireCarcassSyntheticScenario::CombinedBrakingCornering:
        input.ringLongitudinalOffsetM = -0.022;
        input.ringLateralOffsetM = 0.018;
        input.ringYawRadians = 0.030;
        input.contactPatchTwistRadians = 0.025;
        break;
    case TireCarcassSyntheticScenario::LowPressure:
        input.inflationPressurePa = 90000.0;
        break;
    case TireCarcassSyntheticScenario::ZeroPressure:
        input.inflationPressurePa = 0.0;
        break;
    case TireCarcassSyntheticScenario::HighPressure:
        input.inflationPressurePa = 420000.0;
        break;
    case TireCarcassSyntheticScenario::FlatSpot:
        input.flatSpotDepthM = 0.006;
        input.flatSpotSector = 4.0;
        input.wheelRotationRadians = 0.0;
        break;
    default:
        break;
    }

    if (input.grounded)
    {
        const VehicleScalar overlap = synthetic.roadOverlapM
            * (scenario == TireCarcassSyntheticScenario::ZeroPressure
                ? VehicleScalar{2.2}
                : scenario == TireCarcassSyntheticScenario::LowPressure
                    ? VehicleScalar{1.5} : VehicleScalar{1.0});
        const VehicleScalar roadDown = synthetic.description.unloadedRadiusM - overlap;
        const bool partialEdge = scenario == TireCarcassSyntheticScenario::PartialRoadEdge;
        const VehicleScalar bank = scenario == TireCarcassSyntheticScenario::BankedRoad
            ? VehicleScalar{0.20} : VehicleScalar{0.0};
        setFlatRoadSamples(input, synthetic.description, roadDown, partialEdge, bank);
    }
    return input;
}

VehicleScalar minimumRimRadius(
    const TireFlexibleRingFieldDescription& d,
    const TireFlexibleRingDevelopmentTuning& tuning,
    std::size_t band)
{
    const VehicleScalar sidewall = d.unloadedRadiusM - d.rimRadiusM;
    const VehicleScalar absoluteWidth = std::abs(TireFlexibleRingWidthCoordinates[band]);
    const VehicleScalar flangeHeight = std::clamp(
        sidewall * VehicleScalar{0.15} * tuning.flangeHeightScale,
        VehicleScalar{0.001}, VehicleScalar{0.050});
    const VehicleScalar clearance = std::clamp(
        sidewall * VehicleScalar{0.10} * tuning.flangeClearanceScale,
        VehicleScalar{0.0}, VehicleScalar{0.040});
    const VehicleScalar shoulder = sidewall * VehicleScalar{0.035}
        * tuning.shoulderAllowanceScale * (VehicleScalar{1.0} - absoluteWidth);
    return d.rimRadiusM + flangeHeight + clearance + shoulder;
}

} // namespace

std::size_t tireCarcassDevelopmentParameterCount()
{
    return kParameterCount;
}

const char* tireCarcassDevelopmentGroupName(std::size_t groupIndex)
{
    return groupIndex < kGroupNames.size() ? kGroupNames[groupIndex] : "UNKNOWN";
}

bool tireCarcassDevelopmentParameterInfo(
    std::size_t index,
    TireCarcassDevelopmentParameterInfo& value)
{
    if (index >= kParameterCount)
        return false;
    if (index < kGlobalParameterCount)
    {
        const auto& g = kGlobals[index];
        value.key = g.key;
        value.label = g.label;
        value.group = tireCarcassDevelopmentGroupName(g.group);
        value.groupIndex = g.group;
        value.minimum = g.minimum;
        value.maximum = g.maximum;
        value.defaultValue = g.defaultValue;
        value.integer = g.integer;
        return true;
    }

    index -= kGlobalParameterCount;
    if (index < kStationParameterCount)
    {
        const std::size_t bank = index / TireFlexibleRingFieldStations;
        const std::size_t station = index % TireFlexibleRingFieldStations;
        static const std::array<const char*, kStationBankCount> keys{
            "station_foundation", "station_contact", "station_damping",
            "station_anchor", "station_coupling" };
        static const std::array<std::size_t, kStationBankCount> groups{
            6, 7, 8, 9, 10 };
        value.key = std::string(keys[bank]) + "_" + std::to_string(station);
        value.label = std::string("Station ") + std::to_string(station)
            + " / " + keys[bank];
        value.groupIndex = groups[bank];
        value.group = tireCarcassDevelopmentGroupName(value.groupIndex);
        value.minimum = 0.0;
        value.maximum = 10.0;
        value.defaultValue = 1.0;
        value.integer = false;
        return true;
    }

    index -= kStationParameterCount;
    const std::size_t bank = index / TireFlexibleRingFieldBands;
    const std::size_t band = index % TireFlexibleRingFieldBands;
    static const std::array<const char*, kBandBankCount> keys{
        "band_foundation", "band_contact", "band_damping",
        "band_anchor", "band_coupling" };
    static const std::array<std::size_t, kBandBankCount> groups{
        11, 12, 13, 14, 15 };
    value.key = std::string(keys[bank]) + "_" + std::to_string(band);
    value.label = std::string("Band ") + std::to_string(band)
        + " / " + keys[bank];
    value.groupIndex = groups[bank];
    value.group = tireCarcassDevelopmentGroupName(value.groupIndex);
    value.minimum = 0.0;
    value.maximum = 10.0;
    value.defaultValue = 1.0;
    value.integer = false;
    return true;
}

VehicleScalar tireCarcassDevelopmentParameterValue(
    const TireFlexibleRingDevelopmentTuning& t,
    std::size_t index)
{
    switch (index)
    {
    case 0: return t.effectiveMassScale;
    case 1: return t.foundationScale;
    case 2: return t.circumferentialScale;
    case 3: return t.secondNeighborScale;
    case 4: return t.lateralScale;
    case 5: return t.dampingScale;
    case 6: return t.velocityRetention;
    case 7: return t.pressureExponent;
    case 8: return t.pneumaticMinimumScale;
    case 9: return t.pneumaticMaximumScale;
    case 10: return t.thermalInfluence;
    case 11: return t.contactScale;
    case 12: return t.associationForwardScale;
    case 13: return t.associationLateralScale;
    case 14: return t.groundStationThreshold;
    case 15: return t.contactSlopM * VehicleScalar{1000.0};
    case 16: return t.rimContactScale;
    case 17: return t.flangeHeightScale;
    case 18: return t.flangeClearanceScale;
    case 19: return t.shoulderAllowanceScale;
    case 20: return t.longitudinalAnchorScale;
    case 21: return t.lateralAnchorScale;
    case 22: return t.yawAnchorScale;
    case 23: return t.windupAnchorScale;
    case 24: return t.contactTwistAnchorScale;
    case 25: return t.poissonBulgeScale;
    case 26: return t.flatSpotScale;
    case 27: return t.radialCompressionScale;
    case 28: return t.lowerHemisphereAnchorScale;
    case 29: return t.maximumMagnitudeScale;
    case 30: return t.structuralRateScale;
    case 31: return static_cast<VehicleScalar>(t.implicitIterations);
    default: break;
    }
    index -= kGlobalParameterCount;
    if (index < kStationParameterCount)
    {
        const std::size_t bank = index / TireFlexibleRingFieldStations;
        const std::size_t station = index % TireFlexibleRingFieldStations;
        switch (bank)
        {
        case 0: return t.stationFoundationScale[station];
        case 1: return t.stationContactScale[station];
        case 2: return t.stationDampingScale[station];
        case 3: return t.stationAnchorScale[station];
        default: return t.stationCircumferentialScale[station];
        }
    }
    index -= kStationParameterCount;
    const std::size_t bank = index / TireFlexibleRingFieldBands;
    const std::size_t band = index % TireFlexibleRingFieldBands;
    switch (bank)
    {
    case 0: return t.bandFoundationScale[band];
    case 1: return t.bandContactScale[band];
    case 2: return t.bandDampingScale[band];
    case 3: return t.bandAnchorScale[band];
    default: return t.bandLateralScale[band];
    }
}

bool setTireCarcassDevelopmentParameterValue(
    TireFlexibleRingDevelopmentTuning& t,
    std::size_t index,
    VehicleScalar raw)
{
    TireCarcassDevelopmentParameterInfo info;
    if (!tireCarcassDevelopmentParameterInfo(index, info))
        return false;
    VehicleScalar value = std::clamp(
        clampFinite(raw, info.defaultValue), info.minimum, info.maximum);
    if (info.integer)
        value = std::round(value);
    switch (index)
    {
    case 0: t.effectiveMassScale = value; return true;
    case 1: t.foundationScale = value; return true;
    case 2: t.circumferentialScale = value; return true;
    case 3: t.secondNeighborScale = value; return true;
    case 4: t.lateralScale = value; return true;
    case 5: t.dampingScale = value; return true;
    case 6: t.velocityRetention = value; return true;
    case 7: t.pressureExponent = value; return true;
    case 8: t.pneumaticMinimumScale = value; return true;
    case 9: t.pneumaticMaximumScale = value; return true;
    case 10: t.thermalInfluence = value; return true;
    case 11: t.contactScale = value; return true;
    case 12: t.associationForwardScale = value; return true;
    case 13: t.associationLateralScale = value; return true;
    case 14: t.groundStationThreshold = value; return true;
    case 15: t.contactSlopM = value * VehicleScalar{0.001}; return true;
    case 16: t.rimContactScale = value; return true;
    case 17: t.flangeHeightScale = value; return true;
    case 18: t.flangeClearanceScale = value; return true;
    case 19: t.shoulderAllowanceScale = value; return true;
    case 20: t.longitudinalAnchorScale = value; return true;
    case 21: t.lateralAnchorScale = value; return true;
    case 22: t.yawAnchorScale = value; return true;
    case 23: t.windupAnchorScale = value; return true;
    case 24: t.contactTwistAnchorScale = value; return true;
    case 25: t.poissonBulgeScale = value; return true;
    case 26: t.flatSpotScale = value; return true;
    case 27: t.radialCompressionScale = value; return true;
    case 28: t.lowerHemisphereAnchorScale = value; return true;
    case 29: t.maximumMagnitudeScale = value; return true;
    case 30: t.structuralRateScale = value; return true;
    case 31: t.implicitIterations = static_cast<int>(value); return true;
    default: break;
    }
    index -= kGlobalParameterCount;
    if (index < kStationParameterCount)
    {
        const std::size_t bank = index / TireFlexibleRingFieldStations;
        const std::size_t station = index % TireFlexibleRingFieldStations;
        switch (bank)
        {
        case 0: t.stationFoundationScale[station] = value; break;
        case 1: t.stationContactScale[station] = value; break;
        case 2: t.stationDampingScale[station] = value; break;
        case 3: t.stationAnchorScale[station] = value; break;
        default: t.stationCircumferentialScale[station] = value; break;
        }
        return true;
    }
    index -= kStationParameterCount;
    const std::size_t bank = index / TireFlexibleRingFieldBands;
    const std::size_t band = index % TireFlexibleRingFieldBands;
    switch (bank)
    {
    case 0: t.bandFoundationScale[band] = value; break;
    case 1: t.bandContactScale[band] = value; break;
    case 2: t.bandDampingScale[band] = value; break;
    case 3: t.bandAnchorScale[band] = value; break;
    default: t.bandLateralScale[band] = value; break;
    }
    return true;
}

void resetTireCarcassDevelopmentTuning(
    TireFlexibleRingDevelopmentTuning& tuning,
    bool enabled)
{
    tuning = TireFlexibleRingDevelopmentTuning{};
    tuning.enabled = enabled;
}

const char* tireCarcassSyntheticScenarioName(TireCarcassSyntheticScenario value)
{
    switch (value)
    {
    case TireCarcassSyntheticScenario::StaticFlat: return "static_flat";
    case TireCarcassSyntheticScenario::HardAcceleration: return "hard_acceleration";
    case TireCarcassSyntheticScenario::HardBraking: return "hard_braking";
    case TireCarcassSyntheticScenario::HardCornering: return "hard_cornering";
    case TireCarcassSyntheticScenario::CombinedBrakingCornering: return "combined_braking_cornering";
    case TireCarcassSyntheticScenario::LowPressure: return "low_pressure";
    case TireCarcassSyntheticScenario::ZeroPressure: return "zero_pressure";
    case TireCarcassSyntheticScenario::PartialRoadEdge: return "partial_road_edge";
    case TireCarcassSyntheticScenario::BankedRoad: return "banked_road";
    case TireCarcassSyntheticScenario::FlatSpot: return "flat_spot";
    case TireCarcassSyntheticScenario::AirborneRelaxation: return "airborne_relaxation";
    case TireCarcassSyntheticScenario::HighPressure: return "high_pressure";
    }
    return "static_flat";
}

bool tireCarcassSyntheticScenarioFromName(
    const std::string& name,
    TireCarcassSyntheticScenario& value)
{
    for (int raw = 0; raw <= static_cast<int>(TireCarcassSyntheticScenario::HighPressure); ++raw)
    {
        const auto candidate = static_cast<TireCarcassSyntheticScenario>(raw);
        if (name == tireCarcassSyntheticScenarioName(candidate))
        {
            value = candidate;
            return true;
        }
    }
    return false;
}

TireCarcassSyntheticResult runTireCarcassSyntheticScenario(
    const TireCarcassSyntheticInput& synthetic,
    const TireFlexibleRingDevelopmentTuning& tuning,
    TireCarcassSyntheticScenario scenario)
{
    TireCarcassSyntheticResult result;
    result.scenario = tireCarcassSyntheticScenarioName(scenario);
    if (!validTireFlexibleRingFieldDescription(synthetic.description))
        return result;

    TireFlexibleRingDynamicState state;
    TireFlexibleRingFieldOutput field;
    auto input = scenarioInput(synthetic, scenario, tuning);
    const std::size_t steps = std::clamp<std::size_t>(
        synthetic.integrationSteps, 4, 240);
    for (std::size_t step = 0; step < steps; ++step)
        field = advanceTireFlexibleRingDynamics(
            synthetic.description, input, state);
    result.integrationSteps = steps;
    if (!field.valid && scenario != TireCarcassSyntheticScenario::AirborneRelaxation)
        return result;

    const std::size_t bottom = TireFlexibleRingFieldStations / 4;
    const std::size_t front = (bottom + TireFlexibleRingFieldStations - 1)
        % TireFlexibleRingFieldStations;
    const std::size_t rear = (bottom + 1) % TireFlexibleRingFieldStations;
    const std::size_t centerBand = TireFlexibleRingFieldBands / 2;

    VehicleScalar maxDisplacement = 0.0;
    VehicleScalar velocitySquared = 0.0;
    VehicleScalar roadPenetration = 0.0;
    VehicleScalar rimPenetration = 0.0;
    for (std::size_t station = 0; station < TireFlexibleRingFieldStations; ++station)
    {
        const VehicleScalar theta = kTwoPi * static_cast<VehicleScalar>(station)
            / static_cast<VehicleScalar>(TireFlexibleRingFieldStations);
        const VehicleScalar radial = radialDisplacement(field, station, centerBand);
        result.radialProfileMm[station] = radial * 1000.0;
        for (std::size_t band = 0; band < TireFlexibleRingFieldBands; ++band)
        {
            const std::size_t index = fieldIndex(station, band);
            const VehicleScalar dx = field.forwardDisplacementM[index];
            const VehicleScalar dy = field.downDisplacementM[index];
            const VehicleScalar dz = field.lateralDisplacementM[index];
            maxDisplacement = std::max(maxDisplacement,
                std::sqrt(dx * dx + dy * dy + dz * dz));
            const VehicleScalar vx = state.forwardVelocityMps[index];
            const VehicleScalar vy = state.downVelocityMps[index];
            const VehicleScalar vz = state.lateralVelocityMps[index];
            velocitySquared += vx * vx + vy * vy + vz * vz;

            // Metrics must use the exact same rounded structural rest carcass
            // convention as TireFlexibleRingField.cpp.  Using a full-radius
            // cylinder here falsely reported ~17 mm of "road penetration" at
            // the outer width bands even when the exact solver was within a
            // millimetre of the road.
            const VehicleScalar restScale = structuralRestRadialScale(band);
            const VehicleScalar restX = synthetic.description.unloadedRadiusM
                * restScale * std::cos(theta);
            const VehicleScalar restY = synthetic.description.unloadedRadiusM
                * restScale * std::sin(theta);
            const VehicleScalar currentX = restX + dx;
            const VehicleScalar currentY = restY + dy;
            const VehicleScalar restZ = synthetic.description.sectionWidthM
                * VehicleScalar{0.5} * VehicleScalar{0.94}
                * TireFlexibleRingWidthCoordinates[band];
            const VehicleScalar currentZ = restZ + dz;

            // Score road penetration against the same discrete road-envelope
            // query topology used by the structural solve.  A banked road is
            // not a flat world-height plane, and an explicit unsupported query
            // at a road edge must not be scored as if invisible ground existed
            // there.  This is diagnostic/ranking logic only; it never changes
            // the carcass solution.
            if (input.grounded && std::sin(theta) > tuning.groundStationThreshold)
            {
                VehicleScalar bestDistanceSquared =
                    std::numeric_limits<VehicleScalar>::infinity();
                const TireFlexibleRingRoadSample* nearestRoad = nullptr;
                const std::size_t sampleCount = std::min(
                    input.roadSampleCount, TireFlexibleRingMaximumRoadSamples);
                for (std::size_t sampleIndex = 0;
                     sampleIndex < sampleCount; ++sampleIndex)
                {
                    const auto& sample = input.roadSamples[sampleIndex];
                    if (!sample.queried)
                        continue;
                    const VehicleScalar queryDx =
                        (currentX - sample.queryForwardM)
                        * tuning.associationForwardScale;
                    const VehicleScalar queryDz =
                        (currentZ - sample.queryLateralM)
                        * tuning.associationLateralScale;
                    const VehicleScalar distanceSquared =
                        queryDx * queryDx + queryDz * queryDz;
                    if (distanceSquared < bestDistanceSquared)
                    {
                        bestDistanceSquared = distanceSquared;
                        nearestRoad = &sample;
                    }
                }
                if (nearestRoad != nullptr && nearestRoad->supported)
                {
                    VehicleScalar nx = nearestRoad->normalForward;
                    VehicleScalar ny = nearestRoad->normalDown;
                    VehicleScalar nz = nearestRoad->normalLateral;
                    const VehicleScalar normalLength = std::sqrt(
                        nx * nx + ny * ny + nz * nz);
                    if (normalLength > VehicleScalar{1.0e-9})
                    {
                        nx /= normalLength;
                        ny /= normalLength;
                        nz /= normalLength;
                    }
                    else
                    {
                        nx = 0.0;
                        ny = -1.0;
                        nz = 0.0;
                    }
                    const VehicleScalar signedDistance =
                        (currentX - nearestRoad->pointForwardM) * nx
                        + (currentY - nearestRoad->pointDownM) * ny
                        + (currentZ - nearestRoad->pointLateralM) * nz;
                    const VehicleScalar slop = std::clamp(
                        tuning.contactSlopM,
                        VehicleScalar{0.0}, VehicleScalar{0.020});
                    roadPenetration = std::max(
                        roadPenetration,
                        std::max(-signedDistance - slop, VehicleScalar{0.0}));
                }
            }
            const VehicleScalar currentRadius = std::sqrt(currentX * currentX + currentY * currentY);
            rimPenetration = std::max(rimPenetration,
                minimumRimRadius(synthetic.description, tuning, band) - currentRadius);
        }
    }

    VehicleScalar minBottom = std::numeric_limits<VehicleScalar>::infinity();
    VehicleScalar maxBottom = -std::numeric_limits<VehicleScalar>::infinity();
    for (std::size_t band = 0; band < TireFlexibleRingFieldBands; ++band)
    {
        const std::size_t index = fieldIndex(bottom, band);
        const VehicleScalar height = synthetic.description.unloadedRadiusM
            * structuralRestRadialScale(band)
            + field.downDisplacementM[index];
        result.bottomCrossSectionMm[band] = height * 1000.0;
        minBottom = std::min(minBottom, height);
        maxBottom = std::max(maxBottom, height);
    }

    const auto stationBottomHeight = [&](std::size_t station) {
        const VehicleScalar theta = kTwoPi * static_cast<VehicleScalar>(station)
            / static_cast<VehicleScalar>(TireFlexibleRingFieldStations);
        return synthetic.description.unloadedRadiusM * std::sin(theta)
            + field.downDisplacementM[fieldIndex(station, centerBand)];
    };
    const VehicleScalar centerHeight = stationBottomHeight(bottom);
    const VehicleScalar frontHeight = stationBottomHeight(front);
    const VehicleScalar rearHeight = stationBottomHeight(rear);
    // Positive hook means the exact bottom is pulled farther toward the rim
    // than the neighbouring lower controls, the failure visible in the user's
    // screenshots. This is a ranking metric only, never a solver constraint.
    const VehicleScalar hook = std::max(
        VehicleScalar{0.0},
        (frontHeight + rearHeight) * VehicleScalar{0.5} - centerHeight);
    const VehicleScalar asymmetry = std::abs(frontHeight - rearHeight);

    result.roadPenetrationMm = std::max(roadPenetration, VehicleScalar{0.0}) * 1000.0;
    result.rimPenetrationMm = std::max(rimPenetration, VehicleScalar{0.0}) * 1000.0;
    result.lowerHookMm = hook * 1000.0;
    result.staticAsymmetryMm = asymmetry * 1000.0;
    result.footprintHeightRangeMm = std::max(
        maxBottom - minBottom, VehicleScalar{0.0}) * 1000.0;
    result.maximumDisplacementMm = maxDisplacement * 1000.0;
    result.rmsVelocityMps = std::sqrt(velocitySquared
        / static_cast<VehicleScalar>(TireFlexibleRingFieldCount));
    result.centerBottomHeightMm = centerHeight * 1000.0;
    result.frontBottomHeightMm = frontHeight * 1000.0;
    result.rearBottomHeightMm = rearHeight * 1000.0;

    const std::size_t centerIndex = fieldIndex(bottom, centerBand);
    const VehicleScalar centerTheta = kTwoPi
        * static_cast<VehicleScalar>(bottom)
        / static_cast<VehicleScalar>(TireFlexibleRingFieldStations);
    const VehicleScalar radialForward = std::cos(centerTheta);
    const VehicleScalar radialDown = std::sin(centerTheta);
    const VehicleScalar tangentForward = -radialDown;
    const VehicleScalar tangentDown = radialForward;
    const VehicleScalar centerForward = field.forwardDisplacementM[centerIndex];
    const VehicleScalar centerDown = field.downDisplacementM[centerIndex];
    result.centerForwardDisplacementMm = centerForward * 1000.0;
    result.centerDownDisplacementMm = centerDown * 1000.0;
    result.centerLateralDisplacementMm =
        field.lateralDisplacementM[centerIndex] * 1000.0;
    result.centerRadialDisplacementMm =
        (centerForward * radialForward + centerDown * radialDown) * 1000.0;
    result.centerTangentialDisplacementMm =
        (centerForward * tangentForward + centerDown * tangentDown) * 1000.0;

    // Search ordering only. Zero is "no obvious pathology"; it is deliberately
    // not fed back to physics and the user can ignore it while visually judging
    // candidates. Road/rim penetration and the inward hook dominate.
    const bool flatSupportScenario =
        scenario != TireCarcassSyntheticScenario::PartialRoadEdge
        && scenario != TireCarcassSyntheticScenario::BankedRoad
        && scenario != TireCarcassSyntheticScenario::AirborneRelaxation;
    result.pathologyScore =
        result.roadPenetrationMm * VehicleScalar{20.0}
        + result.rimPenetrationMm * VehicleScalar{30.0}
        + result.lowerHookMm * VehicleScalar{12.0}
        + result.staticAsymmetryMm * VehicleScalar{2.0}
        + (flatSupportScenario
            ? std::max(result.footprintHeightRangeMm - VehicleScalar{3.0}, VehicleScalar{0.0})
                * VehicleScalar{0.5}
            : VehicleScalar{0.0})
        + result.rmsVelocityMps * VehicleScalar{100.0}
        + std::max(result.maximumDisplacementMm - VehicleScalar{100.0}, VehicleScalar{0.0})
            * VehicleScalar{0.1};
    result.valid = true;
    return result;
}

TireFlexibleRingDevelopmentTuning tireCarcassDevelopmentTrialTuning(
    const TireFlexibleRingDevelopmentTuning& base,
    std::uint64_t seed,
    std::uint64_t trialIndex,
    VehicleScalar spread,
    std::uint32_t groupMask)
{
    TireFlexibleRingDevelopmentTuning candidate = base;
    candidate.enabled = true;
    const VehicleScalar amount = std::clamp(spread, VehicleScalar{0.0}, VehicleScalar{1.0});
    for (std::size_t index = 0; index < tireCarcassDevelopmentParameterCount(); ++index)
    {
        TireCarcassDevelopmentParameterInfo info;
        if (!tireCarcassDevelopmentParameterInfo(index, info))
            continue;
        if ((groupMask & (std::uint32_t{1} << info.groupIndex)) == 0)
            continue;
        const VehicleScalar baseValue = tireCarcassDevelopmentParameterValue(base, index);
        const VehicleScalar u = unitRandom(seed ^ trialIndex, index + 1);
        const VehicleScalar halfRange = (info.maximum - info.minimum)
            * VehicleScalar{0.5} * amount;
        VehicleScalar value = baseValue + (u * VehicleScalar{2.0} - VehicleScalar{1.0})
            * halfRange;
        value = std::clamp(value, info.minimum, info.maximum);
        if (info.integer)
            value = std::round(value);
        setTireCarcassDevelopmentParameterValue(candidate, index, value);
    }
    return candidate;
}

TireCarcassSearchBatchResult runTireCarcassSearchBatch(
    const TireCarcassSyntheticInput& input,
    const TireFlexibleRingDevelopmentTuning& base,
    TireCarcassSyntheticScenario scenario,
    std::uint64_t seed,
    std::uint64_t firstTrialIndex,
    std::size_t trialCount,
    VehicleScalar spread,
    std::uint32_t groupMask)
{
    TireCarcassSearchBatchResult result;
    VehicleScalar best = std::numeric_limits<VehicleScalar>::infinity();
    const std::size_t count = std::clamp<std::size_t>(trialCount, 1, 256);
    const auto started = std::chrono::steady_clock::now();
    for (std::size_t offset = 0; offset < count; ++offset)
    {
        const std::uint64_t trial = firstTrialIndex + offset;
        const auto candidate = tireCarcassDevelopmentTrialTuning(
            base, seed, trial, spread, groupMask);
        const auto scenarioResult = runTireCarcassSyntheticScenario(
            input, candidate, scenario);
        if (!scenarioResult.valid || scenarioResult.pathologyScore >= best)
            continue;
        best = scenarioResult.pathologyScore;
        result.valid = true;
        result.bestTrialIndex = trial;
        result.bestScore = best;
        result.bestScenario = scenarioResult;
    }
    result.evaluatedCount = count;
    result.elapsedSeconds = static_cast<VehicleScalar>(
        std::chrono::duration<double>(
            std::chrono::steady_clock::now() - started).count());
    return result;
}

} // namespace heritage::vehicles::tires

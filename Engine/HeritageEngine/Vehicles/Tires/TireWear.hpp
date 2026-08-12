#pragma once

#include "../VehiclePrecision.hpp"
#include "TireThermal.hpp"

#include <array>
#include <cstddef>

namespace heritage::vehicles::tires {

// TIRE08 spatial tread state. Heritage deliberately keeps this independent
// from the proprietary MF-Tyre/MF-Swift internals: the MF solver still runs
// once per tire while these 16 circumferential x 3 lateral cells retain cheap
// local surface-temperature and wear history.
inline constexpr std::size_t kTireTreadSectorCount = 16;
inline constexpr std::size_t kTireTreadBandCount = 3;
inline constexpr std::size_t kTireTreadCellCount =
    kTireTreadSectorCount * kTireTreadBandCount;

enum class TireTreadBand : std::size_t
{
    Inside = 0,
    Center = 1,
    Outside = 2
};

struct TireWearDescription
{
    bool enabled = false;

    VehicleScalar initialTreadDepthM = 0.0070;
    VehicleScalar minimumTreadDepthM = 0.0005;

    // Abrasion coefficient [m/J] applied to local dissipated slip energy.
    // This is an authoring/fitting parameter, not an MF coefficient.
    VehicleScalar wearDepthPerJoule = 1.0e-9;
    VehicleScalar wearLoadExponent = 0.30;
    VehicleScalar wearTemperatureSensitivityPerC = 0.018;
    VehicleScalar minimumWearTemperatureScale = 0.35;
    VehicleScalar maximumWearTemperatureScale = 6.0;

    // Relative tendency to shed visible loose rubber under otherwise equal
    // stress. This is explicit tire authoring data (compound/construction/
    // tread behavior), not inferred from brand or peak friction.
    VehicleScalar rubberSheddingPropensity = 1.0;

    // A thin tread-surface layer retains circumferential/lateral hot spots.
    // The bulk tread/carcass/gas energy remains owned by TIRE07.
    VehicleScalar surfaceHeatCapacityJPerKPerCell = 24.0;
    VehicleScalar surfaceSlipHeatFraction = 0.18;
    VehicleScalar surfaceToBulkRelaxationHz = 0.20;
    VehicleScalar circumferentialDiffusionHz = 0.08;
    VehicleScalar lateralDiffusionHz = 0.12;
    VehicleScalar maximumSurfaceOffsetC = 120.0;

    // Three-band footprint pressure distribution. Pressure shifts load toward
    // the centre or shoulders; camber shifts it from one shoulder to the other.
    VehicleScalar baseCenterLoadFraction = 0.40;
    VehicleScalar pressureCenterBiasGain = 0.25;
    VehicleScalar camberShoulderBiasPerRad = 0.90;
    VehicleScalar maximumShoulderBias = 0.30;

    // Dry-road wear response. Early wear is intentionally subtle; near-cord
    // tread loss progressively reduces the effective contact friction.
    VehicleScalar maximumWearFrictionLoss = 0.18;
    VehicleScalar wearFrictionExponent = 3.0;

    // A local flat spot can also reduce the current contact scale slightly.
    // TIRE10 converts the same local wear field into physical rolling-radius variation.
    VehicleScalar flatSpotFrictionLossPerMm = 0.015;
    VehicleScalar maximumFlatSpotFrictionLoss = 0.12;
};

struct TireTreadCellState
{
    // Temperature offset relative to TIRE07's shared bulk-tread temperature.
    VehicleScalar surfaceTemperatureOffsetC = 0.0;
    VehicleScalar remainingTreadDepthM = 0.0070;

    // TIRE11 material pickup carried by this exact tread cell. Values are
    // normalized surface-film/load fractions in [0,1], not kilograms and not
    // extra MF solvers. The currently contacting cells are blended into one
    // contact modifier by TireSurfaceInteraction.
    VehicleScalar organicContamination = 0.0;
    VehicleScalar mineralContamination = 0.0;
    VehicleScalar gravelFinesContamination = 0.0;
    VehicleScalar rubberPickupContamination = 0.0;
    VehicleScalar mudFilmContamination = 0.0;

    // TIRE12 retained water film carried by this material-fixed tread cell.
    // Road water depth remains an external surface state; this small local
    // film provides spatial wet history after leaving a puddle.
    VehicleScalar retainedWaterFilmM = 0.0;

    // TIRE13 compacted-snow packing retained by this material-fixed tread
    // cell. This is a normalized groove/block packing state, not terrain
    // sinkage; deep snow remains a later terramechanics problem.
    VehicleScalar packedSnowFraction = 0.0;
};

struct TireWearState
{
    bool initialized = false;
    std::array<TireTreadCellState, kTireTreadCellCount> cells{};
    VehicleScalar diffusionAccumulatorSeconds = 0.0;
};

struct TireWearInput
{
    bool grounded = false;
    VehicleScalar wheelRotationDegrees = 0.0;
    VehicleScalar normalLoadN = 0.0;
    VehicleScalar nominalLoadN = 3500.0;
    VehicleScalar camberAngleRadians = 0.0;
    VehicleScalar inflationPressurePa = 220000.0;
    VehicleScalar referencePressurePa = 220000.0;
    VehicleScalar bulkTreadTemperatureC = 20.0;
    VehicleScalar slipDissipationWatts = 0.0;

    // Development-only wear acceleration. It multiplies abrasion energy, not
    // tread heating/diffusion, so a 1000x Tire Lab test does not pretend the
    // tire has experienced 1000x the thermal time. Normal simulation is 1x.
    VehicleScalar wearEnergyMultiplier = 1.0;
};


struct TireTreadContactWeights
{
    std::size_t primarySector = 0;
    std::size_t secondarySector = 1;
    VehicleScalar primaryWeight = 1.0;
    VehicleScalar secondaryWeight = 0.0;
    std::array<VehicleScalar, kTireTreadBandCount> bands{ 0.30, 0.40, 0.30 };
};

struct TireWearOutput
{
    bool valid = false;
    std::size_t primaryContactSector = 0;
    VehicleScalar secondaryContactWeight = 0.0;
    VehicleScalar insideLoadFraction = 0.30;
    VehicleScalar centerLoadFraction = 0.40;
    VehicleScalar outsideLoadFraction = 0.30;

    VehicleScalar insideSurfaceTemperatureC = 20.0;
    VehicleScalar centerSurfaceTemperatureC = 20.0;
    VehicleScalar outsideSurfaceTemperatureC = 20.0;
    VehicleScalar hottestSurfaceTemperatureC = 20.0;
    std::size_t hottestSector = 0;
    std::size_t hottestBand = 0;

    VehicleScalar insideAverageTreadDepthM = 0.0070;
    VehicleScalar centerAverageTreadDepthM = 0.0070;
    VehicleScalar outsideAverageTreadDepthM = 0.0070;
    VehicleScalar averageTreadDepthM = 0.0070;
    VehicleScalar minimumTreadDepthM = 0.0070;
    VehicleScalar wearFraction = 0.0;
    VehicleScalar flatSpotDepthM = 0.0;
    std::size_t flatSpotSector = 0;
    std::size_t flatSpotBand = 0;

    // TIRE10 physical rolling-radius state derived from the same 48-cell
    // tread history. Uniform wear lowers the whole tire; local circumferential
    // non-uniformity produces the radius variation that excites the unsprung
    // mass as the wheel rotates. These are geometric state, not extra forces.
    VehicleScalar averageTreadRadiusLossM = 0.0;
    VehicleScalar contactTreadRadiusLossM = 0.0;
    VehicleScalar contactRadiusVariationM = 0.0;

    // Ratio applied on top of TIRE07's bulk thermal friction scale.
    VehicleScalar contactTemperatureFrictionScale = 1.0;
    VehicleScalar contactWearFrictionScale = 1.0;
    VehicleScalar contactFrictionScale = 1.0;
};

bool validTireWearDescription(const TireWearDescription& value);

std::size_t tireTreadCellIndex(std::size_t sector, std::size_t band);

// Shared 16x3 contact weighting used by wear, contamination and later water
// state so the spatial tire layers cannot disagree about which material-fixed
// cells are under the footprint.
TireTreadContactWeights tireTreadContactWeights(
    const TireWearDescription& description,
    const TireWearInput& input);

TireWearOutput evaluateTireWearState(
    const TireWearDescription& description,
    const TireThermalDescription& thermalDescription,
    const TireWearInput& input,
    const TireWearState& state);

TireWearOutput advanceTireWear(
    const TireWearDescription& description,
    const TireThermalDescription& thermalDescription,
    const TireWearInput& input,
    VehicleScalar deltaTimeSeconds,
    TireWearState& state);

} // namespace heritage::vehicles::tires

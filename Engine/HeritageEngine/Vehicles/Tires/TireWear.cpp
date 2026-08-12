#include "TireWear.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::vehicles::tires {
namespace {

constexpr VehicleScalar kPi = 3.14159265358979323846;
constexpr VehicleScalar kEpsilon = 1.0e-12;

bool finiteValue(VehicleScalar value)
{
    return std::isfinite(static_cast<double>(value));
}

VehicleScalar clamp01(VehicleScalar value)
{
    return std::clamp(value, VehicleScalar{0.0}, VehicleScalar{1.0});
}

void initializeState(const TireWearDescription& d, TireWearState& state)
{
    state.initialized = true;
    state.diffusionAccumulatorSeconds = 0.0;
    for (auto& cell : state.cells)
    {
        cell.surfaceTemperatureOffsetC = 0.0;
        cell.remainingTreadDepthM = d.initialTreadDepthM;
        cell.organicContamination = 0.0;
        cell.mineralContamination = 0.0;
        cell.gravelFinesContamination = 0.0;
        cell.rubberPickupContamination = 0.0;
        cell.mudFilmContamination = 0.0;
    }
}

TireTreadContactWeights contactWeightsImpl(
    const TireWearDescription& d,
    const TireWearInput& input)
{
    TireTreadContactWeights out;

    VehicleScalar wrapped = std::fmod(input.wheelRotationDegrees, VehicleScalar{360.0});
    if (wrapped < 0.0)
        wrapped += 360.0;
    const VehicleScalar sectorPosition = wrapped
        / (VehicleScalar{360.0} / static_cast<VehicleScalar>(kTireTreadSectorCount));
    const VehicleScalar sectorFloor = std::floor(sectorPosition);
    out.primarySector = static_cast<std::size_t>(sectorFloor)
        % kTireTreadSectorCount;
    out.secondarySector = (out.primarySector + 1) % kTireTreadSectorCount;
    out.secondaryWeight = clamp01(sectorPosition - sectorFloor);
    out.primaryWeight = VehicleScalar{1.0} - out.secondaryWeight;

    const VehicleScalar referencePressure = std::max(
        input.referencePressurePa, VehicleScalar{30000.0});
    const VehicleScalar pressureRatio = std::clamp(
        input.inflationPressurePa / referencePressure,
        VehicleScalar{0.50}, VehicleScalar{1.75});
    const VehicleScalar centreDelta = d.pressureCenterBiasGain
        * (pressureRatio - VehicleScalar{1.0});
    VehicleScalar centre = std::clamp(
        d.baseCenterLoadFraction + centreDelta,
        VehicleScalar{0.15}, VehicleScalar{0.70});
    VehicleScalar shoulderTotal = VehicleScalar{1.0} - centre;

    const VehicleScalar camberBias = std::clamp(
        d.camberShoulderBiasPerRad * input.camberAngleRadians,
        -d.maximumShoulderBias,
        d.maximumShoulderBias);
    VehicleScalar inside = shoulderTotal * VehicleScalar{0.5} - camberBias;
    VehicleScalar outside = shoulderTotal * VehicleScalar{0.5} + camberBias;
    inside = std::max(inside, VehicleScalar{0.02});
    outside = std::max(outside, VehicleScalar{0.02});
    const VehicleScalar sum = std::max(
        inside + centre + outside, VehicleScalar{0.01});
    out.bands = { inside / sum, centre / sum, outside / sum };
    return out;
}

VehicleScalar localTemperatureC(
    const TireWearInput& input,
    const TireTreadCellState& cell)
{
    return input.bulkTreadTemperatureC + cell.surfaceTemperatureOffsetC;
}

VehicleScalar wearFraction(
    const TireWearDescription& d,
    VehicleScalar remainingDepthM)
{
    const VehicleScalar usable = std::max(
        d.initialTreadDepthM - d.minimumTreadDepthM,
        VehicleScalar{1.0e-6});
    return clamp01((d.initialTreadDepthM - remainingDepthM) / usable);
}

VehicleScalar cellWearTemperatureScale(
    const TireWearDescription& d,
    const TireThermalDescription& thermal,
    VehicleScalar temperatureC)
{
    const VehicleScalar reference = thermal.enabled
        ? thermal.optimumTreadTemperatureC
        : VehicleScalar{70.0};
    const VehicleScalar exponent = d.wearTemperatureSensitivityPerC
        * (temperatureC - reference);
    return std::clamp(
        std::exp(std::clamp(exponent, VehicleScalar{-5.0}, VehicleScalar{5.0})),
        d.minimumWearTemperatureScale,
        d.maximumWearTemperatureScale);
}

void normalizeSurfaceOffsets(TireWearState& state, VehicleScalar maximumOffsetC)
{
    VehicleScalar mean = 0.0;
    for (const auto& cell : state.cells)
        mean += cell.surfaceTemperatureOffsetC;
    mean /= static_cast<VehicleScalar>(kTireTreadCellCount);
    for (auto& cell : state.cells)
    {
        cell.surfaceTemperatureOffsetC = std::clamp(
            cell.surfaceTemperatureOffsetC - mean,
            -maximumOffsetC,
            maximumOffsetC);
    }
}

void diffuseSurfaceOffsets(
    const TireWearDescription& d,
    VehicleScalar dt,
    TireWearState& state)
{
    if (dt <= 0.0)
        return;

    std::array<VehicleScalar, kTireTreadCellCount> next{};
    const VehicleScalar bulkDecay = std::exp(
        -VehicleScalar{2.0} * kPi * d.surfaceToBulkRelaxationHz * dt);
    const VehicleScalar circumMix = clamp01(
        VehicleScalar{1.0} - std::exp(
            -VehicleScalar{2.0} * kPi * d.circumferentialDiffusionHz * dt));
    const VehicleScalar lateralMix = clamp01(
        VehicleScalar{1.0} - std::exp(
            -VehicleScalar{2.0} * kPi * d.lateralDiffusionHz * dt));

    for (std::size_t sector = 0; sector < kTireTreadSectorCount; ++sector)
    {
        const std::size_t previousSector =
            (sector + kTireTreadSectorCount - 1) % kTireTreadSectorCount;
        const std::size_t nextSector = (sector + 1) % kTireTreadSectorCount;
        for (std::size_t band = 0; band < kTireTreadBandCount; ++band)
        {
            const std::size_t index = tireTreadCellIndex(sector, band);
            VehicleScalar value = state.cells[index].surfaceTemperatureOffsetC
                * bulkDecay;
            const VehicleScalar around = VehicleScalar{0.5} * (
                state.cells[tireTreadCellIndex(previousSector, band)].surfaceTemperatureOffsetC
                + state.cells[tireTreadCellIndex(nextSector, band)].surfaceTemperatureOffsetC);
            value += circumMix * (around - value);

            VehicleScalar lateralAverage = value;
            VehicleScalar lateralCount = 1.0;
            if (band > 0)
            {
                lateralAverage += state.cells[tireTreadCellIndex(sector, band - 1)]
                    .surfaceTemperatureOffsetC;
                lateralCount += 1.0;
            }
            if (band + 1 < kTireTreadBandCount)
            {
                lateralAverage += state.cells[tireTreadCellIndex(sector, band + 1)]
                    .surfaceTemperatureOffsetC;
                lateralCount += 1.0;
            }
            lateralAverage /= lateralCount;
            value += lateralMix * (lateralAverage - value);
            next[index] = std::clamp(
                value, -d.maximumSurfaceOffsetC, d.maximumSurfaceOffsetC);
        }
    }

    for (std::size_t i = 0; i < kTireTreadCellCount; ++i)
        state.cells[i].surfaceTemperatureOffsetC = next[i];
    normalizeSurfaceOffsets(state, d.maximumSurfaceOffsetC);
}

TireWearOutput outputFromState(
    const TireWearDescription& d,
    const TireThermalDescription& thermal,
    const TireWearInput& input,
    const TireWearState& state)
{
    TireWearOutput out;
    if (!d.enabled)
        return out;

    TireWearState readable = state;
    if (!readable.initialized)
        initializeState(d, readable);

    const TireTreadContactWeights weights = contactWeightsImpl(d, input);
    out.valid = true;
    out.primaryContactSector = weights.primarySector;
    out.secondaryContactWeight = weights.secondaryWeight;
    out.insideLoadFraction = weights.bands[0];
    out.centerLoadFraction = weights.bands[1];
    out.outsideLoadFraction = weights.bands[2];

    std::array<VehicleScalar, kTireTreadBandCount> temperatureSum{};
    std::array<VehicleScalar, kTireTreadBandCount> depthSum{};
    std::array<VehicleScalar, kTireTreadBandCount> minimumDepth{};
    minimumDepth.fill(std::numeric_limits<VehicleScalar>::max());

    out.hottestSurfaceTemperatureC = -std::numeric_limits<VehicleScalar>::max();
    out.minimumTreadDepthM = std::numeric_limits<VehicleScalar>::max();
    VehicleScalar depthTotal = 0.0;

    for (std::size_t sector = 0; sector < kTireTreadSectorCount; ++sector)
    {
        for (std::size_t band = 0; band < kTireTreadBandCount; ++band)
        {
            const auto& cell = readable.cells[tireTreadCellIndex(sector, band)];
            const VehicleScalar temperature = localTemperatureC(input, cell);
            temperatureSum[band] += temperature;
            depthSum[band] += cell.remainingTreadDepthM;
            minimumDepth[band] = std::min(minimumDepth[band], cell.remainingTreadDepthM);
            depthTotal += cell.remainingTreadDepthM;
            out.minimumTreadDepthM = std::min(
                out.minimumTreadDepthM, cell.remainingTreadDepthM);
            if (temperature > out.hottestSurfaceTemperatureC)
            {
                out.hottestSurfaceTemperatureC = temperature;
                out.hottestSector = sector;
                out.hottestBand = band;
            }
        }
    }

    const VehicleScalar sectors = static_cast<VehicleScalar>(kTireTreadSectorCount);
    out.insideSurfaceTemperatureC = temperatureSum[0] / sectors;
    out.centerSurfaceTemperatureC = temperatureSum[1] / sectors;
    out.outsideSurfaceTemperatureC = temperatureSum[2] / sectors;
    out.insideAverageTreadDepthM = depthSum[0] / sectors;
    out.centerAverageTreadDepthM = depthSum[1] / sectors;
    out.outsideAverageTreadDepthM = depthSum[2] / sectors;
    out.averageTreadDepthM = depthTotal
        / static_cast<VehicleScalar>(kTireTreadCellCount);
    out.wearFraction = wearFraction(d, out.averageTreadDepthM);
    out.averageTreadRadiusLossM = std::clamp(
        d.initialTreadDepthM - out.averageTreadDepthM,
        VehicleScalar{0.0},
        std::max(d.initialTreadDepthM - d.minimumTreadDepthM, VehicleScalar{0.0}));

    // The flat-spot datum is circumferential non-uniformity, not simply the
    // globally thinnest tread cell. Compare every cell to its own lateral
    // band's average so steady shoulder wear from camber does not masquerade
    // as a braking flat spot. TIRE09 visual deformation uses the resulting
    // material-fixed sector to drive both the same visual dent and TIRE10 physical radius variation.
    for (std::size_t band = 0; band < kTireTreadBandCount; ++band)
    {
        const VehicleScalar bandAverage = depthSum[band] / sectors;
        for (std::size_t sector = 0; sector < kTireTreadSectorCount; ++sector)
        {
            const auto& cell = readable.cells[tireTreadCellIndex(sector, band)];
            const VehicleScalar deficit = std::max(
                bandAverage - cell.remainingTreadDepthM, VehicleScalar{0.0});
            if (deficit > out.flatSpotDepthM)
            {
                out.flatSpotDepthM = deficit;
                out.flatSpotSector = sector;
                out.flatSpotBand = band;
            }
        }
    }

    VehicleScalar localTemperature = 0.0;
    VehicleScalar localDepth = 0.0;
    for (std::size_t band = 0; band < kTireTreadBandCount; ++band)
    {
        const VehicleScalar bandWeight = weights.bands[band];
        const auto& primary = readable.cells[
            tireTreadCellIndex(weights.primarySector, band)];
        const auto& secondary = readable.cells[
            tireTreadCellIndex(weights.secondarySector, band)];
        const VehicleScalar sectorTemperature =
            localTemperatureC(input, primary) * weights.primaryWeight
            + localTemperatureC(input, secondary) * weights.secondaryWeight;
        const VehicleScalar sectorDepth =
            primary.remainingTreadDepthM * weights.primaryWeight
            + secondary.remainingTreadDepthM * weights.secondaryWeight;
        localTemperature += bandWeight * sectorTemperature;
        localDepth += bandWeight * sectorDepth;
    }

    if (thermal.enabled && validTireThermalDescription(thermal))
    {
        const VehicleScalar localScale = tireThermalFrictionScaleForTemperature(
            thermal, localTemperature);
        const VehicleScalar bulkScale = tireThermalFrictionScaleForTemperature(
            thermal, input.bulkTreadTemperatureC);
        out.contactTemperatureFrictionScale = std::clamp(
            localScale / std::max(bulkScale, VehicleScalar{0.05}),
            VehicleScalar{0.75}, VehicleScalar{1.20});
    }

    out.contactTreadRadiusLossM = std::clamp(
        d.initialTreadDepthM - localDepth,
        VehicleScalar{0.0},
        std::max(d.initialTreadDepthM - d.minimumTreadDepthM, VehicleScalar{0.0}));
    out.contactRadiusVariationM =
        out.contactTreadRadiusLossM - out.averageTreadRadiusLossM;

    const VehicleScalar localWear = wearFraction(d, localDepth);
    out.contactWearFrictionScale = std::clamp(
        VehicleScalar{1.0} - d.maximumWearFrictionLoss
            * std::pow(localWear, std::max(d.wearFrictionExponent, VehicleScalar{0.1})),
        VehicleScalar{0.60}, VehicleScalar{1.0});
    const VehicleScalar flatSpotLoss = std::clamp(
        d.flatSpotFrictionLossPerMm * out.flatSpotDepthM * VehicleScalar{1000.0},
        VehicleScalar{0.0}, d.maximumFlatSpotFrictionLoss);
    out.contactWearFrictionScale *= VehicleScalar{1.0} - flatSpotLoss;
    out.contactFrictionScale = std::clamp(
        out.contactTemperatureFrictionScale * out.contactWearFrictionScale,
        VehicleScalar{0.50}, VehicleScalar{1.20});
    return out;
}

} // namespace

bool validTireWearDescription(const TireWearDescription& d)
{
    if (!d.enabled)
        return true;
    const VehicleScalar values[] = {
        d.initialTreadDepthM,
        d.minimumTreadDepthM,
        d.wearDepthPerJoule,
        d.wearLoadExponent,
        d.wearTemperatureSensitivityPerC,
        d.minimumWearTemperatureScale,
        d.maximumWearTemperatureScale,
        d.rubberSheddingPropensity,
        d.surfaceHeatCapacityJPerKPerCell,
        d.surfaceSlipHeatFraction,
        d.surfaceToBulkRelaxationHz,
        d.circumferentialDiffusionHz,
        d.lateralDiffusionHz,
        d.maximumSurfaceOffsetC,
        d.baseCenterLoadFraction,
        d.pressureCenterBiasGain,
        d.camberShoulderBiasPerRad,
        d.maximumShoulderBias,
        d.maximumWearFrictionLoss,
        d.wearFrictionExponent,
        d.flatSpotFrictionLossPerMm,
        d.maximumFlatSpotFrictionLoss
    };
    for (VehicleScalar value : values)
    {
        if (!finiteValue(value))
            return false;
    }
    return d.initialTreadDepthM > 0.001
        && d.minimumTreadDepthM >= 0.0
        && d.minimumTreadDepthM < d.initialTreadDepthM
        && d.wearDepthPerJoule >= 0.0
        && d.wearLoadExponent >= 0.0
        && d.minimumWearTemperatureScale > 0.0
        && d.maximumWearTemperatureScale >= d.minimumWearTemperatureScale
        && d.rubberSheddingPropensity >= 0.0
        && d.rubberSheddingPropensity <= 3.0
        && d.surfaceHeatCapacityJPerKPerCell > 0.1
        && d.surfaceSlipHeatFraction >= 0.0
        && d.surfaceSlipHeatFraction <= 1.0
        && d.surfaceToBulkRelaxationHz >= 0.0
        && d.circumferentialDiffusionHz >= 0.0
        && d.lateralDiffusionHz >= 0.0
        && d.maximumSurfaceOffsetC > 1.0
        && d.baseCenterLoadFraction > 0.05
        && d.baseCenterLoadFraction < 0.90
        && d.maximumShoulderBias >= 0.0
        && d.maximumShoulderBias <= 0.45
        && d.maximumWearFrictionLoss >= 0.0
        && d.maximumWearFrictionLoss < 0.80
        && d.wearFrictionExponent > 0.0
        && d.maximumFlatSpotFrictionLoss >= 0.0
        && d.maximumFlatSpotFrictionLoss < 0.80;
}

std::size_t tireTreadCellIndex(std::size_t sector, std::size_t band)
{
    return (sector % kTireTreadSectorCount) * kTireTreadBandCount
        + (band % kTireTreadBandCount);
}

TireTreadContactWeights tireTreadContactWeights(
    const TireWearDescription& description,
    const TireWearInput& input)
{
    return contactWeightsImpl(description, input);
}

TireWearOutput evaluateTireWearState(
    const TireWearDescription& description,
    const TireThermalDescription& thermalDescription,
    const TireWearInput& input,
    const TireWearState& state)
{
    if (!validTireWearDescription(description))
        return {};
    return outputFromState(description, thermalDescription, input, state);
}

TireWearOutput advanceTireWear(
    const TireWearDescription& d,
    const TireThermalDescription& thermal,
    const TireWearInput& input,
    VehicleScalar dt,
    TireWearState& state)
{
    if (!d.enabled || !validTireWearDescription(d)
        || !finiteValue(dt) || dt <= 0.0)
    {
        return {};
    }
    if (!state.initialized)
        initializeState(d, state);
    dt = std::min(dt, VehicleScalar{0.05});

    const TireTreadContactWeights weights = contactWeightsImpl(d, input);
    if (input.grounded && input.normalLoadN > 1.0
        && input.slipDissipationWatts > 0.0)
    {
        const VehicleScalar surfaceEnergyJ = input.slipDissipationWatts
            * d.surfaceSlipHeatFraction * dt;
        const VehicleScalar totalSlipEnergyJ = input.slipDissipationWatts * dt;
        const VehicleScalar loadScale = std::pow(
            std::max(input.normalLoadN
                / std::max(input.nominalLoadN, VehicleScalar{1.0}), VehicleScalar{0.05}),
            d.wearLoadExponent);

        const std::array<std::pair<std::size_t, VehicleScalar>, 2> sectors{{
            {weights.primarySector, weights.primaryWeight},
            {weights.secondarySector, weights.secondaryWeight}
        }};
        for (const auto& sectorWeight : sectors)
        {
            if (sectorWeight.second <= 0.0)
                continue;
            for (std::size_t band = 0; band < kTireTreadBandCount; ++band)
            {
                const VehicleScalar weight = sectorWeight.second * weights.bands[band];
                if (weight <= 0.0)
                    continue;
                auto& cell = state.cells[tireTreadCellIndex(sectorWeight.first, band)];
                const VehicleScalar cellEnergyJ = surfaceEnergyJ * weight;
                cell.surfaceTemperatureOffsetC += cellEnergyJ
                    / std::max(d.surfaceHeatCapacityJPerKPerCell, kEpsilon);

                const VehicleScalar temperature = localTemperatureC(input, cell);
                const VehicleScalar temperatureScale = cellWearTemperatureScale(
                    d, thermal, temperature);
                const VehicleScalar wearMultiplier = std::clamp(
                    finiteValue(input.wearEnergyMultiplier)
                        ? input.wearEnergyMultiplier : VehicleScalar{1.0},
                    VehicleScalar{0.0}, VehicleScalar{1000.0});
                const VehicleScalar wearEnergyJ = totalSlipEnergyJ * weight
                    * wearMultiplier;
                cell.remainingTreadDepthM = std::max(
                    d.minimumTreadDepthM,
                    cell.remainingTreadDepthM
                        - d.wearDepthPerJoule * wearEnergyJ
                            * loadScale * temperatureScale);
            }
        }

        // TIRE07 owns the mean tread energy. Preserve only the spatial
        // temperature deviation here so local hot spots don't double-count it.
        normalizeSurfaceOffsets(state, d.maximumSurfaceOffsetC);
    }

    state.diffusionAccumulatorSeconds += dt;
    if (state.diffusionAccumulatorSeconds >= VehicleScalar{0.004})
    {
        const VehicleScalar diffusionDt = std::min(
            state.diffusionAccumulatorSeconds, VehicleScalar{0.050});
        state.diffusionAccumulatorSeconds = 0.0;
        diffuseSurfaceOffsets(d, diffusionDt, state);
    }

    return outputFromState(d, thermal, input, state);
}

} // namespace heritage::vehicles::tires

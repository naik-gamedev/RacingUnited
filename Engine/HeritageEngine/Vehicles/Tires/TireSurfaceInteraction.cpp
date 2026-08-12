#include "TireSurfaceInteraction.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace heritage::vehicles::tires {
namespace {

bool finiteValue(VehicleScalar value)
{
    return std::isfinite(static_cast<double>(value));
}

VehicleScalar clamp01(VehicleScalar value)
{
    return std::clamp(value, VehicleScalar{0.0}, VehicleScalar{1.0});
}

struct MaterialAvailability
{
    VehicleScalar organic = 0.0;
    VehicleScalar mineral = 0.0;
    VehicleScalar gravel = 0.0;
    VehicleScalar rubber = 0.0;
    VehicleScalar mud = 0.0;
    VehicleScalar cleanHardFraction = 0.0;
};

MaterialAvailability materialAvailability(const TireContaminationInput& input)
{
    using heritage::physics::SurfaceMaterial;
    VehicleScalar wet = clamp01(input.surfaceWetness);
    MaterialAvailability out;
    out.rubber = clamp01(input.surfaceRubberDebrisFraction);

    if (input.footprintSurfaceBlendValid)
    {
        const VehicleScalar grass = clamp01(input.footprintGrassFraction);
        const VehicleScalar dirt = clamp01(input.footprintDirtFraction);
        const VehicleScalar gravel = clamp01(input.footprintGravelFraction);
        const VehicleScalar mudTerrain = clamp01(input.footprintMudFraction);
        const VehicleScalar sand = clamp01(input.footprintSandFraction);
        const VehicleScalar softSoil = clamp01(input.footprintSoftSoilFraction);
        const VehicleScalar deepSnow = clamp01(input.footprintDeepSnowFraction);
        out.cleanHardFraction = clamp01(input.footprintCleanHardFraction);
        wet = clamp01(input.footprintAverageWetness);

        out.organic = grass * (VehicleScalar{0.70} + VehicleScalar{0.30} * wet);
        out.mineral = dirt * VehicleScalar{0.90}
                * (VehicleScalar{1.0} - VehicleScalar{0.55} * wet)
            + gravel * VehicleScalar{0.42}
                * (VehicleScalar{1.0} - VehicleScalar{0.35} * wet)
            + sand * VehicleScalar{0.92}
            + softSoil * VehicleScalar{0.75}
                * (VehicleScalar{1.0} - VehicleScalar{0.55} * wet)
            + mudTerrain * VehicleScalar{0.20}
                * (VehicleScalar{1.0} - wet);
        out.gravel = gravel * VehicleScalar{0.92}
                * (VehicleScalar{1.0} - VehicleScalar{0.40} * wet)
            + sand * VehicleScalar{0.12};
        out.mud = grass * VehicleScalar{0.72} * wet
            + dirt * VehicleScalar{0.92} * wet
            + gravel * VehicleScalar{0.30} * wet
            + mudTerrain * VehicleScalar{0.95}
            + softSoil * VehicleScalar{0.82} * wet;
        // Deep snow is deliberately not smuggled into generic dirt/mud
        // contamination. Its packed-snow tread state belongs to the winter/
        // snow mechanisms even though TIRE15 owns its deformable support.
        (void)deepSnow;
        out.organic = clamp01(out.organic);
        out.mineral = clamp01(out.mineral);
        out.gravel = clamp01(out.gravel);
        out.mud = clamp01(out.mud);
        return out;
    }

    switch (input.surfaceMaterial)
    {
    case SurfaceMaterial::Grass:
        out.organic = VehicleScalar{0.70} + VehicleScalar{0.30} * wet;
        out.mineral = VehicleScalar{0.08} * (VehicleScalar{1.0} - wet);
        out.mud = VehicleScalar{0.72} * wet;
        break;
    case SurfaceMaterial::Dirt:
        out.mineral = VehicleScalar{0.90} * (VehicleScalar{1.0} - VehicleScalar{0.55} * wet);
        out.mud = VehicleScalar{0.92} * wet;
        break;
    case SurfaceMaterial::Gravel:
        out.gravel = VehicleScalar{0.92} * (VehicleScalar{1.0} - VehicleScalar{0.40} * wet);
        out.mineral = VehicleScalar{0.42} * (VehicleScalar{1.0} - VehicleScalar{0.35} * wet);
        out.mud = VehicleScalar{0.30} * wet;
        break;
    case SurfaceMaterial::Mud:
        out.mud = VehicleScalar{0.95};
        out.mineral = VehicleScalar{0.20} * (VehicleScalar{1.0} - wet);
        break;
    case SurfaceMaterial::Sand:
        out.mineral = VehicleScalar{0.92};
        out.gravel = VehicleScalar{0.12};
        break;
    case SurfaceMaterial::SoftSoil:
        out.mineral = VehicleScalar{0.75} * (VehicleScalar{1.0} - VehicleScalar{0.55} * wet);
        out.mud = VehicleScalar{0.82} * wet;
        break;
    case SurfaceMaterial::Asphalt:
    case SurfaceMaterial::Kerb:
    case SurfaceMaterial::PaintedLine:
        out.cleanHardFraction = 1.0;
        break;
    case SurfaceMaterial::Default:
        // Preserve legacy/default scenes as a clean hard surface. A future
        // dynamic surface layer can provide explicit material state instead.
        out.cleanHardFraction = 1.0;
        break;
    case SurfaceMaterial::Snow:
    case SurfaceMaterial::DeepSnow:
    case SurfaceMaterial::Ice:
        // TIRE13 will own snow/ice pickup/water-film state. Do not smuggle it
        // into the contamination model prematurely.
        break;
    }
    return out;
}

VehicleScalar contactSlipSpeed(const TireContaminationInput& input)
{
    return std::sqrt(
        input.longitudinalSlipVelocityMps * input.longitudinalSlipVelocityMps
        + input.lateralSlipVelocityMps * input.lateralSlipVelocityMps);
}

VehicleScalar pickupIntensity(const TireContaminationInput& input)
{
    const VehicleScalar loadRatio = std::clamp(
        input.normalLoadN / std::max(input.nominalLoadN, VehicleScalar{1.0}),
        VehicleScalar{0.05}, VehicleScalar{2.5});
    const VehicleScalar slip = std::min(contactSlipSpeed(input), VehicleScalar{8.0});
    return std::clamp(
        VehicleScalar{0.30} + VehicleScalar{0.55} * std::sqrt(loadRatio)
            + VehicleScalar{0.045} * slip,
        VehicleScalar{0.20}, VehicleScalar{1.55});
}

VehicleScalar cleaningRate(
    const TireContaminationDescription& d,
    const TireContaminationInput& input,
    VehicleScalar cleanHardFraction)
{
    cleanHardFraction = clamp01(cleanHardFraction);
    if (!input.grounded || cleanHardFraction <= 0.0)
        return 0.0;
    const VehicleScalar speed = std::min(std::abs(input.forwardSpeedMps), VehicleScalar{90.0});
    const VehicleScalar slip = std::min(contactSlipSpeed(input), VehicleScalar{15.0});
    const VehicleScalar hotExcess = std::max(
        input.bulkTreadTemperatureC - d.hotTreadCleaningThresholdC,
        VehicleScalar{0.0});
    return cleanHardFraction * std::max(
        d.baseHardSurfaceCleaningRateHz
        + d.speedCleaningRatePerM * speed
        + d.slipCleaningRatePerM * slip
        + d.hotTreadCleaningRatePerC * hotExcess,
        VehicleScalar{0.0});
}

VehicleScalar channelContactValue(
    const TireTreadContactWeights& weights,
    const TireWearState& state,
    VehicleScalar TireTreadCellState::* member)
{
    VehicleScalar out = 0.0;
    for (std::size_t band = 0; band < kTireTreadBandCount; ++band)
    {
        const auto& primary = state.cells[
            tireTreadCellIndex(weights.primarySector, band)];
        const auto& secondary = state.cells[
            tireTreadCellIndex(weights.secondarySector, band)];
        const VehicleScalar sectorValue =
            primary.*member * weights.primaryWeight
            + secondary.*member * weights.secondaryWeight;
        out += weights.bands[band] * sectorValue;
    }
    return clamp01(out);
}

VehicleScalar cellTotal(const TireTreadCellState& cell)
{
    // Saturating union: several thin films can coexist without a total > 1.
    const VehicleScalar remain =
        (VehicleScalar{1.0} - clamp01(cell.organicContamination))
        * (VehicleScalar{1.0} - clamp01(cell.mineralContamination))
        * (VehicleScalar{1.0} - clamp01(cell.gravelFinesContamination))
        * (VehicleScalar{1.0} - clamp01(cell.rubberPickupContamination))
        * (VehicleScalar{1.0} - clamp01(cell.mudFilmContamination));
    return clamp01(VehicleScalar{1.0} - remain);
}

TireWearInput wearInputForWeights(
    const TireContaminationInput& input)
{
    TireWearInput out;
    out.grounded = input.grounded;
    out.wheelRotationDegrees = input.wheelRotationDegrees;
    out.normalLoadN = input.normalLoadN;
    out.nominalLoadN = input.nominalLoadN;
    out.camberAngleRadians = input.camberAngleRadians;
    out.inflationPressurePa = input.inflationPressurePa;
    out.referencePressurePa = input.referencePressurePa;
    out.bulkTreadTemperatureC = input.bulkTreadTemperatureC;
    return out;
}

TireContaminationOutput outputFromState(
    const TireContaminationDescription& d,
    const TireWearDescription& wearDescription,
    const TireContaminationInput& input,
    const TireWearState& treadState)
{
    TireContaminationOutput out;
    if (!d.enabled || !treadState.initialized)
        return out;

    const TireTreadContactWeights weights = tireTreadContactWeights(
        wearDescription, wearInputForWeights(input));
    out.valid = true;
    out.primaryContactSector = weights.primarySector;
    out.contactOrganic = channelContactValue(
        weights, treadState, &TireTreadCellState::organicContamination);
    out.contactMineral = channelContactValue(
        weights, treadState, &TireTreadCellState::mineralContamination);
    out.contactGravelFines = channelContactValue(
        weights, treadState, &TireTreadCellState::gravelFinesContamination);
    out.contactRubberPickup = channelContactValue(
        weights, treadState, &TireTreadCellState::rubberPickupContamination);
    out.contactMudFilm = channelContactValue(
        weights, treadState, &TireTreadCellState::mudFilmContamination);

    const VehicleScalar remain =
        (VehicleScalar{1.0} - out.contactOrganic)
        * (VehicleScalar{1.0} - out.contactMineral)
        * (VehicleScalar{1.0} - out.contactGravelFines)
        * (VehicleScalar{1.0} - out.contactRubberPickup)
        * (VehicleScalar{1.0} - out.contactMudFilm);
    out.contactTotal = clamp01(VehicleScalar{1.0} - remain);

    VehicleScalar total = 0.0;
    for (std::size_t sector = 0; sector < kTireTreadSectorCount; ++sector)
    {
        for (std::size_t band = 0; band < kTireTreadBandCount; ++band)
        {
            const VehicleScalar value = cellTotal(
                treadState.cells[tireTreadCellIndex(sector, band)]);
            total += value;
            if (value > out.maximumCellTotal)
            {
                out.maximumCellTotal = value;
                out.dirtiestSector = sector;
                out.dirtiestBand = band;
            }
        }
    }
    out.averageTotal = total / static_cast<VehicleScalar>(kTireTreadCellCount);

    const VehicleScalar frictionLoss = std::clamp(
        d.organicMaximumFrictionLoss * out.contactOrganic
        + d.mineralMaximumFrictionLoss * out.contactMineral
        + d.gravelFinesMaximumFrictionLoss * out.contactGravelFines
        + d.rubberPickupMaximumFrictionLoss * out.contactRubberPickup
        + d.mudMaximumFrictionLoss * out.contactMudFilm,
        VehicleScalar{0.0}, d.maximumCombinedFrictionLoss);
    out.contactFrictionScale = VehicleScalar{1.0} - frictionLoss;

    const VehicleScalar insulation = std::clamp(
        d.organicRoadHeatInsulation * out.contactOrganic
        + d.mineralRoadHeatInsulation * out.contactMineral
        + d.gravelRoadHeatInsulation * out.contactGravelFines
        + d.rubberRoadHeatInsulation * out.contactRubberPickup
        + d.mudRoadHeatInsulation * out.contactMudFilm,
        VehicleScalar{0.0}, VehicleScalar{0.90});
    out.roadHeatTransferScale = std::clamp(
        VehicleScalar{1.0} - insulation,
        d.minimumRoadHeatTransferScale,
        VehicleScalar{1.0});

    out.rollingResistanceScale = std::clamp(
        VehicleScalar{1.0}
        + d.organicRollingResistanceGain * out.contactOrganic
        + d.mineralRollingResistanceGain * out.contactMineral
        + d.gravelRollingResistanceGain * out.contactGravelFines
        + d.rubberRollingResistanceGain * out.contactRubberPickup
        + d.mudRollingResistanceGain * out.contactMudFilm,
        VehicleScalar{1.0}, d.maximumRollingResistanceScale);

    const MaterialAvailability availability = materialAvailability(input);
    const VehicleScalar intensity = pickupIntensity(input);
    out.pickupRatePerSecond = std::max({
        d.grassOrganicPickupRateHz * availability.organic,
        d.dirtMineralPickupRateHz * availability.mineral,
        d.gravelFinesPickupRateHz * availability.gravel,
        d.rubberPickupRateHz * availability.rubber,
        d.mudFilmPickupRateHz * availability.mud
    }) * intensity;
    out.cleaningRatePerSecond = cleaningRate(
        d, input, availability.cleanHardFraction);
    return out;
}

void approachTarget(
    VehicleScalar target,
    VehicleScalar rateHz,
    VehicleScalar dt,
    VehicleScalar& value)
{
    target = clamp01(target);
    rateHz = std::max(rateHz, VehicleScalar{0.0});
    const VehicleScalar blend = VehicleScalar{1.0} - std::exp(-rateHz * dt);
    value = clamp01(value + (target - value) * blend);
}

void cleanChannel(
    VehicleScalar cleanRateHz,
    VehicleScalar retention,
    VehicleScalar dt,
    VehicleScalar& value)
{
    retention = std::clamp(retention, VehicleScalar{0.0}, VehicleScalar{0.995});
    // Retention slows release without making highly retentive organic/mud
    // films practically immortal. Cleaning is active only while that exact
    // material-fixed sector is in contact with a clean hard surface.
    const VehicleScalar retentionTimeScale = VehicleScalar{0.35}
        + VehicleScalar{1.65} * retention;
    const VehicleScalar effective = cleanRateHz
        / std::max(retentionTimeScale, VehicleScalar{0.1});
    value = std::max(VehicleScalar{0.0}, value * std::exp(-effective * dt));
}

} // namespace

bool validTireContaminationDescription(const TireContaminationDescription& d)
{
    if (!d.enabled)
        return true;
    const VehicleScalar values[] = {
        d.grassOrganicPickupRateHz, d.dirtMineralPickupRateHz,
        d.gravelFinesPickupRateHz, d.rubberPickupRateHz,
        d.mudFilmPickupRateHz, d.baseHardSurfaceCleaningRateHz,
        d.speedCleaningRatePerM, d.slipCleaningRatePerM,
        d.hotTreadCleaningRatePerC, d.hotTreadCleaningThresholdC,
        d.organicRetention, d.mineralRetention, d.gravelFinesRetention,
        d.rubberRetention, d.mudRetention,
        d.organicMaximumFrictionLoss, d.mineralMaximumFrictionLoss,
        d.gravelFinesMaximumFrictionLoss, d.rubberPickupMaximumFrictionLoss,
        d.mudMaximumFrictionLoss, d.maximumCombinedFrictionLoss,
        d.organicRoadHeatInsulation, d.mineralRoadHeatInsulation,
        d.gravelRoadHeatInsulation, d.rubberRoadHeatInsulation,
        d.mudRoadHeatInsulation, d.minimumRoadHeatTransferScale,
        d.organicRollingResistanceGain, d.mineralRollingResistanceGain,
        d.gravelRollingResistanceGain, d.rubberRollingResistanceGain,
        d.mudRollingResistanceGain, d.maximumRollingResistanceScale
    };
    for (VehicleScalar value : values)
        if (!finiteValue(value)) return false;

    return d.grassOrganicPickupRateHz >= 0.0
        && d.dirtMineralPickupRateHz >= 0.0
        && d.gravelFinesPickupRateHz >= 0.0
        && d.rubberPickupRateHz >= 0.0
        && d.mudFilmPickupRateHz >= 0.0
        && d.baseHardSurfaceCleaningRateHz >= 0.0
        && d.speedCleaningRatePerM >= 0.0
        && d.slipCleaningRatePerM >= 0.0
        && d.organicRetention >= 0.0 && d.organicRetention < 1.0
        && d.mineralRetention >= 0.0 && d.mineralRetention < 1.0
        && d.gravelFinesRetention >= 0.0 && d.gravelFinesRetention < 1.0
        && d.rubberRetention >= 0.0 && d.rubberRetention < 1.0
        && d.mudRetention >= 0.0 && d.mudRetention < 1.0
        && d.maximumCombinedFrictionLoss >= 0.0
        && d.maximumCombinedFrictionLoss < 0.90
        && d.minimumRoadHeatTransferScale > 0.05
        && d.minimumRoadHeatTransferScale <= 1.0
        && d.maximumRollingResistanceScale >= 1.0
        && d.maximumRollingResistanceScale <= 5.0;
}

TireContaminationOutput evaluateTireContamination(
    const TireContaminationDescription& description,
    const TireWearDescription& wearDescription,
    const TireContaminationInput& input,
    const TireWearState& treadState)
{
    if (!validTireContaminationDescription(description))
        return {};
    return outputFromState(description, wearDescription, input, treadState);
}

TireContaminationOutput advanceTireContamination(
    const TireContaminationDescription& d,
    const TireWearDescription& wearDescription,
    const TireContaminationInput& input,
    VehicleScalar dt,
    TireWearState& treadState)
{
    if (!d.enabled || !validTireContaminationDescription(d)
        || !finiteValue(dt) || dt <= 0.0)
    {
        return {};
    }
    if (!treadState.initialized)
    {
        // Keep initialization ownership in TIRE08 by making a harmless state
        // evaluation/advance establish tread depth before TIRE11 writes cells.
        TireThermalDescription thermal;
        TireWearInput init = wearInputForWeights(input);
        advanceTireWear(wearDescription, thermal, init,
            VehicleScalar{1.0e-9}, treadState);
    }

    dt = std::min(dt, VehicleScalar{0.05});
    const TireTreadContactWeights weights = tireTreadContactWeights(
        wearDescription, wearInputForWeights(input));
    const MaterialAvailability availability = materialAvailability(input);
    const VehicleScalar intensity = pickupIntensity(input);
    const VehicleScalar clean = cleaningRate(d, input, availability.cleanHardFraction);

    if (input.grounded && input.normalLoadN > 1.0)
    {
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
                const VehicleScalar contactWeight = sectorWeight.second * weights.bands[band];
                if (contactWeight <= 0.0)
                    continue;
                auto& cell = treadState.cells[
                    tireTreadCellIndex(sectorWeight.first, band)];

                if (availability.organic > 0.0)
                    approachTarget(availability.organic,
                        d.grassOrganicPickupRateHz * intensity * contactWeight,
                        dt, cell.organicContamination);
                if (availability.mineral > 0.0)
                    approachTarget(availability.mineral,
                        d.dirtMineralPickupRateHz * intensity * contactWeight,
                        dt, cell.mineralContamination);
                if (availability.gravel > 0.0)
                    approachTarget(availability.gravel,
                        d.gravelFinesPickupRateHz * intensity * contactWeight,
                        dt, cell.gravelFinesContamination);
                if (availability.rubber > 0.0)
                    approachTarget(availability.rubber,
                        d.rubberPickupRateHz * intensity * contactWeight,
                        dt, cell.rubberPickupContamination);
                if (availability.mud > 0.0)
                    approachTarget(availability.mud,
                        d.mudFilmPickupRateHz * intensity * contactWeight,
                        dt, cell.mudFilmContamination);

                if (clean > 0.0)
                {
                    // Every lateral band in the footprint physically rubs
                    // the road; use sector residence for cleaning and keep
                    // band load weighting for pickup/force influence.
                    const VehicleScalar localClean = clean * sectorWeight.second;
                    cleanChannel(localClean, d.organicRetention, dt, cell.organicContamination);
                    cleanChannel(localClean, d.mineralRetention, dt, cell.mineralContamination);
                    cleanChannel(localClean, d.gravelFinesRetention, dt, cell.gravelFinesContamination);
                    cleanChannel(localClean, d.rubberRetention, dt, cell.rubberPickupContamination);
                    cleanChannel(localClean, d.mudRetention, dt, cell.mudFilmContamination);
                }
            }
        }
    }

    return outputFromState(d, wearDescription, input, treadState);
}

} // namespace heritage::vehicles::tires

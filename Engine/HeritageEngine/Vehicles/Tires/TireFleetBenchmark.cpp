#include "TireFleetBenchmark.hpp"

#include "TireDistributedContact.hpp"
#include "TireThermal.hpp"
#include "TireWear.hpp"
#include "TireWetSurfaceInteraction.hpp"
#include "../../Physics/Surfaces/Water/SurfaceHydrology.hpp"
#include "../../Physics/Surfaces/SurfaceMaterialProperties.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <vector>

namespace heritage::vehicles::tires {
namespace {

constexpr VehicleScalar kPi = 3.14159265358979323846;

struct FleetTireState
{
    TireThermalState thermal;
    TireWearState wear;
    TireForceResult previousForce;
    VehicleScalar rotationDegrees = 0.0;
};

bool finiteValue(VehicleScalar value)
{
    return std::isfinite(static_cast<double>(value));
}

} // namespace

TireFleetBenchmarkResult runTireFleetBenchmark(
    const TireModelDescription& fittedTire,
    VehicleScalar wheelRadiusM,
    const TireFleetBenchmarkDescription& d)
{
    TireFleetBenchmarkResult result;
    result.vehicleCount = d.vehicleCount;
    if (!validTireModelDescription(fittedTire))
    {
        result.error = "The fitted tire description is invalid.";
        return result;
    }
    if (d.vehicleCount == 0 || d.vehicleCount > 1000
        || d.tiresPerVehicle == 0 || d.tiresPerVehicle > 16
        || !finiteValue(d.simulatedDurationSeconds)
        || d.simulatedDurationSeconds < 0.01
        || d.simulatedDurationSeconds > 10.0
        || !finiteValue(d.tireRateHz) || d.tireRateHz < 100.0
        || d.tireRateHz > 2000.0
        || !finiteValue(d.physicalStateRateHz)
        || d.physicalStateRateHz < 1.0
        || d.physicalStateRateHz > d.tireRateHz
        || !finiteValue(wheelRadiusM) || wheelRadiusM <= 0.05
        || wheelRadiusM > 2.0)
    {
        result.error = "Fleet benchmark dimensions or rates are outside bounded limits.";
        return result;
    }

    result.tireCount = d.vehicleCount * d.tiresPerVehicle;
    result.tireSteps = static_cast<std::size_t>(std::max(
        VehicleScalar{1.0},
        std::round(d.simulatedDurationSeconds * d.tireRateHz)));
    result.simulatedSeconds = static_cast<VehicleScalar>(result.tireSteps)
        / d.tireRateHz;
    const std::size_t stateStride = static_cast<std::size_t>(std::max(
        VehicleScalar{1.0}, std::round(d.tireRateHz / d.physicalStateRateHz)));
    const VehicleScalar tireDt = VehicleScalar{1.0} / d.tireRateHz;
    const VehicleScalar stateDt = tireDt * static_cast<VehicleScalar>(stateStride);
    const std::size_t distributedTires = std::min(
        d.distributedContactVehicleCount, d.vehicleCount) * d.tiresPerVehicle;

    std::vector<FleetTireState> states(result.tireCount);
    TireModelDescription tire = fittedTire;
    heritage::physics::water::SurfaceHydrology hydrology;
    heritage::physics::SurfaceWeatherDescription hydrologyWeather;
    heritage::physics::SurfaceWeatherOutput hydrologyWeatherOutput;
    if (d.wetWeather && d.includeSpatialHydrology)
    {
        // A compact synthetic multi-lane road lets the fleet diagnostic time
        // real water-cell sampling, tire clearing and 30 Hz runoff without
        // depending on whichever module scene happens to be loaded.
        std::vector<heritage::physics::StaticSceneTriangle> road;
        constexpr int longitudinalTiles = 80;
        constexpr int lateralTiles = 6;
        constexpr float tileM = 2.0f;
        road.reserve(longitudinalTiles * lateralTiles * 2u);
        const auto asphalt = heritage::physics::defaultSurfaceMaterialProperties(
            heritage::physics::SurfaceMaterial::Asphalt);
        for (int x = 0; x < longitudinalTiles; ++x)
        {
            for (int z = 0; z < lateralTiles; ++z)
            {
                const float x0 = static_cast<float>(x) * tileM;
                const float x1 = x0 + tileM;
                const float z0 = static_cast<float>(z) * tileM;
                const float z1 = z0 + tileM;
                const auto height = [](float px, float pz) {
                    const float crown = 0.025f
                        * std::abs(pz - 6.0f) / 6.0f;
                    return 0.010f * std::sin(px * 0.035f) + crown;
                };
                const heritage::math::Vec3 a{ x0, height(x0, z0), z0 };
                const heritage::math::Vec3 b{ x1, height(x1, z0), z0 };
                const heritage::math::Vec3 c{ x1, height(x1, z1), z1 };
                const heritage::math::Vec3 e{ x0, height(x0, z1), z1 };
                heritage::physics::StaticSceneTriangle first;
                first.a = a; first.b = b; first.c = c;
                first.normal = { 0.0f, 1.0f, 0.0f };
                first.surfaceMaterial = heritage::physics::SurfaceMaterial::Asphalt;
                first.surfaceProperties = asphalt;
                heritage::physics::StaticSceneTriangle second = first;
                second.a = a; second.b = c; second.c = e;
                road.push_back(first);
                road.push_back(second);
            }
        }
        heritage::physics::water::SurfaceHydrologyBakeReport bake;
        if (!hydrology.bake(road, { 0.0, 0.0, 0.0 }, bake)
            || !hydrology.setUniformWaterDepthForLab(d.roadWaterDepthM))
        {
            result.error = "Fleet benchmark could not initialize spatial hydrology.";
            return result;
        }
        result.hydrologyCellCount = bake.cellCount;
        hydrologyWeather.enabled = true;
        hydrologyWeather.precipitationRateMmPerHour = 12.0;
        hydrologyWeather.relativeHumidity = 0.90;
        hydrologyWeather.windSpeedMps = d.windSpeedMps;
        hydrologyWeather.cloudCover = 1.0;
        hydrologyWeatherOutput.valid = true;
        hydrologyWeatherOutput.windSpeedMps = d.windSpeedMps;
        hydrologyWeatherOutput.evaporationRateMmPerHour = 0.05;
    }
    // The benchmark consumes the fitted provider configuration. Wet weather
    // remains a no-op when the fitted tire deliberately disables that layer.
    const auto start = std::chrono::steady_clock::now();
    double checksum = 0.0;

    for (std::size_t step = 0; step < result.tireSteps; ++step)
    {
        const VehicleScalar time = static_cast<VehicleScalar>(step) * tireDt;
        if (d.wetWeather && d.includeSpatialHydrology)
            hydrology.advance(hydrologyWeather, hydrologyWeatherOutput, tireDt);
        for (std::size_t tireIndex = 0; tireIndex < result.tireCount; ++tireIndex)
        {
            FleetTireState& state = states[tireIndex];
            const std::size_t vehicleIndex = tireIndex / d.tiresPerVehicle;
            const std::size_t cornerIndex = tireIndex % d.tiresPerVehicle;
            const VehicleScalar phase = static_cast<VehicleScalar>(
                (vehicleIndex * 17 + cornerIndex * 31) % 97) / 97.0;
            const VehicleScalar speedMps = VehicleScalar{18.0}
                + VehicleScalar{30.0} * phase
                + VehicleScalar{4.0} * std::sin(
                    time * VehicleScalar{1.7} + phase * VehicleScalar{6.0});
            const VehicleScalar normalLoadN = fittedTire.nominalLoad
                * (VehicleScalar{0.72} + VehicleScalar{0.48}
                    * static_cast<VehicleScalar>((vehicleIndex + cornerIndex) % 11)
                        / VehicleScalar{10.0});
            const VehicleScalar longitudinalSlip = VehicleScalar{0.07}
                * std::sin(time * VehicleScalar{3.1} + phase * VehicleScalar{5.0});
            const VehicleScalar slipAngle = VehicleScalar{0.075}
                * std::sin(time * VehicleScalar{2.3} + phase * VehicleScalar{7.0});
            const VehicleScalar camber = (cornerIndex % 2 == 0
                ? VehicleScalar{-0.025} : VehicleScalar{0.025});

            const TireThermalOutput thermal = evaluateTireThermalState(
                tire.thermal, state.thermal);
            const VehicleScalar pressurePa = thermal.valid
                ? thermal.inflationPressurePa : tire.inflationPressurePa;
            const TireWearInput wearReadInput{
                true,
                state.rotationDegrees,
                normalLoadN,
                tire.nominalLoad,
                camber,
                pressurePa,
                tire.referenceInflationPressurePa,
                thermal.valid ? thermal.treadTemperatureC : VehicleScalar{20.0},
                0.0,
                1.0
            };
            const TireWearOutput wear = evaluateTireWearState(
                tire.wear, tire.thermal, wearReadInput, state.wear);

            TireWetSurfaceInput wetInput;
            wetInput.grounded = true;
            wetInput.surfaceMaterial = heritage::physics::SurfaceMaterial::Asphalt;
            wetInput.surfaceWetness = d.wetWeather ? 1.0 : 0.0;
            wetInput.surfaceWeatherWetness = d.wetWeather ? 1.0 : 0.0;
            wetInput.surfaceWaterDepthValid = true;
            wetInput.surfaceWaterDepthM = d.wetWeather
                ? d.roadWaterDepthM : VehicleScalar{0.0};
            wetInput.wheelRotationDegrees = state.rotationDegrees;
            wetInput.normalLoadN = normalLoadN;
            wetInput.inflationPressurePa = pressurePa;
            wetInput.referencePressurePa = tire.referenceInflationPressurePa;
            wetInput.forwardSpeedMps = speedMps;
            wetInput.longitudinalSlipVelocityMps = longitudinalSlip * speedMps;
            wetInput.lateralSlipVelocityMps = std::tan(slipAngle) * speedMps;
            wetInput.contactPatchLengthM = 0.12;
            wetInput.contactPatchWidthM = 0.20;
            wetInput.contactPatchAreaM2 = 0.024;
            wetInput.currentAverageTreadDepthM = wear.valid
                ? wear.averageTreadDepthM : tire.wear.initialTreadDepthM;
            wetInput.initialTreadDepthM = tire.wear.initialTreadDepthM;
            wetInput.minimumTreadDepthM = tire.wear.minimumTreadDepthM;
            wetInput.bulkTreadTemperatureC = thermal.valid
                ? thermal.treadTemperatureC : VehicleScalar{20.0};
            const TireWetSurfaceOutput wet = evaluateTireWetSurface(
                tire.wetSurface, tire.wear, wetInput, state.wear);

            TireContactInput contact;
            contact.normalLoad = normalLoadN;
            contact.longitudinalSlip = longitudinalSlip;
            contact.slipAngleRadians = slipAngle;
            contact.camberAngleRadians = camber;
            contact.forwardSpeedMps = speedMps;
            contact.wheelRadiusM = wheelRadiusM;
            contact.inflationPressurePa = pressurePa;
            contact.frictionMultiplier =
                (thermal.valid ? thermal.frictionScale : VehicleScalar{1.0})
                * (wear.valid ? wear.contactFrictionScale : VehicleScalar{1.0})
                * (wet.valid ? wet.frictionScale : VehicleScalar{1.0});
            contact.stiffnessMultiplier =
                (thermal.valid ? thermal.stiffnessScale : VehicleScalar{1.0})
                * (wet.valid ? wet.stiffnessScale : VehicleScalar{1.0});

            if (tireIndex < distributedTires)
            {
                TireDistributedContactInput distributed;
                distributed.aggregateInput = contact;
                distributed.contactPatchLengthM = 0.12;
                distributed.contactPatchWidthM = 0.20;
                state.previousForce = evaluateTireDistributedContact(
                    tire, distributed).integrated;
                result.distributedBrushCellEvaluations +=
                    kDistributedContactCellCount;
            }
            else
            {
                state.previousForce = evaluateAdvancedRoadTire(tire, contact);
            }
            ++result.wholeTireForceEvaluations;

            if (d.wetWeather && d.includeSpatialHydrology)
            {
                heritage::physics::water::SurfaceHydrologyTireInput waterContact;
                waterContact.deltaTimeSeconds = tireDt;
                waterContact.contactPatchLengthM = wetInput.contactPatchLengthM;
                waterContact.contactPatchWidthM = wetInput.contactPatchWidthM;
                waterContact.contactPatchAreaM2 = wetInput.contactPatchAreaM2;
                waterContact.normalLoadN = normalLoadN;
                waterContact.nominalLoadN = tire.nominalLoad;
                waterContact.forwardSpeedMps = speedMps;
                waterContact.lateralSpeedMps = wetInput.lateralSlipVelocityMps;
                waterContact.treadVoidRatio = tire.wetSurface.treadVoidRatio;
                waterContact.slipDissipationWatts = std::abs(
                    state.previousForce.longitudinalForce
                        * wetInput.longitudinalSlipVelocityMps)
                    + std::abs(state.previousForce.lateralForce
                        * wetInput.lateralSlipVelocityMps);
                const double roadX = std::fmod(
                    static_cast<double>(vehicleIndex) * 3.7
                        + static_cast<double>(time * speedMps),
                    158.0) + 1.0;
                const double roadZ = 1.0
                    + static_cast<double>((vehicleIndex + cornerIndex) % 6) * 2.0;
                hydrology.applyTireContact({ roadX, 0.0, roadZ }, waterContact);
                ++result.hydrologyTireContacts;
            }

            if (step % stateStride == 0)
            {
                TireThermalInput thermalInput;
                thermalInput.grounded = true;
                thermalInput.forwardSpeedMps = speedMps;
                thermalInput.ambientAirSpeedMps = d.windSpeedMps;
                thermalInput.longitudinalSlipVelocityMps =
                    wetInput.longitudinalSlipVelocityMps;
                thermalInput.lateralSlipVelocityMps =
                    wetInput.lateralSlipVelocityMps;
                thermalInput.longitudinalForceN =
                    state.previousForce.longitudinalForce;
                thermalInput.lateralForceN = state.previousForce.lateralForce;
                thermalInput.radialDissipationWatts = normalLoadN * 0.015;
                thermalInput.rollingResistanceDissipationWatts =
                    std::abs(state.previousForce.rollingResistanceMoment
                        * speedMps / wheelRadiusM);
                thermalInput.contactPatchAreaM2 = 0.024;
                thermalInput.environmentTemperatureOverride = true;
                thermalInput.ambientTemperatureC = d.ambientTemperatureC;
                thermalInput.roadTemperatureC = d.roadTemperatureC;
                thermalInput.roadHeatTransferScale = wet.valid
                    ? wet.roadHeatTransferScale : VehicleScalar{1.0};
                advanceTireThermal(
                    tire.thermal, thermalInput, stateDt, state.thermal);
                ++result.thermalStateUpdates;

                TireWearInput wearInput = wearReadInput;
                wearInput.slipDissipationWatts =
                    std::abs(state.previousForce.longitudinalForce
                        * wetInput.longitudinalSlipVelocityMps)
                    + std::abs(state.previousForce.lateralForce
                        * wetInput.lateralSlipVelocityMps);
                advanceTireWear(
                    tire.wear, tire.thermal, wearInput, stateDt, state.wear);
                ++result.wearStateUpdates;
                advanceTireWetSurface(
                    tire.wetSurface, tire.wear, wetInput, stateDt, state.wear);
                ++result.wetStateUpdates;
            }

            state.rotationDegrees = std::fmod(
                state.rotationDegrees
                    + speedMps / wheelRadiusM * tireDt
                        * VehicleScalar{180.0} / kPi,
                VehicleScalar{360.0});
            checksum += static_cast<double>(
                state.previousForce.longitudinalForce * VehicleScalar{0.000001}
                + state.previousForce.lateralForce * VehicleScalar{0.0000001}
                + pressurePa * VehicleScalar{0.000000001});
        }
    }

    const auto finish = std::chrono::steady_clock::now();
    const double wallSeconds = std::chrono::duration<double>(finish - start).count();
    result.wallClockMilliseconds = wallSeconds * 1000.0;
    result.realTimeFactor = wallSeconds > 0.0
        ? static_cast<double>(result.simulatedSeconds) / wallSeconds : 0.0;
    result.tireEvaluationsPerSecond = wallSeconds > 0.0
        ? static_cast<double>(result.wholeTireForceEvaluations) / wallSeconds : 0.0;
    result.microsecondsPerVehicleStep =
        wallSeconds > 0.0 && result.tireSteps > 0 && result.vehicleCount > 0
        ? wallSeconds * 1.0e6
            / static_cast<double>(result.tireSteps * result.vehicleCount)
        : 0.0;
    result.checksum = checksum;
    if (d.wetWeather && d.includeSpatialHydrology)
        result.hydrologySteps = static_cast<std::size_t>(
            hydrology.stats().simulationStepCount);
    result.valid = std::isfinite(checksum)
        && result.wholeTireForceEvaluations
            == result.tireCount * result.tireSteps
        && (!d.wetWeather || !d.includeSpatialHydrology
            || result.hydrologyTireContacts
                == result.tireCount * result.tireSteps);
    if (!result.valid)
        result.error = "Fleet tire workload produced an invalid result.";
    return result;
}

} // namespace heritage::vehicles::tires

#include "TireFleetBenchmark.hpp"

#include "TireDistributedContact.hpp"
#include "TireThermal.hpp"
#include "TireWear.hpp"
#include "TireWetSurfaceInteraction.hpp"

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
    // OPT03C: this is deliberately a CPU tire-stack benchmark only. Spatial
    // water is renderer/GPU authority and cannot be represented faithfully by
    // spinning up a second CPU Hydro model inside this diagnostic.
    (void)d.includeSpatialHydrology;
    // The benchmark consumes the fitted provider configuration. Wet weather
    // remains a no-op when the fitted tire deliberately disables that layer.
    const auto start = std::chrono::steady_clock::now();
    double checksum = 0.0;

    for (std::size_t step = 0; step < result.tireSteps; ++step)
    {
        const VehicleScalar time = static_cast<VehicleScalar>(step) * tireDt;
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
    result.valid = std::isfinite(checksum)
        && result.wholeTireForceEvaluations
            == result.tireCount * result.tireSteps
        ;
    if (!result.valid)
        result.error = "Fleet tire workload produced an invalid result.";
    return result;
}

} // namespace heritage::vehicles::tires

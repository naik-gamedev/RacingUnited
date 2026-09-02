#pragma once

#include "../TireModel.hpp"

#include <cstddef>
#include <limits>
#include <string>

namespace heritage::vehicles::tires {

// Executable tire-only workload laboratory. It deliberately excludes chassis
// rigid bodies, collision broadphase, AI, rendering, audio and networking so
// its result isolates the cost of the tire stack instead of pretending to be
// a complete 150-car race benchmark.
struct TireFleetBenchmarkDescription
{
    std::size_t vehicleCount = 150;
    std::size_t tiresPerVehicle = 4;
    VehicleScalar simulatedDurationSeconds = 0.25;
    VehicleScalar tireRateHz = 1000.0;
    VehicleScalar physicalStateRateHz = 100.0;
    // TIRE46: every tire is distributed by default. Tests/tools may still cap
    // this explicitly when isolating Aggregate as a diagnostic fallback.
    std::size_t distributedContactVehicleCount =
        (std::numeric_limits<std::size_t>::max)();
    bool wetWeather = false;
    VehicleScalar roadWaterDepthM = 0.0020;
    VehicleScalar ambientTemperatureC = 18.0;
    VehicleScalar roadTemperatureC = 16.0;
    VehicleScalar windSpeedMps = 8.0;
    // OPT03C: retained as a source/API compatibility flag. The tire-only CPU
    // benchmark no longer instantiates any spatial water solver; production
    // spatial water is GPU-owned and must be profiled in the renderer runtime.
    bool includeSpatialHydrology = false;
};

struct TireFleetBenchmarkResult
{
    bool valid = false;
    std::string error;
    std::size_t vehicleCount = 0;
    std::size_t tireCount = 0;
    std::size_t tireSteps = 0;
    std::size_t wholeTireForceEvaluations = 0;
    std::size_t distributedBrushCellEvaluations = 0;
    std::size_t thermalStateUpdates = 0;
    std::size_t wearStateUpdates = 0;
    std::size_t wetStateUpdates = 0;
    // OPT03C compatibility telemetry. These remain zero because the CPU-only
    // fleet benchmark no longer creates a second spatial-water authority.
    std::size_t hydrologyCellCount = 0;
    std::size_t hydrologySteps = 0;
    std::size_t hydrologyTireContacts = 0;
    VehicleScalar simulatedSeconds = 0.0;
    double wallClockMilliseconds = 0.0;
    double realTimeFactor = 0.0;
    double tireEvaluationsPerSecond = 0.0;
    double microsecondsPerVehicleStep = 0.0;
    double checksum = 0.0;
};

TireFleetBenchmarkResult runTireFleetBenchmark(
    const TireModelDescription& fittedTire,
    VehicleScalar wheelRadiusM,
    const TireFleetBenchmarkDescription& description = {});

} // namespace heritage::vehicles::tires

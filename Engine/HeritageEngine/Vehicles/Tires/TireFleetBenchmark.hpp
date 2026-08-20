#pragma once

#include "../TireModel.hpp"

#include <cstddef>
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
    std::size_t distributedContactVehicleCount = 1;
    bool wetWeather = false;
    VehicleScalar roadWaterDepthM = 0.0020;
    VehicleScalar ambientTemperatureC = 18.0;
    VehicleScalar roadTemperatureC = 16.0;
    VehicleScalar windSpeedMps = 8.0;
    bool includeSpatialHydrology = true;
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

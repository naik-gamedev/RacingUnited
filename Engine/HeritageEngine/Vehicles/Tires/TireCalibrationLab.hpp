#pragma once

#include "../TireModel.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace heritage::vehicles {

// Canonical SI axes used by the deterministic TIRE18 steady-state laboratory.
// Degrees and PSI belong in presentation code; files and calculations retain
// radians and pascals so repeated runs cannot acquire unit-conversion drift.
enum class TireCalibrationAxis
{
    None,
    LongitudinalSlip,
    SlipAngleRadians,
    NormalLoadNewtons,
    InflationPressurePascals,
    CamberAngleRadians,
    TurnSlipPerMeter
};

struct TireCalibrationAxisRange
{
    TireCalibrationAxis axis = TireCalibrationAxis::None;
    VehicleScalar minimum = 0.0;
    VehicleScalar maximum = 0.0;
    std::size_t sampleCount = 1;
};

struct TireCalibrationSweepDescription
{
    std::string name;
    TireContactInput baseline;
    TireCalibrationAxisRange primary;
    TireCalibrationAxisRange secondary;
};

struct TireCalibrationSample
{
    std::size_t primaryIndex = 0;
    std::size_t secondaryIndex = 0;
    VehicleScalar primaryValue = 0.0;
    VehicleScalar secondaryValue = 0.0;
    TireContactInput input;
    TireForceResult force;
};

struct TireCalibrationSweepResult
{
    bool valid = false;
    std::string name;
    TireCalibrationAxisRange primary;
    TireCalibrationAxisRange secondary;
    std::vector<TireCalibrationSample> samples;
    std::string error;
};

const char* tireCalibrationAxisName(TireCalibrationAxis axis);

bool validTireCalibrationSweepDescription(
    const TireCalibrationSweepDescription& description,
    std::string* error = nullptr);

TireCalibrationSweepResult runTireCalibrationSweep(
    const TireModelDescription& tire,
    const TireCalibrationSweepDescription& description);

// Standard clean-room evidence suite. It deliberately exercises the fitted
// model inside ordinary road-tire validity ranges; puncture/collapse and
// transient tests remain separate stateful laboratory scenarios.
std::vector<TireCalibrationSweepDescription> standardTireCalibrationSweeps(
    const TireModelDescription& tire,
    VehicleScalar wheelRadiusM);

std::vector<TireCalibrationSweepResult> runStandardTireCalibrationSuite(
    const TireModelDescription& tire,
    VehicleScalar wheelRadiusM);

bool exportTireCalibrationSweepCsv(
    const TireCalibrationSweepResult& result,
    const std::filesystem::path& path,
    std::string* error = nullptr);

} // namespace heritage::vehicles

#pragma once

#include "../TireModel.hpp"
#include "TireFailure.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace heritage::vehicles {

// TIRE18C stateful evidence samples. A single stable schema lets external
// analysis compare unlike scenarios without each test inventing a CSV format.
// Fields that do not participate in a scenario remain at their neutral value.
struct TireScenarioSample
{
    VehicleScalar timeSeconds = 0.0;
    VehicleScalar targetLongitudinalSlip = 0.0;
    VehicleScalar effectiveLongitudinalSlip = 0.0;
    VehicleScalar targetSlipAngleRadians = 0.0;
    VehicleScalar effectiveSlipAngleRadians = 0.0;
    VehicleScalar forwardSpeedMps = 0.0;
    VehicleScalar normalLoadN = 0.0;
    TireForceResult force;

    VehicleScalar treadTemperatureC = 20.0;
    VehicleScalar carcassTemperatureC = 20.0;
    VehicleScalar gasTemperatureC = 20.0;
    VehicleScalar rimTemperatureC = 20.0;
    VehicleScalar inflationPressurePa = 0.0;
    VehicleScalar averageTreadDepthM = 0.0;
    VehicleScalar minimumTreadDepthM = 0.0;
    VehicleScalar flatSpotDepthM = 0.0;
    VehicleScalar wearFraction = 0.0;

    tires::TireFailureStage failureStage =
        tires::TireFailureStage::Healthy;
    VehicleScalar containedGasMassRatio = 1.0;
    VehicleScalar structuralIntegrity = 1.0;
    VehicleScalar treadAttachment = 1.0;
    VehicleScalar rimContactFraction = 0.0;
};

struct TireScenarioResult
{
    bool valid = false;
    std::string name;
    VehicleScalar integrationStepSeconds = 0.0;
    VehicleScalar sampleIntervalSeconds = 0.0;
    std::vector<TireScenarioSample> samples;
    std::string error;
};

std::vector<std::string> standardTireScenarioNames();

// Runs an isolated copy of state against the fitted model. It never mutates
// the live vehicle tire, so lab evidence cannot change the driver's setup.
TireScenarioResult runStandardTireScenario(
    const TireModelDescription& tire,
    VehicleScalar wheelRadiusM,
    const std::string& scenarioName);

bool exportTireScenarioCsv(
    const TireScenarioResult& result,
    const std::filesystem::path& path,
    std::string* error = nullptr);

} // namespace heritage::vehicles

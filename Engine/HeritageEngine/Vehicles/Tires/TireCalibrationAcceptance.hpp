#pragma once

#include "TireCalibrationLab.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace heritage::vehicles {

struct TireCalibrationValueEnvelope
{
    bool enabled = false;
    VehicleScalar minimum = 0.0;
    VehicleScalar maximum = 0.0;
};

struct TireCalibrationAcceptancePoint
{
    std::size_t primaryIndex = 0;
    std::size_t secondaryIndex = 0;
    VehicleScalar primaryValue = 0.0;
    VehicleScalar secondaryValue = 0.0;
    TireCalibrationValueEnvelope longitudinalForceN;
    TireCalibrationValueEnvelope lateralForceN;
    TireCalibrationValueEnvelope aligningTorqueNm;
    TireCalibrationValueEnvelope overturningMomentNm;
    TireCalibrationValueEnvelope rollingResistanceMomentNm;
};

struct TireCalibrationAcceptanceEnvelope
{
    std::string schema = "heritage_tire_acceptance_v1";
    std::string sweepName;
    TireCalibrationAxis primaryAxis = TireCalibrationAxis::None;
    TireCalibrationAxis secondaryAxis = TireCalibrationAxis::None;
    std::string source;
    std::string provenance;
    bool synthetic = true;
    VehicleScalar confidence = 0.0;
    // Structural gate independent of reference data. A single adjacent force
    // step larger than this fraction of local Fz is considered discontinuous.
    VehicleScalar maximumAdjacentForceJumpLoadFraction = 0.80;
    VehicleScalar maximumAdjacentMomentJumpLoadRadiusFraction = 0.50;
    std::vector<TireCalibrationAcceptancePoint> points;
};

struct TireCalibrationAcceptanceViolation
{
    std::size_t sampleIndex = 0;
    std::string code;
    std::string message;
};

struct TireCalibrationAcceptanceReport
{
    bool valid = false;
    bool passed = false;
    bool usedSyntheticReference = true;
    std::size_t evaluatedSampleCount = 0;
    std::size_t violationCount = 0;
    std::vector<TireCalibrationAcceptanceViolation> violations;
    std::string error;
};

// Produces an explicitly labelled envelope around one reference run. This is
// useful for regression baselines, but remains synthetic until the caller
// supplies measured source/provenance and marks it accordingly.
TireCalibrationAcceptanceEnvelope buildRelativeTireCalibrationEnvelope(
    const TireCalibrationSweepResult& reference,
    const std::string& source,
    const std::string& provenance,
    bool synthetic,
    VehicleScalar confidence,
    VehicleScalar relativeForceTolerance = 0.05,
    VehicleScalar absoluteForceToleranceN = 25.0,
    VehicleScalar relativeMomentTolerance = 0.08,
    VehicleScalar absoluteMomentToleranceNm = 2.0);

// Checks fitted validity limits, non-finite values, force signs, continuity
// and every enabled reference envelope. Violations are retained in stable
// sample order and capped to bounded diagnostic storage.
TireCalibrationAcceptanceReport evaluateTireCalibrationAcceptance(
    const TireModelDescription& tire,
    const TireCalibrationSweepResult& candidate,
    const TireCalibrationAcceptanceEnvelope& envelope);

} // namespace heritage::vehicles

#pragma once

#include "../TireModel.hpp"

#include <array>
#include <cstddef>

namespace heritage::vehicles::tires {

inline constexpr std::size_t kDistributedContactLongitudinalCells = 3;
inline constexpr std::size_t kDistributedContactLateralCells = 3;
inline constexpr std::size_t kDistributedContactCellCount = 9;

struct TireContactWorkEstimate
{
    std::size_t wholeTireForceEvaluations = 1;
    std::size_t localBrushCells = 0;
    std::size_t maximumRoadEnvelopeSamples = 5;
};

// The integer mirrors VehicleSystem's public TireContactFidelity values while
// keeping this low-level provider independent of VehicleSystem.hpp.
TireContactWorkEstimate tireContactWorkEstimate(int fidelityTier);

struct TireDistributedContactCell
{
    // Fractions are normalized by the evaluator. They represent local
    // contact pressure before a surface-specific capacity is applied.
    VehicleScalar normalLoadFraction = 1.0 / 9.0;
    // Relative to the aggregate/base contact input. One preserves the exact
    // fitted whole-tire curve on a homogeneous footprint.
    VehicleScalar frictionScale = 1.0;
    VehicleScalar stiffnessScale = 1.0;
    bool supported = true;
};

struct TireDistributedContactInput
{
    TireContactInput aggregateInput;
    VehicleScalar contactPatchLengthM = 0.12;
    VehicleScalar contactPatchWidthM = 0.20;
    std::array<TireDistributedContactCell,
        kDistributedContactCellCount> cells{};
};

struct TireDistributedContactCellOutput
{
    VehicleScalar longitudinalOffsetM = 0.0;
    VehicleScalar lateralOffsetM = 0.0;
    VehicleScalar normalLoadN = 0.0;
    VehicleScalar longitudinalForceN = 0.0;
    VehicleScalar lateralForceN = 0.0;
    VehicleScalar capacityN = 0.0;
    VehicleScalar utilization = 0.0;
    VehicleScalar localLongitudinalSlip = 0.0;
    VehicleScalar localSlipAngleRadians = 0.0;
};

struct TireDistributedContactOutput
{
    bool valid = false;
    TireForceResult aggregateBaseline;
    TireForceResult integrated;
    std::array<TireDistributedContactCellOutput,
        kDistributedContactCellCount> cells{};
    VehicleScalar supportedLoadFraction = 0.0;
    VehicleScalar maximumCellUtilization = 0.0;
    VehicleScalar forceDifferenceFromAggregateN = 0.0;
};

// Bounded 3x3 brush allocation around one calibrated whole-tire force target.
// It does not evaluate nine independent whole-tire MF models. Homogeneous
// footprints reproduce the aggregate curve; split support/friction and
// pressure/stiffness distribution can redistribute/cap local shear and add
// the corresponding hub moment. Turn-slip remains owned by the calibrated
// whole-tire provider and is exposed per cell only as diagnostic local slip;
// it is not counted twice as a second uncalibrated force model.
TireDistributedContactOutput evaluateTireDistributedContact(
    const TireModelDescription& tire,
    const TireDistributedContactInput& input);

} // namespace heritage::vehicles::tires

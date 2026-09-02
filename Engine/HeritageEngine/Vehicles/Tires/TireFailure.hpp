#pragma once

#include "../VehiclePrecision.hpp"

#include <cstdint>

namespace heritage::vehicles::tires {

// The coarse presentation stage remains monotonic for compatibility. TIRE46
// adds independent physical damage coordinates underneath it so valve/bead/
// sidewall leaks, belt/cord fatigue, heat distress and rim damage do not have
// to masquerade as a single puncture scalar.
enum class TireFailureStage : std::uint8_t
{
    Healthy = 0,
    SlowPuncture = 1,
    RapidPressureLoss = 2,
    Blowout = 3,
    PartiallyDetachedTread = 4,
    CollapsedCarcass = 5,
    BareRimRunning = 6
};

// Explicit incident vocabulary for collision/gameplay/lab integration. Normal
// driving also accumulates the same underlying damage continuously.
enum class TireDamageIncident : std::uint8_t
{
    TreadPuncture = 0,
    TreadCut = 1,
    SidewallCut = 2,
    ValveLeak = 3,
    BeadLeak = 4,
    BeadUnseat = 5,
    BeltSeparation = 6,
    Impact = 7,
    RimImpact = 8,
    RepairPuncture = 9
};

const char* tireFailureStageName(TireFailureStage stage);
const char* tireDamageIncidentName(TireDamageIncident incident);

VehicleScalar estimatedTireContainedAirVolumeM3(
    VehicleScalar unloadedRadiusM,
    VehicleScalar sectionWidthM,
    VehicleScalar rimRadiusM);

struct TireFailureDescription
{
    bool enabled = false;

    VehicleScalar containedAirVolumeM3 = 0.025;
    VehicleScalar dischargeCoefficient = 0.72;
    VehicleScalar airSpecificGasConstantJPerKgK = 287.05;
    VehicleScalar airHeatCapacityRatio = 1.40;
    VehicleScalar slowPunctureAreaM2 = 0.08e-6;
    VehicleScalar rapidPressureLossAreaM2 = 6.0e-6;
    VehicleScalar blowoutAreaM2 = 600.0e-6;
    VehicleScalar treadCutAreaM2 = 24.0e-6;
    VehicleScalar sidewallCutAreaM2 = 55.0e-6;
    VehicleScalar valveLeakAreaM2 = 0.20e-6;
    VehicleScalar beadLeakAreaM2 = 1.2e-6;

    VehicleScalar minimumEmbeddedSealOpeningFraction = 0.06;
    VehicleScalar flexOpeningGain = 2.5;
    VehicleScalar rapidLossAreaThresholdM2 = 1.5e-6;

    VehicleScalar underinflationDamageStartRatio = 0.65;
    VehicleScalar collapsedPressureRatio = 0.12;
    VehicleScalar collapseLoadedDelaySeconds = 1.25;
    VehicleScalar maximumSafeCarcassTemperatureC = 135.0;
    VehicleScalar severeUnderinflationIntegrityLossPerSecond = 0.00040;
    VehicleScalar collapsedRunningIntegrityLossPerSecond = 0.020;
    VehicleScalar collapsedRunningTreadLossPerSecond = 0.045;
    VehicleScalar blowoutRunningTreadLossPerSecond = 0.20;
    VehicleScalar bareRimIntegrityThreshold = 0.08;
    VehicleScalar bareRimTreadAttachmentThreshold = 0.06;

    // Endurance/structural damage. Reference energies are intentionally
    // construction-authored estimates, not manufacturer secrets.
    VehicleScalar beltFatigueReferenceEnergyJ = 1.8e8;
    VehicleScalar cordFatigueReferenceEnergyJ = 2.6e8;
    VehicleScalar sidewallFatigueReferenceEnergyJ = 1.4e8;
    VehicleScalar fatigueHeatAccelerationPerC = 0.018;
    VehicleScalar fatigueOverloadExponent = 2.2;

    // Heat/surface distress. Graining is predominantly cold/high-slip surface
    // tearing; blistering is hot/high-energy damage. Both are reversible only
    // slowly; delamination is permanent and can progress to tread separation.
    VehicleScalar grainingColdThresholdBelowOptimumC = 22.0;
    VehicleScalar grainingBuildPerKJ = 0.00045;
    VehicleScalar grainingRecoveryPerSecond = 0.00010;
    VehicleScalar blisterTemperatureC = 125.0;
    VehicleScalar blisterBuildPerKJ = 0.00034;
    VehicleScalar blisterRecoveryPerSecond = 0.000025;
    VehicleScalar delaminationTemperatureC = 145.0;
    VehicleScalar delaminationBuildPerSecond = 0.0012;

    // Bead/rim mechanics under low pressure and lateral load.
    VehicleScalar beadUnseatPressureRatio = 0.18;
    VehicleScalar beadUnseatLateralForceRatio = 0.90;
    VehicleScalar beadDamageRatePerSecond = 0.020;
    VehicleScalar rimDamagePowerThresholdW = 9000.0;
    VehicleScalar rimDamageRatePerSecond = 0.018;

    // Optional run-flat insert/reinforced-sidewall support. It is a bounded
    // support layer, not a second tire model; heat/speed/load consume its health.
    bool runFlatSupportEnabled = false;
    VehicleScalar runFlatSupportLoadFraction = 0.62;
    VehicleScalar runFlatMaximumSpeedMps = 22.0;
    VehicleScalar runFlatMaximumTemperatureC = 125.0;
    VehicleScalar runFlatHealthLossPerSecond = 0.0025;
};

struct TireFailureState
{
    bool initialized = false;
    TireFailureStage stage = TireFailureStage::Healthy;

    VehicleScalar containedGasMassRatio = 1.0;
    VehicleScalar punctureAreaM2 = 0.0;
    VehicleScalar embeddedObjectSealFraction = 0.0;
    VehicleScalar valveLeakAreaM2 = 0.0;
    VehicleScalar beadLeakAreaM2 = 0.0;
    VehicleScalar sidewallLeakAreaM2 = 0.0;
    VehicleScalar effectiveLeakAreaM2 = 0.0;
    VehicleScalar leakMassFlowKgPerSecond = 0.0;
    VehicleScalar pressurizedGasFraction = 1.0;

    VehicleScalar structuralIntegrity = 1.0;
    VehicleScalar beltIntegrity = 1.0;
    VehicleScalar cordIntegrity = 1.0;
    VehicleScalar sidewallIntegrity = 1.0;
    VehicleScalar beadRetention = 1.0;
    VehicleScalar treadAttachment = 1.0;
    VehicleScalar rimIntegrity = 1.0;
    VehicleScalar runFlatSupportHealth = 1.0;
    VehicleScalar treadGraining = 0.0;
    VehicleScalar treadBlistering = 0.0;
    VehicleScalar delaminationFraction = 0.0;
    VehicleScalar rimContactFraction = 0.0;
    VehicleScalar lowPressureLoadedSeconds = 0.0;
    VehicleScalar blowoutElapsedSeconds = 0.0;
    VehicleScalar eventElapsedSeconds = 0.0;
    bool blowoutLatched = false;
    std::uint64_t eventSerial = 0;
};

struct TireFailureInput
{
    bool grounded = false;
    VehicleScalar ambientPressurePa = 101325.0;
    VehicleScalar referenceGaugePressurePa = 220000.0;
    VehicleScalar referenceTemperatureC = 20.0;
    VehicleScalar gasTemperatureC = 20.0;
    VehicleScalar inflationGaugePressurePa = 220000.0;
    VehicleScalar identifiedReferencePressurePa = 220000.0;
    VehicleScalar normalLoadN = 0.0;
    VehicleScalar nominalLoadN = 3500.0;
    VehicleScalar forwardSpeedMps = 0.0;
    VehicleScalar longitudinalSlipVelocityMps = 0.0;
    VehicleScalar lateralSlipVelocityMps = 0.0;
    VehicleScalar longitudinalForceN = 0.0;
    VehicleScalar lateralForceN = 0.0;
    VehicleScalar radialDissipationWatts = 0.0;
    VehicleScalar slipDissipationWatts = 0.0;
    VehicleScalar treadTemperatureC = 20.0;
    VehicleScalar optimumTreadTemperatureC = 70.0;
    VehicleScalar carcassTemperatureC = 20.0;
    VehicleScalar rimTemperatureC = 20.0;
    VehicleScalar camberAngleRadians = 0.0;
};

struct TireFailureOutput
{
    bool valid = false;
    TireFailureStage stage = TireFailureStage::Healthy;
    VehicleScalar containedGasMassRatio = 1.0;
    VehicleScalar pressurizedGasFraction = 1.0;
    VehicleScalar effectiveLeakAreaM2 = 0.0;
    VehicleScalar leakMassFlowKgPerSecond = 0.0;
    VehicleScalar structuralIntegrity = 1.0;
    VehicleScalar beltIntegrity = 1.0;
    VehicleScalar cordIntegrity = 1.0;
    VehicleScalar sidewallIntegrity = 1.0;
    VehicleScalar beadRetention = 1.0;
    VehicleScalar treadAttachment = 1.0;
    VehicleScalar rimIntegrity = 1.0;
    VehicleScalar runFlatSupportHealth = 1.0;
    VehicleScalar treadGraining = 0.0;
    VehicleScalar treadBlistering = 0.0;
    VehicleScalar delaminationFraction = 0.0;
    VehicleScalar rimContactFraction = 0.0;
    VehicleScalar eventElapsedSeconds = 0.0;
    VehicleScalar forceCapacityScale = 1.0;
    VehicleScalar carcassSupportScale = 1.0;
    VehicleScalar rollingResistanceScale = 1.0;
};

bool validTireFailureDescription(const TireFailureDescription& description);

TireFailureOutput evaluateTireFailureState(
    const TireFailureDescription& description,
    const TireFailureInput& input,
    const TireFailureState& state);

TireFailureOutput advanceTireFailure(
    const TireFailureDescription& description,
    const TireFailureInput& input,
    VehicleScalar deltaTimeSeconds,
    TireFailureState& state);

void triggerTireFailure(
    const TireFailureDescription& description,
    const TireFailureInput& input,
    TireFailureStage stage,
    TireFailureState& state);

void triggerTireDamageIncident(
    const TireFailureDescription& description,
    const TireFailureInput& input,
    TireDamageIncident incident,
    VehicleScalar severity01,
    TireFailureState& state);

} // namespace heritage::vehicles::tires

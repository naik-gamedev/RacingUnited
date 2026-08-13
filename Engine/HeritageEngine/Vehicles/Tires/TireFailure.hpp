#pragma once

#include "../VehiclePrecision.hpp"

#include <cstdint>

namespace heritage::vehicles::tires {

// TIRE19 persistent pneumatic and structural failure state. The failure model
// deliberately remains reduced-order: one gas inventory and a few bounded
// carcass/tread damage coordinates per tire are sufficient for large fields,
// while presentation may consume the same state at a lower rate.
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

const char* tireFailureStageName(TireFailureStage stage);

// Estimates the free pneumatic cavity from fitted section geometry. This is
// shared by generated tire families and imported property-file tires so leak
// timing does not silently disappear merely because a .tir file predates the
// Heritage failure-state extension.
VehicleScalar estimatedTireContainedAirVolumeM3(
    VehicleScalar unloadedRadiusM,
    VehicleScalar sectionWidthM,
    VehicleScalar rimRadiusM);

struct TireFailureDescription
{
    bool enabled = false;

    // Contained free-air volume and compressible-orifice parameters. Leak
    // areas are physical opening areas rather than arbitrary pressure timers.
    VehicleScalar containedAirVolumeM3 = 0.025;
    VehicleScalar dischargeCoefficient = 0.72;
    VehicleScalar airSpecificGasConstantJPerKgK = 287.05;
    VehicleScalar airHeatCapacityRatio = 1.40;
    VehicleScalar slowPunctureAreaM2 = 0.08e-6;
    VehicleScalar rapidPressureLossAreaM2 = 6.0e-6;
    VehicleScalar blowoutAreaM2 = 600.0e-6;

    // An embedded screw/nail can seal most of a small hole while the carcass
    // is relaxed. Load, lateral slip and radial work flex the opening and make
    // the seal intermittent; road speed itself is not a pressure source.
    VehicleScalar minimumEmbeddedSealOpeningFraction = 0.06;
    VehicleScalar flexOpeningGain = 2.5;
    VehicleScalar rapidLossAreaThresholdM2 = 1.5e-6;

    // Persistent underinflation and heat damage. Rates are conservative road
    // time scales; direct lab triggers exist for deterministic testing.
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
};

struct TireFailureState
{
    bool initialized = false;
    TireFailureStage stage = TireFailureStage::Healthy;

    // Current contained gas mass divided by the mass at the fitted cold
    // reference state. A vented tire settles at an ambient-equilibrium ratio,
    // not zero mass, which prevents an unphysical vacuum.
    VehicleScalar containedGasMassRatio = 1.0;
    VehicleScalar punctureAreaM2 = 0.0;
    VehicleScalar embeddedObjectSealFraction = 0.0;
    VehicleScalar effectiveLeakAreaM2 = 0.0;
    VehicleScalar leakMassFlowKgPerSecond = 0.0;
    VehicleScalar pressurizedGasFraction = 1.0;

    VehicleScalar structuralIntegrity = 1.0;
    VehicleScalar treadAttachment = 1.0;
    VehicleScalar rimContactFraction = 0.0;
    VehicleScalar lowPressureLoadedSeconds = 0.0;
    VehicleScalar blowoutElapsedSeconds = 0.0;
    // Seconds since the most recent explicit incident/repair trigger. Physics
    // owns this clock so render-rate changes cannot alter how long a torn belt
    // remains tethered before its bounded presentation pieces depart.
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
    VehicleScalar radialDissipationWatts = 0.0;
    VehicleScalar carcassTemperatureC = 20.0;
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
    VehicleScalar treadAttachment = 1.0;
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

// Deterministic development/incident trigger. It changes the same persistent
// state used by the runtime model; it is not a presentation-only effect.
void triggerTireFailure(
    const TireFailureDescription& description,
    const TireFailureInput& input,
    TireFailureStage stage,
    TireFailureState& state);

} // namespace heritage::vehicles::tires

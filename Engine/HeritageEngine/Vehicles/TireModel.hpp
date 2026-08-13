#pragma once

#include "VehiclePrecision.hpp"
#include "Tires/MagicFormula/MagicFormula62.hpp"
#include "Tires/MagicFormula/TirePropertyFile.hpp"
#include "Tires/MotorcycleTireProfile.hpp"
#include "Tires/TireSlipDynamics.hpp"
#include "Tires/TireContactGeometry.hpp"
#include "Tires/TireRigidRing.hpp"
#include "Tires/TireRoadEnveloping.hpp"
#include "Tires/TireThermal.hpp"
#include "Tires/TireWear.hpp"
#include "Tires/TireSurfaceInteraction.hpp"
#include "Tires/TireWetSurfaceInteraction.hpp"
#include "Tires/TireWinterSurfaceInteraction.hpp"
#include "Tires/TireShallowGranularInteraction.hpp"
#include "Tires/TireDeformableTerrainInteraction.hpp"

#include <cstddef>
#include <string>

namespace heritage::vehicles {

enum class TireProviderKind
{
    // Public MF-Tyre 6.x / FITTYP=62-compatible steady-state branch.
    MagicFormula62,

    // Same MF force/moment branch plus motorcycle contour metadata. Contact
    // geometry is evaluated separately by MotorcycleTireProfile.
    MagicFormula62Motorcycle,

    // Step 29G Heritage generalized curve retained as a regression/fallback
    // provider while MF62 datasets are being authored.
    LegacyGeneralizedRoad
};

// TIRE01/TIRE02 tire data. Existing Heritage controls remain as a compatibility
// bridge: while magicFormulaUsesLegacySeed is true they seed a coherent MF6.2
// coefficient set at evaluation time. TIRE02 property-file import sets it false
// and provides an identified coefficient set without changing the solver API.
struct TireModelDescription
{
    VehicleScalar nominalLoad = 3500.0;
    VehicleScalar peakFriction = 1.15;
    VehicleScalar longitudinalStiffness = 90000.0;
    VehicleScalar corneringStiffness = 80000.0;
    VehicleScalar loadSensitivity = 0.12;
    VehicleScalar longitudinalRelaxationLength = 0.35;
    VehicleScalar lateralRelaxationLength = 0.45;
    VehicleScalar wheelInertia = 1.55;
    VehicleScalar pneumaticTrail = 0.075;

    // Compatibility curve controls used by the legacy provider and by the
    // MF62 seed bridge. Existing SetTireModel callers therefore remain valid.
    VehicleScalar stiffnessLoadExponent = 0.85;
    VehicleScalar longitudinalShapeFactor = 1.65;
    VehicleScalar lateralShapeFactor = 1.30;
    VehicleScalar longitudinalCurvatureFactor = 0.20;
    VehicleScalar lateralCurvatureFactor = 0.15;
    VehicleScalar combinedSlipExponent = 2.0;
    VehicleScalar pneumaticTrailFalloff = 0.70;

    TireProviderKind provider = TireProviderKind::MagicFormula62;
    bool magicFormulaUsesLegacySeed = true;
    tires::MagicFormula62Parameters magicFormula;
    // Identified/construction datum. Runtime setup pressure may be changed by
    // the driver or tire lab without moving this reference: every pressure-
    // sensitive structural/force law compares the live gauge pressure against
    // the pressure at which its parameters were identified.
    VehicleScalar referenceInflationPressurePa = 220000.0;
    VehicleScalar inflationPressurePa = 220000.0;
    tires::MotorcycleTireProfileDescription motorcycleProfile;
    tires::TireSlipDynamicsCoefficients slipDynamicsCoefficients;
    tires::TireContactGeometryDescription contactGeometry;
    tires::TireRigidRingDescription rigidRing;
    tires::TireRoadEnvelopingDescription roadEnveloping;
    tires::TireThermalDescription thermal;
    tires::TireWearDescription wear;
    tires::TireContaminationDescription contamination;
    tires::TireWetSurfaceDescription wetSurface;
    tires::TireWinterSurfaceDescription winterSurface;
    tires::TireShallowGranularDescription shallowGranularSurface;
    tires::TireDeformableTerrainDescription deformableTerrainSurface;

    // TIRE02 parameter provenance. These fields never alter the equations;
    // they make it explicit whether a wheel is running a fitted/imported
    // dataset or the compatibility seed.
    bool importedPropertyFile = false;
    int importedFitType = 0;
    std::string parameterSource;
    std::string parameterProvenance;
    std::string parameterTireSide;
    VehicleScalar parameterConfidence = 0.0;
    std::size_t importedMappedParameterCount = 0;
    std::size_t importedUnsupportedParameterCount = 0;
};

struct TireContactInput
{
    VehicleScalar normalLoad = 0.0;
    VehicleScalar longitudinalSlip = 0.0;
    VehicleScalar slipAngleRadians = 0.0;
    VehicleScalar camberAngleRadians = 0.0;
    VehicleScalar forwardSpeedMps = 0.0;
    VehicleScalar turnSlipPerM = 0.0;
    VehicleScalar contactPatchTurnMomentNm = 0.0;
    VehicleScalar wheelRadiusM = 0.0;
    // Negative means unspecified/use the tire description. Zero is a valid
    // explicitly commanded gauge pressure for flat-tire simulation.
    VehicleScalar inflationPressurePa = -1.0;
    VehicleScalar frictionMultiplier = 1.0;
    VehicleScalar stiffnessMultiplier = 1.0;
};

struct TireForceResult
{
    VehicleScalar longitudinalForce = 0.0;
    VehicleScalar lateralForce = 0.0;
    VehicleScalar pureLongitudinalForce = 0.0;
    VehicleScalar pureLateralForce = 0.0;
    VehicleScalar effectiveFriction = 0.0;
    VehicleScalar gripUtilization = 0.0;
    VehicleScalar combinedSlipScale = 1.0;
    VehicleScalar pneumaticTrail = 0.0;
    VehicleScalar aligningTorque = 0.0;

    // MF6.x outputs retained for steering/FFB, wheel dynamics and diagnostics.
    VehicleScalar overturningMoment = 0.0;
    VehicleScalar rollingResistanceMoment = 0.0;
    VehicleScalar residualAligningTorque = 0.0;
    VehicleScalar longitudinalSlipStiffness = 0.0;
    VehicleScalar corneringStiffness = 0.0;
    VehicleScalar camberStiffness = 0.0;
    VehicleScalar combinedLongitudinalWeight = 1.0;
    VehicleScalar combinedLateralWeight = 1.0;
    VehicleScalar turnSlipMoment = 0.0;
    VehicleScalar normalizedTurnSlip = 0.0;
    VehicleScalar turnSlipLongitudinalReduction = 1.0;
    VehicleScalar turnSlipLateralReduction = 1.0;
    VehicleScalar turnSlipCorneringReduction = 1.0;
    VehicleScalar turnSlipTrailReduction = 1.0;

    // Motorcycle contour telemetry. The current chassis contact ray remains
    // authoritative until the future SWIFT/enveloping contact provider is
    // promoted; this value is already usable by motorcycle wheel kinematics.
    bool motorcycleContourValid = false;
    VehicleScalar motorcycleContactLateralOffset = 0.0;
    VehicleScalar motorcycleCenterToRoad = 0.0;
};

bool validTireModelDescription(const TireModelDescription& value);

// Maps the long-standing Heritage tuning controls into a coherent MF6.2 seed
// set. It is intentionally an approximation, not a replacement for tire-rig
// identification. It lets existing vehicle definitions adopt the new solver
// without an abrupt content break.
tires::MagicFormula62Parameters seededMagicFormula62Parameters(
    const TireModelDescription& description,
    VehicleScalar wheelRadiusM);


TireModelDescription tireModelDescriptionFromPropertyFile(
    const tires::TirePropertyFileData& propertyFile,
    TireProviderKind provider,
    const std::string& source,
    const std::string& provenance,
    VehicleScalar confidence,
    const TireModelDescription& fallback = {});

TireForceResult evaluateAdvancedRoadTire(
    const TireModelDescription& description,
    const TireContactInput& input);

} // namespace heritage::vehicles

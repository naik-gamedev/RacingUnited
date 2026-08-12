#pragma once

#include "MagicFormula62.hpp"
#include "../MotorcycleTireProfile.hpp"
#include "../TireThermal.hpp"
#include "../TireWear.hpp"
#include "../TireSurfaceInteraction.hpp"
#include "../TireWetSurfaceInteraction.hpp"
#include "../TireWinterSurfaceInteraction.hpp"
#include "../TireShallowGranularInteraction.hpp"
#include "../TireDeformableTerrainInteraction.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace heritage::vehicles::tires {

// TIRE02 keeps the property-file layer independent from the vehicle solver.
// Human-readable MF-Tyre/MF-Swift style .tir files are parsed into this
// immutable data object, which can then be applied to any wheel/provider.
struct TirePropertyFileData
{
    MagicFormula62Parameters magicFormula;
    MotorcycleTireProfileDescription motorcycleProfile;
    TireThermalDescription thermal;
    bool hasHeritageThermalModel = false;
    TireWearDescription wear;
    bool hasHeritageTreadState = false;
    TireContaminationDescription contamination;
    bool hasHeritageContaminationModel = false;
    TireWetSurfaceDescription wetSurface;
    bool hasHeritageWetSurfaceModel = false;
    TireWinterSurfaceDescription winterSurface;
    bool hasHeritageWinterSurfaceModel = false;
    TireShallowGranularDescription shallowGranularSurface;
    bool hasHeritageShallowGranularModel = false;
    TireDeformableTerrainDescription deformableTerrainSurface;
    bool hasHeritageDeformableTerrainModel = false;

    int fitType = 0;
    std::string tireSide;

    VehicleScalar widthM = 0.0;
    VehicleScalar rimRadiusM = 0.0;
    VehicleScalar rimWidthM = 0.0;
    VehicleScalar aspectRatio = 0.0;

    VehicleScalar inflationPressurePa = 0.0;
    VehicleScalar tireMassKg = 0.0;
    VehicleScalar tireDiametralInertiaKgM2 = 0.0;
    VehicleScalar tirePolarInertiaKgM2 = 0.0;
    VehicleScalar beltMassKg = 0.0;
    VehicleScalar beltDiametralInertiaKgM2 = 0.0;
    VehicleScalar beltPolarInertiaKgM2 = 0.0;

    VehicleScalar verticalStiffnessNPerM = 0.0;
    VehicleScalar verticalDampingNsPerM = 0.0;

    // TIRE04 public MF load/velocity-dependent effective rolling-radius data.
    // The booleans prevent partial real datasets from silently inheriting
    // synthetic coefficients.
    bool hasEffectiveRollingRadiusModel = false;
    VehicleScalar bReff = 0.0;
    VehicleScalar dReff = 0.0;
    VehicleScalar fReff = 0.0;
    VehicleScalar qRe0 = 1.0;
    VehicleScalar qV1 = 0.0;
    VehicleScalar qV2 = 0.0;

    // Finite contact-patch property-file vocabulary. TIRE04 activates Q_RA1/2
    // for contact length. Q_RB1/2 are preserved for the later SWIFT contact/
    // enveloping provider until their complete public equation is integrated.
    bool hasContactPatchLengthModel = false;
    VehicleScalar qRa1 = 0.0;
    VehicleScalar qRa2 = 0.0;
    VehicleScalar qRb1 = 0.0;
    VehicleScalar qRb2 = 0.0;

    // TIRE05 public MF-Swift structural-mode vocabulary. Heritage maps the
    // identified static stiffness, natural-frequency and damping quantities
    // into its independent rigid-ring state provider. Rotational terms are
    // preserved even where TIRE05 does not yet actively couple them.
    bool hasRigidRingModel = false;
    VehicleScalar structuralLongitudinalStiffnessNPerM = 0.0;
    VehicleScalar structuralLateralStiffnessNPerM = 0.0;
    VehicleScalar structuralYawStiffnessNmPerRad = 0.0;
    VehicleScalar structuralFrequencyLongHz = 0.0;
    VehicleScalar structuralFrequencyLatHz = 0.0;
    VehicleScalar structuralFrequencyYawHz = 0.0;
    VehicleScalar structuralFrequencyWindupHz = 0.0;
    VehicleScalar structuralDampingLong = 0.0;
    VehicleScalar structuralDampingLat = 0.0;
    VehicleScalar structuralDampingYaw = 0.0;
    VehicleScalar structuralDampingWindup = 0.0;
    VehicleScalar structuralResidualDamping = 0.0;
    VehicleScalar structuralLowSpeedDamping = 0.0;
    VehicleScalar structuralQBvx = 0.0;
    VehicleScalar structuralQBvt = 0.0;

    // TIRE05 tandem-cam road-enveloping vocabulary from [CONTACT_PATCH].
    bool hasRoadEnvelopingModel = false;
    VehicleScalar ellipseShiftScale = 0.0;
    VehicleScalar ellipseLengthM = 0.0;
    VehicleScalar ellipseHeightM = 0.0;
    VehicleScalar ellipseOrder = 0.0;
    VehicleScalar ellipseMaximumStepM = 0.0;
    int ellipseWidthCount = 1;
    int ellipseSideCount = 1;
    VehicleScalar envelopeHeightAttenuation = 1.0;
    VehicleScalar envelopePlaneAngleAttenuation = 1.0;
    VehicleScalar rigidRingLowSpeedDampingScale = 1.0;
    VehicleScalar rigidRingLowSpeedThresholdMps = 1.0;

    // Raw transient coefficients are preserved for the later transient/Swift
    // milestones even though TIRE02 does not yet claim the complete 6.2
    // relaxation-length equations.
    VehicleScalar pTx1 = 0.0;
    VehicleScalar pTx2 = 0.0;
    VehicleScalar pTx3 = 0.0;
    VehicleScalar pTy1 = 0.0;
    VehicleScalar pTy2 = 0.0;
    VehicleScalar lSgKappa = 1.0;
    VehicleScalar lSgAlpha = 1.0;

    bool hasMotorcycleContour = false;
    bool temperatureVelocityRequested = false;
    bool obfuscated = false;

    // Import diagnostics / authoring provenance. Unknown parameters are not a
    // hard error: newer .tir files can therefore be inspected without silently
    // pretending unsupported mechanisms are active.
    std::size_t parsedAssignmentCount = 0;
    std::size_t mappedAssignmentCount = 0;
    std::size_t unsupportedAssignmentCount = 0;
    std::vector<std::string> unsupportedParameters;
};

struct TirePropertyFileLoadResult
{
    bool success = false;
    TirePropertyFileData data;
    std::string errorMessage;
    std::vector<std::string> warnings;
};

TirePropertyFileLoadResult parseTirePropertyFileText(
    const std::string& text,
    const std::string& sourceLabel = {});

TirePropertyFileLoadResult loadTirePropertyFile(
    const std::filesystem::path& path);

} // namespace heritage::vehicles::tires

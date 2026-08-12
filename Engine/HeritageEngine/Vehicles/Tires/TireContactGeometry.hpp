#pragma once

#include "../VehiclePrecision.hpp"

namespace heritage::vehicles::tires {

// TIRE04 separates quasi-static tire geometry from both the MF force equations
// and the TIRE03 torsional contact-patch state. This provider owns the loaded /
// effective rolling radii and the finite footprint dimensions that later SWIFT-
// like rigid-ring and road-enveloping providers can consume.
struct TireContactGeometryDescription
{
    VehicleScalar unloadedRadiusM = 0.0;
    VehicleScalar nominalLoadN = 0.0;
    VehicleScalar verticalStiffnessNPerM = 0.0;
    VehicleScalar nominalWidthM = 0.0;
    VehicleScalar rimRadiusM = 0.0;
    VehicleScalar referenceSpeedMps = 0.0;

    // Public MF load/velocity-dependent effective rolling radius coefficients.
    bool useMagicFormulaEffectiveRadius = false;
    VehicleScalar bReff = 0.0;
    VehicleScalar dReff = 0.0;
    VehicleScalar fReff = 0.0;
    VehicleScalar qRe0 = 1.0;
    VehicleScalar qV1 = 0.0;

    // Public MF-Swift property-file vocabulary for contact length. TIRE04 uses
    // the documented square-root + linear contact-length relation. Width is
    // independently reconstructed from load / inflation pressure so we do not
    // invent an undocumented Q_RB root exponent.
    bool useMagicFormulaContactLength = false;
    VehicleScalar qRa1 = 0.0;
    VehicleScalar qRa2 = 0.0;

    // Preserved for the later SWIFT/enveloping stage. They are parsed and
    // exposed in the data model but deliberately not evaluated in TIRE04.
    VehicleScalar qRb1 = 0.0;
    VehicleScalar qRb2 = 0.0;
};

struct TireContactGeometryInput
{
    VehicleScalar normalLoadN = 0.0;
    VehicleScalar wheelAngularVelocityRadPerS = 0.0;
    VehicleScalar inflationPressurePa = 0.0;

    // When true, this is the authoritative radial deflection from the high-rate
    // unsprung-mass tire state. The massless compatibility path can set false
    // and let the geometry provider infer a quasi-static deflection from Fz/Cz.
    bool verticalDeflectionKnown = false;
    VehicleScalar verticalDeflectionM = 0.0;
};

struct TireContactGeometryOutput
{
    bool valid = false;
    VehicleScalar freeRollingRadiusM = 0.0;
    VehicleScalar loadedRadiusM = 0.0;
    VehicleScalar effectiveRollingRadiusM = 0.0;
    VehicleScalar verticalDeflectionM = 0.0;
    VehicleScalar normalizedDeflection = 0.0;
    VehicleScalar contactPatchLengthM = 0.0;
    VehicleScalar contactPatchWidthM = 0.0;
    VehicleScalar contactPatchAreaM2 = 0.0;
};

bool validTireContactGeometryDescription(
    const TireContactGeometryDescription& description);

TireContactGeometryOutput evaluateTireContactGeometry(
    const TireContactGeometryDescription& description,
    const TireContactGeometryInput& input);

} // namespace heritage::vehicles::tires

#include "TirePropertyImportInternal.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles::tires::authoring_detail {

void mapPhysicalStructureAndRanges(
    const RawPropertyFile& raw,
    TirePropertyFileLoadResult& result,
    const UnitSystem& units)
{
    VehicleScalar value = 0.0;
    mapScalar(raw, result.data, "INERTIA", "MASS", result.data.tireMassKg, units.massToKg);
    mapScalar(raw, result.data, "INERTIA", "IXX", result.data.tireDiametralInertiaKgM2, units.inertiaToKgM2());
    mapScalar(raw, result.data, "INERTIA", "IYY", result.data.tirePolarInertiaKgM2, units.inertiaToKgM2());
    mapScalar(raw, result.data, "INERTIA", "BELT_MASS", result.data.beltMassKg, units.massToKg);
    mapScalar(raw, result.data, "INERTIA", "BELT_IXX", result.data.beltDiametralInertiaKgM2, units.inertiaToKgM2());
    mapScalar(raw, result.data, "INERTIA", "BELT_IYY", result.data.beltPolarInertiaKgM2, units.inertiaToKgM2());

    mapScalar(raw, result.data, "VERTICAL", "FNOMIN", result.data.magicFormula.nominalLoadN, units.forceToN);
    mapScalar(raw, result.data, "VERTICAL", "VERTICAL_STIFFNESS", result.data.verticalStiffnessNPerM, units.stiffnessToNPerM());
    mapScalar(raw, result.data, "VERTICAL", "VERTICAL_DAMPING", result.data.verticalDampingNsPerM, units.dampingToNsPerM());

    const bool hasBReff = mapScalar(raw, result.data, "VERTICAL", "BREFF", result.data.bReff);
    const bool hasDReff = mapScalar(raw, result.data, "VERTICAL", "DREFF", result.data.dReff);
    const bool hasFReff = mapScalar(raw, result.data, "VERTICAL", "FREFF", result.data.fReff);
    const bool hasQRe0 = mapScalar(raw, result.data, "VERTICAL", "Q_RE0", result.data.qRe0);
    const bool hasQV1 = mapScalar(raw, result.data, "VERTICAL", "Q_V1", result.data.qV1);
    mapScalar(raw, result.data, "VERTICAL", "Q_V2", result.data.qV2);
    result.data.hasEffectiveRollingRadiusModel = hasBReff && hasDReff
        && hasFReff && hasQRe0 && hasQV1
        && result.data.verticalStiffnessNPerM > 0.0;
    const int effectiveRadiusTerms = static_cast<int>(hasBReff)
        + static_cast<int>(hasDReff) + static_cast<int>(hasFReff)
        + static_cast<int>(hasQRe0) + static_cast<int>(hasQV1);
    if (effectiveRadiusTerms > 0 && !result.data.hasEffectiveRollingRadiusModel)
    {
        result.warnings.push_back(
            "Partial BREFF/DREFF/FREFF/Q_RE0/Q_V1 dataset preserved; "
            "TIRE04 effective-radius model remains disabled for this file.");
    }

    const bool hasQRa1 = mapScalar(raw, result.data, "CONTACT_PATCH", "Q_RA1", result.data.qRa1);
    const bool hasQRa2 = mapScalar(raw, result.data, "CONTACT_PATCH", "Q_RA2", result.data.qRa2);
    mapScalar(raw, result.data, "CONTACT_PATCH", "Q_RB1", result.data.qRb1);
    mapScalar(raw, result.data, "CONTACT_PATCH", "Q_RB2", result.data.qRb2);
    result.data.hasContactPatchLengthModel = hasQRa1 && hasQRa2;
    if (hasQRa1 != hasQRa2)
    {
        result.warnings.push_back(
            "Partial Q_RA1/Q_RA2 contact-length dataset preserved; "
            "TIRE04 MF contact-length relation remains disabled for this file.");
    }

    // TIRE05 structural mode parameters. Static stiffness and modal frequency
    // are intentionally both preserved because the public MF-Swift file format
    // identifies both; Heritage uses stiffness for equilibrium compliance and
    // FREQ_* for the state poles rather than forcing one to be derived from the
    // other.
    const bool hasStructuralLong = mapScalar(raw, result.data, "STRUCTURAL",
        "LONGITUDINAL_STIFFNESS",
        result.data.structuralLongitudinalStiffnessNPerM,
        units.stiffnessToNPerM());
    const bool hasStructuralLat = mapScalar(raw, result.data, "STRUCTURAL",
        "LATERAL_STIFFNESS",
        result.data.structuralLateralStiffnessNPerM,
        units.stiffnessToNPerM());
    if (numberFrom(raw, "STRUCTURAL", "YAW_STIFFNESS", value))
    {
        result.data.structuralYawStiffnessNmPerRad = value
            * units.forceToN * units.lengthToM
            / std::max(units.angleToRad, VehicleScalar{1.0e-18});
        ++result.data.mappedAssignmentCount;
    }
    const bool hasFreqLong = mapScalar(raw, result.data, "STRUCTURAL",
        "FREQ_LONG", result.data.structuralFrequencyLongHz);
    const bool hasFreqLat = mapScalar(raw, result.data, "STRUCTURAL",
        "FREQ_LAT", result.data.structuralFrequencyLatHz);
    mapScalar(raw, result.data, "STRUCTURAL", "FREQ_YAW",
        result.data.structuralFrequencyYawHz);
    mapScalar(raw, result.data, "STRUCTURAL", "FREQ_WINDUP",
        result.data.structuralFrequencyWindupHz);
    const bool hasDampLong = mapScalar(raw, result.data, "STRUCTURAL",
        "DAMP_LONG", result.data.structuralDampingLong);
    const bool hasDampLat = mapScalar(raw, result.data, "STRUCTURAL",
        "DAMP_LAT", result.data.structuralDampingLat);
    mapScalar(raw, result.data, "STRUCTURAL", "DAMP_YAW",
        result.data.structuralDampingYaw);
    mapScalar(raw, result.data, "STRUCTURAL", "DAMP_WINDUP",
        result.data.structuralDampingWindup);
    mapScalar(raw, result.data, "STRUCTURAL", "DAMP_RESIDUAL",
        result.data.structuralResidualDamping);
    mapScalar(raw, result.data, "STRUCTURAL", "DAMP_VLOW",
        result.data.structuralLowSpeedDamping);
    mapScalar(raw, result.data, "STRUCTURAL", "Q_BVX",
        result.data.structuralQBvx);
    mapScalar(raw, result.data, "STRUCTURAL", "Q_BVT",
        result.data.structuralQBvt);
    // Preserve currently inactive structural scaling terms as mapped data so a
    // 2412/2512-era human-readable file does not falsely look unsupported.
    VehicleScalar ignoredStructural = 0.0;
    for (const char* key : { "PCFX1", "PCFX2", "PCFX3",
                             "PCFY1", "PCFY2", "PCFY3", "PCMZ1" })
        mapScalar(raw, result.data, "STRUCTURAL", key, ignoredStructural);

    result.data.hasRigidRingModel =
        hasStructuralLong && hasStructuralLat
        && hasFreqLong && hasFreqLat
        && hasDampLong && hasDampLat
        && result.data.beltMassKg > 0.05;
    if ((hasStructuralLong || hasStructuralLat || hasFreqLong || hasFreqLat)
        && !result.data.hasRigidRingModel)
    {
        result.warnings.push_back(
            "Partial [STRUCTURAL] rigid-ring dataset preserved; TIRE05 rigid-ring dynamics remain disabled for this file.");
    }

    const bool hasEllipsShift = mapScalar(raw, result.data, "CONTACT_PATCH",
        "ELLIPS_SHIFT", result.data.ellipseShiftScale);
    const bool hasEllipsLength = mapScalar(raw, result.data, "CONTACT_PATCH",
        "ELLIPS_LENGTH", result.data.ellipseLengthM, units.lengthToM);
    const bool hasEllipsHeight = mapScalar(raw, result.data, "CONTACT_PATCH",
        "ELLIPS_HEIGHT", result.data.ellipseHeightM, units.lengthToM);
    const bool hasEllipsOrder = mapScalar(raw, result.data, "CONTACT_PATCH",
        "ELLIPS_ORDER", result.data.ellipseOrder);
    const bool hasEllipsStep = mapScalar(raw, result.data, "CONTACT_PATCH",
        "ELLIPS_MAX_STEP", result.data.ellipseMaximumStepM, units.lengthToM);
    int ellipsNWidth = 1;
    if (integerFrom(raw, "CONTACT_PATCH", "ELLIPS_NWIDTH", ellipsNWidth))
    {
        ++result.data.mappedAssignmentCount;
        result.data.ellipseWidthCount = ellipsNWidth;
    }
    int ellipsNLength = 1;
    if (integerFrom(raw, "CONTACT_PATCH", "ELLIPS_NLENGTH", ellipsNLength))
    {
        ++result.data.mappedAssignmentCount;
        result.data.ellipseSideCount = ellipsNLength;
    }
    mapScalar(raw, result.data, "CONTACT_PATCH", "ENV_C1",
        result.data.envelopeHeightAttenuation);
    mapScalar(raw, result.data, "CONTACT_PATCH", "ENV_C2",
        result.data.envelopePlaneAngleAttenuation);
    // FITTYP=70-only geometry terms are retained for future use.
    VehicleScalar ignoredContact = 0.0;
    for (const char* key : { "Q_A1", "Q_A2", "Q_CFG1" })
        mapScalar(raw, result.data, "CONTACT_PATCH", key, ignoredContact);

    result.data.hasRoadEnvelopingModel = hasEllipsShift
        && hasEllipsLength && hasEllipsHeight && hasEllipsOrder && hasEllipsStep;
    if ((hasEllipsShift || hasEllipsLength || hasEllipsHeight
            || hasEllipsOrder || hasEllipsStep)
        && !result.data.hasRoadEnvelopingModel)
    {
        result.warnings.push_back(
            "Partial ELLIPS_* road-enveloping dataset preserved; TIRE05 enveloping remains disabled for this file.");
    }

    result.data.motorcycleProfile.tireWidthM = result.data.widthM > 0.0
        ? result.data.widthM : result.data.motorcycleProfile.tireWidthM;
    VehicleScalar contourA = 0.0;
    VehicleScalar contourB = 0.0;
    const bool hasA = numberFrom(raw, "VERTICAL", "MC_CONTOUR_A", contourA);
    const bool hasB = numberFrom(raw, "VERTICAL", "MC_CONTOUR_B", contourB);
    if (hasA) ++result.data.mappedAssignmentCount;
    if (hasB) ++result.data.mappedAssignmentCount;
    if (hasA || hasB)
    {
        result.data.motorcycleProfile.mcContourA = std::abs(contourA);
        result.data.motorcycleProfile.mcContourB = std::abs(contourB);
        result.data.hasMotorcycleContour = contourA > 0.0 && contourB > 0.0;
    }

    VehicleScalar minimum = 0.0;
    VehicleScalar maximum = 0.0;
    if (numberFrom(raw, "INFLATION_PRESSURE_RANGE", "PRESMIN", minimum))
    {
        result.data.magicFormula.minimumPressurePa = minimum * units.pressureToPa();
        ++result.data.mappedAssignmentCount;
    }
    if (numberFrom(raw, "INFLATION_PRESSURE_RANGE", "PRESMAX", maximum))
    {
        result.data.magicFormula.maximumPressurePa = maximum * units.pressureToPa();
        ++result.data.mappedAssignmentCount;
    }
    if (numberFrom(raw, "VERTICAL_FORCE_RANGE", "FZMIN", minimum))
    {
        result.data.magicFormula.minimumLoadN = minimum * units.forceToN;
        ++result.data.mappedAssignmentCount;
    }
    if (numberFrom(raw, "VERTICAL_FORCE_RANGE", "FZMAX", maximum))
    {
        result.data.magicFormula.maximumLoadN = maximum * units.forceToN;
        ++result.data.mappedAssignmentCount;
    }
    if (numberFrom(raw, "LONG_SLIP_RANGE", "KPUMIN", minimum)) ++result.data.mappedAssignmentCount;
    else minimum = -result.data.magicFormula.maximumAbsLongitudinalSlip;
    if (numberFrom(raw, "LONG_SLIP_RANGE", "KPUMAX", maximum)) ++result.data.mappedAssignmentCount;
    else maximum = result.data.magicFormula.maximumAbsLongitudinalSlip;
    result.data.magicFormula.maximumAbsLongitudinalSlip = std::max(std::abs(minimum), std::abs(maximum));

    mapAngleLimit(raw, result.data, units, "SLIP_ANGLE_RANGE", "ALPMIN", "ALPMAX", result.data.magicFormula.maximumAbsSlipAngleRadians);
    mapAngleLimit(raw, result.data, units, "INCLINATION_ANGLE_RANGE", "CAMMIN", "CAMMAX", result.data.magicFormula.maximumAbsCamberRadians);
}

} // namespace heritage::vehicles::tires::authoring_detail

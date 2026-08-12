#include "TirePropertyImportInternal.hpp"

namespace heritage::vehicles::tires::authoring_detail {

bool mapModelDimensionAndOperating(
    const RawPropertyFile& raw,
    TirePropertyFileLoadResult& result,
    const std::string& sourceLabel,
    UnitSystem& units)
{
    if (!integerFrom(raw, "MODEL", "FITTYP", result.data.fitType))
    {
        result.errorMessage = "Missing or invalid [MODEL] FITTYP in tire property file";
        if (!sourceLabel.empty()) result.errorMessage += " '" + sourceLabel + "'";
        result.errorMessage += ".";
        return false;
    }
    ++result.data.mappedAssignmentCount;

    if (result.data.fitType != 62 && result.data.fitType != 70)
    {
        result.errorMessage = "Heritage TIRE02 currently accepts FITTYP=62 or FITTYP=70 (MF6.2 core). The file requested FITTYP="
            + std::to_string(result.data.fitType) + ".";
        return false;
    }
    if (result.data.fitType == 70)
    {
        result.data.temperatureVelocityRequested = true;
        result.warnings.push_back("FITTYP=70 loaded through the MF6.2 steady-state core; Temperature & Velocity coefficients are preserved as unsupported until the thermal milestone.");
    }

    loadUnits(raw, units, result.warnings);
    // Count unit assignments we understand.
    for (const char* key : { "LENGTH", "FORCE", "ANGLE", "MASS", "TIME", "TEMPERATURE" })
        if (rawValue(raw, "UNITS", key)) ++result.data.mappedAssignmentCount;

    result.data.tireSide = upper(stringFrom(raw, "MODEL", "TYRESIDE"));
    if (!result.data.tireSide.empty()) ++result.data.mappedAssignmentCount;

    VehicleScalar value = 0.0;
    if (mapScalar(raw, result.data, "MODEL", "LONGVL", result.data.magicFormula.referenceSpeedMps, units.speedToMps())) {}
    int tvModel = 0;
    if (integerFrom(raw, "MODEL", "TV_MODEL", tvModel))
    {
        ++result.data.mappedAssignmentCount;
        result.data.temperatureVelocityRequested = result.data.temperatureVelocityRequested || tvModel != 0;
    }
    mapScalar(raw, result.data, "MODEL", "DAMP_LSG",
        result.data.rigidRingLowSpeedDampingScale);
    mapScalar(raw, result.data, "MODEL", "VX_STBL",
        result.data.rigidRingLowSpeedThresholdMps, units.speedToMps());

    mapScalar(raw, result.data, "DIMENSION", "UNLOADED_RADIUS", result.data.magicFormula.unloadedRadiusM, units.lengthToM);
    mapScalar(raw, result.data, "DIMENSION", "WIDTH", result.data.widthM, units.lengthToM);
    mapScalar(raw, result.data, "DIMENSION", "RIM_RADIUS", result.data.rimRadiusM, units.lengthToM);
    mapScalar(raw, result.data, "DIMENSION", "RIM_WIDTH", result.data.rimWidthM, units.lengthToM);
    mapScalar(raw, result.data, "DIMENSION", "ASPECT_RATIO", result.data.aspectRatio);

    if (numberFrom(raw, "OPERATING_CONDITIONS", "NOMPRES", value))
    {
        result.data.magicFormula.nominalPressurePa = value * units.pressureToPa();
        ++result.data.mappedAssignmentCount;
    }
    if (numberFrom(raw, "OPERATING_CONDITIONS", "INFLPRES", value))
    {
        result.data.inflationPressurePa = value * units.pressureToPa();
        ++result.data.mappedAssignmentCount;
    }
    else
    {
        result.data.inflationPressurePa = result.data.magicFormula.nominalPressurePa;
    }
    return true;
}

} // namespace heritage::vehicles::tires::authoring_detail

#include "TirePropertyFile.hpp"
#include "../Authoring/TirePropertyImportInternal.hpp"

#include <fstream>
#include <sstream>

namespace heritage::vehicles::tires {

TirePropertyFileLoadResult parseTirePropertyFileText(
    const std::string& text,
    const std::string& sourceLabel)
{
    using namespace authoring_detail;

    TirePropertyFileLoadResult result;
    RawPropertyFile raw = parseRaw(text);
    acceptPluralSectionAliases(raw);
    result.data.parsedAssignmentCount = raw.assignmentCount;
    resetImportedCoefficientDefaults(result.data.magicFormula);

    if (raw.sections.find("OBFUSCATED") != raw.sections.end())
    {
        result.data.obfuscated = true;
        result.errorMessage = "Obfuscated TIR data is proprietary/binary and is not supported by Heritage's clean-room importer.";
        return result;
    }

    UnitSystem units;
    if (!mapModelDimensionAndOperating(raw, result, sourceLabel, units))
        return result;
    if (!mapHeritageExtensions(raw, result))
        return result;

    mapPhysicalStructureAndRanges(raw, result, units);

    std::string missingCore;
    if (!hasCoreForceCoefficients(raw, missingCore))
    {
        result.errorMessage = "TIRE02 requires explicit MF6.2 core force coefficients; missing: " + missingCore + ".";
        return result;
    }

    mapMagicFormulaCoefficients(raw, result.data);
    enumerateUnsupported(raw, result.data);

    if (!finalizeImportedTireProperty(result))
        return result;
    return result;
}

TirePropertyFileLoadResult loadTirePropertyFile(const std::filesystem::path& path)
{
    TirePropertyFileLoadResult result;
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        result.errorMessage = "Could not open tire property file: " + path.string();
        return result;
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    if (!stream.good() && !stream.eof())
    {
        result.errorMessage = "Could not read tire property file: " + path.string();
        return result;
    }
    return parseTirePropertyFileText(buffer.str(), path.string());
}

} // namespace heritage::vehicles::tires

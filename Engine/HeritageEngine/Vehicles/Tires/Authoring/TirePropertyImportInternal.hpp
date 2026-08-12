#pragma once

#include "../MagicFormula/TirePropertyFile.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace heritage::vehicles::tires::authoring_detail {

// Private CLEAN11 import contract.  These types/functions are intentionally
// not part of the public tire API; they let the property-file importer be
// divided by authoring responsibility without leaking parser internals into
// vehicle simulation code.
struct UnitSystem
{
    VehicleScalar lengthToM = 1.0;
    VehicleScalar forceToN = 1.0;
    VehicleScalar angleToRad = 1.0;
    VehicleScalar massToKg = 1.0;
    VehicleScalar timeToS = 1.0;

    VehicleScalar pressureToPa() const
    {
        return forceToN / std::max(lengthToM * lengthToM, VehicleScalar{1.0e-18});
    }
    VehicleScalar speedToMps() const
    {
        return lengthToM / std::max(timeToS, VehicleScalar{1.0e-18});
    }
    VehicleScalar inertiaToKgM2() const
    {
        return massToKg * lengthToM * lengthToM;
    }
    VehicleScalar stiffnessToNPerM() const
    {
        return forceToN / std::max(lengthToM, VehicleScalar{1.0e-18});
    }
    VehicleScalar dampingToNsPerM() const
    {
        return forceToN * timeToS / std::max(lengthToM, VehicleScalar{1.0e-18});
    }
};

struct RawPropertyFile
{
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> sections;
    std::size_t assignmentCount = 0;
};

std::string trim(std::string value);
std::string upper(std::string value);
std::string stripQuotes(std::string value);
std::string removeInlineComment(const std::string& line);
bool parseNumber(std::string text, VehicleScalar& value);
bool parseInteger(const std::string& text, int& value);
RawPropertyFile parseRaw(const std::string& text);
const std::string* rawValue(const RawPropertyFile& raw, std::string_view section, std::string_view key);
bool numberFrom(const RawPropertyFile& raw, std::string_view section, std::string_view key, VehicleScalar& value);
bool integerFrom(const RawPropertyFile& raw, std::string_view section, std::string_view key, int& value);
std::string stringFrom(const RawPropertyFile& raw, std::string_view section, std::string_view key);
bool unitScale(std::string text, std::string_view kind, VehicleScalar& scale);
bool loadUnits(const RawPropertyFile& raw, UnitSystem& units, std::vector<std::string>& warnings);
void acceptPluralSectionAliases(RawPropertyFile& raw);

void noteUnsupported(TirePropertyFileData& data, const std::string& section, const std::string& key);
bool mapScalar(const RawPropertyFile& raw, TirePropertyFileData& data, const char* section, const char* key, VehicleScalar& destination, VehicleScalar scale = 1.0);
bool mapAngleLimit(const RawPropertyFile& raw, TirePropertyFileData& data, const UnitSystem& units, const char* section, const char* minKey, const char* maxKey, VehicleScalar& maxAbs);

void resetImportedCoefficientDefaults(MagicFormula62Parameters& p);
bool hasCoreForceCoefficients(const RawPropertyFile& raw, std::string& missing);
void mapMagicFormulaCoefficients(const RawPropertyFile& raw, TirePropertyFileData& data);

bool mapModelDimensionAndOperating(const RawPropertyFile& raw, TirePropertyFileLoadResult& result, const std::string& sourceLabel, UnitSystem& units);
bool mapHeritageExtensions(const RawPropertyFile& raw, TirePropertyFileLoadResult& result);
void mapPhysicalStructureAndRanges(const RawPropertyFile& raw, TirePropertyFileLoadResult& result, const UnitSystem& units);
void enumerateUnsupported(const RawPropertyFile& raw, TirePropertyFileData& data);
bool finalizeImportedTireProperty(TirePropertyFileLoadResult& result);

} // namespace heritage::vehicles::tires::authoring_detail

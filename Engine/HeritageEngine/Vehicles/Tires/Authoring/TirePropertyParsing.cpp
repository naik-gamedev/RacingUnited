#include "TirePropertyImportInternal.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <utility>

namespace heritage::vehicles::tires::authoring_detail {
namespace {
constexpr VehicleScalar kPi = 3.14159265358979323846;
}

std::string trim(std::string value)
{
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string upper(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

std::string stripQuotes(std::string value)
{
    value = trim(std::move(value));
    if (value.size() >= 2)
    {
        const char first = value.front();
        const char last = value.back();
        if ((first == '\'' && last == '\'') || (first == '"' && last == '"'))
            value = value.substr(1, value.size() - 2);
    }
    return trim(std::move(value));
}

std::string removeInlineComment(const std::string& line)
{
    bool singleQuoted = false;
    bool doubleQuoted = false;
    for (std::size_t i = 0; i < line.size(); ++i)
    {
        const char c = line[i];
        if (c == '\'' && !doubleQuoted)
            singleQuoted = !singleQuoted;
        else if (c == '"' && !singleQuoted)
            doubleQuoted = !doubleQuoted;
        else if (!singleQuoted && !doubleQuoted && (c == '$' || c == '!'))
            return line.substr(0, i);
    }
    return line;
}

bool parseNumber(std::string text, VehicleScalar& value)
{
    text = stripQuotes(std::move(text));
    if (text.empty())
        return false;

    // Accept Fortran-style D exponents occasionally found in engineering data.
    for (char& c : text)
    {
        if (c == 'D' || c == 'd')
            c = 'E';
    }

    char* end = nullptr;
    const double parsed = std::strtod(text.c_str(), &end);
    if (!end || end == text.c_str())
        return false;
    while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end)))
        ++end;
    if (*end != '\0' || !std::isfinite(parsed))
        return false;
    value = static_cast<VehicleScalar>(parsed);
    return true;
}

bool parseInteger(const std::string& text, int& value)
{
    VehicleScalar numeric = 0.0;
    if (!parseNumber(text, numeric))
        return false;
    const VehicleScalar rounded = std::round(numeric);
    if (std::abs(numeric - rounded) > 1.0e-9
        || rounded < static_cast<VehicleScalar>(std::numeric_limits<int>::min())
        || rounded > static_cast<VehicleScalar>(std::numeric_limits<int>::max()))
    {
        return false;
    }
    value = static_cast<int>(rounded);
    return true;
}

RawPropertyFile parseRaw(const std::string& text)
{
    RawPropertyFile raw;
    std::istringstream stream(text);
    std::string section;
    std::string line;
    bool firstLine = true;
    while (std::getline(stream, line))
    {
        if (firstLine)
        {
            firstLine = false;
            if (line.size() >= 3
                && static_cast<unsigned char>(line[0]) == 0xEF
                && static_cast<unsigned char>(line[1]) == 0xBB
                && static_cast<unsigned char>(line[2]) == 0xBF)
            {
                line.erase(0, 3);
            }
        }

        line = trim(removeInlineComment(line));
        if (line.empty())
            continue;

        if (line.front() == '[' && line.back() == ']')
        {
            section = upper(trim(line.substr(1, line.size() - 2)));
            continue;
        }

        const std::size_t equals = line.find('=');
        if (equals == std::string::npos)
            continue;

        const std::string key = upper(trim(line.substr(0, equals)));
        const std::string value = trim(line.substr(equals + 1));
        if (key.empty())
            continue;
        raw.sections[section][key] = value;
        ++raw.assignmentCount;
    }
    return raw;
}

const std::string* rawValue(
    const RawPropertyFile& raw,
    std::string_view section,
    std::string_view key)
{
    const auto sectionIt = raw.sections.find(std::string(section));
    if (sectionIt == raw.sections.end())
        return nullptr;
    const auto valueIt = sectionIt->second.find(std::string(key));
    return valueIt == sectionIt->second.end() ? nullptr : &valueIt->second;
}

bool numberFrom(
    const RawPropertyFile& raw,
    std::string_view section,
    std::string_view key,
    VehicleScalar& value)
{
    const std::string* text = rawValue(raw, section, key);
    return text && parseNumber(*text, value);
}

bool integerFrom(
    const RawPropertyFile& raw,
    std::string_view section,
    std::string_view key,
    int& value)
{
    const std::string* text = rawValue(raw, section, key);
    return text && parseInteger(*text, value);
}

std::string stringFrom(
    const RawPropertyFile& raw,
    std::string_view section,
    std::string_view key)
{
    const std::string* text = rawValue(raw, section, key);
    return text ? stripQuotes(*text) : std::string{};
}

bool unitScale(std::string text, std::string_view kind, VehicleScalar& scale)
{
    text = upper(stripQuotes(std::move(text)));
    text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char c) {
        return std::isspace(c) != 0 || c == '_' || c == '-';
    }), text.end());

    if (kind == "LENGTH")
    {
        if (text.empty() || text == "M" || text == "METER" || text == "METRE") scale = 1.0;
        else if (text == "MM" || text == "MILLIMETER" || text == "MILLIMETRE") scale = 0.001;
        else if (text == "CM" || text == "CENTIMETER" || text == "CENTIMETRE") scale = 0.01;
        else if (text == "IN" || text == "INCH") scale = 0.0254;
        else if (text == "FT" || text == "FOOT" || text == "FEET") scale = 0.3048;
        else return false;
        return true;
    }
    if (kind == "FORCE")
    {
        if (text.empty() || text == "N" || text == "NEWTON") scale = 1.0;
        else if (text == "KN" || text == "KILONEWTON") scale = 1000.0;
        else if (text == "LBF" || text == "POUNDFORCE") scale = 4.4482216152605;
        else return false;
        return true;
    }
    if (kind == "ANGLE")
    {
        if (text.empty() || text == "RAD" || text == "RADIAN" || text == "RADIANS") scale = 1.0;
        else if (text == "DEG" || text == "DEGREE" || text == "DEGREES") scale = kPi / 180.0;
        else return false;
        return true;
    }
    if (kind == "MASS")
    {
        if (text.empty() || text == "KG" || text == "KILOGRAM") scale = 1.0;
        else if (text == "G" || text == "GRAM") scale = 0.001;
        else if (text == "LB" || text == "LBS" || text == "POUND") scale = 0.45359237;
        else return false;
        return true;
    }
    if (kind == "TIME")
    {
        if (text.empty() || text == "S" || text == "SEC" || text == "SECOND") scale = 1.0;
        else if (text == "MS" || text == "MILLISECOND") scale = 0.001;
        else return false;
        return true;
    }
    return false;
}

bool loadUnits(const RawPropertyFile& raw, UnitSystem& units, std::vector<std::string>& warnings)
{
    struct Entry { const char* key; const char* kind; VehicleScalar UnitSystem::*member; };
    const Entry entries[] = {
        { "LENGTH", "LENGTH", &UnitSystem::lengthToM },
        { "FORCE", "FORCE", &UnitSystem::forceToN },
        { "ANGLE", "ANGLE", &UnitSystem::angleToRad },
        { "MASS", "MASS", &UnitSystem::massToKg },
        { "TIME", "TIME", &UnitSystem::timeToS }
    };
    for (const Entry& entry : entries)
    {
        const std::string* rawText = rawValue(raw, "UNITS", entry.key);
        if (!rawText)
            continue;
        VehicleScalar scale = 1.0;
        if (!unitScale(*rawText, entry.kind, scale))
        {
            warnings.push_back(std::string("Unsupported [UNITS] ") + entry.key
                + "='" + stripQuotes(*rawText) + "'; SI unit assumed for that dimension.");
            continue;
        }
        units.*(entry.member) = scale;
    }
    return true;
}

void acceptPluralSectionAliases(RawPropertyFile& raw)
{
    const std::pair<const char*, const char*> aliases[] = {
        { "LONGITUDINAL_COEFFICIENTS", "LONGITUDINAL_COEFFICIENT" },
        { "LATERAL_COEFFICIENTS", "LATERAL_COEFFICIENT" },
        { "OVERTURNING_COEFFICIENT", "OVERTURNING_COEFFICIENTS" },
        { "ROLLING_COEFFICIENT", "ROLLING_COEFFICIENTS" },
        { "ALIGNING_COEFFICIENT", "ALIGNING_COEFFICIENTS" }
    };
    for (const auto& [aliasName, canonicalName] : aliases)
    {
        auto alias = raw.sections.find(aliasName);
        if (alias == raw.sections.end())
            continue;

        // Move legacy spelling into the canonical section so diagnostics do not
        // report the same recognized assignment a second time as unsupported.
        const auto aliasValues = alias->second;
        raw.sections.erase(alias);
        auto& canonical = raw.sections[canonicalName];
        for (const auto& [key, value] : aliasValues)
            canonical.emplace(key, value);
    }
}

} // namespace heritage::vehicles::tires::authoring_detail

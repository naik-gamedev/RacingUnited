#include "TirePropertyImportInternal.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles::tires::authoring_detail {

void noteUnsupported(
    TirePropertyFileData& data,
    const std::string& section,
    const std::string& key)
{
    ++data.unsupportedAssignmentCount;
    if (data.unsupportedParameters.size() < 64)
        data.unsupportedParameters.push_back("[" + section + "] " + key);
}

bool mapScalar(
    const RawPropertyFile& raw,
    TirePropertyFileData& data,
    const char* section,
    const char* key,
    VehicleScalar& destination,
    VehicleScalar scale)
{
    VehicleScalar value = 0.0;
    if (!numberFrom(raw, section, key, value))
        return false;
    destination = value * scale;
    ++data.mappedAssignmentCount;
    return true;
}

bool mapAngleLimit(
    const RawPropertyFile& raw,
    TirePropertyFileData& data,
    const UnitSystem& units,
    const char* section,
    const char* minKey,
    const char* maxKey,
    VehicleScalar& maxAbs)
{
    VehicleScalar minimum = 0.0;
    VehicleScalar maximum = 0.0;
    const bool hasMin = numberFrom(raw, section, minKey, minimum);
    const bool hasMax = numberFrom(raw, section, maxKey, maximum);
    if (!hasMin && !hasMax)
        return false;
    if (hasMin) ++data.mappedAssignmentCount;
    if (hasMax) ++data.mappedAssignmentCount;
    maxAbs = std::max(
        hasMin ? std::abs(minimum * units.angleToRad) : VehicleScalar{0.0},
        hasMax ? std::abs(maximum * units.angleToRad) : VehicleScalar{0.0});
    return true;
}

} // namespace heritage::vehicles::tires::authoring_detail

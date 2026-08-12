#pragma once

#include <string>
#include <unordered_map>

namespace heritage::graphics {

enum class AssetMetadataValueType
{
    String,
    Number,
    Boolean
};

struct AssetMetadataValue
{
    AssetMetadataValueType type = AssetMetadataValueType::String;
    std::string stringValue;
    double numberValue = 0.0;
    bool boolValue = false;
};

using AssetMetadataMap = std::unordered_map<std::string, AssetMetadataValue>;

} // namespace heritage::graphics

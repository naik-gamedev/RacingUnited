#pragma once

#include "EntityRegistry.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>

namespace heritage::entities::entity_registry_internal {

constexpr std::size_t kMaximumHierarchyDepth = 4096;

inline bool validVec3(const heritage::math::Vec3& value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

inline bool validScale(const heritage::math::Vec3& value)
{
    return validVec3(value)
        && std::abs(value.x) > 0.000001f
        && std::abs(value.y) > 0.000001f
        && std::abs(value.z) > 0.000001f;
}

inline std::string trimmed(const std::string& value)
{
    const auto first = std::find_if_not(
        value.begin(), value.end(),
        [](unsigned char character) { return std::isspace(character) != 0; });
    const auto last = std::find_if_not(
        value.rbegin(), value.rend(),
        [](unsigned char character) { return std::isspace(character) != 0; }).base();

    return first < last ? std::string(first, last) : std::string{};
}

inline heritage::math::Vec3 add(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return { left.x + right.x, left.y + right.y, left.z + right.z };
}

inline heritage::math::Vec3 subtract(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return { left.x - right.x, left.y - right.y, left.z - right.z };
}

inline heritage::math::Vec3 multiplyComponents(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return { left.x * right.x, left.y * right.y, left.z * right.z };
}

inline heritage::math::Vec3 divideComponents(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return { left.x / right.x, left.y / right.y, left.z / right.z };
}

} // namespace heritage::entities::entity_registry_internal

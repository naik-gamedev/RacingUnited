#pragma once

#include "InputSystem.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace heritage::input::input_internal {

inline constexpr float kActionThreshold = 0.5f;
inline constexpr float kCaptureAxisThreshold = 0.70f;

inline bool newlyPressed(unsigned char current, unsigned char previous)
{
    return current != 0 && previous == 0;
}

inline float triggerValue(float raw)
{
    // GLFW standard gamepad triggers are normally reported in [-1, +1].
    return std::clamp((raw + 1.0f) * 0.5f, 0.0f, 1.0f);
}

inline const std::vector<int>& supportedKeyCodes()
{
    static const std::vector<int> keys = [] {
        std::vector<int> result;
        for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_GRAVE_ACCENT; ++key)
            result.push_back(key);
        for (int key = GLFW_KEY_ESCAPE; key <= GLFW_KEY_END; ++key)
            result.push_back(key);
        for (int key = GLFW_KEY_CAPS_LOCK; key <= GLFW_KEY_PAUSE; ++key)
            result.push_back(key);
        for (int key = GLFW_KEY_F1; key <= GLFW_KEY_F25; ++key)
            result.push_back(key);
        for (int key = GLFW_KEY_KP_0; key <= GLFW_KEY_KP_EQUAL; ++key)
            result.push_back(key);
        for (int key = GLFW_KEY_LEFT_SHIFT; key <= GLFW_KEY_MENU; ++key)
            result.push_back(key);
        return result;
    }();
    return keys;
}

inline bool isUnsignedInteger(const std::string& value)
{
    return !value.empty()
        && std::all_of(value.begin(), value.end(),
            [](unsigned char character) { return std::isdigit(character) != 0; });
}

inline bool containsBinding(
    const std::vector<std::string>& bindings,
    const std::string& binding,
    std::size_t ignoredIndex = static_cast<std::size_t>(-1))
{
    if (binding.empty())
        return false;

    for (std::size_t index = 0; index < bindings.size(); ++index)
    {
        if (index != ignoredIndex && bindings[index] == binding)
            return true;
    }
    return false;
}

inline std::size_t occupiedBindingCount(const std::vector<std::string>& bindings)
{
    return static_cast<std::size_t>(std::count_if(
        bindings.begin(), bindings.end(),
        [](const std::string& binding) { return !binding.empty(); }));
}

inline std::size_t firstEmptyBindingSlot(const std::vector<std::string>& bindings)
{
    const auto iterator = std::find(bindings.begin(), bindings.end(), std::string{});
    return iterator == bindings.end()
        ? bindings.size()
        : static_cast<std::size_t>(std::distance(bindings.begin(), iterator));
}

inline std::size_t occupiedBindingSpan(const std::vector<std::string>& bindings)
{
    for (std::size_t index = bindings.size(); index > 0; --index)
    {
        if (!bindings[index - 1].empty())
            return index;
    }
    return 0;
}

inline bool profileNamesEqual(const std::string& left, const std::string& right)
{
    if (left.size() != right.size())
        return false;

    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (std::tolower(static_cast<unsigned char>(left[index]))
            != std::tolower(static_cast<unsigned char>(right[index])))
        {
            return false;
        }
    }
    return true;
}

inline bool nearlyEqual(float left, float right)
{
    return std::fabs(left - right) <= 0.00001f;
}

inline bool analogSettingsEqual(
    const InputAnalogSettings& left,
    const InputAnalogSettings& right)
{
    return left.invert == right.invert
        && nearlyEqual(left.innerDeadzone, right.innerDeadzone)
        && nearlyEqual(left.outerDeadzone, right.outerDeadzone)
        && nearlyEqual(left.sensitivity, right.sensitivity)
        && nearlyEqual(left.bezierX1, right.bezierX1)
        && nearlyEqual(left.bezierY1, right.bezierY1)
        && nearlyEqual(left.bezierX2, right.bezierX2)
        && nearlyEqual(left.bezierY2, right.bezierY2);
}

inline bool isReservedWindowsProfileName(const std::string& name)
{
    std::string base = name;
    const std::size_t dot = base.find('.');
    if (dot != std::string::npos)
        base.resize(dot);

    std::transform(base.begin(), base.end(), base.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::toupper(character));
        });

    static const std::array<const char*, 4> fixed = {
        "CON", "PRN", "AUX", "NUL"
    };
    if (std::find(fixed.begin(), fixed.end(), base) != fixed.end())
        return true;

    if (base.size() == 4
        && (base.rfind("COM", 0) == 0 || base.rfind("LPT", 0) == 0)
        && base[3] >= '1' && base[3] <= '9')
    {
        return true;
    }

    return false;
}

} // namespace heritage::input::input_internal

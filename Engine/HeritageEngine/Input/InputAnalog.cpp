#include "InputSystemInternal.hpp"

namespace heritage::input {
using namespace input_internal;

bool InputSystem::bindingSupportsAnalog(
    const std::string& actionName,
    std::size_t bindingIndex) const
{
    const auto iterator = m_actions.find(actionName);
    return iterator != m_actions.end()
        && bindingIndex < iterator->second.parsedBindings.size()
        && parsedBindingSupportsAnalog(
            iterator->second.parsedBindings[bindingIndex]);
}

InputAnalogSettings InputSystem::bindingAnalogSettings(
    const std::string& actionName,
    std::size_t bindingIndex) const
{
    const auto iterator = m_actions.find(actionName);
    if (iterator == m_actions.end()
        || bindingIndex >= iterator->second.analogSettings.size())
    {
        return InputAnalogSettings{};
    }
    return iterator->second.analogSettings[bindingIndex];
}

bool InputSystem::setBindingAnalogSettings(
    const std::string& actionName,
    std::size_t bindingIndex,
    const InputAnalogSettings& settings,
    bool persist)
{
    const auto iterator = m_actions.find(actionName);
    if (iterator == m_actions.end())
    {
        m_lastError = "Unknown input action: " + actionName;
        return false;
    }

    ActionRecord& record = iterator->second;
    if (bindingIndex >= record.parsedBindings.size()
        || bindingIndex >= record.analogSettings.size())
    {
        m_lastError = "Binding index is outside action '" + actionName + "'.";
        return false;
    }
    if (!parsedBindingSupportsAnalog(record.parsedBindings[bindingIndex]))
    {
        m_lastError = "That binding is digital and has no analogue response curve.";
        return false;
    }

    const InputAnalogSettings sanitized = sanitizeAnalogSettings(settings);
    if (!analogSettingsEqual(record.analogSettings[bindingIndex], sanitized))
    {
        record.analogSettings[bindingIndex] = sanitized;
        markProfileDirty();
    }
    m_lastError.clear();
    return !persist || save();
}

bool InputSystem::resetBindingAnalogSettings(
    const std::string& actionName,
    std::size_t bindingIndex)
{
    const auto iterator = m_actions.find(actionName);
    if (iterator == m_actions.end())
    {
        m_lastError = "Unknown input action: " + actionName;
        return false;
    }

    ActionRecord& record = iterator->second;
    if (bindingIndex >= record.parsedBindings.size()
        || bindingIndex >= record.analogSettings.size())
    {
        m_lastError = "Binding index is outside action '" + actionName + "'.";
        return false;
    }

    record.analogSettings[bindingIndex] =
        defaultAnalogSettings(record.parsedBindings[bindingIndex]);
    markProfileDirty();
    return save();
}

float InputSystem::bindingRawValue(
    const std::string& actionName,
    std::size_t bindingIndex) const
{
    const auto iterator = m_actions.find(actionName);
    if (iterator == m_actions.end()
        || bindingIndex >= iterator->second.parsedBindings.size())
    {
        return 0.0f;
    }
    return evaluateBindingRaw(iterator->second.parsedBindings[bindingIndex]);
}

float InputSystem::bindingValue(
    const std::string& actionName,
    std::size_t bindingIndex) const
{
    const auto iterator = m_actions.find(actionName);
    if (iterator == m_actions.end()
        || bindingIndex >= iterator->second.parsedBindings.size()
        || bindingIndex >= iterator->second.analogSettings.size())
    {
        return 0.0f;
    }
    return evaluateBinding(
        iterator->second.parsedBindings[bindingIndex],
        iterator->second.analogSettings[bindingIndex]);
}

float InputSystem::evaluateBezier(
    float input,
    float x1,
    float y1,
    float x2,
    float y2)
{
    input = std::clamp(input, 0.0f, 1.0f);
    x1 = std::clamp(x1, 0.0f, 1.0f);
    y1 = std::clamp(y1, 0.0f, 1.0f);
    x2 = std::clamp(x2, 0.0f, 1.0f);
    y2 = std::clamp(y2, 0.0f, 1.0f);

    const auto component = [](float t, float p1, float p2) {
        const float inverse = 1.0f - t;
        return 3.0f * inverse * inverse * t * p1
            + 3.0f * inverse * t * t * p2
            + t * t * t;
    };

    float low = 0.0f;
    float high = 1.0f;
    float parameter = input;
    for (int iteration = 0; iteration < 22; ++iteration)
    {
        parameter = (low + high) * 0.5f;
        const float x = component(parameter, x1, x2);
        if (x < input)
            low = parameter;
        else
            high = parameter;
    }

    return std::clamp(component(parameter, y1, y2), 0.0f, 1.0f);
}

float InputSystem::applyAnalogProcessing(
    float rawValue,
    const InputAnalogSettings& requestedSettings)
{
    InputAnalogSettings settings = requestedSettings;
    settings.innerDeadzone = std::clamp(settings.innerDeadzone, 0.0f, 0.95f);
    settings.outerDeadzone = std::clamp(
        settings.outerDeadzone,
        0.0f,
        0.95f - settings.innerDeadzone);
    settings.sensitivity = std::clamp(settings.sensitivity, 0.1f, 3.0f);

    float value = std::clamp(rawValue, 0.0f, 1.0f);
    if (settings.invert)
        value = 1.0f - value;

    const float upper = 1.0f - settings.outerDeadzone;
    if (value <= settings.innerDeadzone)
        return 0.0f;
    if (value >= upper)
        return 1.0f;

    value = (value - settings.innerDeadzone)
        / (std::max)(0.0001f, upper - settings.innerDeadzone);
    value = std::clamp(value * settings.sensitivity, 0.0f, 1.0f);
    return evaluateBezier(
        value,
        settings.bezierX1,
        settings.bezierY1,
        settings.bezierX2,
        settings.bezierY2);
}



bool InputSystem::parsedBindingSupportsAnalog(
    const ParsedBinding& binding) const
{
    if (binding.type == BindingType::GamepadAxisPositive
        || binding.type == BindingType::GamepadAxisNegative)
    {
        return true;
    }

    return binding.type == BindingType::DirectInput
        && (binding.directInputControl
                == WindowsDirectInputBackend::ControlType::AxisPositive
            || binding.directInputControl
                == WindowsDirectInputBackend::ControlType::AxisNegative);
}

InputAnalogSettings InputSystem::defaultAnalogSettings(
    const ParsedBinding& binding) const
{
    InputAnalogSettings settings;
    if (!parsedBindingSupportsAnalog(binding))
        return settings;

    const bool directInputAxis = binding.type == BindingType::DirectInput;
    const bool trigger = binding.type != BindingType::DirectInput
        && (binding.code == GLFW_GAMEPAD_AXIS_LEFT_TRIGGER
            || binding.code == GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER);
    settings.innerDeadzone = directInputAxis || trigger ? 0.02f : 0.08f;
    settings.outerDeadzone = 0.0f;
    return settings;
}

InputAnalogSettings InputSystem::sanitizeAnalogSettings(
    const InputAnalogSettings& requested) const
{
    InputAnalogSettings settings = requested;
    settings.innerDeadzone = std::clamp(settings.innerDeadzone, 0.0f, 0.95f);
    settings.outerDeadzone = std::clamp(
        settings.outerDeadzone,
        0.0f,
        0.95f - settings.innerDeadzone);
    settings.sensitivity = std::clamp(settings.sensitivity, 0.1f, 3.0f);
    settings.bezierX1 = std::clamp(settings.bezierX1, 0.0f, 1.0f);
    settings.bezierY1 = std::clamp(settings.bezierY1, 0.0f, 1.0f);
    settings.bezierX2 = std::clamp(settings.bezierX2, 0.0f, 1.0f);
    settings.bezierY2 = std::clamp(settings.bezierY2, 0.0f, 1.0f);
    return settings;
}

void InputSystem::initializeAnalogSettings(
    const std::string& actionName,
    ActionRecord& record)
{
    if (record.bindings.size() != kMaxBindingsPerAction)
        record.bindings.resize(kMaxBindingsPerAction);
    if (record.parsedBindings.size() != kMaxBindingsPerAction)
        record.parsedBindings.resize(kMaxBindingsPerAction);

    record.analogSettings.assign(kMaxBindingsPerAction, InputAnalogSettings{});
    for (std::size_t index = 0; index < kMaxBindingsPerAction; ++index)
    {
        if (!record.bindings[index].empty())
        {
            record.analogSettings[index] =
                defaultAnalogSettings(record.parsedBindings[index]);
        }
    }

    const auto actionIterator = m_loadedAnalogSettings.find(actionName);
    if (actionIterator == m_loadedAnalogSettings.end())
        return;

    for (const auto& [index, loaded] : actionIterator->second)
    {
        if (index >= kMaxBindingsPerAction
            || index >= record.parsedBindings.size()
            || record.bindings[index].empty())
        {
            continue;
        }
        if (record.parsedBindings[index].canonical != loaded.binding
            || !parsedBindingSupportsAnalog(record.parsedBindings[index]))
        {
            continue;
        }
        record.analogSettings[index] = sanitizeAnalogSettings(loaded.settings);
    }
}

bool InputSystem::analogSettingsAreDefault(
    const ParsedBinding& binding,
    const InputAnalogSettings& settings) const
{
    const InputAnalogSettings defaults = defaultAnalogSettings(binding);
    const auto close = [](float left, float right) {
        return std::fabs(left - right) <= 0.00001f;
    };
    return settings.invert == defaults.invert
        && close(settings.innerDeadzone, defaults.innerDeadzone)
        && close(settings.outerDeadzone, defaults.outerDeadzone)
        && close(settings.sensitivity, defaults.sensitivity)
        && close(settings.bezierX1, defaults.bezierX1)
        && close(settings.bezierY1, defaults.bezierY1)
        && close(settings.bezierX2, defaults.bezierX2)
        && close(settings.bezierY2, defaults.bezierY2);
}



} // namespace heritage::input

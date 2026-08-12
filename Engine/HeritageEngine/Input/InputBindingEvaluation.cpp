#include "InputSystemInternal.hpp"

namespace heritage::input {
using namespace input_internal;

float InputSystem::evaluateBindingRaw(const ParsedBinding& binding) const
{
    switch (binding.type)
    {
    case BindingType::Key:
        return binding.code >= 0 && binding.code <= GLFW_KEY_LAST
            && m_keys[static_cast<std::size_t>(binding.code)] ? 1.0f : 0.0f;
    case BindingType::MouseButton:
        return binding.code >= 0 && binding.code <= GLFW_MOUSE_BUTTON_LAST
            && m_mouseButtons[static_cast<std::size_t>(binding.code)] ? 1.0f : 0.0f;
    case BindingType::GamepadButton:
    case BindingType::GamepadAxisPositive:
    case BindingType::GamepadAxisNegative:
    {
        float value = 0.0f;
        for (const GamepadSnapshot* gamepad : matchingGamepads(binding))
            value = (std::max)(value, evaluateGamepadBinding(binding, *gamepad));
        return value;
    }
    case BindingType::DirectInput:
        return m_directInput.value(
            binding.directInputGuid,
            binding.directInputControl,
            binding.code);
    default:
        return 0.0f;
    }
}

float InputSystem::evaluateBinding(
    const ParsedBinding& binding,
    const InputAnalogSettings& settings) const
{
    const float rawValue = evaluateBindingRaw(binding);
    return parsedBindingSupportsAnalog(binding)
        ? applyAnalogProcessing(rawValue, settings)
        : rawValue;
}

float InputSystem::evaluateGamepadBinding(
    const ParsedBinding& binding,
    const GamepadSnapshot& gamepad) const
{
    if (!gamepad.available)
        return 0.0f;

    switch (binding.type)
    {
    case BindingType::GamepadButton:
        return binding.code >= 0 && binding.code <= GLFW_GAMEPAD_BUTTON_LAST
            && gamepad.state.buttons[binding.code] == GLFW_PRESS ? 1.0f : 0.0f;
    case BindingType::GamepadAxisPositive:
        if (binding.code < 0 || binding.code > GLFW_GAMEPAD_AXIS_LAST)
            return 0.0f;
        if (binding.code == GLFW_GAMEPAD_AXIS_LEFT_TRIGGER
            || binding.code == GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER)
        {
            return triggerValue(gamepad.state.axes[binding.code]);
        }
        return (std::max)(0.0f, gamepad.state.axes[binding.code]);
    case BindingType::GamepadAxisNegative:
        if (binding.code < 0 || binding.code > GLFW_GAMEPAD_AXIS_LAST)
            return 0.0f;
        return (std::max)(0.0f, -gamepad.state.axes[binding.code]);
    default:
        return 0.0f;
    }
}

std::vector<const InputSystem::GamepadSnapshot*> InputSystem::matchingGamepads(
    const ParsedBinding& binding) const
{
    std::vector<const GamepadSnapshot*> result;

    if (binding.gamepadSelector == GamepadSelector::Ordinal)
    {
        const GamepadSnapshot* gamepad = gamepadForOrdinal(binding.gamepadOrdinal);
        if (gamepad)
            result.push_back(gamepad);
        return result;
    }

    for (const GamepadSnapshot& gamepad : m_gamepads)
    {
        if (!gamepad.available)
            continue;
        if (binding.gamepadSelector == GamepadSelector::Guid
            && lower(gamepad.guid) != lower(binding.gamepadGuid))
        {
            continue;
        }
        result.push_back(&gamepad);
    }
    return result;
}


} // namespace heritage::input

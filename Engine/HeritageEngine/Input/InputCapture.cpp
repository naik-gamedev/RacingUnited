#include "InputSystemInternal.hpp"

namespace heritage::input {
using namespace input_internal;

bool InputSystem::beginBindingCapture(
    const std::string& actionName,
    std::size_t bindingIndex)
{
    const auto iterator = m_actions.find(actionName);
    if (iterator == m_actions.end())
    {
        m_lastError = "Unknown input action: " + actionName;
        return false;
    }
    if (bindingIndex >= kMaxBindingsPerAction
        || bindingIndex >= iterator->second.bindings.size())
    {
        m_lastError = "Binding index is outside action '" + actionName + "'.";
        return false;
    }

    m_captureAction = actionName;
    m_captureBindingIndex = bindingIndex;
    m_captureAppend = false;
    m_directInput.beginCapture();
    m_lastError.clear();
    return true;
}

bool InputSystem::beginBindingCapture(const std::string& actionName)
{
    const auto iterator = m_actions.find(actionName);
    if (iterator == m_actions.end())
    {
        m_lastError = "Unknown input action: " + actionName;
        return false;
    }
    return beginBindingCapture(actionName, 0);
}

bool InputSystem::beginAddBindingCapture(const std::string& actionName)
{
    const auto iterator = m_actions.find(actionName);
    if (iterator == m_actions.end())
    {
        m_lastError = "Unknown input action: " + actionName;
        return false;
    }

    const std::size_t emptySlot = firstEmptyBindingSlot(iterator->second.bindings);
    if (emptySlot >= kMaxBindingsPerAction)
    {
        m_lastError = "Action '" + actionName
            + "' already has the maximum of eight bindings.";
        return false;
    }

    m_captureAction = actionName;
    m_captureBindingIndex = emptySlot;
    m_captureAppend = true;
    m_directInput.beginCapture();
    m_lastError.clear();
    return true;
}

void InputSystem::cancelBindingCapture()
{
    m_captureAction.clear();
    m_captureBindingIndex = 0;
    m_captureAppend = false;
}


void InputSystem::updateBindingCapture()
{
    if (m_captureAction.empty())
        return;

    // Keyboard first because it is the most common rebinding workflow.
    for (const int key : supportedKeyCodes())
    {
        if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_F11)
            continue;

        if (newlyPressed(
            m_keys[static_cast<std::size_t>(key)],
            m_previousKeys[static_cast<std::size_t>(key)]))
        {
            const std::string name = keyNameFromCode(key);
            if (!name.empty())
            {
                applyCapturedBinding("Key:" + name);
                return;
            }
        }
    }

    for (int button = 0; button <= GLFW_MOUSE_BUTTON_LAST; ++button)
    {
        if (newlyPressed(
            m_mouseButtons[static_cast<std::size_t>(button)],
            m_previousMouseButtons[static_cast<std::size_t>(button)]))
        {
            applyCapturedBinding("Mouse:" + mouseButtonName(button));
            return;
        }
    }

    for (const GamepadSnapshot& gamepad : m_gamepads)
    {
        if (!gamepad.available || !gamepad.previousAvailable)
            continue;

        const std::string prefix = specificGamepadPrefix(gamepad);
        for (int button = 0; button <= GLFW_GAMEPAD_BUTTON_LAST; ++button)
        {
            if (newlyPressed(
                gamepad.state.buttons[button],
                gamepad.previousState.buttons[button]))
            {
                applyCapturedBinding(prefix + gamepadButtonName(button));
                return;
            }
        }

        for (int axis = 0; axis <= GLFW_GAMEPAD_AXIS_LAST; ++axis)
        {
            float current = gamepad.state.axes[axis];
            float previous = gamepad.previousState.axes[axis];
            const bool trigger = axis == GLFW_GAMEPAD_AXIS_LEFT_TRIGGER
                || axis == GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER;

            if (trigger)
            {
                current = triggerValue(current);
                previous = triggerValue(previous);
                if (current >= kCaptureAxisThreshold
                    && previous < kCaptureAxisThreshold)
                {
                    applyCapturedBinding(prefix + gamepadAxisName(axis) + "+");
                    return;
                }
                continue;
            }

            if (current >= kCaptureAxisThreshold
                && previous < kCaptureAxisThreshold)
            {
                applyCapturedBinding(prefix + gamepadAxisName(axis) + "+");
                return;
            }
            if (current <= -kCaptureAxisThreshold
                && previous > -kCaptureAxisThreshold)
            {
                applyCapturedBinding(prefix + gamepadAxisName(axis) + "-");
                return;
            }
        }
    }

    std::string directInputBinding;
    if (m_directInput.captureBinding(directInputBinding))
        applyCapturedBinding(directInputBinding);
}


bool InputSystem::applyCapturedBinding(const std::string& binding)
{
    const std::string action = m_captureAction;
    const std::size_t index = m_captureBindingIndex;
    const bool append = m_captureAppend;
    cancelBindingCapture();

    return append
        ? addBinding(action, binding)
        : setBinding(action, index, binding);
}


} // namespace heritage::input

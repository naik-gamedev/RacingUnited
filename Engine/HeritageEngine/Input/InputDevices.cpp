#include "InputSystemInternal.hpp"

namespace heritage::input {
using namespace input_internal;

bool InputSystem::keyDown(const std::string& keyName) const
{
    const int key = keyCodeFromName(keyName);
    return key >= 0 && key <= GLFW_KEY_LAST
        && m_keys[static_cast<std::size_t>(key)] != 0;
}

bool InputSystem::keyPressed(const std::string& keyName) const
{
    const int key = keyCodeFromName(keyName);
    return key >= 0 && key <= GLFW_KEY_LAST
        && newlyPressed(
            m_keys[static_cast<std::size_t>(key)],
            m_previousKeys[static_cast<std::size_t>(key)]);
}

bool InputSystem::keyReleased(const std::string& keyName) const
{
    const int key = keyCodeFromName(keyName);
    return key >= 0 && key <= GLFW_KEY_LAST
        && m_keys[static_cast<std::size_t>(key)] == 0
        && m_previousKeys[static_cast<std::size_t>(key)] != 0;
}

bool InputSystem::mouseDown(const std::string& buttonName) const
{
    const int button = mouseButtonFromName(buttonName);
    return button >= 0 && button <= GLFW_MOUSE_BUTTON_LAST
        && m_mouseButtons[static_cast<std::size_t>(button)] != 0;
}

bool InputSystem::mousePressed(const std::string& buttonName) const
{
    const int button = mouseButtonFromName(buttonName);
    return button >= 0 && button <= GLFW_MOUSE_BUTTON_LAST
        && newlyPressed(
            m_mouseButtons[static_cast<std::size_t>(button)],
            m_previousMouseButtons[static_cast<std::size_t>(button)]);
}

bool InputSystem::mouseReleased(const std::string& buttonName) const
{
    const int button = mouseButtonFromName(buttonName);
    return button >= 0 && button <= GLFW_MOUSE_BUTTON_LAST
        && m_mouseButtons[static_cast<std::size_t>(button)] == 0
        && m_previousMouseButtons[static_cast<std::size_t>(button)] != 0;
}

int InputSystem::connectedGamepadCount() const
{
    int count = 0;
    for (const GamepadSnapshot& gamepad : m_gamepads)
    {
        if (gamepad.available)
            ++count;
    }
    return count;
}

bool InputSystem::gamepadConnected(int ordinal) const
{
    return gamepadForOrdinal(ordinal) != nullptr;
}

std::string InputSystem::gamepadName(int ordinal) const
{
    const GamepadSnapshot* gamepad = gamepadForOrdinal(ordinal);
    return gamepad ? gamepad->name : std::string{};
}

std::string InputSystem::gamepadGuid(int ordinal) const
{
    const GamepadSnapshot* gamepad = gamepadForOrdinal(ordinal);
    return gamepad ? gamepad->guid : std::string{};
}

std::vector<InputDeviceInfo> InputSystem::gamepads() const
{
    std::vector<InputDeviceInfo> result;
    int ordinal = 0;
    for (const GamepadSnapshot& gamepad : m_gamepads)
    {
        if (!gamepad.available)
            continue;

        InputDeviceInfo info;
        info.ordinal = ordinal++;
        info.name = gamepad.name;
        info.guid = gamepad.guid;
        result.push_back(std::move(info));
    }
    return result;
}

std::vector<WindowsDirectInputBackend::DeviceInfo>
InputSystem::directInputDevices() const
{
    return m_directInput.devices();
}

void InputSystem::refreshInputDevices()
{
    m_directInput.refreshDevices();
    updateHardwareState();
}


void InputSystem::updateHardwareState()
{
    m_keys.fill(0);
    for (const int key : supportedKeyCodes())
    {
        const int state = glfwGetKey(m_window, key);
        m_keys[static_cast<std::size_t>(key)] =
            (state == GLFW_PRESS || state == GLFW_REPEAT) ? 1 : 0;
    }

    for (int button = 0; button <= GLFW_MOUSE_BUTTON_LAST; ++button)
    {
        m_mouseButtons[static_cast<std::size_t>(button)] =
            glfwGetMouseButton(m_window, button) == GLFW_PRESS ? 1 : 0;
    }

    glfwGetCursorPos(m_window, &m_mouseX, &m_mouseY);
    if (m_firstMouseUpdate)
    {
        m_mouseDeltaX = 0.0;
        m_mouseDeltaY = 0.0;
        m_firstMouseUpdate = false;
    }
    else
    {
        m_mouseDeltaX = m_mouseX - m_previousMouseX;
        m_mouseDeltaY = m_mouseY - m_previousMouseY;
    }

    for (int joystick = GLFW_JOYSTICK_1; joystick <= GLFW_JOYSTICK_LAST; ++joystick)
    {
        GamepadSnapshot& gamepad = m_gamepads[static_cast<std::size_t>(joystick)];
        gamepad.joystickId = joystick;
        gamepad.available = false;
        gamepad.name.clear();
        gamepad.guid.clear();
        gamepad.state = GLFWgamepadstate{};

        if (glfwJoystickPresent(joystick) != GLFW_TRUE
            || glfwJoystickIsGamepad(joystick) != GLFW_TRUE
            || !readGamepadState(joystick, gamepad.state))
        {
            continue;
        }

        gamepad.available = true;
        const char* name = glfwGetGamepadName(joystick);
        if (!name)
            name = glfwGetJoystickName(joystick);
        gamepad.name = name ? std::string(name) : std::string("Gamepad");

        const char* guid = glfwGetJoystickGUID(joystick);
        gamepad.guid = guid ? lower(guid) : std::string{};
    }

    m_directInput.update();
}


std::string InputSystem::specificGamepadPrefix(
    const GamepadSnapshot& gamepad) const
{
    if (!gamepad.guid.empty())
        return "Gamepad[" + lower(gamepad.guid) + "]:";

    const int ordinal = gamepadOrdinalForJoystickId(gamepad.joystickId);
    return ordinal >= 0
        ? "Gamepad" + std::to_string(ordinal + 1) + ":"
        : std::string("Gamepad:");
}

const InputSystem::GamepadSnapshot* InputSystem::gamepadForOrdinal(int ordinal) const
{
    if (ordinal < 0)
        return nullptr;

    int current = 0;
    for (const GamepadSnapshot& gamepad : m_gamepads)
    {
        if (!gamepad.available)
            continue;
        if (current == ordinal)
            return &gamepad;
        ++current;
    }
    return nullptr;
}

int InputSystem::gamepadOrdinalForJoystickId(int joystickId) const
{
    int ordinal = 0;
    for (const GamepadSnapshot& gamepad : m_gamepads)
    {
        if (!gamepad.available)
            continue;
        if (gamepad.joystickId == joystickId)
            return ordinal;
        ++ordinal;
    }
    return -1;
}

bool InputSystem::readGamepadState(
    int joystickId,
    GLFWgamepadstate& state) const
{
    return joystickId >= 0
        && glfwGetGamepadState(joystickId, &state) == GLFW_TRUE;
}


} // namespace heritage::input

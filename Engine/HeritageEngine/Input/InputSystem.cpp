#include "InputSystemInternal.hpp"

namespace heritage::input {
using namespace input_internal;

bool InputSystem::initialize(
    GLFWwindow* window,
    const std::filesystem::path& settingsPath,
    std::string& message)
{
    shutdown();

    if (!window)
    {
        message = "InputSystem received a null GLFW window.";
        return false;
    }

    m_window = window;
    m_settingsPath = settingsPath;

    std::string directInputMessage;
    m_directInput.initialize(window, directInputMessage);

    for (int joystick = GLFW_JOYSTICK_1; joystick <= GLFW_JOYSTICK_LAST; ++joystick)
        m_gamepads[static_cast<std::size_t>(joystick)].joystickId = joystick;

    glfwGetCursorPos(m_window, &m_mouseX, &m_mouseY);
    m_previousMouseX = m_mouseX;
    m_previousMouseY = m_mouseY;
    m_firstMouseUpdate = true;

    if (!load())
    {
        message = m_lastError;
        // A damaged or missing binding file must not disable input.
        m_lastError.clear();
    }
    else
    {
        message.clear();
    }

    updateHardwareState();
    m_previousKeys = m_keys;
    m_previousMouseButtons = m_mouseButtons;
    for (auto& gamepad : m_gamepads)
    {
        gamepad.previousAvailable = gamepad.available;
        gamepad.previousState = gamepad.state;
    }
    return true;
}

void InputSystem::shutdown()
{
    if (m_window)
        save();

    m_directInput.shutdown();
    m_window = nullptr;
    m_settingsPath.clear();
    m_keys.fill(0);
    m_previousKeys.fill(0);
    m_mouseButtons.fill(0);
    m_previousMouseButtons.fill(0);
    m_mouseX = m_mouseY = 0.0;
    m_previousMouseX = m_previousMouseY = 0.0;
    m_mouseDeltaX = m_mouseDeltaY = 0.0;
    m_firstMouseUpdate = true;
    m_gamepads = {};
    m_actions.clear();
    m_actionGroups.clear();
    m_loadedBindings.clear();
    m_loadedBindingOverrides.clear();
    m_loadedLegacyActions.clear();
    m_loadedAnalogSettings.clear();
    m_captureAction.clear();
    m_captureBindingIndex = 0;
    m_captureAppend = false;
    m_lastAppliedProfile.clear();
    m_profileDirty = false;
    m_lastError.clear();
}

void InputSystem::update()
{
    if (!m_window)
        return;

    m_previousKeys = m_keys;
    m_previousMouseButtons = m_mouseButtons;
    m_previousMouseX = m_mouseX;
    m_previousMouseY = m_mouseY;
    for (auto& gamepad : m_gamepads)
    {
        gamepad.previousAvailable = gamepad.available;
        gamepad.previousState = gamepad.state;
    }

    updateHardwareState();
    updateActions();
    updateBindingCapture();
}

bool InputSystem::loadActionDefinitions(
    const std::filesystem::path& definitionsPath,
    std::string& message)
{
    if (definitionsPath.empty())
    {
        message = "The module input-definition path is empty.";
        return false;
    }

    if (!std::filesystem::is_regular_file(definitionsPath))
    {
        message = "No module input definitions were found at: "
            + definitionsPath.string();
        return true;
    }

    std::ifstream file(definitionsPath);
    if (!file)
    {
        message = "Could not open module input definitions: "
            + definitionsPath.string();
        return false;
    }

    std::size_t registeredCount = 0;
    std::size_t lineNumber = 0;
    std::string line;
    std::string currentGroup = "Common";

    while (std::getline(file, line))
    {
        ++lineNumber;
        line = trim(line);

        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;

        if (line.front() == '[')
        {
            if (line.size() < 3 || line.back() != ']')
            {
                message = "Invalid input group at "
                    + definitionsPath.string() + ":"
                    + std::to_string(lineNumber)
                    + ". Expected [Group Name].";
                return false;
            }

            currentGroup = trim(line.substr(1, line.size() - 2));
            if (currentGroup.empty())
            {
                message = "Input group names cannot be empty at "
                    + definitionsPath.string() + ":"
                    + std::to_string(lineNumber) + ".";
                return false;
            }
            rememberActionGroup(currentGroup);
            continue;
        }

        const std::size_t equals = line.find('=');
        if (equals == std::string::npos)
        {
            message = "Invalid input action at "
                + definitionsPath.string() + ":"
                + std::to_string(lineNumber)
                + ". Expected Action Name = Binding | Binding.";
            return false;
        }

        const std::string actionName = trim(line.substr(0, equals));
        const std::string bindingList = trim(line.substr(equals + 1));

        if (actionName.empty())
        {
            message = "Invalid input action at "
                + definitionsPath.string() + ":"
                + std::to_string(lineNumber)
                + ". Action name is required.";
            return false;
        }
        // INPUT03: an empty right-hand side is a valid deliberately-unbound
        // action declaration. The Settings UI can then capture the user's own
        // shifter/button without Heritage inventing a conflicting default.

        if (!registerAction(actionName, bindingList, currentGroup))
        {
            message = "Could not register input action '"
                + actionName + "' from "
                + definitionsPath.string() + ": "
                + m_lastError;
            return false;
        }

        ++registeredCount;
    }

    message = "Loaded " + std::to_string(registeredCount)
        + " module input action"
        + (registeredCount == 1 ? "" : "s")
        + " across " + std::to_string(m_actionGroups.size())
        + " action group" + (m_actionGroups.size() == 1 ? "" : "s")
        + " from " + definitionsPath.string();
    m_lastError.clear();
    return true;
}


std::vector<InputActionInfo> InputSystem::actions() const
{
    std::vector<InputActionInfo> result;
    result.reserve(m_actions.size());

    for (const auto& [name, record] : m_actions)
    {
        InputActionInfo info;
        info.name = name;
        info.group = record.group.empty() ? "Common" : record.group;
        info.defaultBindings = record.defaultBindings;
        info.value = record.value;
        info.down = record.down;
        info.pressed = record.pressed;
        info.released = record.released;

        for (std::size_t index = 0; index < record.bindings.size(); ++index)
        {
            InputBindingInfo bindingInfo;
            bindingInfo.binding = record.bindings[index];
            bindingInfo.displayName = record.bindings[index].empty()
                ? std::string{}
                : bindingDisplayName(record.parsedBindings[index]);
            bindingInfo.rawValue = record.bindings[index].empty()
                ? 0.0f
                : evaluateBindingRaw(record.parsedBindings[index]);
            bindingInfo.analog = parsedBindingSupportsAnalog(record.parsedBindings[index]);
            bindingInfo.analogSettings = index < record.analogSettings.size()
                ? record.analogSettings[index]
                : defaultAnalogSettings(record.parsedBindings[index]);
            bindingInfo.value = bindingInfo.analog
                ? applyAnalogProcessing(bindingInfo.rawValue, bindingInfo.analogSettings)
                : bindingInfo.rawValue;
            bindingInfo.active = bindingInfo.value >= kActionThreshold;
            info.bindings.push_back(std::move(bindingInfo));
        }

        result.push_back(std::move(info));
    }

    return result;
}

void InputSystem::rememberActionGroup(const std::string& group)
{
    std::string clean = trim(group);
    if (clean.empty())
        clean = "Common";

    if (std::find(m_actionGroups.begin(), m_actionGroups.end(), clean)
        == m_actionGroups.end())
    {
        m_actionGroups.push_back(std::move(clean));
    }
}


void InputSystem::updateActions()
{
    for (auto& [name, record] : m_actions)
    {
        (void)name;
        const bool wasDown = record.down;
        record.value = 0.0f;
        for (std::size_t index = 0; index < record.parsedBindings.size(); ++index)
        {
            const InputAnalogSettings settings = index < record.analogSettings.size()
                ? record.analogSettings[index]
                : defaultAnalogSettings(record.parsedBindings[index]);
            record.value = (std::max)(
                record.value,
                evaluateBinding(record.parsedBindings[index], settings));
        }

        record.down = record.value >= kActionThreshold;
        record.pressed = record.down && !wasDown;
        record.released = !record.down && wasDown;
    }
}


} // namespace heritage::input

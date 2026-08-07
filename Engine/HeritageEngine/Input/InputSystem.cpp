#include "InputSystem.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace heritage::input {
namespace {

constexpr float kActionThreshold = 0.5f;
constexpr float kCaptureAxisThreshold = 0.70f;

bool newlyPressed(unsigned char current, unsigned char previous)
{
    return current != 0 && previous == 0;
}

float triggerValue(float raw)
{
    // GLFW standard gamepad triggers are normally reported in [-1, +1].
    return std::clamp((raw + 1.0f) * 0.5f, 0.0f, 1.0f);
}

const std::vector<int>& supportedKeyCodes()
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

bool isUnsignedInteger(const std::string& value)
{
    return !value.empty()
        && std::all_of(value.begin(), value.end(),
            [](unsigned char character) { return std::isdigit(character) != 0; });
}

bool containsBinding(
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

std::size_t occupiedBindingCount(const std::vector<std::string>& bindings)
{
    return static_cast<std::size_t>(std::count_if(
        bindings.begin(),
        bindings.end(),
        [](const std::string& binding) { return !binding.empty(); }));
}

std::size_t firstEmptyBindingSlot(const std::vector<std::string>& bindings)
{
    const auto iterator = std::find(bindings.begin(), bindings.end(), std::string{});
    return iterator == bindings.end()
        ? bindings.size()
        : static_cast<std::size_t>(std::distance(bindings.begin(), iterator));
}

std::size_t occupiedBindingSpan(const std::vector<std::string>& bindings)
{
    for (std::size_t index = bindings.size(); index > 0; --index)
    {
        if (!bindings[index - 1].empty())
            return index;
    }
    return 0;
}

bool profileNamesEqual(const std::string& left, const std::string& right)
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

bool nearlyEqual(float left, float right)
{
    return std::fabs(left - right) <= 0.00001f;
}

bool analogSettingsEqual(
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

bool isReservedWindowsProfileName(const std::string& name)
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

} // namespace

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

        if (actionName.empty() || bindingList.empty())
        {
            message = "Invalid input action at "
                + definitionsPath.string() + ":"
                + std::to_string(lineNumber)
                + ". Action name and at least one binding are required.";
            return false;
        }

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

bool InputSystem::registerAction(
    const std::string& actionName,
    const std::string& defaultBinding,
    const std::string& group)
{
    const std::string cleanName = trim(actionName);
    if (cleanName.empty())
    {
        m_lastError = "Input action names cannot be empty.";
        return false;
    }

    std::string cleanGroup = trim(group);
    if (cleanGroup.empty())
        cleanGroup = "Common";
    rememberActionGroup(cleanGroup);

    std::vector<ParsedBinding> parsedDefaults;
    std::vector<std::string> canonicalDefaults;
    std::string parseError;
    if (!parseBindingList(
        defaultBinding,
        parsedDefaults,
        canonicalDefaults,
        parseError))
    {
        m_lastError = "Action '" + cleanName + "': " + parseError;
        return false;
    }

    if (canonicalDefaults.size() > kMaxBindingsPerAction)
    {
        m_lastError = "Action '" + cleanName
            + "' declares more than "
            + std::to_string(kMaxBindingsPerAction)
            + " default bindings.";
        return false;
    }

    auto iterator = m_actions.find(cleanName);
    if (iterator == m_actions.end())
    {
        ActionRecord record;
        record.group = cleanGroup;
        record.defaultBindings.assign(kMaxBindingsPerAction, {});
        record.bindings.assign(kMaxBindingsPerAction, {});
        record.parsedBindings.assign(kMaxBindingsPerAction, ParsedBinding{});
        record.analogSettings.assign(kMaxBindingsPerAction, InputAnalogSettings{});

        for (std::size_t index = 0; index < canonicalDefaults.size(); ++index)
            record.defaultBindings[index] = canonicalDefaults[index];

        const bool hasLoadedOverride =
            m_loadedBindingOverrides.find(cleanName) != m_loadedBindingOverrides.end();
        record.hasUserBindings = hasLoadedOverride;

        std::vector<std::string> selectedBindings(kMaxBindingsPerAction);
        if (hasLoadedOverride)
        {
            const auto loaded = m_loadedBindings.find(cleanName);
            if (loaded != m_loadedBindings.end())
            {
                for (std::size_t index = 0;
                    index < loaded->second.size() && index < kMaxBindingsPerAction;
                    ++index)
                {
                    selectedBindings[index] = loaded->second[index];
                }
            }
        }
        else
        {
            for (std::size_t index = 0; index < canonicalDefaults.size(); ++index)
                selectedBindings[index] = canonicalDefaults[index];
        }

        // Step 26E wrote one legacy binding per action. Preserve that primary
        // choice while adding newly declared secondary defaults into the next
        // empty slots.
        if (hasLoadedOverride
            && m_loadedLegacyActions.find(cleanName) != m_loadedLegacyActions.end()
            && canonicalDefaults.size() > 1)
        {
            for (std::size_t index = 1; index < canonicalDefaults.size(); ++index)
            {
                if (containsBinding(selectedBindings, canonicalDefaults[index]))
                    continue;
                const std::size_t empty = firstEmptyBindingSlot(selectedBindings);
                if (empty >= kMaxBindingsPerAction)
                    break;
                selectedBindings[empty] = canonicalDefaults[index];
            }
        }

        bool selectedValid = true;
        for (std::size_t index = 0; index < kMaxBindingsPerAction; ++index)
        {
            if (selectedBindings[index].empty())
                continue;

            ParsedBinding parsed;
            if (!parseBinding(selectedBindings[index], parsed, parseError))
            {
                selectedValid = false;
                break;
            }
            if (containsBinding(record.bindings, parsed.canonical))
                continue;

            record.bindings[index] = parsed.canonical;
            record.parsedBindings[index] = std::move(parsed);
        }

        if (!selectedValid)
        {
            record.bindings.assign(kMaxBindingsPerAction, {});
            record.parsedBindings.assign(kMaxBindingsPerAction, ParsedBinding{});
            for (std::size_t index = 0; index < canonicalDefaults.size(); ++index)
            {
                record.bindings[index] = canonicalDefaults[index];
                record.parsedBindings[index] = parsedDefaults[index];
            }
            record.hasUserBindings = false;
        }

        initializeAnalogSettings(cleanName, record);
        m_actions.emplace(cleanName, std::move(record));
    }
    else
    {
        ActionRecord& record = iterator->second;
        if (record.defaultBindings.size() != kMaxBindingsPerAction)
            record.defaultBindings.resize(kMaxBindingsPerAction);
        if (record.bindings.size() != kMaxBindingsPerAction)
            record.bindings.resize(kMaxBindingsPerAction);
        if (record.parsedBindings.size() != kMaxBindingsPerAction)
            record.parsedBindings.resize(kMaxBindingsPerAction);
        if (record.analogSettings.size() != kMaxBindingsPerAction)
            record.analogSettings.resize(kMaxBindingsPerAction);

        // Native module declarations load before Lua. A later Lua call that
        // omits its optional group must not move a Car or Motorcycle action
        // back into Common.
        if ((record.group.empty() || record.group == "Common")
            && cleanGroup != "Common")
        {
            record.group = cleanGroup;
        }

        std::size_t newDefaultCount = 0;
        for (const std::string& canonical : canonicalDefaults)
        {
            if (!containsBinding(record.defaultBindings, canonical))
                ++newDefaultCount;
        }

        if (occupiedBindingCount(record.defaultBindings) + newDefaultCount
            > kMaxBindingsPerAction)
        {
            m_lastError = "Action '" + cleanName
                + "' would exceed the eight-binding limit.";
            return false;
        }

        for (std::size_t index = 0; index < canonicalDefaults.size(); ++index)
        {
            const std::string& canonical = canonicalDefaults[index];
            if (!containsBinding(record.defaultBindings, canonical))
            {
                const std::size_t defaultSlot = firstEmptyBindingSlot(record.defaultBindings);
                if (defaultSlot < kMaxBindingsPerAction)
                    record.defaultBindings[defaultSlot] = canonical;
            }

            if (!record.hasUserBindings
                && !containsBinding(record.bindings, canonical))
            {
                const std::size_t bindingSlot = firstEmptyBindingSlot(record.bindings);
                if (bindingSlot < kMaxBindingsPerAction)
                {
                    record.bindings[bindingSlot] = canonical;
                    record.parsedBindings[bindingSlot] = parsedDefaults[index];
                    record.analogSettings[bindingSlot] =
                        defaultAnalogSettings(parsedDefaults[index]);
                }
            }
        }
    }

    m_lastError.clear();
    return true;
}

bool InputSystem::setBinding(
    const std::string& actionName,
    const std::string& binding)
{
    const auto iterator = m_actions.find(actionName);
    if (iterator == m_actions.end())
    {
        m_lastError = "Unknown input action: " + actionName;
        return false;
    }

    return setBinding(actionName, 0, binding);
}

bool InputSystem::setBinding(
    const std::string& actionName,
    std::size_t bindingIndex,
    const std::string& binding)
{
    const auto iterator = m_actions.find(actionName);
    if (iterator == m_actions.end())
    {
        m_lastError = "Unknown input action: " + actionName;
        return false;
    }

    ActionRecord& record = iterator->second;
    if (bindingIndex >= record.bindings.size())
    {
        m_lastError = "Binding index is outside action '" + actionName + "'.";
        return false;
    }

    ParsedBinding parsed;
    std::string parseError;
    if (!parseBinding(binding, parsed, parseError))
    {
        m_lastError = parseError;
        return false;
    }

    if (containsBinding(record.bindings, parsed.canonical, bindingIndex))
    {
        m_lastError = "That input is already bound to '" + actionName + "'.";
        return false;
    }

    record.bindings[bindingIndex] = parsed.canonical;
    record.parsedBindings[bindingIndex] = std::move(parsed);
    if (bindingIndex >= record.analogSettings.size())
        record.analogSettings.resize(record.parsedBindings.size());
    record.analogSettings[bindingIndex] =
        defaultAnalogSettings(record.parsedBindings[bindingIndex]);
    record.hasUserBindings = true;
    record.value = 0.0f;
    record.down = false;
    record.pressed = false;
    record.released = false;
    markProfileDirty();

    return save();
}

bool InputSystem::addBinding(
    const std::string& actionName,
    const std::string& binding)
{
    const auto iterator = m_actions.find(actionName);
    if (iterator == m_actions.end())
    {
        m_lastError = "Unknown input action: " + actionName;
        return false;
    }

    ParsedBinding parsed;
    std::string parseError;
    if (!parseBinding(binding, parsed, parseError))
    {
        m_lastError = parseError;
        return false;
    }

    ActionRecord& record = iterator->second;
    const std::size_t emptySlot = firstEmptyBindingSlot(record.bindings);
    if (emptySlot >= kMaxBindingsPerAction)
    {
        m_lastError = "Action '" + actionName
            + "' already has the maximum of eight bindings.";
        return false;
    }

    if (containsBinding(record.bindings, parsed.canonical))
    {
        m_lastError = "That input is already bound to '" + actionName + "'.";
        return false;
    }

    record.bindings[emptySlot] = parsed.canonical;
    record.parsedBindings[emptySlot] = std::move(parsed);
    record.analogSettings[emptySlot] =
        defaultAnalogSettings(record.parsedBindings[emptySlot]);
    record.hasUserBindings = true;
    markProfileDirty();
    return save();
}

bool InputSystem::removeBinding(
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
    if (bindingIndex >= kMaxBindingsPerAction
        || bindingIndex >= record.bindings.size())
    {
        m_lastError = "Binding index is outside action '" + actionName + "'.";
        return false;
    }
    if (record.bindings[bindingIndex].empty())
    {
        m_lastError.clear();
        return true;
    }

    // Binding slots are positional. Removing Binding 1 must never pull
    // Binding 2 into its place; the selected cell simply becomes empty.
    record.bindings[bindingIndex].clear();
    record.parsedBindings[bindingIndex] = ParsedBinding{};
    record.analogSettings[bindingIndex] = InputAnalogSettings{};
    record.hasUserBindings = true;
    record.value = 0.0f;
    record.down = false;
    record.pressed = false;
    record.released = false;
    markProfileDirty();
    return save();
}

bool InputSystem::resetBindings(const std::string& actionName)
{
    const auto iterator = m_actions.find(actionName);
    if (iterator == m_actions.end())
    {
        m_lastError = "Unknown input action: " + actionName;
        return false;
    }

    ActionRecord& record = iterator->second;
    record.bindings.assign(kMaxBindingsPerAction, {});
    record.parsedBindings.assign(kMaxBindingsPerAction, ParsedBinding{});
    record.analogSettings.assign(kMaxBindingsPerAction, InputAnalogSettings{});

    for (std::size_t index = 0;
        index < record.defaultBindings.size() && index < kMaxBindingsPerAction;
        ++index)
    {
        if (record.defaultBindings[index].empty())
            continue;

        ParsedBinding parsed;
        std::string error;
        if (!parseBinding(record.defaultBindings[index], parsed, error))
        {
            m_lastError = error;
            return false;
        }
        record.bindings[index] = parsed.canonical;
        record.parsedBindings[index] = std::move(parsed);
        record.analogSettings[index] =
            defaultAnalogSettings(record.parsedBindings[index]);
    }

    record.hasUserBindings = false;
    record.value = 0.0f;
    record.down = false;
    record.pressed = false;
    record.released = false;
    markProfileDirty();
    return save();
}

bool InputSystem::actionDown(const std::string& actionName) const
{
    const auto iterator = m_actions.find(actionName);
    return iterator != m_actions.end() && iterator->second.down;
}

bool InputSystem::actionPressed(const std::string& actionName) const
{
    const auto iterator = m_actions.find(actionName);
    return iterator != m_actions.end() && iterator->second.pressed;
}

bool InputSystem::actionReleased(const std::string& actionName) const
{
    const auto iterator = m_actions.find(actionName);
    return iterator != m_actions.end() && iterator->second.released;
}

float InputSystem::actionValue(const std::string& actionName) const
{
    const auto iterator = m_actions.find(actionName);
    return iterator != m_actions.end() ? iterator->second.value : 0.0f;
}

std::string InputSystem::actionBinding(const std::string& actionName) const
{
    const auto iterator = m_actions.find(actionName);
    if (iterator == m_actions.end())
        return {};

    std::ostringstream summary;
    bool first = true;
    for (const std::string& binding : iterator->second.bindings)
    {
        if (binding.empty())
            continue;
        if (!first)
            summary << " | ";
        summary << binding;
        first = false;
    }
    return summary.str();
}

std::string InputSystem::actionBinding(
    const std::string& actionName,
    std::size_t bindingIndex) const
{
    const auto iterator = m_actions.find(actionName);
    if (iterator == m_actions.end()
        || bindingIndex >= iterator->second.bindings.size())
    {
        return {};
    }
    return iterator->second.bindings[bindingIndex];
}

std::size_t InputSystem::actionBindingCount(const std::string& actionName) const
{
    const auto iterator = m_actions.find(actionName);
    return iterator != m_actions.end()
        ? occupiedBindingSpan(iterator->second.bindings)
        : 0;
}

std::vector<std::string> InputSystem::actionBindings(
    const std::string& actionName) const
{
    const auto iterator = m_actions.find(actionName);
    return iterator != m_actions.end()
        ? iterator->second.bindings
        : std::vector<std::string>{};
}

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
    m_lastError.clear();
    return true;
}

void InputSystem::cancelBindingCapture()
{
    m_captureAction.clear();
    m_captureBindingIndex = 0;
    m_captureAppend = false;
}

bool InputSystem::save()
{
    if (m_settingsPath.empty())
        return true;

    try
    {
        std::filesystem::create_directories(m_settingsPath.parent_path());
        std::ofstream file(m_settingsPath, std::ios::trunc);
        if (!file)
        {
            m_lastError = "Could not write input settings: " + m_settingsPath.string();
            return false;
        }

        file << "# Heritage Engine module input bindings\n";
        file << "# Format 6: positional slots, analogue processing, and named profile state\n";
        file << "profile.last_applied=" << m_lastAppliedProfile << '\n';
        file << "profile.dirty=" << (m_profileDirty ? 1 : 0) << '\n';
        file << std::fixed << std::setprecision(6);

        for (const auto& [name, record] : m_actions)
        {
            // Defaults remain module-owned. Only user binding overrides are stored.
            if (record.hasUserBindings)
            {
                file << "binding_count." << name << '='
                    << kMaxBindingsPerAction << '\n';
                for (std::size_t index = 0;
                    index < kMaxBindingsPerAction;
                    ++index)
                {
                    file << "binding." << name << '.' << index << '='
                        << (index < record.bindings.size()
                            ? record.bindings[index]
                            : std::string{})
                        << '\n';
                }
            }

            for (std::size_t index = 0;
                index < record.parsedBindings.size()
                    && index < record.analogSettings.size();
                ++index)
            {
                const ParsedBinding& binding = record.parsedBindings[index];
                const InputAnalogSettings& settings = record.analogSettings[index];
                if (!parsedBindingSupportsAnalog(binding)
                    || analogSettingsAreDefault(binding, settings))
                {
                    continue;
                }

                file << "analog." << name << '.' << index << '='
                    << binding.canonical << '|'
                    << (settings.invert ? 1 : 0) << ','
                    << settings.innerDeadzone << ','
                    << settings.outerDeadzone << ','
                    << settings.sensitivity << ','
                    << settings.bezierX1 << ','
                    << settings.bezierY1 << ','
                    << settings.bezierX2 << ','
                    << settings.bezierY2 << '\n';
            }
        }

        m_lastError.clear();
        return true;
    }
    catch (const std::exception& exception)
    {
        m_lastError = std::string("Could not save input settings: ") + exception.what();
        return false;
    }
}


std::filesystem::path InputSystem::profilesDirectory() const
{
    if (m_settingsPath.empty())
        return {};
    return m_settingsPath.parent_path() / "InputProfiles";
}

bool InputSystem::validateProfileName(
    const std::string& profileName,
    std::string& cleanName,
    std::string& errorMessage) const
{
    cleanName = trim(profileName);
    if (cleanName.empty())
    {
        errorMessage = "Profile names cannot be empty.";
        return false;
    }
    if (cleanName.size() > 48)
    {
        errorMessage = "Profile names may contain at most 48 characters.";
        return false;
    }
    if (cleanName == "." || cleanName == "..")
    {
        errorMessage = "That profile name is reserved.";
        return false;
    }
    if (cleanName.back() == ' ' || cleanName.back() == '.')
    {
        errorMessage = "Profile names cannot end with a space or period.";
        return false;
    }

    constexpr const char* invalid = "<>:\"/\\|?*=";
    for (unsigned char character : cleanName)
    {
        if (character < 32 || std::strchr(invalid, character) != nullptr)
        {
            errorMessage =
                "Profile names cannot contain control characters or <>:\"/\\|?*=.";
            return false;
        }
    }

    if (isReservedWindowsProfileName(cleanName))
    {
        errorMessage = "That profile name is reserved by Windows.";
        return false;
    }

    errorMessage.clear();
    return true;
}

std::filesystem::path InputSystem::profilePathForName(
    const std::string& cleanName) const
{
    const std::filesystem::path directory = profilesDirectory();
    if (directory.empty())
        return {};
    return directory / (cleanName + ".heinputprofile");
}

std::vector<InputProfileInfo> InputSystem::profiles() const
{
    std::vector<InputProfileInfo> result;
    const std::filesystem::path directory = profilesDirectory();
    if (directory.empty() || !std::filesystem::is_directory(directory))
        return result;

    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error))
    {
        if (error)
            break;
        if (!entry.is_regular_file(error) || error)
            continue;
        if (lower(entry.path().extension().string()) != ".heinputprofile")
            continue;

        ProfileSnapshot snapshot;
        std::string readError;
        if (!readProfileSnapshot(entry.path(), snapshot, readError))
            continue;

        InputProfileInfo info;
        info.name = snapshot.name;
        info.path = entry.path();
        result.push_back(std::move(info));
    }

    std::sort(result.begin(), result.end(),
        [](const InputProfileInfo& left, const InputProfileInfo& right) {
            std::string leftName = left.name;
            std::string rightName = right.name;
            std::transform(leftName.begin(), leftName.end(), leftName.begin(),
                [](unsigned char character) {
                    return static_cast<char>(std::tolower(character));
                });
            std::transform(rightName.begin(), rightName.end(), rightName.begin(),
                [](unsigned char character) {
                    return static_cast<char>(std::tolower(character));
                });
            return leftName < rightName;
        });
    return result;
}

std::filesystem::path InputSystem::findProfilePath(
    const std::string& profileName) const
{
    const std::string clean = trim(profileName);
    for (const InputProfileInfo& profile : profiles())
    {
        if (profileNamesEqual(profile.name, clean))
            return profile.path;
    }
    return {};
}

bool InputSystem::profileExists(const std::string& profileName) const
{
    return !findProfilePath(profileName).empty();
}

InputSystem::ProfileSnapshot InputSystem::captureProfileSnapshot(
    const std::string& profileName) const
{
    ProfileSnapshot snapshot;
    snapshot.name = profileName;

    for (const auto& [actionName, record] : m_actions)
    {
        ProfileActionSnapshot action;
        action.bindings.assign(kMaxBindingsPerAction, {});

        for (std::size_t index = 0;
            index < record.bindings.size() && index < kMaxBindingsPerAction;
            ++index)
        {
            action.bindings[index] = record.bindings[index];

            if (index >= record.parsedBindings.size()
                || index >= record.analogSettings.size()
                || !parsedBindingSupportsAnalog(record.parsedBindings[index]))
            {
                continue;
            }

            LoadedAnalogSettings loaded;
            loaded.binding = record.parsedBindings[index].canonical;
            loaded.settings = sanitizeAnalogSettings(record.analogSettings[index]);
            action.analogSettings[index] = loaded;
        }

        snapshot.actions[actionName] = std::move(action);
    }

    return snapshot;
}

bool InputSystem::writeProfileSnapshot(
    const std::filesystem::path& path,
    const ProfileSnapshot& snapshot)
{
    try
    {
        std::filesystem::create_directories(path.parent_path());
        const std::filesystem::path temporary = path.string() + ".tmp";
        std::ofstream file(temporary, std::ios::trunc);
        if (!file)
        {
            m_lastError = "Could not write input profile: " + path.string();
            return false;
        }

        file << "# Heritage Engine named input profile\n";
        file << "# Complete snapshot: eight binding slots and analogue settings\n";
        file << "profile_format=1\n";
        file << "profile_name=" << snapshot.name << '\n';
        file << std::fixed << std::setprecision(6);

        for (const auto& [actionName, action] : snapshot.actions)
        {
            file << "binding_count." << actionName << '='
                << kMaxBindingsPerAction << '\n';

            for (std::size_t index = 0; index < kMaxBindingsPerAction; ++index)
            {
                file << "binding." << actionName << '.' << index << '=';
                if (index < action.bindings.size())
                    file << action.bindings[index];
                file << '\n';
            }

            for (const auto& [index, loaded] : action.analogSettings)
            {
                if (index >= kMaxBindingsPerAction || loaded.binding.empty())
                    continue;

                const InputAnalogSettings settings =
                    sanitizeAnalogSettings(loaded.settings);
                file << "analog." << actionName << '.' << index << '='
                    << loaded.binding << '|'
                    << (settings.invert ? 1 : 0) << ','
                    << settings.innerDeadzone << ','
                    << settings.outerDeadzone << ','
                    << settings.sensitivity << ','
                    << settings.bezierX1 << ','
                    << settings.bezierY1 << ','
                    << settings.bezierX2 << ','
                    << settings.bezierY2 << '\n';
            }
        }

        file.close();
        if (!file)
        {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            m_lastError = "Could not finish writing input profile: " + path.string();
            return false;
        }

        std::error_code error;
        std::filesystem::remove(path, error);
        error.clear();
        std::filesystem::rename(temporary, path, error);
        if (error)
        {
            std::filesystem::remove(temporary, error);
            m_lastError = "Could not install input profile: " + path.string();
            return false;
        }

        m_lastError.clear();
        return true;
    }
    catch (const std::exception& exception)
    {
        m_lastError =
            std::string("Could not save input profile: ") + exception.what();
        return false;
    }
}

bool InputSystem::readProfileSnapshot(
    const std::filesystem::path& path,
    ProfileSnapshot& snapshot,
    std::string& errorMessage) const
{
    snapshot = {};

    std::ifstream file(path);
    if (!file)
    {
        errorMessage = "Could not open input profile: " + path.string();
        return false;
    }

    std::unordered_map<std::string, std::map<std::size_t, std::string>> indexed;
    std::unordered_map<std::string, std::size_t> declaredCounts;
    std::unordered_map<std::string, std::map<std::size_t, LoadedAnalogSettings>>
        analog;

    std::string line;
    while (std::getline(file, line))
    {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;

        const std::size_t equals = line.find('=');
        if (equals == std::string::npos)
            continue;

        const std::string key = trim(line.substr(0, equals));
        const std::string value = trim(line.substr(equals + 1));

        if (key == "profile_name")
        {
            snapshot.name = value;
            continue;
        }

        constexpr const char* analogPrefix = "analog.";
        if (key.rfind(analogPrefix, 0) == 0)
        {
            const std::string tail = key.substr(
                std::char_traits<char>::length(analogPrefix));
            const std::size_t lastDot = tail.rfind('.');
            if (lastDot == std::string::npos)
                continue;

            const std::string indexText = tail.substr(lastDot + 1);
            if (!isUnsignedInteger(indexText))
                continue;

            const std::string actionName = tail.substr(0, lastDot);
            const std::size_t pipe = value.find('|');
            if (actionName.empty() || pipe == std::string::npos)
                continue;

            LoadedAnalogSettings loaded;
            loaded.binding = trim(value.substr(0, pipe));

            std::stringstream values(value.substr(pipe + 1));
            std::array<std::string, 8> fields{};
            bool complete = true;
            for (std::size_t field = 0; field < fields.size(); ++field)
            {
                if (!std::getline(values, fields[field], ','))
                {
                    complete = false;
                    break;
                }
            }
            if (!complete)
                continue;

            try
            {
                loaded.settings.invert = std::stoi(trim(fields[0])) != 0;
                loaded.settings.innerDeadzone = std::stof(trim(fields[1]));
                loaded.settings.outerDeadzone = std::stof(trim(fields[2]));
                loaded.settings.sensitivity = std::stof(trim(fields[3]));
                loaded.settings.bezierX1 = std::stof(trim(fields[4]));
                loaded.settings.bezierY1 = std::stof(trim(fields[5]));
                loaded.settings.bezierX2 = std::stof(trim(fields[6]));
                loaded.settings.bezierY2 = std::stof(trim(fields[7]));
                loaded.settings = sanitizeAnalogSettings(loaded.settings);
                analog[actionName][
                    static_cast<std::size_t>(std::stoull(indexText))] = loaded;
            }
            catch (...) {}
            continue;
        }

        constexpr const char* countPrefix = "binding_count.";
        if (key.rfind(countPrefix, 0) == 0)
        {
            const std::string actionName = key.substr(
                std::char_traits<char>::length(countPrefix));
            try
            {
                declaredCounts[actionName] =
                    static_cast<std::size_t>(std::stoull(value));
            }
            catch (...) {}
            continue;
        }

        constexpr const char* bindingPrefix = "binding.";
        if (key.rfind(bindingPrefix, 0) != 0)
            continue;

        const std::string tail = key.substr(
            std::char_traits<char>::length(bindingPrefix));
        const std::size_t lastDot = tail.rfind('.');
        if (lastDot == std::string::npos)
            continue;

        const std::string indexText = tail.substr(lastDot + 1);
        if (!isUnsignedInteger(indexText))
            continue;

        const std::string actionName = tail.substr(0, lastDot);
        indexed[actionName][
            static_cast<std::size_t>(std::stoull(indexText))] = value;
    }

    if (snapshot.name.empty())
        snapshot.name = path.stem().string();

    for (const auto& [actionName, count] : declaredCounts)
    {
        (void)count;
        snapshot.actions[actionName].bindings.assign(
            kMaxBindingsPerAction,
            {});
    }
    for (const auto& [actionName, values] : indexed)
    {
        ProfileActionSnapshot& action = snapshot.actions[actionName];
        if (action.bindings.empty())
            action.bindings.assign(kMaxBindingsPerAction, {});

        for (const auto& [index, value] : values)
        {
            if (index < kMaxBindingsPerAction)
                action.bindings[index] = value;
        }
    }
    for (const auto& [actionName, values] : analog)
    {
        ProfileActionSnapshot& action = snapshot.actions[actionName];
        if (action.bindings.empty())
            action.bindings.assign(kMaxBindingsPerAction, {});
        action.analogSettings = values;
    }

    errorMessage.clear();
    return true;
}

bool InputSystem::applyProfileSnapshot(const ProfileSnapshot& snapshot)
{
    struct PreparedAction
    {
        std::vector<std::string> bindings;
        std::vector<ParsedBinding> parsedBindings;
        std::vector<InputAnalogSettings> analogSettings;
    };

    std::map<std::string, PreparedAction> prepared;

    for (const auto& [actionName, record] : m_actions)
    {
        const auto profileAction = snapshot.actions.find(actionName);
        if (profileAction == snapshot.actions.end())
            continue;

        PreparedAction action;
        action.bindings.assign(kMaxBindingsPerAction, {});
        action.parsedBindings.assign(kMaxBindingsPerAction, ParsedBinding{});
        action.analogSettings.assign(kMaxBindingsPerAction, InputAnalogSettings{});

        for (std::size_t index = 0; index < kMaxBindingsPerAction; ++index)
        {
            if (index >= profileAction->second.bindings.size()
                || profileAction->second.bindings[index].empty())
            {
                continue;
            }

            ParsedBinding parsed;
            std::string parseError;
            if (!parseBinding(
                profileAction->second.bindings[index],
                parsed,
                parseError))
            {
                m_lastError = "Profile '" + snapshot.name
                    + "', action '" + actionName
                    + "', Binding " + std::to_string(index + 1)
                    + ": " + parseError;
                return false;
            }

            if (containsBinding(action.bindings, parsed.canonical))
            {
                m_lastError = "Profile '" + snapshot.name
                    + "' binds the same input more than once to action '"
                    + actionName + "'.";
                return false;
            }

            action.bindings[index] = parsed.canonical;
            action.parsedBindings[index] = parsed;
            action.analogSettings[index] = defaultAnalogSettings(parsed);

            const auto analogSetting =
                profileAction->second.analogSettings.find(index);
            if (analogSetting != profileAction->second.analogSettings.end()
                && analogSetting->second.binding == parsed.canonical
                && parsedBindingSupportsAnalog(parsed))
            {
                action.analogSettings[index] =
                    sanitizeAnalogSettings(analogSetting->second.settings);
            }
        }

        prepared[actionName] = std::move(action);
    }

    for (auto& [actionName, action] : prepared)
    {
        ActionRecord& record = m_actions[actionName];
        record.bindings = std::move(action.bindings);
        record.parsedBindings = std::move(action.parsedBindings);
        record.analogSettings = std::move(action.analogSettings);
        record.hasUserBindings = true;
        record.value = 0.0f;
        record.down = false;
        record.pressed = false;
        record.released = false;
    }

    cancelBindingCapture();
    m_lastAppliedProfile = snapshot.name;
    m_profileDirty = false;
    return save();
}

bool InputSystem::createProfile(const std::string& profileName)
{
    std::string cleanName;
    std::string errorMessage;
    if (!validateProfileName(profileName, cleanName, errorMessage))
    {
        m_lastError = errorMessage;
        return false;
    }
    if (profileExists(cleanName))
    {
        m_lastError = "An input profile named '" + cleanName + "' already exists.";
        return false;
    }

    const ProfileSnapshot snapshot = captureProfileSnapshot(cleanName);
    if (!writeProfileSnapshot(profilePathForName(cleanName), snapshot))
        return false;

    m_lastAppliedProfile = cleanName;
    m_profileDirty = false;
    return save();
}

bool InputSystem::updateProfile(const std::string& profileName)
{
    const std::filesystem::path path = findProfilePath(profileName);
    if (path.empty())
    {
        m_lastError = "Input profile not found: " + trim(profileName);
        return false;
    }

    ProfileSnapshot existing;
    std::string errorMessage;
    if (!readProfileSnapshot(path, existing, errorMessage))
    {
        m_lastError = errorMessage;
        return false;
    }

    const ProfileSnapshot snapshot = captureProfileSnapshot(existing.name);
    if (!writeProfileSnapshot(path, snapshot))
        return false;

    m_lastAppliedProfile = existing.name;
    m_profileDirty = false;
    return save();
}

bool InputSystem::applyProfile(const std::string& profileName)
{
    const std::filesystem::path path = findProfilePath(profileName);
    if (path.empty())
    {
        m_lastError = "Input profile not found: " + trim(profileName);
        return false;
    }

    ProfileSnapshot snapshot;
    std::string errorMessage;
    if (!readProfileSnapshot(path, snapshot, errorMessage))
    {
        m_lastError = errorMessage;
        return false;
    }

    return applyProfileSnapshot(snapshot);
}

bool InputSystem::duplicateProfile(
    const std::string& sourceProfileName,
    const std::string& newProfileName)
{
    const std::filesystem::path sourcePath =
        findProfilePath(sourceProfileName);
    if (sourcePath.empty())
    {
        m_lastError = "Input profile not found: " + trim(sourceProfileName);
        return false;
    }

    std::string cleanName;
    std::string errorMessage;
    if (!validateProfileName(newProfileName, cleanName, errorMessage))
    {
        m_lastError = errorMessage;
        return false;
    }
    if (profileExists(cleanName))
    {
        m_lastError = "An input profile named '" + cleanName + "' already exists.";
        return false;
    }

    ProfileSnapshot snapshot;
    if (!readProfileSnapshot(sourcePath, snapshot, errorMessage))
    {
        m_lastError = errorMessage;
        return false;
    }

    snapshot.name = cleanName;
    return writeProfileSnapshot(profilePathForName(cleanName), snapshot);
}

bool InputSystem::renameProfile(
    const std::string& oldProfileName,
    const std::string& newProfileName)
{
    const std::filesystem::path oldPath = findProfilePath(oldProfileName);
    if (oldPath.empty())
    {
        m_lastError = "Input profile not found: " + trim(oldProfileName);
        return false;
    }

    std::string cleanName;
    std::string errorMessage;
    if (!validateProfileName(newProfileName, cleanName, errorMessage))
    {
        m_lastError = errorMessage;
        return false;
    }
    if (!profileNamesEqual(oldProfileName, cleanName)
        && profileExists(cleanName))
    {
        m_lastError = "An input profile named '" + cleanName + "' already exists.";
        return false;
    }

    ProfileSnapshot snapshot;
    if (!readProfileSnapshot(oldPath, snapshot, errorMessage))
    {
        m_lastError = errorMessage;
        return false;
    }

    snapshot.name = cleanName;
    const bool sameLogicalName =
        profileNamesEqual(oldProfileName, cleanName);
    const std::filesystem::path newPath = sameLogicalName
        ? oldPath
        : profilePathForName(cleanName);
    if (!writeProfileSnapshot(newPath, snapshot))
        return false;

    if (!sameLogicalName && oldPath != newPath)
    {
        std::error_code error;
        std::filesystem::remove(oldPath, error);
        if (error)
        {
            std::error_code ignored;
            std::filesystem::remove(newPath, ignored);
            m_lastError = "Could not remove the old input profile file.";
            return false;
        }
    }

    if (profileNamesEqual(m_lastAppliedProfile, oldProfileName))
    {
        m_lastAppliedProfile = cleanName;
        return save();
    }

    m_lastError.clear();
    return true;
}

bool InputSystem::deleteProfile(const std::string& profileName)
{
    const std::filesystem::path path = findProfilePath(profileName);
    if (path.empty())
    {
        m_lastError = "Input profile not found: " + trim(profileName);
        return false;
    }

    std::error_code error;
    const bool removed = std::filesystem::remove(path, error);
    if (error || !removed)
    {
        m_lastError = "Could not delete input profile: " + path.string();
        return false;
    }

    if (profileNamesEqual(m_lastAppliedProfile, profileName))
    {
        m_lastAppliedProfile.clear();
        m_profileDirty = false;
        return save();
    }

    m_lastError.clear();
    return true;
}

void InputSystem::markProfileDirty()
{
    if (!m_lastAppliedProfile.empty())
        m_profileDirty = true;
}

bool InputSystem::load()
{
    m_loadedBindings.clear();
    m_loadedBindingOverrides.clear();
    m_loadedLegacyActions.clear();
    m_loadedAnalogSettings.clear();
    m_lastAppliedProfile.clear();
    m_profileDirty = false;

    if (m_settingsPath.empty() || !std::filesystem::is_regular_file(m_settingsPath))
        return true;

    std::ifstream file(m_settingsPath);
    if (!file)
    {
        m_lastError = "Could not open input settings: " + m_settingsPath.string();
        return false;
    }

    std::unordered_map<std::string, std::map<std::size_t, std::string>> indexed;
    std::unordered_map<std::string, std::size_t> declaredCounts;
    std::unordered_map<std::string, std::string> legacy;

    std::string line;
    while (std::getline(file, line))
    {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;

        const std::size_t equals = line.find('=');
        if (equals == std::string::npos)
            continue;

        const std::string key = trim(line.substr(0, equals));
        const std::string value = trim(line.substr(equals + 1));

        if (key == "profile.last_applied")
        {
            m_lastAppliedProfile = value;
            continue;
        }
        if (key == "profile.dirty")
        {
            m_profileDirty = value == "1" || lower(value) == "true";
            continue;
        }

        constexpr const char* analogPrefix = "analog.";
        if (key.rfind(analogPrefix, 0) == 0)
        {
            const std::string tail = key.substr(
                std::char_traits<char>::length(analogPrefix));
            const std::size_t lastDot = tail.rfind('.');
            if (lastDot == std::string::npos)
                continue;

            const std::string indexText = tail.substr(lastDot + 1);
            if (!isUnsignedInteger(indexText))
                continue;

            const std::string actionName = tail.substr(0, lastDot);
            const std::size_t pipe = value.find('|');
            if (actionName.empty() || pipe == std::string::npos)
                continue;

            LoadedAnalogSettings loaded;
            loaded.binding = trim(value.substr(0, pipe));
            std::stringstream values(value.substr(pipe + 1));
            std::array<std::string, 8> fields{};
            bool complete = true;
            for (std::size_t field = 0; field < fields.size(); ++field)
            {
                if (!std::getline(values, fields[field], ','))
                {
                    complete = false;
                    break;
                }
            }
            if (!complete)
                continue;

            try
            {
                loaded.settings.invert = std::stoi(trim(fields[0])) != 0;
                loaded.settings.innerDeadzone = std::stof(trim(fields[1]));
                loaded.settings.outerDeadzone = std::stof(trim(fields[2]));
                loaded.settings.sensitivity = std::stof(trim(fields[3]));
                loaded.settings.bezierX1 = std::stof(trim(fields[4]));
                loaded.settings.bezierY1 = std::stof(trim(fields[5]));
                loaded.settings.bezierX2 = std::stof(trim(fields[6]));
                loaded.settings.bezierY2 = std::stof(trim(fields[7]));
                loaded.settings = sanitizeAnalogSettings(loaded.settings);
                m_loadedAnalogSettings[actionName][
                    static_cast<std::size_t>(std::stoull(indexText))] = loaded;
            }
            catch (...) {}
            continue;
        }

        constexpr const char* countPrefix = "binding_count.";
        if (key.rfind(countPrefix, 0) == 0)
        {
            const std::string actionName = key.substr(
                std::char_traits<char>::length(countPrefix));
            try
            {
                declaredCounts[actionName] = static_cast<std::size_t>(std::stoull(value));
                m_loadedBindingOverrides.insert(actionName);
            }
            catch (...) {}
            continue;
        }

        constexpr const char* bindingPrefix = "binding.";
        if (key.rfind(bindingPrefix, 0) != 0)
            continue;

        const std::string tail = key.substr(
            std::char_traits<char>::length(bindingPrefix));
        const std::size_t lastDot = tail.rfind('.');

        if (lastDot != std::string::npos)
        {
            const std::string suffix = tail.substr(lastDot + 1);
            if (isUnsignedInteger(suffix))
            {
                const std::string actionName = tail.substr(0, lastDot);
                indexed[actionName][static_cast<std::size_t>(std::stoull(suffix))] = value;
                m_loadedBindingOverrides.insert(actionName);
                continue;
            }
        }

        // Step 26E legacy format: binding.Action Name=Key:W
        legacy[tail] = value;
        m_loadedBindingOverrides.insert(tail);
        m_loadedLegacyActions.insert(tail);
    }

    for (const auto& [actionName, count] : declaredCounts)
    {
        (void)count;
        std::vector<std::string>& bindings = m_loadedBindings[actionName];
        bindings.assign(kMaxBindingsPerAction, {});
        const auto indexedIterator = indexed.find(actionName);
        if (indexedIterator == indexed.end())
            continue;

        for (const auto& [index, value] : indexedIterator->second)
        {
            if (index < kMaxBindingsPerAction)
                bindings[index] = value;
        }
    }

    for (const auto& [actionName, values] : indexed)
    {
        if (declaredCounts.find(actionName) != declaredCounts.end())
            continue;

        std::vector<std::string>& bindings = m_loadedBindings[actionName];
        bindings.assign(kMaxBindingsPerAction, {});
        for (const auto& [index, value] : values)
        {
            if (index < kMaxBindingsPerAction)
                bindings[index] = value;
        }
    }

    for (const auto& [actionName, value] : legacy)
    {
        if (m_loadedBindings.find(actionName) != m_loadedBindings.end())
            continue;
        std::vector<std::string> bindings(kMaxBindingsPerAction);
        bindings[0] = value;
        m_loadedBindings[actionName] = std::move(bindings);
    }

    m_lastError.clear();
    return true;
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

bool InputSystem::parseBinding(
    const std::string& text,
    ParsedBinding& result,
    std::string& errorMessage) const
{
    result = ParsedBinding{};
    const std::string cleaned = trim(text);
    const std::size_t colon = cleaned.find(':');
    if (colon == std::string::npos)
    {
        errorMessage = "Binding must use Type:Name syntax, for example Key:W.";
        return false;
    }

    const std::string originalType = trim(cleaned.substr(0, colon));
    const std::string type = lower(originalType);
    std::string name = trim(cleaned.substr(colon + 1));

    if (type == "key")
    {
        const int code = keyCodeFromName(name);
        const std::string canonicalName = keyNameFromCode(code);
        if (code < 0 || canonicalName.empty())
        {
            errorMessage = "Unknown keyboard key: " + name;
            return false;
        }
        result.type = BindingType::Key;
        result.code = code;
        result.canonical = "Key:" + canonicalName;
        return true;
    }

    if (type == "mouse")
    {
        const int code = mouseButtonFromName(name);
        if (code < 0)
        {
            errorMessage = "Unknown mouse button: " + name;
            return false;
        }
        result.type = BindingType::MouseButton;
        result.code = code;
        result.canonical = "Mouse:" + mouseButtonName(code);
        return true;
    }

    if (type.size() > 8
        && type.rfind("dinput[", 0) == 0
        && type.back() == ']')
    {
        const std::string guid = lower(trim(originalType.substr(
            std::char_traits<char>::length("DInput["),
            originalType.size() - std::char_traits<char>::length("DInput[") - 1)));
        if (guid.empty())
        {
            errorMessage = "A DirectInput binding needs a device GUID.";
            return false;
        }

        result.type = BindingType::DirectInput;
        result.directInputGuid = guid;
        const std::string prefix = "DInput[" + guid + "]:";

        bool positive = false;
        bool negative = false;
        if (!name.empty() && name.back() == '+')
        {
            positive = true;
            name.pop_back();
        }
        else if (!name.empty() && name.back() == '-')
        {
            negative = true;
            name.pop_back();
        }
        name = trim(name);

        if (positive || negative)
        {
            const int axis = WindowsDirectInputBackend::axisIndexFromName(name);
            if (axis < 0)
            {
                errorMessage = "Unknown DirectInput axis: " + name;
                return false;
            }
            result.code = axis;
            result.directInputControl = positive
                ? WindowsDirectInputBackend::ControlType::AxisPositive
                : WindowsDirectInputBackend::ControlType::AxisNegative;
            result.canonical = prefix
                + WindowsDirectInputBackend::axisName(axis)
                + (positive ? "+" : "-");
            return true;
        }

        const std::string loweredName = lower(name);
        constexpr const char* buttonPrefix = "button";
        if (loweredName.rfind(buttonPrefix, 0) == 0)
        {
            const std::string numberText = loweredName.substr(
                std::char_traits<char>::length(buttonPrefix));
            if (!isUnsignedInteger(numberText))
            {
                errorMessage = "DirectInput buttons use Button1 through Button128.";
                return false;
            }
            const int oneBased = std::stoi(numberText);
            if (oneBased < 1
                || oneBased > static_cast<int>(WindowsDirectInputBackend::kButtonCount))
            {
                errorMessage = "DirectInput button number is outside 1-128.";
                return false;
            }
            result.code = oneBased - 1;
            result.directInputControl = WindowsDirectInputBackend::ControlType::Button;
            result.canonical = prefix + "Button" + std::to_string(oneBased);
            return true;
        }

        constexpr const char* povPrefix = "pov";
        if (loweredName.rfind(povPrefix, 0) == 0)
        {
            std::size_t digitEnd = std::char_traits<char>::length(povPrefix);
            while (digitEnd < loweredName.size()
                && std::isdigit(static_cast<unsigned char>(loweredName[digitEnd])) != 0)
            {
                ++digitEnd;
            }
            const std::string numberText = loweredName.substr(
                std::char_traits<char>::length(povPrefix),
                digitEnd - std::char_traits<char>::length(povPrefix));
            const std::string directionText = name.substr(digitEnd);
            WindowsDirectInputBackend::ControlType direction;
            if (!isUnsignedInteger(numberText)
                || !WindowsDirectInputBackend::povDirectionFromName(
                    directionText,
                    direction))
            {
                errorMessage = "DirectInput POV bindings use forms such as Pov1Up or Pov1DownRight.";
                return false;
            }
            const int oneBased = std::stoi(numberText);
            if (oneBased < 1
                || oneBased > static_cast<int>(WindowsDirectInputBackend::kPovCount))
            {
                errorMessage = "DirectInput POV number is outside 1-4.";
                return false;
            }
            result.code = oneBased - 1;
            result.directInputControl = direction;
            result.canonical = prefix + "Pov" + std::to_string(oneBased)
                + WindowsDirectInputBackend::povDirectionName(direction);
            return true;
        }

        errorMessage = "Unknown DirectInput control: " + name;
        return false;
    }

    std::string canonicalGamepadPrefix;
    if (type == "gamepad")
    {
        result.gamepadSelector = GamepadSelector::Any;
        canonicalGamepadPrefix = "Gamepad:";
    }
    else if (type.size() > 9
        && type.rfind("gamepad[", 0) == 0
        && type.back() == ']')
    {
        const std::string guid = lower(trim(originalType.substr(
            std::char_traits<char>::length("Gamepad["),
            originalType.size() - std::char_traits<char>::length("Gamepad[") - 1)));
        if (guid.empty())
        {
            errorMessage = "A device-specific gamepad binding needs a GUID.";
            return false;
        }
        result.gamepadSelector = GamepadSelector::Guid;
        result.gamepadGuid = guid;
        canonicalGamepadPrefix = "Gamepad[" + guid + "]:";
    }
    else if (type.rfind("gamepad", 0) == 0)
    {
        const std::string ordinalText = type.substr(
            std::char_traits<char>::length("gamepad"));
        if (!isUnsignedInteger(ordinalText))
        {
            errorMessage = "Unknown binding type: " + originalType;
            return false;
        }
        const int oneBasedOrdinal = std::stoi(ordinalText);
        if (oneBasedOrdinal < 1)
        {
            errorMessage = "Gamepad ordinals begin at 1.";
            return false;
        }
        result.gamepadSelector = GamepadSelector::Ordinal;
        result.gamepadOrdinal = oneBasedOrdinal - 1;
        canonicalGamepadPrefix = "Gamepad" + std::to_string(oneBasedOrdinal) + ":";
    }
    else
    {
        errorMessage = "Unknown binding type: " + originalType;
        return false;
    }

    bool positive = false;
    bool negative = false;
    if (!name.empty() && name.back() == '+')
    {
        positive = true;
        name.pop_back();
    }
    else if (!name.empty() && name.back() == '-')
    {
        negative = true;
        name.pop_back();
    }
    name = trim(name);

    if (positive || negative)
    {
        const int axis = gamepadAxisFromName(name);
        if (axis < 0)
        {
            errorMessage = "Unknown gamepad axis: " + name;
            return false;
        }
        if (negative
            && (axis == GLFW_GAMEPAD_AXIS_LEFT_TRIGGER
                || axis == GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER))
        {
            errorMessage = "Gamepad triggers support the positive direction only.";
            return false;
        }
        result.type = positive
            ? BindingType::GamepadAxisPositive
            : BindingType::GamepadAxisNegative;
        result.code = axis;
        result.canonical = canonicalGamepadPrefix + gamepadAxisName(axis)
            + (positive ? "+" : "-");
        return true;
    }

    const int button = gamepadButtonFromName(name);
    if (button < 0)
    {
        errorMessage = "Unknown gamepad button: " + name;
        return false;
    }
    result.type = BindingType::GamepadButton;
    result.code = button;
    result.canonical = canonicalGamepadPrefix + gamepadButtonName(button);
    return true;
}

bool InputSystem::parseBindingList(
    const std::string& text,
    std::vector<ParsedBinding>& parsed,
    std::vector<std::string>& canonical,
    std::string& errorMessage) const
{
    parsed.clear();
    canonical.clear();

    for (const std::string& part : splitBindingList(text))
    {
        ParsedBinding binding;
        if (!parseBinding(part, binding, errorMessage))
            return false;
        if (containsBinding(canonical, binding.canonical))
            continue;
        canonical.push_back(binding.canonical);
        parsed.push_back(std::move(binding));
    }

    if (canonical.empty())
    {
        errorMessage = "At least one binding is required.";
        return false;
    }
    return true;
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

std::string InputSystem::bindingDisplayName(const ParsedBinding& binding) const
{
    switch (binding.type)
    {
    case BindingType::Key:
        return "Keyboard: " + keyNameFromCode(binding.code);
    case BindingType::MouseButton:
        return "Mouse: " + mouseButtonName(binding.code);
    case BindingType::DirectInput:
    {
        std::string device = m_directInput.deviceName(binding.directInputGuid);
        if (device.empty())
        {
            const std::string shortGuid = binding.directInputGuid.substr(
                0, (std::min)(std::size_t{ 8 }, binding.directInputGuid.size()));
            device = "DirectInput " + shortGuid + " (disconnected)";
        }

        std::string control;
        using DirectControl = WindowsDirectInputBackend::ControlType;
        switch (binding.directInputControl)
        {
        case DirectControl::Button:
            control = "Button " + std::to_string(binding.code + 1);
            break;
        case DirectControl::AxisPositive:
            control = WindowsDirectInputBackend::axisName(binding.code) + "+";
            break;
        case DirectControl::AxisNegative:
            control = WindowsDirectInputBackend::axisName(binding.code) + "-";
            break;
        default:
            control = "POV " + std::to_string(binding.code + 1) + " "
                + WindowsDirectInputBackend::povDirectionName(
                    binding.directInputControl);
            break;
        }
        return device + ": " + control;
    }
    case BindingType::GamepadButton:
    case BindingType::GamepadAxisPositive:
    case BindingType::GamepadAxisNegative:
        break;
    default:
        return "Unbound";
    }

    std::string device;
    if (binding.gamepadSelector == GamepadSelector::Any)
    {
        device = "Any gamepad";
    }
    else if (binding.gamepadSelector == GamepadSelector::Ordinal)
    {
        const GamepadSnapshot* gamepad = gamepadForOrdinal(binding.gamepadOrdinal);
        device = gamepad
            ? gamepad->name
            : "Gamepad " + std::to_string(binding.gamepadOrdinal + 1)
                + " (disconnected)";
    }
    else
    {
        const GamepadSnapshot* connected = nullptr;
        for (const GamepadSnapshot& gamepad : m_gamepads)
        {
            if (gamepad.available
                && lower(gamepad.guid) == lower(binding.gamepadGuid))
            {
                connected = &gamepad;
                break;
            }
        }
        if (connected)
        {
            device = connected->name;
        }
        else
        {
            const std::string shortGuid = binding.gamepadGuid.substr(
                0, (std::min)(std::size_t{ 8 }, binding.gamepadGuid.size()));
            device = "Gamepad " + shortGuid + " (disconnected)";
        }
    }

    std::string control;
    if (binding.type == BindingType::GamepadButton)
        control = gamepadButtonName(binding.code);
    else
        control = gamepadAxisName(binding.code)
            + (binding.type == BindingType::GamepadAxisPositive ? "+" : "-");

    return device + ": " + control;
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

int InputSystem::keyCodeFromName(const std::string& name)
{
    const std::string value = lower(trim(name));
    if (value.size() == 1)
    {
        const unsigned char character = static_cast<unsigned char>(value[0]);
        if (character >= 'a' && character <= 'z')
            return GLFW_KEY_A + (character - 'a');
        if (character >= '0' && character <= '9')
            return GLFW_KEY_0 + (character - '0');
    }

    const std::unordered_map<std::string, int> named = {
        {"space", GLFW_KEY_SPACE}, {"apostrophe", GLFW_KEY_APOSTROPHE},
        {"comma", GLFW_KEY_COMMA}, {"minus", GLFW_KEY_MINUS},
        {"period", GLFW_KEY_PERIOD}, {"slash", GLFW_KEY_SLASH},
        {"semicolon", GLFW_KEY_SEMICOLON}, {"equal", GLFW_KEY_EQUAL},
        {"leftbracket", GLFW_KEY_LEFT_BRACKET}, {"backslash", GLFW_KEY_BACKSLASH},
        {"rightbracket", GLFW_KEY_RIGHT_BRACKET}, {"grave", GLFW_KEY_GRAVE_ACCENT},
        {"escape", GLFW_KEY_ESCAPE}, {"enter", GLFW_KEY_ENTER},
        {"tab", GLFW_KEY_TAB}, {"backspace", GLFW_KEY_BACKSPACE},
        {"insert", GLFW_KEY_INSERT}, {"delete", GLFW_KEY_DELETE},
        {"right", GLFW_KEY_RIGHT}, {"left", GLFW_KEY_LEFT},
        {"down", GLFW_KEY_DOWN}, {"up", GLFW_KEY_UP},
        {"pageup", GLFW_KEY_PAGE_UP}, {"pagedown", GLFW_KEY_PAGE_DOWN},
        {"home", GLFW_KEY_HOME}, {"end", GLFW_KEY_END},
        {"capslock", GLFW_KEY_CAPS_LOCK}, {"scrolllock", GLFW_KEY_SCROLL_LOCK},
        {"numlock", GLFW_KEY_NUM_LOCK}, {"printscreen", GLFW_KEY_PRINT_SCREEN},
        {"pause", GLFW_KEY_PAUSE}, {"leftshift", GLFW_KEY_LEFT_SHIFT},
        {"leftctrl", GLFW_KEY_LEFT_CONTROL}, {"leftcontrol", GLFW_KEY_LEFT_CONTROL},
        {"leftalt", GLFW_KEY_LEFT_ALT}, {"rightshift", GLFW_KEY_RIGHT_SHIFT},
        {"rightctrl", GLFW_KEY_RIGHT_CONTROL}, {"rightcontrol", GLFW_KEY_RIGHT_CONTROL},
        {"rightalt", GLFW_KEY_RIGHT_ALT}, {"menu", GLFW_KEY_MENU}
    };

    const auto iterator = named.find(value);
    if (iterator != named.end())
        return iterator->second;

    if (value.size() >= 2 && value[0] == 'f')
    {
        try
        {
            const int number = std::stoi(value.substr(1));
            if (number >= 1 && number <= 25)
                return GLFW_KEY_F1 + (number - 1);
        }
        catch (...) {}
    }

    return -1;
}

std::string InputSystem::keyNameFromCode(int key)
{
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
        return std::string(1, static_cast<char>('A' + (key - GLFW_KEY_A)));
    if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
        return std::string(1, static_cast<char>('0' + (key - GLFW_KEY_0)));
    if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25)
        return "F" + std::to_string(key - GLFW_KEY_F1 + 1);

    const std::map<int, std::string> named = {
        {GLFW_KEY_SPACE, "Space"}, {GLFW_KEY_APOSTROPHE, "Apostrophe"},
        {GLFW_KEY_COMMA, "Comma"}, {GLFW_KEY_MINUS, "Minus"},
        {GLFW_KEY_PERIOD, "Period"}, {GLFW_KEY_SLASH, "Slash"},
        {GLFW_KEY_SEMICOLON, "Semicolon"}, {GLFW_KEY_EQUAL, "Equal"},
        {GLFW_KEY_LEFT_BRACKET, "LeftBracket"}, {GLFW_KEY_BACKSLASH, "Backslash"},
        {GLFW_KEY_RIGHT_BRACKET, "RightBracket"}, {GLFW_KEY_GRAVE_ACCENT, "Grave"},
        {GLFW_KEY_ESCAPE, "Escape"}, {GLFW_KEY_ENTER, "Enter"},
        {GLFW_KEY_TAB, "Tab"}, {GLFW_KEY_BACKSPACE, "Backspace"},
        {GLFW_KEY_INSERT, "Insert"}, {GLFW_KEY_DELETE, "Delete"},
        {GLFW_KEY_RIGHT, "Right"}, {GLFW_KEY_LEFT, "Left"},
        {GLFW_KEY_DOWN, "Down"}, {GLFW_KEY_UP, "Up"},
        {GLFW_KEY_PAGE_UP, "PageUp"}, {GLFW_KEY_PAGE_DOWN, "PageDown"},
        {GLFW_KEY_HOME, "Home"}, {GLFW_KEY_END, "End"},
        {GLFW_KEY_CAPS_LOCK, "CapsLock"}, {GLFW_KEY_SCROLL_LOCK, "ScrollLock"},
        {GLFW_KEY_NUM_LOCK, "NumLock"}, {GLFW_KEY_PRINT_SCREEN, "PrintScreen"},
        {GLFW_KEY_PAUSE, "Pause"}, {GLFW_KEY_LEFT_SHIFT, "LeftShift"},
        {GLFW_KEY_LEFT_CONTROL, "LeftCtrl"}, {GLFW_KEY_LEFT_ALT, "LeftAlt"},
        {GLFW_KEY_RIGHT_SHIFT, "RightShift"}, {GLFW_KEY_RIGHT_CONTROL, "RightCtrl"},
        {GLFW_KEY_RIGHT_ALT, "RightAlt"}, {GLFW_KEY_MENU, "Menu"}
    };

    const auto iterator = named.find(key);
    if (iterator != named.end())
        return iterator->second;

    const char* glfwName = glfwGetKeyName(key, 0);
    if (glfwName && *glfwName)
    {
        std::string result(glfwName);
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char character) {
                return static_cast<char>(std::toupper(character));
            });
        return result;
    }

    return {};
}

int InputSystem::mouseButtonFromName(const std::string& name)
{
    const std::string value = lower(trim(name));
    if (value == "left") return GLFW_MOUSE_BUTTON_LEFT;
    if (value == "right") return GLFW_MOUSE_BUTTON_RIGHT;
    if (value == "middle") return GLFW_MOUSE_BUTTON_MIDDLE;
    if (value == "button4" || value == "4") return GLFW_MOUSE_BUTTON_4;
    if (value == "button5" || value == "5") return GLFW_MOUSE_BUTTON_5;
    if (value == "button6" || value == "6") return GLFW_MOUSE_BUTTON_6;
    if (value == "button7" || value == "7") return GLFW_MOUSE_BUTTON_7;
    if (value == "button8" || value == "8") return GLFW_MOUSE_BUTTON_8;
    return -1;
}

std::string InputSystem::mouseButtonName(int button)
{
    switch (button)
    {
    case GLFW_MOUSE_BUTTON_LEFT: return "Left";
    case GLFW_MOUSE_BUTTON_RIGHT: return "Right";
    case GLFW_MOUSE_BUTTON_MIDDLE: return "Middle";
    default: return "Button" + std::to_string(button + 1);
    }
}

int InputSystem::gamepadButtonFromName(const std::string& name)
{
    const std::string value = lower(trim(name));
    const std::unordered_map<std::string, int> buttons = {
        {"a", GLFW_GAMEPAD_BUTTON_A}, {"b", GLFW_GAMEPAD_BUTTON_B},
        {"x", GLFW_GAMEPAD_BUTTON_X}, {"y", GLFW_GAMEPAD_BUTTON_Y},
        {"leftbumper", GLFW_GAMEPAD_BUTTON_LEFT_BUMPER},
        {"rightbumper", GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER},
        {"back", GLFW_GAMEPAD_BUTTON_BACK}, {"start", GLFW_GAMEPAD_BUTTON_START},
        {"guide", GLFW_GAMEPAD_BUTTON_GUIDE},
        {"leftthumb", GLFW_GAMEPAD_BUTTON_LEFT_THUMB},
        {"rightthumb", GLFW_GAMEPAD_BUTTON_RIGHT_THUMB},
        {"dpadup", GLFW_GAMEPAD_BUTTON_DPAD_UP},
        {"dpadright", GLFW_GAMEPAD_BUTTON_DPAD_RIGHT},
        {"dpaddown", GLFW_GAMEPAD_BUTTON_DPAD_DOWN},
        {"dpadleft", GLFW_GAMEPAD_BUTTON_DPAD_LEFT}
    };
    const auto iterator = buttons.find(value);
    return iterator != buttons.end() ? iterator->second : -1;
}

std::string InputSystem::gamepadButtonName(int button)
{
    switch (button)
    {
    case GLFW_GAMEPAD_BUTTON_A: return "A";
    case GLFW_GAMEPAD_BUTTON_B: return "B";
    case GLFW_GAMEPAD_BUTTON_X: return "X";
    case GLFW_GAMEPAD_BUTTON_Y: return "Y";
    case GLFW_GAMEPAD_BUTTON_LEFT_BUMPER: return "LeftBumper";
    case GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER: return "RightBumper";
    case GLFW_GAMEPAD_BUTTON_BACK: return "Back";
    case GLFW_GAMEPAD_BUTTON_START: return "Start";
    case GLFW_GAMEPAD_BUTTON_GUIDE: return "Guide";
    case GLFW_GAMEPAD_BUTTON_LEFT_THUMB: return "LeftThumb";
    case GLFW_GAMEPAD_BUTTON_RIGHT_THUMB: return "RightThumb";
    case GLFW_GAMEPAD_BUTTON_DPAD_UP: return "DPadUp";
    case GLFW_GAMEPAD_BUTTON_DPAD_RIGHT: return "DPadRight";
    case GLFW_GAMEPAD_BUTTON_DPAD_DOWN: return "DPadDown";
    case GLFW_GAMEPAD_BUTTON_DPAD_LEFT: return "DPadLeft";
    default: return "Button" + std::to_string(button);
    }
}

int InputSystem::gamepadAxisFromName(const std::string& name)
{
    const std::string value = lower(trim(name));
    if (value == "leftx") return GLFW_GAMEPAD_AXIS_LEFT_X;
    if (value == "lefty") return GLFW_GAMEPAD_AXIS_LEFT_Y;
    if (value == "rightx") return GLFW_GAMEPAD_AXIS_RIGHT_X;
    if (value == "righty") return GLFW_GAMEPAD_AXIS_RIGHT_Y;
    if (value == "lefttrigger") return GLFW_GAMEPAD_AXIS_LEFT_TRIGGER;
    if (value == "righttrigger") return GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER;
    return -1;
}

std::string InputSystem::gamepadAxisName(int axis)
{
    switch (axis)
    {
    case GLFW_GAMEPAD_AXIS_LEFT_X: return "LeftX";
    case GLFW_GAMEPAD_AXIS_LEFT_Y: return "LeftY";
    case GLFW_GAMEPAD_AXIS_RIGHT_X: return "RightX";
    case GLFW_GAMEPAD_AXIS_RIGHT_Y: return "RightY";
    case GLFW_GAMEPAD_AXIS_LEFT_TRIGGER: return "LeftTrigger";
    case GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER: return "RightTrigger";
    default: return "Axis" + std::to_string(axis);
    }
}

std::vector<std::string> InputSystem::splitBindingList(const std::string& value)
{
    std::vector<std::string> result;
    std::size_t start = 0;
    while (start <= value.size())
    {
        const std::size_t separator = value.find('|', start);
        const std::size_t end = separator == std::string::npos
            ? value.size()
            : separator;
        const std::string part = trim(value.substr(start, end - start));
        if (!part.empty())
            result.push_back(part);
        if (separator == std::string::npos)
            break;
        start = separator + 1;
    }
    return result;
}

std::string InputSystem::trim(const std::string& value)
{
    const auto first = std::find_if_not(value.begin(), value.end(),
        [](unsigned char character) { return std::isspace(character) != 0; });
    const auto last = std::find_if_not(value.rbegin(), value.rend(),
        [](unsigned char character) { return std::isspace(character) != 0; }).base();
    return first < last ? std::string(first, last) : std::string{};
}

std::string InputSystem::lower(const std::string& value)
{
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return result;
}

} // namespace heritage::input

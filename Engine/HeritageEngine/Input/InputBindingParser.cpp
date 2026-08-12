#include "InputSystemInternal.hpp"

namespace heritage::input {
using namespace input_internal;

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

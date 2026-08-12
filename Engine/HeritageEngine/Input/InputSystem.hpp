#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <GLFW/glfw3.h>

#include "WindowsDirectInputBackend.hpp"

struct GLFWwindow;

namespace heritage::input {

struct InputAnalogSettings
{
    bool invert = false;
    float innerDeadzone = 0.0f;
    float outerDeadzone = 0.0f;
    float sensitivity = 1.0f;
    float bezierX1 = 0.0f;
    float bezierY1 = 0.0f;
    float bezierX2 = 1.0f;
    float bezierY2 = 1.0f;
};

struct InputBindingInfo
{
    std::string binding;
    std::string displayName;
    float rawValue = 0.0f;
    float value = 0.0f;
    bool active = false;
    bool analog = false;
    InputAnalogSettings analogSettings;
};

struct InputActionInfo
{
    std::string name;
    std::string group;
    std::vector<InputBindingInfo> bindings;
    std::vector<std::string> defaultBindings;
    float value = 0.0f;
    bool down = false;
    bool pressed = false;
    bool released = false;
};

struct InputDeviceInfo
{
    int ordinal = -1;
    std::string name;
    std::string guid;
};

struct InputProfileInfo
{
    std::string name;
    std::filesystem::path path;
};

// Native input service used by Heritage Engine and module Lua scripts.
//
// Step 26F adds:
// - eight positional binding slots per action
// - simultaneous keyboard, mouse and all GLFW-standard gamepads
// - optional device-specific gamepad bindings using stable GLFW GUIDs
// - hot-plug-safe binding persistence
// - append, replace and remove capture operations for the in-game UI
//
// Step 26G adds per-binding analogue processing and cubic Bezier curves.
// Step 26I-B adds explicit named profile snapshots. Live settings continue
// to autosave, while profiles change only through deliberate profile actions.
class InputSystem
{
public:
    static constexpr std::size_t kMaxBindingsPerAction = 8;

    bool initialize(
        GLFWwindow* window,
        const std::filesystem::path& settingsPath,
        std::string& message);
    void shutdown();

    // Call once after glfwPollEvents().
    void update();

    bool isAvailable() const { return m_window != nullptr; }

    // Loads module-owned declarations from Data/InputActions.ini.
    // Section headers define module-owned action groups:
    // [Car]
    // Throttle = Key:W | Gamepad:RightTrigger+
    // Empty sections are retained so modules may reserve future tabs.
    bool loadActionDefinitions(
        const std::filesystem::path& definitionsPath,
        std::string& message);

    // Backwards-compatible registration. defaultBinding may contain one or
    // more bindings separated by '|'. Existing actions merge new defaults.
    bool registerAction(
        const std::string& actionName,
        const std::string& defaultBinding,
        const std::string& group = "Common");

    // Backwards-compatible primary-binding setter. It replaces binding 0,
    // or appends the first binding when the action currently has none.
    bool setBinding(
        const std::string& actionName,
        const std::string& binding);

    bool setBinding(
        const std::string& actionName,
        std::size_t bindingIndex,
        const std::string& binding);
    bool addBinding(
        const std::string& actionName,
        const std::string& binding);
    // Clears only the requested positional slot. Later slots do not shift.
    bool removeBinding(
        const std::string& actionName,
        std::size_t bindingIndex);
    bool resetBindings(const std::string& actionName);
    bool resetBinding(const std::string& actionName)
    {
        return resetBindings(actionName);
    }

    bool actionDown(const std::string& actionName) const;
    bool actionPressed(const std::string& actionName) const;
    bool actionReleased(const std::string& actionName) const;
    float actionValue(const std::string& actionName) const;

    // Returns all bindings as a human-readable summary for old Lua scripts.
    std::string actionBinding(const std::string& actionName) const;
    std::string actionBinding(
        const std::string& actionName,
        std::size_t bindingIndex) const;
    std::size_t actionBindingCount(const std::string& actionName) const;
    std::vector<std::string> actionBindings(const std::string& actionName) const;

    bool bindingSupportsAnalog(
        const std::string& actionName,
        std::size_t bindingIndex) const;
    InputAnalogSettings bindingAnalogSettings(
        const std::string& actionName,
        std::size_t bindingIndex) const;
    bool setBindingAnalogSettings(
        const std::string& actionName,
        std::size_t bindingIndex,
        const InputAnalogSettings& settings,
        bool persist = true);
    bool resetBindingAnalogSettings(
        const std::string& actionName,
        std::size_t bindingIndex);
    float bindingRawValue(
        const std::string& actionName,
        std::size_t bindingIndex) const;
    float bindingValue(
        const std::string& actionName,
        std::size_t bindingIndex) const;

    static float evaluateBezier(
        float input,
        float x1,
        float y1,
        float x2,
        float y2);
    static float applyAnalogProcessing(
        float rawValue,
        const InputAnalogSettings& settings);

    bool keyDown(const std::string& keyName) const;
    bool keyPressed(const std::string& keyName) const;
    bool keyReleased(const std::string& keyName) const;

    bool mouseDown(const std::string& buttonName) const;
    bool mousePressed(const std::string& buttonName) const;
    bool mouseReleased(const std::string& buttonName) const;

    double mouseDeltaX() const { return m_mouseDeltaX; }
    double mouseDeltaY() const { return m_mouseDeltaY; }

    int connectedGamepadCount() const;
    bool gamepadConnected(int ordinal = 0) const;
    std::string gamepadName(int ordinal = 0) const;
    std::string gamepadGuid(int ordinal = 0) const;
    std::vector<InputDeviceInfo> gamepads() const;
    bool directInputAvailable() const { return m_directInput.available(); }
    std::vector<WindowsDirectInputBackend::DeviceInfo> directInputDevices() const;
    void refreshInputDevices();

    std::vector<InputActionInfo> actions() const;
    std::vector<std::string> actionGroups() const { return m_actionGroups; }
    std::size_t actionCount() const { return m_actions.size(); }

    // Replace an existing row.
    bool beginBindingCapture(
        const std::string& actionName,
        std::size_t bindingIndex);

    // Backwards-compatible primary-row capture.
    bool beginBindingCapture(const std::string& actionName);

    // Captures into the first empty positional slot. Kept for Lua and
    // compatibility; the settings UI can target any empty slot directly.
    bool beginAddBindingCapture(const std::string& actionName);

    void cancelBindingCapture();
    bool isCapturingBinding() const { return !m_captureAction.empty(); }
    const std::string& captureAction() const { return m_captureAction; }
    bool captureAddsBinding() const { return m_captureAppend; }
    std::size_t captureBindingIndex() const { return m_captureBindingIndex; }

    bool save();

    // Named profiles are explicit snapshots of every action's eight binding
    // slots and all per-binding analogue processing values.
    std::filesystem::path profilesDirectory() const;
    std::vector<InputProfileInfo> profiles() const;
    bool createProfile(const std::string& profileName);
    bool updateProfile(const std::string& profileName);
    bool applyProfile(const std::string& profileName);
    bool duplicateProfile(
        const std::string& sourceProfileName,
        const std::string& newProfileName);
    bool renameProfile(
        const std::string& oldProfileName,
        const std::string& newProfileName);
    bool deleteProfile(const std::string& profileName);
    bool profileExists(const std::string& profileName) const;
    const std::string& lastAppliedProfile() const { return m_lastAppliedProfile; }
    bool profileDirty() const { return m_profileDirty; }

    const std::string& lastError() const { return m_lastError; }
    const std::filesystem::path& settingsPath() const { return m_settingsPath; }

private:
    enum class BindingType
    {
        None,
        Key,
        MouseButton,
        GamepadButton,
        GamepadAxisPositive,
        GamepadAxisNegative,
        DirectInput
    };

    enum class GamepadSelector
    {
        Any,
        Ordinal,
        Guid
    };

    struct ParsedBinding
    {
        BindingType type = BindingType::None;
        int code = -1;
        GamepadSelector gamepadSelector = GamepadSelector::Any;
        int gamepadOrdinal = -1;
        std::string gamepadGuid;
        std::string directInputGuid;
        WindowsDirectInputBackend::ControlType directInputControl =
            WindowsDirectInputBackend::ControlType::Button;
        std::string canonical;
    };

    struct ActionRecord
    {
        std::string group = "Common";
        std::vector<std::string> defaultBindings;
        std::vector<std::string> bindings;
        std::vector<ParsedBinding> parsedBindings;
        std::vector<InputAnalogSettings> analogSettings;
        bool hasUserBindings = false;
        float value = 0.0f;
        bool down = false;
        bool pressed = false;
        bool released = false;
    };

    struct LoadedAnalogSettings
    {
        std::string binding;
        InputAnalogSettings settings;
    };

    struct ProfileActionSnapshot
    {
        std::vector<std::string> bindings;
        std::map<std::size_t, LoadedAnalogSettings> analogSettings;
    };

    struct ProfileSnapshot
    {
        std::string name;
        std::map<std::string, ProfileActionSnapshot> actions;
    };

    struct GamepadSnapshot
    {
        int joystickId = -1;
        bool available = false;
        bool previousAvailable = false;
        std::string name;
        std::string guid;
        GLFWgamepadstate state{};
        GLFWgamepadstate previousState{};
    };

    bool load();
    void rememberActionGroup(const std::string& group);

    bool validateProfileName(
        const std::string& profileName,
        std::string& cleanName,
        std::string& errorMessage) const;
    std::filesystem::path profilePathForName(
        const std::string& cleanName) const;
    std::filesystem::path findProfilePath(
        const std::string& profileName) const;
    ProfileSnapshot captureProfileSnapshot(
        const std::string& profileName) const;
    bool writeProfileSnapshot(
        const std::filesystem::path& path,
        const ProfileSnapshot& snapshot);
    bool readProfileSnapshot(
        const std::filesystem::path& path,
        ProfileSnapshot& snapshot,
        std::string& errorMessage) const;
    bool applyProfileSnapshot(const ProfileSnapshot& snapshot);
    void markProfileDirty();

    void updateHardwareState();
    void updateActions();
    void updateBindingCapture();

    float evaluateBindingRaw(const ParsedBinding& binding) const;
    float evaluateBinding(
        const ParsedBinding& binding,
        const InputAnalogSettings& settings) const;
    float evaluateGamepadBinding(
        const ParsedBinding& binding,
        const GamepadSnapshot& gamepad) const;
    std::vector<const GamepadSnapshot*> matchingGamepads(
        const ParsedBinding& binding) const;

    bool parseBinding(
        const std::string& text,
        ParsedBinding& result,
        std::string& errorMessage) const;
    bool parseBindingList(
        const std::string& text,
        std::vector<ParsedBinding>& parsed,
        std::vector<std::string>& canonical,
        std::string& errorMessage) const;
    bool applyCapturedBinding(const std::string& binding);
    std::string bindingDisplayName(const ParsedBinding& binding) const;
    std::string specificGamepadPrefix(const GamepadSnapshot& gamepad) const;
    bool parsedBindingSupportsAnalog(const ParsedBinding& binding) const;
    InputAnalogSettings defaultAnalogSettings(const ParsedBinding& binding) const;
    InputAnalogSettings sanitizeAnalogSettings(
        const InputAnalogSettings& settings) const;
    void initializeAnalogSettings(
        const std::string& actionName,
        ActionRecord& record);
    bool analogSettingsAreDefault(
        const ParsedBinding& binding,
        const InputAnalogSettings& settings) const;

    const GamepadSnapshot* gamepadForOrdinal(int ordinal) const;
    int gamepadOrdinalForJoystickId(int joystickId) const;
    bool readGamepadState(int joystickId, GLFWgamepadstate& state) const;

    static int keyCodeFromName(const std::string& name);
    static std::string keyNameFromCode(int key);
    static int mouseButtonFromName(const std::string& name);
    static std::string mouseButtonName(int button);
    static int gamepadButtonFromName(const std::string& name);
    static std::string gamepadButtonName(int button);
    static int gamepadAxisFromName(const std::string& name);
    static std::string gamepadAxisName(int axis);
    static std::vector<std::string> splitBindingList(const std::string& value);
    static std::string trim(const std::string& value);
    static std::string lower(const std::string& value);

    GLFWwindow* m_window = nullptr;
    std::filesystem::path m_settingsPath;

    std::array<unsigned char, GLFW_KEY_LAST + 1> m_keys{};
    std::array<unsigned char, GLFW_KEY_LAST + 1> m_previousKeys{};
    std::array<unsigned char, GLFW_MOUSE_BUTTON_LAST + 1> m_mouseButtons{};
    std::array<unsigned char, GLFW_MOUSE_BUTTON_LAST + 1> m_previousMouseButtons{};

    double m_mouseX = 0.0;
    double m_mouseY = 0.0;
    double m_previousMouseX = 0.0;
    double m_previousMouseY = 0.0;
    double m_mouseDeltaX = 0.0;
    double m_mouseDeltaY = 0.0;
    bool m_firstMouseUpdate = true;

    std::array<GamepadSnapshot, GLFW_JOYSTICK_LAST + 1> m_gamepads{};
    WindowsDirectInputBackend m_directInput;

    std::map<std::string, ActionRecord> m_actions;
    std::vector<std::string> m_actionGroups;
    std::unordered_map<std::string, std::vector<std::string>> m_loadedBindings;
    std::unordered_set<std::string> m_loadedBindingOverrides;
    std::unordered_set<std::string> m_loadedLegacyActions;
    std::unordered_map<std::string, std::map<std::size_t, LoadedAnalogSettings>>
        m_loadedAnalogSettings;

    std::string m_captureAction;
    std::size_t m_captureBindingIndex = 0;
    bool m_captureAppend = false;

    std::string m_lastAppliedProfile;
    bool m_profileDirty = false;
    std::string m_lastError;
};

} // namespace heritage::input

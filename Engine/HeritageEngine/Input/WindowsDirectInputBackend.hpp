#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;

namespace heritage::input {

// A Windows-only DirectInput 8 backend for racing peripherals and legacy
// controllers that are not exposed through GLFW's standard gamepad mapping.
// The non-Windows implementation is a harmless stub so the rest of the input
// system remains portable.
class WindowsDirectInputBackend
{
public:
    static constexpr std::size_t kAxisCount = 8;
    static constexpr std::size_t kButtonCount = 128;
    static constexpr std::size_t kPovCount = 4;

    enum class ControlType
    {
        Button,
        AxisPositive,
        AxisNegative,
        PovUp,
        PovUpRight,
        PovRight,
        PovDownRight,
        PovDown,
        PovDownLeft,
        PovLeft,
        PovUpLeft
    };

    struct DeviceInfo
    {
        std::string name;
        std::string instanceGuid;
        std::string productGuid;
        int axisCount = 0;
        int buttonCount = 0;
        int povCount = 0;
        bool connected = false;
    };

    WindowsDirectInputBackend();
    ~WindowsDirectInputBackend();

    WindowsDirectInputBackend(const WindowsDirectInputBackend&) = delete;
    WindowsDirectInputBackend& operator=(const WindowsDirectInputBackend&) = delete;

    bool initialize(GLFWwindow* window, std::string& message);
    void shutdown();
    void update();

    bool available() const;
    std::vector<DeviceInfo> devices() const;
    std::string deviceName(const std::string& instanceGuid) const;

    float value(
        const std::string& instanceGuid,
        ControlType type,
        int controlIndex) const;

    // Returns the first newly activated DirectInput control in canonical
    // Heritage Engine binding syntax, for example:
    // DInput[guid]:AxisX- or DInput[guid]:Button3.
    bool captureBinding(std::string& binding) const;

    static std::string axisName(int axisIndex);
    static int axisIndexFromName(const std::string& name);
    static std::string povDirectionName(ControlType type);
    static bool povDirectionFromName(
        const std::string& name,
        ControlType& type);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace heritage::input

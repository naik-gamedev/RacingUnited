#pragma once

#include <cstddef>
#include <functional>

struct GLFWwindow;

#include "../../Graphics/DisplaySystem.hpp"
#include "../../Graphics/WindowSystem.hpp"
#include "../../Core/Settings/VideoSettings.hpp"

namespace heritage::engine {

class DisplayModeController final
{
public:
    DisplayModeController(
        heritage::graphics::DisplaySystem& display,
        heritage::graphics::WindowSystem& windowSystem,
        heritage::settings::VideoSettings& videoSettings);

    void applyLoadedMode(GLFWwindow* window);
    void enforceSpanCompatibility();
    void syncVideoSettings(GLFWwindow* window);

    void initiateChange(
        GLFWwindow* window,
        heritage::graphics::WindowMode newMode,
        int desiredWidth,
        int desiredHeight,
        int desiredRefresh);

    void restorePendingChange(GLFWwindow* window);
    bool changePending() const { return m_changePending; }

    void drawChangeConfirmationPopup(
        GLFWwindow* window,
        int framebufferWidth,
        int framebufferHeight,
        const std::function<void()>& saveSettings);

private:
    const heritage::graphics::MonitorInfo* primaryMonitorInfo() const;
    void setWindowModeIndex(heritage::graphics::WindowMode mode);
    static heritage::graphics::WindowMode windowModeFromIndex(int index);
    static bool monitorSupportsMode(
        const heritage::graphics::MonitorInfo& monitor,
        int width,
        int height,
        int refreshRate);

    heritage::graphics::DisplaySystem& m_display;
    heritage::graphics::WindowSystem& m_windowSystem;
    heritage::settings::VideoSettings& m_videoSettings;

    bool m_changePending = false;
    double m_changeDeadline = 0.0;
    bool m_popupOpened = false;
    static constexpr double kChangeTimeoutSeconds = 15.0;

    heritage::graphics::WindowMode m_previousMode =
        heritage::graphics::WindowMode::Windowed;
    int m_previousWidth = 0;
    int m_previousHeight = 0;
    int m_previousRefresh = 0;
    int m_previousSettingWidth = 0;
    int m_previousSettingHeight = 0;
    int m_previousSettingRefresh = 0;

    int m_desiredWidth = 0;
    int m_desiredHeight = 0;
    int m_desiredRefresh = 0;
};

} // namespace heritage::engine

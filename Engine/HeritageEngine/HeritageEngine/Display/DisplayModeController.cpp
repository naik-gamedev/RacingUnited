#include "DisplayModeController.hpp"

#include <algorithm>

#include <GLFW/glfw3.h>
#include <imgui.h>

namespace heritage::engine {

using heritage::graphics::WindowMode;

DisplayModeController::DisplayModeController(
    heritage::graphics::DisplaySystem& display,
    heritage::graphics::WindowSystem& windowSystem,
    heritage::settings::VideoSettings& videoSettings)
    : m_display(display)
    , m_windowSystem(windowSystem)
    , m_videoSettings(videoSettings)
{
}

const heritage::graphics::MonitorInfo* DisplayModeController::primaryMonitorInfo() const
{
    const auto& monitors = m_display.monitors();
    if (monitors.empty())
        return nullptr;

    const std::size_t primaryIndex = m_display.primaryMonitorIndex();
    if (primaryIndex >= monitors.size())
        return &monitors.front();

    return &monitors[primaryIndex];
}

void DisplayModeController::setWindowModeIndex(WindowMode mode)
{
    if (mode == WindowMode::Windowed)
        m_videoSettings.windowModeIndex = 0;
    else if (mode == WindowMode::Borderless)
        m_videoSettings.windowModeIndex = 1;
    else
        m_videoSettings.windowModeIndex = 2;
}

void DisplayModeController::enforceSpanCompatibility()
{
    if (m_windowSystem.mode() == WindowMode::Windowed)
        m_display.spanAllMonitors = false;
}

void DisplayModeController::syncVideoSettings(GLFWwindow* window)
{
    setWindowModeIndex(m_windowSystem.mode());

    if (m_windowSystem.mode() == WindowMode::Windowed)
    {
        glfwGetWindowSize(
            window,
            &m_videoSettings.resolutionWidth,
            &m_videoSettings.resolutionHeight);
    }
    else if (const auto* monitor = primaryMonitorInfo())
    {
        const GLFWvidmode* mode = glfwGetVideoMode(monitor->handle);
        if (mode)
        {
            m_videoSettings.resolutionWidth = mode->width;
            m_videoSettings.resolutionHeight = mode->height;
            m_videoSettings.refreshRate = mode->refreshRate;
        }
    }

    if (m_videoSettings.refreshRate <= 0)
    {
        if (const auto* monitor = primaryMonitorInfo())
            m_videoSettings.refreshRate = monitor->refreshRate;
    }
}

WindowMode DisplayModeController::windowModeFromIndex(int index)
{
    if (index == 1)
        return WindowMode::Borderless;
    if (index == 2)
        return WindowMode::Exclusive;
    return WindowMode::Windowed;
}

bool DisplayModeController::monitorSupportsMode(
    const heritage::graphics::MonitorInfo& monitor,
    int width,
    int height,
    int refreshRate)
{
    return std::any_of(
        monitor.modes.begin(),
        monitor.modes.end(),
        [&](const heritage::graphics::MonitorMode& mode)
        {
            return mode.w == width
                && mode.h == height
                && mode.refresh == refreshRate;
        });
}

void DisplayModeController::applyLoadedMode(GLFWwindow* window)
{
    const auto* monitor = primaryMonitorInfo();
    if (!monitor)
    {
        setWindowModeIndex(WindowMode::Windowed);
        m_display.spanAllMonitors = false;
        syncVideoSettings(window);
        return;
    }

    WindowMode requestedMode = windowModeFromIndex(m_videoSettings.windowModeIndex);

    if (requestedMode == WindowMode::Borderless)
    {
        m_windowSystem.setMode(
            window,
            WindowMode::Borderless,
            0,
            0,
            GLFW_DONT_CARE,
            monitor->handle);
    }
    else if (requestedMode == WindowMode::Exclusive)
    {
        int width = m_videoSettings.resolutionWidth;
        int height = m_videoSettings.resolutionHeight;
        int refresh = m_videoSettings.refreshRate;

        if (!monitorSupportsMode(*monitor, width, height, refresh))
        {
            width = monitor->width;
            height = monitor->height;
            refresh = monitor->refreshRate;
        }

        m_windowSystem.setMode(
            window,
            WindowMode::Exclusive,
            width,
            height,
            refresh,
            monitor->handle);
    }
    else
    {
        setWindowModeIndex(WindowMode::Windowed);
    }

    enforceSpanCompatibility();
    syncVideoSettings(window);
}

void DisplayModeController::restorePendingChange(GLFWwindow* window)
{
    GLFWmonitor* targetMonitor = nullptr;
    if (const auto* monitor = primaryMonitorInfo())
        targetMonitor = monitor->handle;

    m_windowSystem.setMode(
        window,
        m_previousMode,
        m_previousWidth,
        m_previousHeight,
        m_previousRefresh,
        targetMonitor);
    enforceSpanCompatibility();

    m_videoSettings.resolutionWidth = m_previousSettingWidth;
    m_videoSettings.resolutionHeight = m_previousSettingHeight;
    m_videoSettings.refreshRate = m_previousSettingRefresh;
    setWindowModeIndex(m_previousMode);
    m_changePending = false;
}

void DisplayModeController::initiateChange(
    GLFWwindow* window,
    WindowMode newMode,
    int desiredWidth,
    int desiredHeight,
    int desiredRefresh)
{
    m_previousMode = m_windowSystem.mode();
    glfwGetWindowSize(window, &m_previousWidth, &m_previousHeight);

    m_previousSettingWidth = m_videoSettings.resolutionWidth;
    m_previousSettingHeight = m_videoSettings.resolutionHeight;
    m_previousSettingRefresh = m_videoSettings.refreshRate;

    m_previousRefresh = m_videoSettings.refreshRate;
    if (m_previousMode != WindowMode::Windowed)
    {
        if (const auto* monitor = primaryMonitorInfo())
        {
            if (const GLFWvidmode* currentMode = glfwGetVideoMode(monitor->handle))
                m_previousRefresh = currentMode->refreshRate;
        }
    }

    m_desiredWidth = desiredWidth;
    m_desiredHeight = desiredHeight;
    m_desiredRefresh = desiredRefresh;

    GLFWmonitor* targetMonitor = nullptr;
    if (const auto* monitor = primaryMonitorInfo())
        targetMonitor = monitor->handle;

    m_windowSystem.setMode(
        window,
        newMode,
        desiredWidth,
        desiredHeight,
        desiredRefresh,
        targetMonitor);
    enforceSpanCompatibility();

    setWindowModeIndex(newMode);

    if (newMode != WindowMode::Borderless)
    {
        if (desiredWidth > 0)
            m_videoSettings.resolutionWidth = desiredWidth;
        if (desiredHeight > 0)
            m_videoSettings.resolutionHeight = desiredHeight;
        if (desiredRefresh > 0)
            m_videoSettings.refreshRate = desiredRefresh;
    }

    m_changePending = true;
    m_changeDeadline = glfwGetTime() + kChangeTimeoutSeconds;
    m_popupOpened = true;
}

void DisplayModeController::drawChangeConfirmationPopup(
    GLFWwindow* window,
    int framebufferWidth,
    int framebufferHeight,
    const std::function<void()>& saveSettings)
{
    if (!m_changePending)
        return;

    if (m_popupOpened)
    {
        if (ImGuiViewport* viewport = ImGui::GetMainViewport())
            ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup("Display Change");
        m_popupOpened = false;
    }

    const double now = glfwGetTime();
    if (now >= m_changeDeadline)
        restorePendingChange(window);

    if (m_changePending && ImGui::BeginPopupModal(
            "Display Change",
            nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        const char* modeString =
            (m_windowSystem.mode() == WindowMode::Windowed) ? "Windowed" :
            (m_windowSystem.mode() == WindowMode::Borderless) ? "Borderless" : "Exclusive";
        ImGui::Text("Switched to %s mode.", modeString);
        ImGui::Spacing();
        ImGui::Text(
            "Resolution: %d x %d",
            m_desiredWidth > 0 ? m_desiredWidth : framebufferWidth,
            m_desiredHeight > 0 ? m_desiredHeight : framebufferHeight);
        if (m_desiredRefresh > 0)
            ImGui::Text("Refresh rate: %d Hz", m_desiredRefresh);
        ImGui::Spacing();
        double remaining = m_changeDeadline - now;
        if (remaining < 0.0)
            remaining = 0.0;
        ImGui::Text("Keep these display settings? Reverting in %.0f seconds...", remaining);
        ImGui::Spacing();
        if (ImGui::Button("Keep", ImVec2(120, 0)))
        {
            m_changePending = false;
            saveSettings();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Revert", ImVec2(120, 0)))
        {
            restorePendingChange(window);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace heritage::engine

#include "VideoSettingsPage.hpp"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>
#include <utility>

#include <imgui.h>

#include "../../Graphics/AntiAliasing.hpp"

namespace heritage::ui::settings {
namespace {

struct ResolutionOption
{
    int width = 0;
    int height = 0;
    std::string label;
};

const char* kTextureFilterOptions[] = {
    "Nearest",
    "Bilinear",
    "Trilinear",
    "Anisotropic x2",
    "Anisotropic x4",
    "Anisotropic x8",
    "Anisotropic x16"
};

const char* kScaleOptions[] = {
    "Native",
    "Integer x1",
    "Integer x2",
    "Integer x3",
    "Half (50%)",
    "Quarter (25%)"
};

const char* kVSyncOptions[] = { "Off", "On" };

const char* kApiOptions[] = { "OpenGL" };
const char* kWindowModeOptions[] = { "Windowed", "Borderless", "Exclusive" };

const heritage::graphics::MonitorInfo* getPrimaryMonitor(
    const heritage::graphics::DisplaySystem& display)
{
    const auto& monitors = display.monitors();
    if (monitors.empty())
        return nullptr;

    const std::size_t primaryIndex = display.primaryMonitorIndex();
    if (primaryIndex >= monitors.size())
        return &monitors.front();

    return &monitors[primaryIndex];
}

bool isResolutionAvailableForWindowMode(
    const heritage::graphics::MonitorInfo& monitor,
    heritage::graphics::WindowMode windowMode,
    int width,
    int height)
{
    if (windowMode != heritage::graphics::WindowMode::Windowed)
        return true;

    // A borderless, undecorated window matching the entire desktop can be
    // treated like fullscreen by Windows. Keep desktop-sized modes out of the
    // Windowed list rather than relying on a one-pixel workaround.
    const bool fillsDesktop = width >= monitor.width && height >= monitor.height;

    const bool exceedsWorkArea = monitor.workWidth > 0
        && monitor.workHeight > 0
        && (width > monitor.workWidth || height > monitor.workHeight);

    return !fillsDesktop && !exceedsWorkArea;
}

std::vector<ResolutionOption> buildResolutionOptions(
    const heritage::graphics::MonitorInfo& monitor,
    heritage::graphics::WindowMode windowMode)
{
    std::vector<ResolutionOption> resolutions;

    for (const auto& mode : monitor.modes)
    {
        if (!isResolutionAvailableForWindowMode(
                monitor,
                windowMode,
                mode.w,
                mode.h))
        {
            continue;
        }
        const bool alreadyAdded = std::any_of(
            resolutions.begin(),
            resolutions.end(),
            [&](const ResolutionOption& resolution)
            {
                return resolution.width == mode.w && resolution.height == mode.h;
            });

        if (alreadyAdded)
            continue;

        ResolutionOption resolution;
        resolution.width = mode.w;
        resolution.height = mode.h;
        resolution.label = std::to_string(mode.w) + " x " + std::to_string(mode.h);
        resolutions.push_back(std::move(resolution));
    }

    std::sort(
        resolutions.begin(),
        resolutions.end(),
        [](const ResolutionOption& a, const ResolutionOption& b)
        {
            if (a.width != b.width)
                return a.width > b.width;
            return a.height > b.height;
        });

    return resolutions;
}

std::vector<int> buildRefreshRateOptions(
    const heritage::graphics::MonitorInfo& monitor,
    int width,
    int height)
{
    std::vector<int> refreshRates;

    for (const auto& mode : monitor.modes)
    {
        if (mode.w != width || mode.h != height)
            continue;

        if (std::find(refreshRates.begin(), refreshRates.end(), mode.refresh) == refreshRates.end())
            refreshRates.push_back(mode.refresh);
    }

    std::sort(refreshRates.begin(), refreshRates.end(), std::greater<int>());
    return refreshRates;
}

void ensureValidSelectedMode(
    heritage::settings::VideoSettings& videoSettings,
    const heritage::graphics::MonitorInfo& monitor,
    heritage::graphics::WindowMode windowMode)
{
    const std::vector<ResolutionOption> resolutions = buildResolutionOptions(
        monitor,
        windowMode);
    if (resolutions.empty())
    {
        videoSettings.resolutionWidth = monitor.width;
        videoSettings.resolutionHeight = monitor.height;
        videoSettings.refreshRate = monitor.refreshRate;
        return;
    }

    const bool resolutionIsValid = std::any_of(
        resolutions.begin(),
        resolutions.end(),
        [&](const ResolutionOption& resolution)
        {
            return resolution.width == videoSettings.resolutionWidth
                && resolution.height == videoSettings.resolutionHeight;
        });

    if (!resolutionIsValid)
    {
        auto currentResolution = std::find_if(
            resolutions.begin(),
            resolutions.end(),
            [&](const ResolutionOption& resolution)
            {
                return resolution.width == monitor.width
                    && resolution.height == monitor.height;
            });

        if (currentResolution == resolutions.end())
            currentResolution = resolutions.begin();

        videoSettings.resolutionWidth = currentResolution->width;
        videoSettings.resolutionHeight = currentResolution->height;
    }

    const std::vector<int> refreshRates = buildRefreshRateOptions(
        monitor,
        videoSettings.resolutionWidth,
        videoSettings.resolutionHeight);

    if (refreshRates.empty())
    {
        videoSettings.refreshRate = monitor.refreshRate;
        return;
    }

    if (std::find(refreshRates.begin(), refreshRates.end(), videoSettings.refreshRate) == refreshRates.end())
    {
        if (std::find(refreshRates.begin(), refreshRates.end(), monitor.refreshRate) != refreshRates.end())
            videoSettings.refreshRate = monitor.refreshRate;
        else
            videoSettings.refreshRate = refreshRates.front();
    }
}

} // namespace

void drawVideoSettingsPage(
    GLFWwindow* window,
    heritage::graphics::DisplaySystem& display,
    heritage::graphics::WindowSystem& windowSystem,
    heritage::settings::VideoSettings& videoSettings,
    int desktopWidth,
    int desktopHeight,
    const DisplayChangeHandler& initiateDisplayChange)
{
    using heritage::graphics::WindowMode;

    ImGui::Spacing();

    const WindowMode currentWindowMode = windowSystem.mode();
    const heritage::graphics::MonitorInfo* primaryMonitor = getPrimaryMonitor(display);
    if (primaryMonitor)
        ensureValidSelectedMode(videoSettings, *primaryMonitor, currentWindowMode);
    else
    {
        if (videoSettings.resolutionWidth <= 0)
            videoSettings.resolutionWidth = desktopWidth;
        if (videoSettings.resolutionHeight <= 0)
            videoSettings.resolutionHeight = desktopHeight;
    }

    // Core display mode controls.
    ImGui::SetNextItemWidth(220);
    ImGui::Combo(
        "Render API",
        &videoSettings.renderApiIndex,
        kApiOptions,
        IM_ARRAYSIZE(kApiOptions));

    int requestedWindowModeIndex = videoSettings.windowModeIndex;
    ImGui::SetNextItemWidth(220);
    if (ImGui::Combo(
            "Window Mode",
            &requestedWindowModeIndex,
            kWindowModeOptions,
            IM_ARRAYSIZE(kWindowModeOptions)))
    {
        WindowMode newMode = WindowMode::Windowed;
        if (requestedWindowModeIndex == 1)
            newMode = WindowMode::Borderless;
        else if (requestedWindowModeIndex == 2)
            newMode = WindowMode::Exclusive;

        if (newMode != windowSystem.mode())
        {
            if (windowSystem.mode() == WindowMode::Windowed)
                windowSystem.saveCurrentRect(window);

            int desiredWidth = videoSettings.resolutionWidth;
            int desiredHeight = videoSettings.resolutionHeight;
            int desiredRefresh = videoSettings.refreshRate;

            if (newMode == WindowMode::Borderless && primaryMonitor)
            {
                desiredWidth = primaryMonitor->width;
                desiredHeight = primaryMonitor->height;
                desiredRefresh = primaryMonitor->refreshRate;
            }
            else if (newMode == WindowMode::Windowed && primaryMonitor)
            {
                const std::vector<ResolutionOption> windowedResolutions =
                    buildResolutionOptions(*primaryMonitor, WindowMode::Windowed);

                const auto selectedWindowedResolution = std::find_if(
                    windowedResolutions.begin(),
                    windowedResolutions.end(),
                    [&](const ResolutionOption& resolution)
                    {
                        return resolution.width == desiredWidth
                            && resolution.height == desiredHeight;
                    });

                if (selectedWindowedResolution == windowedResolutions.end()
                    && !windowedResolutions.empty())
                {
                    // The options are sorted largest first, so this chooses the
                    // largest driver-reported mode that is safe for Windowed use.
                    desiredWidth = windowedResolutions.front().width;
                    desiredHeight = windowedResolutions.front().height;

                    const std::vector<int> validRefreshRates = buildRefreshRateOptions(
                        *primaryMonitor,
                        desiredWidth,
                        desiredHeight);

                    if (!validRefreshRates.empty()
                        && std::find(
                            validRefreshRates.begin(),
                            validRefreshRates.end(),
                            desiredRefresh) == validRefreshRates.end())
                    {
                        desiredRefresh = validRefreshRates.front();
                    }
                }
            }

            initiateDisplayChange(
                newMode,
                desiredWidth,
                desiredHeight,
                desiredRefresh);
        }
    }

    const bool displayModeLocked = display.spanAllMonitors
        || currentWindowMode == WindowMode::Borderless
        || primaryMonitor == nullptr;

    if (displayModeLocked)
        ImGui::BeginDisabled();

    const std::vector<ResolutionOption> resolutions = primaryMonitor
        ? buildResolutionOptions(*primaryMonitor, currentWindowMode)
        : std::vector<ResolutionOption>{};

    const int previewWidth = (currentWindowMode == WindowMode::Borderless && primaryMonitor)
        ? primaryMonitor->width
        : videoSettings.resolutionWidth;
    const int previewHeight = (currentWindowMode == WindowMode::Borderless && primaryMonitor)
        ? primaryMonitor->height
        : videoSettings.resolutionHeight;
    const int previewRefresh = (currentWindowMode == WindowMode::Borderless && primaryMonitor)
        ? primaryMonitor->refreshRate
        : videoSettings.refreshRate;

    const std::string resolutionPreview = std::to_string(previewWidth)
        + " x " + std::to_string(previewHeight);

    ImGui::SetNextItemWidth(220);
    if (ImGui::BeginCombo("Resolution", resolutionPreview.c_str()))
    {
        for (const ResolutionOption& resolution : resolutions)
        {
            const bool selected = resolution.width == videoSettings.resolutionWidth
                && resolution.height == videoSettings.resolutionHeight;

            if (ImGui::Selectable(resolution.label.c_str(), selected) && !selected)
            {
                int newRefreshRate = videoSettings.refreshRate;
                const std::vector<int> validRefreshRates = buildRefreshRateOptions(
                    *primaryMonitor,
                    resolution.width,
                    resolution.height);

                if (!validRefreshRates.empty()
                    && std::find(
                        validRefreshRates.begin(),
                        validRefreshRates.end(),
                        newRefreshRate) == validRefreshRates.end())
                {
                    newRefreshRate = validRefreshRates.front();
                }

                initiateDisplayChange(
                    currentWindowMode,
                    resolution.width,
                    resolution.height,
                    newRefreshRate);
            }

            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    const std::vector<int> refreshRates = primaryMonitor
        ? buildRefreshRateOptions(
            *primaryMonitor,
            videoSettings.resolutionWidth,
            videoSettings.resolutionHeight)
        : std::vector<int>{};

    const std::string refreshPreview = std::to_string(previewRefresh) + " Hz";

    ImGui::SetNextItemWidth(220);
    if (ImGui::BeginCombo("Refresh Rate", refreshPreview.c_str()))
    {
        for (const int refreshRate : refreshRates)
        {
            const bool selected = refreshRate == videoSettings.refreshRate;
            const std::string label = std::to_string(refreshRate) + " Hz";

            if (ImGui::Selectable(label.c_str(), selected) && !selected)
            {
                initiateDisplayChange(
                    currentWindowMode,
                    videoSettings.resolutionWidth,
                    videoSettings.resolutionHeight,
                    refreshRate);
            }

            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (displayModeLocked)
        ImGui::EndDisabled();

    if (display.spanAllMonitors)
        ImGui::TextDisabled("Resolution and refresh rate are locked while spanning.");
    else if (currentWindowMode == WindowMode::Borderless)
        ImGui::TextDisabled("Borderless mode uses the desktop resolution and refresh rate.");
    else if (currentWindowMode == WindowMode::Windowed)
        ImGui::TextDisabled("Desktop-sized modes are hidden in Windowed mode.");

    // Presentation timing controls.
    int vsyncIndex = videoSettings.vsyncEnabled ? 1 : 0;
    ImGui::SetNextItemWidth(220);
    if (ImGui::Combo(
            "VSync",
            &vsyncIndex,
            kVSyncOptions,
            IM_ARRAYSIZE(kVSyncOptions)))
    {
        videoSettings.vsyncEnabled = (vsyncIndex == 1);
    }

    ImGui::SetNextItemWidth(220);
    ImGui::Combo(
        "FPS Cap",
        &videoSettings.fpsCapIndex,
        heritage::settings::kFpsCapOptionNames.data(),
        static_cast<int>(heritage::settings::kFpsCapOptionNames.size()));

    if (videoSettings.vsyncEnabled)
        ImGui::TextDisabled("VSync may limit FPS below the selected cap.");

    ImGui::SetNextItemWidth(220);
    ImGui::Combo(
        "Scale Mode",
        &videoSettings.scaleModeIndex,
        kScaleOptions,
        IM_ARRAYSIZE(kScaleOptions));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Multi-monitor setup");
    ImGui::Separator();
    ImGui::Spacing();

    // ===== Multi-monitor / Span =====
    const bool spanAvailable = currentWindowMode != WindowMode::Windowed;
    if (!spanAvailable)
        ImGui::BeginDisabled();

    ImGui::Checkbox("Span all monitors", &display.spanAllMonitors);

    if (!spanAvailable)
        ImGui::EndDisabled();

    if (!spanAvailable)
        ImGui::TextDisabled("Switch to Borderless or Exclusive mode to enable spanning.");

    ImGui::Spacing();

    ImGui::TextDisabled("Monitors:");
    for (std::size_t monitorIndex = 0; monitorIndex < display.monitors().size(); ++monitorIndex)
    {
        ImGui::PushID(static_cast<int>(monitorIndex));

        bool selected = display.selected[monitorIndex];
        if (ImGui::Checkbox(display.monitors()[monitorIndex].name.c_str(), &selected))
            display.selected[monitorIndex] = selected;

        ImGui::SameLine();
        ImGui::TextDisabled(
            "%dx%d",
            display.monitors()[monitorIndex].width,
            display.monitors()[monitorIndex].height);

        ImGui::SetNextItemWidth(120);
        if (ImGui::InputInt("Bezel mm", &display.bezelMm[monitorIndex], 1, 10))
        {
            display.bezelMm[monitorIndex] = std::clamp(display.bezelMm[monitorIndex], 0, 100);
        }

        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::SetNextItemWidth(220);
    ImGui::InputFloat("Eye distance (cm)", &display.eyeDistanceCm, 1.0f, 5.0f, "%.1f");

    ImGui::SetNextItemWidth(220);
    if (ImGui::InputInt("Global Bezel (mm)", &display.globalBezelMm))
    {
        display.globalBezelMm = std::clamp(display.globalBezelMm, 0, 100);
        std::fill(display.bezelMm.begin(), display.bezelMm.end(), display.globalBezelMm);
    }

    if (display.isSpanning())
    {
        const float horizontalFov = display.getCombinedHFOVDegrees();
        ImGui::TextDisabled("Combined HFOV: %.1f°", horizontalFov);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Image adjustments");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::SliderFloat("Gamma", &videoSettings.gamma, 1.00f, 3.00f, "%.2f");
    ImGui::SliderFloat("Brightness", &videoSettings.brightness, -0.50f, 0.50f, "%.2f");
    ImGui::SliderFloat("Contrast", &videoSettings.contrast, 0.50f, 1.50f, "%.2f");
    ImGui::SliderFloat("Saturation", &videoSettings.saturation, 0.00f, 2.00f, "%.2f");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Rendering quality");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::SetNextItemWidth(220);
    ImGui::Combo(
        "Anti-Aliasing",
        &videoSettings.antiAliasingIndex,
        heritage::graphics::antiAliasingOptionNames(),
        heritage::graphics::antiAliasingOptionCount());

    ImGui::SetNextItemWidth(220);
    ImGui::Combo(
        "Texture Filter",
        &videoSettings.textureFilterIndex,
        kTextureFilterOptions,
        IM_ARRAYSIZE(kTextureFilterOptions));
}

} // namespace heritage::ui::settings

#include "LauncherSettingsPage.hpp"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include <imgui.h>
#include <GLFW/glfw3.h>

namespace racing::launcher {
namespace {

struct ResolutionOption
{
    int width = 0;
    int height = 0;
    std::string label;
};

constexpr const char* kApiOptions[] = { "OpenGL" };
constexpr const char* kWindowModeOptions[] = { "Windowed", "Borderless", "Exclusive" };
constexpr const char* kVSyncOptions[] = { "Off", "On" };
constexpr const char* kScaleOptions[] = {
    "Native",
    "Integer x1",
    "Integer x2",
    "Integer x3",
    "Half (50%)",
    "Quarter (25%)"
};
constexpr const char* kTextureFilterOptions[] = {
    "Nearest",
    "Bilinear",
    "Trilinear",
    "Anisotropic x2",
    "Anisotropic x4",
    "Anisotropic x8",
    "Anisotropic x16"
};
constexpr const char* kAntiAliasingOptions[] = {
    "None",
    "MSAA x2",
    "MSAA x4",
    "MSAA x8",
    "FXAA",
    "FXAA + MSAA x2",
    "FXAA + MSAA x4"
};

std::vector<ResolutionOption> buildResolutionOptions(
    GLFWmonitor* monitor,
    int windowModeIndex)
{
    std::vector<ResolutionOption> resolutions;
    if (!monitor)
        return resolutions;

    const GLFWvidmode* desktopMode = glfwGetVideoMode(monitor);
    if (!desktopMode)
        return resolutions;

    int workX = 0;
    int workY = 0;
    int workWidth = desktopMode->width;
    int workHeight = desktopMode->height;
    glfwGetMonitorWorkarea(monitor, &workX, &workY, &workWidth, &workHeight);

    int modeCount = 0;
    const GLFWvidmode* modes = glfwGetVideoModes(monitor, &modeCount);
    for (int index = 0; modes && index < modeCount; ++index)
    {
        const int width = modes[index].width;
        const int height = modes[index].height;

        if (windowModeIndex == 0)
        {
            const bool fillsDesktop = width >= desktopMode->width
                && height >= desktopMode->height;
            const bool exceedsWorkArea = width > workWidth || height > workHeight;
            if (fillsDesktop || exceedsWorkArea)
                continue;
        }

        const bool exists = std::any_of(
            resolutions.begin(),
            resolutions.end(),
            [&](const ResolutionOption& option)
            {
                return option.width == width && option.height == height;
            });
        if (exists)
            continue;

        resolutions.push_back({
            width,
            height,
            std::to_string(width) + " x " + std::to_string(height)
        });
    }

    std::sort(
        resolutions.begin(),
        resolutions.end(),
        [](const ResolutionOption& left, const ResolutionOption& right)
        {
            if (left.width != right.width)
                return left.width > right.width;
            return left.height > right.height;
        });

    return resolutions;
}

std::vector<int> buildRefreshRates(
    GLFWmonitor* monitor,
    int width,
    int height)
{
    std::vector<int> rates;
    if (!monitor)
        return rates;

    int modeCount = 0;
    const GLFWvidmode* modes = glfwGetVideoModes(monitor, &modeCount);
    for (int index = 0; modes && index < modeCount; ++index)
    {
        if (modes[index].width != width || modes[index].height != height)
            continue;

        if (std::find(rates.begin(), rates.end(), modes[index].refreshRate) == rates.end())
            rates.push_back(modes[index].refreshRate);
    }

    std::sort(rates.begin(), rates.end(), std::greater<int>());
    return rates;
}

void ensureValidMode(
    heritage::settings::VideoSettings& settings,
    GLFWmonitor* monitor)
{
    if (!monitor)
        return;

    const GLFWvidmode* desktopMode = glfwGetVideoMode(monitor);
    if (!desktopMode)
        return;

    if (settings.windowModeIndex == 1)
    {
        settings.resolutionWidth = desktopMode->width;
        settings.resolutionHeight = desktopMode->height;
        settings.refreshRate = desktopMode->refreshRate;
        return;
    }

    const std::vector<ResolutionOption> resolutions = buildResolutionOptions(
        monitor,
        settings.windowModeIndex);
    if (resolutions.empty())
        return;

    const bool resolutionValid = std::any_of(
        resolutions.begin(),
        resolutions.end(),
        [&](const ResolutionOption& option)
        {
            return option.width == settings.resolutionWidth
                && option.height == settings.resolutionHeight;
        });

    if (!resolutionValid)
    {
        settings.resolutionWidth = resolutions.front().width;
        settings.resolutionHeight = resolutions.front().height;
    }

    const std::vector<int> rates = buildRefreshRates(
        monitor,
        settings.resolutionWidth,
        settings.resolutionHeight);

    if (!rates.empty()
        && std::find(rates.begin(), rates.end(), settings.refreshRate) == rates.end())
    {
        settings.refreshRate = rates.front();
    }
}

bool drawVideoTab(
    heritage::settings::VideoSettings& settings,
    heritage::graphics::DisplaySystem& display,
    GLFWmonitor* monitor)
{
    bool changed = false;
    ensureValidMode(settings, monitor);

    if (settings.windowModeIndex == 0 && display.spanAllMonitors)
    {
        display.spanAllMonitors = false;
        changed = true;
    }

    ImGui::Spacing();
    ImGui::SetNextItemWidth(220);
    changed |= ImGui::Combo(
        "Render API",
        &settings.renderApiIndex,
        kApiOptions,
        IM_ARRAYSIZE(kApiOptions));

    ImGui::SetNextItemWidth(220);
    if (ImGui::Combo(
            "Window Mode",
            &settings.windowModeIndex,
            kWindowModeOptions,
            IM_ARRAYSIZE(kWindowModeOptions)))
    {
        ensureValidMode(settings, monitor);
        if (settings.windowModeIndex == 0)
        {
            settings.windowPlacementValid = false;
            settings.windowWidth = settings.resolutionWidth;
            settings.windowHeight = settings.resolutionHeight;
        }
        changed = true;
    }

    const bool borderless = settings.windowModeIndex == 1;
    if (borderless)
        ImGui::BeginDisabled();

    const std::vector<ResolutionOption> resolutions = buildResolutionOptions(
        monitor,
        settings.windowModeIndex);
    const std::string resolutionPreview = std::to_string(settings.resolutionWidth)
        + " x " + std::to_string(settings.resolutionHeight);

    ImGui::SetNextItemWidth(220);
    if (ImGui::BeginCombo("Resolution", resolutionPreview.c_str()))
    {
        for (const ResolutionOption& resolution : resolutions)
        {
            const bool selected = resolution.width == settings.resolutionWidth
                && resolution.height == settings.resolutionHeight;
            if (ImGui::Selectable(resolution.label.c_str(), selected) && !selected)
            {
                settings.resolutionWidth = resolution.width;
                settings.resolutionHeight = resolution.height;
                ensureValidMode(settings, monitor);
                if (settings.windowModeIndex == 0)
                {
                    settings.windowPlacementValid = false;
                    settings.windowWidth = settings.resolutionWidth;
                    settings.windowHeight = settings.resolutionHeight;
                }
                changed = true;
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    const std::vector<int> rates = buildRefreshRates(
        monitor,
        settings.resolutionWidth,
        settings.resolutionHeight);
    const std::string refreshPreview = std::to_string(settings.refreshRate) + " Hz";

    ImGui::SetNextItemWidth(220);
    if (ImGui::BeginCombo("Refresh Rate", refreshPreview.c_str()))
    {
        for (const int rate : rates)
        {
            const bool selected = rate == settings.refreshRate;
            const std::string label = std::to_string(rate) + " Hz";
            if (ImGui::Selectable(label.c_str(), selected) && !selected)
            {
                settings.refreshRate = rate;
                changed = true;
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (borderless)
        ImGui::EndDisabled();

    if (borderless)
        ImGui::TextDisabled("Borderless mode uses the desktop resolution and refresh rate.");
    else if (settings.windowModeIndex == 0)
        ImGui::TextDisabled("Desktop-sized modes are hidden in Windowed mode.");

    int vsyncIndex = settings.vsyncEnabled ? 1 : 0;
    ImGui::SetNextItemWidth(220);
    if (ImGui::Combo("VSync", &vsyncIndex, kVSyncOptions, IM_ARRAYSIZE(kVSyncOptions)))
    {
        settings.vsyncEnabled = vsyncIndex == 1;
        changed = true;
    }

    ImGui::SetNextItemWidth(220);
    changed |= ImGui::Combo(
        "FPS Cap",
        &settings.fpsCapIndex,
        heritage::settings::kFpsCapOptionNames.data(),
        static_cast<int>(heritage::settings::kFpsCapOptionNames.size()));

    ImGui::SetNextItemWidth(220);
    changed |= ImGui::Combo(
        "Scale Mode",
        &settings.scaleModeIndex,
        kScaleOptions,
        IM_ARRAYSIZE(kScaleOptions));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Multi-monitor setup");
    ImGui::Separator();
    ImGui::Spacing();

    const bool spanAvailable = settings.windowModeIndex != 0;
    if (!spanAvailable)
        ImGui::BeginDisabled();

    changed |= ImGui::Checkbox("Span all monitors", &display.spanAllMonitors);

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
        {
            display.selected[monitorIndex] = selected;
            changed = true;
        }

        ImGui::SameLine();
        ImGui::TextDisabled(
            "%dx%d",
            display.monitors()[monitorIndex].width,
            display.monitors()[monitorIndex].height);

        ImGui::SetNextItemWidth(120);
        if (ImGui::InputInt("Bezel mm", &display.bezelMm[monitorIndex], 1, 10))
        {
            display.bezelMm[monitorIndex] = std::clamp(
                display.bezelMm[monitorIndex],
                0,
                100);
            changed = true;
        }

        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::SetNextItemWidth(220);
    changed |= ImGui::InputFloat(
        "Eye distance (cm)",
        &display.eyeDistanceCm,
        1.0f,
        5.0f,
        "%.1f");

    ImGui::SetNextItemWidth(220);
    if (ImGui::InputInt("Global Bezel (mm)", &display.globalBezelMm))
    {
        display.globalBezelMm = std::clamp(display.globalBezelMm, 0, 100);
        std::fill(
            display.bezelMm.begin(),
            display.bezelMm.end(),
            display.globalBezelMm);
        changed = true;
    }

    if (display.spanAllMonitors)
    {
        ImGui::TextDisabled(
            "Combined HFOV: %.1f°",
            display.getCombinedHFOVDegrees());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Image adjustments");
    ImGui::Separator();
    ImGui::Spacing();

    changed |= ImGui::SliderFloat("Gamma", &settings.gamma, 1.00f, 3.00f, "%.2f");
    changed |= ImGui::SliderFloat("Brightness", &settings.brightness, -0.50f, 0.50f, "%.2f");
    changed |= ImGui::SliderFloat("Contrast", &settings.contrast, 0.50f, 1.50f, "%.2f");
    changed |= ImGui::SliderFloat("Saturation", &settings.saturation, 0.00f, 2.00f, "%.2f");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Rendering quality");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::SetNextItemWidth(220);
    changed |= ImGui::Combo(
        "Anti-Aliasing",
        &settings.antiAliasingIndex,
        kAntiAliasingOptions,
        IM_ARRAYSIZE(kAntiAliasingOptions));

    ImGui::SetNextItemWidth(220);
    changed |= ImGui::Combo(
        "Texture Filter",
        &settings.textureFilterIndex,
        kTextureFilterOptions,
        IM_ARRAYSIZE(kTextureFilterOptions));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Shadows");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::SetNextItemWidth(220);
    changed |= ImGui::Combo(
        "Shadow Quality",
        &settings.shadowQualityIndex,
        heritage::settings::kShadowQualityOptionNames.data(),
        static_cast<int>(heritage::settings::kShadowQualityOptionNames.size()));

    ImGui::SetNextItemWidth(220);
    changed |= ImGui::Combo(
        "Shadow Filtering",
        &settings.shadowFilterIndex,
        heritage::settings::kShadowFilterOptionNames.data(),
        static_cast<int>(heritage::settings::kShadowFilterOptionNames.size()));
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
    {
        ImGui::BeginTooltip();
        ImGui::TextDisabled("Shadow filtering modes");
        ImGui::Separator();
        ImGui::BulletText("Nearest - crisp single-compare shadows.");
        ImGui::BulletText("Poisson PCF - smoother fixed-radius filtering.");
        ImGui::BulletText("PCSS + Poisson - contact-hardening soft shadows.");
        ImGui::EndTooltip();
    }

    return changed;
}

} // namespace

bool drawSettingsTabs(
    heritage::settings::VideoSettings& videoSettings,
    heritage::graphics::DisplaySystem& display)
{
    bool changed = false;

    if (ImGui::BeginTabBar("SettingsTabs"))
    {
        if (ImGui::BeginTabItem("Video"))
        {
            GLFWmonitor* primaryMonitor = nullptr;
            const auto& monitors = display.monitors();
            if (!monitors.empty())
            {
                const std::size_t primaryIndex = display.primaryMonitorIndex();
                primaryMonitor = primaryIndex < monitors.size()
                    ? monitors[primaryIndex].handle
                    : monitors.front().handle;
            }

            changed |= drawVideoTab(videoSettings, display, primaryMonitor);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Audio"))
        {
            ImGui::Spacing();
            ImGui::TextDisabled("Audio settings coming soon.");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Input"))
        {
            ImGui::Spacing();
            ImGui::TextWrapped("Input bindings are configured in-engine for the selected module.");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Gameplay"))
        {
            ImGui::Spacing();
            ImGui::TextDisabled("Gameplay settings are provided by the active module.");
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    return changed;
}

} // namespace racing::launcher

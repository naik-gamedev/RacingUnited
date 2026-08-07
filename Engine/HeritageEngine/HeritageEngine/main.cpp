#include <glad/glad.h>

// Heritage Engine - main.cpp
// Requirements: GLAD (OpenGL 4.6 core), GLFW 3.4, ImGui
//
// ESC          — open/close in-game menu
// F11          — cycle window modes (Windowed → Borderless → Exclusive)
// Mouse drag   — module-defined interaction
// Scroll       — module-defined interaction

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifdef _WIN32
#include <windows.h>
#include <dwmapi.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#else
#include <unistd.h>
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <array>
#include <fstream>
#include <sstream>
#include <map>
#include <set>
#include <tuple>
#include <filesystem>
#include <algorithm>
#include <climits>

#include "../Core/Math/Math.hpp"
#include "../Core/Diagnostics/BuildIdentity.hpp"
#include "../Core/Settings/VideoSettings.hpp"
#include "../Core/Settings/VideoSettingsStorage.hpp"
#include "../Core/Settings/AudioSettings.hpp"
#include "../Core/Settings/AudioSettingsStorage.hpp"
#include "../Core/Modules/ModuleLoader.hpp"
#include "../Core/Modules/ModuleContext.hpp"
#include "../Core/Timing/FrameLimiter.hpp"
#include "../Audio/AudioSystem.hpp"
#include "../Input/InputSystem.hpp"
#include "../Physics/PhysicsWorld.hpp"
#include "../Core/Entities/EntityRegistry.hpp"
#include "../Graphics/AntiAliasing.hpp"
#include "../Graphics/DisplaySystem.hpp"
#include "../Graphics/WindowSystem.hpp"
#include "../Graphics/Framebuffer/PostFramebuffer.hpp"
#include "../Graphics/PostProcessing/PostProcessor.hpp"
#include "../Graphics/Renderer/EntityDebugRenderer.hpp"
#include "../Graphics/Renderer/EntityMeshRenderer.hpp"
#include "../Graphics/RenderScaler.hpp"
#include "../Core/Modules/ModuleRuntimeManager.hpp"
#include "../Core/Modules/ModuleRuntimeServices.hpp"
#include "../UI/PauseMenu.hpp"

namespace fs = std::filesystem;
using heritage::math::Mat4;
using heritage::math::Vec3;
using heritage::math::perspective;
using heritage::graphics::AntiAliasingSettings;
using heritage::graphics::antiAliasingOptionCount;
using heritage::graphics::antiAliasingOptionNames;
using heritage::graphics::resolveAntiAliasing;
using heritage::graphics::DisplaySystem;
using heritage::graphics::WindowSystem;
using heritage::graphics::WindowMode;
using heritage::graphics::PostFramebuffer;
using heritage::graphics::PostProcessor;
using heritage::graphics::EntityDebugRenderer;
using heritage::graphics::EntityMeshRenderer;
using heritage::graphics::RenderScaler;
using heritage::graphics::RenderSize;
using heritage::settings::VideoSettings;
using heritage::settings::VideoSettingsStorage;
using heritage::settings::kDefaultWindowHeight;
using heritage::settings::kDefaultWindowWidth;
using heritage::settings::kMinimumInteractiveWindowHeight;
using heritage::settings::kMinimumInteractiveWindowWidth;
using heritage::settings::AudioSettings;
using heritage::settings::AudioSettingsStorage;
using heritage::settings::selectedFpsCap;
using heritage::modules::ModuleInfo;
using heritage::modules::ModuleLoader;
using heritage::modules::ModuleContext;
using heritage::modules::ModuleRuntimeManager;
using heritage::modules::ModuleRuntimeServices;
using heritage::modules::ModuleRuntimeAction;
using heritage::modules::ModuleRuntimeActionType;
using heritage::timing::FrameLimiter;
using heritage::audio::AudioSystem;
using heritage::input::InputSystem;
using heritage::physics::PhysicsWorld;
using heritage::entities::EntityRegistry;
using heritage::ui::drawPauseMenu;

#ifdef _WIN32
#define RACING_GLSL_VERSION "#version 460 core\n"
#else
#define RACING_GLSL_VERSION "#version 330 core\n"
#endif

static fs::path findProjectRoot()
{
#ifdef _WIN32
    char modulePath[MAX_PATH] = {};
    DWORD length = GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    fs::path location = length ? fs::path(modulePath).parent_path() : fs::current_path();
#else
    char modulePath[4096] = {};
    ssize_t length = readlink("/proc/self/exe", modulePath, sizeof(modulePath) - 1);
    fs::path location = length > 0 ? fs::path(std::string(modulePath, length)).parent_path() : fs::current_path();
#endif
    for (fs::path candidate = location; !candidate.empty(); candidate = candidate.parent_path())
    {
        if (fs::exists(candidate / "Modules") && fs::exists(candidate / "Assets"))
            return candidate.lexically_normal();
        if (candidate == candidate.parent_path()) break;
    }
    return fs::current_path().lexically_normal();
}

static void writeLaunchDiagnostics(
    const fs::path& projectRoot,
    const fs::path& requestedModulePath,
    const std::string& requestedModuleId,
    const ModuleInfo& activeModule,
    const ModuleContext& moduleContext,
    const std::string& activeRuntimeId,
    const std::string& activeContentId)
{
    try
    {
        const fs::path diagnosticRoot = projectRoot / "UserData";
        fs::create_directories(diagnosticRoot);

        std::ofstream file(diagnosticRoot / "last_launch.txt", std::ios::trunc);
        if (!file)
            return;

        file << "build_identity="
            << heritage::diagnostics::buildIdentity() << '\n';
        file << "build_step="
            << heritage::diagnostics::generated::kMilestone << '\n';
        file << "build_configuration="
            << heritage::diagnostics::compiledConfiguration() << '\n';
        file << "git_commit="
            << heritage::diagnostics::generated::kGitCommit << '\n';
        file << "git_dirty="
            << heritage::diagnostics::generated::kGitDirty << '\n';
        file << "build_identity_generated_utc="
            << heritage::diagnostics::generated::kGeneratedUtc << '\n';
        file << "project_root=" << projectRoot.string() << '\n';
        file << "requested_module_id=" << requestedModuleId << '\n';
        file << "requested_module_path=" << requestedModulePath.string() << '\n';
        file << "loaded_module_id=" << activeModule.id << '\n';
        file << "loaded_module_folder=" << activeModule.folderName << '\n';
        file << "loaded_module_root=" << activeModule.rootPath.string() << '\n';
        file << "manifest_runtime=" << activeModule.runtime << '\n';
        file << "manifest_entry_scene=" << activeModule.scene << '\n';
        file << "manifest_entry_script=" << activeModule.entryScript << '\n';
        file << "manifest_entry_ui=" << activeModule.entryUi << '\n';
        file << "active_runtime=" << activeRuntimeId << '\n';
        file << "active_content=" << activeContentId << '\n';
        file << "module_assets_root=" << moduleContext.assetRoot().string() << '\n';
        file << "module_scripts_root=" << moduleContext.scriptsRoot().string() << '\n';
        file << "module_scenes_root=" << moduleContext.scenesRoot().string() << '\n';
        file << "module_data_root=" << moduleContext.dataRoot().string() << '\n';
        file << "module_ui_root=" << moduleContext.uiRoot().string() << '\n';
        file << "module_settings_root=" << moduleContext.settingsRoot().string() << '\n';
        file << "module_save_root=" << moduleContext.saveRoot().string() << '\n';
    }
    catch (...)
    {
    }
}

// -----------------------------------------------------------------------
//  Fonts
// -----------------------------------------------------------------------
static ImFont* g_fontSmall = nullptr;
static ImFont* g_fontNormal = nullptr;
static ImFont* g_fontLarge = nullptr;

// -----------------------------------------------------------------------
//  Video settings state
// -----------------------------------------------------------------------
static VideoSettings g_videoSettings;
static AudioSettings g_audioSettings;

static bool g_displayChangePending = false;
static double g_displayChangeDeadline = 0.0;
static bool g_displayPopupOpened = false;
static constexpr double kDisplayChangeTimeout = 15.0;

static WindowMode g_displayPendingPrevMode = WindowMode::Windowed;
static int g_displayPendingPrevW = 0;
static int g_displayPendingPrevH = 0;
static int g_displayPendingPrevRefresh = GLFW_DONT_CARE;
static int g_displayPendingPrevSettingW = 0;
static int g_displayPendingPrevSettingH = 0;
static int g_displayPendingPrevSettingRefresh = 0;

static int g_displayPendingDesiredW = 0;
static int g_displayPendingDesiredH = 0;
static int g_displayPendingDesiredRefresh = GLFW_DONT_CARE;

// Systems
static DisplaySystem g_display;
static WindowSystem  g_window;
static AudioSystem   g_audio;
static InputSystem   g_input;
static PhysicsWorld  g_physics;

static fs::path g_videoSettingsPath;
static fs::path g_displaySettingsPath;
static fs::path g_audioSettingsPath;
static fs::path g_inputSettingsPath;

static void applyTextureFiltering(GLenum target)
{
    switch (g_videoSettings.textureFilterIndex)
    {
    case 0:
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        break;
    case 1:
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        break;
    case 2:
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        break;
    default:
    {
        float af = 1.f;
        switch (g_videoSettings.textureFilterIndex) {
        case 3: af = 2.f;  break;
        case 4: af = 4.f;  break;
        case 5: af = 8.f;  break;
        case 6: af = 16.f; break;
        }
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        float maxAF; glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAF);
        glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY, fminf(af, maxAF));
        break;
    }
    }
}

static void applyStyle()
{
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 0; s.FrameRounding = 4; s.GrabRounding = 4;
    s.WindowBorderSize = 0; s.FrameBorderSize = 0;
    s.WindowPadding = ImVec2(10, 6); s.ItemSpacing = ImVec2(8, 6);

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.f);
    c[ImGuiCol_Button] = ImVec4(0.15f, 0.15f, 0.15f, 1.f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.f);
    c[ImGuiCol_Text] = ImVec4(0.85f, 0.85f, 0.85f, 1.f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.40f, 0.40f, 0.40f, 1.f);
    c[ImGuiCol_Separator] = ImVec4(0.20f, 0.20f, 0.20f, 1.f);
    c[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.f);
    c[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.f);
    c[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.90f, 0.90f, 1.f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.70f, 0.70f, 0.70f, 1.f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 1.00f, 1.00f, 1.f);
    c[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.12f, 1.f);
    c[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.f);
    c[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.f);
    c[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.20f, 1.f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.25f, 0.25f, 1.f);
}

static const heritage::graphics::MonitorInfo* primaryMonitorInfo()
{
    const auto& monitors = g_display.monitors();
    if (monitors.empty())
        return nullptr;

    const std::size_t primaryIndex = g_display.primaryMonitorIndex();
    if (primaryIndex >= monitors.size())
        return &monitors.front();

    return &monitors[primaryIndex];
}

static void setWindowModeIndex(WindowMode mode)
{
    if (mode == WindowMode::Windowed)
        g_videoSettings.windowModeIndex = 0;
    else if (mode == WindowMode::Borderless)
        g_videoSettings.windowModeIndex = 1;
    else
        g_videoSettings.windowModeIndex = 2;
}

static void enforceSpanCompatibility()
{
    // The current custom spanning path is not valid in an ordinary resizable
    // window. Disable it as soon as the engine enters Windowed mode.
    if (g_window.mode() == WindowMode::Windowed)
        g_display.spanAllMonitors = false;
}

static void syncVideoSettingsToCurrentDisplay(GLFWwindow* window)
{
    setWindowModeIndex(g_window.mode());

    if (g_window.mode() == WindowMode::Windowed)
    {
        glfwGetWindowSize(
            window,
            &g_videoSettings.resolutionWidth,
            &g_videoSettings.resolutionHeight);
    }
    else if (const auto* monitor = primaryMonitorInfo())
    {
        const GLFWvidmode* mode = glfwGetVideoMode(monitor->handle);
        if (mode)
        {
            g_videoSettings.resolutionWidth = mode->width;
            g_videoSettings.resolutionHeight = mode->height;
            g_videoSettings.refreshRate = mode->refreshRate;
        }
    }

    if (g_videoSettings.refreshRate <= 0)
    {
        if (const auto* monitor = primaryMonitorInfo())
            g_videoSettings.refreshRate = monitor->refreshRate;
    }
}

static WindowMode windowModeFromIndex(int index)
{
    if (index == 1)
        return WindowMode::Borderless;
    if (index == 2)
        return WindowMode::Exclusive;
    return WindowMode::Windowed;
}

static bool monitorSupportsMode(
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

static void applyLoadedDisplayMode(GLFWwindow* window)
{
    const auto* monitor = primaryMonitorInfo();
    if (!monitor)
    {
        setWindowModeIndex(WindowMode::Windowed);
        g_display.spanAllMonitors = false;
        syncVideoSettingsToCurrentDisplay(window);
        return;
    }

    WindowMode requestedMode = windowModeFromIndex(g_videoSettings.windowModeIndex);

    if (requestedMode == WindowMode::Borderless)
    {
        g_window.setMode(
            window,
            WindowMode::Borderless,
            0,
            0,
            GLFW_DONT_CARE,
            monitor->handle);
    }
    else if (requestedMode == WindowMode::Exclusive)
    {
        int width = g_videoSettings.resolutionWidth;
        int height = g_videoSettings.resolutionHeight;
        int refresh = g_videoSettings.refreshRate;

        // Never submit a stale or unsupported exclusive mode. A monitor or
        // graphics-driver change falls back to the current desktop mode.
        if (!monitorSupportsMode(*monitor, width, height, refresh))
        {
            width = monitor->width;
            height = monitor->height;
            refresh = monitor->refreshRate;
        }

        g_window.setMode(
            window,
            WindowMode::Exclusive,
            width,
            height,
            refresh,
            monitor->handle);
    }
    else
    {
        // The window was created directly at the saved Windowed rectangle,
        // so no mode switch is required here. Avoiding setMode also avoids
        // unnecessarily recentering a user-positioned window at startup.
        setWindowModeIndex(WindowMode::Windowed);
    }

    enforceSpanCompatibility();
    syncVideoSettingsToCurrentDisplay(window);
}

static void saveAllSettings(GLFWwindow* window)
{
    if (!window)
        return;

    // Only an ordinary Windowed rectangle should replace the remembered
    // Windowed placement. Borderless and Exclusive dimensions are separate.
    if (g_window.mode() == WindowMode::Windowed)
        g_window.saveCurrentRect(window);

    g_videoSettings.windowPlacementValid = true;
    g_videoSettings.windowX = g_window.savedX();
    g_videoSettings.windowY = g_window.savedY();
    g_videoSettings.windowWidth = g_window.savedW();
    g_videoSettings.windowHeight = g_window.savedH();

    syncVideoSettingsToCurrentDisplay(window);

    try
    {
        if (!VideoSettingsStorage::save(
                g_videoSettingsPath.string(),
                g_videoSettings))
        {
            std::cerr << "Could not save video settings.\n";
        }

        g_display.save(g_displaySettingsPath.string());

        if (!AudioSettingsStorage::save(
                g_audioSettingsPath.string(),
                g_audio.settings()))
        {
            std::cerr << "Could not save audio settings.\n";
        }
    }
    catch (...)
    {
        std::cerr << "Could not save engine settings.\n";
    }
}

static void restorePendingDisplayChange(GLFWwindow* window)
{
    GLFWmonitor* targetMonitor = nullptr;
    if (const auto* monitor = primaryMonitorInfo())
        targetMonitor = monitor->handle;

    g_window.setMode(
        window,
        g_displayPendingPrevMode,
        g_displayPendingPrevW,
        g_displayPendingPrevH,
        g_displayPendingPrevRefresh,
        targetMonitor);
    enforceSpanCompatibility();

    g_videoSettings.resolutionWidth = g_displayPendingPrevSettingW;
    g_videoSettings.resolutionHeight = g_displayPendingPrevSettingH;
    g_videoSettings.refreshRate = g_displayPendingPrevSettingRefresh;
    setWindowModeIndex(g_displayPendingPrevMode);
    g_displayChangePending = false;
}

static void initiateDisplayChange(
    GLFWwindow* window,
    WindowMode newMode,
    int desiredW,
    int desiredH,
    int desiredRefresh)
{
    g_displayPendingPrevMode = g_window.mode();
    glfwGetWindowSize(window, &g_displayPendingPrevW, &g_displayPendingPrevH);

    g_displayPendingPrevSettingW = g_videoSettings.resolutionWidth;
    g_displayPendingPrevSettingH = g_videoSettings.resolutionHeight;
    g_displayPendingPrevSettingRefresh = g_videoSettings.refreshRate;

    g_displayPendingPrevRefresh = g_videoSettings.refreshRate;
    if (g_displayPendingPrevMode != WindowMode::Windowed)
    {
        if (const auto* monitor = primaryMonitorInfo())
        {
            if (const GLFWvidmode* currentMode = glfwGetVideoMode(monitor->handle))
                g_displayPendingPrevRefresh = currentMode->refreshRate;
        }
    }

    g_displayPendingDesiredW = desiredW;
    g_displayPendingDesiredH = desiredH;
    g_displayPendingDesiredRefresh = desiredRefresh;

    GLFWmonitor* targetMonitor = nullptr;
    if (const auto* monitor = primaryMonitorInfo())
        targetMonitor = monitor->handle;

    g_window.setMode(
        window,
        newMode,
        desiredW,
        desiredH,
        desiredRefresh,
        targetMonitor);
    enforceSpanCompatibility();

    setWindowModeIndex(newMode);

    // Borderless always follows the desktop mode. Keep the user's selected
    // windowed/exclusive mode so it is still available when they switch back.
    if (newMode != WindowMode::Borderless)
    {
        if (desiredW > 0)
            g_videoSettings.resolutionWidth = desiredW;
        if (desiredH > 0)
            g_videoSettings.resolutionHeight = desiredH;
        if (desiredRefresh > 0)
            g_videoSettings.refreshRate = desiredRefresh;
    }

    g_displayChangePending = true;
    g_displayChangeDeadline = glfwGetTime() + kDisplayChangeTimeout;
    g_displayPopupOpened = true;
}

// -----------------------------------------------------------------------
//  Main
// -----------------------------------------------------------------------
int main(int argc, char** argv)
{
    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return -1; }

#ifdef _WIN32
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#endif
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    const fs::path requestedProjectRoot = ModuleLoader::requestedProjectRoot(argc, argv);
    const fs::path projectRoot = requestedProjectRoot.empty()
        ? findProjectRoot()
        : requestedProjectRoot.lexically_normal();

    const fs::path requestedModulePath = ModuleLoader::requestedModulePath(argc, argv);
    const std::string requestedModuleId = ModuleLoader::requestedModuleId(argc, argv);

    ModuleInfo activeModule;
    if (!requestedModulePath.empty())
        activeModule = ModuleLoader::loadFromPath(projectRoot, requestedModulePath);

    // Compatibility path for direct launches and old shortcuts.
    if (!activeModule.valid && !requestedModuleId.empty())
        activeModule = ModuleLoader::load(projectRoot, requestedModuleId);

    if (!activeModule.valid)
    {
        std::string message = "Heritage Engine could not load the requested module.";
        if (!requestedModuleId.empty())
            message += "\nModule id: " + requestedModuleId;
        if (!requestedModulePath.empty())
            message += "\nModule path: " + requestedModulePath.string();
        message += "\nProject root: " + projectRoot.string();

        std::cerr << message << '\n';
#ifdef _WIN32
        MessageBoxA(nullptr, message.c_str(), "Heritage Engine - Module Error", MB_OK | MB_ICONERROR);
#endif
        glfwTerminate();
        return -1;
    }

    if (!requestedModuleId.empty() && activeModule.id != requestedModuleId)
    {
        const std::string message =
            "The launcher requested module id '" + requestedModuleId
            + "', but the selected module folder declares id '" + activeModule.id
            + "'.\n\nLaunch was stopped instead of loading the wrong module.";

        std::cerr << message << '\n';
#ifdef _WIN32
        MessageBoxA(nullptr, message.c_str(), "Heritage Engine - Module Identity Error", MB_OK | MB_ICONERROR);
#endif
        glfwTerminate();
        return -1;
    }

    ModuleContext moduleContext(projectRoot, activeModule);
    std::string moduleContextError;
    if (!moduleContext.prepareUserDirectories(moduleContextError))
    {
        std::cerr << moduleContextError << '\n';
#ifdef _WIN32
        MessageBoxA(nullptr, moduleContextError.c_str(), "Heritage Engine - Module Error", MB_OK | MB_ICONERROR);
#endif
        glfwTerminate();
        return -1;
    }

    g_videoSettingsPath = moduleContext.resolveSettingsPath("settings_video.ini");
    g_displaySettingsPath = moduleContext.resolveSettingsPath("settings_engine.ini");
    g_audioSettingsPath = moduleContext.resolveSettingsPath("settings_audio.ini");
    g_inputSettingsPath = moduleContext.resolveSettingsPath("settings_input.ini");

    try
    {
        if (fs::exists(g_videoSettingsPath))
        {
            VideoSettingsStorage::load(
                g_videoSettingsPath.string(),
                g_videoSettings);
        }
        else
        {
            // One-time compatibility path for settings created before modules
            // had independent persistent data.
            const fs::path legacyVideoSettingsPath = projectRoot / "settings_video.ini";
            if (fs::exists(legacyVideoSettingsPath))
            {
                VideoSettingsStorage::load(
                    legacyVideoSettingsPath.string(),
                    g_videoSettings);
            }
        }
    }
    catch (...)
    {
        g_videoSettings = VideoSettings{};
    }

    try
    {
        if (fs::exists(g_audioSettingsPath))
            AudioSettingsStorage::load(g_audioSettingsPath.string(), g_audioSettings);
    }
    catch (...)
    {
        g_audioSettings = AudioSettings{};
    }

    std::cout << "Build identity: "
        << heritage::diagnostics::buildIdentity() << "\n";
    std::cout << "Project root: " << projectRoot.string() << "\n";
    std::cout << "Active module: " << activeModule.name
              << " [" << activeModule.id << "]\n";
    std::cout << "Module root: " << activeModule.rootPath.string() << "\n";
    std::cout << "Manifest entry_scene: "
              << (activeModule.scene.empty() ? "<empty>" : activeModule.scene)
              << "\n";

    GLFWmonitor* mon = glfwGetPrimaryMonitor();
    const GLFWvidmode* vid = glfwGetVideoMode(mon);
    if (!mon || !vid)
    {
        std::cerr << "Primary monitor information is unavailable.\n";
        glfwTerminate();
        return -1;
    }

    int workX = 0;
    int workY = 0;
    int workW = vid->width;
    int workH = vid->height;
    glfwGetMonitorWorkarea(mon, &workX, &workY, &workW, &workH);

    const int maximumWindowWidth = std::max(1, workW);
    const int maximumWindowHeight = std::max(1, workH);
    const int minimumWindowWidth = std::min(
        kMinimumInteractiveWindowWidth,
        maximumWindowWidth);
    const int minimumWindowHeight = std::min(
        kMinimumInteractiveWindowHeight,
        maximumWindowHeight);

    int startW = g_videoSettings.windowPlacementValid
        ? g_videoSettings.windowWidth
        : (g_videoSettings.resolutionWidth > 0
            ? g_videoSettings.resolutionWidth
            : kDefaultWindowWidth);
    int startH = g_videoSettings.windowPlacementValid
        ? g_videoSettings.windowHeight
        : (g_videoSettings.resolutionHeight > 0
            ? g_videoSettings.resolutionHeight
            : kDefaultWindowHeight);

    startW = std::clamp(startW, minimumWindowWidth, maximumWindowWidth);
    startH = std::clamp(startH, minimumWindowHeight, maximumWindowHeight);

    int startX = workX + std::max(0, (workW - startW) / 2);
    int startY = workY + std::max(0, (workH - startH) / 2);

    if (g_videoSettings.windowPlacementValid)
    {
        const int maximumX = workX + std::max(0, workW - startW);
        const int maximumY = workY + std::max(0, workH - startH);
        startX = std::clamp(g_videoSettings.windowX, workX, maximumX);
        startY = std::clamp(g_videoSettings.windowY, workY, maximumY);
    }

    const std::string sceneTitle = activeModule.scene.empty()
        ? "<empty>"
        : activeModule.scene;
    const std::string windowTitle = activeModule.name + " [" + activeModule.id
        + " | " + sceneTitle + "] - Heritage Engine";
    GLFWwindow* window = glfwCreateWindow(startW, startH, windowTitle.c_str(), nullptr, nullptr);
    if (!window) { std::cerr << "Window failed\n"; glfwTerminate(); return -1; }

    // Step 29I.1 safety: custom window chrome and the prototype lab become
    // unusable below this size. GLFW enforces the limit for manual resizing,
    // while the settings loader above repairs stale/tiny remembered rectangles.
    glfwSetWindowSizeLimits(
        window,
        minimumWindowWidth,
        minimumWindowHeight,
        GLFW_DONT_CARE,
        GLFW_DONT_CARE);
    glfwSetWindowPos(window, startX, startY);
    std::cout << "Window safety floor: "
              << minimumWindowWidth << "x" << minimumWindowHeight << "\n";

#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(window);
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
#endif

    glfwMakeContextCurrent(window);
    glfwSwapInterval(g_videoSettings.vsyncEnabled ? 1 : 0);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "GLAD failed\n"; return -1; }
    std::cout << "OpenGL " << glGetString(GL_VERSION) << "\n";

    // Systems
    g_window.initialize(window);
    g_display.initialize();
    try {
        if (fs::exists(g_displaySettingsPath))
        {
            g_display.load(g_displaySettingsPath.string());
        }
        else
        {
            const fs::path legacyDisplaySettingsPath = projectRoot / "settings_engine.ini";
            if (fs::exists(legacyDisplaySettingsPath))
                g_display.load(legacyDisplaySettingsPath.string());
        }
    }
    catch (...) {}

    applyLoadedDisplayMode(window);

    std::string audioMessage;
    if (!g_audio.initialize(audioMessage))
    {
        std::cerr << "Audio disabled: " << audioMessage << '\n';
    }
    g_audio.applySettings(g_audioSettings);

    std::string inputMessage;
    if (!g_input.initialize(window, g_inputSettingsPath, inputMessage))
    {
        std::cerr << "Input initialization warning: " << inputMessage << '\n';
    }
    else if (!inputMessage.empty())
    {
        std::cerr << "Input settings warning: " << inputMessage << '\n';
    }

    // Module-owned action declarations are loaded natively before Lua starts.
    // This keeps the Input settings page functional even if a script has a
    // syntax error or chooses not to register actions dynamically.
    const fs::path inputDefinitionsPath =
        moduleContext.resolveDataPath("InputActions.ini");
    std::string inputDefinitionsMessage;
    if (!g_input.loadActionDefinitions(
            inputDefinitionsPath,
            inputDefinitionsMessage))
    {
        std::cerr << "Input action definition error: "
                  << inputDefinitionsMessage << '\n';
    }
    else if (!inputDefinitionsMessage.empty())
    {
        std::cout << inputDefinitionsMessage << '\n';
    }

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    const std::string fontPath = (projectRoot / "Assets/Fonts/Orbitron-SemiBold.ttf").string();
    g_fontSmall = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 13.0f);
    g_fontNormal = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f);
    g_fontLarge = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 22.0f);
    if (!g_fontSmall || !g_fontNormal || !g_fontLarge)
        g_fontSmall = g_fontNormal = g_fontLarge = io.Fonts->AddFontDefault();

    applyStyle();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(RACING_GLSL_VERSION);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_CULL_FACE);

    EntityRegistry entityRegistry;
    entityRegistry.resetForModule(activeModule.id);
    g_physics.reset();

    ModuleRuntimeManager moduleRuntime;
    ModuleRuntimeServices runtimeServices;
    runtimeServices.audio = &g_audio;
    runtimeServices.input = &g_input;
    runtimeServices.entities = &entityRegistry;
    runtimeServices.physics = &g_physics;

    std::string runtimeMessage;
    if (!moduleRuntime.initialize(window, moduleContext, runtimeServices, runtimeMessage))
    {
        const std::string message = runtimeMessage.empty()
            ? "Heritage Engine could not start the selected module runtime."
            : runtimeMessage;

        std::cerr << message << '\n';
#ifdef _WIN32
        MessageBoxA(
            nullptr,
            message.c_str(),
            "Heritage Engine - Module Runtime Error",
            MB_OK | MB_ICONERROR);
#endif
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        g_input.shutdown();
        g_audio.shutdown();
        g_display.shutdown();
        g_window.shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    if (!runtimeMessage.empty())
        std::cerr << runtimeMessage << '\n';

    EntityMeshRenderer entityMeshRenderer;
    if (!entityMeshRenderer.initialize(moduleContext.assetRoot()))
    {
        const std::string message = entityMeshRenderer.lastError().empty()
            ? "EntityMeshRenderer could not initialize."
            : entityMeshRenderer.lastError();
        std::cerr << message << '\n';
#ifdef _WIN32
        MessageBoxA(
            nullptr,
            message.c_str(),
            "Heritage Engine - Entity Mesh Renderer Error",
            MB_OK | MB_ICONERROR);
#endif
        moduleRuntime.shutdown();
        entityRegistry.clear();
        g_input.shutdown();
        g_audio.shutdown();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        g_display.shutdown();
        g_window.shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    EntityDebugRenderer entityDebugRenderer;
    if (!entityDebugRenderer.initialize())
    {
        const std::string message =
            "EntityDebugRenderer could not initialize its primitive meshes or shader.";
        std::cerr << message << '\n';
#ifdef _WIN32
        MessageBoxA(
            nullptr,
            message.c_str(),
            "Heritage Engine - Entity Renderer Error",
            MB_OK | MB_ICONERROR);
#endif
        moduleRuntime.shutdown();
        entityMeshRenderer.shutdown();
        entityRegistry.clear();
        g_input.shutdown();
        g_audio.shutdown();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        g_display.shutdown();
        g_window.shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    std::cout << "Active runtime: " << moduleRuntime.runtimeId() << "\n";
    std::cout << "Active content: " << moduleRuntime.activeContentId() << "\n";
    std::cout << "Registered input actions: "
              << g_input.actionCount() << "\n";
    writeLaunchDiagnostics(
        projectRoot,
        requestedModulePath,
        requestedModuleId,
        activeModule,
        moduleContext,
        moduleRuntime.runtimeId(),
        moduleRuntime.activeContentId());

    PostProcessor postProcessor;
    if (!postProcessor.initialize()) {
        std::cerr << "PostProcessor initialization failed\n";
        return -1;
    }

    PostFramebuffer msaaFBO, resolveFBO;

    FrameLimiter frameLimiter;
    bool appliedVSync = g_videoSettings.vsyncEnabled;

    double prevTime = glfwGetTime();
    bool   shouldClose = false;
    bool   shouldMin = false;
    bool   f11Prev = false;
    bool   escPrev = false;
    bool   menuOpen = false;
    bool   menuShowSettings = false;

    int prevFbW = 0, prevFbH = 0;
    int prevAA = -1, prevScale = -1;

    while (!glfwWindowShouldClose(window) && !shouldClose)
    {
        if (appliedVSync != g_videoSettings.vsyncEnabled)
        {
            glfwSwapInterval(g_videoSettings.vsyncEnabled ? 1 : 0);
            appliedVSync = g_videoSettings.vsyncEnabled;
            frameLimiter.reset();
        }

        frameLimiter.beginFrame();
        glfwPollEvents();
        g_input.update();
        g_audio.update(glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE);

        if (shouldMin) { glfwIconifyWindow(window); shouldMin = false; }

        // F11
        bool f11Now = (glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS);
        if (f11Now && !f11Prev)
        {
            g_window.cycleMode(window);
            enforceSpanCompatibility();
            syncVideoSettingsToCurrentDisplay(window);
        }
        f11Prev = f11Now;

        // ESC
        bool escNow = (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS);
        if (escNow && !escPrev) {
            menuOpen = !menuOpen;
            if (!menuOpen) menuShowSettings = false;
        }
        escPrev = escNow;

        g_window.update(window);
        enforceSpanCompatibility();

        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        if (fbW == 0 || fbH == 0)
        {
            glfwSwapBuffers(window);
            frameLimiter.endFrame(selectedFpsCap(g_videoSettings));
            continue;
        }

        const RenderSize renderSize = RenderScaler::calculateRenderSize(fbW, fbH, g_videoSettings.scaleModeIndex);
        const int rW = renderSize.width;
        const int rH = renderSize.height;

        if (fbW != prevFbW || fbH != prevFbH || g_videoSettings.antiAliasingIndex != prevAA || g_videoSettings.scaleModeIndex != prevScale)
        {
            const AntiAliasingSettings antiAliasing = resolveAntiAliasing(g_videoSettings.antiAliasingIndex);
            if (antiAliasing.msaaSamples > 1)
                msaaFBO.init(rW, rH, antiAliasing.msaaSamples);
            // The normal scene always renders to an off-screen texture.
            // This gives every non-spanning frame one consistent presentation
            // path and prepares the pipeline for global post-processing passes.
            resolveFBO.init(rW, rH, 1);
            prevFbW = fbW; prevFbH = fbH;
            prevAA = g_videoSettings.antiAliasingIndex; prevScale = g_videoSettings.scaleModeIndex;
        }

        g_display.updateSpanFBO();

        double now = glfwGetTime();
        float dt = (float)(now - prevTime); prevTime = now;

        // Physics owns a deterministic fixed-step clock independent from the
        // rendered frame rate. Module fixed updates, rigid bodies, contacts and
        // future vehicle systems all execute from this world clock.
        g_physics.advance(
            static_cast<double>(dt),
            !menuOpen,
            [&](float fixedDeltaTime)
            {
                moduleRuntime.fixedUpdate(fixedDeltaTime);
            });

        moduleRuntime.update(dt, !menuOpen);

        // Rigid bodies remain on the deterministic fixed clock. Only their
        // interpolated poses are copied to bound entities for this render.
        g_physics.synchronizeEntityTransforms(entityRegistry);

        const AntiAliasingSettings antiAliasing = resolveAntiAliasing(g_videoSettings.antiAliasingIndex);
        bool needMSAA = antiAliasing.msaaSamples > 1;
        bool needFXAA = antiAliasing.useFxaa;
        if (needMSAA) glEnable(GL_MULTISAMPLE);
        else glDisable(GL_MULTISAMPLE);
        bool needScale = RenderScaler::requiresScaling(fbW, fbH, renderSize);
        bool nearestUp = RenderScaler::usesNearestNeighbour(g_videoSettings.scaleModeIndex);

        // ========== RENDER ==========
        const Vec3 sceneClearColor = moduleRuntime.clearColor();
        bool spanning = g_display.isSpanning() && g_display.spanFBO() != 0;

        if (spanning)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, g_display.spanFBO());
            glViewport(0, 0, g_display.spanWidth(), g_display.spanHeight());
            glClearColor(sceneClearColor.x, sceneClearColor.y, sceneClearColor.z, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_SCISSOR_TEST);
            glEnable(GL_DEPTH_TEST);

            int minX = INT_MAX, minY = INT_MAX, maxY = INT_MIN;
            for (size_t i = 0; i < g_display.monitors().size(); ++i) {
                if (!g_display.selected[i]) continue;
                const auto& mi = g_display.monitors()[i];
                minX = std::min(minX, mi.xpos);
                minY = std::min(minY, mi.ypos);
                maxY = std::max(maxY, mi.ypos + mi.height);
            }
            int desktopH = maxY - minY;

            for (size_t i = 0; i < g_display.monitors().size(); ++i)
            {
                if (!g_display.selected[i]) continue;
                const auto& mi = g_display.monitors()[i];

                int relX = mi.xpos - minX;
                int relY = mi.ypos - minY;
                int monW = mi.width;
                int monH = mi.height;

                int fboX = (int)floorf(relX * g_display.spanScale());
                int fboY = (int)floorf((desktopH - (relY + monH)) * g_display.spanScale());
                int fboW = (int)floorf(monW * g_display.spanScale());
                int fboH = (int)floorf(monH * g_display.spanScale());

                glViewport(fboX, fboY, fboW, fboH);
                glScissor(fboX, fboY, fboW, fboH);
                glClear(GL_DEPTH_BUFFER_BIT);

                Mat4 projOff = g_display.getOffAxisProjection(i);

                moduleRuntime.render(projOff, g_videoSettings);
                entityMeshRenderer.draw(
                    entityRegistry,
                    projOff,
                    g_videoSettings,
                    static_cast<float>(now));
                entityDebugRenderer.draw(
                    entityRegistry,
                    projOff,
                    g_videoSettings,
                    static_cast<float>(now));
            }

            glDisable(GL_SCISSOR_TEST);

            glBindFramebuffer(GL_READ_FRAMEBUFFER, g_display.spanFBO());
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlitFramebuffer(0, 0, g_display.spanWidth(), g_display.spanHeight(),
                0, 0, fbW, fbH, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        }
        else
        {
            if (needMSAA && msaaFBO.fbo)
                glBindFramebuffer(GL_FRAMEBUFFER, msaaFBO.fbo);
            else
                glBindFramebuffer(GL_FRAMEBUFFER, resolveFBO.fbo);

            glViewport(0, 0, rW, rH);
            glClearColor(sceneClearColor.x, sceneClearColor.y, sceneClearColor.z, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);

            Mat4 proj = perspective(0.6f, (float)rW / (float)rH, 0.1f, 100.f);

            moduleRuntime.render(proj, g_videoSettings);
            entityMeshRenderer.draw(
                entityRegistry,
                proj,
                g_videoSettings,
                static_cast<float>(now));
            entityDebugRenderer.draw(
                entityRegistry,
                proj,
                g_videoSettings,
                static_cast<float>(now));

            if (needMSAA && msaaFBO.fbo && resolveFBO.fbo)
            {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFBO.fbo);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFBO.fbo);
                glBlitFramebuffer(0, 0, rW, rH, 0, 0, rW, rH, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            }

            if (resolveFBO.fbo)
            {
                if (needFXAA)
                {
                    // FXAA samples the internal-resolution scene texture and
                    // writes directly to the final framebuffer. Scaling, when
                    // enabled, happens naturally in this fullscreen pass.
                    postProcessor.applyFxaa(
                        resolveFBO.tex,
                        rW,
                        rH,
                        0,
                        fbW,
                        fbH);
                }
                else
                {
                    // Even native-resolution frames now use the same final
                    // presentation path. This is intentionally a simple blit
                    // today; later color grading and vignette can be inserted
                    // here without changing how the scene renderer works.
                    postProcessor.blit(
                        resolveFBO.tex,
                        0,
                        fbW,
                        fbH,
                        !needScale || nearestUp);
                }
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Module-owned overlay UI is drawn behind the permanent engine UI.
        moduleRuntime.drawUI(fbW, fbH);

        // Runtimes communicate with the engine through a deliberately small
        // action queue instead of reaching into main.cpp or WindowSystem.
        ModuleRuntimeAction moduleAction;
        while (moduleRuntime.pollAction(moduleAction))
        {
            if (moduleAction.type == ModuleRuntimeActionType::OpenEngineSettings)
            {
                menuOpen = true;
                menuShowSettings = true;
            }
            else if (moduleAction.type == ModuleRuntimeActionType::ExitApplication)
            {
                shouldClose = true;
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
        }

        // Display change confirmation (kept for now)
        if (g_displayChangePending)
        {
            if (g_displayPopupOpened)
            {
                if (ImGuiViewport* vp = ImGui::GetMainViewport())
                    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                ImGui::OpenPopup("Display Change");
                g_displayPopupOpened = false;
            }

            double nowt = glfwGetTime();
            if (nowt >= g_displayChangeDeadline)
                restorePendingDisplayChange(window);

            if (g_displayChangePending && ImGui::BeginPopupModal("Display Change", nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
            {
                const char* modeStr = (g_window.mode() == WindowMode::Windowed) ? "Windowed" :
                    (g_window.mode() == WindowMode::Borderless) ? "Borderless" : "Exclusive";
                ImGui::Text("Switched to %s mode.", modeStr);
                ImGui::Spacing();
                ImGui::Text(
                    "Resolution: %d x %d",
                    g_displayPendingDesiredW > 0 ? g_displayPendingDesiredW : fbW,
                    g_displayPendingDesiredH > 0 ? g_displayPendingDesiredH : fbH);
                if (g_displayPendingDesiredRefresh > 0)
                    ImGui::Text("Refresh rate: %d Hz", g_displayPendingDesiredRefresh);
                ImGui::Spacing();
                double remaining = g_displayChangeDeadline - nowt;
                if (remaining < 0) remaining = 0;
                ImGui::Text("Keep these display settings? Reverting in %.0f seconds...", remaining);
                ImGui::Spacing();
                if (ImGui::Button("Keep", ImVec2(120, 0)))
                {
                    g_displayChangePending = false;
                    saveAllSettings(window);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Revert", ImVec2(120, 0)))
                {
                    restorePendingDisplayChange(window);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        // Titlebar + Resize (now clean)
        g_window.drawTitlebar(window, fbW, fbH, shouldClose, shouldMin);
        g_window.drawResizeHandles(window);

        // In-game pause menu
        drawPauseMenu(
            window,
            menuOpen,
            menuShowSettings,
            shouldClose,
            fbW,
            fbH,
            g_fontLarge,
            g_fontNormal,
            g_display,
            g_window,
            g_videoSettings,
            g_audio,
            g_input,
            vid->width,
            vid->height,
            [&](WindowMode newMode, int desiredW, int desiredH, int desiredRefresh)
            {
                initiateDisplayChange(window, newMode, desiredW, desiredH, desiredRefresh);
            });

        ImGui::Render();
        glDisable(GL_DEPTH_TEST);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        frameLimiter.endFrame(selectedFpsCap(g_videoSettings));
    }

    // Cleanup
    // An unconfirmed display change must never become the saved startup mode.
    if (g_displayChangePending)
        restorePendingDisplayChange(window);

    saveAllSettings(window);

    moduleRuntime.shutdown();
    entityDebugRenderer.shutdown();
    entityMeshRenderer.shutdown();
    entityRegistry.clear();
    g_physics.reset();
    g_input.shutdown();
    g_audio.shutdown();
    g_display.shutdown();
    g_window.shutdown();
    msaaFBO.destroy();
    resolveFBO.destroy();
    postProcessor.shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();


    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
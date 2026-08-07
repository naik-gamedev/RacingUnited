// Racing United - Launcher
// Powered by Heritage Engine
// Borderless custom window — no Windows chrome.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <dwmapi.h>
#else
#include <unistd.h>
#endif
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <cstdint>
#include <algorithm>
#include <utility>

#include "../HeritageEngine/Core/Modules/ModuleLoader.hpp"
#include "../HeritageEngine/Core/Settings/VideoSettingsStorage.hpp"
#include "../HeritageEngine/Graphics/DisplaySystem.hpp"
#include "LauncherSettingsPage.hpp"
#include "LauncherState.hpp"

namespace fs = std::filesystem;
using heritage::modules::ModuleInfo;
using heritage::modules::ModuleLoader;
using heritage::settings::VideoSettings;
using heritage::settings::VideoSettingsStorage;
using heritage::graphics::DisplaySystem;
using racing::launcher::drawSettingsTabs;
using racing::launcher::LauncherState;
using racing::launcher::LauncherStateStorage;

#ifdef _WIN32
#define RACING_GLSL_VERSION "#version 460"
#else
#define RACING_GLSL_VERSION "#version 330"
#endif

static fs::path executableDirectory()
{
#ifdef _WIN32
    char modulePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    return length
        ? fs::path(modulePath).parent_path().lexically_normal()
        : fs::current_path().lexically_normal();
#else
    char modulePath[4096] = {};
    const ssize_t length = readlink("/proc/self/exe", modulePath, sizeof(modulePath) - 1);
    return length > 0
        ? fs::path(std::string(modulePath, length)).parent_path().lexically_normal()
        : fs::current_path().lexically_normal();
#endif
}

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
            return candidate;
        if (candidate == candidate.parent_path()) break;
    }
    return fs::current_path();
}

// -----------------------------------------------------------------------
//  Font pointers
// -----------------------------------------------------------------------
static ImFont* g_fontSmall = nullptr;  // 13px — titlebar, labels
static ImFont* g_fontNormal = nullptr;  // 16px — buttons, UI text
static ImFont* g_fontLarge = nullptr;  // 22px — headers

// -----------------------------------------------------------------------
//  Drag state
// -----------------------------------------------------------------------
static bool   g_dragging = false;
static double g_dragStartX = 0, g_dragStartY = 0;
static int    g_winStartX = 0, g_winStartY = 0;

// -----------------------------------------------------------------------
//  Engine executable and per-module settings
// -----------------------------------------------------------------------
static fs::path resolveEngineExecutable(const fs::path& projectRoot)
{
    // Prefer the engine next to the running launcher. This guarantees that a
    // Release launcher starts the Release engine beside it, while a Debug
    // launcher starts the matching Debug engine.
    const std::vector<fs::path> candidates = {
        executableDirectory() / "HeritageEngine.exe",
        projectRoot / "HeritageEngine.exe",
        projectRoot / "build/bin/HeritageEngine.exe",
        projectRoot / "Engine/HeritageEngine/x64/Release/HeritageEngine.exe",
        projectRoot / "Engine/x64/Release/HeritageEngine.exe",
        projectRoot / "x64/Release/HeritageEngine.exe",
        projectRoot / "Engine/HeritageEngine/x64/Debug/HeritageEngine.exe",
        projectRoot / "Engine/x64/Debug/HeritageEngine.exe",
        projectRoot / "x64/Debug/HeritageEngine.exe"
    };

    for (const fs::path& candidate : candidates)
    {
        if (fs::is_regular_file(candidate))
            return candidate.lexically_normal();
    }

    return {};
}

static std::string quotedArgument(const fs::path& value)
{
    return "\"" + value.string() + "\"";
}

static std::string quotedArgument(const std::string& value)
{
    return "\"" + value + "\"";
}

static std::string moduleLaunchArguments(
    const fs::path& projectRoot,
    const ModuleInfo& module)
{
    return "--project-root " + quotedArgument(projectRoot)
        + " --module-path " + quotedArgument(module.rootPath)
        + " --module " + quotedArgument(module.id);
}

static fs::path getModuleSettingsRoot(
    const fs::path& projectRoot,
    const ModuleInfo& module)
{
    return ModuleLoader::userDataRoot(projectRoot, module);
}

static VideoSettings loadModuleSettings(
    const fs::path& projectRoot,
    const ModuleInfo& module)
{
    VideoSettings settings;
    const fs::path path = getModuleSettingsRoot(projectRoot, module) / "settings_video.ini";

    try
    {
        if (fs::exists(path))
        {
            VideoSettingsStorage::load(path.string(), settings);
        }
        else
        {
            const fs::path legacyPath = projectRoot / "settings_video.ini";
            if (fs::exists(legacyPath))
                VideoSettingsStorage::load(legacyPath.string(), settings);
        }
    }
    catch (...)
    {
        settings = VideoSettings{};
    }

    return settings;
}

static void loadModuleDisplaySettings(
    const fs::path& projectRoot,
    const ModuleInfo& module,
    DisplaySystem& display)
{
    try
    {
        const fs::path path = getModuleSettingsRoot(projectRoot, module)
            / "settings_engine.ini";
        if (fs::exists(path))
        {
            display.load(path.string());
        }
        else
        {
            const fs::path legacyPath = projectRoot / "settings_engine.ini";
            if (fs::exists(legacyPath))
                display.load(legacyPath.string());
        }
    }
    catch (...)
    {
    }
}

static void saveModuleSettings(
    const fs::path& projectRoot,
    const ModuleInfo& module,
    const VideoSettings& settings,
    const DisplaySystem& display)
{
    try
    {
        const fs::path settingsRoot = getModuleSettingsRoot(projectRoot, module);
        fs::create_directories(settingsRoot);
        VideoSettingsStorage::save(
            (settingsRoot / "settings_video.ini").string(),
            settings);
        display.save((settingsRoot / "settings_engine.ini").string());
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Could not save module settings: "
                  << exception.what() << "\n";
    }
}

static int findModuleIndexById(
    const std::vector<ModuleInfo>& modules,
    const std::string& moduleId)
{
    for (int index = 0; index < static_cast<int>(modules.size()); ++index)
    {
        if (modules[index].id == moduleId)
            return index;
    }
    return -1;
}

static void resizeLauncherWindow(
    GLFWwindow* window,
    int desiredWidth,
    int desiredHeight)
{
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (!monitor)
    {
        glfwSetWindowSize(window, desiredWidth, desiredHeight);
        return;
    }

    int workX = 0, workY = 0, workW = desiredWidth, workH = desiredHeight;
    glfwGetMonitorWorkarea(monitor, &workX, &workY, &workW, &workH);

    const int width = (std::min)(desiredWidth, (std::max)(320, workW - 24));
    const int height = (std::min)(desiredHeight, (std::max)(240, workH - 24));

    int windowX = 0, windowY = 0;
    glfwGetWindowPos(window, &windowX, &windowY);

    if (windowX + width > workX + workW)
        windowX = workX + workW - width;
    if (windowY + height > workY + workH)
        windowY = workY + workH - height;
    windowX = (std::max)(windowX, workX);
    windowY = (std::max)(windowY, workY);

    glfwSetWindowSize(window, width, height);
    glfwSetWindowPos(window, windowX, windowY);
}

// -----------------------------------------------------------------------
//  Desktop shortcut
// -----------------------------------------------------------------------
static bool createDesktopShortcut(const ModuleInfo& mod, const fs::path& exePath)
{
#ifdef _WIN32
    HRESULT hr = CoInitialize(nullptr);
    IShellLinkA* pLink = nullptr;
    hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
        IID_IShellLinkA, (void**)&pLink);
    if (SUCCEEDED(hr))
    {
        pLink->SetPath(exePath.string().c_str());
        const fs::path projectRoot = findProjectRoot();
        const std::string arguments = moduleLaunchArguments(projectRoot, mod);
        pLink->SetArguments(arguments.c_str());
        pLink->SetDescription(mod.description.c_str());
        pLink->SetWorkingDirectory(exePath.parent_path().string().c_str());
        IPersistFile* pFile = nullptr;
        hr = pLink->QueryInterface(IID_IPersistFile, (void**)&pFile);
        if (SUCCEEDED(hr))
        {
            char desktopPath[MAX_PATH];
            SHGetFolderPathA(nullptr, CSIDL_DESKTOP, nullptr, 0, desktopPath);
            std::string lnkPath = std::string(desktopPath) + "\\" + mod.name + ".lnk";
            std::wstring wLnk(lnkPath.begin(), lnkPath.end());
            const HRESULT saveResult = pFile->Save(wLnk.c_str(), TRUE);
            pFile->Release();
            pLink->Release();
            CoUninitialize();
            return SUCCEEDED(saveResult);
        }
        pLink->Release();
    }
    CoUninitialize();
    return false;
#else
    (void)mod;
    (void)exePath;
    std::cerr << "Desktop shortcut creation is not implemented on Linux\n";
    return false;
#endif
}

// -----------------------------------------------------------------------
//  ImGui style
// -----------------------------------------------------------------------
static void applyStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f; style.FrameRounding = 4.0f;
    style.ScrollbarRounding = 4.0f; style.GrabRounding = 4.0f;
    style.WindowBorderSize = 0.0f; style.FrameBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(10, 8); style.WindowPadding = ImVec2(16, 16);

    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    c[ImGuiCol_ChildBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    c[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    c[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    c[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    c[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    c[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    c[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
    c[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    c[ImGuiCol_Separator] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    c[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    c[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    c[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    c[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
}

// -----------------------------------------------------------------------
//  Main
// -----------------------------------------------------------------------
int main()
{
    glfwSetErrorCallback([](int code, const char* description) {
        std::cerr << "GLFW error " << code << ": " << description << "\n";
    });

    const int WIN_W = 820;
    const int COLLAPSED_H = 560;
    const int EXPANDED_H = 760;
    const int TITLEBAR_H = 36;
    const double MODULE_RESCAN_INTERVAL = 5.0;

    const fs::path projectRoot = findProjectRoot();
    const std::string fontPath = (projectRoot / "Assets/Fonts/Orbitron-SemiBold.ttf").string();

    const fs::path launcherStatePath = projectRoot / "UserData/launcher.ini";
    LauncherState launcherState;
    LauncherStateStorage::load(launcherStatePath, launcherState);

    heritage::modules::ModuleScanResult scanResult = ModuleLoader::scanDetailed(projectRoot);
    std::vector<ModuleInfo> modules = std::move(scanResult.modules);
    std::vector<std::string> moduleWarnings = std::move(scanResult.warnings);
    int selectedModule = findModuleIndexById(modules, launcherState.lastModuleId);
    if (selectedModule < 0 && !modules.empty())
        selectedModule = 0;

    VideoSettings settings;
    if (selectedModule >= 0)
    {
        settings = loadModuleSettings(projectRoot, modules[selectedModule]);
        launcherState.lastModuleId = modules[selectedModule].id;
        LauncherStateStorage::save(launcherStatePath, launcherState);
    }

    if (!glfwInit()) return -1;

#ifdef _WIN32
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#endif
#ifdef _WIN32
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    const int initialHeight = launcherState.settingsVisible ? EXPANDED_H : COLLAPSED_H;
    GLFWwindow* window = glfwCreateWindow(WIN_W, initialHeight, "Racing United", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    glfwSetWindowPos(window, (mode->width - WIN_W) / 2, (mode->height - initialHeight) / 2);

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    DisplaySystem display;
    display.initialize();
    if (selectedModule >= 0)
        loadModuleDisplaySettings(projectRoot, modules[selectedModule], display);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    // Load Orbitron SemiBold at 3 sizes
    g_fontSmall = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 13.0f);
    g_fontNormal = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f);
    g_fontLarge = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 22.0f);
    if (!g_fontSmall || !g_fontNormal || !g_fontLarge)
    {
        std::cerr << "Warning: Could not load Orbitron font, using default\n";
        g_fontSmall = g_fontNormal = g_fontLarge = io.Fonts->AddFontDefault();
    }

    applyStyle();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(RACING_GLSL_VERSION);

    std::string notification = "";
    double      notifyTime = 0.0;
    bool        showSettings = launcherState.settingsVisible;
    bool        shouldClose = false;
    bool        shouldMin = false;
    bool        notificationIsError = false;
    double      nextModuleScanTime = glfwGetTime() + MODULE_RESCAN_INTERVAL;

    while (!glfwWindowShouldClose(window) && !shouldClose)
    {
        glfwPollEvents();
        if (shouldMin) { glfwIconifyWindow(window); shouldMin = false; }

        const double currentTime = glfwGetTime();
        if (currentTime >= nextModuleScanTime)
        {
            nextModuleScanTime = currentTime + MODULE_RESCAN_INTERVAL;

            const std::string previousId =
                (selectedModule >= 0 && selectedModule < static_cast<int>(modules.size()))
                ? modules[selectedModule].id
                : launcherState.lastModuleId;

            heritage::modules::ModuleScanResult refreshed = ModuleLoader::scanDetailed(projectRoot);
            std::vector<ModuleInfo> refreshedModules = std::move(refreshed.modules);
            moduleWarnings = std::move(refreshed.warnings);

            int refreshedIndex = findModuleIndexById(refreshedModules, previousId);
            if (refreshedIndex < 0 && !refreshedModules.empty())
                refreshedIndex = 0;

            const std::string refreshedId = refreshedIndex >= 0
                ? refreshedModules[refreshedIndex].id
                : std::string{};
            const bool selectionChanged = refreshedId != previousId;

            modules = std::move(refreshedModules);
            selectedModule = refreshedIndex;

            if (selectionChanged && selectedModule >= 0)
            {
                settings = loadModuleSettings(projectRoot, modules[selectedModule]);
                display.shutdown();
                display.initialize();
                loadModuleDisplaySettings(projectRoot, modules[selectedModule], display);
                launcherState.lastModuleId = modules[selectedModule].id;
                LauncherStateStorage::save(launcherStatePath, launcherState);
            }
        }

        int windowW = 0, windowH = 0;
        int framebufferW = 0, framebufferH = 0;
        glfwGetWindowSize(window, &windowW, &windowH);
        glfwGetFramebufferSize(window, &framebufferW, &framebufferH);
        glViewport(0, 0, framebufferW, framebufferH);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Custom title bar
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)windowW, (float)TITLEBAR_H));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.05f, 1.f));
        ImGui::Begin("##titlebar", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings);

        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##drag", ImVec2((float)windowW - 80, (float)TITLEBAR_H));
        if (ImGui::IsItemActive())
        {
            if (!g_dragging) {
                g_dragging = true;
                glfwGetWindowPos(window, &g_winStartX, &g_winStartY);
                double cursorX, cursorY;
                glfwGetCursorPos(window, &cursorX, &cursorY);
                // Store the cursor in screen coordinates. Window-local cursor
                // coordinates change as the window moves and cause feedback.
                g_dragStartX = g_winStartX + cursorX;
                g_dragStartY = g_winStartY + cursorY;
            }
            int currentWindowX, currentWindowY;
            double cx, cy;
            glfwGetWindowPos(window, &currentWindowX, &currentWindowY);
            glfwGetCursorPos(window, &cx, &cy);
            const double cursorScreenX = currentWindowX + cx;
            const double cursorScreenY = currentWindowY + cy;
            glfwSetWindowPos(window,
                g_winStartX + (int)(cursorScreenX - g_dragStartX),
                g_winStartY + (int)(cursorScreenY - g_dragStartY));
        }
        else g_dragging = false;

        ImGui::PushFont(g_fontSmall);
        ImGui::SetCursorPos(ImVec2(12, 11));
        ImGui::TextDisabled("RACING UNITED");
        ImGui::PopFont();

        ImGui::PushFont(g_fontSmall);
        ImGui::SetCursorPos(ImVec2((float)windowW - 76, 4));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.35f, 0.35f, 1));
        if (ImGui::Button(" _ ##min", ImVec2(36, 28))) shouldMin = true;
        ImGui::SetCursorPos(ImVec2((float)windowW - 40, 4));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.1f, 0.1f, 1));
        if (ImGui::Button(" X ##cls", ImVec2(36, 28)))
        {
            shouldClose = true;
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        ImGui::PopStyleColor(4);
        ImGui::PopFont();

        ImGui::End();
        ImGui::PopStyleColor();

        // Main content
        ImGui::SetNextWindowPos(ImVec2(0, (float)TITLEBAR_H));
        ImGui::SetNextWindowSize(ImVec2((float)windowW, (float)(windowH - TITLEBAR_H)));
        ImGui::Begin("##root", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings);

        // Header
        ImGui::PushFont(g_fontLarge);
        ImGui::SetCursorPosY(14);
        ImGui::Text("RACING UNITED");
        ImGui::PopFont();
        ImGui::SameLine(0, 12);
        ImGui::PushFont(g_fontSmall);
        ImGui::SetCursorPosY(22);
        ImGui::TextDisabled("powered by Heritage Engine");
        ImGui::PopFont();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Columns(2, "layout", false);
        ImGui::SetColumnWidth(0, 280);

        // Module list
        ImGui::PushFont(g_fontSmall);
        ImGui::TextDisabled("INSTALLED MODULES");
        ImGui::PopFont();
        ImGui::Spacing();

        if (modules.empty())
        {
            ImGui::PushFont(g_fontSmall);
            ImGui::TextDisabled("No modules found in Modules/");
            ImGui::PopFont();
        }
        else
        {
            ImGui::PushFont(g_fontNormal);
            for (int i = 0;i < (int)modules.size();i++)
            {
                bool selected = (i == selectedModule);
                if (ImGui::Selectable(modules[i].name.c_str(), selected, 0, ImVec2(0, 36)))
                {
                    if (selectedModule != i)
                    {
                        if (selectedModule >= 0)
                            saveModuleSettings(projectRoot, modules[selectedModule], settings, display);
                        selectedModule = i;
                        settings = loadModuleSettings(projectRoot, modules[selectedModule]);
                        display.shutdown();
                        display.initialize();
                        loadModuleDisplaySettings(
                            projectRoot,
                            modules[selectedModule],
                            display);
                        launcherState.lastModuleId = modules[selectedModule].id;
                        LauncherStateStorage::save(launcherStatePath, launcherState);
                    }
                }
            }
            ImGui::PopFont();
        }

        if (!moduleWarnings.empty())
        {
            ImGui::PushFont(g_fontSmall);
            ImGui::PushStyleColor(
                ImGuiCol_Text,
                ImVec4(0.95f, 0.55f, 0.25f, 1.0f));

            const std::string warningLabel = "MODULE WARNINGS: "
                + std::to_string(moduleWarnings.size());
            if (ImGui::Selectable(
                    warningLabel.c_str(),
                    false,
                    ImGuiSelectableFlags_None,
                    ImVec2(0.0f, 20.0f)))
            {
                ImGui::OpenPopup("Module Warnings");
            }

            ImGui::PopStyleColor();
            ImGui::PopFont();

            ImGui::SetNextWindowSize(
                ImVec2(560.0f, 260.0f),
                ImGuiCond_Appearing);
            if (ImGui::BeginPopupModal(
                    "Module Warnings",
                    nullptr,
                    ImGuiWindowFlags_NoResize
                    | ImGuiWindowFlags_NoSavedSettings))
            {
                ImGui::PushFont(g_fontSmall);
                ImGui::TextWrapped(
                    "The launcher found one or more module manifest problems. "
                    "Click Close after reading them, then correct the relevant module.ini file.");
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::BeginChild(
                    "##module_warning_list",
                    ImVec2(0.0f, -42.0f),
                    false,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
                for (const std::string& warning : moduleWarnings)
                {
                    ImGui::Bullet();
                    ImGui::SameLine();
                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 480.0f);
                    ImGui::TextWrapped("%s", warning.c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::Spacing();
                }
                ImGui::EndChild();

                if (ImGui::Button("CLOSE", ImVec2(120.0f, 30.0f)))
                    ImGui::CloseCurrentPopup();

                ImGui::PopFont();
                ImGui::EndPopup();
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool hasModule = selectedModule >= 0;
        if (!hasModule) ImGui::BeginDisabled();

        ImGui::PushFont(g_fontNormal);
        if (ImGui::Button("LAUNCH", ImVec2(120, 36)))
        {
            saveModuleSettings(projectRoot, modules[selectedModule], settings, display);
            const fs::path executablePath = resolveEngineExecutable(projectRoot);
            const std::string arguments = moduleLaunchArguments(
                projectRoot,
                modules[selectedModule]);

            if (executablePath.empty())
            {
                notification = "HeritageEngine.exe was not found in the Racing United package.";
                notificationIsError = true;
                notifyTime = glfwGetTime();
            }
            else
            {
#ifdef _WIN32
                const HINSTANCE launchResult = ShellExecuteA(
                    nullptr,
                    "open",
                    executablePath.string().c_str(),
                    arguments.c_str(),
                    executablePath.parent_path().string().c_str(),
                    SW_SHOWNORMAL);
                if (reinterpret_cast<std::intptr_t>(launchResult) <= 32)
                {
                    notification = "Windows could not start HeritageEngine.exe.";
                    notificationIsError = true;
                    notifyTime = glfwGetTime();
                }
                else
                {
                    shouldClose = true;
                }
#else
                pid_t child = fork();
                if (child == 0)
                {
                    const std::string projectRootArgument = projectRoot.string();
                    const std::string moduleRootArgument = modules[selectedModule].rootPath.string();
                    execl(
                        executablePath.string().c_str(),
                        executablePath.string().c_str(),
                        "--project-root",
                        projectRootArgument.c_str(),
                        "--module-path",
                        moduleRootArgument.c_str(),
                        "--module",
                        modules[selectedModule].id.c_str(),
                        nullptr);
                    _exit(127);
                }
                shouldClose = true;
#endif
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("SHORTCUT", ImVec2(120, 36)))
        {
            const fs::path executablePath = resolveEngineExecutable(projectRoot);
            if (executablePath.empty())
            {
                notification = "HeritageEngine.exe was not found; shortcut was not created.";
                notificationIsError = true;
            }
            else if (createDesktopShortcut(modules[selectedModule], executablePath))
            {
                notification = "Shortcut created for " + modules[selectedModule].name;
                notificationIsError = false;
            }
            else
            {
                notification = "Windows could not create the desktop shortcut.";
                notificationIsError = true;
            }
            notifyTime = glfwGetTime();
        }
        ImGui::PopFont();

        if (!hasModule) ImGui::EndDisabled();
        ImGui::Spacing();

        ImGui::PushFont(g_fontNormal);
        if (ImGui::Button(showSettings ? "HIDE SETTINGS" : "SETTINGS", ImVec2(248, 28)))
        {
            showSettings = !showSettings;
            launcherState.settingsVisible = showSettings;
            LauncherStateStorage::save(launcherStatePath, launcherState);
            resizeLauncherWindow(
                window,
                WIN_W,
                showSettings ? EXPANDED_H : COLLAPSED_H);
        }
        ImGui::PopFont();

        ImGui::NextColumn();

        // Module info
        if (selectedModule >= 0)
        {
            auto& m = modules[selectedModule];
            ImGui::PushFont(g_fontLarge);
            ImGui::Text("%s", m.name.c_str());
            ImGui::PopFont();
            ImGui::PushFont(g_fontSmall);
            ImGui::TextDisabled("v%s  by %s", m.version.c_str(), m.author.c_str());
            ImGui::TextDisabled("Runtime: %s", m.runtime.c_str());
            if (m.runtime == "scripted_ui")
            {
                ImGui::TextDisabled(
                    "Entry UI: %s",
                    m.entryUi.empty() ? "<missing>" : m.entryUi.c_str());
            }
            else if (m.runtime == "lua")
            {
                ImGui::TextDisabled(
                    "Entry script: %s",
                    m.entryScript.empty() ? "<missing>" : m.entryScript.c_str());
            }
            else
            {
                ImGui::TextDisabled(
                    "Entry scene: %s",
                    m.scene.empty() ? "<empty black scene>" : m.scene.c_str());
            }
            ImGui::Spacing();
            ImGui::TextWrapped("%s", m.description.c_str());
            ImGui::PopFont();
        }

        ImGui::Columns(1);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Settings panel. Both the launcher and the engine read/write the
        // same per-module VideoSettings file, so there is no second settings
        // format to drift out of sync.
        if (showSettings && selectedModule >= 0)
        {
            ImGui::PushFont(g_fontNormal);
            ImGui::BeginChild(
                "##module_settings",
                ImVec2(0.0f, (std::max)(220.0f, ImGui::GetContentRegionAvail().y)),
                true,
                ImGuiWindowFlags_AlwaysVerticalScrollbar);

            if (drawSettingsTabs(settings, display))
                saveModuleSettings(projectRoot, modules[selectedModule], settings, display);

            ImGui::EndChild();
            ImGui::PopFont();
        }

        // Notification toast
        if (!notification.empty())
        {
            double elapsed = glfwGetTime() - notifyTime;
            if (elapsed < 3.0)
            {
                ImGui::SetCursorPosY((std::max)(0.0f, ImGui::GetWindowHeight() - 36.0f));
                ImGui::PushFont(g_fontSmall);
                const ImVec4 notificationColor = notificationIsError
                    ? ImVec4(0.95f, 0.35f, 0.30f, 1.0f)
                    : ImVec4(0.60f, 0.90f, 0.60f, 1.0f);
                ImGui::TextColored(notificationColor, "%s", notification.c_str());
                ImGui::PopFont();
            }
            else notification = "";
        }

        ImGui::End();

        ImGui::Render();
        glClearColor(0.08f, 0.08f, 0.08f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    if (selectedModule >= 0)
    {
        saveModuleSettings(projectRoot, modules[selectedModule], settings, display);
        launcherState.lastModuleId = modules[selectedModule].id;
    }
    launcherState.settingsVisible = showSettings;
    LauncherStateStorage::save(launcherStatePath, launcherState);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    display.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

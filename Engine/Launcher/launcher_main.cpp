// Racing United - Launcher
// Powered by Heritage Engine
// Borderless custom window — no Windows chrome.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
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

namespace fs = std::filesystem;

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
//  Module descriptor
// -----------------------------------------------------------------------
struct Module
{
    std::string folderName;
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::string executable;
};

// -----------------------------------------------------------------------
//  INI parser
// -----------------------------------------------------------------------
static std::map<std::string, std::string> parseINI(const std::string& path)
{
    std::map<std::string, std::string> out;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line))
    {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        auto trim = [](std::string& s) {
            size_t a = s.find_first_not_of(" \t\r\n");
            size_t b = s.find_last_not_of(" \t\r\n");
            s = (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
            };
        trim(key); trim(val);
        out[key] = val;
    }
    return out;
}

// -----------------------------------------------------------------------
//  Scan Modules/ folder
// -----------------------------------------------------------------------
static std::vector<Module> scanModules(const std::string& modulesRoot)
{
    std::vector<Module> results;
    if (!fs::exists(modulesRoot)) return results;
    for (auto& entry : fs::directory_iterator(modulesRoot))
    {
        if (!entry.is_directory()) continue;
        std::string iniPath = entry.path().string() + "/module.ini";
        if (!fs::exists(iniPath)) continue;
        auto ini = parseINI(iniPath);
        Module m;
        m.folderName = entry.path().filename().string();
        m.name = ini.count("name") ? ini["name"] : m.folderName;
        m.version = ini.count("version") ? ini["version"] : "?";
        m.author = ini.count("author") ? ini["author"] : "Unknown";
        m.description = ini.count("description") ? ini["description"] : "";
        m.executable = ini.count("executable") ? ini["executable"] : "RacingUnited.exe";
        results.push_back(m);
    }
    return results;
}

// -----------------------------------------------------------------------
//  Desktop shortcut
// -----------------------------------------------------------------------
static void createDesktopShortcut(const Module& mod, const std::string& exePath)
{
    HRESULT hr = CoInitialize(nullptr);
    IShellLinkA* pLink = nullptr;
    hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
        IID_IShellLinkA, (void**)&pLink);
    if (SUCCEEDED(hr))
    {
        pLink->SetPath(exePath.c_str());
        pLink->SetArguments(mod.folderName.c_str());
        pLink->SetDescription(mod.description.c_str());
        pLink->SetWorkingDirectory(fs::path(exePath).parent_path().string().c_str());
        IPersistFile* pFile = nullptr;
        hr = pLink->QueryInterface(IID_IPersistFile, (void**)&pFile);
        if (SUCCEEDED(hr))
        {
            char desktopPath[MAX_PATH];
            SHGetFolderPathA(nullptr, CSIDL_DESKTOP, nullptr, 0, desktopPath);
            std::string lnkPath = std::string(desktopPath) + "\\" + mod.name + ".lnk";
            std::wstring wLnk(lnkPath.begin(), lnkPath.end());
            pFile->Save(wLnk.c_str(), TRUE);
            pFile->Release();
        }
        pLink->Release();
    }
    CoUninitialize();
}

// -----------------------------------------------------------------------
//  Settings
// -----------------------------------------------------------------------
struct Settings
{
    int   resolutionIdx = 2;
    bool  fullscreen = false;
    int   fpsCapIdx = 0;
    float masterVolume = 1.0f;
    float musicVolume = 0.8f;
    float sfxVolume = 1.0f;
    std::string apiName = "OpenGL";
};

static const char* resolutions[] = { "1280x720","1600x900","1920x1080","2560x1440","3840x2160" };
static const char* fpsCaps[] = { "Unlimited","30","60","90","120","144","165","240" };
static const char* apis[] = { "OpenGL" };

static void saveSettings(const Settings& s, const std::string& path)
{
    if (path.empty()) return;
    std::ofstream f(path);
    f << "resolution = " << resolutions[s.resolutionIdx] << "\n";
    f << "fullscreen = " << (s.fullscreen ? "1" : "0") << "\n";
    f << "fps_cap    = " << fpsCaps[s.fpsCapIdx] << "\n";
    f << "master_vol = " << s.masterVolume << "\n";
    f << "music_vol  = " << s.musicVolume << "\n";
    f << "sfx_vol    = " << s.sfxVolume << "\n";
    f << "api        = " << s.apiName << "\n";
}

static Settings loadSettings(const std::string& path)
{
    Settings s;
    if (path.empty() || !fs::exists(path)) return s;
    auto ini = parseINI(path);
    if (ini.count("fullscreen")) s.fullscreen = ini["fullscreen"] == "1";
    if (ini.count("master_vol")) s.masterVolume = std::stof(ini["master_vol"]);
    if (ini.count("music_vol"))  s.musicVolume = std::stof(ini["music_vol"]);
    if (ini.count("sfx_vol"))    s.sfxVolume = std::stof(ini["sfx_vol"]);
    if (ini.count("resolution"))
        for (int i = 0;i < IM_ARRAYSIZE(resolutions);i++)
            if (ini["resolution"] == resolutions[i]) { s.resolutionIdx = i;break; }
    if (ini.count("fps_cap"))
        for (int i = 0;i < IM_ARRAYSIZE(fpsCaps);i++)
            if (ini["fps_cap"] == fpsCaps[i]) { s.fpsCapIdx = i;break; }
    return s;
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
    const int WIN_W = 820;
    const int WIN_H = 560;
    const int TITLEBAR_H = 36;

    std::string modulesRoot = "F:/Racing United/SourceCode/GitHub/RacingUnited/Modules";
    std::string exePath = "F:/Racing United/SourceCode/GitHub/RacingUnited/Engine/HeritageEngine/x64/Debug/HeritageEngine.exe";
    const char* fontPath = "F:/Racing United/SourceCode/GitHub/RacingUnited/Assets/Fonts/Orbitron-SemiBold.ttf";

    std::vector<Module> modules = scanModules(modulesRoot);
    int selectedModule = 0;

    Settings settings;
    auto getSettingsPath = [&]()->std::string {
        if (modules.empty()) return "";
        return modulesRoot + "/" + modules[selectedModule].folderName + "/settings.ini";
        };
    if (!modules.empty()) settings = loadSettings(getSettingsPath());

    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(WIN_W, WIN_H, "Racing United", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    glfwSetWindowPos(window, (mode->width - WIN_W) / 2, (mode->height - WIN_H) / 2);

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    // Load Orbitron SemiBold at 3 sizes
    g_fontSmall = io.Fonts->AddFontFromFileTTF(fontPath, 13.0f);
    g_fontNormal = io.Fonts->AddFontFromFileTTF(fontPath, 16.0f);
    g_fontLarge = io.Fonts->AddFontFromFileTTF(fontPath, 22.0f);
    if (!g_fontSmall || !g_fontNormal || !g_fontLarge)
    {
        std::cerr << "Warning: Could not load Orbitron font, using default\n";
        g_fontSmall = g_fontNormal = g_fontLarge = io.Fonts->AddFontDefault();
    }

    applyStyle();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    std::string notification = "";
    double      notifyTime = 0.0;
    bool        showSettings = false;
    bool        shouldClose = false;
    bool        shouldMin = false;

    while (!glfwWindowShouldClose(window) && !shouldClose)
    {
        glfwPollEvents();
        if (shouldMin) { glfwIconifyWindow(window); shouldMin = false; }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Custom title bar
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)WIN_W, (float)TITLEBAR_H));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.05f, 1.f));
        ImGui::Begin("##titlebar", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings);

        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##drag", ImVec2((float)WIN_W - 80, (float)TITLEBAR_H));
        if (ImGui::IsItemActive())
        {
            if (!g_dragging) {
                g_dragging = true;
                glfwGetCursorPos(window, &g_dragStartX, &g_dragStartY);
                glfwGetWindowPos(window, &g_winStartX, &g_winStartY);
            }
            double cx, cy; glfwGetCursorPos(window, &cx, &cy);
            glfwSetWindowPos(window,
                g_winStartX + (int)(cx - g_dragStartX),
                g_winStartY + (int)(cy - g_dragStartY));
        }
        else g_dragging = false;

        ImGui::PushFont(g_fontSmall);
        ImGui::SetCursorPos(ImVec2(12, 11));
        ImGui::TextDisabled("RACING UNITED");
        ImGui::PopFont();

        ImGui::PushFont(g_fontSmall);
        ImGui::SetCursorPos(ImVec2((float)WIN_W - 76, 4));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.35f, 0.35f, 1));
        if (ImGui::Button(" _ ##min", ImVec2(36, 28))) shouldMin = true;
        ImGui::SetCursorPos(ImVec2((float)WIN_W - 40, 4));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.1f, 0.1f, 1));
        if (ImGui::Button(" X ##cls", ImVec2(36, 28))) shouldClose = true;
        ImGui::PopStyleColor(4);
        ImGui::PopFont();

        ImGui::End();
        ImGui::PopStyleColor();

        // Main content
        ImGui::SetNextWindowPos(ImVec2(0, (float)TITLEBAR_H));
        ImGui::SetNextWindowSize(ImVec2((float)WIN_W, (float)(WIN_H - TITLEBAR_H)));
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
                        saveSettings(settings, getSettingsPath());
                        selectedModule = i;
                        settings = loadSettings(getSettingsPath());
                    }
                }
            }
            ImGui::PopFont();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool hasModule = !modules.empty();
        if (!hasModule) ImGui::BeginDisabled();

        ImGui::PushFont(g_fontNormal);
        if (ImGui::Button("LAUNCH", ImVec2(120, 36)))
        {
            saveSettings(settings, getSettingsPath());
            ShellExecuteA(nullptr, "open", exePath.c_str(),
                modules[selectedModule].folderName.c_str(), nullptr, SW_SHOWNORMAL);
            shouldClose = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("SHORTCUT", ImVec2(120, 36)))
        {
            createDesktopShortcut(modules[selectedModule], exePath);
            notification = "Shortcut created for " + modules[selectedModule].name;
            notifyTime = glfwGetTime();
        }
        ImGui::PopFont();

        if (!hasModule) ImGui::EndDisabled();
        ImGui::Spacing();

        ImGui::PushFont(g_fontNormal);
        if (ImGui::Button(showSettings ? "HIDE SETTINGS" : "SETTINGS", ImVec2(248, 28)))
            showSettings = !showSettings;
        ImGui::PopFont();

        ImGui::NextColumn();

        // Module info
        if (!modules.empty())
        {
            auto& m = modules[selectedModule];
            ImGui::PushFont(g_fontLarge);
            ImGui::Text("%s", m.name.c_str());
            ImGui::PopFont();
            ImGui::PushFont(g_fontSmall);
            ImGui::TextDisabled("v%s  by %s", m.version.c_str(), m.author.c_str());
            ImGui::Spacing();
            ImGui::TextWrapped("%s", m.description.c_str());
            ImGui::PopFont();
        }

        ImGui::Columns(1);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Settings panel
        if (showSettings && !modules.empty())
        {
            ImGui::PushFont(g_fontNormal);
            if (ImGui::BeginTabBar("SettingsTabs"))
            {
                if (ImGui::BeginTabItem("Video"))
                {
                    ImGui::Spacing();
                    ImGui::SetNextItemWidth(200);
                    if (ImGui::Combo("Resolution", &settings.resolutionIdx, resolutions, IM_ARRAYSIZE(resolutions)))
                        saveSettings(settings, getSettingsPath());
                    if (ImGui::Checkbox("Fullscreen", &settings.fullscreen))
                        saveSettings(settings, getSettingsPath());
                    ImGui::SetNextItemWidth(200);
                    if (ImGui::Combo("FPS Cap", &settings.fpsCapIdx, fpsCaps, IM_ARRAYSIZE(fpsCaps)))
                        saveSettings(settings, getSettingsPath());
                    ImGui::SetNextItemWidth(200);
                    int apiIdx = 0;
                    if (ImGui::Combo("Render API", &apiIdx, apis, IM_ARRAYSIZE(apis)))
                        saveSettings(settings, getSettingsPath());
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Audio"))
                {
                    ImGui::Spacing();
                    ImGui::SetNextItemWidth(200);
                    if (ImGui::SliderFloat("Master", &settings.masterVolume, 0.f, 1.f))
                        saveSettings(settings, getSettingsPath());
                    ImGui::SetNextItemWidth(200);
                    if (ImGui::SliderFloat("Music", &settings.musicVolume, 0.f, 1.f))
                        saveSettings(settings, getSettingsPath());
                    ImGui::SetNextItemWidth(200);
                    if (ImGui::SliderFloat("SFX", &settings.sfxVolume, 0.f, 1.f))
                        saveSettings(settings, getSettingsPath());
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Input"))
                {
                    ImGui::Spacing();
                    ImGui::TextDisabled("Input binding will be configured here.");
                    ImGui::TextDisabled("Steering wheel, gamepad, keyboard supported.");
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::PopFont();
        }

        // Notification toast
        if (!notification.empty())
        {
            double elapsed = glfwGetTime() - notifyTime;
            if (elapsed < 3.0)
            {
                ImGui::SetCursorPosY(490);
                ImGui::PushFont(g_fontSmall);
                ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.f), "%s", notification.c_str());
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

    saveSettings(settings, getSettingsPath());
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
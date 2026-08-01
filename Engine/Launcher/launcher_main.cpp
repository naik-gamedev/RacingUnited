// Racing United - Launcher
// Powered by Heritage Engine
// Scans Modules/ folder automatically, no manual registration needed.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
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
#include <windows.h>
#include <shlobj.h>

namespace fs = std::filesystem;

// -----------------------------------------------------------------------
//  Module descriptor  (read from Modules/<name>/module.ini)
// -----------------------------------------------------------------------
struct Module
{
    std::string folderName;   // used as launch argument
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::string executable;   // which exe to launch (default: RacingUnited.exe)
};

// -----------------------------------------------------------------------
//  INI parser  (dead simple: key = value, # comments)
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
        // trim whitespace
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
//  Scan Modules/ folder — returns list of valid modules
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
//  Create desktop shortcut  (Windows only)
// -----------------------------------------------------------------------
static void createDesktopShortcut(const Module& mod, const std::string& exePath)
{
    HRESULT hr = CoInitialize(nullptr);

    IShellLinkA* pLink = nullptr;
    hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
        IID_IShellLinkA, (void**)&pLink);
    if (SUCCEEDED(hr))
    {
        std::string args = mod.folderName;
        pLink->SetPath(exePath.c_str());
        pLink->SetArguments(args.c_str());
        pLink->SetDescription(mod.description.c_str());
        pLink->SetWorkingDirectory(
            fs::path(exePath).parent_path().string().c_str());

        IPersistFile* pFile = nullptr;
        hr = pLink->QueryInterface(IID_IPersistFile, (void**)&pFile);
        if (SUCCEEDED(hr))
        {
            // Get desktop path
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
//  Settings  (shared with the game via settings.ini in module folder)
// -----------------------------------------------------------------------
struct Settings
{
    int  resolutionIdx = 2;       // index into resolutions list
    bool fullscreen = false;
    int  fpsCapIdx = 0;       // 0 = unlimited
    float masterVolume = 1.0f;
    float musicVolume = 0.8f;
    float sfxVolume = 1.0f;
    std::string apiName = "OpenGL";
};

static const char* resolutions[] = {
    "1280x720", "1600x900", "1920x1080", "2560x1440", "3840x2160"
};
static const char* fpsCaps[] = {
    "Unlimited", "30", "60", "90", "120", "144", "165", "240"
};
static const char* apis[] = {
    "OpenGL"  // Vulkan, D3D12 etc added here when supported
};

static void saveSettings(const Settings& s, const std::string& path)
{
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
    if (!fs::exists(path)) return s;
    auto ini = parseINI(path);

    if (ini.count("fullscreen")) s.fullscreen = ini["fullscreen"] == "1";
    if (ini.count("master_vol")) s.masterVolume = std::stof(ini["master_vol"]);
    if (ini.count("music_vol"))  s.musicVolume = std::stof(ini["music_vol"]);
    if (ini.count("sfx_vol"))    s.sfxVolume = std::stof(ini["sfx_vol"]);

    if (ini.count("resolution"))
        for (int i = 0; i < IM_ARRAYSIZE(resolutions); i++)
            if (ini["resolution"] == resolutions[i]) { s.resolutionIdx = i; break; }

    if (ini.count("fps_cap"))
        for (int i = 0; i < IM_ARRAYSIZE(fpsCaps); i++)
            if (ini["fps_cap"] == fpsCaps[i]) { s.fpsCapIdx = i; break; }

    return s;
}

// -----------------------------------------------------------------------
//  ImGui style  — dark, clean, minimal
// -----------------------------------------------------------------------
static void applyStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(10, 8);
    style.WindowPadding = ImVec2(16, 16);

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
    // Paths (relative to exe location)
    std::string modulesRoot = "F:/Racing United/SourceCode/GitHub/RacingUnited/Modules";
    std::string exePath = "../Engine/HeritageEngine/x64/Debug/HeritageEngine.exe";

    // Scan modules
    std::vector<Module> modules = scanModules(modulesRoot);
    int selectedModule = 0;

    // Load settings for first module if any
    Settings settings;
    auto getSettingsPath = [&]() -> std::string {
        if (modules.empty()) return "";
        return modulesRoot + "/" + modules[selectedModule].folderName + "/settings.ini";
        };
    if (!modules.empty())
        settings = loadSettings(getSettingsPath());

    // GLFW init
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(820, 560, "Racing United", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    // ImGui init
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // don't save imgui layout

    applyStyle();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    // Notification state
    std::string notification = "";
    double      notifyTime = 0.0;

    bool showSettings = false;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Full screen window
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(820, 560));
        ImGui::Begin("##root", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar);

        // ---- Header ----
        ImGui::SetCursorPosY(14);
        ImGui::SetWindowFontScale(1.5f);
        ImGui::Text("RACING UNITED");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::SameLine(0, 12);
        ImGui::SetCursorPosY(22);
        ImGui::TextDisabled("powered by Heritage Engine");
        ImGui::Separator();
        ImGui::Spacing();

        // ---- Two column layout ----
        ImGui::Columns(2, "layout", false);
        ImGui::SetColumnWidth(0, 280);

        // ---- Module list ----
        ImGui::TextDisabled("INSTALLED MODULES");
        ImGui::Spacing();

        if (modules.empty())
        {
            ImGui::TextDisabled("No modules found in Modules/");
        }
        else
        {
            for (int i = 0; i < (int)modules.size(); i++)
            {
                bool selected = (i == selectedModule);
                if (ImGui::Selectable(modules[i].name.c_str(), selected,
                    0, ImVec2(0, 36)))
                {
                    if (selectedModule != i)
                    {
                        saveSettings(settings, getSettingsPath());
                        selectedModule = i;
                        settings = loadSettings(getSettingsPath());
                    }
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ---- Launch / Shortcut buttons ----
        bool hasModule = !modules.empty();

        if (!hasModule) ImGui::BeginDisabled();

        if (ImGui::Button("LAUNCH", ImVec2(120, 36)))
        {
            saveSettings(settings, getSettingsPath());
            std::string cmd = "\"" + exePath + "\" " +
                modules[selectedModule].folderName;
            ShellExecuteA(nullptr, "open", exePath.c_str(),
                modules[selectedModule].folderName.c_str(),
                nullptr, SW_SHOWNORMAL);
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        ImGui::SameLine();

        if (ImGui::Button("SHORTCUT", ImVec2(120, 36)))
        {
            createDesktopShortcut(modules[selectedModule], exePath);
            notification = "Shortcut created for " + modules[selectedModule].name;
            notifyTime = glfwGetTime();
        }

        if (!hasModule) ImGui::EndDisabled();

        ImGui::Spacing();

        if (ImGui::Button(showSettings ? "HIDE SETTINGS" : "SETTINGS",
            ImVec2(248, 28)))
            showSettings = !showSettings;

        ImGui::NextColumn();

        // ---- Module info panel ----
        if (!modules.empty())
        {
            auto& m = modules[selectedModule];
            ImGui::SetWindowFontScale(1.2f);
            ImGui::Text("%s", m.name.c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::TextDisabled("v%s  by %s", m.version.c_str(), m.author.c_str());
            ImGui::Spacing();
            ImGui::TextWrapped("%s", m.description.c_str());
        }

        ImGui::Columns(1);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ---- Settings panel ----
        if (showSettings && !modules.empty())
        {
            if (ImGui::BeginTabBar("SettingsTabs"))
            {
                // VIDEO
                if (ImGui::BeginTabItem("Video"))
                {
                    ImGui::Spacing();
                    ImGui::SetNextItemWidth(200);
                    if (ImGui::Combo("Resolution", &settings.resolutionIdx,
                        resolutions, IM_ARRAYSIZE(resolutions)))
                        saveSettings(settings, getSettingsPath());

                    if (ImGui::Checkbox("Fullscreen", &settings.fullscreen))
                        saveSettings(settings, getSettingsPath());

                    ImGui::SetNextItemWidth(200);
                    if (ImGui::Combo("FPS Cap", &settings.fpsCapIdx,
                        fpsCaps, IM_ARRAYSIZE(fpsCaps)))
                        saveSettings(settings, getSettingsPath());

                    ImGui::SetNextItemWidth(200);
                    int apiIdx = 0;
                    if (ImGui::Combo("Render API", &apiIdx,
                        apis, IM_ARRAYSIZE(apis)))
                        saveSettings(settings, getSettingsPath());

                    ImGui::EndTabItem();
                }

                // AUDIO
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

                // INPUT
                if (ImGui::BeginTabItem("Input"))
                {
                    ImGui::Spacing();
                    ImGui::TextDisabled("Input binding will be configured here.");
                    ImGui::TextDisabled("Steering wheel, gamepad, keyboard supported.");
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }

        // ---- Notification toast ----
        if (!notification.empty())
        {
            double elapsed = glfwGetTime() - notifyTime;
            if (elapsed < 3.0)
            {
                ImGui::SetCursorPosY(530);
                ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.f),
                    "%s", notification.c_str());
            }
            else notification = "";
        }

        ImGui::End();

        // Render
        ImGui::Render();
        glClearColor(0.06f, 0.06f, 0.06f, 1.f);
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
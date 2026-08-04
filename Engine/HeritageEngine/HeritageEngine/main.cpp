#include <glad/glad.h>

// Heritage Engine - main.cpp
// Requirements: GLAD (OpenGL 4.6 core), GLFW 3.4, ImGui
//
// ESC          — open/close in-game menu
// F11          — cycle window modes (Windowed → Borderless → Exclusive)
// Mouse drag   — orbit logo
// Scroll       — zoom

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

#include "../Core/Math/Math.hpp"
#include "../Graphics/Mesh.hpp"
#include "../Graphics/AntiAliasing.hpp"
#include "../Graphics/ShaderProgram.hpp"
#include "../Graphics/DisplaySystem.hpp"
#include "../Graphics/WindowSystem.hpp"          // ← new

namespace fs = std::filesystem;
using heritage::math::Mat4;
using heritage::math::Vec3;
using heritage::math::identity;
using heritage::math::perspective;
using heritage::graphics::Mesh;
using heritage::graphics::AntiAliasingSettings;
using heritage::graphics::antiAliasingOptionCount;
using heritage::graphics::antiAliasingOptionNames;
using heritage::graphics::resolveAntiAliasing;
using heritage::graphics::loadObjMesh;
using heritage::graphics::uploadMesh;
using heritage::graphics::buildShaderProgram;
using heritage::graphics::DisplaySystem;
using heritage::graphics::WindowSystem;
using heritage::graphics::WindowMode;

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
        if (fs::exists(candidate / "Assets")) return candidate;
        if (candidate == candidate.parent_path()) break;
    }
    return fs::current_path();
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
static const char* tfOptions[] = { "Nearest","Bilinear","Trilinear","Anisotropic x2","Anisotropic x4","Anisotropic x8","Anisotropic x16" };
static const char* scaleOptions[] = { "Native","Integer x1","Integer x2","Integer x3","Half (50%)","Quarter (25%)" };
static const char* fpsOptions[] = { "Unlimited","30","60","90","120","144","165","240" };
static const char* apiOptions[] = { "OpenGL" };
static const char* wmOptions[] = { "Windowed","Borderless","Exclusive" };

static int g_aaIdx = 2;
static int g_tfIdx = 2;
static int g_scaleIdx = 0;
static int g_resIdx = 2;
static int g_fpsIdx = 0;
static int g_apiIdx = 0;
static int g_wmIdx = 0;

static bool g_displayChangePending = false;
static double g_displayChangeDeadline = 0.0;
static WindowMode g_displayPendingPrevMode = WindowMode::Windowed;
static int g_displayPendingPrevResIdx = 2;
static int g_displayPendingDesiredW = 0;
static int g_displayPendingDesiredH = 0;
static bool g_displayPopupOpened = false;
static constexpr double kDisplayChangeTimeout = 15.0;

static float g_brightness = 0.0f;
static float g_contrast = 1.0f;
static float g_saturation = 1.0f;

static bool g_resolutionWarning = false;
static int g_resolutionRequestedW = 0;
static int g_resolutionRequestedH = 0;

// Systems
static DisplaySystem g_display;
static WindowSystem  g_window;

// -----------------------------------------------------------------------
//  FXAA + Blit shaders
// -----------------------------------------------------------------------
static const char* QUAD_VS = RACING_GLSL_VERSION R"glsl(
out vec2 vUV;
void main()
{
    const vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0));
    vec2 pos = positions[gl_VertexID];
    vUV = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)glsl";

static const char* FXAA_FS = RACING_GLSL_VERSION R"glsl(
in vec2 vUV;
uniform sampler2D uScene;
uniform vec2 uTexelSize;
out vec4 FragColor;
void main()
{
    vec3 rgbNW = texture(uScene, vUV + vec2(-1.0,-1.0)*uTexelSize).rgb;
    vec3 rgbNE = texture(uScene, vUV + vec2( 1.0,-1.0)*uTexelSize).rgb;
    vec3 rgbSW = texture(uScene, vUV + vec2(-1.0, 1.0)*uTexelSize).rgb;
    vec3 rgbSE = texture(uScene, vUV + vec2( 1.0, 1.0)*uTexelSize).rgb;
    vec3 rgbM  = texture(uScene, vUV).rgb;

    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;

    if (lumaRange < max(0.0833, lumaMax * 0.125))
    {
        FragColor = vec4(rgbM, 1.0);
        return;
    }

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW+lumaNE+lumaSW+lumaSE)*0.03125, 0.0078125);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, -8.0, 8.0) * uTexelSize;

    vec3 rgbA = 0.5 * (
        texture(uScene, vUV + dir * (1.0/3.0 - 0.5)).rgb +
        texture(uScene, vUV + dir * (2.0/3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(uScene, vUV + dir * -0.5).rgb +
        texture(uScene, vUV + dir *  0.5).rgb);

    float lumaB = dot(rgbB, luma);
    if (lumaB < lumaMin || lumaB > lumaMax)
        FragColor = vec4(rgbA, 1.0);
    else
        FragColor = vec4(rgbB, 1.0);
}
)glsl";

static const char* BLIT_FS = RACING_GLSL_VERSION R"glsl(
in vec2 vUV;
uniform sampler2D uScene;
uniform bool uNearestNeighbour;
out vec4 FragColor;
void main()
{
    FragColor = texture(uScene, vUV);
}
)glsl";

// -----------------------------------------------------------------------
//  PostFBO
// -----------------------------------------------------------------------
struct PostFBO
{
    GLuint fbo = 0, tex = 0, colorRbo = 0, rbo = 0;
    int w = 0, h = 0;

    void init(int width, int height, int samples = 1)
    {
        destroy();
        w = width; h = height;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        if (samples > 1)
        {
            glGenRenderbuffers(1, &colorRbo);
            glBindRenderbuffer(GL_RENDERBUFFER, colorRbo);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGB8, w, h);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRbo);
        }
        else
        {
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        }

        glGenRenderbuffers(1, &rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        if (samples > 1)
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, w, h);
        else
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "Post-processing framebuffer is incomplete.\n";

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void destroy()
    {
        if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
        if (tex) { glDeleteTextures(1, &tex); tex = 0; }
        if (colorRbo) { glDeleteRenderbuffers(1, &colorRbo); colorRbo = 0; }
        if (rbo) { glDeleteRenderbuffers(1, &rbo); rbo = 0; }
    }
};

// -----------------------------------------------------------------------
//  Scene shaders
// -----------------------------------------------------------------------
static const char* VS = RACING_GLSL_VERSION R"glsl(
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
uniform mat4 uModel, uView, uProj;
out vec3 vNormal, vFragPos;
void main()
{
    vec4 wp  = uModel * vec4(aPos, 1.0);
    vFragPos = wp.xyz;
    vNormal  = mat3(transpose(inverse(uModel))) * aNormal;
    gl_Position = uProj * uView * wp;
}
)glsl";

static const char* FS = RACING_GLSL_VERSION R"glsl(
in vec3 vNormal, vFragPos;
uniform vec3 uLightPos, uViewPos, uColor;
uniform float uBrightness, uContrast, uSaturation;
out vec4 FragColor;
void main()
{
    vec3 norm     = normalize(vNormal);
    vec3 lightDir = normalize(uLightPos - vFragPos);
    vec3 viewDir  = normalize(uViewPos  - vFragPos);
    vec3 halfway  = normalize(lightDir + viewDir);
    float ambient = 0.18;
    float diff    = max(dot(norm, lightDir), 0.0);
    float spec    = pow(max(dot(norm, halfway), 0.0), 64.0) * 0.7;
    float rim     = pow(1.0 - max(dot(norm, viewDir), 0.0), 3.0) * 0.25;
    vec3 col = uColor*(ambient+diff) + vec3(spec) + uColor*rim;
    col = pow(clamp(col,0.0,1.0), vec3(1.0/2.2));
    col = (col - 0.5) * uContrast + 0.5 + uBrightness;
    float luminance = dot(col, vec3(0.2126, 0.7152, 0.0722));
    col = mix(vec3(luminance), col, uSaturation);
    FragColor = vec4(col, 1.0);
}
)glsl";

static void applyTextureFiltering(GLenum target)
{
    switch (g_tfIdx)
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
        switch (g_tfIdx) {
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

static void getRenderSize(int fbW, int fbH, int& rW, int& rH)
{
    switch (g_scaleIdx)
    {
    case 0: case 1: rW = fbW; rH = fbH; break;
    case 2: rW = fbW / 2; rH = fbH / 2; break;
    case 3: rW = fbW / 3; rH = fbH / 3; break;
    case 4: rW = fbW / 2; rH = fbH / 2; break;
    case 5: rW = fbW / 4; rH = fbH / 4; break;
    default: rW = fbW; rH = fbH; break;
    }
    rW = std::max(rW, 1); rH = std::max(rH, 1);
}

// -----------------------------------------------------------------------
//  Mouse orbit
// -----------------------------------------------------------------------
static float  g_orbitX = 0.3f, g_orbitY = 0.f, g_zoom = 4.5f;
static bool   g_drag = false;
static double g_lastX = 0, g_lastY = 0;

static void mouseButtonCB(GLFWwindow* w, int btn, int action, int mods)
{
    ImGui_ImplGlfw_MouseButtonCallback(w, btn, action, mods);
    if (ImGui::GetIO().WantCaptureMouse) return;
    if (btn == GLFW_MOUSE_BUTTON_LEFT) g_drag = (action == GLFW_PRESS);
}

static void cursorPosCB(GLFWwindow* w, double x, double y)
{
    ImGui_ImplGlfw_CursorPosCallback(w, x, y);
    if (!ImGui::GetIO().WantCaptureMouse && g_drag)
    {
        g_orbitY += (float)(x - g_lastX) * 0.01f;
        g_orbitX += (float)(y - g_lastY) * 0.01f;
        g_orbitX = fmaxf(-1.4f, fminf(1.4f, g_orbitX));
    }
    g_lastX = x; g_lastY = y;
}

static void scrollCB(GLFWwindow* w, double dx, double dy)
{
    ImGui_ImplGlfw_ScrollCallback(w, dx, dy);
    if (ImGui::GetIO().WantCaptureMouse) return;
    g_zoom -= (float)dy * 0.3f;
    g_zoom = fmaxf(2.f, fminf(12.f, g_zoom));
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

static void initiateDisplayChange(GLFWwindow* window, WindowMode newMode, int desiredW, int desiredH, int newResIdx)
{
    g_displayPendingPrevMode = g_window.mode();
    g_displayPendingPrevResIdx = g_resIdx;
    g_displayPendingDesiredW = desiredW;
    g_displayPendingDesiredH = desiredH;

    g_window.setMode(window, newMode, desiredW, desiredH);

    if (newMode == WindowMode::Windowed) g_wmIdx = 0;
    else if (newMode == WindowMode::Borderless) g_wmIdx = 1;
    else g_wmIdx = 2;

    if (newResIdx >= 0) g_resIdx = newResIdx;

    g_displayChangePending = true;
    g_displayChangeDeadline = glfwGetTime() + kDisplayChangeTimeout;
    g_displayPopupOpened = true;
}

// -----------------------------------------------------------------------
//  Main
// -----------------------------------------------------------------------
int main()
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

    GLFWmonitor* mon = glfwGetPrimaryMonitor();
    const GLFWvidmode* vid = glfwGetVideoMode(mon);

    int startW = 1280, startH = 720;
    int startX = (vid->width - startW) / 2;
    int startY = (vid->height - startH) / 2;

    GLFWwindow* window = glfwCreateWindow(startW, startH, "Heritage Engine", nullptr, nullptr);
    if (!window) { std::cerr << "Window failed\n"; glfwTerminate(); return -1; }
    glfwSetWindowPos(window, startX, startY);

#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(window);
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
#endif

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "GLAD failed\n"; return -1; }
    std::cout << "OpenGL " << glGetString(GL_VERSION) << "\n";

    // Systems
    g_window.initialize(window);
    g_display.initialize();
    try {
        std::string settingsPath = (findProjectRoot() / "settings_engine.ini").string();
        if (fs::exists(settingsPath)) g_display.load(settingsPath);
    }
    catch (...) {}

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    const std::string fontPath = (findProjectRoot() / "Assets/Fonts/Orbitron-SemiBold.ttf").string();
    g_fontSmall = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 13.0f);
    g_fontNormal = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f);
    g_fontLarge = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 22.0f);
    if (!g_fontSmall || !g_fontNormal || !g_fontLarge)
        g_fontSmall = g_fontNormal = g_fontLarge = io.Fonts->AddFontDefault();

    applyStyle();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(RACING_GLSL_VERSION);

    glfwSetMouseButtonCallback(window, mouseButtonCB);
    glfwSetCursorPosCallback(window, cursorPosCB);
    glfwSetScrollCallback(window, scrollCB);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_CULL_FACE);

    GLuint sceneProg = buildShaderProgram(VS, FS);
    GLuint fxaaProg = buildShaderProgram(QUAD_VS, FXAA_FS);
    GLuint blitProg = buildShaderProgram(QUAD_VS, BLIT_FS);

    GLuint quadVAO;
    glGenVertexArrays(1, &quadVAO);

    Mesh logo = loadObjMesh((findProjectRoot() / "Assets/RacingUnited_3D_Logo.obj").string());
    uploadMesh(logo);

    PostFBO msaaFBO, resolveFBO, scaleFBO;

    double prevTime = glfwGetTime();
    float  autoYaw = 0.f;
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
        glfwPollEvents();

        if (shouldMin) { glfwIconifyWindow(window); shouldMin = false; }

        // F11
        bool f11Now = (glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS);
        if (f11Now && !f11Prev) {
            g_window.cycleMode(window);
            // keep UI index in sync
            if (g_window.mode() == WindowMode::Windowed) g_wmIdx = 0;
            else if (g_window.mode() == WindowMode::Borderless) g_wmIdx = 1;
            else g_wmIdx = 2;
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

        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        if (fbW == 0 || fbH == 0) { glfwSwapBuffers(window); continue; }

        int rW, rH;
        getRenderSize(fbW, fbH, rW, rH);

        if (fbW != prevFbW || fbH != prevFbH || g_aaIdx != prevAA || g_scaleIdx != prevScale)
        {
            const AntiAliasingSettings antiAliasing = resolveAntiAliasing(g_aaIdx);
            if (antiAliasing.msaaSamples > 1)
                msaaFBO.init(rW, rH, antiAliasing.msaaSamples);
            resolveFBO.init(rW, rH, 1);
            if (rW != fbW || rH != fbH)
                scaleFBO.init(fbW, fbH, 1);
            prevFbW = fbW; prevFbH = fbH;
            prevAA = g_aaIdx; prevScale = g_scaleIdx;
        }

        g_display.updateSpanFBO();

        double now = glfwGetTime();
        float dt = (float)(now - prevTime); prevTime = now;
        if (!g_drag && !menuOpen) autoYaw += dt * 0.6f;

        const AntiAliasingSettings antiAliasing = resolveAntiAliasing(g_aaIdx);
        bool needMSAA = antiAliasing.msaaSamples > 1;
        bool needFXAA = antiAliasing.useFxaa;
        if (needMSAA) glEnable(GL_MULTISAMPLE);
        else glDisable(GL_MULTISAMPLE);
        bool needScale = (g_scaleIdx > 0) && (rW != fbW || rH != fbH);
        bool nearestUp = (g_scaleIdx >= 1 && g_scaleIdx <= 3);

        // Camera
        float camX = sinf(g_orbitY + autoYaw) * cosf(g_orbitX) * g_zoom;
        float camY = sinf(g_orbitX) * g_zoom;
        float camZ = cosf(g_orbitY + autoYaw) * cosf(g_orbitX) * g_zoom;

        Vec3 eye{ camX,camY,camZ }, up{ 0,1,0 };
        Vec3 fwd{ -camX,-camY,-camZ };
        float fl = sqrtf(fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z);
        fwd = { fwd.x / fl,fwd.y / fl,fwd.z / fl };
        Vec3 right{ fwd.y * up.z - fwd.z * up.y, fwd.z * up.x - fwd.x * up.z, fwd.x * up.y - fwd.y * up.x };
        float rl = sqrtf(right.x * right.x + right.y * right.y + right.z * right.z);
        right = { right.x / rl,right.y / rl,right.z / rl };
        Vec3 vup{ right.y * fwd.z - right.z * fwd.y, right.z * fwd.x - right.x * fwd.z, right.x * fwd.y - right.y * fwd.x };

        Mat4 view;
        view.m[0] = right.x; view.m[4] = right.y; view.m[8] = right.z;
        view.m[1] = vup.x;   view.m[5] = vup.y;   view.m[9] = vup.z;
        view.m[2] = -fwd.x;  view.m[6] = -fwd.y;  view.m[10] = -fwd.z;
        view.m[15] = 1.f;
        view.m[12] = -(right.x * eye.x + right.y * eye.y + right.z * eye.z);
        view.m[13] = -(vup.x * eye.x + vup.y * eye.y + vup.z * eye.z);
        view.m[14] = (fwd.x * eye.x + fwd.y * eye.y + fwd.z * eye.z);

        Mat4 model = identity();

        // ========== RENDER ==========
        bool spanning = g_display.isSpanning() && g_display.spanFBO() != 0;

        if (spanning)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, g_display.spanFBO());
            glViewport(0, 0, g_display.spanWidth(), g_display.spanHeight());
            glClearColor(0.f, 0.f, 0.f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_SCISSOR_TEST);
            glEnable(GL_DEPTH_TEST);
            glUseProgram(sceneProg);

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

                glUniformMatrix4fv(glGetUniformLocation(sceneProg, "uModel"), 1, GL_FALSE, model.m);
                glUniformMatrix4fv(glGetUniformLocation(sceneProg, "uView"), 1, GL_FALSE, view.m);
                glUniformMatrix4fv(glGetUniformLocation(sceneProg, "uProj"), 1, GL_FALSE, projOff.m);
                glUniform3f(glGetUniformLocation(sceneProg, "uLightPos"), 4.f, 6.f, 5.f);
                glUniform3f(glGetUniformLocation(sceneProg, "uViewPos"), eye.x, eye.y, eye.z);
                glUniform3f(glGetUniformLocation(sceneProg, "uColor"), 1.f, 1.f, 1.f);
                glUniform1f(glGetUniformLocation(sceneProg, "uBrightness"), g_brightness);
                glUniform1f(glGetUniformLocation(sceneProg, "uContrast"), g_contrast);
                glUniform1f(glGetUniformLocation(sceneProg, "uSaturation"), g_saturation);

                glBindVertexArray(logo.vao);
                glDrawElements(GL_TRIANGLES, (GLsizei)logo.indices.size(), GL_UNSIGNED_INT, nullptr);
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
            else if (needFXAA && resolveFBO.fbo)
                glBindFramebuffer(GL_FRAMEBUFFER, resolveFBO.fbo);
            else
                glBindFramebuffer(GL_FRAMEBUFFER, 0);

            glViewport(0, 0, rW, rH);
            glClearColor(0.f, 0.f, 0.f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);
            glUseProgram(sceneProg);

            Mat4 proj = perspective(0.6f, (float)rW / (float)rH, 0.1f, 100.f);

            glUniformMatrix4fv(glGetUniformLocation(sceneProg, "uModel"), 1, GL_FALSE, model.m);
            glUniformMatrix4fv(glGetUniformLocation(sceneProg, "uView"), 1, GL_FALSE, view.m);
            glUniformMatrix4fv(glGetUniformLocation(sceneProg, "uProj"), 1, GL_FALSE, proj.m);
            glUniform3f(glGetUniformLocation(sceneProg, "uLightPos"), 4.f, 6.f, 5.f);
            glUniform3f(glGetUniformLocation(sceneProg, "uViewPos"), eye.x, eye.y, eye.z);
            glUniform3f(glGetUniformLocation(sceneProg, "uColor"), 1.f, 1.f, 1.f);
            glUniform1f(glGetUniformLocation(sceneProg, "uBrightness"), g_brightness);
            glUniform1f(glGetUniformLocation(sceneProg, "uContrast"), g_contrast);
            glUniform1f(glGetUniformLocation(sceneProg, "uSaturation"), g_saturation);

            glBindVertexArray(logo.vao);
            glDrawElements(GL_TRIANGLES, (GLsizei)logo.indices.size(), GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);

            if (needMSAA && msaaFBO.fbo && resolveFBO.fbo)
            {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFBO.fbo);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFBO.fbo);
                glBlitFramebuffer(0, 0, rW, rH, 0, 0, rW, rH, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            }

            if (needMSAA && !needFXAA && !needScale && resolveFBO.fbo)
            {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, resolveFBO.fbo);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                glBlitFramebuffer(0, 0, rW, rH, 0, 0, fbW, fbH, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            }

            if (needFXAA && resolveFBO.fbo)
            {
                if (needScale && scaleFBO.fbo)
                    glBindFramebuffer(GL_FRAMEBUFFER, scaleFBO.fbo);
                else
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);

                glViewport(0, 0, fbW, fbH);
                glDisable(GL_DEPTH_TEST);
                glUseProgram(fxaaProg);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, resolveFBO.tex);
                glUniform1i(glGetUniformLocation(fxaaProg, "uScene"), 0);
                glUniform2f(glGetUniformLocation(fxaaProg, "uTexelSize"), 1.f / (float)rW, 1.f / (float)rH);
                glBindVertexArray(quadVAO);
                glDrawArrays(GL_TRIANGLES, 0, 3);
            }

            if (needScale && !needFXAA && resolveFBO.fbo)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glViewport(0, 0, fbW, fbH);
                glDisable(GL_DEPTH_TEST);
                glUseProgram(blitProg);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, resolveFBO.tex);
                GLenum filter = nearestUp ? GL_NEAREST : GL_LINEAR;
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
                glUniform1i(glGetUniformLocation(blitProg, "uScene"), 0);
                glUniform1i(glGetUniformLocation(blitProg, "uNearestNeighbour"), nearestUp ? 1 : 0);
                glBindVertexArray(quadVAO);
                glDrawArrays(GL_TRIANGLES, 0, 3);
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

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
            {
                g_window.setMode(window, g_displayPendingPrevMode);
                g_resIdx = g_displayPendingPrevResIdx;
                if (g_displayPendingPrevMode == WindowMode::Windowed) g_wmIdx = 0;
                else if (g_displayPendingPrevMode == WindowMode::Borderless) g_wmIdx = 1;
                else g_wmIdx = 2;
                g_displayChangePending = false;
            }

            if (ImGui::BeginPopupModal("Display Change", nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
            {
                const char* modeStr = (g_window.mode() == WindowMode::Windowed) ? "Windowed" :
                    (g_window.mode() == WindowMode::Borderless) ? "Borderless" : "Exclusive";
                ImGui::Text("Switched to %s mode.", modeStr);
                ImGui::Spacing();
                ImGui::Text("Resolution: %d x %d", g_displayPendingDesiredW > 0 ? g_displayPendingDesiredW : fbW,
                    g_displayPendingDesiredH > 0 ? g_displayPendingDesiredH : fbH);
                ImGui::Spacing();
                double remaining = g_displayChangeDeadline - nowt;
                if (remaining < 0) remaining = 0;
                ImGui::Text("Keep these display settings? Reverting in %.0f seconds...", remaining);
                ImGui::Spacing();
                if (ImGui::Button("Keep", ImVec2(120, 0)))
                {
                    g_displayChangePending = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Revert", ImVec2(120, 0)))
                {
                    g_window.setMode(window, g_displayPendingPrevMode);
                    g_resIdx = g_displayPendingPrevResIdx;
                    if (g_displayPendingPrevMode == WindowMode::Windowed) g_wmIdx = 0;
                    else if (g_displayPendingPrevMode == WindowMode::Borderless) g_wmIdx = 1;
                    else g_wmIdx = 2;
                    g_displayChangePending = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        // Titlebar + Resize (now clean)
        g_window.drawTitlebar(window, fbW, fbH, shouldClose, shouldMin);
        g_window.drawResizeHandles(window);

        // In-game menu (kept as-is for now)
        if (menuOpen)
        {
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2((float)fbW, (float)fbH));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.55f));
            ImGui::Begin("##dim", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs);
            ImGui::End();
            ImGui::PopStyleColor();

            float mw = 420, mh = menuShowSettings ? 620.f : 180.f;
            ImGui::SetNextWindowPos(ImVec2((fbW - mw) * 0.5f, (fbH - mh) * 0.5f));
            ImGui::SetNextWindowSize(ImVec2(mw, mh));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.07f, 0.07f, 0.98f));
            ImGui::Begin("##menu", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoSavedSettings);

            ImGui::PushFont(g_fontLarge);
            float tw = ImGui::CalcTextSize("HERITAGE ENGINE").x;
            ImGui::SetCursorPosX((mw - tw) * 0.5f);
            ImGui::SetCursorPosY(16);
            ImGui::Text("HERITAGE ENGINE");
            ImGui::PopFont();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushFont(g_fontNormal);
            float bw = 200, bh = 38;
            ImGui::SetCursorPosX((mw - bw) * 0.5f);
            if (ImGui::Button(menuShowSettings ? "HIDE SETTINGS" : "SETTINGS", ImVec2(bw, bh)))
                menuShowSettings = !menuShowSettings;

            ImGui::Spacing();
            ImGui::SetCursorPosX((mw - bw) * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.08f, 0.08f, 1));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.10f, 0.10f, 1));
            if (ImGui::Button("EXIT", ImVec2(bw, bh)))
            {
                shouldClose = true;
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
            ImGui::PopStyleColor(2);
            ImGui::PopFont();

            if (menuShowSettings)
            {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::PushFont(g_fontNormal);
                if (ImGui::BeginTabBar("MenuSettings"))
                {
                    if (ImGui::BeginTabItem("Video"))
                    {
                        ImGui::Spacing();

                        // ===== Multi-monitor / Span =====
                        ImGui::Checkbox("Span all monitors", &g_display.spanAllMonitors);
                        ImGui::Spacing();

                        if (g_display.spanAllMonitors)
                        {
                            ImGui::TextDisabled("Resolution is locked while spanning.");
                            ImGui::TextDisabled("(This is how most racing sims handle triple-monitor)");
                            ImGui::Spacing();
                        }

                        ImGui::TextDisabled("Monitors:");
                        for (size_t mi = 0; mi < g_display.monitors().size(); ++mi)
                        {
                            ImGui::PushID((int)mi);
                            bool sel = g_display.selected[mi];
                            if (ImGui::Checkbox(g_display.monitors()[mi].name.c_str(), &sel))
                                g_display.selected[mi] = sel;
                            ImGui::SameLine();
                            ImGui::TextDisabled("%dx%d", g_display.monitors()[mi].width, g_display.monitors()[mi].height);

                            ImGui::SetNextItemWidth(120);
                            if (ImGui::InputInt("Bezel mm", &g_display.bezelMm[mi], 1, 10))
                            {
                                if (g_display.bezelMm[mi] < 0) g_display.bezelMm[mi] = 0;
                                if (g_display.bezelMm[mi] > 100) g_display.bezelMm[mi] = 100;
                            }
                            ImGui::PopID();
                        }

                        ImGui::Spacing();
                        ImGui::SetNextItemWidth(220);
                        ImGui::InputFloat("Eye distance (cm)", &g_display.eyeDistanceCm, 1.0f, 5.0f, "%.1f");
                        ImGui::SetNextItemWidth(220);
                        ImGui::InputInt("Global Bezel (mm)", &g_display.globalBezelMm);

                        if (g_display.isSpanning())
                        {
                            float hfov = g_display.getCombinedHFOVDegrees();
                            ImGui::TextDisabled("Combined HFOV: %.1f°", hfov);
                        }

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();

                        // Window Mode (always available)
                        ImGui::SetNextItemWidth(220);
                        if (ImGui::Combo("Window Mode", &g_wmIdx, wmOptions, IM_ARRAYSIZE(wmOptions)))
                        {
                            WindowMode nm = WindowMode::Windowed;
                            if (g_wmIdx == 1) nm = WindowMode::Borderless;
                            if (g_wmIdx == 2) nm = WindowMode::Exclusive;

                            if (nm != g_window.mode())
                            {
                                if (g_window.mode() == WindowMode::Windowed)
                                    g_window.saveCurrentRect(window);

                                if (nm == WindowMode::Exclusive)
                                    initiateDisplayChange(window, nm, vid->width, vid->height, g_resIdx);
                                else
                                    initiateDisplayChange(window, nm, 0, 0, -1);
                            }
                        }

                        // Resolution only when NOT spanning
                        if (!g_display.spanAllMonitors)
                        {
                            ImGui::SetNextItemWidth(220);
                            // For now we keep a simple native resolution fallback.
                            // (We can make a full per-monitor mode list later if you want)
                            ImGui::TextDisabled("Resolution: Native (desktop resolution)");
                            ImGui::TextDisabled("(Full resolution list coming in next clean-up)");
                        }

                        ImGui::Spacing();
                        ImGui::SetNextItemWidth(220);
                        ImGui::Combo("Anti-Aliasing", &g_aaIdx, antiAliasingOptionNames(), antiAliasingOptionCount());
                        ImGui::SliderFloat("Brightness", &g_brightness, -0.50f, 0.50f, "%.2f");
                        ImGui::SliderFloat("Contrast", &g_contrast, 0.50f, 1.50f, "%.2f");
                        ImGui::SliderFloat("Saturation", &g_saturation, 0.00f, 2.00f, "%.2f");
                        ImGui::SetNextItemWidth(220);
                        ImGui::Combo("Texture Filter", &g_tfIdx, tfOptions, IM_ARRAYSIZE(tfOptions));
                        ImGui::SetNextItemWidth(220);
                        ImGui::Combo("Scale Mode", &g_scaleIdx, scaleOptions, IM_ARRAYSIZE(scaleOptions));
                        ImGui::SetNextItemWidth(220);
                        ImGui::Combo("FPS Cap", &g_fpsIdx, fpsOptions, IM_ARRAYSIZE(fpsOptions));
                        ImGui::SetNextItemWidth(220);
                        ImGui::Combo("Render API", &g_apiIdx, apiOptions, IM_ARRAYSIZE(apiOptions));

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
                        ImGui::TextDisabled("Input binding coming soon.");
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
                ImGui::PopFont();
            }

            ImGui::End();
            ImGui::PopStyleColor();
        }

        ImGui::Render();
        glDisable(GL_DEPTH_TEST);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    g_display.shutdown();
    g_window.shutdown();
    msaaFBO.destroy();
    resolveFBO.destroy();
    scaleFBO.destroy();
    glDeleteVertexArrays(1, &quadVAO);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &logo.vao);
    glDeleteBuffers(1, &logo.vbo);
    glDeleteBuffers(1, &logo.ebo);
    glDeleteProgram(sceneProg);
    glDeleteProgram(fxaaProg);
    glDeleteProgram(blitProg);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
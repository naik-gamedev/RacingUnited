// Heritage Engine - main.cpp
// Requirements: GLAD (OpenGL 4.6 core), GLFW 3.4, ImGui
//
// F11          — cycle window modes (Windowed → Borderless → Exclusive)
// ESC          — close
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
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#ifdef _WIN32
#include <GLFW/glfw3native.h>
#endif
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
#include <filesystem>

namespace fs = std::filesystem;

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
//  Font pointers (loaded once, used everywhere)
// -----------------------------------------------------------------------
static ImFont* g_fontSmall = nullptr;  // 13px — titlebar, labels
static ImFont* g_fontNormal = nullptr;  // 16px — buttons, UI text
static ImFont* g_fontLarge = nullptr;  // 22px — headers

// -----------------------------------------------------------------------
//  Window mode
// -----------------------------------------------------------------------
enum class WindowMode { Windowed, Borderless, Exclusive };
static WindowMode g_windowMode = WindowMode::Windowed;
static int g_savedX = 100, g_savedY = 100, g_savedW = 1280, g_savedH = 720;
static const int TITLEBAR_H = 28;

static bool   g_winDragging = false;
static double g_winDragStartX = 0, g_winDragStartY = 0;
static int    g_winStartX = 0, g_winStartY = 0;

static void applyWindowMode(GLFWwindow* window, WindowMode mode)
{
    GLFWmonitor* mon = glfwGetPrimaryMonitor();
    const GLFWvidmode* vid = glfwGetVideoMode(mon);
    if (mode == WindowMode::Windowed)
        glfwSetWindowMonitor(window, nullptr, g_savedX, g_savedY, g_savedW, g_savedH, GLFW_DONT_CARE);
    else if (mode == WindowMode::Borderless)
        glfwSetWindowMonitor(window, nullptr, 0, 0, vid->width, vid->height, GLFW_DONT_CARE);
    else
        glfwSetWindowMonitor(window, mon, 0, 0, vid->width, vid->height, vid->refreshRate);
    g_windowMode = mode;
}

// -----------------------------------------------------------------------
//  Math helpers
// -----------------------------------------------------------------------
struct Vec3 { float x, y, z; };
struct Mat4 { float m[16] = {}; };

static Mat4 identity()
{
    Mat4 r; r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.f; return r;
}

static Mat4 perspective(float fovY, float aspect, float zNear, float zFar)
{
    float f = 1.f / tanf(fovY * 0.5f);
    Mat4 r;
    r.m[0] = f / aspect; r.m[5] = f;
    r.m[10] = (zFar + zNear) / (zNear - zFar);
    r.m[11] = -1.f;
    r.m[14] = (2.f * zFar * zNear) / (zNear - zFar);
    return r;
}

// -----------------------------------------------------------------------
//  Shaders
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
    FragColor = vec4(col, 1.0);
}
)glsl";

// -----------------------------------------------------------------------
//  Mesh + OBJ loader
// -----------------------------------------------------------------------
struct Mesh
{
    std::vector<float>    verts;
    std::vector<unsigned> idx;
    GLuint vao = 0, vbo = 0, ebo = 0;
};

static Mesh loadOBJ(const std::string& path)
{
    Mesh m;
    std::vector<std::array<float, 3>> positions, normals;
    std::map<std::pair<int, int>, unsigned> cache;

    std::ifstream f(path);
    if (!f.is_open()) { std::cerr << "Could not open OBJ: " << path << "\n"; return m; }

    std::string line;
    while (std::getline(f, line))
    {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string token; ss >> token;

        if (token == "v") { float x, y, z; ss >> x >> y >> z; positions.push_back({ x,y,z }); }
        else if (token == "vn") { float x, y, z; ss >> x >> y >> z; normals.push_back({ x,y,z }); }
        else if (token == "f")
        {
            std::vector<std::pair<int, int>> face;
            std::string chunk;
            while (ss >> chunk)
            {
                int pi = 0, ni = 0;
                size_t s1 = chunk.find('/');
                if (s1 == std::string::npos) { pi = std::stoi(chunk); }
                else
                {
                    pi = std::stoi(chunk.substr(0, s1));
                    size_t s2 = chunk.find('/', s1 + 1);
                    if (s2 != std::string::npos) ni = std::stoi(chunk.substr(s2 + 1));
                }
                if (pi < 0) pi = (int)positions.size() + pi + 1;
                if (ni < 0) ni = (int)normals.size() + ni + 1;
                face.push_back({ pi,ni });
            }
            for (int i = 1;i + 1 < (int)face.size();i++)
            {
                for (auto& fp : { face[0],face[i],face[i + 1] })
                {
                    auto it = cache.find(fp);
                    if (it != cache.end()) { m.idx.push_back(it->second); continue; }
                    unsigned newIdx = (unsigned)(m.verts.size() / 6);
                    cache[fp] = newIdx;
                    auto& p = positions[fp.first - 1];
                    m.verts.push_back(p[0]); m.verts.push_back(p[1]); m.verts.push_back(p[2]);
                    int ni = fp.second - 1;
                    if (ni >= 0 && ni < (int)normals.size())
                    {
                        auto& n = normals[ni]; m.verts.push_back(n[0]); m.verts.push_back(n[1]); m.verts.push_back(n[2]);
                    }
                    else { m.verts.push_back(0); m.verts.push_back(1); m.verts.push_back(0); }
                    m.idx.push_back(newIdx);
                }
            }
        }
    }

    if (!m.verts.empty())
    {
        float minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9, minZ = 1e9, maxZ = -1e9;
        for (int i = 0;i < (int)m.verts.size();i += 6)
        {
            minX = fminf(minX, m.verts[i]);   maxX = fmaxf(maxX, m.verts[i]);
            minY = fminf(minY, m.verts[i + 1]); maxY = fmaxf(maxY, m.verts[i + 1]);
            minZ = fminf(minZ, m.verts[i + 2]); maxZ = fmaxf(maxZ, m.verts[i + 2]);
        }
        float cx = (minX + maxX) * .5f, cy = (minY + maxY) * .5f, cz = (minZ + maxZ) * .5f;
        float scale = 2.f / fmaxf(fmaxf(maxX - minX, maxY - minY), maxZ - minZ);
        for (int i = 0;i < (int)m.verts.size();i += 6)
        {
            m.verts[i] = (m.verts[i] - cx) * scale; m.verts[i + 1] = (m.verts[i + 1] - cy) * scale; m.verts[i + 2] = (m.verts[i + 2] - cz) * scale;
        }
    }
    std::cout << "OBJ loaded: " << m.idx.size() / 3 << " triangles\n";
    return m;
}

static void uploadMesh(Mesh& m)
{
    glGenVertexArrays(1, &m.vao); glGenBuffers(1, &m.vbo); glGenBuffers(1, &m.ebo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, m.verts.size() * sizeof(float), m.verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m.idx.size() * sizeof(unsigned), m.idx.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

static GLuint compileShader(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type); glShaderSource(s, 1, &src, nullptr); glCompileShader(s);
    int ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[512]; glGetShaderInfoLog(s, 512, nullptr, log); std::cerr << "Shader:\n" << log << "\n"; }
    return s;
}

static GLuint buildProgram()
{
    GLuint vs = compileShader(GL_VERTEX_SHADER, VS);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, FS);
    GLuint p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs); glLinkProgram(p);
    int ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char log[512]; glGetProgramInfoLog(p, 512, nullptr, log); std::cerr << "Link:\n" << log << "\n"; }
    glDeleteShader(vs); glDeleteShader(fs);
    return p;
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

// -----------------------------------------------------------------------
//  ImGui style
// -----------------------------------------------------------------------
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

// -----------------------------------------------------------------------
//  Main
// -----------------------------------------------------------------------
int main()
{
    glfwSetErrorCallback([](int code, const char* description) {
        std::cerr << "GLFW error " << code << ": " << description << "\n";
    });

    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return -1; }

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
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    GLFWmonitor* mon = glfwGetPrimaryMonitor();
    const GLFWvidmode* vid = glfwGetVideoMode(mon);

    g_savedW = 1280; g_savedH = 720;
    g_savedX = (vid->width - g_savedW) / 2;
    g_savedY = (vid->height - g_savedH) / 2;

    GLFWwindow* window = glfwCreateWindow(g_savedW, g_savedH, "Heritage Engine", nullptr, nullptr);
    if (!window) { std::cerr << "Window failed\n"; glfwTerminate(); return -1; }
    glfwSetWindowPos(window, g_savedX, g_savedY);

#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(window);
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
#endif

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "GLAD failed\n"; return -1; }
    std::cout << "OpenGL " << glGetString(GL_VERSION) << "\n";

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    // Load Orbitron SemiBold at 3 sizes
    const std::string fontPath = (findProjectRoot() / "Assets/Fonts/Orbitron-SemiBold.ttf").string();
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

    glfwSetMouseButtonCallback(window, mouseButtonCB);
    glfwSetCursorPosCallback(window, cursorPosCB);
    glfwSetScrollCallback(window, scrollCB);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_CULL_FACE);

    GLuint prog = buildProgram();
    Mesh logo = loadOBJ((findProjectRoot() / "Assets/RacingUnited_3D_Logo.obj").string());
    uploadMesh(logo);

    double prevTime = glfwGetTime();
    float  autoYaw = 0.f;
    bool   shouldClose = false;
    bool   shouldMin = false;
    bool   f11Prev = false;

    auto modeName = [](WindowMode m)->const char* {
        if (m == WindowMode::Windowed)   return "Windowed";
        if (m == WindowMode::Borderless) return "Borderless";
        return "Exclusive";
        };

    while (!glfwWindowShouldClose(window) && !shouldClose)
    {
        glfwPollEvents();

        if (shouldMin) { glfwIconifyWindow(window); shouldMin = false; }

        bool f11Now = (glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS);
        if (f11Now && !f11Prev)
        {
            if (g_windowMode == WindowMode::Windowed)
            {
                glfwGetWindowPos(window, &g_savedX, &g_savedY);
                glfwGetWindowSize(window, &g_savedW, &g_savedH);
                applyWindowMode(window, WindowMode::Borderless);
            }
            else if (g_windowMode == WindowMode::Borderless)
                applyWindowMode(window, WindowMode::Exclusive);
            else
                applyWindowMode(window, WindowMode::Windowed);
        }
        f11Prev = f11Now;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            shouldClose = true;
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        if (fbW == 0 || fbH == 0) { glfwSwapBuffers(window); continue; }

        double now = glfwGetTime();
        float dt = (float)(now - prevTime); prevTime = now;
        if (!g_drag) autoYaw += dt * 0.6f;

        glViewport(0, 0, fbW, fbH);
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(prog);

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

        Mat4 proj = perspective(0.6f, (float)fbW / (float)fbH, 0.1f, 100.f);
        Mat4 model = identity();

        glUniformMatrix4fv(glGetUniformLocation(prog, "uModel"), 1, GL_FALSE, model.m);
        glUniformMatrix4fv(glGetUniformLocation(prog, "uView"), 1, GL_FALSE, view.m);
        glUniformMatrix4fv(glGetUniformLocation(prog, "uProj"), 1, GL_FALSE, proj.m);
        glUniform3f(glGetUniformLocation(prog, "uLightPos"), 4.f, 6.f, 5.f);
        glUniform3f(glGetUniformLocation(prog, "uViewPos"), eye.x, eye.y, eye.z);
        glUniform3f(glGetUniformLocation(prog, "uColor"), 1.f, 1.f, 1.f);

        glBindVertexArray(logo.vao);
        glDrawElements(GL_TRIANGLES, (GLsizei)logo.idx.size(), GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        // ImGui titlebar
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        bool showTitlebar = (g_windowMode == WindowMode::Windowed);
        if (!showTitlebar)
        {
            double mx, my; glfwGetCursorPos(window, &mx, &my);
            if (my < 50) showTitlebar = true;
        }

        if (showTitlebar)
        {
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2((float)fbW, (float)TITLEBAR_H));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.05f, 0.95f));
            ImGui::Begin("##titlebar", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoSavedSettings);

            if (g_windowMode == WindowMode::Windowed)
            {
                ImGui::SetCursorPos(ImVec2(0, 0));
                ImGui::InvisibleButton("##drag", ImVec2((float)fbW - 120, (float)TITLEBAR_H));
                if (ImGui::IsItemActive())
                {
                    if (!g_winDragging) {
                        g_winDragging = true;
                        glfwGetWindowPos(window, &g_winStartX, &g_winStartY);
                        double cursorX, cursorY;
                        glfwGetCursorPos(window, &cursorX, &cursorY);
                        // Store the cursor in screen coordinates. Window-local
                        // cursor coordinates change as the window moves and cause feedback.
                        g_winDragStartX = g_winStartX + cursorX;
                        g_winDragStartY = g_winStartY + cursorY;
                    }
                    int currentWindowX, currentWindowY;
                    double cx, cy;
                    glfwGetWindowPos(window, &currentWindowX, &currentWindowY);
                    glfwGetCursorPos(window, &cx, &cy);
                    const double cursorScreenX = currentWindowX + cx;
                    const double cursorScreenY = currentWindowY + cy;
                    glfwSetWindowPos(window,
                        g_winStartX + (int)(cursorScreenX - g_winDragStartX),
                        g_winStartY + (int)(cursorScreenY - g_winDragStartY));
                }
                else g_winDragging = false;
            }

            // Title — small Orbitron
            ImGui::PushFont(g_fontSmall);
            ImGui::SetCursorPos(ImVec2(10, 7));
            ImGui::TextDisabled("HERITAGE ENGINE");
            ImGui::SameLine();
            ImGui::SetCursorPosY(7);
            ImGui::TextDisabled("|  %s  |  F11", modeName(g_windowMode));
            ImGui::PopFont();

            // Minimize
            ImGui::PushFont(g_fontSmall);
            ImGui::SetCursorPos(ImVec2((float)fbW - 112, 1));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.35f, 0.35f, 1));
            if (ImGui::Button(" _ ##min", ImVec2(36, 26))) shouldMin = true;

            // Close
            ImGui::SetCursorPos(ImVec2((float)fbW - 40, 1));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.1f, 0.1f, 1));
            if (ImGui::Button(" X ##cls", ImVec2(36, 26)))
            {
                shouldClose = true;
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
            ImGui::PopStyleColor(4);
            ImGui::PopFont();

            ImGui::End();
            ImGui::PopStyleColor();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &logo.vao);
    glDeleteBuffers(1, &logo.vbo);
    glDeleteBuffers(1, &logo.ebo);
    glDeleteProgram(prog);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

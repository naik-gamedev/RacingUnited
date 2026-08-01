// Heritage Engine - main.cpp
// Requirements: GLAD (OpenGL 4.6 core), GLFW 3.4
// Add glad.c to your project sources.
// Include paths: glad/include, glfw/include
//
// Controls: mouse drag to orbit, scroll to zoom

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <array>
#include <fstream>
#include <sstream>
#include <map>

// -----------------------------------------------------------------------
//  Math helpers (no external math lib needed yet)
// -----------------------------------------------------------------------
struct Vec3 { float x, y, z; };
struct Mat4 { float m[16] = {}; };

static Mat4 identity()
{
    Mat4 r;
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.f;
    return r;
}

static Mat4 perspective(float fovY, float aspect, float zNear, float zFar)
{
    float f = 1.f / tanf(fovY * 0.5f);
    Mat4 r;
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zFar + zNear) / (zNear - zFar);
    r.m[11] = -1.f;
    r.m[14] = (2.f * zFar * zNear) / (zNear - zFar);
    return r;
}

static Mat4 rotateY(float a)
{
    Mat4 r = identity();
    r.m[0] = cosf(a);
    r.m[2] = sinf(a);
    r.m[8] = -sinf(a);
    r.m[10] = cosf(a);
    return r;
}

static Mat4 rotateX(float a)
{
    Mat4 r = identity();
    r.m[5] = cosf(a);
    r.m[6] = -sinf(a);
    r.m[9] = sinf(a);
    r.m[10] = cosf(a);
    return r;
}

static Mat4 translate(float x, float y, float z)
{
    Mat4 r = identity();
    r.m[12] = x; r.m[13] = y; r.m[14] = z;
    return r;
}

static Mat4 mul(const Mat4& a, const Mat4& b)
{
    Mat4 r;
    for (int row = 0;row < 4;row++)
        for (int col = 0;col < 4;col++)
            for (int k = 0;k < 4;k++)
                r.m[col * 4 + row] += a.m[k * 4 + row] * b.m[col * 4 + k];
    return r;
}

// -----------------------------------------------------------------------
//  Shaders
// -----------------------------------------------------------------------
static const char* VS = R"glsl(
#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vNormal;
out vec3 vFragPos;

void main()
{
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vFragPos  = worldPos.xyz;
    vNormal   = mat3(transpose(inverse(uModel))) * aNormal;
    gl_Position = uProj * uView * worldPos;
}
)glsl";

static const char* FS = R"glsl(
#version 460 core
in vec3 vNormal;
in vec3 vFragPos;

uniform vec3 uLightPos;
uniform vec3 uViewPos;
uniform vec3 uColor;

out vec4 FragColor;

void main()
{
    vec3 norm    = normalize(vNormal);
    vec3 lightDir= normalize(uLightPos - vFragPos);
    vec3 viewDir = normalize(uViewPos  - vFragPos);
    vec3 halfway = normalize(lightDir + viewDir);

    // Phong
    float ambient  = 0.18;
    float diff     = max(dot(norm, lightDir), 0.0);
    float spec     = pow(max(dot(norm, halfway), 0.0), 64.0) * 0.7;
    float rim      = pow(1.0 - max(dot(norm, viewDir), 0.0), 3.0) * 0.25;

    vec3 col = uColor * (ambient + diff) + vec3(spec) + uColor * rim;
    // Subtle gamma
    col = pow(clamp(col, 0.0, 1.0), vec3(1.0/2.2));
    FragColor = vec4(col, 1.0);
}
)glsl";

// -----------------------------------------------------------------------
//  Mesh
// -----------------------------------------------------------------------
struct Mesh
{
    std::vector<float> verts;   // x y z  nx ny nz
    std::vector<unsigned> idx;
    GLuint vao = 0, vbo = 0, ebo = 0;
};

// -----------------------------------------------------------------------
//  OBJ loader  (positions + normals, no UVs needed yet)
//  Supports: v, vn, f  —  ignores everything else.
//  Face formats handled: v//vn  and  v/vt/vn
// -----------------------------------------------------------------------
static Mesh loadOBJ(const std::string& path)
{
    Mesh m;

    std::vector<std::array<float, 3>> positions;
    std::vector<std::array<float, 3>> normals;

    // Cache so we don't duplicate verts
    std::map<std::pair<int, int>, unsigned> cache;

    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Could not open OBJ: " << path << "\n";
        return m;
    }

    std::string line;
    while (std::getline(f, line))
    {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "v")
        {
            float x, y, z; ss >> x >> y >> z;
            positions.push_back({ x,y,z });
        }
        else if (token == "vn")
        {
            float x, y, z; ss >> x >> y >> z;
            normals.push_back({ x,y,z });
        }
        else if (token == "f")
        {
            // Triangulate on the fly (fan from first vertex)
            std::vector<std::pair<int, int>> face; // (posIdx, normIdx)
            std::string chunk;
            while (ss >> chunk)
            {
                // formats: v  v/vt  v//vn  v/vt/vn
                int pi = 0, ni = 0;
                size_t s1 = chunk.find('/');
                if (s1 == std::string::npos)
                {
                    pi = std::stoi(chunk);
                }
                else
                {
                    pi = std::stoi(chunk.substr(0, s1));
                    size_t s2 = chunk.find('/', s1 + 1);
                    if (s2 != std::string::npos && s2 > s1 + 1)
                        ni = std::stoi(chunk.substr(s2 + 1));
                    else if (s2 != std::string::npos)
                        ni = std::stoi(chunk.substr(s2 + 1));
                    else
                        ni = 0;
                }
                // OBJ indices are 1-based
                if (pi < 0) pi = (int)positions.size() + pi + 1;
                if (ni < 0) ni = (int)normals.size() + ni + 1;
                face.push_back({ pi, ni });
            }

            // Fan triangulation
            for (int i = 1; i + 1 < (int)face.size(); i++)
            {
                for (auto& fp : { face[0], face[i], face[i + 1] })
                {
                    auto key = fp;
                    auto it = cache.find(key);
                    if (it != cache.end())
                    {
                        m.idx.push_back(it->second);
                    }
                    else
                    {
                        unsigned newIdx = (unsigned)(m.verts.size() / 6);
                        cache[key] = newIdx;

                        int pi = fp.first - 1;
                        int ni = fp.second - 1;

                        auto& p = positions[pi];
                        m.verts.push_back(p[0]);
                        m.verts.push_back(p[1]);
                        m.verts.push_back(p[2]);

                        if (ni >= 0 && ni < (int)normals.size())
                        {
                            auto& n = normals[ni];
                            m.verts.push_back(n[0]);
                            m.verts.push_back(n[1]);
                            m.verts.push_back(n[2]);
                        }
                        else
                        {
                            m.verts.push_back(0); m.verts.push_back(1); m.verts.push_back(0);
                        }

                        m.idx.push_back(newIdx);
                    }
                }
            }
        }
    }

    // Auto-center and normalize scale
    if (!m.verts.empty())
    {
        float minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9, minZ = 1e9, maxZ = -1e9;
        for (int i = 0;i < (int)m.verts.size();i += 6)
        {
            minX = fminf(minX, m.verts[i]);   maxX = fmaxf(maxX, m.verts[i]);
            minY = fminf(minY, m.verts[i + 1]); maxY = fmaxf(maxY, m.verts[i + 1]);
            minZ = fminf(minZ, m.verts[i + 2]); maxZ = fmaxf(maxZ, m.verts[i + 2]);
        }
        float cx = (minX + maxX) * 0.5f, cy = (minY + maxY) * 0.5f, cz = (minZ + maxZ) * 0.5f;
        float scale = 2.0f / fmaxf(fmaxf(maxX - minX, maxY - minY), maxZ - minZ);
        for (int i = 0;i < (int)m.verts.size();i += 6)
        {
            m.verts[i] = (m.verts[i] - cx) * scale;
            m.verts[i + 1] = (m.verts[i + 1] - cy) * scale;
            m.verts[i + 2] = (m.verts[i + 2] - cz) * scale;
        }
    }

    std::cout << "OBJ loaded: " << m.idx.size() / 3 << " triangles\n";
    return m;
}

static void uploadMesh(Mesh& m)
{
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ebo);

    glBindVertexArray(m.vao);

    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER,
        m.verts.size() * sizeof(float), m.verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        m.idx.size() * sizeof(unsigned), m.idx.data(), GL_STATIC_DRAW);

    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

// -----------------------------------------------------------------------
//  Shader compilation
// -----------------------------------------------------------------------
static GLuint compileShader(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetShaderInfoLog(s, 512, nullptr, log);
        std::cerr << "Shader error:\n" << log << "\n";
    }
    return s;
}

static GLuint buildProgram()
{
    GLuint vs = compileShader(GL_VERTEX_SHADER, VS);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, FS);
    GLuint p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    int ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetProgramInfoLog(p, 512, nullptr, log);
        std::cerr << "Link error:\n" << log << "\n";
    }
    glDeleteShader(vs); glDeleteShader(fs);
    return p;
}

// -----------------------------------------------------------------------
//  Mouse orbit state
// -----------------------------------------------------------------------
static float g_orbitX = 0.3f;   // radians
static float g_orbitY = 0.0f;
static float g_zoom = 4.5f;
static bool  g_drag = false;
static double g_lastX = 0, g_lastY = 0;

static void mouseButtonCB(GLFWwindow*, int btn, int action, int)
{
    if (btn == GLFW_MOUSE_BUTTON_LEFT)
        g_drag = (action == GLFW_PRESS);
}

static void cursorPosCB(GLFWwindow* w, double x, double y)
{
    if (g_drag) {
        g_orbitY += (float)(x - g_lastX) * 0.01f;
        g_orbitX += (float)(y - g_lastY) * 0.01f;
        g_orbitX = fmaxf(-1.4f, fminf(1.4f, g_orbitX));
    }
    g_lastX = x; g_lastY = y;
}

static void scrollCB(GLFWwindow*, double, double dy)
{
    g_zoom -= (float)dy * 0.3f;
    g_zoom = fmaxf(2.0f, fminf(12.0f, g_zoom));
}

static void keyCB(GLFWwindow* w, int key, int, int action, int)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(w, GLFW_TRUE);
}

// -----------------------------------------------------------------------
//  Main
// -----------------------------------------------------------------------
int main()
{
    if (!glfwInit()) {
        std::cerr << "GLFW init failed\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);   // MSAA x4

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Heritage Engine", nullptr, nullptr);
    if (!window) {
        std::cerr << "Window creation failed\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // vsync

    // Load OpenGL via GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "GLAD failed to load OpenGL\n";
        return -1;
    }
    std::cout << "OpenGL " << glGetString(GL_VERSION) << "\n";

    // Callbacks
    glfwSetMouseButtonCallback(window, mouseButtonCB);
    glfwSetCursorPosCallback(window, cursorPosCB);
    glfwSetScrollCallback(window, scrollCB);
    glfwSetKeyCallback(window, keyCB);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_CULL_FACE);

    GLuint prog = buildProgram();

    Mesh logo = loadOBJ("F:/Racing United/SourceCode/GitHub/RacingUnited/Assets/RacingUnited_3D_Logo.obj");
    uploadMesh(logo);

    // Projection (fixed)
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    Mat4 proj = perspective(0.6f, (float)fbW / (float)fbH, 0.1f, 100.f);

    double prevTime = glfwGetTime();
    float  autoYaw = 0.f;         // slow auto-spin when not dragging

    while (!glfwWindowShouldClose(window))
    {
        double now = glfwGetTime();
        float  dt = (float)(now - prevTime);
        prevTime = now;

        if (!g_drag) autoYaw += dt * 0.6f;

        // Rebuild framebuffer size in case of resize
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        proj = perspective(0.6f, (float)fbW / (float)fbH, 0.1f, 100.f);

        glClearColor(0.0f, 0.0f, 0.0f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(prog);

        // Camera: orbit around origin
        float camX = sinf(g_orbitY + autoYaw) * cosf(g_orbitX) * g_zoom;
        float camY = sinf(g_orbitX) * g_zoom;
        float camZ = cosf(g_orbitY + autoYaw) * cosf(g_orbitX) * g_zoom;

        // Simple lookat (target = origin)
        // View = translate(-eye) then rotate — we compute it directly
        // using the standard lookAt construction
        Vec3 eye{ camX, camY, camZ };
        Vec3 up{ 0,1,0 };
        Vec3 fwd{ -camX,-camY,-camZ };
        float fl = sqrtf(fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z);
        fwd = { fwd.x / fl,fwd.y / fl,fwd.z / fl };
        // right = fwd x up
        Vec3 right{
            fwd.y * up.z - fwd.z * up.y,
            fwd.z * up.x - fwd.x * up.z,
            fwd.x * up.y - fwd.y * up.x
        };
        float rl = sqrtf(right.x * right.x + right.y * right.y + right.z * right.z);
        right = { right.x / rl,right.y / rl,right.z / rl };
        // recompute up
        Vec3 vup{
            right.y * fwd.z - right.z * fwd.y,
            right.z * fwd.x - right.x * fwd.z,
            right.x * fwd.y - right.y * fwd.x
        };

        Mat4 view;
        view.m[0] = right.x; view.m[4] = right.y; view.m[8] = right.z;
        view.m[1] = vup.x;   view.m[5] = vup.y;   view.m[9] = vup.z;
        view.m[2] = -fwd.x;  view.m[6] = -fwd.y;  view.m[10] = -fwd.z;
        view.m[15] = 1.f;
        view.m[12] = -(right.x * eye.x + right.y * eye.y + right.z * eye.z);
        view.m[13] = -(vup.x * eye.x + vup.y * eye.y + vup.z * eye.z);
        view.m[14] = (fwd.x * eye.x + fwd.y * eye.y + fwd.z * eye.z);

        Mat4 model = identity();

        glUniformMatrix4fv(glGetUniformLocation(prog, "uModel"), 1, GL_FALSE, model.m);
        glUniformMatrix4fv(glGetUniformLocation(prog, "uView"), 1, GL_FALSE, view.m);
        glUniformMatrix4fv(glGetUniformLocation(prog, "uProj"), 1, GL_FALSE, proj.m);

        glUniform3f(glGetUniformLocation(prog, "uLightPos"), 4.f, 6.f, 5.f);
        glUniform3f(glGetUniformLocation(prog, "uViewPos"), eye.x, eye.y, eye.z);
        // White
        glUniform3f(glGetUniformLocation(prog, "uColor"), 1.0f, 1.0f, 1.0f);

        glBindVertexArray(logo.vao);
        glDrawElements(GL_TRIANGLES, (GLsizei)logo.idx.size(), GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &logo.vao);
    glDeleteBuffers(1, &logo.vbo);
    glDeleteBuffers(1, &logo.ebo);
    glDeleteProgram(prog);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
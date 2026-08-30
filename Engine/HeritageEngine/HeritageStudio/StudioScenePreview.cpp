#include "StudioScenePreview.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <iostream>
#include <limits>
#include <unordered_map>

#include "../Graphics/GltfBinary.hpp"

namespace heritage::studio {
namespace {

GLuint compileShader(GLenum type, const char* source, std::string& message)
{
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_TRUE)
        return shader;

    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    message = "Studio scene-preview shader compile failed: " + log;
    glDeleteShader(shader);
    return 0;
}

bool linkProgram(GLuint program, std::string& message)
{
    glLinkProgram(program);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok == GL_TRUE)
        return true;

    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
    glGetProgramInfoLog(program, length, nullptr, log.data());
    message = "Studio scene-preview shader link failed: " + log;
    return false;
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool isAuthoringName(const std::string& name)
{
    const std::string value = lower(name);
    return value == "collision"
        || value.rfind("collision_", 0) == 0
        || value.find("_collision") != std::string::npos
        || value.find("spawn_player") != std::string::npos
        || value.find("player_spawn") != std::string::npos
        || value.find("playerspawn") != std::string::npos;
}

float dot(const authoring::Vec3& a, const authoring::Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

authoring::Vec3 sub(const authoring::Vec3& a, const authoring::Vec3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

authoring::Vec3 cross(const authoring::Vec3& a, const authoring::Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

bool intersectTriangle(const authoring::Vec3& origin, const authoring::Vec3& direction,
    const authoring::Vec3& a, const authoring::Vec3& b, const authoring::Vec3& c, float& t)
{
    constexpr float epsilon = 1.0e-7f;
    const authoring::Vec3 edge1 = sub(b, a);
    const authoring::Vec3 edge2 = sub(c, a);
    const authoring::Vec3 p = cross(direction, edge2);
    const float det = dot(edge1, p);
    if (std::abs(det) < epsilon)
        return false;
    const float invDet = 1.0f / det;
    const authoring::Vec3 s = sub(origin, a);
    const float u = dot(s, p) * invDet;
    if (u < 0.0f || u > 1.0f)
        return false;
    const authoring::Vec3 q = cross(s, edge1);
    const float v = dot(direction, q) * invDet;
    if (v < 0.0f || u + v > 1.0f)
        return false;
    const float candidate = dot(edge2, q) * invDet;
    if (candidate <= epsilon)
        return false;
    t = candidate;
    return true;
}

void setUniformVec3(GLuint program, const char* name, const authoring::Vec3& value)
{
    const GLint location = glGetUniformLocation(program, name);
    if (location >= 0)
        glUniform3f(location, value.x, value.y, value.z);
}

void setUniformVec3(GLuint program, const char* name, const heritage::math::Vec3& value)
{
    const GLint location = glGetUniformLocation(program, name);
    if (location >= 0)
        glUniform3f(location, value.x, value.y, value.z);
}

void setUniform1f(GLuint program, const char* name, float value)
{
    const GLint location = glGetUniformLocation(program, name);
    if (location >= 0)
        glUniform1f(location, value);
}

void setUniform1i(GLuint program, const char* name, int value)
{
    const GLint location = glGetUniformLocation(program, name);
    if (location >= 0)
        glUniform1i(location, value);
}

} // namespace

StudioScenePreview::~StudioScenePreview()
{
    shutdown();
}

bool StudioScenePreview::initialize(const std::filesystem::path& assetRoot, std::string& message)
{
    m_assetRoot = assetRoot;
    if (!ensureProgram(message))
        return false;
    if (!ensureGridProgram(message))
        return false;
    discoverAndLoadLatest(message);
    return true;
}

void StudioScenePreview::shutdown()
{
    destroyFramebuffer();
    m_textureCache.clear();
    heritage::graphics::destroyMesh(m_sceneMesh);
    if (m_gridVao)
        glDeleteVertexArrays(1, &m_gridVao);
    if (m_gridProgram)
        glDeleteProgram(m_gridProgram);
    if (m_program)
        glDeleteProgram(m_program);
    m_gridVao = 0;
    m_gridProgram = 0;
    m_program = 0;
    m_loaded = false;
    m_nodeGlobals.clear();
    m_raycastTriangles.clear();
}

bool StudioScenePreview::discoverAndLoadLatest(std::string& message)
{
    const std::filesystem::path sceneRoot = m_assetRoot / "Scenes";
    std::error_code ec;
    if (!std::filesystem::is_directory(sceneRoot, ec))
    {
        message = "Studio scene preview: Assets/Scenes does not exist.";
        m_status = message;
        return false;
    }

    std::filesystem::path newest;
    std::filesystem::file_time_type newestTime{};
    bool found = false;
    for (std::filesystem::recursive_directory_iterator it(sceneRoot, ec), end; it != end && !ec; it.increment(ec))
    {
        if (!it->is_regular_file(ec))
            continue;
        const auto& path = it->path();
        if (lower(path.extension().string()) != ".glb")
            continue;
        const std::string stem = lower(path.stem().string());
        if (stem.rfind("scene_", 0) != 0)
            continue;
        std::error_code timeError;
        const auto time = it->last_write_time(timeError);
        if (timeError)
            continue;
        if (!found || time > newestTime)
        {
            newest = path;
            newestTime = time;
            found = true;
        }
    }

    if (!found)
    {
        message = "Studio scene preview: no Scene_*.glb found under Assets/Scenes.";
        m_status = message;
        return false;
    }
    return loadScene(newest, message);
}

bool StudioScenePreview::loadScene(const std::filesystem::path& absolutePath, std::string& message)
{
    std::error_code ec;
    if (!std::filesystem::is_regular_file(absolutePath, ec))
    {
        message = "Studio scene preview: GLB not found: " + absolutePath.string();
        m_status = message;
        return false;
    }

    m_textureCache.clear();
    heritage::graphics::destroyMesh(m_sceneMesh);
    m_nodeGlobals.clear();
    m_raycastTriangles.clear();

    m_sceneMesh = heritage::graphics::loadGlbMesh(absolutePath, false);
    if (m_sceneMesh.vertices.empty() || m_sceneMesh.indices.empty())
    {
        message = "Studio scene preview: GLB loaded no renderable geometry: " + absolutePath.filename().string();
        m_status = message;
        m_loaded = false;
        return false;
    }
    heritage::graphics::uploadMesh(m_sceneMesh);
    if (!m_sceneMesh.vao)
    {
        message = "Studio scene preview: GLB GPU upload failed.";
        m_status = message;
        m_loaded = false;
        return false;
    }

    m_scenePath = absolutePath;
    rebuildNodeTransforms();
    rebuildRaycastTriangles();
    m_loaded = true;
    message = "Studio scene preview loaded " + absolutePath.filename().string()
        + " | " + std::to_string(triangleCount()) + " triangles | "
        + std::to_string(materialCount()) + " materials.";
    m_status = message;
    return true;
}

bool StudioScenePreview::reload(std::string& message)
{
    if (m_scenePath.empty())
        return discoverAndLoadLatest(message);
    return loadScene(m_scenePath, message);
}

bool StudioScenePreview::ensureProgram(std::string& message)
{
    if (m_program)
        return true;

    static const char* vertexShader = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUv;
layout(location=3) in vec4 aTangent;
layout(location=6) in vec4 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPosition;
out vec3 vNormal;
out vec2 vUv;
out vec4 vTangent;
out vec4 vColor;

void main()
{
    vec4 world = uModel * vec4(aPosition, 1.0);
    vWorldPosition = world.xyz;
    mat3 normalMatrix = transpose(inverse(mat3(uModel)));
    vNormal = normalize(normalMatrix * aNormal);
    vTangent = vec4(normalize(normalMatrix * aTangent.xyz), aTangent.w);
    vUv = aUv;
    vColor = aColor;
    gl_Position = uProjection * uView * world;
}
)GLSL";

    static const char* fragmentShader = R"GLSL(
#version 330 core
in vec3 vWorldPosition;
in vec3 vNormal;
in vec2 vUv;
in vec4 vTangent;
in vec4 vColor;

uniform vec3 uCameraPosition;
uniform vec3 uBaseColor;
uniform vec3 uEmissive;
uniform float uRoughness;
uniform float uMetallic;
uniform float uOpacity;
uniform float uExposure;
uniform int uHasVertexColors;

uniform sampler2D uBaseColorMap;
uniform sampler2D uNormalMap;
uniform sampler2D uRoughnessMap;
uniform sampler2D uMetallicMap;
uniform sampler2D uAoMap;
uniform sampler2D uEmissiveMap;
uniform int uHasBaseColorMap;
uniform int uHasNormalMap;
uniform int uHasRoughnessMap;
uniform int uHasMetallicMap;
uniform int uHasAoMap;
uniform int uHasEmissiveMap;
uniform int uRoughnessChannel;
uniform int uMetallicChannel;
uniform int uAoChannel;

out vec4 FragColor;

float channelValue(vec4 value, int channel)
{
    if (channel == 0) return value.r;
    if (channel == 1) return value.g;
    if (channel == 2) return value.b;
    return value.a;
}

float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / max(3.14159265 * denom * denom, 1e-5);
}

float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / max(NdotV * (1.0 - k) + k, 1e-5);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    return geometrySchlickGGX(max(dot(N, V), 0.0), roughness)
         * geometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

void main()
{
    vec4 baseSample = uHasBaseColorMap != 0 ? texture(uBaseColorMap, vUv) : vec4(1.0);
    vec4 vertexColor = uHasVertexColors != 0 ? vColor : vec4(1.0);
    vec3 albedo = max(uBaseColor * baseSample.rgb * vertexColor.rgb, vec3(0.0));
    float alpha = uOpacity * baseSample.a * vertexColor.a;
    if (alpha < 0.02) discard;

    float roughness = clamp(uRoughness, 0.04, 1.0);
    if (uHasRoughnessMap != 0)
        roughness = clamp(roughness * channelValue(texture(uRoughnessMap, vUv), uRoughnessChannel), 0.04, 1.0);
    float metallic = clamp(uMetallic, 0.0, 1.0);
    if (uHasMetallicMap != 0)
        metallic = clamp(metallic * channelValue(texture(uMetallicMap, vUv), uMetallicChannel), 0.0, 1.0);
    float ao = 1.0;
    if (uHasAoMap != 0)
        ao = channelValue(texture(uAoMap, vUv), uAoChannel);

    vec3 N = normalize(vNormal);
    if (uHasNormalMap != 0)
    {
        vec3 T = normalize(vTangent.xyz);
        vec3 B = normalize(cross(N, T) * vTangent.w);
        vec3 tangentNormal = texture(uNormalMap, vUv).xyz * 2.0 - 1.0;
        N = normalize(mat3(T, B, N) * tangentNormal);
    }

    vec3 V = normalize(uCameraPosition - vWorldPosition);
    vec3 L = normalize(vec3(-0.42, 0.82, 0.38));
    vec3 H = normalize(V + L);
    vec3 lightColor = vec3(4.8, 4.65, 4.4);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float NDF = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
    vec3 specular = numerator / denominator;
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    float NdotL = max(dot(N, L), 0.0);
    vec3 direct = (kD * albedo / 3.14159265 + specular) * lightColor * NdotL;

    vec3 skyAmbient = mix(vec3(0.11, 0.13, 0.16), vec3(0.28, 0.32, 0.38), clamp(N.y * 0.5 + 0.5, 0.0, 1.0));
    vec3 ambient = skyAmbient * albedo * ao;
    vec3 emissive = uEmissive;
    if (uHasEmissiveMap != 0)
        emissive *= texture(uEmissiveMap, vUv).rgb;

    vec3 color = (ambient + direct + emissive) * uExposure;
    color = color / (color + vec3(1.0));
    color = pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));
    FragColor = vec4(color, alpha);
}
)GLSL";

    std::string error;
    const GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexShader, error);
    if (!vertex)
    {
        message = error;
        return false;
    }
    const GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentShader, error);
    if (!fragment)
    {
        glDeleteShader(vertex);
        message = error;
        return false;
    }

    m_program = glCreateProgram();
    glAttachShader(m_program, vertex);
    glAttachShader(m_program, fragment);
    const bool linked = linkProgram(m_program, error);
    glDetachShader(m_program, vertex);
    glDetachShader(m_program, fragment);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    if (!linked)
    {
        glDeleteProgram(m_program);
        m_program = 0;
        message = error;
        return false;
    }
    message.clear();
    return true;
}

bool StudioScenePreview::ensureGridProgram(std::string& message)
{
    if (m_gridProgram && m_gridVao)
        return true;

    // STUDIO31: render the editor floor grid inside the preview framebuffer
    // instead of as an ImGui line overlay.  The fragment shader reconstructs
    // the Y=0 world-plane intersection per pixel, chooses an adaptive grid
    // scale from screen-space derivatives and writes the correct depth.  This
    // is much closer to Blender's infinite grid: no finite patch edges, no
    // nested-line moire at the horizon, and scene geometry correctly occludes
    // grid fragments that are behind it.
    static const char* vertexShader = R"GLSL(
#version 330 core
void main()
{
    vec2 p;
    if (gl_VertexID == 0) p = vec2(-1.0, -1.0);
    else if (gl_VertexID == 1) p = vec2(3.0, -1.0);
    else p = vec2(-1.0, 3.0);
    gl_Position = vec4(p, 0.0, 1.0);
}
)GLSL";

    static const char* fragmentShader = R"GLSL(
#version 330 core

uniform vec2 uViewportSize;
uniform vec3 uCameraPosition;
uniform vec3 uCameraForward;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;
uniform float uFovYRadians;
uniform float uAspect;
uniform int uOrthographic;
uniform float uOrthoHalfHeight;
uniform mat4 uView;
uniform mat4 uProjection;

out vec4 FragColor;

float lineMask(vec2 worldXZ, float stepM)
{
    vec2 coord = worldXZ / max(stepM, 1e-5);
    vec2 width = max(fwidth(coord), vec2(1e-5));
    vec2 distanceToLine = abs(fract(coord - 0.5) - 0.5) / width;
    return 1.0 - min(min(distanceToLine.x, distanceToLine.y), 1.0);
}

float axisMask(float coordinate)
{
    float width = max(fwidth(coordinate), 1e-5);
    return 1.0 - smoothstep(0.65, 1.65, abs(coordinate) / width);
}

void main()
{
    vec2 ndc = vec2(
        (gl_FragCoord.x / max(uViewportSize.x, 1.0)) * 2.0 - 1.0,
        (gl_FragCoord.y / max(uViewportSize.y, 1.0)) * 2.0 - 1.0);

    vec3 rayOrigin = uCameraPosition;
    vec3 rayDirection = uCameraForward;
    if (uOrthographic != 0)
    {
        rayOrigin += uCameraRight * (ndc.x * uOrthoHalfHeight * uAspect);
        rayOrigin += uCameraUp * (ndc.y * uOrthoHalfHeight);
    }
    else
    {
        float tanHalf = tan(uFovYRadians * 0.5);
        rayDirection = normalize(
            uCameraForward
            + uCameraRight * (ndc.x * uAspect * tanHalf)
            + uCameraUp * (ndc.y * tanHalf));
    }

    // The editor grid is the global X/Z plane (Y=0), exactly like Blender's
    // world floor. Rays above the horizon never intersect it in front of the
    // camera and therefore contribute no grid fragment.
    if (abs(rayDirection.y) < 1e-7)
        discard;
    float t = -rayOrigin.y / rayDirection.y;
    if (t <= 0.0)
        discard;

    vec3 world = rayOrigin + rayDirection * t;
    float viewDistance = length(world - uCameraPosition);

    // Estimate metres per pixel at this exact floor fragment.  We select only
    // the two useful neighbouring powers-of-five instead of drawing six full
    // line lattices at once.  This prevents the converging comb / moire pattern
    // that STUDIO30 could produce near the perspective horizon.
    float metresPerPixel = max(
        length(dFdx(world.xz)),
        length(dFdy(world.xz)));
    metresPerPixel = clamp(metresPerPixel, 1e-5, 1e8);

    const float baseStepM = 5.0;
    const float targetPixels = 24.0;
    float desiredStep = max(baseStepM, metresPerPixel * targetPixels);
    float continuousLevel = max(log(desiredStep / baseStepM) / log(5.0), 0.0);
    float level = clamp(floor(continuousLevel), 0.0, 8.0);
    float levelPhase = clamp(continuousLevel - level, 0.0, 1.0);
    float fineStep = baseStepM * pow(5.0, level);
    float coarseStep = fineStep * 5.0;

    // As a projected cell shrinks from roughly 24 px toward 5 px, fade only
    // the intermediate fine lines. The coarse subset never moves, so crossing
    // an LOD boundary changes density smoothly instead of creating a band.
    float fineFade = 1.0 - smoothstep(0.05, 0.95, levelPhase);
    float fine = lineMask(world.xz, fineStep) * fineFade;
    float coarse = lineMask(world.xz, coarseStep);

    float gridAlpha = max(fine * 0.28, coarse * 0.28);
    vec3 gridColor = vec3(0.27, 0.31, 0.34);
    vec3 color = gridColor;
    float alpha = gridAlpha;

    // Infinite world axes are evaluated analytically in the same shader, so
    // they have constant screen-space thickness and no artificial endpoints.
    float xAxis = axisMask(world.z); // red X axis: Z == 0
    float zAxis = axisMask(world.x); // blue Z axis: X == 0
    if (xAxis > 0.0)
    {
        color = mix(color, vec3(0.68, 0.24, 0.24), xAxis);
        alpha = max(alpha, xAxis * 0.88);
    }
    if (zAxis > 0.0)
    {
        color = mix(color, vec3(0.25, 0.43, 0.75), zAxis);
        alpha = max(alpha, zAxis * 0.88);
    }

    // Blender fades its floor toward grazing angles instead of allowing an
    // infinite number of sub-pixel lines to collapse into a bright horizon.
    // The very long distance fade is only a numerical safety net; visually the
    // angle fade is what removes the perspective combing.
    float horizonFade = smoothstep(0.0015, 0.030, abs(rayDirection.y));
    float distanceFade = 1.0 - smoothstep(50000.0, 95000.0, viewDistance);
    alpha *= horizonFade * distanceFade;
    if (alpha <= 0.002)
        discard;

    // Give the floor grid real depth so scene geometry can occlude it.  A tiny
    // camera-side Y bias avoids z-fighting with authoring geometry that lies
    // exactly on Y=0 without making the grid an always-on-top overlay.
    vec3 depthWorld = world;
    depthWorld.y += uCameraPosition.y >= 0.0 ? 0.002 : -0.002;
    vec4 clip = uProjection * uView * vec4(depthWorld, 1.0);
    if (clip.w <= 0.0)
        discard;
    float ndcDepth = clip.z / clip.w;
    if (ndcDepth < -1.0 || ndcDepth > 1.0)
        discard;
    gl_FragDepth = ndcDepth * 0.5 + 0.5;

    FragColor = vec4(color, alpha);
}
)GLSL";

    std::string error;
    const GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexShader, error);
    if (!vertex)
    {
        message = error;
        return false;
    }
    const GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentShader, error);
    if (!fragment)
    {
        glDeleteShader(vertex);
        message = error;
        return false;
    }

    m_gridProgram = glCreateProgram();
    glAttachShader(m_gridProgram, vertex);
    glAttachShader(m_gridProgram, fragment);
    const bool linked = linkProgram(m_gridProgram, error);
    glDetachShader(m_gridProgram, vertex);
    glDetachShader(m_gridProgram, fragment);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    if (!linked)
    {
        glDeleteProgram(m_gridProgram);
        m_gridProgram = 0;
        message = error;
        return false;
    }

    glGenVertexArrays(1, &m_gridVao);
    if (!m_gridVao)
    {
        glDeleteProgram(m_gridProgram);
        m_gridProgram = 0;
        message = "Studio infinite-grid VAO creation failed.";
        return false;
    }
    message.clear();
    return true;
}

bool StudioScenePreview::ensureFramebuffer(int width, int height)
{
    width = std::max(width, 1);
    height = std::max(height, 1);
    if (m_framebuffer && width == m_framebufferWidth && height == m_framebufferHeight)
        return true;

    destroyFramebuffer();
    glGenFramebuffers(1, &m_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

    glGenTextures(1, &m_colorTexture);
    glBindTexture(GL_TEXTURE_2D, m_colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTexture, 0);

    glGenRenderbuffers(1, &m_depthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthRenderbuffer);

    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!complete)
    {
        destroyFramebuffer();
        return false;
    }
    m_framebufferWidth = width;
    m_framebufferHeight = height;
    return true;
}

void StudioScenePreview::destroyFramebuffer()
{
    if (m_depthRenderbuffer)
        glDeleteRenderbuffers(1, &m_depthRenderbuffer);
    if (m_colorTexture)
        glDeleteTextures(1, &m_colorTexture);
    if (m_framebuffer)
        glDeleteFramebuffers(1, &m_framebuffer);
    m_depthRenderbuffer = 0;
    m_colorTexture = 0;
    m_framebuffer = 0;
    m_framebufferWidth = 0;
    m_framebufferHeight = 0;
}

GLuint StudioScenePreview::render(int width, int height, const StudioPreviewCamera& camera, bool gridVisible)
{
    if (!m_visible || !m_loaded || !m_program || !ensureFramebuffer(width, height))
        return 0;

    GLint previousFramebuffer = 0;
    GLint previousViewport[4]{};
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousFramebuffer);
    glGetIntegerv(GL_VIEWPORT, previousViewport);
    const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    const GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean previousDepthMask = GL_TRUE;
    GLint previousDepthFunc = GL_LESS;
    GLint previousBlendSrcRgb = GL_ONE;
    GLint previousBlendDstRgb = GL_ZERO;
    GLint previousBlendSrcAlpha = GL_ONE;
    GLint previousBlendDstAlpha = GL_ZERO;
    GLint previousBlendEquationRgb = GL_FUNC_ADD;
    GLint previousBlendEquationAlpha = GL_FUNC_ADD;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
    glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);
    glGetIntegerv(GL_BLEND_SRC_RGB, &previousBlendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &previousBlendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &previousBlendDstAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &previousBlendEquationRgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &previousBlendEquationAlpha);

    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, m_wireframe ? GL_LINE : GL_FILL);
    glClearColor(0.045f, 0.055f, 0.070f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glUseProgram(m_program);
    const Mat4 view = viewMatrix(camera);
    const Mat4 projection = camera.orthographic
        ? orthographicMatrix(camera.orthoHalfHeight, camera.aspect, -10000.0f, 10000.0f)
        : perspectiveMatrix(camera.fovYRadians, camera.aspect, 0.05f, 100000.0f);
    glUniformMatrix4fv(glGetUniformLocation(m_program, "uView"), 1, GL_FALSE, view.m);
    glUniformMatrix4fv(glGetUniformLocation(m_program, "uProjection"), 1, GL_FALSE, projection.m);
    setUniformVec3(m_program, "uCameraPosition", camera.position);
    setUniform1f(m_program, "uExposure", m_exposure);

    glBindVertexArray(m_sceneMesh.vao);
    for (const auto& range : m_sceneMesh.drawRanges)
    {
        if (range.indexCount == 0 || hiddenAuthoringRange(range))
            continue;

        Mat4 model = identity();
        if (range.nodeIndex >= 0 && static_cast<std::size_t>(range.nodeIndex) < m_nodeGlobals.size())
            model = m_nodeGlobals[static_cast<std::size_t>(range.nodeIndex)];
        glUniformMatrix4fv(glGetUniformLocation(m_program, "uModel"), 1, GL_FALSE, model.m);

        heritage::graphics::MaterialDefinition material;
        if (const auto found = m_sceneMesh.materials.find(range.materialName); found != m_sceneMesh.materials.end())
            material = found->second;

        setUniformVec3(m_program, "uBaseColor", material.baseColor);
        setUniformVec3(m_program, "uEmissive", material.emissiveColor);
        setUniform1f(m_program, "uRoughness", material.roughness);
        setUniform1f(m_program, "uMetallic", material.metallic);
        setUniform1f(m_program, "uOpacity", material.opacity);
        setUniform1i(m_program, "uHasVertexColors", range.hasVertexColors ? 1 : 0);

        struct TextureBinding
        {
            const char* sampler;
            const char* enabled;
            const heritage::graphics::MaterialTextureReference* reference;
            heritage::graphics::TextureColorSpace colorSpace;
            int unit;
        };
        const std::array<TextureBinding, 6> bindings{{
            { "uBaseColorMap", "uHasBaseColorMap", &material.baseColorMap, heritage::graphics::TextureColorSpace::SRgb, 0 },
            { "uNormalMap", "uHasNormalMap", &material.normalMap, heritage::graphics::TextureColorSpace::Linear, 1 },
            { "uRoughnessMap", "uHasRoughnessMap", &material.roughnessMap, heritage::graphics::TextureColorSpace::Linear, 2 },
            { "uMetallicMap", "uHasMetallicMap", &material.metallicMap, heritage::graphics::TextureColorSpace::Linear, 3 },
            { "uAoMap", "uHasAoMap", &material.ambientOcclusionMap, heritage::graphics::TextureColorSpace::Linear, 4 },
            { "uEmissiveMap", "uHasEmissiveMap", &material.emissiveMap, heritage::graphics::TextureColorSpace::SRgb, 5 }
        }};

        for (const auto& binding : bindings)
        {
            std::string textureError;
            const auto* texture = acquireTexture(*binding.reference, binding.colorSpace, textureError);
            setUniform1i(m_program, binding.enabled, texture ? 1 : 0);
            setUniform1i(m_program, binding.sampler, binding.unit);
            if (texture)
            {
                glActiveTexture(GL_TEXTURE0 + binding.unit);
                glBindTexture(GL_TEXTURE_2D, texture->id);
            }
        }
        setUniform1i(m_program, "uRoughnessChannel", static_cast<int>(material.roughnessMap.channel));
        setUniform1i(m_program, "uMetallicChannel", static_cast<int>(material.metallicMap.channel));
        setUniform1i(m_program, "uAoChannel", static_cast<int>(material.ambientOcclusionMap.channel));

        glDrawElements(GL_TRIANGLES,
            static_cast<GLsizei>(range.indexCount), GL_UNSIGNED_INT,
            reinterpret_cast<const void*>(range.firstIndex * sizeof(unsigned int)));
    }
    glBindVertexArray(0);
    glUseProgram(0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    if (gridVisible && m_gridProgram && m_gridVao)
    {
        // STUDIO31: the infinite grid is a depth-aware procedural floor pass,
        // not an ImGui overlay.  Scene geometry therefore hides grid lines
        // naturally, while derivatives keep line density stable at distance.
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        glUseProgram(m_gridProgram);
        glUniform2f(glGetUniformLocation(m_gridProgram, "uViewportSize"),
            static_cast<float>(width), static_cast<float>(height));
        setUniformVec3(m_gridProgram, "uCameraPosition", camera.position);
        setUniformVec3(m_gridProgram, "uCameraForward", camera.forward);
        setUniformVec3(m_gridProgram, "uCameraRight", camera.right);
        setUniformVec3(m_gridProgram, "uCameraUp", camera.up);
        setUniform1f(m_gridProgram, "uFovYRadians", camera.fovYRadians);
        setUniform1f(m_gridProgram, "uAspect", camera.aspect);
        setUniform1i(m_gridProgram, "uOrthographic", camera.orthographic ? 1 : 0);
        setUniform1f(m_gridProgram, "uOrthoHalfHeight", camera.orthoHalfHeight);
        glUniformMatrix4fv(glGetUniformLocation(m_gridProgram, "uView"), 1, GL_FALSE, view.m);
        glUniformMatrix4fv(glGetUniformLocation(m_gridProgram, "uProjection"), 1, GL_FALSE, projection.m);

        glBindVertexArray(m_gridVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
        glUseProgram(0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, previousFramebuffer);
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
    glDepthMask(previousDepthMask);
    glDepthFunc(static_cast<GLenum>(previousDepthFunc));
    glBlendEquationSeparate(static_cast<GLenum>(previousBlendEquationRgb), static_cast<GLenum>(previousBlendEquationAlpha));
    glBlendFuncSeparate(
        static_cast<GLenum>(previousBlendSrcRgb), static_cast<GLenum>(previousBlendDstRgb),
        static_cast<GLenum>(previousBlendSrcAlpha), static_cast<GLenum>(previousBlendDstAlpha));
    if (depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (cullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    return m_colorTexture;
}

const heritage::graphics::Texture2D* StudioScenePreview::acquireTexture(
    const heritage::graphics::MaterialTextureReference& reference,
    heritage::graphics::TextureColorSpace colorSpace,
    std::string& error)
{
    if (reference.empty())
        return nullptr;
    if (reference.embedded)
    {
        return m_textureCache.acquireEmbedded(
            reference.embedded->key,
            reference.embedded->bytes,
            colorSpace,
            2,
            reference.flipVerticalOnDecode,
            error);
    }
    return m_textureCache.acquire(
        reference.filePath,
        colorSpace,
        2,
        reference.flipVerticalOnDecode,
        error);
}

bool StudioScenePreview::hiddenAuthoringRange(const heritage::graphics::MeshDrawRange& range) const
{
    int node = range.nodeIndex;
    std::size_t guard = 0;
    while (node >= 0 && static_cast<std::size_t>(node) < m_sceneMesh.nodes.size() && guard++ <= m_sceneMesh.nodes.size())
    {
        if (isAuthoringName(m_sceneMesh.nodes[static_cast<std::size_t>(node)].name))
            return true;
        node = m_sceneMesh.nodes[static_cast<std::size_t>(node)].parentIndex;
    }
    return false;
}

void StudioScenePreview::rebuildNodeTransforms()
{
    m_nodeGlobals.assign(m_sceneMesh.nodes.size(), identity());
    std::vector<char> visited(m_sceneMesh.nodes.size(), 0);
    std::vector<char> visiting(m_sceneMesh.nodes.size(), 0);

    auto evaluate = [&](auto&& self, int nodeIndex) -> Mat4
    {
        if (nodeIndex < 0 || static_cast<std::size_t>(nodeIndex) >= m_sceneMesh.nodes.size())
            return identity();
        const std::size_t index = static_cast<std::size_t>(nodeIndex);
        if (visited[index])
            return m_nodeGlobals[index];
        if (visiting[index])
            return identity();
        visiting[index] = 1;
        const auto& node = m_sceneMesh.nodes[index];
        Mat4 local{};
        if (node.hasMatrix)
            std::copy(node.localMatrix.begin(), node.localMatrix.end(), local.m);
        else
            local = trs(node);
        if (node.parentIndex >= 0)
            m_nodeGlobals[index] = multiply(self(self, node.parentIndex), local);
        else
            m_nodeGlobals[index] = local;
        visiting[index] = 0;
        visited[index] = 1;
        return m_nodeGlobals[index];
    };

    for (std::size_t i = 0; i < m_sceneMesh.nodes.size(); ++i)
        evaluate(evaluate, static_cast<int>(i));
}

void StudioScenePreview::rebuildRaycastTriangles()
{
    m_raycastTriangles.clear();
    m_boundsCenter = {};
    m_boundsRadius = 1.0f;
    const std::size_t stride = m_sceneMesh.vertexStrideFloats;
    if (stride < 3)
        return;

    authoring::Vec3 boundsMin{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max() };
    authoring::Vec3 boundsMax{
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max() };
    const auto includeBounds = [&](const authoring::Vec3& point)
    {
        boundsMin.x = std::min(boundsMin.x, point.x); boundsMin.y = std::min(boundsMin.y, point.y); boundsMin.z = std::min(boundsMin.z, point.z);
        boundsMax.x = std::max(boundsMax.x, point.x); boundsMax.y = std::max(boundsMax.y, point.y); boundsMax.z = std::max(boundsMax.z, point.z);
    };

    m_raycastTriangles.reserve(m_sceneMesh.indices.size() / 3);
    for (const auto& range : m_sceneMesh.drawRanges)
    {
        if (range.indexCount < 3 || hiddenAuthoringRange(range))
            continue;
        Mat4 model = identity();
        if (range.nodeIndex >= 0 && static_cast<std::size_t>(range.nodeIndex) < m_nodeGlobals.size())
            model = m_nodeGlobals[static_cast<std::size_t>(range.nodeIndex)];

        const std::size_t end = std::min(range.firstIndex + range.indexCount, m_sceneMesh.indices.size());
        for (std::size_t i = range.firstIndex; i + 2 < end; i += 3)
        {
            const unsigned int ia = m_sceneMesh.indices[i + 0];
            const unsigned int ib = m_sceneMesh.indices[i + 1];
            const unsigned int ic = m_sceneMesh.indices[i + 2];
            if ((static_cast<std::size_t>(ia) + 1) * stride > m_sceneMesh.vertices.size()
                || (static_cast<std::size_t>(ib) + 1) * stride > m_sceneMesh.vertices.size()
                || (static_cast<std::size_t>(ic) + 1) * stride > m_sceneMesh.vertices.size())
                continue;
            const auto point = [&](unsigned int index) -> authoring::Vec3
            {
                const std::size_t base = static_cast<std::size_t>(index) * stride;
                return transformPoint(model, {
                    m_sceneMesh.vertices[base + 0],
                    m_sceneMesh.vertices[base + 1],
                    m_sceneMesh.vertices[base + 2]
                });
            };
            const authoring::Vec3 a = point(ia);
            const authoring::Vec3 b = point(ib);
            const authoring::Vec3 c = point(ic);
            includeBounds(a); includeBounds(b); includeBounds(c);
            m_raycastTriangles.push_back({ a, b, c });
        }
    }

    if (!m_raycastTriangles.empty())
    {
        m_boundsCenter = {
            (boundsMin.x + boundsMax.x) * 0.5f,
            (boundsMin.y + boundsMax.y) * 0.5f,
            (boundsMin.z + boundsMax.z) * 0.5f };
        const authoring::Vec3 extent{
            boundsMax.x - m_boundsCenter.x,
            boundsMax.y - m_boundsCenter.y,
            boundsMax.z - m_boundsCenter.z };
        m_boundsRadius = std::max(1.0f, std::sqrt(dot(extent, extent)));
    }
}

bool StudioScenePreview::raycast(const authoring::Vec3& origin, const authoring::Vec3& direction,
    authoring::Vec3& hitPosition) const
{
    if (!m_loaded || m_raycastTriangles.empty())
        return false;

    float nearest = std::numeric_limits<float>::max();
    bool hit = false;
    for (const auto& triangle : m_raycastTriangles)
    {
        float t = 0.0f;
        if (intersectTriangle(origin, direction, triangle.a, triangle.b, triangle.c, t) && t < nearest)
        {
            nearest = t;
            hit = true;
        }
    }
    if (!hit)
        return false;
    hitPosition = {
        origin.x + direction.x * nearest,
        origin.y + direction.y * nearest,
        origin.z + direction.z * nearest
    };
    return true;
}

StudioScenePreview::Mat4 StudioScenePreview::identity()
{
    Mat4 result{};
    result.m[0] = result.m[5] = result.m[10] = result.m[15] = 1.0f;
    return result;
}

StudioScenePreview::Mat4 StudioScenePreview::multiply(const Mat4& a, const Mat4& b)
{
    Mat4 result{};
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            float value = 0.0f;
            for (int k = 0; k < 4; ++k)
                value += a.m[k * 4 + row] * b.m[column * 4 + k];
            result.m[column * 4 + row] = value;
        }
    }
    return result;
}

StudioScenePreview::Mat4 StudioScenePreview::trs(const heritage::graphics::MeshNode& node)
{
    const float x = node.rotation[0];
    const float y = node.rotation[1];
    const float z = node.rotation[2];
    const float w = node.rotation[3];
    Mat4 result = identity();
    result.m[0] = (1.0f - 2.0f * (y * y + z * z)) * node.scale[0];
    result.m[1] = (2.0f * (x * y + z * w)) * node.scale[0];
    result.m[2] = (2.0f * (x * z - y * w)) * node.scale[0];
    result.m[4] = (2.0f * (x * y - z * w)) * node.scale[1];
    result.m[5] = (1.0f - 2.0f * (x * x + z * z)) * node.scale[1];
    result.m[6] = (2.0f * (y * z + x * w)) * node.scale[1];
    result.m[8] = (2.0f * (x * z + y * w)) * node.scale[2];
    result.m[9] = (2.0f * (y * z - x * w)) * node.scale[2];
    result.m[10] = (1.0f - 2.0f * (x * x + y * y)) * node.scale[2];
    result.m[12] = node.translation[0];
    result.m[13] = node.translation[1];
    result.m[14] = node.translation[2];
    return result;
}

StudioScenePreview::Mat4 StudioScenePreview::viewMatrix(const StudioPreviewCamera& camera)
{
    Mat4 result = identity();
    const auto& r = camera.right;
    const auto& u = camera.up;
    const auto& f = camera.forward;
    result.m[0] = r.x; result.m[4] = r.y; result.m[8] = r.z;
    result.m[1] = u.x; result.m[5] = u.y; result.m[9] = u.z;
    result.m[2] = -f.x; result.m[6] = -f.y; result.m[10] = -f.z;
    result.m[12] = -(r.x * camera.position.x + r.y * camera.position.y + r.z * camera.position.z);
    result.m[13] = -(u.x * camera.position.x + u.y * camera.position.y + u.z * camera.position.z);
    result.m[14] = f.x * camera.position.x + f.y * camera.position.y + f.z * camera.position.z;
    return result;
}

StudioScenePreview::Mat4 StudioScenePreview::perspectiveMatrix(float fovY, float aspect, float zNear, float zFar)
{
    Mat4 result{};
    const float f = 1.0f / std::tan(fovY * 0.5f);
    result.m[0] = f / std::max(aspect, 0.001f);
    result.m[5] = f;
    result.m[10] = (zFar + zNear) / (zNear - zFar);
    result.m[11] = -1.0f;
    result.m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
    return result;
}

StudioScenePreview::Mat4 StudioScenePreview::orthographicMatrix(float halfHeight, float aspect, float zNear, float zFar)
{
    Mat4 result = identity();
    const float halfWidth = halfHeight * std::max(aspect, 0.001f);
    result.m[0] = 1.0f / std::max(halfWidth, 0.001f);
    result.m[5] = 1.0f / std::max(halfHeight, 0.001f);
    result.m[10] = -2.0f / (zFar - zNear);
    result.m[14] = -(zFar + zNear) / (zFar - zNear);
    return result;
}

authoring::Vec3 StudioScenePreview::transformPoint(const Mat4& matrix, const authoring::Vec3& point)
{
    return {
        matrix.m[0] * point.x + matrix.m[4] * point.y + matrix.m[8] * point.z + matrix.m[12],
        matrix.m[1] * point.x + matrix.m[5] * point.y + matrix.m[9] * point.z + matrix.m[13],
        matrix.m[2] * point.x + matrix.m[6] * point.y + matrix.m[10] * point.z + matrix.m[14]
    };
}

float StudioScenePreview::determinant3x3(const Mat4& m)
{
    return m.m[0] * (m.m[5] * m.m[10] - m.m[9] * m.m[6])
        - m.m[4] * (m.m[1] * m.m[10] - m.m[9] * m.m[2])
        + m.m[8] * (m.m[1] * m.m[6] - m.m[5] * m.m[2]);
}

} // namespace heritage::studio

#include "WeatherPresentationRenderer.hpp"

#include "../ShaderProgram.hpp"
#include "../EnvironmentMap.hpp"
#include "../../Physics/Surfaces/SurfaceWorld.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

namespace heritage::graphics {
namespace {

#ifdef _WIN32
#define HERITAGE_WEATHER_GLSL_VERSION "#version 460 core\n"
#else
#define HERITAGE_WEATHER_GLSL_VERSION "#version 330 core\n"
#endif

// WEATHER07C6: explicit user-calibrated visual-range populations. These are
// GPU visual representative budgets, not literal physical raindrop counts.
// The authoritative rainfall mass and WEATHER07A microphysics stay unchanged.
// Compute keeps world-space/frustum compaction, but each population owns one
// non-overlapping horizontal distance band.
constexpr std::uint32_t kRainGpuNearCandidates = 10000u;  // 0-2 m
constexpr std::uint32_t kRainGpuMidCandidates = 100000u; // 2-10 m
constexpr std::uint32_t kRainGpuFarCandidates = 10000u;   // 10-100 m
constexpr std::uint32_t kRainGpuMaxInstances =
    kRainGpuNearCandidates + kRainGpuMidCandidates + kRainGpuFarCandidates;
constexpr std::size_t kRainGpuRecordBytes = sizeof(float) * 8u;
constexpr std::uint32_t kRainComputeLocalSize = 256u;

// Static corners are supplied from a tiny VBO instead of relying on an empty
// VAO + gl_VertexID. This keeps the instanced rain path conservative across
// OpenGL drivers while still doing all per-drop placement on the GPU.
// WEATHER07B7 modern OpenGL path: precipitation representatives are generated
// entirely on the GPU. The compute shader writes camera-relative physical drop
// records into an SSBO; the vertex shader consumes those records directly.
// There is no per-drop CPU loop and no CPU dynamic rain VBO in the normal path.
const char* kRainComputeShader = R"glsl(#version 460 core
layout(local_size_x = 256) in;

struct RainRecord
{
    vec4 centerDiameter;   // xyz camera-relative metres, w physical diameter metres
    vec4 velocityAlpha;    // xyz physical m/s, w tier optical weight
};
layout(std430, binding = 0) writeonly buffer RainRecords
{
    RainRecord records[];
};

// The same 16-byte buffer is later bound as GL_DRAW_INDIRECT_BUFFER. Compute
// atomically compacts surviving representatives directly into records[] and
// increments instanceCount, so the CPU never reads back a visible-drop count.
layout(std430, binding = 1) buffer RainDrawCommand
{
    uint vertexCount;
    uint instanceCount;
    uint firstVertex;
    uint baseInstance;
} drawCommand;

uniform uint uInstanceCount;
uniform ivec3 uBaseCell;
uniform vec3 uCameraRemainder;
uniform ivec3 uGridSize;
uniform vec2 uCellSize; // x horizontal X/Z, y vertical
uniform int uVerticalOffset;
uniform float uRainStrength;
uniform vec2 uWindVelocityXZ;
uniform float uTime;
uniform float uDropLambdaPerMm;
uniform float uMinimumDiameterMm;
uniform float uMaximumDiameterMm;
uniform float uTierAlpha;
uniform uint uTierSalt;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uMinimumRadiusM;
uniform float uMaximumRadiusM;

uint hash32(uint x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

uint hashCell(ivec3 cell, uint lane, uint salt)
{
    uint h = hash32(uint(cell.x) ^ 0x9e3779b9u ^ salt);
    h = hash32(h ^ uint(cell.y) * 0x85ebca6bu);
    h = hash32(h ^ uint(cell.z) * 0xc2b2ae35u);
    h = hash32(h ^ lane * 0x27d4eb2du);
    return h;
}

float unitRandom(uint value)
{
    return float(value & 0x00ffffffu) * (1.0 / 16777216.0);
}

float sampleDiameterMm(float uniform01)
{
    float lambda = max(uDropLambdaPerMm, 0.0001);
    float minimumD = max(uMinimumDiameterMm, 0.01);
    float maximumD = max(uMaximumDiameterMm, minimumD + 0.01);
    float expMin = exp(-lambda * minimumD);
    float expMax = exp(-lambda * maximumD);
    float expSample = expMin - clamp(uniform01, 0.0, 1.0) * (expMin - expMax);
    return clamp(-log(max(expSample, 1.0e-10)) / lambda, minimumD, maximumD);
}

float terminalVelocityMps(float diameterMm)
{
    float atlasTransition = 9.65 - 10.3 * exp(-0.6 * 0.50);
    if (diameterMm < 0.50)
        return max(atlasTransition * diameterMm / 0.50, 0.0);
    return clamp(9.65 - 10.3 * exp(-0.6 * diameterMm), 0.0, 9.65);
}

void main()
{
    uint invocation = gl_GlobalInvocationID.x;
    if (invocation >= uInstanceCount)
        return;

    uint gridX = uint(max(uGridSize.x, 1));
    uint gridZ = uint(max(uGridSize.z, 1));
    uint gridY = uint(max(uGridSize.y, 1));
    uint cellCount = gridX * gridZ * gridY;
    uint cellIndex = invocation % cellCount;
    uint lane = invocation / cellCount;

    uint ix = cellIndex % gridX;
    uint iz = (cellIndex / gridX) % gridZ;
    uint iy = cellIndex / (gridX * gridZ);
    ivec3 localCell = ivec3(
        int(ix) - int(gridX / 2u),
        int(iy) + uVerticalOffset,
        int(iz) - int(gridZ / 2u));
    ivec3 absoluteCell = uBaseCell + localCell;

    uint seed = hashCell(absoluteCell, lane, uTierSalt);
    float randomX = unitRandom(hash32(seed ^ 0x12f3a5c7u));
    float randomY = unitRandom(hash32(seed ^ 0x9b71d34du));
    float randomZ = unitRandom(hash32(seed ^ 0x6d2b79f5u));
    float randomD = unitRandom(hash32(seed ^ 0xa24baed5u));

    float diameterMm = sampleDiameterMm(randomD);
    float diameterM = clamp(diameterMm * 0.001, 0.00020, 0.00600);
    float fallSpeed = max(terminalVelocityMps(diameterMm), 0.1);
    float windCoupling = clamp(1.08 - diameterMm * 0.105, 0.38, 1.0);
    vec2 windVelocity = uWindVelocityXZ * windCoupling;

    float phase = fract(randomY + uTime * fallSpeed / max(uCellSize.y, 0.1));
    vec3 center = vec3(
        float(localCell.x) * uCellSize.x + randomX * uCellSize.x - uCameraRemainder.x,
        float(localCell.y + 1) * uCellSize.y - phase * uCellSize.y - uCameraRemainder.y,
        float(localCell.z) * uCellSize.x + randomZ * uCellSize.x - uCameraRemainder.z);
    center.xz += windVelocity * phase * uCellSize.y / fallSpeed;

    // WEATHER07C5: the radial limits are authoritative presentation bands.
    // Keep frustum compaction (the expensive part we actually want) but do not
    // add hidden cross-tier distance fades that would thin the requested counts.
    float horizontalDistanceM = length(center.xz);
    if (horizontalDistanceM < uMinimumRadiusM
        || horizontalDistanceM >= uMaximumRadiusM)
        return;

    vec4 viewPosition = uView * vec4(center, 1.0);
    if (viewPosition.z >= -0.05)
        return;
    vec4 clipPosition = uProjection * viewPosition;
    if (clipPosition.w <= 0.0)
        return;
    vec2 ndc = clipPosition.xy / clipPosition.w;
    const float frustumMargin = 1.16;
    if (abs(ndc.x) > frustumMargin || abs(ndc.y) > frustumMargin)
        return;

    uint outputIndex = atomicAdd(drawCommand.instanceCount, 1u);
    records[outputIndex].centerDiameter = vec4(center, diameterM);
    records[outputIndex].velocityAlpha = vec4(
        windVelocity.x,
        -fallSpeed,
        windVelocity.y,
        uTierAlpha * mix(0.72, 1.0, clamp(uRainStrength, 0.0, 1.0)));
}
)glsl";

const char* kRainVertexShader = HERITAGE_WEATHER_GLSL_VERSION R"glsl(
layout(location=0) in vec2 aCorner; // x=-1..1 width, y=0..1 head -> optical tail

struct RainRecord
{
    vec4 centerDiameter;
    vec4 velocityAlpha;
};
layout(std430, binding = 0) readonly buffer RainRecords
{
    RainRecord records[];
};

uniform mat4 uView;
uniform mat4 uProjection;
uniform float uRainStrength;
uniform vec2 uViewportSize;

out float vAlpha;
out vec2 vUv;
out vec3 vPositionView;
out vec3 vSideView;
out vec3 vUpView;
out float vDiameterMm;

void main()
{
    RainRecord record = records[gl_InstanceID];
    vec3 center = record.centerDiameter.xyz;
    float physicalDiameterM = clamp(record.centerDiameter.w, 0.00020, 0.00600);
    vec3 velocityWorld = record.velocityAlpha.xyz;
    float physicalSpeed = max(length(velocityWorld), 0.1);
    vec3 centerView = (uView * vec4(center, 1.0)).xyz;
    vec3 fallDirectionView = normalize(mat3(uView) * normalize(velocityWorld));
    vec3 upView = -fallDirectionView;
    vec3 sideView = cross(fallDirectionView, vec3(0.0, 0.0, -1.0));
    float sideLength = length(sideView);
    sideView = sideLength > 0.0001 ? sideView / sideLength : vec3(1.0, 0.0, 0.0);

    // Physical drop diameter remains millimetres. The visible streak is the
    // travelled distance over a finite optical exposure interval.
    float exposureSeconds = mix(
        1.0 / 180.0,
        1.0 / 90.0,
        clamp(uRainStrength, 0.0, 1.0));
    float physicalStreakLength = max(
        physicalDiameterM,
        physicalSpeed * exposureSeconds);

    float viewDepth = max(abs(centerView.z), 0.10);
    float projectionX = max(abs(uProjection[0][0]), 0.0001);
    float projectionY = max(abs(uProjection[1][1]), 0.0001);
    float pixelWorldWidth = (2.0 * viewDepth)
        / (projectionX * max(uViewportSize.x, 1.0));
    float pixelWorldHeight = (2.0 * viewDepth)
        / (projectionY * max(uViewportSize.y, 1.0));
    float rasterDiameterM = max(physicalDiameterM, pixelWorldWidth * 1.20);
    float rasterStreakLengthM = max(
        physicalStreakLength,
        pixelWorldHeight * 1.05);
    float opticalAreaCompensation = clamp(
        (physicalDiameterM * physicalStreakLength)
            / max(rasterDiameterM * rasterStreakLengthM, 1.0e-9),
        0.0,
        1.0);

    vec3 positionView = centerView
        + upView * (rasterStreakLengthM * aCorner.y)
        + sideView * (0.5 * rasterDiameterM * aCorner.x);

    vUv = vec2(aCorner.x * 0.5 + 0.5, 1.0 - aCorner.y);
    vPositionView = positionView;
    vSideView = sideView;
    vUpView = upView;
    vDiameterMm = physicalDiameterM * 1000.0;

    // WEATHER07C5: compute owns the exact 0-2 / 2-10 / 10-100 m bands.
    // Do not apply a second vertex-distance fade that silently shortens them.
    vAlpha = record.velocityAlpha.w * opticalAreaCompensation;
    gl_Position = uProjection * vec4(positionView, 1.0);
}
)glsl";

const char* kRainFragmentShader = HERITAGE_WEATHER_GLSL_VERSION R"glsl(
in float vAlpha;
in vec2 vUv;
in vec3 vPositionView;
in vec3 vSideView;
in vec3 vUpView;
in float vDiameterMm;

uniform sampler2D uRainOpacityTexture;
uniform bool uHasOpticalTextures;
uniform samplerCube uEnvironmentMap;
uniform bool uHasEnvironmentMap;
uniform float uEnvironmentMaxLod;
uniform mat3 uViewToWorld;

out vec4 FragColor;

void main()
{
    if (vAlpha <= 0.001)
        discard;

    vec2 p = vec2((vUv.x - 0.5) * 2.0, vUv.y * 2.0 - 1.0);
    float coverage;
    vec3 baseColor;
    if (uHasOpticalTextures)
    {
        // WEATHER07B9: simplified optical material. Use only the supplied base
        // colour + alpha texture, then layer environment reflection on top.
        // Normal and thickness textures are intentionally ignored here.
        vec4 opacitySample = texture(uRainOpacityTexture, vUv);
        coverage = opacitySample.a;
        baseColor = max(opacitySample.rgb, vec3(0.84, 0.88, 0.92));
    }
    else
    {
        // Compatibility shape if the module has not supplied the optional
        // optical material yet. This path is intentionally subdued; the old
        // opaque white-line rain is no longer the normal presentation.
        coverage = 1.0 - smoothstep(0.72, 1.0, length(p));
        baseColor = vec3(0.88, 0.91, 0.95);
    }

    if (coverage <= 0.006)
        discard;

    // Use a cheap procedural bulge derived only from UVs so the drop can still
    // reflect the environment without depending on normal/thickness textures.
    vec3 tangentNormal = normalize(vec3(p.x * 0.62, p.y * 0.34, 1.0));
    vec3 faceView = vec3(0.0, 0.0, 1.0);
    vec3 normalView = normalize(
        vSideView * tangentNormal.x
        + vUpView * tangentNormal.y
        + faceView * max(tangentNormal.z, 0.05));
    vec3 viewDirectionView = normalize(-vPositionView);
    float nDotV = max(dot(normalView, viewDirectionView), 0.001);
    float fresnel = 0.020 + 0.980 * pow(1.0 - nDotV, 5.0);

    vec3 viewDirectionWorld = normalize(uViewToWorld * viewDirectionView);
    vec3 normalWorld = normalize(uViewToWorld * normalView);
    vec3 reflection = vec3(0.78, 0.84, 0.90);
    if (uHasEnvironmentMap)
    {
        float roughness = 0.030;
        reflection = textureLod(
            uEnvironmentMap,
            reflect(-viewDirectionWorld, normalWorld),
            roughness * uEnvironmentMaxLod).rgb;
    }

    float core = smoothstep(0.06, 0.90, coverage);
    float edge = 1.0 - smoothstep(0.10, 0.95, length(vec2(p.x, p.y * 0.92)));
    float sparkle = pow(clamp(fresnel + edge * 0.26, 0.0, 1.0), 1.45);
    vec3 transmissionTint = mix(baseColor, reflection, 0.26);
    vec3 color = mix(
        transmissionTint,
        reflection,
        clamp(0.26 + fresnel * 0.82 + edge * 0.24, 0.0, 0.975));
    color += reflection * sparkle * 0.22;

    float diameterVisibility = mix(0.82, 1.10, clamp(vDiameterMm / 2.0, 0.0, 1.0));
    float alpha = coverage * vAlpha * diameterVisibility
        * mix(0.58, 0.94, core);
    alpha = clamp(alpha, 0.0, 0.84);
    if (alpha <= 0.003)
        discard;

    FragColor = vec4(max(color, vec3(0.0)), alpha);
}
)glsl";

heritage::math::Vec3 subtract(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

float dot(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

heritage::math::Vec3 cross(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

heritage::math::Vec3 normalize(
    const heritage::math::Vec3& v,
    const heritage::math::Vec3& fallback)
{
    const float lengthSquared = dot(v, v);
    if (lengthSquared <= 1.0e-10f)
        return fallback;
    const float invLength = 1.0f / std::sqrt(lengthSquared);
    return { v.x * invLength, v.y * invLength, v.z * invLength };
}

heritage::math::Mat4 lookAt(
    const heritage::math::Vec3& eye,
    const heritage::math::Vec3& target,
    const heritage::math::Vec3& up)
{
    const heritage::math::Vec3 forward = normalize(
        subtract(target, eye), { 0.0f, 0.0f, -1.0f });
    const heritage::math::Vec3 side = normalize(
        cross(forward, up), { 1.0f, 0.0f, 0.0f });
    const heritage::math::Vec3 correctedUp = cross(side, forward);
    heritage::math::Mat4 result = heritage::math::identity();
    result.m[0] = side.x;
    result.m[1] = correctedUp.x;
    result.m[2] = -forward.x;
    result.m[4] = side.y;
    result.m[5] = correctedUp.y;
    result.m[6] = -forward.y;
    result.m[8] = side.z;
    result.m[9] = correctedUp.z;
    result.m[10] = -forward.z;
    result.m[12] = -dot(side, eye);
    result.m[13] = -dot(correctedUp, eye);
    result.m[14] = dot(forward, eye);
    return result;
}

} // namespace

bool WeatherPresentationRenderer::loadRainOpticalMaterial(
    const std::filesystem::path& moduleAssetRoot)
{
    m_rainOpacityTexture = 0;
    m_rainTextureWidth = 0;
    m_rainTextureHeight = 0;
    m_rainOpticalTexturesReady = false;

    const std::filesystem::path rainRoot =
        moduleAssetRoot / "Weather" / "Rain";
    std::string error;
    const Texture2D* opacity = m_rainTextureCache.acquire(
        rainRoot / "RainDrop_BC.png",
        TextureColorSpace::Linear,
        2,
        false,
        error);
    if (!opacity)
    {
        std::cerr << "WEATHER07C1 rain base/alpha texture unavailable: "
                  << error << '\n';
        return false;
    }

    m_rainOpacityTexture = opacity->id;
    m_rainTextureWidth = opacity->width;
    m_rainTextureHeight = opacity->height;
    m_rainOpticalTexturesReady = m_rainOpacityTexture != 0;

    if (m_rainOpticalTexturesReady)
    {
        GLint previousTexture = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
        glBindTexture(GL_TEXTURE_2D, m_rainOpacityTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    }

    return m_rainOpticalTexturesReady;
}

bool WeatherPresentationRenderer::initialize(
    const std::filesystem::path& moduleAssetRoot)
{
    shutdown();
    m_rainProgram = buildShaderProgram(kRainVertexShader, kRainFragmentShader);
    m_rainComputeProgram = buildComputeShaderProgram(kRainComputeShader);

    // buildShaderProgram historically returns a non-zero program object even if
    // OpenGL link failed. Verify the executable link state here so weather
    // diagnostics and fallback selection cannot be fooled by a dead program.
    const auto discardUnlinkedProgram = [](GLuint& program) {
        if (program == 0)
            return;
        GLint linked = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (linked == GL_FALSE)
        {
            glDeleteProgram(program);
            program = 0;
        }
    };
    discardUnlinkedProgram(m_rainProgram);
    discardUnlinkedProgram(m_rainComputeProgram);

    // The authored textured rain is the sole airborne-rain presentation path.
    // WEATHER07B7 intentionally requires modern OpenGL compute + SSBO support;
    // there is no second procedural fullscreen rain system layered over it.
    if (!m_rainProgram || !m_rainComputeProgram)
    {
        shutdown();
        return false;
    }

    glGenVertexArrays(1, &m_rainVao);
    glGenBuffers(1, &m_rainCornerVbo);
    glGenBuffers(1, &m_rainComputeSsbo);
    glGenBuffers(1, &m_rainIndirectBuffer);
    if (!m_rainVao || !m_rainCornerVbo || !m_rainComputeSsbo
        || !m_rainIndirectBuffer)
    {
        shutdown();
        return false;
    }

    // WEATHER07C0: one tiny static VBO now supplies a single UV-unwrapped
    // triangle for every instanced streak instead of a full quad. This trims
    // 25% of the streak vertex workload and avoids shading transparent corner
    // pixels that existed only to fit the old rectangular billboard. The rain
    // texture can therefore be kept very small (for example 16x32 or 32x16,
    // depending naming convention) without wasting work on quad corners.
    constexpr float corners[] = {
         0.0f, 0.0f,
        -1.0f, 1.0f,
         1.0f, 1.0f
    };
    glBindVertexArray(m_rainVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_rainCornerVbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(sizeof(corners)),
        corners,
        GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // WEATHER07B7: one persistent-sized SSBO holds the maximum modern-GPU rain
    // population. Compute overwrites only the active prefix each frame.
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_rainComputeSsbo);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        static_cast<GLsizeiptr>(
            static_cast<std::size_t>(kRainGpuMaxInstances) * kRainGpuRecordBytes),
        nullptr,
        GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_rainComputeSsbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // GPU-driven compacted draw command: {vertexCount, instanceCount, first,
    // baseInstance}. Compute atomically updates instanceCount and the exact
    // compacted population is consumed by glDrawArraysIndirect with no readback.
    const std::uint32_t initialRainDrawCommand[4] = { 3u, 0u, 0u, 0u };
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_rainIndirectBuffer);
    glBufferData(
        GL_DRAW_INDIRECT_BUFFER,
        static_cast<GLsizeiptr>(sizeof(initialRainDrawCommand)),
        initialRainDrawCommand,
        GL_DYNAMIC_DRAW);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_rainIndirectBuffer);

    if (m_rainProgram)
    {
        glUseProgram(m_rainProgram);
        m_uniformView = glGetUniformLocation(m_rainProgram, "uView");
        m_uniformProjection = glGetUniformLocation(m_rainProgram, "uProjection");
        m_uniformRainStrength = glGetUniformLocation(
            m_rainProgram, "uRainStrength");
        m_uniformViewportSize = glGetUniformLocation(
            m_rainProgram, "uViewportSize");
        m_uniformRainOpacityTexture = glGetUniformLocation(
            m_rainProgram, "uRainOpacityTexture");
        m_uniformHasOpticalTextures = glGetUniformLocation(
            m_rainProgram, "uHasOpticalTextures");
        m_uniformEnvironmentMap = glGetUniformLocation(
            m_rainProgram, "uEnvironmentMap");
        m_uniformHasEnvironmentMap = glGetUniformLocation(
            m_rainProgram, "uHasEnvironmentMap");
        m_uniformEnvironmentMaxLod = glGetUniformLocation(
            m_rainProgram, "uEnvironmentMaxLod");
        m_uniformViewToWorld = glGetUniformLocation(
            m_rainProgram, "uViewToWorld");
        if (m_uniformRainOpacityTexture >= 0)
            glUniform1i(m_uniformRainOpacityTexture, 8);
        if (m_uniformEnvironmentMap >= 0)
            glUniform1i(m_uniformEnvironmentMap, 11);
    }

    if (m_rainComputeProgram)
    {
        glUseProgram(m_rainComputeProgram);
        m_computeUniformInstanceCount = glGetUniformLocation(
            m_rainComputeProgram, "uInstanceCount");
        m_computeUniformBaseCell = glGetUniformLocation(
            m_rainComputeProgram, "uBaseCell");
        m_computeUniformCameraRemainder = glGetUniformLocation(
            m_rainComputeProgram, "uCameraRemainder");
        m_computeUniformGridSize = glGetUniformLocation(
            m_rainComputeProgram, "uGridSize");
        m_computeUniformCellSize = glGetUniformLocation(
            m_rainComputeProgram, "uCellSize");
        m_computeUniformVerticalOffset = glGetUniformLocation(
            m_rainComputeProgram, "uVerticalOffset");
        m_computeUniformRainStrength = glGetUniformLocation(
            m_rainComputeProgram, "uRainStrength");
        m_computeUniformWindVelocityXZ = glGetUniformLocation(
            m_rainComputeProgram, "uWindVelocityXZ");
        m_computeUniformTime = glGetUniformLocation(
            m_rainComputeProgram, "uTime");
        m_computeUniformDropLambda = glGetUniformLocation(
            m_rainComputeProgram, "uDropLambdaPerMm");
        m_computeUniformMinimumDiameter = glGetUniformLocation(
            m_rainComputeProgram, "uMinimumDiameterMm");
        m_computeUniformMaximumDiameter = glGetUniformLocation(
            m_rainComputeProgram, "uMaximumDiameterMm");
        m_computeUniformTierAlpha = glGetUniformLocation(
            m_rainComputeProgram, "uTierAlpha");
        m_computeUniformTierSalt = glGetUniformLocation(
            m_rainComputeProgram, "uTierSalt");
        m_computeUniformView = glGetUniformLocation(
            m_rainComputeProgram, "uView");
        m_computeUniformProjection = glGetUniformLocation(
            m_rainComputeProgram, "uProjection");
        m_computeUniformMinimumRadius = glGetUniformLocation(
            m_rainComputeProgram, "uMinimumRadiusM");
        m_computeUniformMaximumRadius = glGetUniformLocation(
            m_rainComputeProgram, "uMaximumRadiusM");
    }

    // WEATHER07C1: rain now needs only one tiny base-colour/alpha optical map.
    // Reflection is procedural/environment-driven; normal/thickness maps are no
    // longer part of the runtime rain material contract.
    loadRainOpticalMaterial(moduleAssetRoot);

    glUseProgram(0);
    return true;
}

void WeatherPresentationRenderer::shutdown()
{
    if (m_rainCornerVbo)
        glDeleteBuffers(1, &m_rainCornerVbo);
    if (m_rainComputeSsbo)
        glDeleteBuffers(1, &m_rainComputeSsbo);
    if (m_rainIndirectBuffer)
        glDeleteBuffers(1, &m_rainIndirectBuffer);
    if (m_rainVao)
        glDeleteVertexArrays(1, &m_rainVao);
    if (m_rainComputeProgram)
        glDeleteProgram(m_rainComputeProgram);
    if (m_rainProgram)
        glDeleteProgram(m_rainProgram);
    m_rainCornerVbo = 0;
    m_rainComputeSsbo = 0;
    m_rainIndirectBuffer = 0;
    m_rainVao = 0;
    m_rainProgram = 0;
    m_rainComputeProgram = 0;
    m_rainTextureCache.clear();
    m_rainOpacityTexture = 0;
    m_rainTextureWidth = 0;
    m_rainTextureHeight = 0;
    m_rainOpticalTexturesReady = false;
    m_frameStats = {};
}

void WeatherPresentationRenderer::draw(
    const heritage::physics::SurfaceWorld& surfaces,
    const heritage::math::Mat4& projection,
    const heritage::camera::CameraFrame& cameraFrame,
    float elapsedSeconds,
    const heritage::graphics::EnvironmentMap& environmentMap)
{
    (void)elapsedSeconds;
    m_frameStats.rendererReady =
        m_rainVao != 0
        && m_rainComputeSsbo != 0
        && m_rainComputeProgram != 0
        && m_rainProgram != 0;
    m_frameStats.opticalTexturesReady = m_rainOpticalTexturesReady;
    m_frameStats.opticalTextureWidth = m_rainTextureWidth;
    m_frameStats.opticalTextureHeight = m_rainTextureHeight;
    const auto& weather = surfaces.weather();
    m_frameStats.precipitationRateMmPerHour =
        weather.enabled ? weather.precipitationRateMmPerHour : 0.0;
    if (!m_frameStats.rendererReady)
        return;
    if (!weather.enabled || weather.precipitationRateMmPerHour <= 0.01)
        return;

    const auto started = std::chrono::steady_clock::now();
    const heritage::math::Vec3 eyeLocal = cameraFrame.valid
        ? cameraFrame.eyeLocal
        : heritage::math::Vec3{ 0.0f, 3.4f, 8.5f };
    const heritage::math::Vec3 targetLocal = cameraFrame.valid
        ? cameraFrame.targetLocal
        : heritage::math::Vec3{ 0.0f, 1.0f, 0.0f };
    const heritage::math::Vec3 up = cameraFrame.valid
        ? cameraFrame.up
        : heritage::math::Vec3{ 0.0f, 1.0f, 0.0f };
    const heritage::math::Vec3 relativeTarget = subtract(targetLocal, eyeLocal);
    const heritage::math::Mat4 view = lookAt(
        { 0.0f, 0.0f, 0.0f }, relativeTarget, up);
    const heritage::math::DVec3 cameraGlobal = surfaces.localToGlobal(eyeLocal);
    const heritage::math::Vec3 cameraForward = normalize(
        relativeTarget,
        heritage::math::Vec3{ 0.0f, 0.0f, -1.0f });
    const heritage::math::Vec3 cameraRight = normalize(
        cross(cameraForward, up),
        heritage::math::Vec3{ 1.0f, 0.0f, 0.0f });
    const heritage::math::Vec3 cameraUp = normalize(
        cross(cameraRight, cameraForward),
        heritage::math::Vec3{ 0.0f, 1.0f, 0.0f });
    // Column-major view-space -> world-space rotation. View +Z points behind
    // the camera, hence the third basis column is -forward.
    const float viewToWorld[9] = {
        cameraRight.x, cameraRight.y, cameraRight.z,
        cameraUp.x, cameraUp.y, cameraUp.z,
        -cameraForward.x, -cameraForward.y, -cameraForward.z };

    // WEATHER06H: shelter must be a real surface in the SAME hydrology X/Z
    // column above the camera. The WEATHER06F/G radius gather could classify
    // steep neighbouring terrain as an overhead roof and return before either
    // rain draw, which is exactly what the live F8 capture (0 draws) exposed.
    // This exact layered-column query keeps open slopes rainy while still
    // suppressing the pass when the camera is genuinely under a bridge/roof.
    const bool cameraUnderPrecipitationCover =
        surfaces.hydrology().hasPrecipitationCoverAbove(
            cameraGlobal, 1.0, 24.0);
    // WEATHER06I: never let shelter classification erase the entire rain pass.
    // The previous WEATHER06H early return is now known to be too dangerous on
    // layered LiDAR terrain. Keep the value as a diagnostic only; the authored
    // world-space textured-rain tiers remain active.
    m_frameStats.suppressedByCover = cameraUnderPrecipitationCover;

    const float rainStrength = std::clamp(
        static_cast<float>(weather.precipitationRateMmPerHour / 80.0),
        0.0f,
        1.0f);

    const auto& precipitation = surfaces.precipitation();
    const auto& rainPopulation = precipitation.rainPopulation();
    m_frameStats.physicalMeanDiameterMm = rainPopulation.valid
        ? rainPopulation.numberWeightedMeanDiameterMm : 0.0;
    m_frameStats.physicalFluxFallSpeedMps = rainPopulation.valid
        ? rainPopulation.fluxWeightedMeanTerminalVelocityMps : 0.0;
    m_frameStats.physicalWindDirectionDegrees =
        precipitation.windDirectionDegrees();
    const heritage::math::Vec3 physicalWind = precipitation.windVelocityMps();
    const float precipitationTime = static_cast<float>(
        precipitation.elapsedSeconds());

    const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean oldDepthMask = GL_TRUE;
    GLint oldActiveTexture = GL_TEXTURE0;
    GLint viewport[4] = { 0, 0, 1, 1 };
    glGetBooleanv(GL_DEPTH_WRITEMASK, &oldDepthMask);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &oldActiveTexture);
    glGetIntegerv(GL_VIEWPORT, viewport);

    // WEATHER06D proved that enabling fixed-function reversed-Z depth testing
    // here can make every transparent rain streak disappear on the live MSAA
    // path. Keep this late transparent pass depth-test disabled. Direct shelter
    // is handled by the conservative layered-surface camera test above; the
    // the authored compute tiers evaluate precipitation in world space.
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(m_rainVao);

    // The module-authored rain texture owns the complete 0-100 m presentation:
    // 120k GPU candidates split into close, mid and far visual bands. Rain rate
    // still owns microphysics/optical strength; counts own density only.
    const std::uint32_t totalGpuCandidates = kRainGpuMaxInstances;

    // Reset the GPU-driven indirect command. Compute increments only the
    // instanceCount word. This 16-byte upload replaces any CPU-side per-drop
    // work or visible-count readback.
    const std::uint32_t rainDrawCommand[4] = { 3u, 0u, 0u, 0u };
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_rainIndirectBuffer);
    glBufferSubData(
        GL_DRAW_INDIRECT_BUFFER,
        0,
        static_cast<GLsizeiptr>(sizeof(rainDrawCommand)),
        rainDrawCommand);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_rainIndirectBuffer);

    glUseProgram(m_rainComputeProgram);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_rainComputeSsbo);
    glUniformMatrix4fv(m_computeUniformView, 1, GL_FALSE, view.m);
    glUniformMatrix4fv(
        m_computeUniformProjection, 1, GL_FALSE, projection.m);
    glUniform1f(m_computeUniformRainStrength, rainStrength);
    glUniform2f(
        m_computeUniformWindVelocityXZ, physicalWind.x, physicalWind.z);
    glUniform1f(m_computeUniformTime, precipitationTime);
    glUniform1f(
        m_computeUniformDropLambda,
        rainPopulation.valid
            ? static_cast<float>(rainPopulation.lambdaPerMm)
            : 4.1f);
    glUniform1f(
        m_computeUniformMinimumDiameter,
        rainPopulation.valid
            ? static_cast<float>(rainPopulation.minimumDiameterMm)
            : 0.20f);
    glUniform1f(
        m_computeUniformMaximumDiameter,
        rainPopulation.valid
            ? static_cast<float>(rainPopulation.maximumDiameterMm)
            : 6.00f);

    const auto dispatchRainTier = [&](std::uint32_t candidateCount,
                                      int gridX,
                                      int gridY,
                                      int gridZ,
                                      double cellXZ,
                                      double cellY,
                                      int verticalOffset,
                                      float minimumRadiusM,
                                      float maximumRadiusM,
                                      float tierAlpha,
                                      std::uint32_t tierSalt) {
        if (candidateCount == 0)
            return;
        const std::int64_t tierBaseX = static_cast<std::int64_t>(
            std::floor(cameraGlobal.x / cellXZ));
        const std::int64_t tierBaseY = static_cast<std::int64_t>(
            std::floor(cameraGlobal.y / cellY));
        const std::int64_t tierBaseZ = static_cast<std::int64_t>(
            std::floor(cameraGlobal.z / cellXZ));
        const float tierRemainderX = static_cast<float>(
            cameraGlobal.x - static_cast<double>(tierBaseX) * cellXZ);
        const float tierRemainderY = static_cast<float>(
            cameraGlobal.y - static_cast<double>(tierBaseY) * cellY);
        const float tierRemainderZ = static_cast<float>(
            cameraGlobal.z - static_cast<double>(tierBaseZ) * cellXZ);

        glUniform1ui(m_computeUniformInstanceCount, candidateCount);
        glUniform3i(
            m_computeUniformBaseCell,
            static_cast<GLint>(tierBaseX),
            static_cast<GLint>(tierBaseY),
            static_cast<GLint>(tierBaseZ));
        glUniform3f(
            m_computeUniformCameraRemainder,
            tierRemainderX,
            tierRemainderY,
            tierRemainderZ);
        glUniform3i(m_computeUniformGridSize, gridX, gridY, gridZ);
        glUniform2f(
            m_computeUniformCellSize,
            static_cast<float>(cellXZ),
            static_cast<float>(cellY));
        glUniform1i(m_computeUniformVerticalOffset, verticalOffset);
        glUniform1f(m_computeUniformMinimumRadius, minimumRadiusM);
        glUniform1f(m_computeUniformMaximumRadius, maximumRadiusM);
        glUniform1f(m_computeUniformTierAlpha, tierAlpha);
        glUniform1ui(m_computeUniformTierSalt, tierSalt);
        const GLuint groups = static_cast<GLuint>(
            (candidateCount + kRainComputeLocalSize - 1u)
                / kRainComputeLocalSize);
        glDispatchCompute(groups, 1, 1);
        ++m_frameStats.rainComputeDispatches;
    };

    // Authored visual-range calibration:
    //    10,000 candidates:   0-2 m
    //   100,000 candidates:   2-10 m
    //    10,000 candidates:  10-100 m
    // Cell footprints are sized to each radial band so candidate work is not
    // squandered across enormous regions only to be rejected by radial culling.
    dispatchRainTier(
        kRainGpuNearCandidates,
        10, 8, 10,
        0.50, 1.25,
        -3,
        0.0f, 2.0f,
        1.0f,
        0xA13F29C7u);

    dispatchRainTier(
        kRainGpuMidCandidates,
        32, 8, 32,
        0.625, 2.75,
        -3,
        2.0f, 10.0f,
        0.72f,
        0xB47D51E3u);

    dispatchRainTier(
        kRainGpuFarCandidates,
        40, 8, 40,
        5.0, 8.0,
        -3,
        10.0f, 100.0f,
        0.30f,
        0xC92A6B15u);

    // Compute writes both the compacted SSBO and the indirect instance count.
    // One barrier makes both visible to the subsequent vertex fetch and draw.
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

    glUseProgram(m_rainProgram);
    glUniformMatrix4fv(m_uniformView, 1, GL_FALSE, view.m);
    glUniformMatrix4fv(m_uniformProjection, 1, GL_FALSE, projection.m);
    glUniform1f(m_uniformRainStrength, rainStrength);
    if (m_uniformViewportSize >= 0)
        glUniform2f(
            m_uniformViewportSize,
            static_cast<float>(std::max(viewport[2], 1)),
            static_cast<float>(std::max(viewport[3], 1)));
    glUniform1i(
        m_uniformHasOpticalTextures,
        m_rainOpticalTexturesReady ? GL_TRUE : GL_FALSE);
    glUniform1i(
        m_uniformHasEnvironmentMap,
        environmentMap.valid() ? GL_TRUE : GL_FALSE);
    glUniform1f(
        m_uniformEnvironmentMaxLod,
        environmentMap.valid() ? environmentMap.maximumLod() : 0.0f);
    if (m_uniformViewToWorld >= 0)
        glUniformMatrix3fv(m_uniformViewToWorld, 1, GL_FALSE, viewToWorld);

    if (m_rainOpticalTexturesReady)
    {
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, m_rainOpacityTexture);
    }
    if (environmentMap.valid())
    {
        glActiveTexture(GL_TEXTURE11);
        glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap.textureId());
    }
    glActiveTexture(static_cast<GLenum>(oldActiveTexture));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_rainComputeSsbo);

    // Only compacted, in-frustum representatives reach the graphics pipeline.
    // The draw count lives entirely on the GPU; no sync-inducing readback.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_rainIndirectBuffer);
    glDrawArraysIndirect(GL_TRIANGLES, nullptr);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    ++m_frameStats.rainDrawCalls;
    m_frameStats.rainComputeInstances += totalGpuCandidates;
    // Exact compacted visibility intentionally stays GPU-side to avoid a stall.

    // WEATHER07B7 intentionally has no per-drop CPU rain fallback. Heritage
    // now targets OpenGL 4.6-class hardware for high-density precipitation.

    glBindVertexArray(0);
    glUseProgram(0);
    glActiveTexture(static_cast<GLenum>(oldActiveTexture));

    glDepthMask(oldDepthMask);
    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (!blendWasEnabled)
        glDisable(GL_BLEND);
    if (cullWasEnabled)
        glEnable(GL_CULL_FACE);

    m_frameStats.rainCpuMs += std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
}

} // namespace heritage::graphics

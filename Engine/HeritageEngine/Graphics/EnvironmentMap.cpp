#include "EnvironmentMap.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace heritage::graphics {
namespace {

struct Float3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

Float3 fromVec3(const heritage::math::Vec3& value)
{
    return { value.x, value.y, value.z };
}

Float3 add(const Float3& a, const Float3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

Float3 multiply(const Float3& value, float scalar)
{
    return { value.x * scalar, value.y * scalar, value.z * scalar };
}

Float3 mix(const Float3& a, const Float3& b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return add(multiply(a, 1.0f - t), multiply(b, t));
}

float dot(const Float3& a, const Float3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Float3 normalize(const Float3& value)
{
    const float lengthSquared = dot(value, value);
    if (lengthSquared <= 1.0e-12f)
        return { 0.0f, 1.0f, 0.0f };
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    return multiply(value, inverseLength);
}

Float3 cubeDirection(int face, float u, float v)
{
    switch (face)
    {
    case 0: return normalize({ 1.0f, -v, -u });
    case 1: return normalize({ -1.0f, -v, u });
    case 2: return normalize({ u, 1.0f, v });
    case 3: return normalize({ u, -1.0f, -v });
    case 4: return normalize({ u, -v, 1.0f });
    default:return normalize({ -u, -v, -1.0f });
    }
}

float hashDirection(const Float3& direction)
{
    // Stable procedural star field. It is intentionally cheap because this is
    // generated on the CPU only when the environment cubemap refreshes.
    const float value = std::sin(
        direction.x * 127.1f
        + direction.y * 311.7f
        + direction.z * 74.7f) * 43758.5453f;
    return value - std::floor(value);
}

Float3 proceduralEnvironment(
    const Float3& direction,
    const EnvironmentLighting& lighting)
{
    Float3 color;
    if (direction.y >= 0.0f)
    {
        const float skyT = std::pow(
            std::clamp(direction.y, 0.0f, 1.0f), 0.55f);
        color = mix(
            fromVec3(lighting.skyHorizon),
            fromVec3(lighting.skyZenith),
            skyT);
    }
    else
    {
        const float groundT = std::pow(
            std::clamp(-direction.y, 0.0f, 1.0f), 0.72f);
        color = mix(
            fromVec3(lighting.groundHorizon),
            fromVec3(lighting.groundNadir),
            groundT);
    }

    const float horizon = std::exp(-std::abs(direction.y) * 11.0f);
    const Float3 horizonGlow = mix(
        { 0.025f, 0.020f, 0.018f },
        { 0.13f, 0.11f, 0.085f },
        lighting.daylightFactor);
    color = add(color, multiply(horizonGlow, horizon));

    const Float3 sunDirection = fromVec3(lighting.sunDirection);
    const float sunDot = std::max(dot(direction, sunDirection), 0.0f);
    if (lighting.sunIntensity > 0.001f)
    {
        const float sunCore = std::pow(sunDot, 1400.0f)
            * (5.5f * lighting.sunIntensity);
        const float sunGlow = std::pow(sunDot, 40.0f)
            * (0.20f * lighting.sunIntensity);
        color = add(
            color,
            multiply(fromVec3(lighting.sunColor), sunCore + sunGlow));
    }

    if (lighting.starIntensity > 0.001f && direction.y > 0.0f)
    {
        const float random = hashDirection(direction);
        if (random > 0.9975f)
        {
            const float star =
                std::pow((random - 0.9975f) / 0.0025f, 5.0f)
                * lighting.starIntensity * 3.0f;
            color = add(color, { star, star, star * 1.08f });
        }
    }

    return color;
}

float wrappedHourDistance(float a, float b)
{
    float distance = std::abs(a - b);
    return std::min(distance, 24.0f - distance);
}

float environmentLightingDifference(
    const EnvironmentLighting& a,
    const EnvironmentLighting& b)
{
    auto difference3 = [](
        const heritage::math::Vec3& left,
        const heritage::math::Vec3& right) {
        return std::max({
            std::abs(left.x - right.x),
            std::abs(left.y - right.y),
            std::abs(left.z - right.z) });
    };
    return std::max({
        difference3(a.sunColor, b.sunColor),
        std::abs(a.sunIntensity - b.sunIntensity),
        difference3(a.skyHorizon, b.skyHorizon),
        difference3(a.skyZenith, b.skyZenith),
        difference3(a.groundHorizon, b.groundHorizon),
        difference3(a.groundNadir, b.groundNadir),
        std::abs(a.starIntensity - b.starIntensity),
        std::abs(a.daylightFactor - b.daylightFactor) });
}

} // namespace

bool EnvironmentMap::allocateCube(GLuint& textureId)
{
    constexpr int kFaceSize = 128;
    constexpr int kFaceCount = 6;

    glGenTextures(1, &textureId);
    if (!textureId)
    {
        m_lastError = "Could not allocate the procedural environment cubemap.";
        return false;
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, textureId);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    for (int face = 0; face < kFaceCount; ++face)
    {
        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
            0,
            GL_RGB16F,
            kFaceSize,
            kFaceSize,
            0,
            GL_RGB,
            GL_FLOAT,
            nullptr);
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    return true;
}

bool EnvironmentMap::initializeProcedural(const EnvironmentLighting& lighting)
{
    shutdown();

    if (!allocateCube(m_textureId) || !allocateCube(m_stagingTextureId))
    {
        shutdown();
        return false;
    }

    return uploadProcedural(lighting);
}

bool EnvironmentMap::updateProcedural(
    const EnvironmentLighting& lighting,
    bool force)
{
    if (!m_textureId || !m_stagingTextureId)
        return initializeProcedural(lighting);

    const auto now = std::chrono::steady_clock::now();

    if (force)
    {
        m_refreshInProgress = false;
        m_nextRefreshFace = 0;
        return uploadProcedural(lighting);
    }

    // Start a new staged refresh only when the environment changed enough and
    // at most five times per second. Once started, one cubemap face is filled
    // per rendered frame. The renderer keeps sampling the old complete map
    // until all six new faces and mip levels are ready, so there are no seams.
    constexpr float kRefreshThresholdHours = 0.01f;
    constexpr auto kMinimumWallInterval = std::chrono::milliseconds(200);
    if (!m_refreshInProgress)
    {
        const bool timeChanged = wrappedHourDistance(
            lighting.timeOfDayHours,
            m_lastGeneratedTimeHours) >= kRefreshThresholdHours;
        const bool lightingChanged = environmentLightingDifference(
            lighting, m_pendingLighting) >= 0.015f;
        if (!timeChanged && !lightingChanged)
            return true;
        if (m_hasUploadWallTime
            && now - m_lastUploadWallTime < kMinimumWallInterval)
        {
            return true;
        }

        m_pendingLighting = lighting;
        m_refreshInProgress = true;
        m_nextRefreshFace = 0;
        m_hasFaceStepWallTime = false;
    }

    // EntityMeshRenderer may draw more than one camera in the same rendered
    // frame (triple-monitor). Prevent those views from consuming multiple
    // cubemap faces in a single frame-sized burst.
    constexpr auto kMinimumFaceStepInterval = std::chrono::milliseconds(2);
    if (m_hasFaceStepWallTime
        && now - m_lastFaceStepWallTime < kMinimumFaceStepInterval)
    {
        return true;
    }

    if (!uploadProceduralFace(
            m_stagingTextureId,
            m_nextRefreshFace,
            m_pendingLighting))
    {
        m_refreshInProgress = false;
        m_nextRefreshFace = 0;
        return false;
    }

    m_lastFaceStepWallTime = now;
    m_hasFaceStepWallTime = true;
    ++m_nextRefreshFace;

    if (m_nextRefreshFace >= 6)
        return finishStagedRefresh();

    return true;
}

bool EnvironmentMap::uploadProceduralFace(
    GLuint textureId,
    int face,
    const EnvironmentLighting& lighting)
{
    constexpr int kFaceSize = 128;
    if (!textureId || face < 0 || face >= 6)
    {
        m_lastError = "Invalid procedural environment cubemap face.";
        return false;
    }

    std::vector<float> pixels(
        static_cast<std::size_t>(kFaceSize)
        * static_cast<std::size_t>(kFaceSize)
        * 3u);

    for (int y = 0; y < kFaceSize; ++y)
    {
        for (int x = 0; x < kFaceSize; ++x)
        {
            const float u =
                ((static_cast<float>(x) + 0.5f) / static_cast<float>(kFaceSize)) * 2.0f - 1.0f;
            const float v =
                ((static_cast<float>(y) + 0.5f) / static_cast<float>(kFaceSize)) * 2.0f - 1.0f;
            const Float3 color = proceduralEnvironment(
                cubeDirection(face, u, v),
                lighting);
            const std::size_t offset =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(kFaceSize)
                    + static_cast<std::size_t>(x)) * 3u;
            pixels[offset + 0] = color.x;
            pixels[offset + 1] = color.y;
            pixels[offset + 2] = color.z;
        }
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, textureId);
    glTexSubImage2D(
        GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
        0,
        0,
        0,
        kFaceSize,
        kFaceSize,
        GL_RGB,
        GL_FLOAT,
        pixels.data());
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    return true;
}

bool EnvironmentMap::finishStagedRefresh()
{
    if (!m_stagingTextureId)
        return false;

    constexpr int kFaceSize = 128;
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_stagingTextureId);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    std::swap(m_textureId, m_stagingTextureId);
    m_maximumLod = std::floor(std::log2(static_cast<float>(kFaceSize)));
    m_lastGeneratedTimeHours = m_pendingLighting.timeOfDayHours;
    m_lastUploadWallTime = std::chrono::steady_clock::now();
    m_hasUploadWallTime = true;
    m_refreshInProgress = false;
    m_nextRefreshFace = 0;
    ++m_generationSerial;
    if (m_generationSerial == 0)
        m_generationSerial = 1;
    m_lastError.clear();
    return true;
}

bool EnvironmentMap::uploadProcedural(const EnvironmentLighting& lighting)
{
    if (!m_textureId)
    {
        m_lastError = "Procedural environment cubemap is not allocated.";
        return false;
    }

    constexpr int kFaceCount = 6;
    constexpr int kFaceSize = 128;
    for (int face = 0; face < kFaceCount; ++face)
    {
        if (!uploadProceduralFace(m_textureId, face, lighting))
            return false;
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureId);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    m_maximumLod = std::floor(std::log2(static_cast<float>(kFaceSize)));
    m_lastGeneratedTimeHours = lighting.timeOfDayHours;
    m_lastUploadWallTime = std::chrono::steady_clock::now();
    m_hasUploadWallTime = true;
    m_refreshInProgress = false;
    m_nextRefreshFace = 0;
    m_pendingLighting = lighting;
    ++m_generationSerial;
    if (m_generationSerial == 0)
        m_generationSerial = 1;
    m_lastError.clear();
    return true;
}

void EnvironmentMap::shutdown()
{
    if (m_textureId)
    {
        glDeleteTextures(1, &m_textureId);
        m_textureId = 0;
    }
    if (m_stagingTextureId)
    {
        glDeleteTextures(1, &m_stagingTextureId);
        m_stagingTextureId = 0;
    }
    m_maximumLod = 0.0f;
    m_lastGeneratedTimeHours = -1000.0f;
    m_lastUploadWallTime = {};
    m_lastFaceStepWallTime = {};
    m_hasUploadWallTime = false;
    m_hasFaceStepWallTime = false;
    m_refreshInProgress = false;
    m_nextRefreshFace = 0;
    m_pendingLighting = {};
    m_generationSerial = 0;
    m_lastError.clear();
}

} // namespace heritage::graphics

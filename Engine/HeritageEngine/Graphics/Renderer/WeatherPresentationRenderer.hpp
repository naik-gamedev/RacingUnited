#pragma once

#include <cstdint>
#include <filesystem>

#include <glad/glad.h>

#include "../Texture2D.hpp"
#include "../../Camera/ChaseCamera.hpp"
#include "../../Core/Math/Math.hpp"

namespace heritage::physics { class SurfaceWorld; }
namespace heritage::graphics { class EnvironmentMap; }

namespace heritage::graphics {

struct WeatherPresentationRendererStats
{
    std::uint64_t rainDrawCalls = 0;
    std::uint64_t rainComputeInstances = 0;
    std::uint64_t rainComputeDispatches = 0;
    double rainCpuMs = 0.0;
    double precipitationRateMmPerHour = 0.0;
    double physicalMeanDiameterMm = 0.0;
    double physicalFluxFallSpeedMps = 0.0;
    double physicalWindDirectionDegrees = 0.0;
    bool rendererReady = false;
    bool suppressedByCover = false;
    bool opticalTexturesReady = false;
    int opticalTextureWidth = 0;
    int opticalTextureHeight = 0;
};

// WEATHER07C1 consumes WEATHER07A's deterministic physical precipitation field
// and renders GPU-compacted representatives as velocity-aligned single-triangle
// optical streaks. A module may provide Weather/Rain/RainDrop_BC.png containing
// base colour + alpha only; reflection comes from the live environment map and
// a tiny procedural bulge. Drop size, trajectory, terminal velocity and wind
// response remain physical and independent of the art texture.
class WeatherPresentationRenderer
{
public:
    bool initialize(const std::filesystem::path& moduleAssetRoot);
    void shutdown();

    void beginFrameStats() { m_frameStats = {}; }
    const WeatherPresentationRendererStats& frameStats() const
    {
        return m_frameStats;
    }

    void draw(
        const heritage::physics::SurfaceWorld& surfaces,
        const heritage::math::Mat4& projection,
        const heritage::camera::CameraFrame& cameraFrame,
        float elapsedSeconds,
        const heritage::graphics::EnvironmentMap& environmentMap,
        GLsizei viewportWidth,
        GLsizei viewportHeight);

private:
    bool loadRainOpticalMaterial(const std::filesystem::path& moduleAssetRoot);

    GLuint m_rainProgram = 0;
    GLuint m_rainComputeProgram = 0;
    GLuint m_rainVao = 0;
    GLuint m_rainCornerVbo = 0;
    GLuint m_rainComputeSsbo = 0;
    GLuint m_rainIndirectBuffer = 0;

    GLint m_uniformView = -1;
    GLint m_uniformProjection = -1;
    GLint m_uniformRainStrength = -1;
    GLint m_uniformViewportSize = -1;
    GLint m_uniformRainOpacityTexture = -1;
    GLint m_uniformHasOpticalTextures = -1;
    GLint m_uniformEnvironmentMap = -1;
    GLint m_uniformHasEnvironmentMap = -1;
    GLint m_uniformEnvironmentMaxLod = -1;
    GLint m_uniformViewToWorld = -1;

    GLint m_computeUniformInstanceCount = -1;
    GLint m_computeUniformBaseCell = -1;
    GLint m_computeUniformCameraRemainder = -1;
    GLint m_computeUniformGridSize = -1;
    GLint m_computeUniformCellSize = -1;
    GLint m_computeUniformVerticalOffset = -1;
    GLint m_computeUniformRainStrength = -1;
    GLint m_computeUniformWindVelocityXZ = -1;
    GLint m_computeUniformTime = -1;
    GLint m_computeUniformDropLambda = -1;
    GLint m_computeUniformMinimumDiameter = -1;
    GLint m_computeUniformMaximumDiameter = -1;
    GLint m_computeUniformTierAlpha = -1;
    GLint m_computeUniformTierSalt = -1;
    GLint m_computeUniformView = -1;
    GLint m_computeUniformProjection = -1;
    GLint m_computeUniformMinimumRadius = -1;
    GLint m_computeUniformMaximumRadius = -1;

    Texture2DCache m_rainTextureCache;
    GLuint m_rainOpacityTexture = 0;
    int m_rainTextureWidth = 0;
    int m_rainTextureHeight = 0;
    bool m_rainOpticalTexturesReady = false;

    WeatherPresentationRendererStats m_frameStats{};
};

} // namespace heritage::graphics

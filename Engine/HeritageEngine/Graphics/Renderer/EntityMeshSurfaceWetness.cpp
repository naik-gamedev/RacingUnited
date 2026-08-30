#include "EntityMeshRenderer.hpp"

#include "../../Physics/Surfaces/SurfaceWorld.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

namespace heritage::graphics {

bool EntityMeshRenderer::initializeSurfaceWetnessMaterialBindings()
{
    if (!m_program)
        return false;

    const auto uniform = [this](const char* name) {
        return glGetUniformLocation(m_program, name);
    };
    m_uniforms.surfaceWetnessReceiver = uniform("uSurfaceWetnessReceiver");
    m_uniforms.gpuDynamicSurfaceAuthorityActive =
        uniform("uGpuDynamicSurfaceAuthorityActive");
    m_uniforms.gpuWaterAtlas = uniform("uGpuWaterAtlas");
    m_uniforms.gpuFarWaterAtlas = uniform("uGpuFarWaterAtlas");
    m_uniforms.gpuFarTileTags = uniform("uGpuFarTileTags");
    m_uniforms.gpuSnowAtlas = uniform("uGpuSnowAtlas");
    m_uniforms.gpuMudAtlas = uniform("uGpuMudAtlas");
    m_uniforms.gpuTireMarkAtlas = uniform("uGpuTireMarkAtlas");
    m_uniforms.gpuTileIndirection = uniform("uGpuTileIndirection");
    m_uniforms.gpuDynamicSurfaceCenterOriginRelativeXZ =
        uniform("uGpuDynamicSurfaceCenterOriginRelativeXZ");
    m_uniforms.gpuDynamicSurfaceCenterWorldTile =
        uniform("uGpuDynamicSurfaceCenterWorldTile");
    m_uniforms.gpuDynamicSurfaceTileMapCenter =
        uniform("uGpuDynamicSurfaceTileMapCenter");
    m_uniforms.gpuDynamicSurfaceTileResolution =
        uniform("uGpuDynamicSurfaceTileResolution");
    m_uniforms.gpuDynamicSurfaceAtlasColumns =
        uniform("uGpuDynamicSurfaceAtlasColumns");
    m_uniforms.gpuFarTileResolution = uniform("uGpuFarTileResolution");
    m_uniforms.gpuFarAtlasTilesPerAxis = uniform("uGpuFarAtlasTilesPerAxis");
    m_uniforms.gpuDynamicSurfaceSnowReady = uniform("uGpuDynamicSurfaceSnowReady");
    m_uniforms.gpuDynamicSurfaceMudReady = uniform("uGpuDynamicSurfaceMudReady");
    m_uniforms.gpuDynamicSurfaceTireMarksReady = uniform("uGpuDynamicSurfaceTireMarksReady");
    m_uniforms.surfaceWetnessBreakupMask = uniform("uSurfaceWetnessBreakupMask");
    m_uniforms.hasSurfaceWetnessBreakupMask =
        uniform("uHasSurfaceWetnessBreakupMask");
    m_uniforms.surfacePatternCameraModuloXZ =
        uniform("uSurfacePatternCameraModuloXZ");
    m_uniforms.surfacePresentationTime = uniform("uSurfacePresentationTime");
    m_uniforms.surfaceWeatherFilmWetness = uniform("uSurfaceWeatherFilmWetness");
    m_uniforms.prebakedWaterExposureM = uniform("uPrebakedWaterExposureM");
    m_uniforms.rainWettingExposureM = uniform("uRainWettingExposureM");
    m_uniforms.rainRateMmPerHour = uniform("uRainRateMmPerHour");

    glUniform1i(m_uniforms.surfaceWetnessBreakupMask, 14);
    glUniform1i(m_uniforms.gpuWaterAtlas, 16);
    glUniform1i(m_uniforms.gpuSnowAtlas, 17);
    glUniform1i(m_uniforms.gpuMudAtlas, 18);
    glUniform1i(m_uniforms.gpuTileIndirection, 19);
    glUniform1i(m_uniforms.gpuFarWaterAtlas, 20);
    glUniform1i(m_uniforms.gpuFarTileTags, 21);
    glUniform1i(m_uniforms.gpuTireMarkAtlas, 22);
    return initializeSurfaceWetnessResources();
}

void EntityMeshRenderer::bindSurfaceWetnessMaterialState(
    const heritage::math::DVec3& cameraGlobal,
    float elapsedSeconds)
{
    constexpr double kSurfacePatternModuloM = 4096.0;
    glUniform2f(
        m_uniforms.surfacePatternCameraModuloXZ,
        static_cast<float>(std::fmod(cameraGlobal.x, kSurfacePatternModuloM)),
        static_cast<float>(std::fmod(cameraGlobal.z, kSurfacePatternModuloM)));
    glUniform1f(m_uniforms.surfacePresentationTime, elapsedSeconds);
    glUniform1f(m_uniforms.surfaceWeatherFilmWetness, m_surfaceWeatherFilmWetness);
    glUniform1f(m_uniforms.prebakedWaterExposureM,
        m_dynamicSurfaceGpuRuntime.stats().backgroundSeedDepthM);
    glUniform1f(m_uniforms.rainWettingExposureM,
        m_dynamicSurfaceGpuRuntime.stats().surfaceWettingExposureM);
    glUniform1f(m_uniforms.rainRateMmPerHour,
        m_dynamicSurfaceGpuRuntime.stats().runoffDriverMmPerHour);
    glUniform1i(
        m_uniforms.hasSurfaceWetnessBreakupMask,
        m_surfaceWetnessBreakupTexture != 0 ? 1 : 0);

    glActiveTexture(GL_TEXTURE0 + 14);
    glBindTexture(GL_TEXTURE_2D, m_surfaceWetnessBreakupTexture);

    const auto& gpuStats = m_dynamicSurfaceGpuRuntime.stats();
    const bool gpuAuthorityActive = m_dynamicSurfaceGpuRuntime.authoritativeReady();
    glUniform1i(
        m_uniforms.gpuDynamicSurfaceAuthorityActive,
        gpuAuthorityActive ? 1 : 0);
    glUniform1i(
        m_uniforms.gpuDynamicSurfaceSnowReady,
        gpuStats.snowReady ? 1 : 0);
    glUniform1i(
        m_uniforms.gpuDynamicSurfaceMudReady,
        gpuStats.mudReady ? 1 : 0);
    glUniform1i(
        m_uniforms.gpuDynamicSurfaceTireMarksReady,
        gpuStats.tireMarksReady ? 1 : 0);

    const double centerOriginX =
        static_cast<double>(m_dynamicSurfaceGpuRuntime.centerTileX())
        * static_cast<double>(heritage::graphics::dynamicsurface::DynamicSurfaceGpuRuntime::tileWorldSizeM());
    const double centerOriginZ =
        static_cast<double>(m_dynamicSurfaceGpuRuntime.centerTileZ())
        * static_cast<double>(heritage::graphics::dynamicsurface::DynamicSurfaceGpuRuntime::tileWorldSizeM());
    glUniform2f(
        m_uniforms.gpuDynamicSurfaceCenterOriginRelativeXZ,
        static_cast<float>(centerOriginX - cameraGlobal.x),
        static_cast<float>(centerOriginZ - cameraGlobal.z));
    glUniform2i(
        m_uniforms.gpuDynamicSurfaceCenterWorldTile,
        m_dynamicSurfaceGpuRuntime.centerTileX(),
        m_dynamicSurfaceGpuRuntime.centerTileZ());
    glUniform2i(
        m_uniforms.gpuDynamicSurfaceTileMapCenter,
        heritage::graphics::dynamicsurface::DynamicSurfaceGpuRuntime::tileMapHalfSpan(),
        heritage::graphics::dynamicsurface::DynamicSurfaceGpuRuntime::tileMapHalfSpan());
    glUniform1i(
        m_uniforms.gpuDynamicSurfaceTileResolution,
        static_cast<GLint>(heritage::graphics::dynamicsurface::DynamicSurfaceGpuRuntime::tileResolution()));
    glUniform1i(
        m_uniforms.gpuDynamicSurfaceAtlasColumns,
        static_cast<GLint>(heritage::graphics::dynamicsurface::DynamicSurfaceGpuRuntime::atlasColumns()));
    glUniform1i(
        m_uniforms.gpuFarTileResolution,
        static_cast<GLint>(heritage::graphics::dynamicsurface::DynamicSurfaceGpuRuntime::farTileResolution()));
    glUniform1i(
        m_uniforms.gpuFarAtlasTilesPerAxis,
        static_cast<GLint>(heritage::graphics::dynamicsurface::DynamicSurfaceGpuRuntime::farAtlasTilesPerAxis()));

    glActiveTexture(GL_TEXTURE0 + 16);
    glBindTexture(GL_TEXTURE_2D, m_dynamicSurfaceGpuRuntime.waterTexture());
    glActiveTexture(GL_TEXTURE0 + 17);
    glBindTexture(GL_TEXTURE_2D, m_dynamicSurfaceGpuRuntime.snowTexture());
    glActiveTexture(GL_TEXTURE0 + 18);
    glBindTexture(GL_TEXTURE_2D, m_dynamicSurfaceGpuRuntime.mudTexture());
    glActiveTexture(GL_TEXTURE0 + 19);
    glBindTexture(GL_TEXTURE_2D, m_dynamicSurfaceGpuRuntime.tileIndirectionTexture());
    glActiveTexture(GL_TEXTURE0 + 20);
    glBindTexture(GL_TEXTURE_2D, m_dynamicSurfaceGpuRuntime.farWaterTexture());
    glActiveTexture(GL_TEXTURE0 + 21);
    glBindTexture(GL_TEXTURE_2D, m_dynamicSurfaceGpuRuntime.farTileTagTexture());
    glActiveTexture(GL_TEXTURE0 + 22);
    glBindTexture(GL_TEXTURE_2D, m_dynamicSurfaceGpuRuntime.tireMarkTexture());
}

bool EntityMeshRenderer::initializeSurfaceWetnessResources()
{
    std::string breakupError;
    const Texture2D* breakup = m_textureCache.acquire(
        m_assetRoot / "Surface" / "Water" / "Water_ShorelineBreakup_A8.png",
        TextureColorSpace::Linear,
        1,
        false,
        breakupError);
    if (breakup)
    {
        m_surfaceWetnessBreakupTexture = breakup->id;
        GLint previousTexture = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
        glBindTexture(GL_TEXTURE_2D, m_surfaceWetnessBreakupTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    }
    else
    {
        std::cerr << "Heritage Dynamic Surface shoreline texture unavailable: "
                  << breakupError << '\n';
    }

    std::cout
        << "LIVETRACK22 unified driven surface: water + 10m/256x256 tire-mark material tiles share one near residency/indirection domain; "
        << "far tire-mark vector history remains only as a 100-500m persistence LOD.\n";
    return true;
}

void EntityMeshRenderer::shutdownSurfaceWetnessResources()
{
    m_surfaceWetnessBreakupTexture = 0;
}

} // namespace heritage::graphics

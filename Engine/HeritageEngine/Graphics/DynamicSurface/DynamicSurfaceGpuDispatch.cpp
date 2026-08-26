#include "DynamicSurfaceGpuRuntime.hpp"

#include "../../Physics/Surfaces/DynamicSurface/DynamicSurfaceSystem.hpp"
#include "../../Physics/Surfaces/Water/SurfaceHydrology.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace heritage::graphics::dynamicsurface {

void DynamicSurfaceGpuRuntime::dispatchTileState(
    StateRuntime& state,
    GLuint program,
    const ProgramUniforms& uniforms,
    const TileRuntime& tile,
    float cycleDtSeconds,
    float precipitationRateMmPerHour,
    float weatherDrainageRateMmPerHour,
    float evaporationRateMmPerHour,
    float ambientTemperatureC)
{
    if (!state.allocated || !program)
        return;
    const auto elapsedMs = [](const auto& started) {
        return std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
    };
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, state.atlas);
    glActiveTexture(GL_TEXTURE0 + 6);
    glBindTexture(GL_TEXTURE_2D, m_tileIndirectionTexture);
    glBindImageTexture(1, state.scratch, 0, GL_FALSE, 0, GL_WRITE_ONLY, state.internalFormat);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 20, m_geometryTriangleBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 21, m_geometryBinHeaderBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 22, m_geometryBinIndexBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 23, m_geometryTileMetaBuffer);
    const auto origin = atlasSlotOrigin(tile.slot);
    if (uniforms.worldTile >= 0) glUniform2i(uniforms.worldTile, tile.x, tile.z);
    if (uniforms.atlasOrigin >= 0) glUniform2i(uniforms.atlasOrigin,
        static_cast<GLint>(origin[0]), static_cast<GLint>(origin[1]));
    if (uniforms.tileMapOrigin >= 0) glUniform2i(uniforms.tileMapOrigin,
        m_tileMapOriginX, m_tileMapOriginZ);
    if (uniforms.geometryCenterChunk >= 0) glUniform2i(uniforms.geometryCenterChunk,
        m_geometryCenterChunkX, m_geometryCenterChunkZ);
    if (uniforms.cellSizeM >= 0) glUniform1f(uniforms.cellSizeM, kCellSizeM);
    if (uniforms.cycleDtSeconds >= 0) glUniform1f(uniforms.cycleDtSeconds, cycleDtSeconds);
    if (uniforms.precipitationRateMmPerHour >= 0)
        glUniform1f(uniforms.precipitationRateMmPerHour, precipitationRateMmPerHour);
    if (uniforms.weatherDrainageRateMmPerHour >= 0)
        glUniform1f(uniforms.weatherDrainageRateMmPerHour, weatherDrainageRateMmPerHour);
    if (uniforms.evaporationRateMmPerHour >= 0)
        glUniform1f(uniforms.evaporationRateMmPerHour, evaporationRateMmPerHour);
    if (uniforms.ambientTemperatureC >= 0)
        glUniform1f(uniforms.ambientTemperatureC, ambientTemperatureC);
    if (uniforms.tickIndex >= 0)
        glUniform1ui(uniforms.tickIndex, static_cast<GLuint>(
            std::max(0.0, std::floor(m_lastElapsedSeconds * 12.0))));
    if (uniforms.stateAtlas >= 0) glUniform1i(uniforms.stateAtlas, 0);

    const auto dispatchStarted = std::chrono::steady_clock::now();
    glDispatchCompute(kTileResolution / 16u, kTileResolution / 16u, 1u);
    m_stats.optionalTileDispatchGlMs += elapsedMs(dispatchStarted);

    const auto firstBarrierStarted = std::chrono::steady_clock::now();
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    m_stats.optionalTileBarrierGlMs += elapsedMs(firstBarrierStarted);

    const auto copyStarted = std::chrono::steady_clock::now();
    glCopyImageSubData(state.scratch, GL_TEXTURE_2D, 0, 0, 0, 0,
        state.atlas, GL_TEXTURE_2D, 0,
        static_cast<GLint>(origin[0]), static_cast<GLint>(origin[1]), 0,
        static_cast<GLsizei>(kTileResolution), static_cast<GLsizei>(kTileResolution), 1);
    m_stats.optionalTileCopyGlMs += elapsedMs(copyStarted);

    const auto secondBarrierStarted = std::chrono::steady_clock::now();
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    m_stats.optionalTileBarrierGlMs += elapsedMs(secondBarrierStarted);

    ++m_stats.dispatchesThisFrame;
    m_stats.cellsThisFrame += static_cast<std::uint64_t>(kTileResolution) * kTileResolution;
}
void DynamicSurfaceGpuRuntime::dispatchDueTiles(
    double elapsedSeconds,
    float precipitationRateMmPerHour,
    float weatherDrainageRateMmPerHour,
    float evaporationRateMmPerHour,
    float ambientTemperatureC)
{
    if (!m_snow.allocated && !m_mud.allocated)
    {
        m_stats.dueTiles = 0u;
        m_stats.dispatchBacklogTiles = 0u;
        return;
    }
    std::uint32_t due = 0u;
    std::uint32_t processed = 0u;
    for (auto& [key, tile] : m_tiles)
    {
        (void)key;
        if (elapsedSeconds + 1.0e-9 < tile.nextDueSeconds)
            continue;
        ++due;
        if (processed >= kOptionalTileBudgetPerFrame)
            continue;

        const float dt = 0.5f;
        if (m_snow.allocated)
            dispatchTileState(m_snow, m_snowProgram, m_snowUniforms,
                tile, dt, precipitationRateMmPerHour, weatherDrainageRateMmPerHour,
                evaporationRateMmPerHour, ambientTemperatureC);
        if (m_mud.allocated)
            dispatchTileState(m_mud, m_mudProgram, m_mudUniforms,
                tile, dt, precipitationRateMmPerHour, weatherDrainageRateMmPerHour,
                evaporationRateMmPerHour, ambientTemperatureC);
        tile.nextDueSeconds = elapsedSeconds + 0.5;
        ++processed;
    }
    m_stats.dueTiles = due;
    m_stats.dispatchBacklogTiles = due - processed;
}

} // namespace heritage::graphics::dynamicsurface

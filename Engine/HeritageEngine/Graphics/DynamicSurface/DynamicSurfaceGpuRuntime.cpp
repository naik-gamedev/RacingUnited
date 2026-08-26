#include "DynamicSurfaceGpuShaders.hpp"
#include "DynamicSurfaceGpuRuntime.hpp"

#include "../ShaderProgram.hpp"
#include "../../Physics/Surfaces/DynamicSurface/DynamicSurfaceSystem.hpp"
#include "../../Physics/Surfaces/Water/SurfaceHydrology.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace heritage::graphics::dynamicsurface {

bool DynamicSurfaceGpuRuntime::initialize(std::string& errorMessage)
{
    shutdown();
    errorMessage.clear();

    GLint maximumTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumTextureSize);
    if (maximumTextureSize < static_cast<GLint>(kAtlasWidth)
        || maximumTextureSize < static_cast<GLint>(kAtlasHeight)
        || maximumTextureSize < static_cast<GLint>(kFarAtlasWidth)
        || maximumTextureSize < static_cast<GLint>(kFarAtlasHeight))
    {
        errorMessage = "GPU cannot satisfy LIVETRACK18 near/far prebaked-water atlases.";
        return false;
    }
    while (glGetError() != GL_NO_ERROR) {}

    // Water has no full-field compute program anymore. Only localized tire
    // clearing remains compute-driven; snow/mud stay lazy optional states.
    m_snowProgram = heritage::graphics::buildComputeShaderProgram(detail::makeSnowShader().c_str());
    m_mudProgram = heritage::graphics::buildComputeShaderProgram(detail::makeMudShader().c_str());
    m_tireEventProgram = heritage::graphics::buildComputeShaderProgram(detail::kTireEventComputeShader);
    m_tireWaterSampleProgram = heritage::graphics::buildComputeShaderProgram(detail::kTireWaterSampleComputeShader);
    if (!m_snowProgram || !m_mudProgram || !m_tireEventProgram || !m_tireWaterSampleProgram)
    {
        errorMessage = "LIVETRACK18 localized Dynamic Surface compute shader compilation/link failed.";
        shutdown();
        return false;
    }

    const auto cacheUniforms = [](GLuint program, ProgramUniforms& u) {
        u.worldTile = glGetUniformLocation(program, "uWorldTile");
        u.atlasOrigin = glGetUniformLocation(program, "uAtlasOrigin");
        u.tileMapOrigin = glGetUniformLocation(program, "uTileMapOrigin");
        u.geometryCenterChunk = glGetUniformLocation(program, "uGeometryCenterChunk");
        u.cellSizeM = glGetUniformLocation(program, "uCellSizeM");
        u.cycleDtSeconds = glGetUniformLocation(program, "uCycleDtSeconds");
        u.precipitationRateMmPerHour = glGetUniformLocation(program, "uPrecipitationRateMmPerHour");
        u.weatherDrainageRateMmPerHour = glGetUniformLocation(program, "uWeatherDrainageRateMmPerHour");
        u.evaporationRateMmPerHour = glGetUniformLocation(program, "uEvaporationRateMmPerHour");
        u.ambientTemperatureC = glGetUniformLocation(program, "uAmbientTemperatureC");
        u.tickIndex = glGetUniformLocation(program, "uTickIndex");
        u.stateAtlas = glGetUniformLocation(program, "uStateAtlas");
        u.tileMap = glGetUniformLocation(program, "uTileMap");
        u.windVelocityXZ = glGetUniformLocation(program, "uWindVelocityXZ");
    };
    cacheUniforms(m_snowProgram, m_snowUniforms);
    cacheUniforms(m_mudProgram, m_mudUniforms);

    m_tireEventUniforms.atlasOrigin = glGetUniformLocation(m_tireEventProgram, "uAtlasOrigin");
    m_tireEventUniforms.minTexel = glGetUniformLocation(m_tireEventProgram, "uMinTexel");
    m_tireEventUniforms.extentTexels = glGetUniformLocation(m_tireEventProgram, "uExtentTexels");
    m_tireEventUniforms.cellSizeM = glGetUniformLocation(m_tireEventProgram, "uCellSizeM");
    m_tireEventUniforms.eventLocalXZ = glGetUniformLocation(m_tireEventProgram, "uEventLocalXZ");
    m_tireEventUniforms.forwardXZ = glGetUniformLocation(m_tireEventProgram, "uForwardXZ");
    m_tireEventUniforms.rightXZ = glGetUniformLocation(m_tireEventProgram, "uRightXZ");
    m_tireEventUniforms.patchHalfLengthM = glGetUniformLocation(m_tireEventProgram, "uPatchHalfLengthM");
    m_tireEventUniforms.patchHalfWidthM = glGetUniformLocation(m_tireEventProgram, "uPatchHalfWidthM");
    m_tireEventUniforms.normalLoadN = glGetUniformLocation(m_tireEventProgram, "uNormalLoadN");
    m_tireEventUniforms.speedMps = glGetUniformLocation(m_tireEventProgram, "uSpeedMps");
    m_tireEventUniforms.accumulatedDtSeconds = glGetUniformLocation(m_tireEventProgram, "uAccumulatedDtSeconds");
    m_tireEventUniforms.snowReady = glGetUniformLocation(m_tireEventProgram, "uSnowReady");
    m_tireEventUniforms.mudReady = glGetUniformLocation(m_tireEventProgram, "uMudReady");
    m_tireEventUniforms.mudDeformable = glGetUniformLocation(m_tireEventProgram, "uMudDeformable");

    m_tireWaterSampleUniforms.waterAtlas = glGetUniformLocation(m_tireWaterSampleProgram, "uWaterAtlas");
    m_tireWaterSampleUniforms.tileIndirection = glGetUniformLocation(m_tireWaterSampleProgram, "uTileIndirection");
    m_tireWaterSampleUniforms.tileResolution = glGetUniformLocation(m_tireWaterSampleProgram, "uTileResolution");
    m_tireWaterSampleUniforms.atlasColumns = glGetUniformLocation(m_tireWaterSampleProgram, "uAtlasColumns");
    m_tireWaterSampleUniforms.tileMapCenter = glGetUniformLocation(m_tireWaterSampleProgram, "uTileMapCenter");
    m_tireWaterSampleUniforms.prebakedWaterExposureM = glGetUniformLocation(m_tireWaterSampleProgram, "uPrebakedWaterExposureM");
    m_tireWaterSampleUniforms.rainWettingExposureM = glGetUniformLocation(m_tireWaterSampleProgram, "uRainWettingExposureM");
    m_tireWaterSampleUniforms.runoffDriverMmPerHour = glGetUniformLocation(m_tireWaterSampleProgram, "uRunoffDriverMmPerHour");
    m_tireWaterSampleUniforms.sampleCount = glGetUniformLocation(m_tireWaterSampleProgram, "uSampleCount");

    if (!initializeTireWaterSampleBridge(errorMessage))
    {
        shutdown();
        return false;
    }

    if (!allocateState(m_water, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4u, false, errorMessage))
    {
        shutdown();
        return false;
    }

    // LIVETRACK18 far presentation cache: one rolling 32x32 topology raster per
    // 10m world tile. LIVETRACK18 adds baked runoff accumulation beside
    // capacity/flow (RGB8). The extra channel costs only ~16 MiB for the complete
    // 4096x4096 rolling atlas and avoids sacrificing either depth or direction.
    // World-tile tags prevent modulo-slot aliasing.
    glGenTextures(1, &m_farWaterAtlas);
    glBindTexture(GL_TEXTURE_2D, m_farWaterAtlas);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8,
        static_cast<GLsizei>(kFarAtlasWidth), static_cast<GLsizei>(kFarAtlasHeight));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &m_farTileTagTexture);
    glBindTexture(GL_TEXTURE_2D, m_farTileTagTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RG32I,
        static_cast<GLsizei>(kFarAtlasTilesPerAxis),
        static_cast<GLsizei>(kFarAtlasTilesPerAxis));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    m_farTileTags.assign(
        static_cast<std::size_t>(kFarAtlasTilesPerAxis) * kFarAtlasTilesPerAxis,
        std::array<std::int32_t, 2>{
            (std::numeric_limits<std::int32_t>::min)(),
            (std::numeric_limits<std::int32_t>::min)()});
    m_farAtlasCpuMirror.assign(
        static_cast<std::size_t>(kFarAtlasWidth) * kFarAtlasHeight * 3u, 0u);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
        static_cast<GLsizei>(kFarAtlasTilesPerAxis),
        static_cast<GLsizei>(kFarAtlasTilesPerAxis),
        GL_RG_INTEGER, GL_INT, m_farTileTags.data());
    m_stats.committedMiB += (
        static_cast<double>(kFarAtlasWidth) * kFarAtlasHeight * 3.0
        + static_cast<double>(kFarAtlasTilesPerAxis) * kFarAtlasTilesPerAxis * 8.0)
        / (1024.0 * 1024.0);

    glGenTextures(1, &m_tileIndirectionTexture);
    glBindTexture(GL_TEXTURE_2D, m_tileIndirectionTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R16UI,
        static_cast<GLsizei>(kTileMapResolution), static_cast<GLsizei>(kTileMapResolution));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    m_tileIndirectionScratch.assign(
        static_cast<std::size_t>(kTileMapResolution) * kTileMapResolution, 0u);

    m_freeSlots.reserve(kMaximumTileSlots);
    for (std::uint32_t slot = 0; slot < kMaximumTileSlots; ++slot)
        m_freeSlots.push_back(static_cast<std::uint16_t>(kMaximumTileSlots - 1u - slot));

    glGenQueries(static_cast<GLsizei>(m_gpuTimerStartQueries.size()), m_gpuTimerStartQueries.data());
    glGenQueries(static_cast<GLsizei>(m_gpuTimerEndQueries.size()), m_gpuTimerEndQueries.data());
    if (!m_tileIndirectionTexture || !m_farWaterAtlas || !m_farTileTagTexture
        || !detail::checkNoGlError(errorMessage, "LIVETRACK18 prebaked-water allocation"))
    {
        shutdown();
        return false;
    }

    m_stats.ready = true;
    m_stats.waterReady = true;
    m_stats.authoritative = false; // becomes true once a valid .hhyd cache is present
    m_lastElapsedSeconds = -1.0;
    return true;
}
void DynamicSurfaceGpuRuntime::shutdown()
{
    destroyExactGeometryAtlas();
    shutdownTireWaterSampleBridge();
    if (m_farTileTagTexture)
        glDeleteTextures(1, &m_farTileTagTexture);
    if (m_farWaterAtlas)
        glDeleteTextures(1, &m_farWaterAtlas);
    if (m_tileIndirectionTexture)
        glDeleteTextures(1, &m_tileIndirectionTexture);
    m_farTileTagTexture = 0;
    m_farWaterAtlas = 0;
    m_tileIndirectionTexture = 0;
    destroyState(m_mud);
    destroyState(m_snow);
    destroyState(m_water);

    if (m_gpuTimerEndQueries[0])
        glDeleteQueries(static_cast<GLsizei>(m_gpuTimerEndQueries.size()), m_gpuTimerEndQueries.data());
    if (m_gpuTimerStartQueries[0])
        glDeleteQueries(static_cast<GLsizei>(m_gpuTimerStartQueries.size()), m_gpuTimerStartQueries.data());
    m_gpuTimerStartQueries.fill(0u);
    m_gpuTimerEndQueries.fill(0u);
    m_gpuTimerPending.fill(false);

    if (m_tireWaterSampleProgram) glDeleteProgram(m_tireWaterSampleProgram);
    if (m_tireEventProgram) glDeleteProgram(m_tireEventProgram);
    if (m_mudProgram) glDeleteProgram(m_mudProgram);
    if (m_snowProgram) glDeleteProgram(m_snowProgram);
    m_snowProgram = m_mudProgram = m_tireEventProgram = m_tireWaterSampleProgram = 0;
    m_snowUniforms = {};
    m_mudUniforms = {};
    m_tireEventUniforms = {};
    m_tireWaterSampleUniforms = {};

    m_tiles.clear();
    m_freeSlots.clear();
    m_tileIndirectionScratch.clear();
    m_farTileTags.clear();
    m_farAtlasCpuMirror.clear();
    m_centerTileValid = false;
    m_geometryAtlasReady = false;
    m_lastElapsedSeconds = -1.0;
    m_lastCameraSampleSeconds = -1.0;
    m_residencyCameraValid = false;
    m_lastPresentationPollSeconds = -1.0;
    m_backgroundSeedDepthM = 0.0f;
    m_surfaceWettingExposureM = 0.0f;
    m_runoffDriverMmPerHour = 0.0f;
    m_appliedHydrologyResetSerial = 0u;
    m_stats = {};
}
void DynamicSurfaceGpuRuntime::update(
    double elapsedSeconds,
    double cameraGlobalX,
    double cameraGlobalY,
    double cameraGlobalZ,
    float precipitationRateMmPerHour,
    float weatherDrainageRateMmPerHour,
    float evaporationRateMmPerHour,
    float ambientTemperatureC,
    float windVelocityXMps,
    float windVelocityZMps,
    const heritage::physics::dynamicsurface::DynamicSurfaceSystem* dynamicSurface,
    const heritage::physics::water::SurfaceHydrology* prebakedHydrology,
    std::uint64_t hydrologyResetSerial,
    const std::vector<DynamicSurfaceGpuTireContactEvent>& tireEvents,
    const std::vector<DynamicSurfaceGpuTireWaterSampleRequest>& tireWaterSampleRequests)
{
    (void)cameraGlobalY;
    if (!m_stats.ready) return;
    updateGpuTimerResult();
    const auto cpuStarted = std::chrono::steady_clock::now();
    auto bookkeepingStarted = cpuStarted;
    const auto elapsedMs = [](const auto& started) {
        return std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
    };
    const double deltaSeconds = m_lastElapsedSeconds >= 0.0
        ? std::clamp(elapsedSeconds - m_lastElapsedSeconds, 0.0, 0.25) : 0.0;
    m_lastElapsedSeconds = elapsedSeconds;

    m_stats.dispatchesThisFrame = 0u;
    m_stats.topologyUploadsThisFrame = 0u;
    m_stats.farTopologyUploadsThisFrame = 0u;
    m_stats.cellsThisFrame = 0u;
    m_stats.tireEventDispatches = 0u;
    m_stats.tireEventCells = 0u;
    resetFrameProfilingStats();
    m_stats.geometryValidTiles.fill(0u);
    m_stats.precipitationRateMmPerHour = std::max(precipitationRateMmPerHour, 0.0f);
    m_stats.drainageRateMmPerHour = std::max(weatherDrainageRateMmPerHour, 0.0f);
    m_stats.evaporationRateMmPerHour = std::max(evaporationRateMmPerHour, 0.0f);
    m_windVelocityX = windVelocityXMps;
    m_windVelocityZ = windVelocityZMps;

    if (m_lastCameraSampleSeconds >= 0.0 && elapsedSeconds > m_lastCameraSampleSeconds)
    {
        const double dt = std::clamp(elapsedSeconds - m_lastCameraSampleSeconds, 1.0e-4, 0.25);
        const double instantVx = (cameraGlobalX - m_lastCameraGlobalX) / dt;
        const double instantVz = (cameraGlobalZ - m_lastCameraGlobalZ) / dt;
        const double alpha = 1.0 - std::exp(-dt * 6.0);
        m_cameraVelocityX += (instantVx - m_cameraVelocityX) * alpha;
        m_cameraVelocityZ += (instantVz - m_cameraVelocityZ) * alpha;
    }
    m_lastCameraGlobalX = cameraGlobalX;
    m_lastCameraGlobalZ = cameraGlobalZ;
    m_lastCameraSampleSeconds = elapsedSeconds;

    bool forceResidencyRefresh = false;
    if (m_appliedHydrologyResetSerial != hydrologyResetSerial)
    {
        m_appliedHydrologyResetSerial = hydrologyResetSerial;
        m_backgroundSeedDepthM = 0.0f;
        m_surfaceWettingExposureM = 0.0f;
        m_runoffDriverMmPerHour = 0.0f;
        for (const auto& [key, tile] : m_tiles)
        {
            (void)key;
            releaseTileSlot(tile.slot);
        }
        m_tiles.clear();
        m_residencyCameraValid = false;
        forceResidencyRefresh = true;
        m_completedTireWaterSamples.clear();
    }

    // Poll only after applying a water-reset serial change, so a result from
    // the pre-reset reservoir can never repopulate SurfaceWorld's sample cache.
    m_stats.cpuBookkeepingMs += elapsedMs(bookkeepingStarted);
    const auto readbackPollStarted = std::chrono::steady_clock::now();
    pollTireWaterSampleReadbacks();
    m_stats.cpuTireReadbackPollMs = elapsedMs(readbackPollStarted);
    bookkeepingStarted = std::chrono::steady_clock::now();

    if (deltaSeconds > 0.0)
    {
        const float dt = static_cast<float>(deltaSeconds);
        const float rainRate = std::max(precipitationRateMmPerHour, 0.0f);
        const float drainageRate = std::max(weatherDrainageRateMmPerHour, 0.0f);
        const float evaporationRate = std::max(evaporationRateMmPerHour, 0.0f);
        const float rainM = rainRate * (0.001f / 3600.0f) * dt;

        // LIVETRACK21 moving-water memory. Catchment runoff should not vanish
        // on the exact frame rainfall stops, but neither should a wet material
        // film keep a gutter flowing for hours. Rain attacks immediately; the
        // effective runoff driver then decays with a 45 s time constant.
        if (rainRate >= m_runoffDriverMmPerHour)
        {
            m_runoffDriverMmPerHour = rainRate;
        }
        else
        {
            m_runoffDriverMmPerHour = std::max(
                rainRate,
                m_runoffDriverMmPerHour * std::exp(-dt / 45.0f));
        }

        // LIVETRACK15 separates visual surface wetting from retained puddle
        // storage. The continuous wet film remembers rainfall quickly and dries only
        // gradually; no procedural impact-circle water state exists.
        const bool raining = rainRate > 0.01f;
        const float filmDryRateMmPerHour = raining
            ? std::min(evaporationRate * 0.10f, 0.05f)
            : std::min(drainageRate * 0.08f + evaporationRate, 0.60f);
        const float filmLossM = filmDryRateMmPerHour * (0.001f / 3600.0f) * dt;
        m_surfaceWettingExposureM = std::clamp(
            m_surfaceWettingExposureM + rainM - filmLossM, 0.0f, 0.004f);

        // Puddles fill only after the road begins connecting into a wet film.
        // The capture fraction ramps up smoothly, so little pools grow instead
        // of popping on. During rain they are deliberately hysteretic: normal
        // road drainage cannot erase a puddle faster than the shower creates
        // its visible wet state. Once rain stops, drainage/evaporation recede
        // the retained reservoir slowly over minutes rather than frames.
        const float wetConnection = std::clamp(
            m_surfaceWettingExposureM / 0.00125f, 0.0f, 1.0f);
        // LIVETRACK21C middle-ground retention. LIVETRACK21A's ~58% maximum
        // capture flooded broad catchments, while LIVETRACK21B's ~10% maximum plus
        // its visibility gates suppressed nearly all puddles. Cap persistent basin
        // capture at 15% and let the baked basin geometry decide where it can remain.
        const float saturation = std::clamp(
            (m_surfaceWettingExposureM - 0.00014f) / 0.00165f,
            0.0f,
            1.0f);
        const float captureFraction = 0.006f
            + 0.030f * wetConnection
            + 0.114f * saturation * saturation;
        const float retainedRainM = rainM * captureFraction;

        // LIVETRACK19 persistence contract: retained basin water NEVER
        // auto-drains/evaporates during the session. It can only stay or grow
        // until Reset Surface Water explicitly clears it. This makes it
        // impossible for a real baked puddle to blink out after appearing.
        m_backgroundSeedDepthM = std::clamp(
            m_backgroundSeedDepthM + retainedRainM, 0.0f, 0.032f);
    }
    m_stats.backgroundSeedDepthM = m_backgroundSeedDepthM;
    m_stats.surfaceWettingExposureM = m_surfaceWettingExposureM;
    m_stats.runoffDriverMmPerHour = m_runoffDriverMmPerHour;

    const bool prebakedResponseReady = prebakedHydrology && prebakedHydrology->stats().available;
    const bool topologyBecameReady = prebakedResponseReady && !m_stats.authoritative;
    if (topologyBecameReady && !m_tiles.empty())
    {
        for (const auto& [key, tile] : m_tiles)
        {
            (void)key;
            releaseTileSlot(tile.slot);
        }
        m_tiles.clear();
        invalidateFarTopologyCache();
        m_residencyCameraValid = false;
        forceResidencyRefresh = true;
    }
    // LIVETRACK18 polls topology residency at exactly 20Hz. Rendering itself
    // remains per-frame; this cadence only governs static tile admission and
    // the rolling far-cache uploads, preventing frame-rate-dependent churn.
    const bool presentationPollDue = m_lastPresentationPollSeconds < 0.0
        || elapsedSeconds + 1.0e-9 >=
            m_lastPresentationPollSeconds + kPresentationPollIntervalSeconds;
    m_stats.cpuBookkeepingMs += elapsedMs(bookkeepingStarted);
    if (forceResidencyRefresh || !m_centerTileValid || !m_residencyCameraValid
        || presentationPollDue)
    {
        const auto residencyStarted = std::chrono::steady_clock::now();
        refreshResidency(elapsedSeconds, cameraGlobalX, cameraGlobalZ,
            m_backgroundSeedDepthM, prebakedHydrology);
        m_stats.cpuResidencyMs = elapsedMs(residencyStarted);
        m_stats.residencyPolledThisFrame = true;
        m_stats.lastResidencyPollMs = m_stats.cpuResidencyMs;
        m_stats.lastFarTopologyTotalMs = m_stats.farTopologyTotalMs;
        m_stats.lastFarCandidateBuildMs = m_stats.farCandidateBuildMs;
        m_stats.lastFarCandidateSortMs = m_stats.farCandidateSortMs;
        m_stats.lastFarMissingScanMs = m_stats.farMissingScanMs;
        m_stats.lastFarTileResolveMs = m_stats.farTileResolveMs;
        m_stats.lastFarAtlasUploadGlMs = m_stats.farAtlasUploadGlMs;
        m_stats.lastFarTagUploadGlMs = m_stats.farTagUploadGlMs;
        m_stats.lastFarCandidateTilesEvaluated = m_stats.farCandidateTilesEvaluated;
        m_lastPresentationPollSeconds = elapsedSeconds;
    }

    const auto stateProvisionStarted = std::chrono::steady_clock::now();
    if (ambientTemperatureC <= 1.5f && precipitationRateMmPerHour > 0.0f && !m_snow.allocated)
    {
        std::string error;
        if (ensureSnowState(error) && dynamicSurface)
        {
            std::string geometryError;
            rebuildExactGeometryAtlas(dynamicSurface, geometryError);
        }
    }
    bool mudRequested = false;
    for (const auto& event : tireEvents) mudRequested = mudRequested || event.mudDeformable;
    if (mudRequested && !m_mud.allocated)
    {
        std::string error;
        if (ensureMudState(error) && dynamicSurface && !m_geometryAtlasReady)
        {
            std::string geometryError;
            rebuildExactGeometryAtlas(dynamicSurface, geometryError);
        }
    }
    m_stats.cpuStateProvisionMs = elapsedMs(stateProvisionStarted);

    const auto bookkeepingTailStarted = std::chrono::steady_clock::now();
    m_stats.snowReady = m_snow.allocated;
    m_stats.mudReady = m_mud.allocated;
    m_stats.authoritative = m_water.allocated && prebakedResponseReady;
    m_stats.waterReady = m_water.allocated;
    m_stats.cpuBookkeepingMs += elapsedMs(bookkeepingTailStarted);

    const auto geometryBindStarted = std::chrono::steady_clock::now();
    if (m_geometryAtlasReady)
    {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 20, m_geometryTriangleBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 21, m_geometryBinHeaderBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 22, m_geometryBinIndexBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 23, m_geometryTileMetaBuffer);
    }
    m_stats.cpuGeometryBindMs = elapsedMs(geometryBindStarted);

    const auto gpuTimerBeginStarted = std::chrono::steady_clock::now();
    const bool timerBegan = beginGpuTimer();
    m_stats.cpuGpuTimerWallMs += elapsedMs(gpuTimerBeginStarted);

    // Rain itself dispatches no full-field compute. Optional snow/mud may still
    // update at their own cadence, and tire clearing is localized to contact patches.
    const auto optionalDispatchStarted = std::chrono::steady_clock::now();
    dispatchDueTiles(elapsedSeconds, precipitationRateMmPerHour,
        weatherDrainageRateMmPerHour, evaporationRateMmPerHour, ambientTemperatureC);
    m_stats.cpuOptionalTileDispatchMs = elapsedMs(optionalDispatchStarted);

    const auto tireEventStarted = std::chrono::steady_clock::now();
    applyTireContactEvents(tireEvents);
    m_stats.cpuTireEventMs = elapsedMs(tireEventStarted);

    const auto tireWaterStarted = std::chrono::steady_clock::now();
    dispatchTireWaterSamples(tireWaterSampleRequests);
    m_stats.cpuTireWaterDispatchMs = elapsedMs(tireWaterStarted);

    const auto gpuTimerEndStarted = std::chrono::steady_clock::now();
    endGpuTimer(timerBegan);
    m_stats.cpuGpuTimerWallMs += elapsedMs(gpuTimerEndStarted);

    m_stats.centerTileX = m_centerTileX;
    m_stats.centerTileZ = m_centerTileZ;
    const auto cpuEnded = std::chrono::steady_clock::now();
    m_stats.cpuDispatchMs = std::chrono::duration<double, std::milli>(cpuEnded - cpuStarted).count();
    const double attributedMs =
        m_stats.cpuBookkeepingMs
        + m_stats.cpuTireReadbackPollMs
        + m_stats.cpuResidencyMs
        + m_stats.cpuStateProvisionMs
        + m_stats.cpuGeometryBindMs
        + m_stats.cpuGpuTimerWallMs
        + m_stats.cpuOptionalTileDispatchMs
        + m_stats.cpuTireEventMs
        + m_stats.cpuTireWaterDispatchMs;
    m_stats.cpuResidualMs = std::max(0.0, m_stats.cpuDispatchMs - attributedMs);
}

} // namespace heritage::graphics::dynamicsurface

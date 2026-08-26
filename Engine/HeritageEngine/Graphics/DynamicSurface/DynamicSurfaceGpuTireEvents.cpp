#include "DynamicSurfaceGpuRuntime.hpp"
#include "DynamicSurfaceGpuShaders.hpp"

#include "../../Physics/Surfaces/DynamicSurface/DynamicSurfaceSystem.hpp"
#include "../../Physics/Surfaces/Water/SurfaceHydrology.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace heritage::graphics::dynamicsurface {

void DynamicSurfaceGpuRuntime::applyTireContactEvents(
    const std::vector<DynamicSurfaceGpuTireContactEvent>& events)
{
    if (!m_tireEventProgram || !m_water.allocated || events.empty())
        return;
    const auto elapsedMs = [](const auto& started) {
        return std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
    };
    const auto setupStarted = std::chrono::steady_clock::now();
    glUseProgram(m_tireEventProgram);
    glBindImageTexture(0, m_water.atlas, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
    if (m_snow.allocated)
        glBindImageTexture(1, m_snow.atlas, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R16UI);
    if (m_mud.allocated)
        glBindImageTexture(2, m_mud.atlas, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R8UI);
    m_stats.tireEventSetupGlMs += elapsedMs(setupStarted);
    const std::size_t eventCount = std::min<std::size_t>(
        events.size(), kMaximumTireContactEventsPerFrame);
    for (std::size_t eventIndex = 0; eventIndex < eventCount; ++eventIndex)
    {
        const auto& event = events[eventIndex];
        const std::int32_t tileX = static_cast<std::int32_t>(std::floor(event.globalX / kTileWorldSizeM));
        const std::int32_t tileZ = static_cast<std::int32_t>(std::floor(event.globalZ / kTileWorldSizeM));
        const auto found = m_tiles.find(tileKey(tileX, tileZ));
        if (found == m_tiles.end())
            continue;
        TileRuntime& tile = found->second;
        tile.recentVehicleContact = true;
        const auto origin = atlasSlotOrigin(tile.slot);
        const double tileOriginX = static_cast<double>(tileX) * kTileWorldSizeM;
        const double tileOriginZ = static_cast<double>(tileZ) * kTileWorldSizeM;
        const float localX = static_cast<float>(event.globalX - tileOriginX);
        const float localZ = static_cast<float>(event.globalZ - tileOriginZ);
        const float halfLength = std::max(event.patchLengthM * 0.5f, kCellSizeM);
        const float halfWidth = std::max(event.patchWidthM * 0.5f, kCellSizeM);
        const float radiusM = std::max(halfLength, halfWidth) + kCellSizeM * 2.0f;
        const int minX = static_cast<int>(std::floor((localX - radiusM) / kCellSizeM));
        const int minZ = static_cast<int>(std::floor((localZ - radiusM) / kCellSizeM));
        const int maxX = static_cast<int>(std::ceil((localX + radiusM) / kCellSizeM));
        const int maxZ = static_cast<int>(std::ceil((localZ + radiusM) / kCellSizeM));
        const int extentX = std::max(maxX - minX + 1, 1);
        const int extentZ = std::max(maxZ - minZ + 1, 1);

        const auto uniformStarted = std::chrono::steady_clock::now();
        if (m_tireEventUniforms.atlasOrigin >= 0) glUniform2i(m_tireEventUniforms.atlasOrigin,
            static_cast<GLint>(origin[0]), static_cast<GLint>(origin[1]));
        if (m_tireEventUniforms.minTexel >= 0) glUniform2i(m_tireEventUniforms.minTexel, minX, minZ);
        if (m_tireEventUniforms.extentTexels >= 0) glUniform2i(m_tireEventUniforms.extentTexels, extentX, extentZ);
        if (m_tireEventUniforms.cellSizeM >= 0) glUniform1f(m_tireEventUniforms.cellSizeM, kCellSizeM);
        if (m_tireEventUniforms.eventLocalXZ >= 0) glUniform2f(m_tireEventUniforms.eventLocalXZ, localX, localZ);
        if (m_tireEventUniforms.forwardXZ >= 0) glUniform2f(m_tireEventUniforms.forwardXZ, event.forwardX, event.forwardZ);
        if (m_tireEventUniforms.rightXZ >= 0) glUniform2f(m_tireEventUniforms.rightXZ, event.rightX, event.rightZ);
        if (m_tireEventUniforms.patchHalfLengthM >= 0) glUniform1f(m_tireEventUniforms.patchHalfLengthM, halfLength);
        if (m_tireEventUniforms.patchHalfWidthM >= 0) glUniform1f(m_tireEventUniforms.patchHalfWidthM, halfWidth);
        if (m_tireEventUniforms.normalLoadN >= 0) glUniform1f(m_tireEventUniforms.normalLoadN, event.normalLoadN);
        if (m_tireEventUniforms.speedMps >= 0) glUniform1f(m_tireEventUniforms.speedMps, event.speedMps);
        if (m_tireEventUniforms.accumulatedDtSeconds >= 0) glUniform1f(m_tireEventUniforms.accumulatedDtSeconds, event.accumulatedDtSeconds);
        if (m_tireEventUniforms.snowReady >= 0) glUniform1i(m_tireEventUniforms.snowReady, m_snow.allocated ? 1 : 0);
        if (m_tireEventUniforms.mudReady >= 0) glUniform1i(m_tireEventUniforms.mudReady, m_mud.allocated ? 1 : 0);
        if (m_tireEventUniforms.mudDeformable >= 0) glUniform1i(m_tireEventUniforms.mudDeformable, event.mudDeformable ? 1 : 0);
        m_stats.tireEventUniformGlMs += elapsedMs(uniformStarted);

        const auto dispatchStarted = std::chrono::steady_clock::now();
        glDispatchCompute(static_cast<GLuint>((extentX + 7) / 8),
            static_cast<GLuint>((extentZ + 7) / 8), 1u);
        const double dispatchMs = elapsedMs(dispatchStarted);
        m_stats.tireEventDispatchGlMs += dispatchMs;
        m_stats.tireEventSlowestDispatchGlMs =
            std::max(m_stats.tireEventSlowestDispatchGlMs, dispatchMs);
        ++m_stats.tireEventDispatches;
        const std::uint64_t cells = static_cast<std::uint64_t>(extentX)
            * static_cast<std::uint64_t>(extentZ);
        m_stats.tireEventCells += cells;
        ++m_stats.dispatchesThisFrame;
        m_stats.cellsThisFrame += cells;
    }
    const auto barrierStarted = std::chrono::steady_clock::now();
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    m_stats.tireEventBarrierGlMs += elapsedMs(barrierStarted);
}

} // namespace heritage::graphics::dynamicsurface

namespace heritage::graphics::dynamicsurface {

bool DynamicSurfaceGpuRuntime::initializeTireWaterSampleBridge(std::string& errorMessage)
{
    constexpr GLsizeiptr bytes = static_cast<GLsizeiptr>(
        kMaximumTireWaterSampleRequestsPerFrame * sizeof(std::array<float, 4>));
    for (auto& slot : m_tireWaterSampleReadback)
    {
        glGenBuffers(1, &slot.inputBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, slot.inputBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, bytes, nullptr, GL_STREAM_DRAW);
        glGenBuffers(1, &slot.outputBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, slot.outputBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, bytes, nullptr, GL_STREAM_READ);
        slot.globalPositions.reserve(kMaximumTireWaterSampleRequestsPerFrame);
        if (!slot.inputBuffer || !slot.outputBuffer)
        {
            errorMessage = "OPT03B tire-water sample SSBO allocation failed.";
            return false;
        }
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return detail::checkNoGlError(errorMessage, "OPT03B tire-water sample bridge allocation");
}

void DynamicSurfaceGpuRuntime::shutdownTireWaterSampleBridge()
{
    for (auto& slot : m_tireWaterSampleReadback)
    {
        if (slot.fence)
            glDeleteSync(slot.fence);
        if (slot.outputBuffer)
            glDeleteBuffers(1, &slot.outputBuffer);
        if (slot.inputBuffer)
            glDeleteBuffers(1, &slot.inputBuffer);
        slot = {};
    }
    m_tireWaterSampleWriteIndex = 0u;
    m_completedTireWaterSamples.clear();
}

void DynamicSurfaceGpuRuntime::pollTireWaterSampleReadbacks()
{
    const auto elapsedMs = [](const auto& started) {
        return std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
    };
    for (auto& slot : m_tireWaterSampleReadback)
    {
        if (!slot.fence)
            continue;
        const auto waitStarted = std::chrono::steady_clock::now();
        const GLenum status = glClientWaitSync(slot.fence, 0, 0);
        m_stats.tireReadbackClientWaitGlMs += elapsedMs(waitStarted);
        if (status != GL_ALREADY_SIGNALED && status != GL_CONDITION_SATISFIED)
            continue;
        glDeleteSync(slot.fence);
        slot.fence = nullptr;
        if (slot.count == 0u)
            continue;

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, slot.outputBuffer);
        const GLsizeiptr bytes = static_cast<GLsizeiptr>(
            slot.count * sizeof(std::array<float, 4>));
        const auto mapStarted = std::chrono::steady_clock::now();
        const auto* mapped = static_cast<const std::array<float, 4>*>(
            glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, bytes, GL_MAP_READ_BIT));
        m_stats.tireReadbackMapGlMs += elapsedMs(mapStarted);
        if (mapped)
        {
            const bool currentHydrology =
                slot.hydrologyResetSerial == m_appliedHydrologyResetSerial;
            for (std::uint32_t i = 0; currentHydrology && i < slot.count; ++i)
            {
                DynamicSurfaceGpuTireWaterSample sample;
                sample.globalX = slot.globalPositions[i][0];
                sample.globalZ = slot.globalPositions[i][1];
                sample.waterDepthM = std::max(mapped[i][0], 0.0f);
                sample.dryLine = std::clamp(mapped[i][1], 0.0f, 1.0f);
                sample.valid = mapped[i][2] > 0.5f;
                m_completedTireWaterSamples.push_back(sample);
            }
            const auto unmapStarted = std::chrono::steady_clock::now();
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
            m_stats.tireReadbackUnmapGlMs += elapsedMs(unmapStarted);
            if (currentHydrology)
                m_stats.tireWaterSamplesCompleted += slot.count;
        }
        slot.count = 0u;
        slot.globalPositions.clear();
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void DynamicSurfaceGpuRuntime::dispatchTireWaterSamples(
    const std::vector<DynamicSurfaceGpuTireWaterSampleRequest>& requests)
{
    if (!m_stats.authoritative || !m_tireWaterSampleProgram || requests.empty())
        return;
    const auto elapsedMs = [](const auto& started) {
        return std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
    };
    auto& slot = m_tireWaterSampleReadback[m_tireWaterSampleWriteIndex];
    if (slot.fence)
    {
        ++m_stats.tireWaterSampleReadbackDrops;
        return; // Never wait for the GPU; stale physics samples are preferable to a stall.
    }

    const std::size_t count = std::min<std::size_t>(
        requests.size(), kMaximumTireWaterSampleRequestsPerFrame);
    std::vector<std::array<float, 4>> packed;
    packed.reserve(count);
    slot.globalPositions.clear();
    slot.globalPositions.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto& request = requests[i];
        const std::int32_t tileX = static_cast<std::int32_t>(std::floor(request.globalX / kTileWorldSizeM));
        const std::int32_t tileZ = static_cast<std::int32_t>(std::floor(request.globalZ / kTileWorldSizeM));
        const double localX = request.globalX - static_cast<double>(tileX) * kTileWorldSizeM;
        const double localZ = request.globalZ - static_cast<double>(tileZ) * kTileWorldSizeM;
        packed.push_back({
            static_cast<float>(tileX - m_centerTileX),
            static_cast<float>(tileZ - m_centerTileZ),
            static_cast<float>(std::clamp(localX / kTileWorldSizeM, 0.0, 0.999999)),
            static_cast<float>(std::clamp(localZ / kTileWorldSizeM, 0.0, 0.999999)) });
        slot.globalPositions.push_back({ request.globalX, request.globalZ });
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, slot.inputBuffer);
    const auto uploadStarted = std::chrono::steady_clock::now();
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
        static_cast<GLsizeiptr>(packed.size() * sizeof(packed[0])), packed.data());
    m_stats.tireWaterUploadGlMs += elapsedMs(uploadStarted);

    const auto setupStarted = std::chrono::steady_clock::now();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, slot.inputBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, slot.outputBuffer);
    glUseProgram(m_tireWaterSampleProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_water.atlas);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_tileIndirectionTexture);
    if (m_tireWaterSampleUniforms.waterAtlas >= 0) glUniform1i(m_tireWaterSampleUniforms.waterAtlas, 0);
    if (m_tireWaterSampleUniforms.tileIndirection >= 0) glUniform1i(m_tireWaterSampleUniforms.tileIndirection, 1);
    if (m_tireWaterSampleUniforms.tileResolution >= 0) glUniform1i(m_tireWaterSampleUniforms.tileResolution, static_cast<GLint>(kTileResolution));
    if (m_tireWaterSampleUniforms.atlasColumns >= 0) glUniform1i(m_tireWaterSampleUniforms.atlasColumns, static_cast<GLint>(kAtlasColumns));
    if (m_tireWaterSampleUniforms.tileMapCenter >= 0) glUniform1i(m_tireWaterSampleUniforms.tileMapCenter, kTileMapHalfSpan);
    if (m_tireWaterSampleUniforms.prebakedWaterExposureM >= 0) glUniform1f(m_tireWaterSampleUniforms.prebakedWaterExposureM, m_backgroundSeedDepthM);
    if (m_tireWaterSampleUniforms.rainWettingExposureM >= 0) glUniform1f(m_tireWaterSampleUniforms.rainWettingExposureM, m_surfaceWettingExposureM);
    if (m_tireWaterSampleUniforms.runoffDriverMmPerHour >= 0) glUniform1f(m_tireWaterSampleUniforms.runoffDriverMmPerHour, m_runoffDriverMmPerHour);
    if (m_tireWaterSampleUniforms.sampleCount >= 0) glUniform1i(m_tireWaterSampleUniforms.sampleCount, static_cast<GLint>(count));
    m_stats.tireWaterSetupGlMs += elapsedMs(setupStarted);

    const auto dispatchStarted = std::chrono::steady_clock::now();
    glDispatchCompute(static_cast<GLuint>((count + 63u) / 64u), 1u, 1u);
    m_stats.tireWaterDispatchGlMs += elapsedMs(dispatchStarted);

    const auto barrierStarted = std::chrono::steady_clock::now();
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    m_stats.tireWaterBarrierGlMs += elapsedMs(barrierStarted);

    slot.count = static_cast<std::uint32_t>(count);
    slot.hydrologyResetSerial = m_appliedHydrologyResetSerial;
    const auto fenceStarted = std::chrono::steady_clock::now();
    slot.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    m_stats.tireWaterFenceGlMs += elapsedMs(fenceStarted);
    if (!slot.fence)
    {
        slot.count = 0u;
        slot.globalPositions.clear();
        ++m_stats.tireWaterSampleReadbackDrops;
        return;
    }
    ++m_stats.tireWaterSampleDispatches;
    m_tireWaterSampleWriteIndex = (m_tireWaterSampleWriteIndex + 1u)
        % m_tireWaterSampleReadback.size();
}

void DynamicSurfaceGpuRuntime::consumeCompletedTireWaterSamples(
    std::vector<DynamicSurfaceGpuTireWaterSample>& outSamples)
{
    outSamples.clear();
    outSamples.swap(m_completedTireWaterSamples);
}

} // namespace heritage::graphics::dynamicsurface

#include "DynamicSurfaceGpuRuntime.hpp"

#include "../../Physics/Surfaces/DynamicSurface/DynamicSurfaceSystem.hpp"
#include "../../Physics/Surfaces/Water/SurfaceHydrology.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace heritage::graphics::dynamicsurface {

std::uint32_t DynamicSurfaceGpuRuntime::farAtlasSlotCoordinate(std::int32_t worldTile)
{
    const std::int32_t axis = static_cast<std::int32_t>(kFarAtlasTilesPerAxis);
    const std::int32_t wrapped = ((worldTile % axis) + axis) % axis;
    return static_cast<std::uint32_t>(wrapped);
}
void DynamicSurfaceGpuRuntime::invalidateFarTopologyCache()
{
    if (m_farTileTags.empty())
        return;
    const std::array<std::int32_t, 2> invalid{{
        (std::numeric_limits<std::int32_t>::min)(),
        (std::numeric_limits<std::int32_t>::min)() }};
    std::fill(m_farTileTags.begin(), m_farTileTags.end(), invalid);
    if (m_farTileTagTexture)
    {
        glTextureSubImage2D(m_farTileTagTexture, 0, 0, 0,
            static_cast<GLsizei>(kFarAtlasTilesPerAxis),
            static_cast<GLsizei>(kFarAtlasTilesPerAxis),
            GL_RG_INTEGER, GL_INT, m_farTileTags.data());
    }
}
void DynamicSurfaceGpuRuntime::streamFarTopology(
    double cameraGlobalX,
    double cameraGlobalZ,
    const heritage::physics::water::SurfaceHydrology* prebakedHydrology)
{
    const auto elapsedMs = [](const auto& started) {
        return std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
    };
    const auto farTotalStarted = std::chrono::steady_clock::now();
    const auto candidateBuildStarted = farTotalStarted;

    struct Desired
    {
        std::int32_t x = 0;
        std::int32_t z = 0;
        float distanceM = 0.0f;
    };

    std::vector<Desired> candidates;
    const int tileRadius = static_cast<int>(
        std::ceil(kFarTopologyPrefetchRadiusM / kTileWorldSizeM)) + 1;
    candidates.reserve(static_cast<std::size_t>((tileRadius * 2 + 1) * (tileRadius * 2 + 1)));
    for (int dz = -tileRadius; dz <= tileRadius; ++dz)
    {
        for (int dx = -tileRadius; dx <= tileRadius; ++dx)
        {
            const std::int32_t tileX = m_centerTileX + dx;
            const std::int32_t tileZ = m_centerTileZ + dz;
            const double minimumX = static_cast<double>(tileX) * kTileWorldSizeM;
            const double minimumZ = static_cast<double>(tileZ) * kTileWorldSizeM;
            const double nearestX = std::clamp(
                cameraGlobalX, minimumX, minimumX + kTileWorldSizeM);
            const double nearestZ = std::clamp(
                cameraGlobalZ, minimumZ, minimumZ + kTileWorldSizeM);
            const float distanceM = static_cast<float>(std::hypot(
                cameraGlobalX - nearestX, cameraGlobalZ - nearestZ));
            if (distanceM <= kFarTopologyPrefetchRadiusM)
                candidates.push_back({tileX, tileZ, distanceM});
        }
    }
    m_stats.farCandidateBuildMs += elapsedMs(candidateBuildStarted);
    const auto candidateSortStarted = std::chrono::steady_clock::now();
    std::sort(candidates.begin(), candidates.end(), [](const Desired& a, const Desired& b) {
        if (a.distanceM != b.distanceM) return a.distanceM < b.distanceM;
        if (a.z != b.z) return a.z < b.z;
        return a.x < b.x;
    });
    m_stats.farCandidateSortMs += elapsedMs(candidateSortStarted);
    m_stats.farCandidateTilesEvaluated = static_cast<std::uint32_t>(candidates.size());

    const bool topologyReady = prebakedHydrology && prebakedHydrology->stats().available;
    std::uint32_t visibleDesired = 0u;
    std::uint32_t visibleResident = 0u;
    std::uint32_t uploads = 0u;
    bool tagsChanged = false;
    bool bulkUpload = false;
    const std::size_t farTexelCount = static_cast<std::size_t>(kFarTileResolution)
        * static_cast<std::size_t>(kFarTileResolution);

    // LIVETRACK18: every tile in the complete 500m set is made renderable in
    // this one 20Hz poll. There is intentionally no per-poll tile budget or
    // progressive backlog. The source payload is already prebaked in .hhyd v9,
    // so this stage only decompresses/copies immutable RGB8 runoff/standing-depth/flow
    // topology to the GPU.
    std::uint32_t missingCount = 0u;
    const auto missingScanStarted = std::chrono::steady_clock::now();
    if (topologyReady)
    {
        for (const Desired& item : candidates)
        {
            const std::uint32_t slotX = farAtlasSlotCoordinate(item.x);
            const std::uint32_t slotZ = farAtlasSlotCoordinate(item.z);
            const std::size_t slot = static_cast<std::size_t>(slotZ)
                * kFarAtlasTilesPerAxis + slotX;
            const auto expected = std::array<std::int32_t, 2>{{item.x, item.z}};
            if (slot >= m_farTileTags.size() || m_farTileTags[slot] != expected)
                ++missingCount;
        }
    }
    m_stats.farMissingScanMs += elapsedMs(missingScanStarted);
    bulkUpload = missingCount >= kFarBulkUploadThreshold;

    for (const Desired& item : candidates)
    {
        const bool visible = item.distanceM <= kPresentationRadiusM;
        if (visible) ++visibleDesired;

        const std::uint32_t slotX = farAtlasSlotCoordinate(item.x);
        const std::uint32_t slotZ = farAtlasSlotCoordinate(item.z);
        const std::size_t slot = static_cast<std::size_t>(slotZ)
            * kFarAtlasTilesPerAxis + slotX;
        const auto expected = std::array<std::int32_t, 2>{{item.x, item.z}};
        if (slot < m_farTileTags.size() && m_farTileTags[slot] == expected)
        {
            if (visible) ++visibleResident;
            continue;
        }

        if (!topologyReady)
            continue;

        const auto tileResolveStarted = std::chrono::steady_clock::now();
        m_prebakedTopologyScratch.clear();
        const bool bakedTileReady = prebakedHydrology->prebakedFarPuddleResponseTile(
            item.x, item.z, m_prebakedTopologyScratch);
        if (!bakedTileReady || m_prebakedTopologyScratch.size() != farTexelCount * 3u)
        {
            // Unauthored tile: zero free-water capacity, no flow. Wet material
            // still comes from scene rain/wetting memory independently.
            m_prebakedTopologyScratch.assign(farTexelCount * 3u, 0u);
        }

        const std::size_t atlasOriginX = static_cast<std::size_t>(slotX) * kFarTileResolution;
        const std::size_t atlasOriginZ = static_cast<std::size_t>(slotZ) * kFarTileResolution;
        for (std::uint32_t z = 0; z < kFarTileResolution; ++z)
        {
            const std::size_t source = static_cast<std::size_t>(z) * kFarTileResolution * 3u;
            const std::size_t destination = ((atlasOriginZ + z) * kFarAtlasWidth
                + atlasOriginX) * 3u;
            std::copy_n(m_prebakedTopologyScratch.data() + source,
                static_cast<std::size_t>(kFarTileResolution) * 3u,
                m_farAtlasCpuMirror.data() + destination);
        }
        m_stats.farTileResolveMs += elapsedMs(tileResolveStarted);
        if (!bulkUpload)
        {
            const auto farUploadStarted = std::chrono::steady_clock::now();
            glTextureSubImage2D(m_farWaterAtlas, 0,
                static_cast<GLint>(slotX * kFarTileResolution),
                static_cast<GLint>(slotZ * kFarTileResolution),
                static_cast<GLsizei>(kFarTileResolution),
                static_cast<GLsizei>(kFarTileResolution),
                GL_RGB, GL_UNSIGNED_BYTE, m_prebakedTopologyScratch.data());
            m_stats.farAtlasUploadGlMs += elapsedMs(farUploadStarted);
        }
        m_farTileTags[slot] = expected;
        tagsChanged = true;
        ++uploads;
        if (visible) ++visibleResident;
    }

    if (bulkUpload && uploads > 0u)
    {
        const auto farUploadStarted = std::chrono::steady_clock::now();
        glTextureSubImage2D(m_farWaterAtlas, 0, 0, 0,
            static_cast<GLsizei>(kFarAtlasWidth),
            static_cast<GLsizei>(kFarAtlasHeight),
            GL_RGB, GL_UNSIGNED_BYTE, m_farAtlasCpuMirror.data());
        m_stats.farAtlasUploadGlMs += elapsedMs(farUploadStarted);
    }
    if (tagsChanged)
    {
        const auto tagUploadStarted = std::chrono::steady_clock::now();
        glTextureSubImage2D(m_farTileTagTexture, 0, 0, 0,
            static_cast<GLsizei>(kFarAtlasTilesPerAxis),
            static_cast<GLsizei>(kFarAtlasTilesPerAxis),
            GL_RG_INTEGER, GL_INT, m_farTileTags.data());
        m_stats.farTagUploadGlMs += elapsedMs(tagUploadStarted);
    }

    m_stats.farDesiredTopologyTiles = visibleDesired;
    m_stats.farResidentTopologyTiles = visibleResident;
    m_stats.farTopologyUploadsThisFrame += uploads;
    // If .hhyd is ready, every desired tile is resolved in this poll. A nonzero
    // value therefore means the scene topology is unavailable, not throttled.
    m_stats.farTopologyBacklogTiles = visibleDesired > visibleResident
        ? visibleDesired - visibleResident : 0u;
    m_stats.farTopologyTotalMs += elapsedMs(farTotalStarted);
}

} // namespace heritage::graphics::dynamicsurface

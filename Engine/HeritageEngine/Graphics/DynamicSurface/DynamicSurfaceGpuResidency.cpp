#include "DynamicSurfaceGpuRuntime.hpp"

#include "../../Physics/Surfaces/DynamicSurface/DynamicSurfaceSystem.hpp"
#include "../../Physics/Surfaces/Water/SurfaceHydrology.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace heritage::graphics::dynamicsurface {

std::uint64_t DynamicSurfaceGpuRuntime::tileKey(std::int32_t x, std::int32_t z)
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32u)
        | static_cast<std::uint32_t>(z);
}
std::uint8_t DynamicSurfaceGpuRuntime::cadenceBandForDistance(float distanceM, bool prewarm) const
{
    if (prewarm || distanceM > kSimulationRadiusM) return 3u;
    const float f = distanceM / kSimulationRadiusM;
    if (f <= 0.25f) return 0u;
    if (f <= 0.50f) return 1u;
    if (f <= 0.75f) return 2u;
    return 3u;
}
std::array<std::uint32_t, 2> DynamicSurfaceGpuRuntime::atlasSlotOrigin(std::uint16_t slot)
{
    return {
        (static_cast<std::uint32_t>(slot) % kAtlasColumns) * kTileResolution,
        (static_cast<std::uint32_t>(slot) / kAtlasColumns) * kTileResolution };
}
std::uint16_t DynamicSurfaceGpuRuntime::allocateTileSlot()
{
    if (m_freeSlots.empty())
        return std::numeric_limits<std::uint16_t>::max();
    const std::uint16_t slot = m_freeSlots.back();
    m_freeSlots.pop_back();
    return slot;
}
void DynamicSurfaceGpuRuntime::releaseTileSlot(std::uint16_t slot)
{
    m_freeSlots.push_back(slot);
}
void DynamicSurfaceGpuRuntime::refreshResidency(
    double elapsedSeconds,
    double cameraGlobalX,
    double cameraGlobalZ,
    float backgroundSeedDepthM,
    const heritage::physics::water::SurfaceHydrology* prebakedHydrology)
{
    (void)backgroundSeedDepthM;
    const auto elapsedMs = [](const auto& started) {
        return std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
    };
    const auto nearBuildStarted = std::chrono::steady_clock::now();
    const std::int32_t newCenterX = static_cast<std::int32_t>(
        std::floor(cameraGlobalX / kTileWorldSizeM));
    const std::int32_t newCenterZ = static_cast<std::int32_t>(
        std::floor(cameraGlobalZ / kTileWorldSizeM));
    if (!m_centerTileValid || newCenterX != m_centerTileX || newCenterZ != m_centerTileZ)
    {
        if (m_centerTileValid) ++m_stats.cameraTileRebases;
        m_centerTileX = newCenterX;
        m_centerTileZ = newCenterZ;
        m_centerTileValid = true;
    }

    struct Desired { std::int32_t x; std::int32_t z; float distanceM; };
    std::vector<Desired> candidates;
    candidates.reserve(kMaximumResidentTiles);
    const int tileRadius = static_cast<int>(std::ceil(kTopologyPrefetchRadiusM / kTileWorldSizeM)) + 1;
    for (int dz = -tileRadius; dz <= tileRadius; ++dz)
    {
        for (int dx = -tileRadius; dx <= tileRadius; ++dx)
        {
            const std::int32_t tileX = m_centerTileX + dx;
            const std::int32_t tileZ = m_centerTileZ + dz;
            const double minimumX = static_cast<double>(tileX) * kTileWorldSizeM;
            const double minimumZ = static_cast<double>(tileZ) * kTileWorldSizeM;
            const double maximumX = minimumX + kTileWorldSizeM;
            const double maximumZ = minimumZ + kTileWorldSizeM;
            const double nearestX = std::clamp(cameraGlobalX, minimumX, maximumX);
            const double nearestZ = std::clamp(cameraGlobalZ, minimumZ, maximumZ);
            const float distanceM = static_cast<float>(std::hypot(
                cameraGlobalX - nearestX, cameraGlobalZ - nearestZ));
            if (distanceM <= kTopologyPrefetchRadiusM)
                candidates.push_back({tileX, tileZ, distanceM});
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const Desired& a, const Desired& b) {
        if (a.distanceM != b.distanceM) return a.distanceM < b.distanceM;
        if (a.z != b.z) return a.z < b.z;
        return a.x < b.x;
    });
    if (candidates.size() > kMaximumResidentTiles) candidates.resize(kMaximumResidentTiles);
    m_stats.desiredTopologyTiles = static_cast<std::uint32_t>(candidates.size());

    std::unordered_set<std::uint64_t> desired;
    desired.reserve(candidates.size());
    for (const auto& item : candidates) desired.insert(tileKey(item.x, item.z));

    bool residencyChanged = false;
    for (auto it = m_tiles.begin(); it != m_tiles.end();)
    {
        if (desired.find(it->first) != desired.end()) { ++it; continue; }
        releaseTileSlot(it->second.slot);
        it = m_tiles.erase(it);
        residencyChanged = true;
    }
    m_stats.nearResidencyBuildMs += elapsedMs(nearBuildStarted);

    const bool topologyReady = prebakedHydrology && prebakedHydrology->stats().available;
    const std::size_t texelCount = static_cast<std::size_t>(kTileResolution) * kTileResolution;
    for (const auto& item : candidates)
    {
        const std::uint64_t key = tileKey(item.x, item.z);
        if (m_tiles.find(key) != m_tiles.end()) continue;
        const std::uint16_t slot = allocateTileSlot();
        if (slot == std::numeric_limits<std::uint16_t>::max()) continue;

        TileRuntime tile;
        tile.x = item.x; tile.z = item.z; tile.slot = slot;
        tile.cadenceBand = cadenceBandForDistance(item.distanceM, false);
        tile.initialized = true;
        tile.lastUpdateSeconds = elapsedSeconds;
        tile.nextDueSeconds = elapsedSeconds + 0.5;
        m_tiles.emplace(key, tile);

        // Every admitted tile is uploaded, even if the scene cache has a gap.
        // LIVETRACK18 fallback capacity is exactly zero: ordinary rain film is
        // a separate shader-side wetness state, so a cache gap cannot invent a
        // standing puddle or consume a shallow-capacity code.
        const auto rasterStarted = std::chrono::steady_clock::now();
        m_prebakedRgbaScratch.assign(texelCount * 4u, 0u);

        const bool prebakedTileReady = topologyReady
            && prebakedHydrology->rasterPrebakedPuddleResponseTile(
                item.x, item.z, kTileResolution, m_prebakedTopologyScratch);
        if (prebakedTileReady)
        {
            for (std::size_t texel = 0; texel < texelCount; ++texel)
            {
                const std::size_t rgba = texel * 4u;
                const std::size_t topology = texel * 3u;
                // R = immutable baked runoff accumulation, G = tire dry-line,
                // B = immutable free-water capacity, A = immutable flow angle.
                m_prebakedRgbaScratch[rgba] = m_prebakedTopologyScratch[topology];
                m_prebakedRgbaScratch[rgba + 2u] = m_prebakedTopologyScratch[topology + 1u];
                m_prebakedRgbaScratch[rgba + 3u] = m_prebakedTopologyScratch[topology + 2u];
            }
        }
        m_stats.nearTopologyRasterMs += elapsedMs(rasterStarted);
        auto insertedTile = m_tiles.find(key);
        if (insertedTile != m_tiles.end())
            insertedTile->second.prebakedTopology = prebakedTileReady;
        const auto origin = atlasSlotOrigin(slot);
        const auto nearUploadStarted = std::chrono::steady_clock::now();
        glTextureSubImage2D(m_water.atlas, 0,
            static_cast<GLint>(origin[0]), static_cast<GLint>(origin[1]),
            static_cast<GLsizei>(kTileResolution), static_cast<GLsizei>(kTileResolution),
            GL_RGBA, GL_UNSIGNED_BYTE, m_prebakedRgbaScratch.data());
        m_stats.nearTopologyUploadGlMs += elapsedMs(nearUploadStarted);
        ++m_stats.topologyUploadsThisFrame;
        if (m_snow.allocated) clearStateSlot(m_snow, slot, 0u);
        if (m_mud.allocated) clearStateSlot(m_mud, slot, 0u);
        residencyChanged = true;
    }

    m_stats.activeTiles.fill(0u);
    m_stats.visibleTopologyTiles = 0u;
    m_stats.prebakedTopologyTiles = 0u;
    m_stats.fallbackTopologyTiles = 0u;
    m_stats.prewarmTiles = 0u;
    for (auto& [key, tile] : m_tiles)
    {
        (void)key;
        const double minimumX = static_cast<double>(tile.x) * kTileWorldSizeM;
        const double minimumZ = static_cast<double>(tile.z) * kTileWorldSizeM;
        const double nearestX = std::clamp(cameraGlobalX, minimumX, minimumX + kTileWorldSizeM);
        const double nearestZ = std::clamp(cameraGlobalZ, minimumZ, minimumZ + kTileWorldSizeM);
        const float distanceM = static_cast<float>(std::hypot(
            cameraGlobalX - nearestX, cameraGlobalZ - nearestZ));
        tile.cadenceBand = cadenceBandForDistance(distanceM, false);
        ++m_stats.activeTiles[std::min<std::size_t>(tile.cadenceBand, 3u)];
        if (distanceM <= kSimulationRadiusM)
            ++m_stats.visibleTopologyTiles;
        else
            ++m_stats.prewarmTiles;
        if (tile.prebakedTopology)
            ++m_stats.prebakedTopologyTiles;
        else
            ++m_stats.fallbackTopologyTiles;
    }
    m_stats.residentTiles = static_cast<std::uint32_t>(m_tiles.size());
    m_stats.cameraSpeedMps = std::hypot(m_cameraVelocityX, m_cameraVelocityZ);
    m_stats.predictivePrewarmM = 0.0;
    if (residencyChanged || m_tileMapOriginX != m_centerTileX - kTileMapHalfSpan
        || m_tileMapOriginZ != m_centerTileZ - kTileMapHalfSpan)
        rebuildTileIndirection();

    // The 500m mesh-prebaked presentation cache streams independently at the
    // fixed 20Hz residency poll. It never receives tire/CFD simulation state.
    streamFarTopology(cameraGlobalX, cameraGlobalZ, prebakedHydrology);

    m_lastResidencyCameraGlobalX = cameraGlobalX;
    m_lastResidencyCameraGlobalZ = cameraGlobalZ;
    m_residencyCameraValid = true;
}
void DynamicSurfaceGpuRuntime::rebuildTileIndirection()
{
    m_tileMapOriginX = m_centerTileX - kTileMapHalfSpan;
    m_tileMapOriginZ = m_centerTileZ - kTileMapHalfSpan;
    std::fill(m_tileIndirectionScratch.begin(), m_tileIndirectionScratch.end(), 0u);
    for (const auto& [key, tile] : m_tiles)
    {
        (void)key;
        const int mx = tile.x - m_tileMapOriginX;
        const int mz = tile.z - m_tileMapOriginZ;
        if (mx < 0 || mz < 0 || mx >= static_cast<int>(kTileMapResolution)
            || mz >= static_cast<int>(kTileMapResolution))
            continue;
        m_tileIndirectionScratch[static_cast<std::size_t>(mz) * kTileMapResolution + mx]
            = static_cast<std::uint16_t>(tile.slot + 1u);
    }
    const auto uploadStarted = std::chrono::steady_clock::now();
    glBindTexture(GL_TEXTURE_2D, m_tileIndirectionTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
        static_cast<GLsizei>(kTileMapResolution), static_cast<GLsizei>(kTileMapResolution),
        GL_RED_INTEGER, GL_UNSIGNED_SHORT, m_tileIndirectionScratch.data());
    m_stats.tileIndirectionUploadGlMs += std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - uploadStarted).count();
}

} // namespace heritage::graphics::dynamicsurface

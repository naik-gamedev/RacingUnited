#include "DynamicSurfaceGpuShaders.hpp"
#include "DynamicSurfaceGpuRuntime.hpp"

#include "../../Physics/Surfaces/DynamicSurface/DynamicSurfaceSystem.hpp"
#include "../../Physics/Surfaces/Water/SurfaceHydrology.hpp"
#include "../../Physics/Surfaces/Presentation/SurfacePresentation.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace heritage::graphics::dynamicsurface {

bool DynamicSurfaceGpuRuntime::allocateState(
    StateRuntime& state,
    GLenum internalFormat,
    GLenum clearFormat,
    GLenum clearType,
    std::size_t bytesPerTexel,
    bool allocateScratch,
    std::string& errorMessage)
{
    destroyState(state);
    state.internalFormat = internalFormat;
    state.clearFormat = clearFormat;
    state.clearType = clearType;
    state.bytesPerTexel = bytesPerTexel;

    glGenTextures(1, &state.atlas);
    glBindTexture(GL_TEXTURE_2D, state.atlas);
    glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat,
        static_cast<GLsizei>(kAtlasWidth), static_cast<GLsizei>(kAtlasHeight));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (internalFormat == GL_RGBA8 || internalFormat == GL_R8)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    if (allocateScratch)
    {
        glGenTextures(1, &state.scratch);
        glBindTexture(GL_TEXTURE_2D, state.scratch);
        glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat,
            static_cast<GLsizei>(kTileResolution), static_cast<GLsizei>(kTileResolution));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    if (!state.atlas || (allocateScratch && !state.scratch)
        || !detail::checkNoGlError(errorMessage, "state atlas allocation"))
    {
        destroyState(state);
        return false;
    }
    state.allocated = true;
    double bytes = static_cast<double>(kAtlasWidth)
        * static_cast<double>(kAtlasHeight) * static_cast<double>(bytesPerTexel);
    if (allocateScratch)
        bytes += static_cast<double>(kTileResolution) * static_cast<double>(kTileResolution)
            * static_cast<double>(bytesPerTexel);
    m_stats.committedMiB += bytes / (1024.0 * 1024.0);
    return true;
}
bool DynamicSurfaceGpuRuntime::ensureSnowState(std::string& errorMessage)
{
    if (m_snow.allocated)
        return true;
    if (!allocateState(m_snow, GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT,
            sizeof(std::uint16_t), true, errorMessage))
        return false;
    for (const auto& [key, tile] : m_tiles)
    {
        (void)key;
        clearStateSlot(m_snow, tile.slot, 0u);
    }
    m_stats.snowReady = true;
    return true;
}
bool DynamicSurfaceGpuRuntime::ensureMudState(std::string& errorMessage)
{
    if (m_mud.allocated)
        return true;
    if (!allocateState(m_mud, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE,
            sizeof(std::uint8_t), true, errorMessage))
        return false;
    for (const auto& [key, tile] : m_tiles)
    {
        (void)key;
        clearStateSlot(m_mud, tile.slot, 0u);
    }
    m_stats.mudReady = true;
    return true;
}
bool DynamicSurfaceGpuRuntime::initializeTireMarkState(std::string& errorMessage)
{
    // LIVETRACK22 resource ownership stays in the resource split: the R8 tire-mark
    // atlas uses the same 10m / 256x256 slot identities and indirection as water.
    if (!allocateState(m_tireMarks, GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1u, false, errorMessage))
        return false;
    m_stats.tireMarksReady = true;
    return true;
}

void DynamicSurfaceGpuRuntime::shutdownTireMarkState()
{
    destroyState(m_tireMarks);
    m_tireMarkRasterScratch.clear();
    m_tireMarkSerialsByTile.clear();
    m_dirtyTireMarkTiles.clear();
    m_lastIndexedTireMarkSerial = 0u;
    m_lastTireMarkFirstSerial = 0u;
    m_lastTireMarkPresentationTime = -1.0;
    m_lastTireMarkRetirementRefreshTime = -1.0;
    m_stats.tireMarksReady = false;
}

void DynamicSurfaceGpuRuntime::destroyState(StateRuntime& state)
{
    if (state.scratch)
        glDeleteTextures(1, &state.scratch);
    if (state.atlas)
        glDeleteTextures(1, &state.atlas);
    state = {};
}
void DynamicSurfaceGpuRuntime::clearStateSlot(
    StateRuntime& state,
    std::uint16_t slot,
    std::uint32_t clearValue)
{
    if (!state.allocated)
        return;
    const auto origin = atlasSlotOrigin(slot);
    if (state.internalFormat == GL_RGBA8)
    {
        const std::array<std::uint8_t, 4> rgba{{
            static_cast<std::uint8_t>(clearValue & 0xffu),
            static_cast<std::uint8_t>((clearValue >> 8u) & 0xffu),
            static_cast<std::uint8_t>((clearValue >> 16u) & 0xffu),
            static_cast<std::uint8_t>((clearValue >> 24u) & 0xffu) }};
        glClearTexSubImage(state.atlas, 0,
            static_cast<GLint>(origin[0]), static_cast<GLint>(origin[1]), 0,
            static_cast<GLsizei>(kTileResolution), static_cast<GLsizei>(kTileResolution), 1,
            GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    }
    else
    {
        glClearTexSubImage(state.atlas, 0,
            static_cast<GLint>(origin[0]), static_cast<GLint>(origin[1]), 0,
            static_cast<GLsizei>(kTileResolution), static_cast<GLsizei>(kTileResolution), 1,
            state.clearFormat, state.clearType, &clearValue);
    }
}


namespace {

inline float liveTrack22Smooth01(float value)
{
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

inline std::int32_t liveTrack22WorldTile(double coordinate, float tileWorldSizeM)
{
    return static_cast<std::int32_t>(std::floor(coordinate / static_cast<double>(tileWorldSizeM)));
}

} // namespace

void DynamicSurfaceGpuRuntime::resetTireMarkAtlas()
{
    m_tireMarkSerialsByTile.clear();
    m_dirtyTireMarkTiles.clear();
    m_lastIndexedTireMarkSerial = 0u;
    m_lastTireMarkFirstSerial = 0u;
    m_lastTireMarkRetirementRefreshTime = -1.0;
    if (m_tireMarks.allocated)
    {
        for (const auto& [key, tile] : m_tiles)
        {
            (void)key;
            clearStateSlot(m_tireMarks, tile.slot, 0u);
        }
    }
}

void DynamicSurfaceGpuRuntime::indexNewTireMarkSegments(
    const heritage::physics::SurfacePresentation& presentation)
{
    const double presentationTime = presentation.elapsedSeconds();
    const std::uint64_t firstSerial = presentation.firstTireMarkSerial();
    const std::uint64_t lastSerial = presentation.lastTireMarkSerial();

    const bool restarted = m_lastTireMarkPresentationTime >= 0.0
        && presentationTime + 1.0e-6 < m_lastTireMarkPresentationTime;
    const bool serialRestarted = m_lastIndexedTireMarkSerial != 0u
        && lastSerial != 0u && lastSerial < m_lastIndexedTireMarkSerial;
    if (restarted || serialRestarted)
        resetTireMarkAtlas();

    // Age/retirement is presentation state, not baked into persistence. Refresh
    // only resident marked tiles every few seconds and let the per-frame raster
    // budget amortize the work; this preserves the twenty-minute smooth fade
    // without rescanning one million segments every rendered frame.
    if (m_lastTireMarkRetirementRefreshTime < 0.0
        || presentationTime - m_lastTireMarkRetirementRefreshTime >= 3.0
        || (firstSerial != 0u && firstSerial != m_lastTireMarkFirstSerial))
    {
        for (const auto& [key, tile] : m_tiles)
        {
            (void)tile;
            if (m_tireMarkSerialsByTile.find(key) != m_tireMarkSerialsByTile.end())
                m_dirtyTireMarkTiles.insert(key);
        }
        m_lastTireMarkRetirementRefreshTime = presentationTime;
    }

    if (lastSerial != 0u)
    {
        std::uint64_t nextSerial = m_lastIndexedTireMarkSerial == 0u
            ? firstSerial : m_lastIndexedTireMarkSerial + 1u;
        if (nextSerial < firstSerial)
            nextSerial = firstSerial;

        for (std::uint64_t serial = nextSerial; serial <= lastSerial; ++serial)
        {
            const auto* mark = presentation.tireMarkSegmentBySerial(serial);
            if (mark == nullptr)
                continue;
            const double expansion = 0.5 * static_cast<double>(std::max(
                mark->startWidthM, mark->endWidthM)) + 0.04;
            const double minimumX = std::min(
                mark->startGlobalPosition.x, mark->endGlobalPosition.x) - expansion;
            const double maximumX = std::max(
                mark->startGlobalPosition.x, mark->endGlobalPosition.x) + expansion;
            const double minimumZ = std::min(
                mark->startGlobalPosition.z, mark->endGlobalPosition.z) - expansion;
            const double maximumZ = std::max(
                mark->startGlobalPosition.z, mark->endGlobalPosition.z) + expansion;
            const std::int32_t minTileX = liveTrack22WorldTile(minimumX, kTileWorldSizeM);
            const std::int32_t maxTileX = liveTrack22WorldTile(maximumX, kTileWorldSizeM);
            const std::int32_t minTileZ = liveTrack22WorldTile(minimumZ, kTileWorldSizeM);
            const std::int32_t maxTileZ = liveTrack22WorldTile(maximumZ, kTileWorldSizeM);
            for (std::int32_t tileZ = minTileZ; tileZ <= maxTileZ; ++tileZ)
            {
                for (std::int32_t tileX = minTileX; tileX <= maxTileX; ++tileX)
                {
                    const std::uint64_t key = tileKey(tileX, tileZ);
                    m_tireMarkSerialsByTile[key].push_back(serial);
                    if (m_tiles.find(key) != m_tiles.end())
                        m_dirtyTireMarkTiles.insert(key);
                }
            }
        }
        m_lastIndexedTireMarkSerial = lastSerial;
    }

    m_lastTireMarkFirstSerial = firstSerial;
    m_lastTireMarkPresentationTime = presentationTime;
}

void DynamicSurfaceGpuRuntime::rasterizeTireMarkTile(
    const heritage::physics::SurfacePresentation& presentation,
    const TileRuntime& tile)
{
    if (!m_tireMarks.allocated)
        return;

    const std::size_t texelCount = static_cast<std::size_t>(kTileResolution)
        * static_cast<std::size_t>(kTileResolution);
    m_tireMarkRasterScratch.assign(texelCount, 0u);

    const std::uint64_t key = tileKey(tile.x, tile.z);
    auto listIt = m_tireMarkSerialsByTile.find(key);
    if (listIt != m_tireMarkSerialsByTile.end())
    {
        auto& serials = listIt->second;
        std::size_t validCount = 0u;
        const double tileOriginX = static_cast<double>(tile.x) * kTileWorldSizeM;
        const double tileOriginZ = static_cast<double>(tile.z) * kTileWorldSizeM;
        const double cellM = static_cast<double>(kCellSizeM);
        const double now = presentation.elapsedSeconds();

        for (std::uint64_t serial : serials)
        {
            const auto* mark = presentation.tireMarkSegmentBySerial(serial);
            if (mark == nullptr)
                continue;
            ++validCount;
            const double ageSeconds = std::max(now - mark->birthTimeSeconds, 0.0);
            if (ageSeconds >= heritage::physics::SurfacePresentation::kTireMarkRetirementSeconds)
                continue;
            const float ageT = static_cast<float>(ageSeconds
                / heritage::physics::SurfacePresentation::kTireMarkRetirementSeconds);
            const float ageOpacity = 1.0f - liveTrack22Smooth01(ageT);
            if (ageOpacity <= 0.0005f)
                continue;

            const double ax = mark->startGlobalPosition.x;
            const double az = mark->startGlobalPosition.z;
            const double bx = mark->endGlobalPosition.x;
            const double bz = mark->endGlobalPosition.z;
            const double dx = bx - ax;
            const double dz = bz - az;
            const double lengthSquared = dx * dx + dz * dz;
            const double halfWidthMax = 0.5 * static_cast<double>(std::max(
                mark->startWidthM, mark->endWidthM));
            const double padding = halfWidthMax + cellM * 2.0;
            const int minX = std::clamp(static_cast<int>(std::floor(
                (std::min(ax, bx) - padding - tileOriginX) / cellM)),
                0, static_cast<int>(kTileResolution) - 1);
            const int maxX = std::clamp(static_cast<int>(std::floor(
                (std::max(ax, bx) + padding - tileOriginX) / cellM)),
                0, static_cast<int>(kTileResolution) - 1);
            const int minZ = std::clamp(static_cast<int>(std::floor(
                (std::min(az, bz) - padding - tileOriginZ) / cellM)),
                0, static_cast<int>(kTileResolution) - 1);
            const int maxZ = std::clamp(static_cast<int>(std::floor(
                (std::max(az, bz) + padding - tileOriginZ) / cellM)),
                0, static_cast<int>(kTileResolution) - 1);

            for (int z = minZ; z <= maxZ; ++z)
            {
                const double pz = tileOriginZ + (static_cast<double>(z) + 0.5) * cellM;
                for (int x = minX; x <= maxX; ++x)
                {
                    const double px = tileOriginX + (static_cast<double>(x) + 0.5) * cellM;
                    double t = 0.0;
                    if (lengthSquared > 1.0e-12)
                        t = std::clamp(((px - ax) * dx + (pz - az) * dz) / lengthSquared, 0.0, 1.0);
                    const double cx = ax + dx * t;
                    const double cz = az + dz * t;
                    const double distance = std::hypot(px - cx, pz - cz);
                    const float width = mark->startWidthM
                        + (mark->endWidthM - mark->startWidthM) * static_cast<float>(t);
                    const float halfWidth = std::max(width * 0.5f, 0.02f);
                    const float feather = static_cast<float>(cellM * 1.35);
                    const float coverage = 1.0f - liveTrack22Smooth01(
                        (static_cast<float>(distance) - (halfWidth - feather))
                            / std::max(feather * 2.0f, 0.0001f));
                    if (coverage <= 0.0001f)
                        continue;
                    const float intensity = mark->startIntensity
                        + (mark->endIntensity - mark->startIntensity) * static_cast<float>(t);
                    const float deposit = std::clamp(
                        intensity * coverage * ageOpacity * 0.90f, 0.0f, 1.0f);
                    const std::size_t index = static_cast<std::size_t>(z)
                        * kTileResolution + static_cast<std::size_t>(x);
                    const float oldValue = static_cast<float>(m_tireMarkRasterScratch[index]) / 255.0f;
                    const float combined = 1.0f - (1.0f - oldValue) * (1.0f - deposit);
                    m_tireMarkRasterScratch[index] = static_cast<std::uint8_t>(
                        std::lround(std::clamp(combined, 0.0f, 1.0f) * 255.0f));
                }
            }
        }

        // The deque's first serial advances as old history retires. Keep tile
        // indexes bounded by compacting stale serials opportunistically when a
        // tile is already being rebuilt.
        if (validCount * 2u < serials.size() && serials.size() > 32u)
        {
            std::vector<std::uint64_t> compact;
            compact.reserve(validCount);
            for (std::uint64_t serial : serials)
            {
                if (presentation.tireMarkSegmentBySerial(serial) != nullptr)
                    compact.push_back(serial);
            }
            serials.swap(compact);
        }
    }

    const auto origin = atlasSlotOrigin(tile.slot);
    glTextureSubImage2D(m_tireMarks.atlas, 0,
        static_cast<GLint>(origin[0]), static_cast<GLint>(origin[1]),
        static_cast<GLsizei>(kTileResolution), static_cast<GLsizei>(kTileResolution),
        GL_RED, GL_UNSIGNED_BYTE, m_tireMarkRasterScratch.data());
}

void DynamicSurfaceGpuRuntime::syncTireMarkPresentation(
    const heritage::physics::SurfacePresentation& presentation)
{
    if (!m_stats.ready || !m_tireMarks.allocated)
        return;

    indexNewTireMarkSegments(presentation);

    // Keep the frame bounded. Newly marked tiles are only a handful in normal
    // driving; retirement/re-entry rebuilds are amortized rather than creating
    // a large one-frame CPU/GL upload spike.
    constexpr std::size_t kRasterBudgetTilesPerFrame = 12u;
    std::size_t rebuilt = 0u;
    for (auto it = m_dirtyTireMarkTiles.begin();
         it != m_dirtyTireMarkTiles.end() && rebuilt < kRasterBudgetTilesPerFrame;)
    {
        auto tileIt = m_tiles.find(*it);
        if (tileIt == m_tiles.end())
        {
            it = m_dirtyTireMarkTiles.erase(it);
            continue;
        }
        rasterizeTireMarkTile(presentation, tileIt->second);
        it = m_dirtyTireMarkTiles.erase(it);
        ++rebuilt;
    }
    m_stats.tireMarksReady = true;
}

} // namespace heritage::graphics::dynamicsurface

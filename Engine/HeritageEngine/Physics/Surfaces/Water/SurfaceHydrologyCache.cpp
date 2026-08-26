#include "SurfaceHydrology.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <system_error>
#include <vector>

namespace heritage::physics::water {
namespace {

constexpr std::uint32_t kCacheVersion = 15;
constexpr char kCacheMagic[8] = { 'H', 'E', 'R', 'I', 'H', 'Y', 'D', '1' };

struct CacheHeader
{
    char magic[8]{};
    std::uint32_t version = 0;
    std::uint32_t reserved = 0;
    std::uint64_t fingerprint = 0;
    double cellSizeM = 0.0;
    double verticalLayerSizeM = 0.0;
    std::uint64_t cellCount = 0;
    std::uint64_t prebakedTriangleCount = 0;
    std::uint64_t prebakedTriangleTileSpanCount = 0;
    std::uint64_t prebakedTriangleTileIndexCount = 0;
    std::uint64_t prebakedFarTileCount = 0;
    std::uint64_t prebakedFarPayloadBytes = 0;
};

struct CacheCell
{
    std::int64_t x = 0;
    std::int64_t z = 0;
    std::int64_t layer = 0;
    double elevationM = 0.0;
    float normalX = 0.0f;
    float normalY = 1.0f;
    float normalZ = 0.0f;
    std::uint32_t material = 0;
    double infiltrationMmPerHour = 0.0;
    double drainageMmPerHour = 0.0;
    double roughness = 0.0;
    double depressionStorageM = 0.0;
    double prebakedSpillElevationM = 0.0;
    float prebakedFlowX = 0.0f;
    float prebakedFlowZ = 0.0f;
};

struct CachePrebakedTriangle
{
    double ax = 0.0, ay = 0.0, az = 0.0;
    double bx = 0.0, by = 0.0, bz = 0.0;
    double cx = 0.0, cy = 0.0, cz = 0.0;
    float normalX = 0.0f, normalY = 1.0f, normalZ = 0.0f;
    double depressionStorageM = 0.0001;
    double spillElevationA = 0.0;
    double spillElevationB = 0.0;
    double spillElevationC = 0.0;
    float flowAX = 0.0f, flowAZ = 0.0f;
    float flowBX = 0.0f, flowBZ = 0.0f;
    float flowCX = 0.0f, flowCZ = 0.0f;
    float runoffAccumulationAM2 = 0.0f;
    float runoffAccumulationBM2 = 0.0f;
    float runoffAccumulationCM2 = 0.0f;
};

struct CachePrebakedTriangleTileSpan
{
    std::uint64_t key = 0;
    std::uint64_t firstIndex = 0;
    std::uint32_t count = 0;
    std::uint32_t reserved = 0;
};

struct CachePrebakedFarTile
{
    std::uint64_t key = 0;
    std::uint64_t payloadOffset = 0;
    std::uint32_t payloadBytes = 0;
    std::uint8_t encoding = 0;
    std::uint8_t reserved[3]{};
};

} // namespace

bool SurfaceHydrology::loadCache(
    const std::filesystem::path& path,
    std::uint64_t expectedFingerprint,
    SurfaceHydrologyBakeReport& report)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;
    CacheHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file || std::memcmp(header.magic, kCacheMagic, sizeof(kCacheMagic)) != 0
        || header.version != kCacheVersion
        || header.fingerprint != expectedFingerprint
        || header.cellSizeM != m_description.cellSizeM
        || header.verticalLayerSizeM != m_description.verticalLayerSizeM
        || header.cellCount == 0
        || header.cellCount > m_description.maximumCellCount
        || header.prebakedTriangleCount == 0
        || header.prebakedTriangleCount > 100000000ull
        || header.prebakedTriangleTileSpanCount == 0
        || header.prebakedTriangleTileSpanCount > 20000000ull
        || header.prebakedTriangleTileIndexCount == 0
        || header.prebakedTriangleTileIndexCount > 400000000ull
        || header.prebakedFarTileCount == 0
        || header.prebakedFarTileCount > 20000000ull
        || header.prebakedFarPayloadBytes == 0
        || header.prebakedFarPayloadBytes > (16ull * 1024ull * 1024ull * 1024ull))
    {
        return false;
    }

    std::vector<Cell> loaded;
    loaded.reserve(static_cast<std::size_t>(header.cellCount));
    for (std::uint64_t i = 0; i < header.cellCount; ++i)
    {
        CacheCell saved;
        file.read(reinterpret_cast<char*>(&saved), sizeof(saved));
        if (!file)
            return false;
        Cell cell;
        cell.key = { saved.x, saved.z, saved.layer };
        cell.elevationM = saved.elevationM;
        cell.normal = { saved.normalX, saved.normalY, saved.normalZ };
        cell.material = static_cast<SurfaceMaterial>(saved.material);
        cell.infiltrationMmPerHour = saved.infiltrationMmPerHour;
        cell.drainageMmPerHour = saved.drainageMmPerHour;
        cell.roughness = saved.roughness;
        cell.depressionStorageM = saved.depressionStorageM;
        cell.prebakedSpillElevationM = saved.prebakedSpillElevationM;
        cell.prebakedFlowX = saved.prebakedFlowX;
        cell.prebakedFlowZ = saved.prebakedFlowZ;
        loaded.push_back(cell);
    }
    std::vector<PrebakedTriangle> loadedPrebakedTriangles;
    loadedPrebakedTriangles.reserve(
        static_cast<std::size_t>(header.prebakedTriangleCount));
    for (std::uint64_t i = 0; i < header.prebakedTriangleCount; ++i)
    {
        CachePrebakedTriangle saved{};
        file.read(reinterpret_cast<char*>(&saved), sizeof(saved));
        if (!file)
            return false;
        PrebakedTriangle triangle;
        triangle.a = { saved.ax, saved.ay, saved.az };
        triangle.b = { saved.bx, saved.by, saved.bz };
        triangle.c = { saved.cx, saved.cy, saved.cz };
        triangle.normal = { saved.normalX, saved.normalY, saved.normalZ };
        triangle.depressionStorageM = saved.depressionStorageM;
        triangle.spillElevationA = saved.spillElevationA;
        triangle.spillElevationB = saved.spillElevationB;
        triangle.spillElevationC = saved.spillElevationC;
        triangle.flowAX = saved.flowAX;
        triangle.flowAZ = saved.flowAZ;
        triangle.flowBX = saved.flowBX;
        triangle.flowBZ = saved.flowBZ;
        triangle.flowCX = saved.flowCX;
        triangle.flowCZ = saved.flowCZ;
        triangle.runoffAccumulationAM2 = saved.runoffAccumulationAM2;
        triangle.runoffAccumulationBM2 = saved.runoffAccumulationBM2;
        triangle.runoffAccumulationCM2 = saved.runoffAccumulationCM2;
        loadedPrebakedTriangles.push_back(triangle);
    }

    std::vector<PrebakedTriangleTileSpan> loadedTileSpans;
    loadedTileSpans.reserve(static_cast<std::size_t>(header.prebakedTriangleTileSpanCount));
    for (std::uint64_t i = 0; i < header.prebakedTriangleTileSpanCount; ++i)
    {
        CachePrebakedTriangleTileSpan saved{};
        file.read(reinterpret_cast<char*>(&saved), sizeof(saved));
        if (!file)
            return false;
        if (saved.firstIndex > header.prebakedTriangleTileIndexCount
            || saved.count > header.prebakedTriangleTileIndexCount - saved.firstIndex)
        {
            return false;
        }
        loadedTileSpans.push_back({ saved.key, saved.firstIndex, saved.count, 0u });
    }
    if (!std::is_sorted(loadedTileSpans.begin(), loadedTileSpans.end(),
            [](const PrebakedTriangleTileSpan& a, const PrebakedTriangleTileSpan& b) {
                return a.key < b.key;
            }))
    {
        return false;
    }
    std::vector<std::int32_t> loadedTileIndices(
        static_cast<std::size_t>(header.prebakedTriangleTileIndexCount));
    file.read(reinterpret_cast<char*>(loadedTileIndices.data()),
        static_cast<std::streamsize>(loadedTileIndices.size() * sizeof(std::int32_t)));
    if (!file)
        return false;
    for (const std::int32_t index : loadedTileIndices)
    {
        if (index < 0 || static_cast<std::uint64_t>(index) >= header.prebakedTriangleCount)
            return false;
    }

    std::vector<PrebakedFarTileIndex> loadedFarTiles;
    loadedFarTiles.reserve(static_cast<std::size_t>(header.prebakedFarTileCount));
    for (std::uint64_t i = 0; i < header.prebakedFarTileCount; ++i)
    {
        CachePrebakedFarTile saved{};
        file.read(reinterpret_cast<char*>(&saved), sizeof(saved));
        if (!file)
            return false;
        PrebakedFarTileIndex tile;
        tile.key = saved.key;
        tile.payloadOffset = saved.payloadOffset;
        tile.payloadBytes = saved.payloadBytes;
        tile.encoding = saved.encoding;
        if (tile.payloadOffset > header.prebakedFarPayloadBytes
            || tile.payloadBytes > header.prebakedFarPayloadBytes - tile.payloadOffset
            || tile.encoding > 2u)
        {
            return false;
        }
        loadedFarTiles.push_back(tile);
    }
    if (!std::is_sorted(loadedFarTiles.begin(), loadedFarTiles.end(),
            [](const PrebakedFarTileIndex& a, const PrebakedFarTileIndex& b) {
                return a.key < b.key;
            }))
    {
        return false;
    }
    std::vector<std::uint8_t> loadedFarPayload(
        static_cast<std::size_t>(header.prebakedFarPayloadBytes));
    file.read(reinterpret_cast<char*>(loadedFarPayload.data()),
        static_cast<std::streamsize>(loadedFarPayload.size()));
    if (!file)
        return false;

    m_cells = std::move(loaded);
    m_prebakedTriangles = std::move(loadedPrebakedTriangles);
    m_prebakedTriangleTileSpans = std::move(loadedTileSpans);
    m_prebakedTriangleTileIndices = std::move(loadedTileIndices);
    m_prebakedFarTiles = std::move(loadedFarTiles);
    m_prebakedFarPayload = std::move(loadedFarPayload);
    rebuildLookupAndConnectivity();
    m_stats.available = true;
    m_stats.loadedFromCache = true;
    m_stats.supportCellCount = m_cells.size();
    m_stats.cellCount = m_cells.size();
    m_stats.connectedCellCount = 0u;
    m_stats.debugVisualizationEnabled = m_debugVisualizationEnabled;
    m_stats.prebakedWorldTileCount = m_prebakedFarTiles.size();
    m_stats.prebakedFarPayloadBytes = static_cast<std::uint64_t>(m_prebakedFarPayload.size());
    report = {};
    report.valid = true;
    report.loadedFromCache = true;
    report.cellCount = m_cells.size();
    report.connectedCellCount = 0u;
    report.sourceFingerprint = expectedFingerprint;
    report.cachePath = path;
    report.message = "Hydrology topology loaded from cache.";
    return true;
}

bool SurfaceHydrology::writeCache(
    const std::filesystem::path& path,
    std::uint64_t sourceFingerprint) const
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error)
        return false;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
        return false;
    CacheHeader header;
    std::memcpy(header.magic, kCacheMagic, sizeof(kCacheMagic));
    header.version = kCacheVersion;
    header.fingerprint = sourceFingerprint;
    header.cellSizeM = m_description.cellSizeM;
    header.verticalLayerSizeM = m_description.verticalLayerSizeM;
    header.cellCount = static_cast<std::uint64_t>(m_cells.size());
    header.prebakedTriangleCount = static_cast<std::uint64_t>(
        m_prebakedTriangles.size());
    header.prebakedTriangleTileSpanCount = static_cast<std::uint64_t>(
        m_prebakedTriangleTileSpans.size());
    header.prebakedTriangleTileIndexCount = static_cast<std::uint64_t>(
        m_prebakedTriangleTileIndices.size());
    header.prebakedFarTileCount = static_cast<std::uint64_t>(m_prebakedFarTiles.size());
    header.prebakedFarPayloadBytes = static_cast<std::uint64_t>(m_prebakedFarPayload.size());
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    for (const Cell& cell : m_cells)
    {
        CacheCell saved;
        saved.x = cell.key.x;
        saved.z = cell.key.z;
        saved.layer = cell.key.layer;
        saved.elevationM = cell.elevationM;
        saved.normalX = cell.normal.x;
        saved.normalY = cell.normal.y;
        saved.normalZ = cell.normal.z;
        saved.material = static_cast<std::uint32_t>(cell.material);
        saved.infiltrationMmPerHour = cell.infiltrationMmPerHour;
        saved.drainageMmPerHour = cell.drainageMmPerHour;
        saved.roughness = cell.roughness;
        saved.depressionStorageM = cell.depressionStorageM;
        saved.prebakedSpillElevationM = cell.prebakedSpillElevationM;
        saved.prebakedFlowX = cell.prebakedFlowX;
        saved.prebakedFlowZ = cell.prebakedFlowZ;
        file.write(reinterpret_cast<const char*>(&saved), sizeof(saved));
    }
    for (const PrebakedTriangle& triangle : m_prebakedTriangles)
    {
        CachePrebakedTriangle saved{};
        saved.ax = triangle.a.x; saved.ay = triangle.a.y; saved.az = triangle.a.z;
        saved.bx = triangle.b.x; saved.by = triangle.b.y; saved.bz = triangle.b.z;
        saved.cx = triangle.c.x; saved.cy = triangle.c.y; saved.cz = triangle.c.z;
        saved.normalX = triangle.normal.x;
        saved.normalY = triangle.normal.y;
        saved.normalZ = triangle.normal.z;
        saved.depressionStorageM = triangle.depressionStorageM;
        saved.spillElevationA = triangle.spillElevationA;
        saved.spillElevationB = triangle.spillElevationB;
        saved.spillElevationC = triangle.spillElevationC;
        saved.flowAX = triangle.flowAX;
        saved.flowAZ = triangle.flowAZ;
        saved.flowBX = triangle.flowBX;
        saved.flowBZ = triangle.flowBZ;
        saved.flowCX = triangle.flowCX;
        saved.flowCZ = triangle.flowCZ;
        saved.runoffAccumulationAM2 = triangle.runoffAccumulationAM2;
        saved.runoffAccumulationBM2 = triangle.runoffAccumulationBM2;
        saved.runoffAccumulationCM2 = triangle.runoffAccumulationCM2;
        file.write(reinterpret_cast<const char*>(&saved), sizeof(saved));
    }

    for (const PrebakedTriangleTileSpan& span : m_prebakedTriangleTileSpans)
    {
        CachePrebakedTriangleTileSpan saved{};
        saved.key = span.key;
        saved.firstIndex = span.firstIndex;
        saved.count = span.count;
        file.write(reinterpret_cast<const char*>(&saved), sizeof(saved));
    }
    if (!m_prebakedTriangleTileIndices.empty())
    {
        file.write(reinterpret_cast<const char*>(m_prebakedTriangleTileIndices.data()),
            static_cast<std::streamsize>(m_prebakedTriangleTileIndices.size()
                * sizeof(std::int32_t)));
    }

    for (const PrebakedFarTileIndex& tile : m_prebakedFarTiles)
    {
        CachePrebakedFarTile saved{};
        saved.key = tile.key;
        saved.payloadOffset = tile.payloadOffset;
        saved.payloadBytes = tile.payloadBytes;
        saved.encoding = tile.encoding;
        file.write(reinterpret_cast<const char*>(&saved), sizeof(saved));
    }
    if (!m_prebakedFarPayload.empty())
    {
        file.write(reinterpret_cast<const char*>(m_prebakedFarPayload.data()),
            static_cast<std::streamsize>(m_prebakedFarPayload.size()));
    }
    return static_cast<bool>(file);
}

} // namespace heritage::physics::water

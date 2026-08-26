#include "SurfaceHydrology.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace heritage::physics::water {
namespace {

std::uint64_t hashBytes(std::uint64_t hash, const void* bytes, std::size_t size)
{
    const auto* data = static_cast<const std::uint8_t*>(bytes);
    for (std::size_t i = 0; i < size; ++i)
    {
        hash ^= static_cast<std::uint64_t>(data[i]);
        hash *= 1099511628211ull;
    }
    return hash;
}

template <typename T>
std::uint64_t hashValue(std::uint64_t hash, const T& value)
{
    return hashBytes(hash, &value, sizeof(T));
}

bool finitePosition(const heritage::math::DVec3& value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

} // namespace

bool validSurfaceHydrologyDescription(const SurfaceHydrologyDescription& d)
{
    return std::isfinite(d.cellSizeM)
        && d.cellSizeM >= 0.10 && d.cellSizeM <= 5.0
        && std::isfinite(d.verticalLayerSizeM)
        && d.verticalLayerSizeM >= 0.25 && d.verticalLayerSizeM <= 100.0
        && std::isfinite(d.minimumUpwardNormal)
        && d.minimumUpwardNormal >= 0.0 && d.minimumUpwardNormal <= 0.99
        && d.maximumCellCount >= 16u && d.maximumCellCount <= 10000000u;
}

std::size_t SurfaceHydrology::CellKeyHash::operator()(const CellKey& key) const
{
    std::uint64_t hash = 1469598103934665603ull;
    hash = hashValue(hash, key.x);
    hash = hashValue(hash, key.z);
    hash = hashValue(hash, key.layer);
    return static_cast<std::size_t>(hash);
}

SurfaceHydrology::SurfaceHydrology(const SurfaceHydrologyDescription& description)
{
    if (validSurfaceHydrologyDescription(description))
        m_description = description;
}

void SurfaceHydrology::clear()
{
    m_cells.clear();
    m_lookup.clear();
    m_topSupportLookup.clear();
    m_prebakedTriangles.clear();
    m_prebakedTriangleTileSpans.clear();
    m_prebakedTriangleTileIndices.clear();
    m_prebakedFarTiles.clear();
    m_prebakedFarPayload.clear();
    m_stats = {};
    m_lastBakeReport = {};
    m_debugVisualizationEnabled = false;
}

bool SurfaceHydrology::setDescription(const SurfaceHydrologyDescription& description)
{
    if (!validSurfaceHydrologyDescription(description) || !m_cells.empty())
        return false;
    m_description = description;
    return true;
}

std::uint64_t SurfaceHydrology::fingerprint(
    const std::vector<StaticSceneTriangle>& triangles,
    const heritage::math::DVec3& globalOrigin) const
{
    std::uint64_t hash = 1469598103934665603ull;
    hash = hashValue(hash, m_description.cellSizeM);
    hash = hashValue(hash, m_description.verticalLayerSizeM);
    hash = hashValue(hash, m_description.minimumUpwardNormal);
    hash = hashValue(hash, globalOrigin.x);
    hash = hashValue(hash, globalOrigin.y);
    hash = hashValue(hash, globalOrigin.z);
    for (const StaticSceneTriangle& triangle : triangles)
    {
        hash = hashValue(hash, triangle.a);
        hash = hashValue(hash, triangle.b);
        hash = hashValue(hash, triangle.c);
        const auto material = static_cast<std::uint32_t>(triangle.surfaceMaterial);
        hash = hashValue(hash, material);
        const auto& h = triangle.surfaceProperties.hydrology;
        hash = hashValue(hash, h.infiltrationCapacityMmPerHour);
        hash = hashValue(hash, h.drainageCapacityMmPerHour);
        hash = hashValue(hash, h.flowRoughness);
        hash = hashValue(hash, h.depressionStorageMm);
    }
    return hash;
}

bool SurfaceHydrology::loadOrBake(
    const std::vector<StaticSceneTriangle>& triangles,
    const heritage::math::DVec3& globalOrigin,
    const std::filesystem::path& cachePath,
    SurfaceHydrologyBakeReport& report)
{
    const auto started = std::chrono::steady_clock::now();
    const std::uint64_t sourceFingerprint = fingerprint(triangles, globalOrigin);
    if (!cachePath.empty() && loadCache(cachePath, sourceFingerprint, report))
    {
        report.sourceTriangleCount = triangles.size();
        m_stats.sourceTriangleCount = triangles.size();
        report.elapsedMilliseconds = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        m_lastBakeReport = report;
        return true;
    }

    if (!bake(triangles, globalOrigin, report))
        return false;

    report.sourceFingerprint = sourceFingerprint;
    report.cachePath = cachePath;
    if (!cachePath.empty())
    {
        if (writeCache(cachePath, sourceFingerprint))
            report.message += " Cache written.";
        else
            report.message += " Cache write failed; runtime bake remains valid.";
    }
    report.elapsedMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    m_lastBakeReport = report;
    return true;
}

bool SurfaceHydrology::bake(
    const std::vector<StaticSceneTriangle>& triangles,
    const heritage::math::DVec3& globalOrigin,
    SurfaceHydrologyBakeReport& report)
{
    const auto started = std::chrono::steady_clock::now();
    clear();
    report = {};
    report.sourceTriangleCount = triangles.size();

    if (triangles.empty() || !finitePosition(globalOrigin))
    {
        report.message = "Hydrology bake requires finite static collision triangles.";
        m_lastBakeReport = report;
        return false;
    }

    std::unordered_map<CellKey, Cell, CellKeyHash> drafts;
    drafts.reserve(std::min<std::size_t>(
        triangles.size() * 2u, m_description.maximumCellCount));
    const double cellSize = m_description.cellSizeM;
    bool capped = false;

    const auto insertPoint = [&](const StaticSceneTriangle& triangle,
                                 double x, double y, double z,
                                 std::unordered_map<CellKey, Cell, CellKeyHash>& target) {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
            return;
        CellKey key;
        key.x = static_cast<std::int64_t>(std::floor(x / cellSize));
        key.z = static_cast<std::int64_t>(std::floor(z / cellSize));
        key.layer = static_cast<std::int64_t>(
            std::floor(y / m_description.verticalLayerSizeM));
        const auto found = target.find(key);
        if (found != target.end() && found->second.elevationM >= y - 0.005)
            return;
        if (found == target.end() && target.size() >= m_description.maximumCellCount)
            return;

        Cell cell;
        cell.key = key;
        cell.elevationM = y;
        cell.normal = triangle.normal;
        cell.material = triangle.surfaceMaterial;
        const auto& h = triangle.surfaceProperties.hydrology;
        cell.infiltrationMmPerHour = h.infiltrationCapacityMmPerHour;
        cell.drainageMmPerHour = h.drainageCapacityMmPerHour;
        cell.roughness = h.flowRoughness;
        cell.depressionStorageM = h.depressionStorageMm / 1000.0;
        target[key] = cell;
    };

    for (const StaticSceneTriangle& triangle : triangles)
    {
        if (drafts.size() >= m_description.maximumCellCount)
        {
            capped = true;
            break;
        }
        if (!std::isfinite(triangle.normal.y)
            || triangle.normal.y < m_description.minimumUpwardNormal)
        {
            continue;
        }

        const double ax = globalOrigin.x + triangle.a.x;
        const double ay = globalOrigin.y + triangle.a.y;
        const double az = globalOrigin.z + triangle.a.z;
        const double bx = globalOrigin.x + triangle.b.x;
        const double by = globalOrigin.y + triangle.b.y;
        const double bz = globalOrigin.z + triangle.b.z;
        const double cx = globalOrigin.x + triangle.c.x;
        const double cy = globalOrigin.y + triangle.c.y;
        const double cz = globalOrigin.z + triangle.c.z;
        const double denominator = (bz - cz) * (ax - cx)
            + (cx - bx) * (az - cz);
        if (!std::isfinite(denominator) || std::abs(denominator) <= 1.0e-12)
            continue;

        ++report.acceptedTriangleCount;
        const std::int64_t minimumX = static_cast<std::int64_t>(
            std::floor(std::min({ ax, bx, cx }) / cellSize));
        const std::int64_t maximumX = static_cast<std::int64_t>(
            std::floor(std::max({ ax, bx, cx }) / cellSize));
        const std::int64_t minimumZ = static_cast<std::int64_t>(
            std::floor(std::min({ az, bz, cz }) / cellSize));
        const std::int64_t maximumZ = static_cast<std::int64_t>(
            std::floor(std::max({ az, bz, cz }) / cellSize));
        bool rasterized = false;
        for (std::int64_t gridZ = minimumZ; gridZ <= maximumZ; ++gridZ)
        {
            const double z = (static_cast<double>(gridZ) + 0.5) * cellSize;
            for (std::int64_t gridX = minimumX; gridX <= maximumX; ++gridX)
            {
                const double x = (static_cast<double>(gridX) + 0.5) * cellSize;
                const double wa = ((bz - cz) * (x - cx)
                    + (cx - bx) * (z - cz)) / denominator;
                const double wb = ((cz - az) * (x - cx)
                    + (ax - cx) * (z - cz)) / denominator;
                const double wc = 1.0 - wa - wb;
                constexpr double epsilon = -1.0e-8;
                if (wa < epsilon || wb < epsilon || wc < epsilon)
                    continue;
                insertPoint(triangle, x, wa * ay + wb * by + wc * cy, z, drafts);
                rasterized = true;
                if (drafts.size() >= m_description.maximumCellCount)
                {
                    capped = true;
                    break;
                }
            }
            if (capped)
                break;
        }
        if (!rasterized && !capped)
        {
            insertPoint(triangle,
                (ax + bx + cx) / 3.0,
                (ay + by + cy) / 3.0,
                (az + bz + cz) / 3.0,
                drafts);
        }
        if (capped)
            break;
    }

    m_cells.reserve(drafts.size());
    for (auto& entry : drafts)
        m_cells.push_back(std::move(entry.second));
    std::sort(m_cells.begin(), m_cells.end(),
        [](const Cell& left, const Cell& right) { return left.key < right.key; });
    rebuildLookupAndConnectivity();
    // OPT02: production water topology is derived from the complete authored
    // triangle mesh. The bounded support raster above is compatibility/shelter
    // metadata only; no adaptive CPU water solver is constructed.
    rebuildPrebakedTriangleTopology(triangles, globalOrigin);
    rebuildPrebakedFarTileCache();

    report.valid = !m_prebakedTriangles.empty() && !m_prebakedFarTiles.empty();
    report.loadedFromCache = false;
    report.cellCount = m_cells.size();
    report.connectedCellCount = 0u;
    report.sourceFingerprint = fingerprint(triangles, globalOrigin);
    report.elapsedMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    report.message = report.valid
        ? (capped
            ? "World hydrology baked completely; legacy CPU support fallback reached its bounded safety limit."
            : "World hydrology baked from static collision geometry.")
        : "No upward-facing collision area was suitable for hydrology.";
    m_stats.available = report.valid;
    m_stats.loadedFromCache = false;
    m_stats.sourceTriangleCount = triangles.size();
    m_stats.supportCellCount = m_cells.size();
    m_stats.cellCount = m_cells.size();
    m_stats.connectedCellCount = 0u;
    m_stats.debugVisualizationEnabled = m_debugVisualizationEnabled;
    m_lastBakeReport = report;
    return report.valid;
}


void SurfaceHydrology::rebuildLookupAndConnectivity()
{
    m_lookup.clear();
    m_lookup.reserve(m_cells.size());
    m_topSupportLookup.clear();
    m_topSupportLookup.reserve(m_cells.size());

    std::unordered_map<CellKey, double, CellKeyHash> highestSurfaceByColumn;
    highestSurfaceByColumn.reserve(m_cells.size());
    for (std::size_t i = 0; i < m_cells.size(); ++i)
    {
        m_lookup[m_cells[i].key] = static_cast<std::int32_t>(i);
        const CellKey columnKey{ m_cells[i].key.x, m_cells[i].key.z, 0 };
        const auto found = highestSurfaceByColumn.find(columnKey);
        if (found == highestSurfaceByColumn.end())
        {
            highestSurfaceByColumn.emplace(columnKey, m_cells[i].elevationM);
            m_topSupportLookup[columnKey] = static_cast<std::int32_t>(i);
        }
        else
        {
            if (m_cells[i].elevationM > found->second)
                m_topSupportLookup[columnKey] = static_cast<std::int32_t>(i);
            found->second = std::max(found->second, m_cells[i].elevationM);
        }
    }

    constexpr double kSkyExposureHeightToleranceM = 0.05;
    for (Cell& cell : m_cells)
    {
        const CellKey columnKey{ cell.key.x, cell.key.z, 0 };
        const auto highest = highestSurfaceByColumn.find(columnKey);
        cell.precipitationExposed = highest == highestSurfaceByColumn.end()
            || cell.elevationM >= highest->second - kSkyExposureHeightToleranceM;
    }

    m_stats.supportCellCount = m_cells.size();
    m_stats.cellCount = m_cells.size();
    m_stats.connectedCellCount = 0u;
    m_stats.debugVisualizationEnabled = m_debugVisualizationEnabled;
}

} // namespace heritage::physics::water

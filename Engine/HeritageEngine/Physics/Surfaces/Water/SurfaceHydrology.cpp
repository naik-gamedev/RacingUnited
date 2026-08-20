#include "SurfaceHydrology.hpp"
#include "VirtualPipeFlow.hpp"

#include "../../../Core/Jobs/JobSystem.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <fstream>
#include <functional>
#include <map>
#include <numeric>
#include <tuple>
#include <limits>
#include <system_error>
#include <unordered_set>

namespace heritage::physics::water {
namespace {

constexpr double kSecondsPerHour = 3600.0;
constexpr double kMillimetresPerMetre = 1000.0;
constexpr double kGravityMps2 = 9.80665;
constexpr std::uint32_t kCacheVersion = 1;
constexpr char kCacheMagic[8] = { 'H', 'E', 'R', 'I', 'H', 'Y', 'D', '1' };
constexpr double kAdaptiveUnitM = 0.05;
constexpr double kAdaptiveSpatialBucketM = 20.0;
constexpr std::int64_t kAdaptiveSpatialBucketUnits = 400;
constexpr std::size_t kHydrologyCellGrain = 2048u;
constexpr std::size_t kHydrologyFlowGrain = 512u;


std::size_t rangeCountFor(std::size_t itemCount, std::size_t grain)
{
    return itemCount == 0u ? 0u : (itemCount + grain - 1u) / grain;
}

std::int64_t floorDivide(std::int64_t value, std::int64_t divisor)
{
    std::int64_t quotient = value / divisor;
    const std::int64_t remainder = value % divisor;
    if (remainder < 0)
        --quotient;
    return quotient;
}

double mmPerHourToMps(double value)
{
    return value / (kMillimetresPerMetre * kSecondsPerHour);
}

double smoothStep(double edge0, double edge1, double value)
{
    const double t = std::clamp((value - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

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

struct CacheHeader
{
    char magic[8]{};
    std::uint32_t version = 0;
    std::uint32_t reserved = 0;
    std::uint64_t fingerprint = 0;
    double cellSizeM = 0.0;
    double verticalLayerSizeM = 0.0;
    std::uint64_t cellCount = 0;
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
};

} // namespace

bool validSurfaceHydrologyDescription(const SurfaceHydrologyDescription& d)
{
    return std::isfinite(d.cellSizeM) && d.cellSizeM >= 0.10 && d.cellSizeM <= 5.0
        && std::isfinite(d.adaptiveMinimumCellSizeM)
        && d.adaptiveMinimumCellSizeM >= 0.10
        && d.adaptiveMinimumCellSizeM <= d.cellSizeM
        && std::isfinite(d.adaptiveMaximumCellSizeM)
        && d.adaptiveMaximumCellSizeM >= d.cellSizeM
        && d.adaptiveMaximumCellSizeM <= 20.0
        && d.adaptiveMaximumCellSizeM >= d.adaptiveMinimumCellSizeM
        && std::isfinite(d.adaptiveSurfaceErrorM)
        && d.adaptiveSurfaceErrorM >= 0.001 && d.adaptiveSurfaceErrorM <= 0.50
        && std::isfinite(d.adaptiveNormalErrorDegrees)
        && d.adaptiveNormalErrorDegrees >= 0.25 && d.adaptiveNormalErrorDegrees <= 30.0
        && std::isfinite(d.adaptiveMinimumCellSlopeDegrees)
        && d.adaptiveMinimumCellSlopeDegrees >= 15.0
        && d.adaptiveMinimumCellSlopeDegrees <= 80.0
        && std::isfinite(d.adaptiveMinimumCellNormalBreakDegrees)
        && d.adaptiveMinimumCellNormalBreakDegrees >= 5.0
        && d.adaptiveMinimumCellNormalBreakDegrees <= 90.0
        && std::isfinite(d.verticalLayerSizeM)
        && d.verticalLayerSizeM >= 0.25 && d.verticalLayerSizeM <= 100.0
        && std::isfinite(d.updateRateHz)
        && d.updateRateHz >= 1.0 && d.updateRateHz <= 120.0
        && std::isfinite(d.nearCadenceRadiusM)
        && std::isfinite(d.mediumCadenceRadiusM)
        && std::isfinite(d.farCadenceRadiusM)
        && std::isfinite(d.distantCadenceRadiusM)
        && d.nearCadenceRadiusM >= 0.0
        && d.mediumCadenceRadiusM >= d.nearCadenceRadiusM
        && d.farCadenceRadiusM >= d.mediumCadenceRadiusM
        && d.distantCadenceRadiusM >= d.farCadenceRadiusM
        && d.distantCadenceRadiusM <= 100000.0
        && std::isfinite(d.nearCadenceHz)
        && std::isfinite(d.mediumCadenceHz)
        && std::isfinite(d.farCadenceHz)
        && std::isfinite(d.distantCadenceHz)
        && std::isfinite(d.backgroundCadenceHz)
        && d.nearCadenceHz >= 1.0 && d.nearCadenceHz <= d.updateRateHz
        && d.mediumCadenceHz >= 1.0 && d.mediumCadenceHz <= d.nearCadenceHz
        && d.farCadenceHz >= 1.0 && d.farCadenceHz <= d.mediumCadenceHz
        && d.distantCadenceHz >= 0.25 && d.distantCadenceHz <= d.farCadenceHz
        && d.backgroundCadenceHz >= 0.1
        && d.backgroundCadenceHz <= d.distantCadenceHz
        && std::isfinite(d.minimumUpwardNormal)
        && d.minimumUpwardNormal >= 0.0 && d.minimumUpwardNormal <= 0.99
        && std::isfinite(d.maximumNeighbourStepM)
        && d.maximumNeighbourStepM >= 0.05 && d.maximumNeighbourStepM <= 20.0
        && std::isfinite(d.maximumWaterDepthM)
        && d.maximumWaterDepthM >= 0.005 && d.maximumWaterDepthM <= 5.0
        && std::isfinite(d.flowCoefficient)
        && d.flowCoefficient >= 0.0 && d.flowCoefficient <= 10.0
        && d.maximumCellCount >= 16 && d.maximumCellCount <= 10000000;
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

void SurfaceHydrology::clearInterestSources()
{
    if (!m_interestSources.empty())
    {
        for (SimulationChunk& chunk : m_simulationChunks)
        {
            chunk.cadenceAccumulatorSeconds = 0.0;
            chunk.elapsedSinceUpdateSeconds = 0.0;
            chunk.cadenceInitialized = false;
            chunk.cadenceBand = CadenceBand::Near30Hz;
        }
    }
    m_interestSources.clear();
    m_stats.interestSourceCount = 0u;
}

void SurfaceHydrology::setInterestSource(const heritage::math::DVec3& source)
{
    if (!finitePosition(source))
    {
        clearInterestSources();
        return;
    }
    const bool enteringInterestMode = m_interestSources.empty();
    m_interestSources.assign(1u, source);
    if (enteringInterestMode)
    {
        for (SimulationChunk& chunk : m_simulationChunks)
            chunk.cadenceInitialized = false;
    }
    m_stats.interestSourceCount = 1u;
}

void SurfaceHydrology::setInterestSources(
    const std::vector<heritage::math::DVec3>& sources)
{
    const std::size_t previousCount = m_interestSources.size();
    m_interestSources.clear();
    m_interestSources.reserve((std::min)(sources.size(), std::size_t{ 16u }));
    for (const auto& source : sources)
    {
        if (finitePosition(source))
            m_interestSources.push_back(source);
        if (m_interestSources.size() >= 16u)
            break;
    }
    if (previousCount != m_interestSources.size())
    {
        for (SimulationChunk& chunk : m_simulationChunks)
            chunk.cadenceInitialized = false;
    }
    m_stats.interestSourceCount = m_interestSources.size();
}

void SurfaceHydrology::clear()
{
    m_cells.clear();
    m_lookup.clear();
    m_adaptiveCells.clear();
    m_adaptivePipes.clear();
    m_adaptiveSpatialBuckets.clear();
    m_presentationBasins.clear();
    m_presentationBasinConnections.clear();
    m_presentationBasinRefreshAccumulatorSeconds = 0.0;
    m_lastPresentationFilmDepthM = 0.0;
    m_simulationChunks.clear();
    m_dueCells.clear();
    m_dueDeltaTimeByCell.clear();
    m_outflowScaleByCell.clear();
    m_boundaryCandidateFluxByCell.clear();
    m_weatherReductionScratch.clear();
    m_flowRunoffReductionScratch.clear();
    m_overflowRunoffReductionScratch.clear();
    m_statsReductionScratch.clear();
    m_stats = {};
    m_interestSources.clear();
    m_lastBakeReport = {};
    m_stepAccumulatorSeconds = 0.0;
    m_statsRefreshAccumulatorSeconds = 0.0;
    m_debugVisualizationEnabled = false;
}

void SurfaceHydrology::resetWater()
{
    for (AdaptiveCell& cell : m_adaptiveCells)
    {
        cell.waterVolumeM3 = 0.0;
        cell.flowVelocityXMps = 0.0;
        cell.flowVelocityZMps = 0.0;
        cell.boundaryOutflowM3ps = 0.0f;
    }
    for (AdaptivePipe& pipe : m_adaptivePipes)
    {
        pipe.fluxM3ps = 0.0f;
        pipe.candidateFluxM3ps = 0.0f;
        pipe.transferredVolumeM3 = 0.0;
        pipe.deltaTimeSeconds = 0.0f;
    }
    for (SimulationChunk& chunk : m_simulationChunks)
    {
        chunk.cadenceAccumulatorSeconds = 0.0;
        chunk.elapsedSinceUpdateSeconds = 0.0;
        chunk.cadenceInitialized = false;
    }
    const bool available = !m_adaptiveCells.empty();
    const bool fromCache = m_stats.loadedFromCache;
    const std::size_t sourceTriangles = m_stats.sourceTriangleCount;
    const std::size_t supportCells = m_cells.size();
    const std::size_t connected = m_stats.connectedCellCount;
    m_stats = {};
    m_stats.available = available;
    m_stats.loadedFromCache = fromCache;
    m_stats.sourceTriangleCount = sourceTriangles;
    m_stats.supportCellCount = supportCells;
    m_stats.cellCount = m_adaptiveCells.size();
    m_stats.connectedCellCount = connected;
    m_stats.updateRateHz = m_description.updateRateHz;
    m_stats.debugVisualizationEnabled = m_debugVisualizationEnabled;
    m_stats.interestSourceCount = m_interestSources.size();
    m_stepAccumulatorSeconds = 0.0;
    m_statsRefreshAccumulatorSeconds = 0.0;
    m_presentationBasinRefreshAccumulatorSeconds = 0.0;
    refreshPresentationBasinLevels(m_lastPresentationFilmDepthM);
    refreshStats();
}

bool SurfaceHydrology::setUniformWaterDepthForLab(double waterDepthM)
{
    if (!std::isfinite(waterDepthM) || waterDepthM < 0.0
        || waterDepthM > m_description.maximumWaterDepthM
        || m_adaptiveCells.empty())
    {
        return false;
    }
    for (AdaptiveCell& cell : m_adaptiveCells)
    {
        cell.waterVolumeM3 = waterDepthM * cell.areaM2;
        cell.flowVelocityXMps = 0.0;
        cell.flowVelocityZMps = 0.0;
        cell.boundaryOutflowM3ps = 0.0f;
    }
    for (AdaptivePipe& pipe : m_adaptivePipes)
    {
        pipe.fluxM3ps = 0.0f;
        pipe.candidateFluxM3ps = 0.0f;
        pipe.transferredVolumeM3 = 0.0;
    }
    refreshPresentationBasinLevels(m_lastPresentationFilmDepthM);
    refreshStats();
    return true;
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

    report.valid = !m_adaptiveCells.empty();
    report.loadedFromCache = false;
    report.cellCount = m_adaptiveCells.size();
    report.connectedCellCount = m_stats.connectedCellCount;
    report.sourceFingerprint = fingerprint(triangles, globalOrigin);
    report.elapsedMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    report.message = report.valid
        ? (capped
            ? "Hydrology baked at the configured cell safety limit."
            : "Hydrology baked from static collision geometry.")
        : "No upward-facing collision area was suitable for hydrology.";
    m_stats.loadedFromCache = false;
    m_stats.sourceTriangleCount = triangles.size();
    m_lastBakeReport = report;
    return report.valid;
}

void SurfaceHydrology::rebuildLookupAndConnectivity()
{
    // WATER14: the fixed support raster is now immutable geometry/cache data.
    // It exists only so adaptive control volumes can sample accurate terrain,
    // material and precipitation-cover information. It is not stepped by the
    // water solver anymore.
    m_lookup.clear();
    m_lookup.reserve(m_cells.size());
    std::unordered_map<CellKey, double, CellKeyHash> highestSurfaceByColumn;
    highestSurfaceByColumn.reserve(m_cells.size());
    for (std::size_t i = 0; i < m_cells.size(); ++i)
    {
        m_lookup[m_cells[i].key] = static_cast<std::int32_t>(i);
        const CellKey columnKey{ m_cells[i].key.x, m_cells[i].key.z, 0 };
        const auto found = highestSurfaceByColumn.find(columnKey);
        if (found == highestSurfaceByColumn.end())
            highestSurfaceByColumn.emplace(columnKey, m_cells[i].elevationM);
        else
            found->second = std::max(found->second, m_cells[i].elevationM);
    }

    constexpr double kSkyExposureHeightToleranceM = 0.05;
    for (Cell& cell : m_cells)
    {
        const CellKey columnKey{ cell.key.x, cell.key.z, 0 };
        const auto highest = highestSurfaceByColumn.find(columnKey);
        cell.precipitationExposed = highest == highestSurfaceByColumn.end()
            || cell.elevationM >= highest->second - kSkyExposureHeightToleranceM;
    }

    rebuildAdaptiveSimulationTopology();
    rebuildPresentationBasins();
    rebuildAdaptivePipes();

    m_simulationChunks.clear();
    std::unordered_map<CellKey, std::size_t, CellKeyHash> simulationChunkLookup;
    simulationChunkLookup.reserve(std::max<std::size_t>(
        m_adaptiveCells.size() / 64u, 16u));
    for (std::size_t i = 0; i < m_adaptiveCells.size(); ++i)
    {
        const AdaptiveCell& cell = m_adaptiveCells[i];
        const CellKey chunkKey{
            static_cast<std::int64_t>(std::floor(
                cell.centerX / kAdaptiveSpatialBucketM)),
            static_cast<std::int64_t>(std::floor(
                cell.centerZ / kAdaptiveSpatialBucketM)),
            0 };
        auto found = simulationChunkLookup.find(chunkKey);
        if (found == simulationChunkLookup.end())
        {
            SimulationChunk chunk;
            chunk.key = chunkKey;
            m_simulationChunks.push_back(std::move(chunk));
            const std::size_t slot = m_simulationChunks.size() - 1u;
            simulationChunkLookup.emplace(chunkKey, slot);
            found = simulationChunkLookup.find(chunkKey);
        }
        m_simulationChunks[found->second].cellIndices.push_back(
            static_cast<std::int32_t>(i));
    }
    std::sort(
        m_simulationChunks.begin(),
        m_simulationChunks.end(),
        [](const SimulationChunk& left, const SimulationChunk& right) {
            return left.key < right.key;
        });

    m_dueCells.clear();
    m_dueCells.reserve(m_adaptiveCells.size());
    m_dueDeltaTimeByCell.assign(m_adaptiveCells.size(), 0.0f);
    m_outflowScaleByCell.assign(m_adaptiveCells.size(), 1.0);
    m_boundaryCandidateFluxByCell.assign(m_adaptiveCells.size(), 0.0);

    m_stats.available = !m_adaptiveCells.empty();
    m_stats.supportCellCount = m_cells.size();
    m_stats.cellCount = m_adaptiveCells.size();
    m_stats.connectedCellCount = 0u;
    for (const AdaptiveCell& cell : m_adaptiveCells)
    {
        if (!cell.pipeIndices.empty())
            ++m_stats.connectedCellCount;
    }
    m_stats.updateRateHz = m_description.updateRateHz;
    m_stats.debugVisualizationEnabled = m_debugVisualizationEnabled;
    refreshStats();
}


void SurfaceHydrology::rebuildPresentationBasins()
{
    m_presentationBasins.clear();
    m_presentationBasinConnections.clear();
    if (m_cells.empty())
        return;

    // WATER16 LiveSurface catchments are built once from the immutable 0.5 m
    // support raster. A neighbour is considered the same continuous drainage
    // surface when its elevation change is explained by the local support plane.
    // A curb/step therefore becomes a hard watershed boundary, while a uniformly
    // banked or downhill road remains connected even when adjacent cell centers
    // differ by many centimetres in world Y.
    constexpr double kMaximumUnexplainedStepM = 0.030;
    constexpr double kPlateauToleranceM = 0.0010;
    constexpr std::array<std::array<std::int64_t, 2>, 4> kDirections{{
        {{ -1, 0 }}, {{ 1, 0 }}, {{ 0, -1 }}, {{ 0, 1 }}
    }};

    const double supportCellSizeM = m_description.cellSizeM;
    const std::size_t cellCount = m_cells.size();
    std::vector<std::int32_t> downstream(cellCount, -1);
    std::vector<std::int32_t> roots(cellCount, -1);

    const auto compatibleSupport = [&](const Cell& a, const Cell& b,
                                       double dx, double dz) {
        if (a.key.layer != b.key.layer)
            return false;
        const double ax = static_cast<double>(a.normal.x);
        const double ay = static_cast<double>(a.normal.y);
        const double az = static_cast<double>(a.normal.z);
        const double bx = static_cast<double>(b.normal.x);
        const double by = static_cast<double>(b.normal.y);
        const double bz = static_cast<double>(b.normal.z);
        double nx = ax + bx;
        double ny = ay + by;
        double nz = az + bz;
        const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (length <= 1.0e-9 || !std::isfinite(length))
            return false;
        nx /= length;
        ny /= length;
        nz /= length;
        if (ny <= 0.05)
            return false;
        const double predictedDeltaM = -(nx * dx + nz * dz) / ny;
        const double actualDeltaM = b.elevationM - a.elevationM;
        return std::abs(actualDeltaM - predictedDeltaM)
            <= kMaximumUnexplainedStepM;
    };

    for (std::size_t index = 0; index < cellCount; ++index)
    {
        const Cell& source = m_cells[index];
        std::int32_t best = -1;
        double bestElevationM = source.elevationM;

        for (const auto& direction : kDirections)
        {
            const CellKey neighbourKey{
                source.key.x + direction[0],
                source.key.z + direction[1],
                source.key.layer };
            const auto found = m_lookup.find(neighbourKey);
            if (found == m_lookup.end() || found->second < 0)
                continue;
            const std::int32_t candidateIndex = found->second;
            const Cell& candidate = m_cells[
                static_cast<std::size_t>(candidateIndex)];
            const double dx = static_cast<double>(direction[0])
                * supportCellSizeM;
            const double dz = static_cast<double>(direction[1])
                * supportCellSizeM;
            if (!compatibleSupport(source, candidate, dx, dz))
                continue;

            const bool clearlyLower = candidate.elevationM
                < bestElevationM - kPlateauToleranceM;
            const bool bestIsAlreadyLower = best >= 0
                && bestElevationM < source.elevationM - kPlateauToleranceM;
            const bool plateauDrain = !bestIsAlreadyLower
                && std::abs(candidate.elevationM - source.elevationM)
                    <= kPlateauToleranceM
                && candidate.key < source.key
                && (best < 0
                    || candidate.key < m_cells[
                        static_cast<std::size_t>(best)].key);
            if (clearlyLower || plateauDrain)
            {
                best = candidateIndex;
                bestElevationM = candidate.elevationM;
            }
        }
        downstream[index] = best;
    }

    // Strict downhill links plus the lexicographically decreasing plateau rule
    // cannot form cycles, so path compression is deterministic and linear-ish.
    for (std::size_t index = 0; index < cellCount; ++index)
    {
        if (roots[index] >= 0)
            continue;
        std::size_t current = index;
        std::size_t guard = 0u;
        while (downstream[current] >= 0 && roots[current] < 0
            && guard++ <= cellCount)
        {
            current = static_cast<std::size_t>(downstream[current]);
        }
        const std::int32_t root = roots[current] >= 0
            ? roots[current]
            : static_cast<std::int32_t>(current);

        current = index;
        guard = 0u;
        while (roots[current] < 0 && guard++ <= cellCount)
        {
            roots[current] = root;
            if (downstream[current] < 0)
                break;
            current = static_cast<std::size_t>(downstream[current]);
        }
    }

    std::unordered_map<std::int32_t, std::int32_t> basinByRoot;
    basinByRoot.reserve(std::max<std::size_t>(cellCount / 64u, 16u));
    for (std::size_t index = 0; index < cellCount; ++index)
    {
        const std::int32_t root = roots[index];
        auto found = basinByRoot.find(root);
        if (found == basinByRoot.end())
        {
            PresentationBasin basin;
            basin.id = static_cast<std::int32_t>(m_presentationBasins.size());
            basin.layer = m_cells[index].key.layer;
            m_presentationBasins.push_back(std::move(basin));
            basinByRoot.emplace(root, m_presentationBasins.back().id);
            found = basinByRoot.find(root);
        }
        const std::int32_t basinId = found->second;
        m_cells[index].presentationBasinId = basinId;
        m_presentationBasins[static_cast<std::size_t>(basinId)]
            .supportIndices.push_back(static_cast<std::int32_t>(index));
    }

    // WATER17 spill graph.  The downstream-root partition is useful for
    // bookkeeping, but exposing each root independently can create striped
    // watershed seams on a smooth sloped road.  Build the dual graph of
    // neighbouring catchments and record the lowest saddle between each pair.
    // Runtime presentation performs a tiny spill-gated graph-Laplacian solve so
    // hydraulically connected puddles share a visually continuous head without
    // ever coupling across a curb/step.
    std::map<std::pair<std::int32_t, std::int32_t>, double> spillByPair;
    constexpr std::array<std::array<std::int64_t, 2>, 2> kPositiveDirections{{
        {{ 1, 0 }}, {{ 0, 1 }}
    }};
    for (const Cell& source : m_cells)
    {
        if (source.presentationBasinId < 0)
            continue;
        for (const auto& direction : kPositiveDirections)
        {
            const CellKey neighbourKey{
                source.key.x + direction[0],
                source.key.z + direction[1],
                source.key.layer };
            const auto found = m_lookup.find(neighbourKey);
            if (found == m_lookup.end() || found->second < 0)
                continue;
            const Cell& neighbour = m_cells[
                static_cast<std::size_t>(found->second)];
            if (neighbour.presentationBasinId < 0
                || neighbour.presentationBasinId == source.presentationBasinId)
            {
                continue;
            }
            const double dx = static_cast<double>(direction[0])
                * supportCellSizeM;
            const double dz = static_cast<double>(direction[1])
                * supportCellSizeM;
            if (!compatibleSupport(source, neighbour, dx, dz))
                continue;

            const auto pair = std::minmax(
                source.presentationBasinId,
                neighbour.presentationBasinId);
            const double spillElevationM = std::max(
                source.elevationM,
                neighbour.elevationM);
            const auto inserted = spillByPair.emplace(pair, spillElevationM);
            if (!inserted.second)
                inserted.first->second = std::min(
                    inserted.first->second, spillElevationM);
        }
    }
    m_presentationBasinConnections.reserve(spillByPair.size());
    for (const auto& [pair, spillElevationM] : spillByPair)
    {
        m_presentationBasinConnections.push_back({
            pair.first, pair.second, spillElevationM });
    }

    for (PresentationBasin& basin : m_presentationBasins)
    {
        basin.sortedSupportElevationsM.reserve(basin.supportIndices.size());
        for (const std::int32_t supportIndex : basin.supportIndices)
        {
            basin.sortedSupportElevationsM.push_back(
                m_cells[static_cast<std::size_t>(supportIndex)].elevationM);
        }
        std::sort(
            basin.sortedSupportElevationsM.begin(),
            basin.sortedSupportElevationsM.end());
        basin.prefixElevationSumM.resize(
            basin.sortedSupportElevationsM.size() + 1u, 0.0);
        for (std::size_t i = 0; i < basin.sortedSupportElevationsM.size(); ++i)
        {
            basin.prefixElevationSumM[i + 1u] =
                basin.prefixElevationSumM[i]
                + basin.sortedSupportElevationsM[i];
        }
    }

    // Map each adaptive authoritative control volume to the drainage catchment
    // beneath its center. This mapping is static until the scene/topology is
    // rebuilt; only the conserved water volume changes at runtime.
    for (AdaptiveCell& adaptive : m_adaptiveCells)
    {
        adaptive.presentationBasinId = -1;
        const std::int32_t supportIndex = findCellIndex({
            adaptive.centerX,
            adaptive.elevationM,
            adaptive.centerZ });
        if (supportIndex < 0)
            continue;
        adaptive.presentationBasinId = m_cells[
            static_cast<std::size_t>(supportIndex)].presentationBasinId;
    }

    refreshPresentationBasinLevels(m_lastPresentationFilmDepthM);
}

void SurfaceHydrology::refreshPresentationBasinLevels(double smoothFilmDepthM)
{
    if (m_presentationBasins.empty())
        return;

    const double filmDepthM = std::clamp(
        std::isfinite(smoothFilmDepthM) ? smoothFilmDepthM : 0.0,
        0.0,
        0.006);
    // The smooth SurfaceWeather film is drawn everywhere and therefore must not
    // be counted again as a discrete puddle. Require another 1.5 mm of local
    // authoritative depth before basin water participates in standing-water
    // optics, matching the established WATER15I threshold.
    constexpr double kLocalPuddleExcessM = 0.0015;
    const double puddleThresholdM = filmDepthM + kLocalPuddleExcessM;

    for (PresentationBasin& basin : m_presentationBasins)
    {
        basin.excessWaterVolumeM3 = 0.0;
        basin.waterSurfaceElevationM = -32768.0;
    }

    for (const AdaptiveCell& cell : m_adaptiveCells)
    {
        if (cell.presentationBasinId < 0
            || static_cast<std::size_t>(cell.presentationBasinId)
                >= m_presentationBasins.size())
        {
            continue;
        }
        const double excessDepthM = std::max(
            adaptiveWaterDepth(cell) - puddleThresholdM,
            0.0);
        if (excessDepthM <= 1.0e-9)
            continue;
        m_presentationBasins[
            static_cast<std::size_t>(cell.presentationBasinId)]
            .excessWaterVolumeM3 += excessDepthM * cell.areaM2;
    }

    const double supportAreaM2 = m_description.cellSizeM
        * m_description.cellSizeM;
    for (PresentationBasin& basin : m_presentationBasins)
    {
        const double volumeM3 = basin.excessWaterVolumeM3;
        const auto& heights = basin.sortedSupportElevationsM;
        if (volumeM3 <= 1.0e-12 || heights.empty())
            continue;

        double levelM = heights.front();
        const double volumeAsHeightSumM = volumeM3
            / std::max(supportAreaM2, 1.0e-9);
        for (std::size_t activeCount = 1u;
             activeCount <= heights.size(); ++activeCount)
        {
            const double prefix = basin.prefixElevationSumM[activeCount];
            const double candidate = (volumeAsHeightSumM + prefix)
                / static_cast<double>(activeCount);
            if (activeCount == heights.size()
                || candidate <= heights[activeCount])
            {
                levelM = candidate;
                break;
            }
        }
        basin.waterSurfaceElevationM = levelM;
    }

    // WATER17 spill-aware graph-Laplacian regularization.  This is strictly a
    // presentation solve: conserved volume remains in authoritative adaptive
    // cells/basins.  It removes millimetre-scale head discontinuities created by
    // arbitrary downstream-root partitioning on a smooth continuous road.  The
    // saddle gate prevents coupling before neighbouring puddles can physically
    // communicate, and the +/-12 mm trust region prevents optical smoothing from
    // overriding macro hydrology.
    if (!m_presentationBasinConnections.empty())
    {
        std::vector<double> rawHead(m_presentationBasins.size(), -32768.0);
        std::vector<double> relaxedHead(m_presentationBasins.size(), -32768.0);
        std::vector<double> nextHead(m_presentationBasins.size(), -32768.0);
        for (std::size_t i = 0; i < m_presentationBasins.size(); ++i)
        {
            rawHead[i] = m_presentationBasins[i].waterSurfaceElevationM;
            relaxedHead[i] = rawHead[i];
        }

        constexpr int kRelaxationIterations = 6;
        constexpr double kMaximumOpticalHeadShiftM = 0.012;
        for (int iteration = 0; iteration < kRelaxationIterations; ++iteration)
        {
            std::vector<double> weightedSum = relaxedHead;
            std::vector<double> weight(m_presentationBasins.size(), 1.0);
            for (const PresentationBasinConnection& connection
                 : m_presentationBasinConnections)
            {
                if (connection.basinA < 0 || connection.basinB < 0)
                    continue;
                const std::size_t a = static_cast<std::size_t>(connection.basinA);
                const std::size_t b = static_cast<std::size_t>(connection.basinB);
                if (a >= relaxedHead.size() || b >= relaxedHead.size())
                    continue;
                const double ha = relaxedHead[a];
                const double hb = relaxedHead[b];
                if (ha <= -16000.0 || hb <= -16000.0)
                    continue;

                // A C1 smooth saddle gate: zero below the spill, fully coupled
                // once either free surface is about 8 mm above the saddle.
                const double fillAboveSpillM =
                    std::max(ha, hb) - connection.spillElevationM;
                double t = std::clamp(
                    (fillAboveSpillM + 0.0015) / 0.0095,
                    0.0,
                    1.0);
                t = t * t * (3.0 - 2.0 * t);
                if (t <= 1.0e-8)
                    continue;
                const double couplingWeight = 0.42 * t;
                weightedSum[a] += hb * couplingWeight;
                weightedSum[b] += ha * couplingWeight;
                weight[a] += couplingWeight;
                weight[b] += couplingWeight;
            }

            for (std::size_t i = 0; i < relaxedHead.size(); ++i)
            {
                if (rawHead[i] <= -16000.0)
                {
                    nextHead[i] = rawHead[i];
                    continue;
                }
                const double candidate = weightedSum[i]
                    / std::max(weight[i], 1.0e-9);
                nextHead[i] = std::clamp(
                    candidate,
                    rawHead[i] - kMaximumOpticalHeadShiftM,
                    rawHead[i] + kMaximumOpticalHeadShiftM);
            }
            relaxedHead.swap(nextHead);
        }

        for (std::size_t i = 0; i < m_presentationBasins.size(); ++i)
        {
            if (rawHead[i] > -16000.0)
                m_presentationBasins[i].waterSurfaceElevationM = relaxedHead[i];
        }
    }
}

double SurfaceHydrology::adaptiveSurfaceElevationAt(
    const AdaptiveCell& cell,
    double x,
    double z) const
{
    const double nx = static_cast<double>(cell.normal.x);
    const double ny = static_cast<double>(cell.normal.y);
    const double nz = static_cast<double>(cell.normal.z);
    if (!std::isfinite(ny) || std::abs(ny) <= 1.0e-8)
        return cell.elevationM;
    return cell.elevationM
        - (nx * (x - cell.centerX) + nz * (z - cell.centerZ)) / ny;
}

double SurfaceHydrology::adaptiveWaterDepth(const AdaptiveCell& cell) const
{
    return cell.areaM2 > 1.0e-12
        ? std::max(cell.waterVolumeM3 / cell.areaM2, 0.0)
        : 0.0;
}

void SurfaceHydrology::rebuildAdaptiveSimulationTopology()
{
    m_adaptiveCells.clear();
    m_adaptiveSpatialBuckets.clear();
    if (m_cells.empty())
        return;

    const double supportSizeM = m_description.cellSizeM;
    const int maximumSupportSpan = std::max(1, static_cast<int>(std::floor(
        m_description.adaptiveMaximumCellSizeM / supportSizeM + 1.0e-9)));
    const double normalCosine = std::cos(
        m_description.adaptiveNormalErrorDegrees
        * 3.14159265358979323846 / 180.0);
    std::vector<std::uint8_t> used(m_cells.size(), 0u);

    const auto supportIndex = [&](std::int64_t x,
                                  std::int64_t z,
                                  std::int64_t layer) -> std::int32_t {
        const auto found = m_lookup.find({ x, z, layer });
        return found == m_lookup.end() ? -1 : found->second;
    };

    const auto normalizedNormal = [](heritage::math::Vec3 n) {
        const double length = std::sqrt(
            static_cast<double>(n.x) * n.x
            + static_cast<double>(n.y) * n.y
            + static_cast<double>(n.z) * n.z);
        if (length <= 1.0e-9 || !std::isfinite(length))
            return heritage::math::Vec3{ 0.0f, 1.0f, 0.0f };
        return heritage::math::Vec3{
            static_cast<float>(n.x / length),
            static_cast<float>(n.y / length),
            static_cast<float>(n.z / length) };
    };

    // WATER14I: classify *where* fine detail lives instead of turning an
    // entire 0.50 m support square into 25 x 0.10 m control volumes whenever a
    // curb merely touches one side. There are now two independent signals:
    //
    //   1) genuinely aggressive angular support -> authoritative 0.10 m cells;
    //   2) sharp N/E/S/W height discontinuity -> one presentation-only 0.10 m
    //      boundary strip, while the authoritative cell beside it stays 0.50 m.
    //
    // Both signals seed the same distance field used by coarse packing, so the
    // topology grows 0.50 -> 1 -> 2 -> 4 -> 8 -> 16 m away from the feature.
    // This keeps the expensive minimum tier concentrated on the actual line.
    const double aggressiveSlopeNormalY = std::cos(
        m_description.adaptiveMinimumCellSlopeDegrees
        * 3.14159265358979323846 / 180.0);
    const double aggressiveNormalBreakCosine = std::cos(
        m_description.adaptiveMinimumCellNormalBreakDegrees
        * 3.14159265358979323846 / 180.0);
    const double fineEdgeSurfaceBreakM = std::max(
        0.025, m_description.adaptiveSurfaceErrorM * 1.25);

    using VisualCell = SurfaceHydrologyVisualCell;
    std::vector<std::uint8_t> angularFineSupport(m_cells.size(), 0u);
    std::vector<std::uint8_t> fineBoundaryMaskBySupport(m_cells.size(), 0u);

    // Angular detail is still allowed to consume true 0.10 m simulation cells.
    // Normal breaks may use all 8 neighbours because they describe local
    // curvature/creases rather than a directional curb boundary.
    for (std::size_t supportIndexValue = 0;
         supportIndexValue < m_cells.size(); ++supportIndexValue)
    {
        const Cell& source = m_cells[supportIndexValue];
        const heritage::math::Vec3 sourceNormal = normalizedNormal(source.normal);
        bool needsSubcells = static_cast<double>(sourceNormal.y)
            <= aggressiveSlopeNormalY;
        for (int dz = -1; dz <= 1 && !needsSubcells; ++dz)
        {
            for (int dx = -1; dx <= 1 && !needsSubcells; ++dx)
            {
                if (dx == 0 && dz == 0)
                    continue;
                const std::int32_t neighbourIndex = supportIndex(
                    source.key.x + dx, source.key.z + dz, source.key.layer);
                if (neighbourIndex < 0)
                    continue;
                const heritage::math::Vec3 neighbourNormal = normalizedNormal(
                    m_cells[static_cast<std::size_t>(neighbourIndex)].normal);
                const double dot = static_cast<double>(sourceNormal.x)
                        * neighbourNormal.x
                    + static_cast<double>(sourceNormal.y) * neighbourNormal.y
                    + static_cast<double>(sourceNormal.z) * neighbourNormal.z;
                if (dot < aggressiveNormalBreakCosine)
                    needsSubcells = true;
            }
        }
        angularFineSupport[supportIndexValue] = needsSubcells ? 1u : 0u;
    }

    // Detect each real support-height step once (east and north), then mark the
    // exact opposing edges on both cells. Raw height delta is corrected for the
    // average tangent plane, so a continuous sloped road does not masquerade as
    // a curb. The directional mask survives into presentation and is what makes
    // only one ~0.10 m line hug the curb.
    for (std::size_t supportIndexValue = 0;
         supportIndexValue < m_cells.size(); ++supportIndexValue)
    {
        const Cell& source = m_cells[supportIndexValue];
        const heritage::math::Vec3 sourceNormal = normalizedNormal(source.normal);
        constexpr std::array<std::array<int, 2>, 2> positiveDirections{{
            {{ 1, 0 }}, {{ 0, 1 }}
        }};
        for (const auto& direction : positiveDirections)
        {
            const int dx = direction[0];
            const int dz = direction[1];
            const std::int32_t neighbourIndex = supportIndex(
                source.key.x + dx, source.key.z + dz, source.key.layer);
            if (neighbourIndex < 0)
                continue;
            const std::size_t neighbourSlot =
                static_cast<std::size_t>(neighbourIndex);
            const Cell& neighbour = m_cells[neighbourSlot];
            const heritage::math::Vec3 neighbourNormal = normalizedNormal(
                neighbour.normal);
            heritage::math::Vec3 averageNormal = normalizedNormal({
                sourceNormal.x + neighbourNormal.x,
                sourceNormal.y + neighbourNormal.y,
                sourceNormal.z + neighbourNormal.z });
            const double averageNy = std::max(
                std::abs(static_cast<double>(averageNormal.y)), 1.0e-6);
            const double deltaX = static_cast<double>(dx) * supportSizeM;
            const double deltaZ = static_cast<double>(dz) * supportSizeM;
            const double expectedContinuousDeltaY = -(
                static_cast<double>(averageNormal.x) * deltaX
                + static_cast<double>(averageNormal.z) * deltaZ)
                / averageNy;
            const double actualDeltaY = neighbour.elevationM - source.elevationM;
            const double unexplainedStepM = std::abs(
                actualDeltaY - expectedContinuousDeltaY);
            if (unexplainedStepM <= fineEdgeSurfaceBreakM)
                continue;

            if (dx > 0)
            {
                fineBoundaryMaskBySupport[supportIndexValue]
                    |= VisualCell::FineBoundaryRight;
                fineBoundaryMaskBySupport[neighbourSlot]
                    |= VisualCell::FineBoundaryLeft;
            }
            else
            {
                fineBoundaryMaskBySupport[supportIndexValue]
                    |= VisualCell::FineBoundaryTop;
                fineBoundaryMaskBySupport[neighbourSlot]
                    |= VisualCell::FineBoundaryBottom;
            }
        }
    }

    // WATER14J: build a real feature-distance hierarchy around sharp detail.
    // The previous five-support halo expanded through all LODs in only 2.5 m,
    // which still bunched many small cells around a curb. Keep the minimum
    // detail on the feature itself, then double both the permitted cell size
    // and the physical width of each transition band: 0.5 -> 1 -> 2 -> 4 ->
    // 8 -> 16 -> 20 m. This is the restricted-quadtree behaviour used by the
    // reference mesh: detail follows the line, not a rectangular carpet.
    constexpr std::uint16_t kMaximumGradingDistanceSupports = 32u; // 16 m.
    constexpr std::uint16_t kFarFromFineDetail = 65535u;
    std::vector<std::uint16_t> fineDetailDistance(
        m_cells.size(), kFarFromFineDetail);
    std::deque<std::size_t> gradingQueue;
    for (std::size_t supportIndexValue = 0;
         supportIndexValue < m_cells.size(); ++supportIndexValue)
    {
        const bool seedsDetail = angularFineSupport[supportIndexValue] != 0u
            || fineBoundaryMaskBySupport[supportIndexValue] != 0u;
        if (!seedsDetail)
            continue;
        fineDetailDistance[supportIndexValue] = 0u;
        gradingQueue.push_back(supportIndexValue);
    }
    while (!gradingQueue.empty())
    {
        const std::size_t currentIndex = gradingQueue.front();
        gradingQueue.pop_front();
        const std::uint16_t currentDistance = fineDetailDistance[currentIndex];
        if (currentDistance >= kMaximumGradingDistanceSupports)
            continue;
        const Cell& current = m_cells[currentIndex];
        // Four-connected propagation measures perpendicular distance from a
        // traced feature much more faithfully than the old 8-neighbour halo.
        // Diagonal shortcuts made the transition bands collapse at corners.
        constexpr std::array<std::array<int, 2>, 4> kCardinalDirections{{
            {{ -1, 0 }}, {{ 1, 0 }}, {{ 0, -1 }}, {{ 0, 1 }}
        }};
        for (const auto& direction : kCardinalDirections)
        {
            const std::int32_t neighbourIndex = supportIndex(
                current.key.x + direction[0],
                current.key.z + direction[1],
                current.key.layer);
            if (neighbourIndex < 0)
                continue;
            const std::size_t neighbour =
                static_cast<std::size_t>(neighbourIndex);
            const std::uint16_t candidateDistance =
                static_cast<std::uint16_t>(currentDistance + 1u);
            if (fineDetailDistance[neighbour] <= candidateDistance)
                continue;
            fineDetailDistance[neighbour] = candidateDistance;
            gradingQueue.push_back(neighbour);
        }
    }

    const auto maximumSpanNearFineDetail = [&](std::uint16_t distance) {
        // Restricted 2:1 hierarchy. Each successive band is twice as wide and
        // permits cells twice as large, so a 0.5 m leaf cannot directly touch
        // an 8/16/20 m leaf. The 20 m tier is only the final capped far field.
        if (distance == 0u)
            return 1;   // 0.00-0.50 m: 0.50 m solver leaf (+0.10 m edge strip).
        if (distance < 2u)
            return 2;   // 0.50-1.00 m: 1 m.
        if (distance < 4u)
            return 4;   // 1-2 m: 2 m.
        if (distance < 8u)
            return 8;   // 2-4 m: 4 m.
        if (distance < 16u)
            return 16;  // 4-8 m: 8 m.
        if (distance < 32u)
            return 32;  // 8-16 m: 16 m.
        return maximumSupportSpan; // >=16 m: capped at configured 20 m.
    };

    // WATER14F: coarse-cell eligibility is based on how well one plane fits
    // the support samples, not on absolute slope and not on a single anchor
    // triangle normal. This lets a long downhill road or broad parking lot use
    // large control volumes while curbs/steps/creases fail locally.
    struct SupportPlaneFit
    {
        double elevationAtMeanM = 0.0;
        double slopeX = 0.0;
        double slopeZ = 0.0;
        double meanX = 0.0;
        double meanZ = 0.0;
        double maximumResidualM = 0.0;
        heritage::math::Vec3 normal{ 0.0f, 1.0f, 0.0f };
    };

    const auto fitSupportPlane = [&](const std::vector<std::int32_t>& members) {
        SupportPlaneFit fit{};
        if (members.empty())
            return fit;

        double sumX = 0.0;
        double sumZ = 0.0;
        double sumY = 0.0;
        for (const std::int32_t member : members)
        {
            const Cell& source = m_cells[static_cast<std::size_t>(member)];
            sumX += (static_cast<double>(source.key.x) + 0.5) * supportSizeM;
            sumZ += (static_cast<double>(source.key.z) + 0.5) * supportSizeM;
            sumY += source.elevationM;
        }
        const double inverse = 1.0 / static_cast<double>(members.size());
        fit.meanX = sumX * inverse;
        fit.meanZ = sumZ * inverse;
        fit.elevationAtMeanM = sumY * inverse;

        double xx = 0.0;
        double zz = 0.0;
        double xz = 0.0;
        double xy = 0.0;
        double zy = 0.0;
        for (const std::int32_t member : members)
        {
            const Cell& source = m_cells[static_cast<std::size_t>(member)];
            const double x = (static_cast<double>(source.key.x) + 0.5)
                * supportSizeM;
            const double z = (static_cast<double>(source.key.z) + 0.5)
                * supportSizeM;
            const double dx = x - fit.meanX;
            const double dz = z - fit.meanZ;
            const double dy = source.elevationM - fit.elevationAtMeanM;
            xx += dx * dx;
            zz += dz * dz;
            xz += dx * dz;
            xy += dx * dy;
            zy += dz * dy;
        }

        const double determinant = xx * zz - xz * xz;
        if (std::abs(determinant) > 1.0e-12)
        {
            fit.slopeX = (xy * zz - zy * xz) / determinant;
            fit.slopeZ = (zy * xx - xy * xz) / determinant;
        }
        else if (xx > 1.0e-12)
        {
            fit.slopeX = xy / xx;
        }
        else if (zz > 1.0e-12)
        {
            fit.slopeZ = zy / zz;
        }

        fit.normal = normalizedNormal({
            static_cast<float>(-fit.slopeX),
            1.0f,
            static_cast<float>(-fit.slopeZ) });
        for (const std::int32_t member : members)
        {
            const Cell& source = m_cells[static_cast<std::size_t>(member)];
            const double x = (static_cast<double>(source.key.x) + 0.5)
                * supportSizeM;
            const double z = (static_cast<double>(source.key.z) + 0.5)
                * supportSizeM;
            const double predicted = fit.elevationAtMeanM
                + fit.slopeX * (x - fit.meanX)
                + fit.slopeZ * (z - fit.meanZ);
            fit.maximumResidualM = std::max(
                fit.maximumResidualM,
                std::abs(predicted - source.elevationM));
        }
        return fit;
    };

    const auto makeAdaptiveCell = [&](std::int64_t minimumSupportX,
                                      std::int64_t minimumSupportZ,
                                      int supportSpan,
                                      std::int64_t layer,
                                      const std::vector<std::int32_t>& members) {
        AdaptiveCell cell;
        const double minimumX = static_cast<double>(minimumSupportX) * supportSizeM;
        const double minimumZ = static_cast<double>(minimumSupportZ) * supportSizeM;
        const double sizeM = static_cast<double>(supportSpan) * supportSizeM;
        cell.minimumXUnits = static_cast<std::int64_t>(std::llround(
            minimumX / kAdaptiveUnitM));
        cell.minimumZUnits = static_cast<std::int64_t>(std::llround(
            minimumZ / kAdaptiveUnitM));
        cell.spanUnits = static_cast<std::uint16_t>(std::clamp<long long>(
            std::llround(sizeM / kAdaptiveUnitM), 1ll, 65535ll));
        cell.layer = layer;
        cell.cellSizeM = sizeM;
        cell.areaM2 = sizeM * sizeM;
        cell.centerX = minimumX + sizeM * 0.5;
        cell.centerZ = minimumZ + sizeM * 0.5;

        const SupportPlaneFit fit = fitSupportPlane(members);
        cell.elevationM = fit.elevationAtMeanM
            + fit.slopeX * (cell.centerX - fit.meanX)
            + fit.slopeZ * (cell.centerZ - fit.meanZ);
        cell.normal = fit.normal;

        double infiltrationSum = 0.0;
        double drainageSum = 0.0;
        double roughnessSum = 0.0;
        double depressionSum = 0.0;
        bool exposed = true;
        SurfaceMaterial material = SurfaceMaterial::Default;
        if (!members.empty())
            material = m_cells[static_cast<std::size_t>(members.front())].material;
        for (const std::int32_t member : members)
        {
            const Cell& source = m_cells[static_cast<std::size_t>(member)];
            infiltrationSum += source.infiltrationMmPerHour;
            drainageSum += source.drainageMmPerHour;
            roughnessSum += source.roughness;
            depressionSum += source.depressionStorageM;
            exposed = exposed && source.precipitationExposed;
        }
        const double inverse = members.empty()
            ? 1.0 : 1.0 / static_cast<double>(members.size());
        cell.material = material;
        cell.infiltrationMmPerHour = infiltrationSum * inverse;
        cell.drainageMmPerHour = drainageSum * inverse;
        cell.roughness = roughnessSum * inverse;
        cell.depressionStorageM = depressionSum * inverse;
        cell.precipitationExposed = exposed;
        if (supportSpan == 1 && members.size() == 1u)
        {
            const std::size_t supportSlot = static_cast<std::size_t>(members.front());
            cell.fineBoundaryMask = fineBoundaryMaskBySupport[supportSlot];
        }
        m_adaptiveCells.push_back(std::move(cell));
    };

    const auto mergeCandidateValid = [&](const Cell& anchor,
                                         int supportSpan,
                                         std::vector<std::int32_t>& members) {
        members.clear();
        members.reserve(static_cast<std::size_t>(supportSpan * supportSpan));
        const double patchSizeM = static_cast<double>(supportSpan) * supportSizeM;
        // The residual allowance grows very slowly with patch size to absorb
        // centimetre-scale collider tessellation noise without flattening real
        // curbs/steps. A 20 m candidate gets 5 cm total tolerance by default.
        const double surfaceToleranceM = m_description.adaptiveSurfaceErrorM
            + patchSizeM * 0.0015;
        for (int dz = 0; dz < supportSpan; ++dz)
        {
            for (int dx = 0; dx < supportSpan; ++dx)
            {
                const std::int32_t index = supportIndex(
                    anchor.key.x + dx, anchor.key.z + dz, anchor.key.layer);
                if (index < 0 || used[static_cast<std::size_t>(index)] != 0u)
                    return false;
                const Cell& candidate = m_cells[static_cast<std::size_t>(index)];
                if (candidate.material != anchor.material
                    || candidate.precipitationExposed != anchor.precipitationExposed
                    || std::abs(candidate.infiltrationMmPerHour
                        - anchor.infiltrationMmPerHour) > 0.05
                    || std::abs(candidate.drainageMmPerHour
                        - anchor.drainageMmPerHour) > 0.05
                    || std::abs(candidate.roughness - anchor.roughness) > 0.006
                    || std::abs(candidate.depressionStorageM
                        - anchor.depressionStorageM) > 0.0004)
                {
                    return false;
                }

                // WATER14I: a detected curb/step is a hard topology boundary.
                // A candidate may terminate on that line, but it may never
                // swallow the line into its interior even if a broad least-
                // squares plane would otherwise make the residual look cheap.
                const std::uint8_t boundaryMask =
                    fineBoundaryMaskBySupport[static_cast<std::size_t>(index)];
                if ((dx > 0
                        && (boundaryMask & VisualCell::FineBoundaryLeft) != 0u)
                    || (dx + 1 < supportSpan
                        && (boundaryMask & VisualCell::FineBoundaryRight) != 0u)
                    || (dz > 0
                        && (boundaryMask & VisualCell::FineBoundaryBottom) != 0u)
                    || (dz + 1 < supportSpan
                        && (boundaryMask & VisualCell::FineBoundaryTop) != 0u))
                {
                    return false;
                }
                members.push_back(index);
            }
        }

        // WATER14G/H graded transition guard. The 40 cm presentation seam search
        // should only repair small boundary disagreement; it must not be asked
        // to bridge a 20 m control volume directly into a 0.10 m detail island.
        // Cap the accepted coarse span according to the nearest aggressive
        // support represented by this candidate.
        std::uint16_t nearestFineDetail = kFarFromFineDetail;
        for (const std::int32_t member : members)
        {
            nearestFineDetail = std::min(
                nearestFineDetail,
                fineDetailDistance[static_cast<std::size_t>(member)]);
        }
        if (supportSpan > maximumSpanNearFineDetail(nearestFineDetail))
            return false;

        // Surface-fit error is still part of coarse-cell merge eligibility;
        // WATER14G only adds a local span cap around aggressive angular detail.
        const SupportPlaneFit fit = fitSupportPlane(members);
        if (!std::isfinite(fit.maximumResidualM)
            || fit.maximumResidualM > surfaceToleranceM)
        {
            return false;
        }

        for (const std::int32_t member : members)
        {
            const heritage::math::Vec3 candidateNormal = normalizedNormal(
                m_cells[static_cast<std::size_t>(member)].normal);
            const double normalDot = static_cast<double>(fit.normal.x)
                * candidateNormal.x
                + static_cast<double>(fit.normal.y) * candidateNormal.y
                + static_cast<double>(fit.normal.z) * candidateNormal.z;
            if (normalDot < normalCosine)
                return false;
        }
        return true;
    };

    // WATER14J: large-to-small restricted-quadtree passes. Powers-of-two are
    // used below the configured 20 m cap so neighbouring refinement tiers have
    // a stable 2:1 relationship and coarse/fine T-junctions stay predictable.
    // The 40-support (20 m) top leaf is a cap for very uniform far-field areas;
    // the next true hierarchy leaf is 32 supports (16 m).
    constexpr std::array<int, 6> kSupportSpanCandidates{
        40, 32, 16, 8, 4, 2 };
    std::vector<std::int32_t> members;
    for (const int span : kSupportSpanCandidates)
    {
        if (span > maximumSupportSpan)
            continue;
        for (std::size_t supportIndexValue = 0;
             supportIndexValue < m_cells.size(); ++supportIndexValue)
        {
            if (used[supportIndexValue] != 0u)
                continue;
            const Cell& anchor = m_cells[supportIndexValue];
            // WATER14F: do not require a global span-aligned origin. A curb or
            // island should invalidate only candidates that actually include it;
            // the planar asphalt immediately beside it may begin a large patch.
            if (!mergeCandidateValid(anchor, span, members))
                continue;
            makeAdaptiveCell(
                anchor.key.x, anchor.key.z, span, anchor.key.layer, members);
            for (const std::int32_t member : members)
                used[static_cast<std::size_t>(member)] = 1u;
        }
    }

    // Anything that cannot safely merge remains 0.5 m. WATER14I keeps the
    // expensive authoritative 0.10 m tier strictly for genuinely aggressive
    // angular geometry. A curb/sidewalk height step alone stays a 0.50 m solver
    // cell and carries only a directional fineBoundaryMask for presentation.
    for (std::size_t supportIndexValue = 0;
         supportIndexValue < m_cells.size(); ++supportIndexValue)
    {
        if (used[supportIndexValue] != 0u)
            continue;
        const Cell& source = m_cells[supportIndexValue];
        const heritage::math::Vec3 sourceNormal = normalizedNormal(source.normal);
        const bool needsSubcells = angularFineSupport[supportIndexValue] != 0u;

        if (!needsSubcells
            || m_description.adaptiveMinimumCellSizeM >= supportSizeM - 1.0e-9)
        {
            members.assign(1u, static_cast<std::int32_t>(supportIndexValue));
            makeAdaptiveCell(
                source.key.x, source.key.z, 1, source.key.layer, members);
            used[supportIndexValue] = 1u;
            continue;
        }

        const int subdivisions = std::max(1, static_cast<int>(std::floor(
            supportSizeM / m_description.adaptiveMinimumCellSizeM + 1.0e-9)));
        const double subSizeM = supportSizeM / static_cast<double>(subdivisions);
        const std::int64_t supportMinimumXUnits = static_cast<std::int64_t>(
            std::llround(static_cast<double>(source.key.x) * supportSizeM
                / kAdaptiveUnitM));
        const std::int64_t supportMinimumZUnits = static_cast<std::int64_t>(
            std::llround(static_cast<double>(source.key.z) * supportSizeM
                / kAdaptiveUnitM));
        const int subSpanUnits = std::max(1, static_cast<int>(std::llround(
            subSizeM / kAdaptiveUnitM)));
        const double sourceCenterX =
            (static_cast<double>(source.key.x) + 0.5) * supportSizeM;
        const double sourceCenterZ =
            (static_cast<double>(source.key.z) + 0.5) * supportSizeM;
        for (int subZ = 0; subZ < subdivisions; ++subZ)
        {
            for (int subX = 0; subX < subdivisions; ++subX)
            {
                AdaptiveCell cell;
                cell.minimumXUnits = supportMinimumXUnits
                    + static_cast<std::int64_t>(subX * subSpanUnits);
                cell.minimumZUnits = supportMinimumZUnits
                    + static_cast<std::int64_t>(subZ * subSpanUnits);
                cell.spanUnits = static_cast<std::uint16_t>(subSpanUnits);
                cell.layer = source.key.layer;
                cell.cellSizeM = subSizeM;
                cell.areaM2 = subSizeM * subSizeM;
                cell.centerX = static_cast<double>(source.key.x) * supportSizeM
                    + (static_cast<double>(subX) + 0.5) * subSizeM;
                cell.centerZ = static_cast<double>(source.key.z) * supportSizeM
                    + (static_cast<double>(subZ) + 0.5) * subSizeM;
                cell.elevationM = source.elevationM
                    - (static_cast<double>(sourceNormal.x)
                        * (cell.centerX - sourceCenterX)
                       + static_cast<double>(sourceNormal.z)
                        * (cell.centerZ - sourceCenterZ))
                        / std::max(static_cast<double>(sourceNormal.y), 1.0e-6);
                cell.normal = sourceNormal;
                cell.material = source.material;
                cell.infiltrationMmPerHour = source.infiltrationMmPerHour;
                cell.drainageMmPerHour = source.drainageMmPerHour;
                cell.roughness = source.roughness;
                cell.depressionStorageM = source.depressionStorageM;
                cell.precipitationExposed = source.precipitationExposed;
                m_adaptiveCells.push_back(std::move(cell));
            }
        }
        used[supportIndexValue] = 1u;
    }

    // WATER14J: explicit 2:1 balancing pass. Distance-based grading gives the
    // hierarchy its intended shape, but unaligned greedy packing can still
    // leave a small orphan leaf immediately beside a much larger accepted
    // patch. Repair only those actual shared-face violations. This is a true
    // topology rule, not another broad refinement halo: a coarse cell is split
    // only when it physically touches a >=2x-finer solver neighbour.
    //
    // The 0.10 m aggressive-angle subcells are intentionally excluded here;
    // they live below the immutable 0.50 m terrain-support resolution and are
    // transitioned by presentation topology. The authoritative 0.50 m+ mesh
    // itself is kept restricted to <=2:1 across shared support faces.
    constexpr std::size_t kMaximumBalancePasses = 8u;
    const auto splitBalancedCell = [&](const AdaptiveCell& parent,
                                       std::vector<AdaptiveCell>& output) {
        const auto appendChild = [&](double minimumX,
                                     double minimumZ,
                                     double sizeM) {
            AdaptiveCell child = parent;
            child.minimumXUnits = static_cast<std::int64_t>(std::llround(
                minimumX / kAdaptiveUnitM));
            child.minimumZUnits = static_cast<std::int64_t>(std::llround(
                minimumZ / kAdaptiveUnitM));
            child.spanUnits = static_cast<std::uint16_t>(std::clamp<long long>(
                std::llround(sizeM / kAdaptiveUnitM), 1ll, 65535ll));
            child.cellSizeM = sizeM;
            child.areaM2 = sizeM * sizeM;
            child.centerX = minimumX + sizeM * 0.5;
            child.centerZ = minimumZ + sizeM * 0.5;
            const double nx = static_cast<double>(parent.normal.x);
            const double ny = std::max(
                std::abs(static_cast<double>(parent.normal.y)), 1.0e-8);
            const double nz = static_cast<double>(parent.normal.z);
            child.elevationM = parent.elevationM
                - (nx * (child.centerX - parent.centerX)
                    + nz * (child.centerZ - parent.centerZ)) / ny;
            child.waterVolumeM3 = parent.waterVolumeM3
                * (child.areaM2 / std::max(parent.areaM2, 1.0e-12));
            child.boundaryOutflowM3ps = 0.0f;
            child.openBoundaryLengthM = 0.0;
            child.pipeIndices.clear();
            child.fineBoundaryMask = 0u;
            output.push_back(std::move(child));
        };

        const double minimumX = static_cast<double>(parent.minimumXUnits)
            * kAdaptiveUnitM;
        const double minimumZ = static_cast<double>(parent.minimumZUnits)
            * kAdaptiveUnitM;
        if (parent.cellSizeM > 16.0001)
        {
            // 20 m is a deliberate non-power-of-two cap. If it ever violates
            // 2:1, drop locally to the 4 m quadtree tier (5x5) instead of
            // inventing 10/5/2.5 m transition levels that cannot descend
            // cleanly to the 0.50 m support lattice.
            constexpr int kTopCapSubdivision = 5;
            const double childSizeM = parent.cellSizeM
                / static_cast<double>(kTopCapSubdivision);
            for (int z = 0; z < kTopCapSubdivision; ++z)
            {
                for (int x = 0; x < kTopCapSubdivision; ++x)
                {
                    appendChild(
                        minimumX + static_cast<double>(x) * childSizeM,
                        minimumZ + static_cast<double>(z) * childSizeM,
                        childSizeM);
                }
            }
            return;
        }

        const double childSizeM = parent.cellSizeM * 0.5;
        for (int z = 0; z < 2; ++z)
        {
            for (int x = 0; x < 2; ++x)
            {
                appendChild(
                    minimumX + static_cast<double>(x) * childSizeM,
                    minimumZ + static_cast<double>(z) * childSizeM,
                    childSizeM);
            }
        }
    };

    for (std::size_t balancePass = 0u;
         balancePass < kMaximumBalancePasses; ++balancePass)
    {
        constexpr std::size_t kNoOwner = static_cast<std::size_t>(-1);
        std::vector<std::size_t> ownerBySupport(m_cells.size(), kNoOwner);
        for (std::size_t adaptiveIndex = 0u;
             adaptiveIndex < m_adaptiveCells.size(); ++adaptiveIndex)
        {
            const AdaptiveCell& cell = m_adaptiveCells[adaptiveIndex];
            if (cell.cellSizeM < supportSizeM - 1.0e-9)
                continue;
            const std::int64_t minimumSupportX = static_cast<std::int64_t>(
                std::llround(static_cast<double>(cell.minimumXUnits)
                    * kAdaptiveUnitM / supportSizeM));
            const std::int64_t minimumSupportZ = static_cast<std::int64_t>(
                std::llround(static_cast<double>(cell.minimumZUnits)
                    * kAdaptiveUnitM / supportSizeM));
            const int supportSpan = std::max(1, static_cast<int>(std::llround(
                cell.cellSizeM / supportSizeM)));
            for (int dz = 0; dz < supportSpan; ++dz)
            {
                for (int dx = 0; dx < supportSpan; ++dx)
                {
                    const std::int32_t index = supportIndex(
                        minimumSupportX + dx, minimumSupportZ + dz, cell.layer);
                    if (index >= 0)
                    {
                        ownerBySupport[static_cast<std::size_t>(index)]
                            = adaptiveIndex;
                    }
                }
            }
        }

        std::vector<std::uint8_t> splitCell(m_adaptiveCells.size(), 0u);
        const auto inspectNeighbour = [&](std::size_t adaptiveIndex,
                                          std::int64_t supportX,
                                          std::int64_t supportZ,
                                          std::int64_t layer) {
            const std::int32_t support = supportIndex(supportX, supportZ, layer);
            if (support < 0)
                return;
            const std::size_t neighbourOwner =
                ownerBySupport[static_cast<std::size_t>(support)];
            if (neighbourOwner == kNoOwner || neighbourOwner == adaptiveIndex)
                return;
            const AdaptiveCell& cell = m_adaptiveCells[adaptiveIndex];
            const AdaptiveCell& neighbour = m_adaptiveCells[neighbourOwner];
            if (neighbour.cellSizeM < supportSizeM - 1.0e-9)
                return;
            if (cell.cellSizeM > neighbour.cellSizeM * 2.0 + 1.0e-9
                && cell.cellSizeM > supportSizeM + 1.0e-9)
            {
                splitCell[adaptiveIndex] = 1u;
            }
        };

        for (std::size_t adaptiveIndex = 0u;
             adaptiveIndex < m_adaptiveCells.size(); ++adaptiveIndex)
        {
            const AdaptiveCell& cell = m_adaptiveCells[adaptiveIndex];
            if (cell.cellSizeM <= supportSizeM + 1.0e-9)
                continue;
            const std::int64_t minimumSupportX = static_cast<std::int64_t>(
                std::llround(static_cast<double>(cell.minimumXUnits)
                    * kAdaptiveUnitM / supportSizeM));
            const std::int64_t minimumSupportZ = static_cast<std::int64_t>(
                std::llround(static_cast<double>(cell.minimumZUnits)
                    * kAdaptiveUnitM / supportSizeM));
            const int supportSpan = std::max(1, static_cast<int>(std::llround(
                cell.cellSizeM / supportSizeM)));
            for (int along = 0; along < supportSpan; ++along)
            {
                inspectNeighbour(adaptiveIndex,
                    minimumSupportX - 1, minimumSupportZ + along, cell.layer);
                inspectNeighbour(adaptiveIndex,
                    minimumSupportX + supportSpan,
                    minimumSupportZ + along, cell.layer);
                inspectNeighbour(adaptiveIndex,
                    minimumSupportX + along, minimumSupportZ - 1, cell.layer);
                inspectNeighbour(adaptiveIndex,
                    minimumSupportX + along,
                    minimumSupportZ + supportSpan, cell.layer);
            }
        }

        if (std::none_of(splitCell.begin(), splitCell.end(),
                [](std::uint8_t split) { return split != 0u; }))
        {
            break;
        }

        std::vector<AdaptiveCell> balanced;
        balanced.reserve(m_adaptiveCells.size() * 2u);
        for (std::size_t adaptiveIndex = 0u;
             adaptiveIndex < m_adaptiveCells.size(); ++adaptiveIndex)
        {
            if (splitCell[adaptiveIndex] == 0u)
            {
                balanced.push_back(std::move(m_adaptiveCells[adaptiveIndex]));
                continue;
            }
            splitBalancedCell(m_adaptiveCells[adaptiveIndex], balanced);
        }
        m_adaptiveCells = std::move(balanced);
    }

    std::sort(
        m_adaptiveCells.begin(),
        m_adaptiveCells.end(),
        [](const AdaptiveCell& left, const AdaptiveCell& right) {
            if (left.minimumXUnits != right.minimumXUnits)
                return left.minimumXUnits < right.minimumXUnits;
            if (left.minimumZUnits != right.minimumZUnits)
                return left.minimumZUnits < right.minimumZUnits;
            if (left.layer != right.layer)
                return left.layer < right.layer;
            return left.spanUnits < right.spanUnits;
        });

    // Spatial bucket index. A large cell is inserted into every 20 m bucket it
    // overlaps so point queries never miss a coarse control volume near an edge.
    m_adaptiveSpatialBuckets.reserve(std::max<std::size_t>(
        m_adaptiveCells.size() / 32u, 16u));
    for (std::size_t i = 0; i < m_adaptiveCells.size(); ++i)
    {
        const AdaptiveCell& cell = m_adaptiveCells[i];
        const std::int64_t maximumXUnits = cell.minimumXUnits + cell.spanUnits - 1;
        const std::int64_t maximumZUnits = cell.minimumZUnits + cell.spanUnits - 1;
        const std::int64_t minimumBucketX = floorDivide(
            cell.minimumXUnits, kAdaptiveSpatialBucketUnits);
        const std::int64_t maximumBucketX = floorDivide(
            maximumXUnits, kAdaptiveSpatialBucketUnits);
        const std::int64_t minimumBucketZ = floorDivide(
            cell.minimumZUnits, kAdaptiveSpatialBucketUnits);
        const std::int64_t maximumBucketZ = floorDivide(
            maximumZUnits, kAdaptiveSpatialBucketUnits);
        for (std::int64_t bz = minimumBucketZ; bz <= maximumBucketZ; ++bz)
        {
            for (std::int64_t bx = minimumBucketX; bx <= maximumBucketX; ++bx)
            {
                m_adaptiveSpatialBuckets[{ bx, bz, 0 }].push_back(
                    static_cast<std::int32_t>(i));
            }
        }
    }
}

void SurfaceHydrology::rebuildAdaptivePipes()
{
    m_adaptivePipes.clear();
    for (AdaptiveCell& cell : m_adaptiveCells)
    {
        cell.pipeIndices.clear();
        cell.openBoundaryLengthM = cell.cellSizeM * 4.0;
        cell.boundaryOutflowM3ps = 0.0f;
    }
    if (m_adaptiveCells.empty())
        return;

    struct EdgeRef
    {
        std::int32_t cell = -1;
        std::int64_t start = 0;
        std::int64_t end = 0;
        std::int8_t side = 0; // -1 minimum edge, +1 maximum edge
    };
    std::unordered_map<std::int64_t, std::vector<EdgeRef>> vertical;
    std::unordered_map<std::int64_t, std::vector<EdgeRef>> horizontal;
    vertical.reserve(m_adaptiveCells.size() * 2u);
    horizontal.reserve(m_adaptiveCells.size() * 2u);
    for (std::size_t i = 0; i < m_adaptiveCells.size(); ++i)
    {
        const AdaptiveCell& cell = m_adaptiveCells[i];
        const std::int64_t minX = cell.minimumXUnits;
        const std::int64_t maxX = minX + cell.spanUnits;
        const std::int64_t minZ = cell.minimumZUnits;
        const std::int64_t maxZ = minZ + cell.spanUnits;
        vertical[minX].push_back({ static_cast<std::int32_t>(i), minZ, maxZ, -1 });
        vertical[maxX].push_back({ static_cast<std::int32_t>(i), minZ, maxZ, 1 });
        horizontal[minZ].push_back({ static_cast<std::int32_t>(i), minX, maxX, -1 });
        horizontal[maxZ].push_back({ static_cast<std::int32_t>(i), minX, maxX, 1 });
    }

    std::vector<double> coveredBoundaryM(m_adaptiveCells.size(), 0.0);
    const auto linkEdges = [&](auto& edgeMap, bool verticalEdge) {
        for (auto& [coordinate, edges] : edgeMap)
        {
            std::vector<EdgeRef> positive;
            std::vector<EdgeRef> negative;
            positive.reserve(edges.size());
            negative.reserve(edges.size());
            for (const EdgeRef& edge : edges)
            {
                if (edge.side > 0)
                    positive.push_back(edge);
                else
                    negative.push_back(edge);
            }
            std::sort(positive.begin(), positive.end(), [](const EdgeRef& a, const EdgeRef& b) {
                return std::tie(a.start, a.end, a.cell) < std::tie(b.start, b.end, b.cell);
            });
            std::sort(negative.begin(), negative.end(), [](const EdgeRef& a, const EdgeRef& b) {
                return std::tie(a.start, a.end, a.cell) < std::tie(b.start, b.end, b.cell);
            });

            for (const EdgeRef& plus : positive)
            {
                for (const EdgeRef& minus : negative)
                {
                    if (minus.end <= plus.start)
                        continue;
                    if (minus.start >= plus.end)
                        break;
                    if (plus.cell == minus.cell)
                        continue;
                    AdaptiveCell& a = m_adaptiveCells[
                        static_cast<std::size_t>(plus.cell)];
                    AdaptiveCell& b = m_adaptiveCells[
                        static_cast<std::size_t>(minus.cell)];
                    if (std::llabs(a.layer - b.layer) > 1)
                        continue;
                    const std::int64_t overlapStart = std::max(plus.start, minus.start);
                    const std::int64_t overlapEnd = std::min(plus.end, minus.end);
                    if (overlapEnd <= overlapStart)
                        continue;
                    const double overlapM = static_cast<double>(
                        overlapEnd - overlapStart) * kAdaptiveUnitM;
                    coveredBoundaryM[static_cast<std::size_t>(plus.cell)] += overlapM;
                    coveredBoundaryM[static_cast<std::size_t>(minus.cell)] += overlapM;

                    const double edgeCoordinateM = static_cast<double>(coordinate)
                        * kAdaptiveUnitM;
                    const double alongM = 0.5 * static_cast<double>(
                        overlapStart + overlapEnd) * kAdaptiveUnitM;
                    const double x = verticalEdge ? edgeCoordinateM : alongM;
                    const double z = verticalEdge ? alongM : edgeCoordinateM;
                    const double yA = adaptiveSurfaceElevationAt(a, x, z);
                    const double yB = adaptiveSurfaceElevationAt(b, x, z);
                    if (!std::isfinite(yA) || !std::isfinite(yB)
                        || std::abs(yA - yB) > m_description.maximumNeighbourStepM)
                    {
                        continue;
                    }

                    AdaptivePipe pipe;
                    pipe.cellA = plus.cell;
                    pipe.cellB = minus.cell;
                    pipe.edgeLengthM = static_cast<float>(overlapM);
                    pipe.centerDistanceM = static_cast<float>(std::max(
                        verticalEdge
                            ? std::abs(b.centerX - a.centerX)
                            : std::abs(b.centerZ - a.centerZ),
                        0.5 * (a.cellSizeM + b.cellSizeM) * 0.25));
                    pipe.directionX = verticalEdge ? 1.0f : 0.0f;
                    pipe.directionZ = verticalEdge ? 0.0f : 1.0f;
                    pipe.sillElevationM = std::max(yA, yB);
                    const std::int32_t pipeIndex = static_cast<std::int32_t>(
                        m_adaptivePipes.size());
                    m_adaptivePipes.push_back(pipe);
                    a.pipeIndices.push_back(pipeIndex);
                    b.pipeIndices.push_back(pipeIndex);
                }
            }
        }
    };
    linkEdges(vertical, true);
    linkEdges(horizontal, false);

    for (std::size_t i = 0; i < m_adaptiveCells.size(); ++i)
    {
        AdaptiveCell& cell = m_adaptiveCells[i];
        cell.openBoundaryLengthM = std::clamp(
            cell.cellSizeM * 4.0 - coveredBoundaryM[i],
            0.0,
            cell.cellSizeM * 4.0);
    }
}

std::int32_t SurfaceHydrology::findCellIndex(
    const heritage::math::DVec3& position) const
{
    if (!finitePosition(position) || m_cells.empty())
        return -1;
    const std::int64_t x = static_cast<std::int64_t>(
        std::floor(position.x / m_description.cellSizeM));
    const std::int64_t z = static_cast<std::int64_t>(
        std::floor(position.z / m_description.cellSizeM));
    const std::int64_t layer = static_cast<std::int64_t>(
        std::floor(position.y / m_description.verticalLayerSizeM));
    std::int32_t best = -1;
    double bestHeight = (std::numeric_limits<double>::max)();
    for (std::int64_t layerOffset = -2; layerOffset <= 2; ++layerOffset)
    {
        const auto found = m_lookup.find({ x, z, layer + layerOffset });
        if (found == m_lookup.end())
            continue;
        const double height = std::abs(m_cells[found->second].elevationM - position.y);
        if (height < bestHeight && height <= m_description.verticalLayerSizeM * 1.5)
        {
            best = found->second;
            bestHeight = height;
        }
    }
    return best;
}


std::int32_t SurfaceHydrology::findAdaptiveCellIndex(
    const heritage::math::DVec3& position) const
{
    if (!finitePosition(position) || m_adaptiveCells.empty())
        return -1;
    const CellKey bucketKey{
        static_cast<std::int64_t>(std::floor(
            position.x / kAdaptiveSpatialBucketM)),
        static_cast<std::int64_t>(std::floor(
            position.z / kAdaptiveSpatialBucketM)),
        0 };
    const auto bucket = m_adaptiveSpatialBuckets.find(bucketKey);
    if (bucket == m_adaptiveSpatialBuckets.end())
        return -1;

    std::int32_t best = -1;
    double bestHeight = (std::numeric_limits<double>::max)();
    for (const std::int32_t candidateIndex : bucket->second)
    {
        if (candidateIndex < 0)
            continue;
        const AdaptiveCell& cell = m_adaptiveCells[
            static_cast<std::size_t>(candidateIndex)];
        const double minimumX = static_cast<double>(cell.minimumXUnits)
            * kAdaptiveUnitM;
        const double minimumZ = static_cast<double>(cell.minimumZUnits)
            * kAdaptiveUnitM;
        const double maximumX = minimumX + cell.cellSizeM;
        const double maximumZ = minimumZ + cell.cellSizeM;
        constexpr double epsilon = 1.0e-7;
        if (position.x < minimumX - epsilon || position.x > maximumX + epsilon
            || position.z < minimumZ - epsilon || position.z > maximumZ + epsilon)
        {
            continue;
        }
        const double surfaceY = adaptiveSurfaceElevationAt(
            cell, position.x, position.z);
        const double height = std::abs(surfaceY - position.y);
        if (height < bestHeight
            && height <= m_description.verticalLayerSizeM * 1.5)
        {
            best = candidateIndex;
            bestHeight = height;
        }
    }
    return best;
}

SurfaceHydrologySample SurfaceHydrology::sample(
    const heritage::math::DVec3& globalPosition) const
{
    SurfaceHydrologySample result;
    const std::int32_t index = findAdaptiveCellIndex(globalPosition);
    if (index < 0)
        return result;
    const AdaptiveCell& cell = m_adaptiveCells[static_cast<std::size_t>(index)];
    const double depth = adaptiveWaterDepth(cell);
    result.valid = true;
    result.material = cell.material;
    result.surfaceElevationM = adaptiveSurfaceElevationAt(
        cell, globalPosition.x, globalPosition.z);
    result.waterDepthM = depth;
    result.wetness = smoothStep(0.00003, 0.00150, depth);
    result.flowVelocityXMps = cell.flowVelocityXMps;
    result.flowVelocityZMps = cell.flowVelocityZMps;
    return result;
}

bool SurfaceHydrology::hasPrecipitationCoverAbove(
    const heritage::math::DVec3& globalPosition,
    double minimumClearanceM,
    double maximumHeightM) const
{
    if (!finitePosition(globalPosition) || m_cells.empty()
        || !std::isfinite(minimumClearanceM)
        || !std::isfinite(maximumHeightM)
        || minimumClearanceM < 0.0
        || maximumHeightM <= minimumClearanceM)
    {
        return false;
    }

    const std::int64_t x = static_cast<std::int64_t>(
        std::floor(globalPosition.x / m_description.cellSizeM));
    const std::int64_t z = static_cast<std::int64_t>(
        std::floor(globalPosition.z / m_description.cellSizeM));
    const std::int64_t firstLayer = static_cast<std::int64_t>(
        std::floor((globalPosition.y + minimumClearanceM)
            / m_description.verticalLayerSizeM));
    const std::int64_t lastLayer = static_cast<std::int64_t>(
        std::floor((globalPosition.y + maximumHeightM)
            / m_description.verticalLayerSizeM));

    const double minimumElevation = globalPosition.y + minimumClearanceM;
    const double maximumElevation = globalPosition.y + maximumHeightM;
    for (std::int64_t layer = firstLayer; layer <= lastLayer; ++layer)
    {
        const auto found = m_lookup.find({ x, z, layer });
        if (found == m_lookup.end())
            continue;
        const Cell& cell = m_cells[static_cast<std::size_t>(found->second)];
        if (cell.elevationM >= minimumElevation
            && cell.elevationM <= maximumElevation
            && cell.normal.y > static_cast<float>(m_description.minimumUpwardNormal))
        {
            return true;
        }
    }
    return false;
}

SurfaceHydrology::CadenceBand SurfaceHydrology::cadenceBandForChunk(
    const SimulationChunk& chunk) const
{
    // Tests/tools that do not provide a runtime interest source preserve the
    // historical full-rate solver. Production gameplay supplies one or more
    // vehicle/player positions every frame.
    if (m_interestSources.empty())
        return CadenceBand::Near30Hz;

    const double chunkWorldSize = kAdaptiveSpatialBucketM;
    const double minimumX = static_cast<double>(chunk.key.x) * chunkWorldSize;
    const double maximumX = minimumX + chunkWorldSize;
    const double minimumZ = static_cast<double>(chunk.key.z) * chunkWorldSize;
    const double maximumZ = minimumZ + chunkWorldSize;

    double minimumDistanceSquared = (std::numeric_limits<double>::max)();
    for (const auto& source : m_interestSources)
    {
        // Distance from EACH source to this chunk's AABB. Taking the minimum
        // creates a union of influence regions. There is no midpoint/average
        // between split-screen or multiplayer players.
        const double dx = source.x < minimumX
            ? minimumX - source.x
            : (source.x > maximumX ? source.x - maximumX : 0.0);
        const double dz = source.z < minimumZ
            ? minimumZ - source.z
            : (source.z > maximumZ ? source.z - maximumZ : 0.0);
        minimumDistanceSquared = std::min(
            minimumDistanceSquared, dx * dx + dz * dz);
    }

    const double nearSquared = m_description.nearCadenceRadiusM
        * m_description.nearCadenceRadiusM;
    const double mediumSquared = m_description.mediumCadenceRadiusM
        * m_description.mediumCadenceRadiusM;
    const double farSquared = m_description.farCadenceRadiusM
        * m_description.farCadenceRadiusM;
    const double distantSquared = m_description.distantCadenceRadiusM
        * m_description.distantCadenceRadiusM;
    if (minimumDistanceSquared <= nearSquared)
        return CadenceBand::Near30Hz;
    if (minimumDistanceSquared <= mediumSquared)
        return CadenceBand::Medium20Hz;
    if (minimumDistanceSquared <= farSquared)
        return CadenceBand::Far6Hz;
    if (minimumDistanceSquared <= distantSquared)
        return CadenceBand::Distant2Hz;
    return CadenceBand::Background;
}

double SurfaceHydrology::cadenceHz(CadenceBand band) const
{
    switch (band)
    {
    case CadenceBand::Near30Hz:
        return m_description.nearCadenceHz;
    case CadenceBand::Medium20Hz:
        return m_description.mediumCadenceHz;
    case CadenceBand::Far6Hz:
        return m_description.farCadenceHz;
    case CadenceBand::Distant2Hz:
        return m_description.distantCadenceHz;
    case CadenceBand::Background:
    default:
        return m_description.backgroundCadenceHz;
    }
}

void SurfaceHydrology::rebuildDueCells(double baseStepSeconds)
{
    m_dueCells.clear();
    std::fill(m_dueDeltaTimeByCell.begin(), m_dueDeltaTimeByCell.end(), 0.0f);

    m_stats.interestSourceCount = m_interestSources.size();
    m_stats.cadence30HzCellCount = 0u;
    m_stats.cadence20HzCellCount = 0u;
    m_stats.cadence6HzCellCount = 0u;
    m_stats.cadence2HzCellCount = 0u;
    m_stats.cadenceBackgroundCellCount = 0u;

    for (SimulationChunk& chunk : m_simulationChunks)
    {
        const CadenceBand band = cadenceBandForChunk(chunk);
        chunk.cadenceBand = band;
        switch (band)
        {
        case CadenceBand::Near30Hz:
            m_stats.cadence30HzCellCount += chunk.cellIndices.size();
            break;
        case CadenceBand::Medium20Hz:
            m_stats.cadence20HzCellCount += chunk.cellIndices.size();
            break;
        case CadenceBand::Far6Hz:
            m_stats.cadence6HzCellCount += chunk.cellIndices.size();
            break;
        case CadenceBand::Distant2Hz:
            m_stats.cadence2HzCellCount += chunk.cellIndices.size();
            break;
        case CadenceBand::Background:
            m_stats.cadenceBackgroundCellCount += chunk.cellIndices.size();
            break;
        }

        const double hz = std::max(cadenceHz(band), 0.1);
        const double period = 1.0 / hz;
        if (!chunk.cadenceInitialized)
        {
            double initialDelay = 0.0;
            if (band != CadenceBand::Near30Hz)
            {
                const std::size_t hash = CellKeyHash{}(chunk.key);
                const double phase = static_cast<double>(hash & 0xffffu)
                    / 65536.0;
                initialDelay = phase * period;
            }
            chunk.cadenceAccumulatorSeconds = -initialDelay;
            chunk.elapsedSinceUpdateSeconds = 0.0;
            chunk.cadenceInitialized = true;
        }

        chunk.cadenceAccumulatorSeconds += baseStepSeconds;
        chunk.elapsedSinceUpdateSeconds += baseStepSeconds;
        if (chunk.cadenceAccumulatorSeconds + 1.0e-12 < period)
            continue;

        chunk.cadenceAccumulatorSeconds -= period;
        const double dueDt = std::clamp(
            chunk.elapsedSinceUpdateSeconds, baseStepSeconds, 2.5);
        chunk.elapsedSinceUpdateSeconds = 0.0;
        const float dueDtF = static_cast<float>(dueDt);
        for (const std::int32_t cellIndex : chunk.cellIndices)
        {
            if (cellIndex < 0)
                continue;
            const std::size_t index = static_cast<std::size_t>(cellIndex);
            if (index >= m_adaptiveCells.size())
                continue;
            m_dueCells.push_back({ cellIndex, dueDtF });
            m_dueDeltaTimeByCell[index] = std::max(
                m_dueDeltaTimeByCell[index], dueDtF);
        }
    }
    m_stats.lastScheduledCellCount = m_dueCells.size();
}

void SurfaceHydrology::advance(
    const SurfaceWeatherDescription& weather,
    const SurfaceWeatherOutput& weatherOutput,
    double deltaTimeSeconds)
{
    if (m_adaptiveCells.empty() || !weather.enabled
        || !std::isfinite(deltaTimeSeconds) || deltaTimeSeconds <= 0.0)
    {
        return;
    }
    m_lastPresentationFilmDepthM = weatherOutput.valid
        ? std::clamp(weatherOutput.waterFilmDepthM, 0.0, 0.006)
        : 0.0;
    m_presentationBasinRefreshAccumulatorSeconds += std::min(
        deltaTimeSeconds, 1.0);
    m_stepAccumulatorSeconds += std::min(deltaTimeSeconds, 5.0);
    const double step = 1.0 / m_description.updateRateHz;
    constexpr double kStatsRefreshPeriodSeconds = 0.5; // diagnostics/Lua only
    while (m_stepAccumulatorSeconds + 1.0e-12 >= step)
    {
        rebuildDueCells(step);
        m_statsRefreshAccumulatorSeconds += step;
        const bool refreshStatistics =
            m_statsRefreshAccumulatorSeconds + 1.0e-12
                >= kStatsRefreshPeriodSeconds;
        if (refreshStatistics)
        {
            m_statsRefreshAccumulatorSeconds = std::fmod(
                m_statsRefreshAccumulatorSeconds,
                kStatsRefreshPeriodSeconds);
        }
        if (!m_dueCells.empty())
            simulateStep(weather, weatherOutput, step, refreshStatistics);
        else if (refreshStatistics)
            refreshStats();
        m_stepAccumulatorSeconds -= step;
    }

    // WATER16 presentation basins intentionally run at a modest 10 Hz. The
    // adaptive solver can stay at its 30/20/6/2/0.5 Hz cadence while visible
    // puddle free surfaces evolve smoothly without an O(N) basin reduction on
    // every physics substep.
    constexpr double kPresentationBasinRefreshPeriodSeconds = 0.10;
    if (m_presentationBasinRefreshAccumulatorSeconds + 1.0e-12
        >= kPresentationBasinRefreshPeriodSeconds)
    {
        m_presentationBasinRefreshAccumulatorSeconds = std::fmod(
            m_presentationBasinRefreshAccumulatorSeconds,
            kPresentationBasinRefreshPeriodSeconds);
        refreshPresentationBasinLevels(m_lastPresentationFilmDepthM);
    }
}

void SurfaceHydrology::simulateStep(
    const SurfaceWeatherDescription& weather,
    const SurfaceWeatherOutput& weatherOutput,
    double baseDt,
    bool refreshStatistics)
{
    const auto started = std::chrono::steady_clock::now();
    const double rainRateMps = mmPerHourToMps(weather.precipitationRateMmPerHour);
    const double evaporationRateMps = weatherOutput.valid
        ? mmPerHourToMps(weatherOutput.evaporationRateMmPerHour) : 0.0;

    const std::size_t weatherRangeCount = rangeCountFor(
        m_dueCells.size(), kHydrologyCellGrain);
    m_weatherReductionScratch.resize(
        std::max(weatherRangeCount, std::size_t{ 1u }));
    const auto weatherPhase = [&](std::size_t begin,
                                  std::size_t end,
                                  std::size_t rangeIndex) {
        WeatherReduction local;
        for (std::size_t dueIndex = begin; dueIndex < end; ++dueIndex)
        {
            const DueCell& due = m_dueCells[dueIndex];
            if (due.index < 0)
                continue;
            AdaptiveCell& cell = m_adaptiveCells[
                static_cast<std::size_t>(due.index)];
            const double dt = static_cast<double>(due.deltaTimeSeconds);
            const double area = cell.areaM2;
            double depth = adaptiveWaterDepth(cell);
            const double rainDepth = cell.precipitationExposed
                ? rainRateMps * dt
                : 0.0;
            const double rainVolume = rainDepth * area;
            cell.waterVolumeM3 += rainVolume;
            local.rainVolumeM3 += rainVolume;
            depth += rainDepth;

            const double infiltrationDepth = std::min(
                depth,
                mmPerHourToMps(cell.infiltrationMmPerHour) * dt);
            const double infiltrationVolume = infiltrationDepth * area;
            cell.waterVolumeM3 = std::max(
                cell.waterVolumeM3 - infiltrationVolume, 0.0);
            local.infiltrationVolumeM3 += infiltrationVolume;
            depth = adaptiveWaterDepth(cell);

            const double drainageDepth = std::min(
                depth,
                mmPerHourToMps(cell.drainageMmPerHour) * dt);
            const double drainageVolume = drainageDepth * area;
            cell.waterVolumeM3 = std::max(
                cell.waterVolumeM3 - drainageVolume, 0.0);
            local.drainageVolumeM3 += drainageVolume;
            depth = adaptiveWaterDepth(cell);

            const double evaporationDepth = std::min(
                depth,
                evaporationRateMps * dt);
            const double evaporationVolume = evaporationDepth * area;
            cell.waterVolumeM3 = std::max(
                cell.waterVolumeM3 - evaporationVolume, 0.0);
            local.evaporationVolumeM3 += evaporationVolume;
        }
        if (rangeIndex < m_weatherReductionScratch.size())
            m_weatherReductionScratch[rangeIndex] = local;
    };

    if (!m_dueCells.empty())
    {
        if (m_jobSystem)
            m_jobSystem->parallelFor(
                m_dueCells.size(), kHydrologyCellGrain, weatherPhase);
        else
            weatherPhase(0u, m_dueCells.size(), 0u);
    }
    for (std::size_t range = 0; range < weatherRangeCount; ++range)
    {
        m_stats.cumulativeRainVolumeM3 +=
            m_weatherReductionScratch[range].rainVolumeM3;
        m_stats.cumulativeInfiltrationVolumeM3 +=
            m_weatherReductionScratch[range].infiltrationVolumeM3;
        m_stats.cumulativeDrainageVolumeM3 +=
            m_weatherReductionScratch[range].drainageVolumeM3;
        m_stats.cumulativeEvaporationVolumeM3 +=
            m_weatherReductionScratch[range].evaporationVolumeM3;
    }

    // WATER14 phase 1: evaluate each unequal-cell face independently. This
    // stage reads cell volume/head only and writes its own pipe record, so it
    // parallelizes without atomics or fixed-grid graph coloring.
    const auto pipePhase = [&](std::size_t begin,
                               std::size_t end,
                               std::size_t) {
        for (std::size_t pipeIndex = begin; pipeIndex < end; ++pipeIndex)
        {
            AdaptivePipe& pipe = m_adaptivePipes[pipeIndex];
            pipe.transferredVolumeM3 = 0.0;
            if (pipe.cellA < 0 || pipe.cellB < 0)
            {
                pipe.deltaTimeSeconds = 0.0f;
                pipe.candidateFluxM3ps = 0.0f;
                continue;
            }
            const std::size_t aIndex = static_cast<std::size_t>(pipe.cellA);
            const std::size_t bIndex = static_cast<std::size_t>(pipe.cellB);
            const float dtF = std::max(
                m_dueDeltaTimeByCell[aIndex],
                m_dueDeltaTimeByCell[bIndex]);
            if (dtF <= 0.0f)
            {
                pipe.deltaTimeSeconds = 0.0f;
                pipe.candidateFluxM3ps = pipe.fluxM3ps;
                continue;
            }

            const AdaptiveCell& a = m_adaptiveCells[aIndex];
            const AdaptiveCell& b = m_adaptiveCells[bIndex];
            const double depthA = adaptiveWaterDepth(a);
            const double depthB = adaptiveWaterDepth(b);
            const double mobileA = std::max(
                depthA - a.depressionStorageM, 0.0);
            const double mobileB = std::max(
                depthB - b.depressionStorageM, 0.0);
            const double headA = a.elevationM + depthA;
            const double headB = b.elevationM + depthB;
            const double headDifference = headA - headB;
            double availableDepth = 0.0;
            if (headDifference >= 0.0)
                availableDepth = std::min(
                    mobileA, std::max(headA - pipe.sillElevationM, 0.0));
            else
                availableDepth = std::min(
                    mobileB, std::max(headB - pipe.sillElevationM, 0.0));

            detail::AdaptiveVirtualPipeFluxInput input;
            input.previousFluxM3ps = pipe.fluxM3ps;
            input.hydraulicHeadDifferenceM = headDifference;
            input.availableFlowDepthM = availableDepth;
            input.edgeLengthM = pipe.edgeLengthM;
            input.centerDistanceM = pipe.centerDistanceM;
            input.deltaTimeSeconds = static_cast<double>(dtF);
            input.roughness = 0.5 * (a.roughness + b.roughness);
            input.conductance = m_description.flowCoefficient;
            input.gravityMps2 = kGravityMps2;
            double candidate = detail::integrateAdaptiveVirtualPipeFlux(input);
            if (availableDepth <= 1.0e-9)
                candidate *= 1.0 / (1.0 + 5.0 * input.deltaTimeSeconds);
            if (!std::isfinite(candidate))
                candidate = 0.0;
            pipe.candidateFluxM3ps = static_cast<float>(candidate);
            pipe.deltaTimeSeconds = dtF;
        }
    };
    if (!m_adaptivePipes.empty())
    {
        if (m_jobSystem)
            m_jobSystem->parallelFor(
                m_adaptivePipes.size(), kHydrologyFlowGrain, pipePhase);
        else
            pipePhase(0u, m_adaptivePipes.size(), 0u);
    }

    // WATER14 phase 2: compute one conservative outflow scale per adaptive
    // control volume. A coarse 20 m cell and a 0.1 m cell therefore obey the
    // same mass law despite radically different area/face sizes.
    for (std::size_t i = 0; i < m_adaptiveCells.size(); ++i)
    {
        AdaptiveCell& cell = m_adaptiveCells[i];
        const double depth = adaptiveWaterDepth(cell);
        const double mobileVolume = std::max(
            depth - cell.depressionStorageM, 0.0) * cell.areaM2;
        double requestedOutflowVolume = 0.0;
        for (const std::int32_t pipeIndexValue : cell.pipeIndices)
        {
            if (pipeIndexValue < 0)
                continue;
            const AdaptivePipe& pipe = m_adaptivePipes[
                static_cast<std::size_t>(pipeIndexValue)];
            if (pipe.deltaTimeSeconds <= 0.0f)
                continue;
            const double candidate = pipe.candidateFluxM3ps;
            const bool outgoing = (pipe.cellA == static_cast<std::int32_t>(i)
                    && candidate > 0.0)
                || (pipe.cellB == static_cast<std::int32_t>(i)
                    && candidate < 0.0);
            if (outgoing)
            {
                requestedOutflowVolume += std::abs(candidate)
                    * static_cast<double>(pipe.deltaTimeSeconds);
            }
        }

        double boundaryCandidate = 0.0;
        const double dt = static_cast<double>(m_dueDeltaTimeByCell[i]);
        if (dt > 0.0 && cell.openBoundaryLengthM > 1.0e-6
            && mobileVolume > 1.0e-12)
        {
            detail::AdaptiveVirtualPipeFluxInput boundary;
            boundary.previousFluxM3ps = cell.boundaryOutflowM3ps;
            boundary.hydraulicHeadDifferenceM = std::max(
                depth - cell.depressionStorageM, 0.0);
            boundary.availableFlowDepthM = boundary.hydraulicHeadDifferenceM;
            boundary.edgeLengthM = cell.openBoundaryLengthM;
            boundary.centerDistanceM = std::max(cell.cellSizeM * 0.5, 0.05);
            boundary.deltaTimeSeconds = dt;
            boundary.roughness = cell.roughness;
            boundary.conductance = m_description.flowCoefficient * 0.30;
            boundary.gravityMps2 = kGravityMps2;
            boundaryCandidate = std::max(
                detail::integrateAdaptiveVirtualPipeFlux(boundary), 0.0);
            requestedOutflowVolume += boundaryCandidate * dt;
        }
        m_boundaryCandidateFluxByCell[i] = boundaryCandidate;
        m_outflowScaleByCell[i] = requestedOutflowVolume > 1.0e-15
            ? std::min(mobileVolume / requestedOutflowVolume, 1.0)
            : 1.0;
    }

    // WATER14 phase 3: source-volume scales are known, so each pipe can commit
    // one signed conservative transfer without any shared writes.
    const auto commitPipePhase = [&](std::size_t begin,
                                     std::size_t end,
                                     std::size_t) {
        for (std::size_t pipeIndex = begin; pipeIndex < end; ++pipeIndex)
        {
            AdaptivePipe& pipe = m_adaptivePipes[pipeIndex];
            if (pipe.deltaTimeSeconds <= 0.0f)
                continue;
            const double candidate = pipe.candidateFluxM3ps;
            double scale = 1.0;
            if (candidate > 0.0)
                scale = m_outflowScaleByCell[
                    static_cast<std::size_t>(pipe.cellA)];
            else if (candidate < 0.0)
                scale = m_outflowScaleByCell[
                    static_cast<std::size_t>(pipe.cellB)];
            const double flux = candidate * scale;
            pipe.fluxM3ps = static_cast<float>(flux);
            pipe.transferredVolumeM3 = flux
                * static_cast<double>(pipe.deltaTimeSeconds);
        }
    };
    if (!m_adaptivePipes.empty())
    {
        if (m_jobSystem)
            m_jobSystem->parallelFor(
                m_adaptivePipes.size(), kHydrologyFlowGrain, commitPipePhase);
        else
            commitPipePhase(0u, m_adaptivePipes.size(), 0u);
    }

    double runoffVolumeM3 = 0.0;
    for (std::size_t i = 0; i < m_adaptiveCells.size(); ++i)
    {
        AdaptiveCell& cell = m_adaptiveCells[i];
        double deltaVolume = 0.0;
        double netFluxX = 0.0;
        double netFluxZ = 0.0;
        for (const std::int32_t pipeIndexValue : cell.pipeIndices)
        {
            if (pipeIndexValue < 0)
                continue;
            const AdaptivePipe& pipe = m_adaptivePipes[
                static_cast<std::size_t>(pipeIndexValue)];
            if (pipe.cellA == static_cast<std::int32_t>(i))
                deltaVolume -= pipe.transferredVolumeM3;
            else if (pipe.cellB == static_cast<std::int32_t>(i))
                deltaVolume += pipe.transferredVolumeM3;
            netFluxX += static_cast<double>(pipe.fluxM3ps)
                * static_cast<double>(pipe.directionX);
            netFluxZ += static_cast<double>(pipe.fluxM3ps)
                * static_cast<double>(pipe.directionZ);
        }

        const double dt = static_cast<double>(m_dueDeltaTimeByCell[i]);
        if (dt > 0.0)
        {
            const double boundaryFlux = m_boundaryCandidateFluxByCell[i]
                * m_outflowScaleByCell[i];
            cell.boundaryOutflowM3ps = static_cast<float>(boundaryFlux);
            const double boundaryVolume = boundaryFlux * dt;
            deltaVolume -= boundaryVolume;
            runoffVolumeM3 += boundaryVolume;
        }

        cell.waterVolumeM3 = std::max(cell.waterVolumeM3 + deltaVolume, 0.0);
        const double maximumVolume = m_description.maximumWaterDepthM
            * cell.areaM2;
        if (cell.waterVolumeM3 > maximumVolume)
        {
            runoffVolumeM3 += cell.waterVolumeM3 - maximumVolume;
            cell.waterVolumeM3 = maximumVolume;
        }

        const double depth = adaptiveWaterDepth(cell);
        const double targetVx = detail::velocityFromNetFlux(
            netFluxX, cell.cellSizeM, depth);
        const double targetVz = detail::velocityFromNetFlux(
            netFluxZ, cell.cellSizeM, depth);
        const double responseDt = dt > 0.0 ? dt : baseDt;
        const double response = std::clamp(
            (6.0 * responseDt) / (1.0 + 6.0 * responseDt), 0.0, 1.0);
        cell.flowVelocityXMps +=
            (targetVx - cell.flowVelocityXMps) * response;
        cell.flowVelocityZMps +=
            (targetVz - cell.flowVelocityZMps) * response;
    }
    m_stats.cumulativeRunoffVolumeM3 += runoffVolumeM3;

    ++m_stats.simulationStepCount;
    if (refreshStatistics)
        refreshStats();
    m_stats.lastStepMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
}

SurfaceHydrologyTireResult SurfaceHydrology::applyTireContact(
    const heritage::math::DVec3& globalPosition,
    const SurfaceHydrologyTireInput& input)
{
    SurfaceHydrologyTireResult result;
    const std::int32_t index = findAdaptiveCellIndex(globalPosition);
    if (index < 0 || !std::isfinite(input.deltaTimeSeconds)
        || input.deltaTimeSeconds <= 0.0)
    {
        return result;
    }

    AdaptiveCell& cell = m_adaptiveCells[static_cast<std::size_t>(index)];
    const double initialDepth = adaptiveWaterDepth(cell);
    result.valid = true;
    result.initialWaterDepthM = initialDepth;
    ++m_stats.tireContactCount;
    if (initialDepth <= 1.0e-9)
    {
        result.finalWaterDepthM = initialDepth;
        return result;
    }

    const double area = cell.areaM2;
    const double width = std::clamp(input.contactPatchWidthM, 0.03, 1.5);
    const double travel = (std::abs(input.forwardSpeedMps)
        + 0.35 * std::abs(input.lateralSpeedMps)) * input.deltaTimeSeconds;
    const double sweptCoverage = std::clamp(width * travel / area, 0.0, 1.0);
    const double loadScale = std::clamp(
        input.normalLoadN / std::max(input.nominalLoadN, 100.0), 0.15, 3.0);
    const double treadScale = std::clamp(
        0.25 + 1.60 * input.treadVoidRatio, 0.25, 1.60);
    const double clearingFraction = std::clamp(
        1.0 - std::exp(-1.30 * sweptCoverage * loadScale * treadScale),
        0.0, 0.55);
    const double displacedVolume = cell.waterVolumeM3 * clearingFraction;

    constexpr double latentHeatJPerKg = 2260000.0;
    constexpr double waterDensityKgM3 = 997.0;
    const double frictionEvaporation = std::min(
        std::max(cell.waterVolumeM3 - displacedVolume, 0.0),
        std::max(input.slipDissipationWatts, 0.0)
            * input.deltaTimeSeconds * 0.02
            / (latentHeatJPerKg * waterDensityKgM3));
    const double removedVolume = displacedVolume + frictionEvaporation;
    cell.waterVolumeM3 = std::max(cell.waterVolumeM3 - removedVolume, 0.0);

    // Redistribute toward the pipe whose geometric direction best matches the
    // tire's forward motion. This remains conservative across unequal adaptive
    // cells; the target converts the received volume using its own area.
    double redistributed = 0.0;
    const double redistributionBudget = displacedVolume * 0.34;
    if (redistributionBudget > 0.0)
    {
        const double fx = input.forward.x;
        const double fz = input.forward.z;
        double bestDot = -2.0;
        std::int32_t bestTarget = -1;
        for (const std::int32_t pipeIndexValue : cell.pipeIndices)
        {
            if (pipeIndexValue < 0)
                continue;
            const AdaptivePipe& pipe = m_adaptivePipes[
                static_cast<std::size_t>(pipeIndexValue)];
            std::int32_t target = -1;
            double directionX = 0.0;
            double directionZ = 0.0;
            if (pipe.cellA == index)
            {
                target = pipe.cellB;
                directionX = pipe.directionX;
                directionZ = pipe.directionZ;
            }
            else if (pipe.cellB == index)
            {
                target = pipe.cellA;
                directionX = -pipe.directionX;
                directionZ = -pipe.directionZ;
            }
            if (target < 0)
                continue;
            const double dot = fx * directionX + fz * directionZ;
            if (dot > bestDot)
            {
                bestDot = dot;
                bestTarget = target;
            }
        }
        if (bestTarget >= 0)
        {
            AdaptiveCell& target = m_adaptiveCells[
                static_cast<std::size_t>(bestTarget)];
            const double targetMaximumVolume =
                m_description.maximumWaterDepthM * target.areaM2;
            const double capacity = std::max(
                targetMaximumVolume - target.waterVolumeM3, 0.0);
            redistributed = std::min(redistributionBudget, capacity);
            target.waterVolumeM3 += redistributed;
        }
    }

    result.removedVolumeM3 = removedVolume;
    result.redistributedVolumeM3 = redistributed;
    result.sprayVolumeM3 = std::max(displacedVolume - redistributed, 0.0);
    result.frictionEvaporatedVolumeM3 = frictionEvaporation;
    result.finalWaterDepthM = adaptiveWaterDepth(cell);
    m_stats.cumulativeTireClearedVolumeM3 += removedVolume;
    m_stats.cumulativeTireSprayVolumeM3 += result.sprayVolumeM3;
    return result;
}

void SurfaceHydrology::refreshStats()
{
    std::size_t wetCells = 0u;
    double volume = 0.0;
    double maximumDepth = 0.0;
    double minimumCellSize = (std::numeric_limits<double>::max)();
    double maximumCellSize = 0.0;
    std::size_t subDecimetreCells = 0u;
    std::size_t largeCells = 0u;
    for (const AdaptiveCell& cell : m_adaptiveCells)
    {
        const double depth = adaptiveWaterDepth(cell);
        if (depth > 0.00001)
            ++wetCells;
        volume += cell.waterVolumeM3;
        maximumDepth = std::max(maximumDepth, depth);
        minimumCellSize = std::min(minimumCellSize, cell.cellSizeM);
        maximumCellSize = std::max(maximumCellSize, cell.cellSizeM);
        if (cell.cellSizeM <= 0.100001)
            ++subDecimetreCells;
        if (cell.cellSizeM >= 5.0)
            ++largeCells;
    }

    std::size_t activeVirtualPipes = 0u;
    double maximumVirtualPipeFluxM3ps = 0.0;
    for (const AdaptivePipe& pipe : m_adaptivePipes)
    {
        const double flux = std::abs(static_cast<double>(pipe.fluxM3ps));
        if (flux > 1.0e-10)
            ++activeVirtualPipes;
        maximumVirtualPipeFluxM3ps = std::max(
            maximumVirtualPipeFluxM3ps, flux);
    }
    for (const AdaptiveCell& cell : m_adaptiveCells)
    {
        const double flux = std::abs(
            static_cast<double>(cell.boundaryOutflowM3ps));
        if (flux > 1.0e-10)
            ++activeVirtualPipes;
        maximumVirtualPipeFluxM3ps = std::max(
            maximumVirtualPipeFluxM3ps, flux);
    }

    m_stats.available = !m_adaptiveCells.empty();
    m_stats.supportCellCount = m_cells.size();
    m_stats.cellCount = m_adaptiveCells.size();
    m_stats.wetCellCount = wetCells;
    m_stats.waterVolumeM3 = volume;
    m_stats.maximumWaterDepthM = maximumDepth;
    m_stats.adaptiveMinimumCellSizeM = m_adaptiveCells.empty()
        ? 0.0 : minimumCellSize;
    m_stats.adaptiveMaximumCellSizeM = maximumCellSize;
    m_stats.adaptiveSubDecimetreCellCount = subDecimetreCells;
    m_stats.adaptiveLargeCellCount = largeCells;
    m_stats.activeVirtualPipeCount = activeVirtualPipes;
    m_stats.maximumVirtualPipeFluxM3ps = maximumVirtualPipeFluxM3ps;
    m_stats.interestSourceCount = m_interestSources.size();
    m_stats.updateRateHz = m_description.updateRateHz;
    m_stats.debugVisualizationEnabled = m_debugVisualizationEnabled;
    m_stats.presentationBasinCount = m_presentationBasins.size();
    m_stats.activePresentationBasinCount = static_cast<std::size_t>(
        std::count_if(
            m_presentationBasins.begin(),
            m_presentationBasins.end(),
            [](const PresentationBasin& basin) {
                return basin.waterSurfaceElevationM > -16000.0;
            }));
}

void SurfaceHydrology::collectVisualCells(
    const heritage::math::DVec3& center,
    double radiusM,
    std::size_t maximumCells,
    std::vector<SurfaceHydrologyVisualCell>& output,
    double,
    bool includeDryCells) const
{
    collectVisualCellsBand(
        center,
        0.0,
        radiusM,
        maximumCells,
        output,
        includeDryCells,
        0.00001);
}


void SurfaceHydrology::collectPresentationBasinCellsBand(
    const heritage::math::DVec3& center,
    double minimumRadiusM,
    double maximumRadiusM,
    std::size_t maximumCells,
    std::vector<SurfaceHydrologyVisualCell>& output,
    double shorelineInfluenceM) const
{
    output.clear();
    if (!finitePosition(center)
        || !std::isfinite(minimumRadiusM)
        || !std::isfinite(maximumRadiusM)
        || maximumRadiusM <= 0.0
        || maximumRadiusM <= minimumRadiusM
        || maximumCells == 0u)
    {
        return;
    }

    const double minimumRadiusSquared = std::max(minimumRadiusM, 0.0)
        * std::max(minimumRadiusM, 0.0);
    const double maximumRadiusSquared = maximumRadiusM * maximumRadiusM;
    (void)shorelineInfluenceM;

    // WATER17: presentation is parameterized from the immutable 0.50 m support
    // raster, NEVER from the adaptive 0.10..20 m solver leaves.  The solver may
    // merge/split for performance without changing the optical sampling lattice.
    // This removes the last direct path by which adaptive rectangles could leak
    // into puddle silhouettes.
    output.reserve(std::min(maximumCells, m_cells.size()));
    const double supportSizeM = m_description.cellSizeM;
    const double half = supportSizeM * 0.5;

    constexpr std::array<std::array<double, 2>, 4> signs{{
        {{ -1.0, -1.0 }}, {{ 1.0, -1.0 }},
        {{ -1.0, 1.0 }}, {{ 1.0, 1.0 }}
    }};

    const auto supportSurfaceAt = [](const Cell& cell,
                                     double centerX,
                                     double centerZ,
                                     double x,
                                     double z) {
        const double nx = static_cast<double>(cell.normal.x);
        const double ny = static_cast<double>(cell.normal.y);
        const double nz = static_cast<double>(cell.normal.z);
        if (!std::isfinite(ny) || std::abs(ny) <= 1.0e-8)
            return cell.elevationM;
        return cell.elevationM
            - (nx * (x - centerX) + nz * (z - centerZ)) / ny;
    };

    for (const Cell& cell : m_cells)
    {
        if (cell.presentationBasinId < 0
            || static_cast<std::size_t>(cell.presentationBasinId)
                >= m_presentationBasins.size())
        {
            continue;
        }
        const PresentationBasin& basin = m_presentationBasins[
            static_cast<std::size_t>(cell.presentationBasinId)];
        const double waterLevelY = basin.waterSurfaceElevationM;
        if (!std::isfinite(waterLevelY) || waterLevelY <= -16000.0)
            continue;

        const double centerX =
            (static_cast<double>(cell.key.x) + 0.5) * supportSizeM;
        const double centerZ =
            (static_cast<double>(cell.key.z) + 0.5) * supportSizeM;

        const double dx = center.x < centerX - half
            ? (centerX - half) - center.x
            : (center.x > centerX + half
                ? center.x - (centerX + half) : 0.0);
        const double dz = center.z < centerZ - half
            ? (centerZ - half) - center.z
            : (center.z > centerZ + half
                ? center.z - (centerZ + half) : 0.0);
        const double distanceSquared = dx * dx + dz * dz;
        if (distanceSquared < minimumRadiusSquared
            || distanceSquared > maximumRadiusSquared)
        {
            continue;
        }

        SurfaceHydrologyVisualCell visual;
        visual.globalPosition = { centerX, waterLevelY, centerZ };
        visual.normal = cell.normal;
        visual.material = cell.material;
        visual.cellSizeM = supportSizeM;
        visual.surfaceElevationM = cell.elevationM;
        visual.presentationBasinId = cell.presentationBasinId;
        visual.presentationLayer = cell.key.layer;

        double maximumCornerDepthM = 0.0;
        for (std::size_t corner = 0; corner < signs.size(); ++corner)
        {
            const double x = centerX + signs[corner][0] * half;
            const double z = centerZ + signs[corner][1] * half;
            const double surfaceY = supportSurfaceAt(
                cell, centerX, centerZ, x, z);
            const double depthM = std::max(waterLevelY - surfaceY, 0.0);
            visual.cornerSurfaceElevationM[corner] = surfaceY;
            visual.cornerWaterDepthM[corner] = depthM;
            visual.cornerWaterSurfaceElevationM[corner] = waterLevelY;
            maximumCornerDepthM = std::max(maximumCornerDepthM, depthM);
        }

        visual.waterDepthM = maximumCornerDepthM;
        visual.flowVelocityXMps = 0.0;
        visual.flowVelocityZMps = 0.0;
        const double centerDx = centerX - center.x;
        const double centerDz = centerZ - center.z;
        visual.cameraDistanceSquaredM2 = centerDx * centerDx + centerDz * centerDz;
        visual.presentationLodClass = 0.0f;
        visual.fineBoundaryMask = 0u;
        output.push_back(visual);
    }

    if (output.size() > maximumCells)
    {
        const auto middle = output.begin()
            + static_cast<std::ptrdiff_t>(maximumCells);
        std::nth_element(
            output.begin(), middle, output.end(),
            [](const SurfaceHydrologyVisualCell& left,
               const SurfaceHydrologyVisualCell& right) {
                return left.cameraDistanceSquaredM2
                    < right.cameraDistanceSquaredM2;
            });
        output.resize(maximumCells);
    }

    std::sort(
        output.begin(), output.end(),
        [](const SurfaceHydrologyVisualCell& left,
           const SurfaceHydrologyVisualCell& right) {
            if (left.presentationLayer != right.presentationLayer)
                return left.presentationLayer < right.presentationLayer;
            if (left.presentationBasinId != right.presentationBasinId)
                return left.presentationBasinId < right.presentationBasinId;
            if (left.globalPosition.z != right.globalPosition.z)
                return left.globalPosition.z < right.globalPosition.z;
            return left.globalPosition.x < right.globalPosition.x;
        });
}

void SurfaceHydrology::collectVisualCellsBand(
    const heritage::math::DVec3& center,
    double minimumRadiusM,
    double maximumRadiusM,
    std::size_t maximumCells,
    std::vector<SurfaceHydrologyVisualCell>& output,
    bool includeDryCells,
    double minimumExplicitWaterDepthM) const
{
    output.clear();
    if (!finitePosition(center)
        || !std::isfinite(minimumRadiusM)
        || !std::isfinite(maximumRadiusM)
        || maximumRadiusM <= 0.0
        || maximumRadiusM <= minimumRadiusM
        || maximumCells == 0u)
    {
        return;
    }

    const double minimumRadiusSquared = std::max(minimumRadiusM, 0.0)
        * std::max(minimumRadiusM, 0.0);
    const double maximumRadiusSquared = maximumRadiusM * maximumRadiusM;
    const double visibleThreshold = std::max(
        std::isfinite(minimumExplicitWaterDepthM)
            ? minimumExplicitWaterDepthM : 0.00001,
        0.0);
    output.reserve(std::min(maximumCells, m_adaptiveCells.size()));

    // WATER15F presentation reconstruction. The adaptive solver remains the
    // sole water-volume authority, but presentation needs a continuous
    // free-surface estimate rather than painting each control volume as a
    // rectangle. Thin films follow the supporting collider plane while
    // standing water progressively approaches hydraulic head. At a cell
    // corner we blend only neighbouring cells whose actual support elevation
    // agrees within 2 cm, which explicitly refuses to smear road water across
    // a curb/sidewalk height discontinuity.
    const auto presentationHeadAt = [&](const AdaptiveCell& source,
                                        double x,
                                        double z) {
        const double depthM = adaptiveWaterDepth(source);
        const double supportY = adaptiveSurfaceElevationAt(source, x, z);
        const double conformalHeadY = supportY + depthM;
        const double hydraulicHeadY = source.elevationM + depthM;
        const double standingWeight = smoothStep(0.0008, 0.0080, depthM);
        return conformalHeadY
            + (hydraulicHeadY - conformalHeadY) * standingWeight;
    };

    for (const AdaptiveCell& cell : m_adaptiveCells)
    {
        const double half = cell.cellSizeM * 0.5;
        const double dx = center.x < cell.centerX - half
            ? (cell.centerX - half) - center.x
            : (center.x > cell.centerX + half
                ? center.x - (cell.centerX + half) : 0.0);
        const double dz = center.z < cell.centerZ - half
            ? (cell.centerZ - half) - center.z
            : (center.z > cell.centerZ + half
                ? center.z - (cell.centerZ + half) : 0.0);
        const double distanceSquared = dx * dx + dz * dz;
        if (distanceSquared < minimumRadiusSquared
            || distanceSquared > maximumRadiusSquared)
        {
            continue;
        }

        const double depth = adaptiveWaterDepth(cell);
        if (!includeDryCells && depth <= visibleThreshold)
            continue;

        SurfaceHydrologyVisualCell visual;
        visual.globalPosition = {
            cell.centerX,
            cell.elevationM + std::max(depth, 0.000000001),
            cell.centerZ };
        visual.normal = cell.normal;
        visual.material = cell.material;
        visual.cellSizeM = cell.cellSizeM;
        visual.surfaceElevationM = cell.elevationM;
        constexpr std::array<std::array<double, 2>, 4> signs{{
            {{ -1.0, -1.0 }}, {{ 1.0, -1.0 }},
            {{ -1.0, 1.0 }}, {{ 1.0, 1.0 }}
        }};
        for (std::size_t corner = 0; corner < signs.size(); ++corner)
        {
            const double x = cell.centerX + signs[corner][0] * half;
            const double z = cell.centerZ + signs[corner][1] * half;
            const double surfaceY = adaptiveSurfaceElevationAt(cell, x, z);
            visual.cornerSurfaceElevationM[corner] = surfaceY;
            visual.cornerWaterDepthM[corner] = depth;

            double weightedHeadY = presentationHeadAt(cell, x, z);
            double totalWeight = 1.0 / std::sqrt(std::max(cell.cellSizeM, 0.10));
            weightedHeadY *= totalWeight;

            // Gather only cells directly connected through the authoritative
            // virtual-pipe topology. WATER15F's second-hop corner walk multiplied
            // presentation CPU work across every clipmap refresh. WATER15G keeps
            // one-hop edge continuity and lets the compact atlas + shader-side
            // micro-relief hide sub-cell visual ownership instead of traversing
            // the hydrology graph again for every corner.
            constexpr double kCompatibleCornerSurfaceDeltaM = 0.020;
            constexpr double kCornerContainmentToleranceM = 0.001;
            std::array<std::int32_t, 16> acceptedIndices{};
            acceptedIndices.fill(-1);
            std::size_t acceptedCount = 0u;

            const auto addCandidate = [&](std::int32_t candidateIndex) {
                if (candidateIndex < 0
                    || acceptedCount >= acceptedIndices.size())
                {
                    return;
                }

                const AdaptiveCell& candidate = m_adaptiveCells[
                    static_cast<std::size_t>(candidateIndex)];
                if (&candidate == &cell)
                    return;
                for (std::size_t accepted = 0u; accepted < acceptedCount; ++accepted)
                {
                    if (acceptedIndices[accepted] == candidateIndex)
                        return;
                }

                const double candidateMinX = static_cast<double>(
                    candidate.minimumXUnits) * kAdaptiveUnitM;
                const double candidateMinZ = static_cast<double>(
                    candidate.minimumZUnits) * kAdaptiveUnitM;
                const double candidateMaxX = candidateMinX + candidate.cellSizeM;
                const double candidateMaxZ = candidateMinZ + candidate.cellSizeM;
                if (x < candidateMinX - kCornerContainmentToleranceM
                    || x > candidateMaxX + kCornerContainmentToleranceM
                    || z < candidateMinZ - kCornerContainmentToleranceM
                    || z > candidateMaxZ + kCornerContainmentToleranceM)
                {
                    return;
                }

                const double candidateSurfaceY = adaptiveSurfaceElevationAt(
                    candidate, x, z);
                if (!std::isfinite(candidateSurfaceY)
                    || std::abs(candidateSurfaceY - surfaceY)
                        > kCompatibleCornerSurfaceDeltaM)
                {
                    return;
                }

                acceptedIndices[acceptedCount++] = candidateIndex;
                const double weight = 1.0 / std::sqrt(
                    std::max(candidate.cellSizeM, 0.10));
                weightedHeadY += presentationHeadAt(candidate, x, z) * weight;
                totalWeight += weight;
            };

            for (const std::int32_t pipeIndex : cell.pipeIndices)
            {
                if (pipeIndex < 0
                    || static_cast<std::size_t>(pipeIndex) >= m_adaptivePipes.size())
                {
                    continue;
                }
                const AdaptivePipe& pipe = m_adaptivePipes[
                    static_cast<std::size_t>(pipeIndex)];
                const std::int32_t candidateIndex = pipe.cellA >= 0
                    && &m_adaptiveCells[static_cast<std::size_t>(pipe.cellA)] == &cell
                    ? pipe.cellB : pipe.cellA;
                addCandidate(candidateIndex);
            }

            visual.cornerWaterSurfaceElevationM[corner] =
                weightedHeadY / std::max(totalWeight, 1.0e-9);
        }
        visual.waterDepthM = depth;
        visual.flowVelocityXMps = cell.flowVelocityXMps;
        visual.flowVelocityZMps = cell.flowVelocityZMps;
        const double centerDx = cell.centerX - center.x;
        const double centerDz = cell.centerZ - center.z;
        visual.cameraDistanceSquaredM2 = centerDx * centerDx + centerDz * centerDz;
        const double scaleDenominator = std::max(
            m_description.adaptiveMaximumCellSizeM
                - m_description.adaptiveMinimumCellSizeM,
            1.0e-6);
        visual.presentationLodClass = static_cast<float>(std::clamp(
            (cell.cellSizeM - m_description.adaptiveMinimumCellSizeM)
                / scaleDenominator,
            0.0,
            1.0));
        visual.fineBoundaryMask = cell.fineBoundaryMask;
        visual.presentationLayer = cell.layer;
        output.push_back(visual);
    }

    if (output.size() > maximumCells)
    {
        const auto middle = output.begin()
            + static_cast<std::ptrdiff_t>(maximumCells);
        std::nth_element(
            output.begin(), middle, output.end(),
            [](const SurfaceHydrologyVisualCell& left,
               const SurfaceHydrologyVisualCell& right) {
                return left.cameraDistanceSquaredM2
                    < right.cameraDistanceSquaredM2;
            });
        output.resize(maximumCells);
    }
}

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
        || header.cellCount > m_description.maximumCellCount)
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
        loaded.push_back(cell);
    }
    m_cells = std::move(loaded);
    rebuildLookupAndConnectivity();
    m_stats.loadedFromCache = true;
    report = {};
    report.valid = true;
    report.loadedFromCache = true;
    report.cellCount = m_adaptiveCells.size();
    report.connectedCellCount = m_stats.connectedCellCount;
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
        file.write(reinterpret_cast<const char*>(&saved), sizeof(saved));
    }
    return static_cast<bool>(file);
}

} // namespace heritage::physics::water

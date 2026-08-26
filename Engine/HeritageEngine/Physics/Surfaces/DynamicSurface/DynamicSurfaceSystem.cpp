#include "DynamicSurfaceSystem.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::physics::dynamicsurface {

namespace {

std::int64_t chunkCoordinate(double value)
{
    return static_cast<std::int64_t>(std::floor(value / kChunkSizeM));
}

double distanceToChunkAabbXZ(
    const heritage::math::DVec3& point,
    ChunkAddress address)
{
    const double minX = static_cast<double>(address.x) * kChunkSizeM;
    const double minZ = static_cast<double>(address.z) * kChunkSizeM;
    const double maxX = minX + kChunkSizeM;
    const double maxZ = minZ + kChunkSizeM;

    const double dx = point.x < minX ? minX - point.x
        : (point.x > maxX ? point.x - maxX : 0.0);
    const double dz = point.z < minZ ? minZ - point.z
        : (point.z > maxZ ? point.z - maxZ : 0.0);
    return std::hypot(dx, dz);
}

} // namespace

void DynamicSurfaceSystem::clear()
{
    m_chunks.clear();
    m_pagePool.clear();
    m_thermal.clear();
    m_sheetLinks.clear();
    m_lastStaticBakeReport = {};
    m_interestSources.clear();
    m_hydroMigrationStats = {};
}

ChunkAddress DynamicSurfaceSystem::chunkAddressFor(
    const heritage::math::DVec3& globalPosition)
{
    return { chunkCoordinate(globalPosition.x), chunkCoordinate(globalPosition.z) };
}

DynamicSurfaceChunk& DynamicSurfaceSystem::acquireChunk(ChunkAddress address)
{
    auto [it, inserted] = m_chunks.try_emplace(address, address);
    (void)inserted;
    return it->second;
}

DynamicSurfaceChunk* DynamicSurfaceSystem::findChunk(ChunkAddress address)
{
    auto it = m_chunks.find(address);
    return it == m_chunks.end() ? nullptr : &it->second;
}

const DynamicSurfaceChunk* DynamicSurfaceSystem::findChunk(ChunkAddress address) const
{
    auto it = m_chunks.find(address);
    return it == m_chunks.end() ? nullptr : &it->second;
}

std::vector<ChunkAddress> DynamicSurfaceSystem::chunkAddresses() const
{
    std::vector<ChunkAddress> result;
    result.reserve(m_chunks.size());
    for (const auto& [address, chunk] : m_chunks)
    {
        (void)chunk;
        result.push_back(address);
    }
    return result;
}

void DynamicSurfaceSystem::setInterestSources(
    const std::vector<heritage::math::DVec3>& sources)
{
    m_interestSources.clear();
    m_interestSources.reserve(sources.size());
    for (const heritage::math::DVec3& source : sources)
    {
        if (std::isfinite(source.x) && std::isfinite(source.y) && std::isfinite(source.z))
            m_interestSources.push_back(source);
    }
}

double DynamicSurfaceSystem::requestedUpdateHz(ChunkAddress address) const
{
    for (const heritage::math::DVec3& source : m_interestSources)
    {
        if (distanceToChunkAabbXZ(source, address) <= UpdateCadence::trackActiveRadiusM)
            return UpdateCadence::trackTileHz;
    }
    return 0.0;
}

std::optional<PhysicalPageAssignment> DynamicSurfaceSystem::ensurePageResident(
    const VirtualPageAddress& address,
    bool pin)
{
    const DynamicSurfaceChunk* chunk = findChunk(address.chunk);
    if (!chunk || address.page.sheet >= chunk->surfaceSheetCount()
        || address.page.x >= kPagesPerAxis || address.page.z >= kPagesPerAxis)
    {
        return std::nullopt;
    }
    return m_pagePool.ensureResident(address, pin);
}

bool DynamicSurfaceSystem::markPageDirty(
    const VirtualPageAddress& address,
    PagePlaneMask planes)
{
    DynamicSurfaceChunk* chunk = findChunk(address.chunk);
    if (!chunk || address.page.sheet >= chunk->surfaceSheetCount())
        return false;
    chunk->markPageDirty(address.page);
    return m_pagePool.markDirty(address, planes);
}


void DynamicSurfaceSystem::refreshHydroResidency()
{
    struct Candidate
    {
        VirtualPageAddress address{};
        double distanceSquaredM2 = 0.0;
    };

    m_hydroMigrationStats = {};
    if (m_interestSources.empty() || m_chunks.empty())
        return;

    std::vector<Candidate> candidates;
    for (const auto& [chunkAddress, chunk] : m_chunks)
    {
        const double chunkHz = requestedUpdateHz(chunkAddress);
        if (chunkHz <= 0.0)
            continue;

        const heritage::math::DVec3 chunkOrigin = chunk.globalOrigin();
        for (const PageAddress& page : chunk.coveredPages())
        {
            const double centerX = chunkOrigin.x
                + (static_cast<double>(page.x) + 0.5) * kPhysicalPageWorldSizeM;
            const double centerZ = chunkOrigin.z
                + (static_cast<double>(page.z) + 0.5) * kPhysicalPageWorldSizeM;
            double bestDistanceSquared = (std::numeric_limits<double>::max)();
            for (const heritage::math::DVec3& source : m_interestSources)
            {
                const double dx = centerX - source.x;
                const double dz = centerZ - source.z;
                bestDistanceSquared = std::min(
                    bestDistanceSquared, dx * dx + dz * dz);
            }
            candidates.push_back({ { chunkAddress, page }, bestDistanceSquared });
        }
    }

    std::sort(
        candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) {
            if (a.distanceSquaredM2 != b.distanceSquaredM2)
                return a.distanceSquaredM2 < b.distanceSquaredM2;
            return a.address < b.address;
        });
    m_hydroMigrationStats.candidatePages = candidates.size();

    const std::size_t requestCount = std::min(
        candidates.size(), m_pagePool.capacityPages());
    m_hydroMigrationStats.requestedResidentPages = requestCount;
    for (std::size_t index = 0; index < requestCount; ++index)
    {
        if (ensurePageResident(candidates[index].address, false))
            ++m_hydroMigrationStats.residentHydroPages;
        else
            ++m_hydroMigrationStats.failedResidencyRequests;
    }
}


void DynamicSurfaceSystem::advanceThermal(
    const SurfaceWeatherDescription& weather,
    const SurfaceWeatherOutput& weatherOutput,
    double ambientTemperatureC,
    bool surfaceTemperatureOverrideEnabled,
    double surfaceTemperatureOverrideC,
    double deltaTimeSeconds)
{
    m_thermal.advance(
        *this,
        weather,
        weatherOutput,
        ambientTemperatureC,
        surfaceTemperatureOverrideEnabled,
        surfaceTemperatureOverrideC,
        deltaTimeSeconds);
}

void DynamicSurfaceSystem::resetThermalState()
{
    m_thermal.reset();
}

DynamicSurfaceThermalSample DynamicSurfaceSystem::sampleThermal(
    const heritage::math::DVec3& globalPosition) const
{
    return m_thermal.sample(*this, globalPosition);
}

DynamicSurfaceThermalTireResult DynamicSurfaceSystem::applyThermalTireContact(
    const heritage::math::DVec3& globalPosition,
    const DynamicSurfaceThermalTireInput& input)
{
    return m_thermal.applyTireContact(*this, globalPosition, input);
}

bool DynamicSurfaceSystem::rasterTrackPage(
    const VirtualPageAddress& address,
    std::uint32_t outputResolution,
    std::vector<float>& trackRgba) const
{
    return m_thermal.rasterTrackPage(*this, address, outputResolution, trackRgba);
}

std::uint64_t DynamicSurfaceSystem::trackPageRevision(
    const VirtualPageAddress& address) const
{
    return m_thermal.pageRevision(address);
}

} // namespace heritage::physics::dynamicsurface

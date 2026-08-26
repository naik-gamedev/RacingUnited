#include "DynamicSurfaceThermal.hpp"

#include "DynamicSurfaceSystem.hpp"
#include "../SurfaceMaterialProperties.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>

namespace heritage::physics::dynamicsurface {

namespace {

constexpr double kMinimumSurfaceMatchM = 0.35;
constexpr std::size_t kMaximumNewAuthorityPagesPerAdvance = 96u;
constexpr double kMinimumTemperatureC = -100.0;
constexpr double kMaximumTemperatureC = 180.0;
constexpr double kRoadHeatFraction = 0.18;

bool finite(const heritage::math::DVec3& value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

bool barycentricHeightXZ(
    const StaticSurfacePatchTriangle& triangle,
    double x,
    double z,
    double& heightM)
{
    const double denominator =
        (triangle.b.z - triangle.c.z) * (triangle.a.x - triangle.c.x)
        + (triangle.c.x - triangle.b.x) * (triangle.a.z - triangle.c.z);
    if (std::abs(denominator) <= 1.0e-12)
        return false;

    const double u = (
        (triangle.b.z - triangle.c.z) * (x - triangle.c.x)
        + (triangle.c.x - triangle.b.x) * (z - triangle.c.z)) / denominator;
    const double v = (
        (triangle.c.z - triangle.a.z) * (x - triangle.c.x)
        + (triangle.a.x - triangle.c.x) * (z - triangle.c.z)) / denominator;
    const double w = 1.0 - u - v;
    constexpr double epsilon = 1.0e-7;
    if (u < -epsilon || v < -epsilon || w < -epsilon)
        return false;

    heightM = u * triangle.a.y + v * triangle.b.y + w * triangle.c.y;
    return std::isfinite(heightM);
}


bool triangleIntersectsCellSampleXZ(
    const StaticSurfacePatchTriangle& triangle,
    double centerX,
    double centerZ,
    double halfExtent,
    double& sampleX,
    double& sampleZ,
    double& heightM)
{
    // Most covered cells are interior cells. Keep the common path as cheap as
    // the old center-point test, then do exact projected triangle/cell overlap
    // work only for boundary cells. A 64x64 Dynamic Surface cell represents an
    // AREA; narrow roads/kerbs/gutters must not disappear just because the
    // 1.5625m cell centre happens to lie beside the authored triangle.
    if (barycentricHeightXZ(triangle, centerX, centerZ, heightM))
    {
        sampleX = centerX;
        sampleZ = centerZ;
        return true;
    }

    const double minX = centerX - halfExtent;
    const double maxX = centerX + halfExtent;
    const double minZ = centerZ - halfExtent;
    const double maxZ = centerZ + halfExtent;
    constexpr double epsilon = 1.0e-9;
    double bestDistanceSquared = (std::numeric_limits<double>::max)();
    bool found = false;

    auto consider = [&](double x, double z) {
        if (x < minX - epsilon || x > maxX + epsilon
            || z < minZ - epsilon || z > maxZ + epsilon)
        {
            return;
        }
        double candidateHeight = 0.0;
        if (!barycentricHeightXZ(triangle, x, z, candidateHeight))
            return;
        const double dx = x - centerX;
        const double dz = z - centerZ;
        const double distanceSquared = dx * dx + dz * dz;
        if (!found || distanceSquared < bestDistanceSquared)
        {
            found = true;
            bestDistanceSquared = distanceSquared;
            sampleX = x;
            sampleZ = z;
            heightM = candidateHeight;
        }
    };

    consider(triangle.a.x, triangle.a.z);
    consider(triangle.b.x, triangle.b.z);
    consider(triangle.c.x, triangle.c.z);
    consider(minX, minZ);
    consider(maxX, minZ);
    consider(maxX, maxZ);
    consider(minX, maxZ);

    struct Point2 { double x = 0.0; double z = 0.0; };
    const std::array<Point2, 3> trianglePoints{{
        { triangle.a.x, triangle.a.z },
        { triangle.b.x, triangle.b.z },
        { triangle.c.x, triangle.c.z }
    }};
    const std::array<std::pair<Point2, Point2>, 4> cellEdges{{
        { { minX, minZ }, { maxX, minZ } },
        { { maxX, minZ }, { maxX, maxZ } },
        { { maxX, maxZ }, { minX, maxZ } },
        { { minX, maxZ }, { minX, minZ } }
    }};

    const auto intersectSegments = [&](const Point2& a, const Point2& b,
                                       const Point2& c, const Point2& d) {
        const double abX = b.x - a.x;
        const double abZ = b.z - a.z;
        const double cdX = d.x - c.x;
        const double cdZ = d.z - c.z;
        const double denominator = abX * cdZ - abZ * cdX;
        if (std::abs(denominator) <= 1.0e-12)
            return;
        const double acX = c.x - a.x;
        const double acZ = c.z - a.z;
        const double t = (acX * cdZ - acZ * cdX) / denominator;
        const double u = (acX * abZ - acZ * abX) / denominator;
        if (t < -epsilon || t > 1.0 + epsilon
            || u < -epsilon || u > 1.0 + epsilon)
        {
            return;
        }
        consider(a.x + t * abX, a.z + t * abZ);
    };

    for (std::size_t edge = 0; edge < trianglePoints.size(); ++edge)
    {
        const Point2& a = trianglePoints[edge];
        const Point2& b = trianglePoints[(edge + 1u) % trianglePoints.size()];
        for (const auto& cellEdge : cellEdges)
            intersectSegments(a, b, cellEdge.first, cellEdge.second);
    }

    return found;
}

bool coveredPage(const DynamicSurfaceChunk& chunk, const PageAddress& page)
{
    return !chunk.staticTriangleIndicesForPage(page).empty();
}

double distanceSquaredToPageCenter(
    const heritage::math::DVec3& source,
    const VirtualPageAddress& address)
{
    const double x = static_cast<double>(address.chunk.x) * kChunkSizeM
        + (static_cast<double>(address.page.x) + 0.5) * kPhysicalPageWorldSizeM;
    const double z = static_cast<double>(address.chunk.z) * kChunkSizeM
        + (static_cast<double>(address.page.z) + 0.5) * kPhysicalPageWorldSizeM;
    const double dx = source.x - x;
    const double dz = source.z - z;
    return dx * dx + dz * dz;
}


double requestedHzForPage(
    const DynamicSurfaceSystem& owner,
    const VirtualPageAddress& address)
{
    return owner.requestedUpdateHz(address.chunk);
}

void thermalMaterialParameters(
    SurfaceMaterial material,
    float& timeScale,
    float& heatCapacityJPerM2K)
{
    timeScale = 1.0f;
    heatCapacityJPerM2K = 220000.0f;
    switch (material)
    {
    case SurfaceMaterial::Kerb:
        timeScale = 1.25f;
        heatCapacityJPerM2K = 300000.0f;
        break;
    case SurfaceMaterial::PaintedLine:
        timeScale = 0.65f;
        heatCapacityJPerM2K = 150000.0f;
        break;
    case SurfaceMaterial::Gravel:
    case SurfaceMaterial::Dirt:
    case SurfaceMaterial::Grass:
        timeScale = 0.70f;
        heatCapacityJPerM2K = 160000.0f;
        break;
    case SurfaceMaterial::Mud:
    case SurfaceMaterial::Sand:
    case SurfaceMaterial::SoftSoil:
        timeScale = 0.80f;
        heatCapacityJPerM2K = 190000.0f;
        break;
    case SurfaceMaterial::Snow:
    case SurfaceMaterial::DeepSnow:
        timeScale = 0.45f;
        heatCapacityJPerM2K = 95000.0f;
        break;
    case SurfaceMaterial::Ice:
        timeScale = 0.90f;
        heatCapacityJPerM2K = 210000.0f;
        break;
    case SurfaceMaterial::Default:
    case SurfaceMaterial::Asphalt:
    default:
        break;
    }
}

} // namespace

void DynamicSurfaceThermal::clear()
{
    m_pages.clear();
    m_stats = {};
    m_elapsedSeconds = 0.0;
}

void DynamicSurfaceThermal::reset()
{
    for (auto& [address, page] : m_pages)
    {
        (void)address;
        for (std::size_t i = 0; i < page.stateCells.size(); ++i)
        {
            page.stateCells[i].surfaceTemperatureC =
                page.staticCells[i].initialTemperatureC;
        }
        page.stepAccumulatorSeconds = 0.0;
        ++page.revision;
    }
    m_stats = {};
    refreshStats();
}

DynamicSurfaceThermal::ThermalPage* DynamicSurfaceThermal::ensurePage(
    DynamicSurfaceSystem& owner,
    const VirtualPageAddress& address)
{
    const DynamicSurfaceChunk* chunk = owner.findChunk(address.chunk);
    if (!chunk
        || address.page.sheet >= chunk->surfaceSheetCount()
        || !coveredPage(*chunk, address.page))
    {
        return nullptr;
    }

    auto [it, inserted] = m_pages.try_emplace(address);
    ThermalPage& page = it->second;
    if (inserted)
    {
        page.address = address;
        page.staticCells.resize(
            static_cast<std::size_t>(kTrackAuthorityResolution)
            * kTrackAuthorityResolution);
        page.stateCells.resize(page.staticCells.size());
        const double initialHz = requestedHzForPage(owner, address);
        if (initialHz > 0.0)
        {
            page.cadenceHz = initialHz;
            page.stepAccumulatorSeconds =
                deterministicPagePhase01(address) / initialHz;
        }
    }

    if (!page.staticReady && !buildStaticPage(owner, page))
    {
        m_pages.erase(it);
        return nullptr;
    }

    if (inserted)
    {
        for (std::size_t i = 0; i < page.stateCells.size(); ++i)
        {
            page.stateCells[i].surfaceTemperatureC =
                page.staticCells[i].initialTemperatureC;
        }
    }
    page.lastTouchedSeconds = m_elapsedSeconds;
    return &page;
}

bool DynamicSurfaceThermal::buildStaticPage(
    const DynamicSurfaceSystem& owner,
    ThermalPage& page) const
{
    const DynamicSurfaceChunk* chunk = owner.findChunk(page.address.chunk);
    if (!chunk)
        return false;

    const heritage::math::DVec3 chunkOrigin = chunk->globalOrigin();
    const double pageOriginX = chunkOrigin.x
        + static_cast<double>(page.address.page.x) * kPhysicalPageWorldSizeM;
    const double pageOriginZ = chunkOrigin.z
        + static_cast<double>(page.address.page.z) * kPhysicalPageWorldSizeM;
    const double pitch = kTrackAuthorityTexelPitchM;
    bool anyValid = false;
    for (std::uint32_t z = 0; z < kTrackAuthorityResolution; ++z)
    {
        for (std::uint32_t x = 0; x < kTrackAuthorityResolution; ++x)
        {
            const double worldX = pageOriginX + (static_cast<double>(x) + 0.5) * pitch;
            const double worldZ = pageOriginZ + (static_cast<double>(z) + 0.5) * pitch;
            StaticCell& cell = page.staticCells[cellIndex(x, z)];

            const auto& surfaceTriangleIndices =
                chunk->staticTriangleIndicesForStateCell(
                    page.address.page.sheet, x, z);
            const auto& xzTriangleIndices =
                chunk->staticTriangleIndicesForXzStateCell(x, z);
            const StaticSurfacePatchTriangle* bestTriangle = nullptr;
            double bestHeight = 0.0;
            double bestSampleX = worldX;
            double bestSampleZ = worldZ;
            for (const std::uint32_t triangleIndex : surfaceTriangleIndices)
            {
                const StaticSurfacePatchTriangle& triangle =
                    chunk->staticTriangles()[triangleIndex];
                double sampleX = worldX;
                double sampleZ = worldZ;
                double height = 0.0;
                if (!triangleIntersectsCellSampleXZ(
                        triangle, worldX, worldZ, pitch * 0.5,
                        sampleX, sampleZ, height))
                {
                    continue;
                }
                if (!bestTriangle || height > bestHeight)
                {
                    bestTriangle = &triangle;
                    bestHeight = height;
                    bestSampleX = sampleX;
                    bestSampleZ = sampleZ;
                }
            }
            if (!bestTriangle)
                continue;

            const SurfaceMaterial material = static_cast<SurfaceMaterial>(
                bestTriangle->materialId);
            cell.valid = true;
            cell.supportHeightM = static_cast<float>(bestHeight);
            cell.initialTemperatureC = bestTriangle->hasAuthoredSurfaceTemperature
                ? bestTriangle->authoredSurfaceTemperatureC
                : static_cast<float>(defaultSurfaceTemperatureC(material));
            thermalMaterialParameters(
                material, cell.thermalTimeScale, cell.heatCapacityJPerM2K);
            cell.skyExposed = true;

            // Solar/rain exposure follows the same stacked-sheet rule as Hydro:
            // a bridge shades and shelters the road below without merging them.
            for (const std::uint32_t triangleIndex : xzTriangleIndices)
            {
                const StaticSurfacePatchTriangle& possibleCover =
                    chunk->staticTriangles()[triangleIndex];
                if (possibleCover.surfaceSheetId == page.address.page.sheet)
                    continue;
                double coverHeight = 0.0;
                if (!barycentricHeightXZ(
                        possibleCover, bestSampleX, bestSampleZ, coverHeight))
                {
                    continue;
                }
                if (coverHeight > bestHeight + 0.20)
                {
                    cell.skyExposed = false;
                    break;
                }
            }
            anyValid = true;
        }
    }

    page.staticReady = anyValid;
    return anyValid;
}

bool DynamicSurfaceThermal::simulateEnvironment(
    const DynamicSurfaceSystem& owner,
    ThermalPage& page,
    const SurfaceWeatherDescription& weather,
    const SurfaceWeatherOutput& weatherOutput,
    double ambientTemperatureC,
    bool surfaceTemperatureOverrideEnabled,
    double surfaceTemperatureOverrideC,
    double stepSeconds)
{
    if (stepSeconds <= 0.0 || !std::isfinite(ambientTemperatureC))
        return false;

    bool changed = false;
    (void)owner;

    for (std::size_t i = 0; i < page.stateCells.size(); ++i)
    {
        const StaticCell& staticCell = page.staticCells[i];
        if (!staticCell.valid)
            continue;

        StateCell& state = page.stateCells[i];
        const float previousTemperatureC = state.surfaceTemperatureC;
        if (surfaceTemperatureOverrideEnabled)
        {
            state.surfaceTemperatureC = static_cast<float>(std::clamp(
                surfaceTemperatureOverrideC,
                kMinimumTemperatureC,
                kMaximumTemperatureC));
            changed = changed
                || std::abs(state.surfaceTemperatureC - previousTemperatureC)
                    > 1.0e-6f;
            continue;
        }

        double wetness = 0.0;
        double waterDepthM = 0.0;
        // OPT03B: Track thermal no longer reaches into the dormant CPU Hydro
        // lattice. Production wet cooling follows scene weather; tire contact
        // heat remains spatial through the Track authority itself.
        if (staticCell.skyExposed && weatherOutput.valid)
        {
            wetness = std::clamp(weatherOutput.effectiveWetness, 0.0, 1.0);
            waterDepthM = std::max(weatherOutput.waterFilmDepthM, 0.0);
        }

        const double solarHeatingC = weather.enabled && staticCell.skyExposed
            ? weather.maximumSolarHeatingC * (1.0 - weather.cloudCover)
                * (1.0 - 0.75 * wetness)
            : 0.0;
        const double rainCoolingC = weather.enabled && staticCell.skyExposed
            ? std::min(weather.precipitationRateMmPerHour / 40.0, 4.0)
                * wetness
            : 0.0;
        const double evaporationCoolingC = weatherOutput.valid
            ? std::min(weatherOutput.evaporationRateMmPerHour * 0.8, 3.0)
                * std::clamp(wetness + waterDepthM * 150.0, 0.0, 1.0)
            : 0.0;
        const double targetTemperatureC = std::clamp(
            ambientTemperatureC + solarHeatingC
                - rainCoolingC - evaporationCoolingC,
            kMinimumTemperatureC,
            kMaximumTemperatureC);

        const double baseTimeConstantSeconds = weather.enabled
            ? weather.roadThermalTimeConstantSeconds
            : 900.0;
        const double timeConstantSeconds = std::max(
            baseTimeConstantSeconds
                * std::max(static_cast<double>(staticCell.thermalTimeScale), 0.1),
            1.0);
        const double blend = 1.0 - std::exp(-stepSeconds / timeConstantSeconds);
        state.surfaceTemperatureC = static_cast<float>(std::clamp(
            static_cast<double>(state.surfaceTemperatureC)
                + (targetTemperatureC - state.surfaceTemperatureC) * blend,
            kMinimumTemperatureC,
            kMaximumTemperatureC));
        changed = changed
            || std::abs(state.surfaceTemperatureC - previousTemperatureC)
                > 1.0e-6f;
    }

    if (changed)
        ++page.revision;
    return changed;
}

void DynamicSurfaceThermal::advance(
    DynamicSurfaceSystem& owner,
    const SurfaceWeatherDescription& weather,
    const SurfaceWeatherOutput& weatherOutput,
    double ambientTemperatureC,
    bool surfaceTemperatureOverrideEnabled,
    double surfaceTemperatureOverrideC,
    double deltaTimeSeconds)
{
    if (!std::isfinite(deltaTimeSeconds) || deltaTimeSeconds <= 0.0)
        return;

    const auto started = std::chrono::steady_clock::now();
    m_elapsedSeconds += deltaTimeSeconds;

    struct Candidate
    {
        VirtualPageAddress address{};
        double distanceSquaredM2 = 0.0;
    };
    // DSURF04B: the bounded resident-page set is the adaptive simulation
    // working set. Persistent CPU state may outlive GPU residency, but pages
    // outside the current real-interest residency set do not consume update
    // time merely because their chunk remains within a broad 1000 m radius.
    if (owner.m_pagePool.residentAssignments().empty()
        && !owner.interestSources().empty())
    {
        owner.refreshHydroResidency();
    }
    const auto residentAssignments = owner.m_pagePool.residentAssignments();
    std::vector<Candidate> candidates;
    candidates.reserve(residentAssignments.size());
    for (const PhysicalPageAssignment& assignment : residentAssignments)
    {
        double best = (std::numeric_limits<double>::max)();
        for (const heritage::math::DVec3& source : owner.interestSources())
        {
            best = std::min(
                best,
                distanceSquaredToPageCenter(source, assignment.virtualAddress));
        }
        candidates.push_back({ assignment.virtualAddress, best });
    }
    std::sort(
        candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) {
            if (a.distanceSquaredM2 != b.distanceSquaredM2)
                return a.distanceSquaredM2 < b.distanceSquaredM2;
            return a.address < b.address;
        });

    std::size_t builtThisAdvance = 0u;
    for (const Candidate& candidate : candidates)
    {
        const auto found = m_pages.find(candidate.address);
        if (found == m_pages.end()
            && builtThisAdvance >= kMaximumNewAuthorityPagesPerAdvance)
        {
            continue;
        }
        ThermalPage* page = ensurePage(owner, candidate.address);
        if (!page)
            continue;
        if (found == m_pages.end())
            ++builtThisAdvance;
        page->lastTouchedSeconds = m_elapsedSeconds;
    }

    m_stats.activePages = 0u;
    m_stats.cadence30HzPages = 0u;
    m_stats.cadence20HzPages = 0u;
    m_stats.cadence6HzPages = 0u;
    m_stats.cadence2HzPages = 0u;
    m_stats.scheduledPagesThisAdvance = 0u;

    for (auto& [address, page] : m_pages)
    {
        if (owner.m_pagePool.find(address) == nullptr)
            continue;
        const double hz = requestedHzForPage(owner, address);
        if (hz <= 0.0)
            continue;
        ++m_stats.activePages;
        ++m_stats.cadence2HzPages;

        const double interval = 1.0 / hz;
        if (page.cadenceHz != hz)
        {
            // Reactivation/cadence changes rephase from stable world/sheet tile
            // identity so dormant backlog can never burst into catch-up work.
            page.cadenceHz = hz;
            page.stepAccumulatorSeconds =
                deterministicPagePhase01(address) * interval;
        }
        page.stepAccumulatorSeconds += deltaTimeSeconds;
        int catchup = 0;
        while (page.stepAccumulatorSeconds + 1.0e-12 >= interval
            && catchup < 4)
        {
            simulateEnvironment(
                owner,
                page,
                weather,
                weatherOutput,
                ambientTemperatureC,
                surfaceTemperatureOverrideEnabled,
                surfaceTemperatureOverrideC,
                interval);
            page.stepAccumulatorSeconds -= interval;
            ++m_stats.simulationStepCount;
            ++catchup;
        }
        if (catchup > 0)
            ++m_stats.scheduledPagesThisAdvance;
        if (catchup == 4 && page.stepAccumulatorSeconds > interval * 4.0)
            page.stepAccumulatorSeconds = interval * 4.0;
    }

    refreshStats();
    m_stats.lastStepMilliseconds =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
}

DynamicSurfaceThermal::LocatedCell DynamicSurfaceThermal::locateConst(
    const DynamicSurfaceSystem& owner,
    const heritage::math::DVec3& globalPosition) const
{
    LocatedCell result;
    if (!finite(globalPosition))
        return result;

    const ChunkAddress chunkAddress = DynamicSurfaceSystem::chunkAddressFor(
        globalPosition);
    const DynamicSurfaceChunk* chunk = owner.findChunk(chunkAddress);
    if (!chunk)
        return result;

    const heritage::math::DVec3 chunkOrigin = chunk->globalOrigin();
    const double localX = globalPosition.x - chunkOrigin.x;
    const double localZ = globalPosition.z - chunkOrigin.z;
    if (localX < 0.0 || localZ < 0.0
        || localX >= kChunkSizeM || localZ >= kChunkSizeM)
    {
        return result;
    }

    const std::uint8_t pageX = static_cast<std::uint8_t>(std::clamp(
        static_cast<int>(std::floor(localX / kPhysicalPageWorldSizeM)),
        0,
        static_cast<int>(kPagesPerAxis) - 1));
    const std::uint8_t pageZ = static_cast<std::uint8_t>(std::clamp(
        static_cast<int>(std::floor(localZ / kPhysicalPageWorldSizeM)),
        0,
        static_cast<int>(kPagesPerAxis) - 1));

    double bestDistance = (std::numeric_limits<double>::max)();
    for (std::uint16_t sheet = 0; sheet < chunk->surfaceSheetCount(); ++sheet)
    {
        const VirtualPageAddress address{
            chunkAddress, { sheet, pageX, pageZ } };
        const auto found = m_pages.find(address);
        if (found == m_pages.end())
            continue;
        const ThermalPage& page = found->second;
        const double pageOriginX = chunkOrigin.x
            + static_cast<double>(pageX) * kPhysicalPageWorldSizeM;
        const double pageOriginZ = chunkOrigin.z
            + static_cast<double>(pageZ) * kPhysicalPageWorldSizeM;
        const std::uint32_t x = static_cast<std::uint32_t>(std::clamp(
            static_cast<int>(std::floor(
                (globalPosition.x - pageOriginX) / kTrackAuthorityTexelPitchM)),
            0,
            static_cast<int>(kTrackAuthorityResolution) - 1));
        const std::uint32_t z = static_cast<std::uint32_t>(std::clamp(
            static_cast<int>(std::floor(
                (globalPosition.z - pageOriginZ) / kTrackAuthorityTexelPitchM)),
            0,
            static_cast<int>(kTrackAuthorityResolution) - 1));
        const StaticCell& cell = page.staticCells[cellIndex(x, z)];
        if (!cell.valid)
            continue;
        const double distance = std::abs(
            globalPosition.y - static_cast<double>(cell.supportHeightM));
        if (distance < bestDistance)
        {
            bestDistance = distance;
            result.constPage = &page;
            result.x = x;
            result.z = z;
            result.supportHeightM = cell.supportHeightM;
        }
    }

    if (bestDistance > kMinimumSurfaceMatchM)
        return {};
    return result;
}

DynamicSurfaceThermal::LocatedCell DynamicSurfaceThermal::locateMutable(
    DynamicSurfaceSystem& owner,
    const heritage::math::DVec3& globalPosition)
{
    LocatedCell immutable = locateConst(owner, globalPosition);
    if (!immutable.constPage)
    {
        const ChunkAddress chunkAddress = DynamicSurfaceSystem::chunkAddressFor(
            globalPosition);
        const DynamicSurfaceChunk* chunk = owner.findChunk(chunkAddress);
        if (!chunk)
            return {};
        for (const PageAddress& pageAddress : chunk->coveredPages())
        {
            const heritage::math::DVec3 chunkOrigin = chunk->globalOrigin();
            const double pageOriginX = chunkOrigin.x
                + static_cast<double>(pageAddress.x) * kPhysicalPageWorldSizeM;
            const double pageOriginZ = chunkOrigin.z
                + static_cast<double>(pageAddress.z) * kPhysicalPageWorldSizeM;
            if (globalPosition.x < pageOriginX
                || globalPosition.x >= pageOriginX + kPhysicalPageWorldSizeM
                || globalPosition.z < pageOriginZ
                || globalPosition.z >= pageOriginZ + kPhysicalPageWorldSizeM)
            {
                continue;
            }
            ensurePage(owner, { chunkAddress, pageAddress });
        }
        immutable = locateConst(owner, globalPosition);
        if (!immutable.constPage)
            return {};
    }

    const auto found = m_pages.find(immutable.constPage->address);
    if (found == m_pages.end())
        return {};
    LocatedCell result = immutable;
    result.page = &found->second;
    result.constPage = &found->second;
    return result;
}

DynamicSurfaceThermalSample DynamicSurfaceThermal::sample(
    const DynamicSurfaceSystem& owner,
    const heritage::math::DVec3& globalPosition) const
{
    DynamicSurfaceThermalSample result;
    const LocatedCell located = locateConst(owner, globalPosition);
    if (!located.constPage)
        return result;

    const StateCell& state = located.constPage->stateCells[
        cellIndex(located.x, located.z)];
    result.valid = true;
    result.surfaceSheetId = located.constPage->address.page.sheet;
    result.surfaceElevationM = located.supportHeightM;
    result.surfaceTemperatureC = state.surfaceTemperatureC;
    return result;
}

DynamicSurfaceThermalTireResult DynamicSurfaceThermal::applyTireContact(
    DynamicSurfaceSystem& owner,
    const heritage::math::DVec3& globalPosition,
    const DynamicSurfaceThermalTireInput& input)
{
    DynamicSurfaceThermalTireResult result;
    if (!std::isfinite(input.deltaTimeSeconds)
        || input.deltaTimeSeconds <= 0.0
        || !std::isfinite(input.slipDissipationWatts)
        || !std::isfinite(input.contactPatchAreaM2))
    {
        return result;
    }

    LocatedCell located = locateMutable(owner, globalPosition);
    if (!located.page)
        return result;

    const std::size_t index = cellIndex(located.x, located.z);
    const StaticCell& staticCell = located.page->staticCells[index];
    StateCell& state = located.page->stateCells[index];
    result.valid = true;
    result.initialTemperatureC = state.surfaceTemperatureC;

    const double generatedEnergyJ = std::max(input.slipDissipationWatts, 0.0)
        * input.deltaTimeSeconds;
    const double roadEnergyJ = generatedEnergyJ * kRoadHeatFraction;
    const double cellAreaM2 = kTrackAuthorityTexelPitchM
        * kTrackAuthorityTexelPitchM;
    const double heatCapacityJPerK = std::max(
        static_cast<double>(staticCell.heatCapacityJPerM2K) * cellAreaM2,
        1.0);
    const double contactCoverage = std::clamp(
        input.contactPatchAreaM2 / std::max(cellAreaM2, 1.0e-9),
        0.08,
        1.0);
    const double temperatureRiseC = roadEnergyJ * contactCoverage
        / heatCapacityJPerK;

    state.surfaceTemperatureC = static_cast<float>(std::clamp(
        static_cast<double>(state.surfaceTemperatureC) + temperatureRiseC,
        kMinimumTemperatureC,
        kMaximumTemperatureC));
    if (std::abs(state.surfaceTemperatureC - result.initialTemperatureC) > 1.0e-6f)
        ++located.page->revision;
    result.finalTemperatureC = state.surfaceTemperatureC;
    result.depositedHeatEnergyJ = roadEnergyJ * contactCoverage;
    ++m_stats.tireContactCount;
    m_stats.cumulativeTireHeatEnergyJ += result.depositedHeatEnergyJ;
    return result;
}

bool DynamicSurfaceThermal::rasterTrackPage(
    const DynamicSurfaceSystem& owner,
    const VirtualPageAddress& address,
    std::uint32_t outputResolution,
    std::vector<float>& trackRgba) const
{
    (void)owner;
    trackRgba.clear();
    if (outputResolution == 0u)
        return false;

    const auto found = m_pages.find(address);
    if (found == m_pages.end())
        return false;
    const ThermalPage& page = found->second;
    trackRgba.assign(
        static_cast<std::size_t>(outputResolution) * outputResolution * 4u,
        0.0f);

    if (outputResolution == kTrackAuthorityResolution)
    {
        const std::size_t texelCount =
            static_cast<std::size_t>(outputResolution) * outputResolution;
        for (std::size_t i = 0; i < texelCount; ++i)
        {
            if (page.staticCells[i].valid)
                trackRgba[i * 4u] = page.stateCells[i].surfaceTemperatureC;
        }
        return true;
    }

    for (std::uint32_t z = 0; z < outputResolution; ++z)
    {
        for (std::uint32_t x = 0; x < outputResolution; ++x)
        {
            const double u = (static_cast<double>(x) + 0.5)
                / static_cast<double>(outputResolution);
            const double v = (static_cast<double>(z) + 0.5)
                / static_cast<double>(outputResolution);
            const double gx = std::clamp(
                u * kTrackAuthorityResolution - 0.5,
                0.0,
                static_cast<double>(kTrackAuthorityResolution - 1u));
            const double gz = std::clamp(
                v * kTrackAuthorityResolution - 0.5,
                0.0,
                static_cast<double>(kTrackAuthorityResolution - 1u));
            const std::uint32_t x0 = static_cast<std::uint32_t>(std::floor(gx));
            const std::uint32_t z0 = static_cast<std::uint32_t>(std::floor(gz));
            const std::uint32_t x1 = std::min(x0 + 1u, kTrackAuthorityResolution - 1u);
            const std::uint32_t z1 = std::min(z0 + 1u, kTrackAuthorityResolution - 1u);
            const double tx = gx - x0;
            const double tz = gz - z0;

            const auto sampleCell = [&](std::uint32_t sx, std::uint32_t sz) {
                const std::size_t sampleIndex = cellIndex(sx, sz);
                return page.staticCells[sampleIndex].valid
                    ? static_cast<double>(page.stateCells[sampleIndex].surfaceTemperatureC)
                    : std::numeric_limits<double>::quiet_NaN();
            };
            const double t00 = sampleCell(x0, z0);
            const double t10 = sampleCell(x1, z0);
            const double t01 = sampleCell(x0, z1);
            const double t11 = sampleCell(x1, z1);
            double weighted = 0.0;
            double weight = 0.0;
            const auto add = [&](double value, double w) {
                if (std::isfinite(value) && w > 0.0)
                {
                    weighted += value * w;
                    weight += w;
                }
            };
            add(t00, (1.0 - tx) * (1.0 - tz));
            add(t10, tx * (1.0 - tz));
            add(t01, (1.0 - tx) * tz);
            add(t11, tx * tz);
            if (weight <= 0.0)
                continue;

            const std::size_t outIndex = (
                static_cast<std::size_t>(z) * outputResolution + x) * 4u;
            trackRgba[outIndex] = static_cast<float>(weighted / weight);
            // G/B/A remain reserved for DSURF05 adhered/loose/maturity state.
        }
    }
    return true;
}

std::uint64_t DynamicSurfaceThermal::pageRevision(
    const VirtualPageAddress& address) const
{
    const auto found = m_pages.find(address);
    return found == m_pages.end() ? 0u : found->second.revision;
}

void DynamicSurfaceThermal::refreshStats()
{
    m_stats.available = !m_pages.empty();
    m_stats.authorityPages = m_pages.size();
    m_stats.validTexels = 0u;
    m_stats.minimumTemperatureC = 0.0;
    m_stats.maximumTemperatureC = 0.0;
    m_stats.averageTemperatureC = 0.0;

    double sum = 0.0;
    bool first = true;
    for (const auto& [address, page] : m_pages)
    {
        (void)address;
        for (std::size_t i = 0; i < page.staticCells.size(); ++i)
        {
            if (!page.staticCells[i].valid)
                continue;
            const double temperature = page.stateCells[i].surfaceTemperatureC;
            if (first)
            {
                m_stats.minimumTemperatureC = temperature;
                m_stats.maximumTemperatureC = temperature;
                first = false;
            }
            else
            {
                m_stats.minimumTemperatureC = std::min(
                    m_stats.minimumTemperatureC, temperature);
                m_stats.maximumTemperatureC = std::max(
                    m_stats.maximumTemperatureC, temperature);
            }
            sum += temperature;
            ++m_stats.validTexels;
        }
    }
    if (m_stats.validTexels > 0u)
        m_stats.averageTemperatureC = sum / static_cast<double>(m_stats.validTexels);
}

} // namespace heritage::physics::dynamicsurface

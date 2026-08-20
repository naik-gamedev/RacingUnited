#include "DynamicSurfaceHydrology.hpp"

#include "DynamicSurfaceSystem.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>

namespace heritage::physics::dynamicsurface {
namespace {

constexpr float kInvalidSupportHeightM = 32768.0f;
constexpr double kMillimetresPerHourToMetresPerSecond = 1.0 / 3'600'000.0;
constexpr double kMaximumWaterDepthM = 0.032;
constexpr double kMinimumVisibleWaterDepthM = 0.0001;
constexpr std::size_t kMaximumNewAuthorityPagesPerAdvance = 16u;
constexpr double kWaterDensityKgM3 = 997.0;
constexpr double kLatentHeatJPerKg = 2'260'000.0;
constexpr std::uint32_t kFlowGuideResolution = 64u;

// 4-bit water ladder. Level 0 is dry and the first non-zero state remains the
// requested 0.1mm visible film. The remaining 14 levels spend more precision
// on shallow water while still reaching the 32mm hard ceiling.
constexpr std::array<double, 16> kWaterDepthLevelsM{{
    0.0,
    0.0001,
    0.0005,
    0.0010,
    0.0020,
    0.0030,
    0.0040,
    0.0060,
    0.0080,
    0.0100,
    0.0130,
    0.0160,
    0.0200,
    0.0240,
    0.0280,
    0.0320
}};

std::uint8_t waterLevel(const std::uint16_t packed)
{
    return static_cast<std::uint8_t>(packed & 0x0fu);
}
std::uint8_t dryLineLevel(const std::uint16_t packed)
{
    return static_cast<std::uint8_t>((packed >> 4u) & 0x0fu);
}
std::uint8_t flowXLevel(const std::uint16_t packed)
{
    return static_cast<std::uint8_t>((packed >> 8u) & 0x0fu);
}
std::uint8_t flowZLevel(const std::uint16_t packed)
{
    return static_cast<std::uint8_t>((packed >> 12u) & 0x0fu);
}

std::uint16_t packRgba4(
    std::uint8_t water,
    std::uint8_t dryLine,
    std::uint8_t flowX,
    std::uint8_t flowZ)
{
    return static_cast<std::uint16_t>(
        (std::min<std::uint8_t>(water, 15u))
        | (static_cast<std::uint16_t>(std::min<std::uint8_t>(dryLine, 15u)) << 4u)
        | (static_cast<std::uint16_t>(std::min<std::uint8_t>(flowX, 15u)) << 8u)
        | (static_cast<std::uint16_t>(std::min<std::uint8_t>(flowZ, 15u)) << 12u));
}

std::uint16_t withWaterLevel(std::uint16_t packed, std::uint8_t level)
{
    return static_cast<std::uint16_t>((packed & 0xfff0u) | (level & 0x0fu));
}
std::uint16_t withDryLineLevel(std::uint16_t packed, std::uint8_t level)
{
    return static_cast<std::uint16_t>(
        (packed & 0xff0fu) | (static_cast<std::uint16_t>(level & 0x0fu) << 4u));
}

std::uint8_t encodeWaterDepth4Bit(double depthM)
{
    if (!std::isfinite(depthM) || depthM < kMinimumVisibleWaterDepthM * 0.5)
        return 0u;
    depthM = std::clamp(depthM, 0.0, kMaximumWaterDepthM);
    std::uint8_t best = 0u;
    double bestError = (std::numeric_limits<double>::max)();
    for (std::uint8_t i = 0u; i < kWaterDepthLevelsM.size(); ++i)
    {
        const double error = std::abs(depthM - kWaterDepthLevelsM[i]);
        if (error < bestError)
        {
            bestError = error;
            best = i;
        }
    }
    return best;
}

double decodeWaterDepth4Bit(std::uint8_t level)
{
    return kWaterDepthLevelsM[std::min<std::size_t>(level, 15u)];
}

// Codes 7 and 8 form a two-code zero dead-band. That is intentional: when a
// GL_LINEAR filtered RGBA4 texture interpolates them, the midpoint is exactly
// the neutral 0.5 value. The remaining codes provide seven signed steps each.
std::uint8_t encodeFlowComponent4Bit(double value)
{
    value = std::clamp(value, -1.0, 1.0);
    if (std::abs(value) < 1.0 / 14.0)
        return value < 0.0 ? 7u : 8u;
    if (value < 0.0)
    {
        const int magnitude = std::clamp(
            static_cast<int>(std::lround(-value * 7.0)), 1, 7);
        return static_cast<std::uint8_t>(7 - magnitude);
    }
    const int magnitude = std::clamp(
        static_cast<int>(std::lround(value * 7.0)), 1, 7);
    return static_cast<std::uint8_t>(8 + magnitude);
}

double decodeFlowComponent4Bit(std::uint8_t level)
{
    level = std::min<std::uint8_t>(level, 15u);
    if (level == 7u || level == 8u)
        return 0.0;
    if (level < 7u)
        return -static_cast<double>(7u - level) / 7.0;
    return static_cast<double>(level - 8u) / 7.0;
}

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
    constexpr double epsilon = 1.0e-6;
    if (u < -epsilon || v < -epsilon || w < -epsilon)
        return false;

    heightM = u * triangle.a.y + v * triangle.b.y + w * triangle.c.y;
    return std::isfinite(heightM);
}

VirtualPageAddress hydroAddressForChunk(ChunkAddress chunk)
{
    return { chunk, { 0u, 0u, 0u } };
}

bool chunkHasHydroGeometry(const DynamicSurfaceChunk& chunk)
{
    return !chunk.staticTriangles().empty();
}

// Distance from a point to the 100m page rectangle in X/Z. A page whose edge
// intersects the 120m near radius therefore receives the 6Hz cadence even if
// its centre lies farther away.
double distanceSquaredToPageBounds(
    const heritage::math::DVec3& source,
    const VirtualPageAddress& address)
{
    const double minX = static_cast<double>(address.chunk.x) * kChunkSizeM;
    const double minZ = static_cast<double>(address.chunk.z) * kChunkSizeM;
    const double maxX = minX + kPhysicalPageWorldSizeM;
    const double maxZ = minZ + kPhysicalPageWorldSizeM;
    const double nearestX = std::clamp(source.x, minX, maxX);
    const double nearestZ = std::clamp(source.z, minZ, maxZ);
    const double dx = source.x - nearestX;
    const double dz = source.z - nearestZ;
    return dx * dx + dz * dz;
}

double requestedHzForPage(
    const DynamicSurfaceSystem& owner,
    const VirtualPageAddress& address)
{
    constexpr double nearRadiusSquared =
        UpdateCadence::puddleVisualRangeM * UpdateCadence::puddleVisualRangeM;
    for (const heritage::math::DVec3& source : owner.interestSources())
    {
        if (distanceSquaredToPageBounds(source, address) <= nearRadiusSquared)
            return UpdateCadence::hydroNearHz;
    }
    return UpdateCadence::hydroDistantHz;
}

std::uint32_t hydroCoordinate(double localM)
{
    const double clamped = std::clamp(localM, 0.0, kChunkSizeM - 1.0e-9);
    return static_cast<std::uint32_t>(std::clamp(
        static_cast<int>(std::floor(clamped / kHydroAuthorityTexelPitchM)),
        0,
        static_cast<int>(kHydroAuthorityResolution) - 1));
}

std::uint32_t guideCoordinate(double localM)
{
    const double pitch = kChunkSizeM / static_cast<double>(kFlowGuideResolution);
    const double clamped = std::clamp(localM, 0.0, kChunkSizeM - 1.0e-9);
    return static_cast<std::uint32_t>(std::clamp(
        static_cast<int>(std::floor(clamped / pitch)),
        0,
        static_cast<int>(kFlowGuideResolution) - 1));
}

bool supportAtGuideCell(
    const DynamicSurfaceChunk& chunk,
    std::uint32_t x,
    std::uint32_t z,
    float& heightM)
{
    const auto& candidates = chunk.staticTriangleIndicesForXzStateCell(x, z);
    if (candidates.empty())
        return false;

    const heritage::math::DVec3 origin = chunk.globalOrigin();
    const double pitch = kChunkSizeM / static_cast<double>(kFlowGuideResolution);
    const double worldX = origin.x + (static_cast<double>(x) + 0.5) * pitch;
    const double worldZ = origin.z + (static_cast<double>(z) + 0.5) * pitch;
    bool found = false;
    double bestHeight = -(std::numeric_limits<double>::max)();
    for (const std::uint32_t triangleIndex : candidates)
    {
        if (triangleIndex >= chunk.staticTriangles().size())
            continue;
        const auto& triangle = chunk.staticTriangles()[triangleIndex];
        double h = 0.0;
        if (!barycentricHeightXZ(triangle, worldX, worldZ, h))
            continue;
        if (!found || h > bestHeight)
        {
            bestHeight = h;
            found = true;
        }
    }
    if (!found)
    {
        // Boundary cells may have no centre hit even though geometry crosses
        // the bin. Use the highest candidate centroid as a cheap fallback.
        for (const std::uint32_t triangleIndex : candidates)
        {
            if (triangleIndex >= chunk.staticTriangles().size())
                continue;
            const auto& triangle = chunk.staticTriangles()[triangleIndex];
            const double h = (triangle.a.y + triangle.b.y + triangle.c.y) / 3.0;
            if (!found || h > bestHeight)
            {
                bestHeight = h;
                found = true;
            }
        }
    }
    if (!found)
        return false;
    heightM = static_cast<float>(bestHeight);
    return true;
}

} // namespace

void DynamicSurfaceHydrology::clear()
{
    m_pages.clear();
    m_stats = {};
    m_elapsedSeconds = 0.0;
}

void DynamicSurfaceHydrology::resetWater()
{
    for (auto& [address, page] : m_pages)
    {
        (void)address;
        for (StateCell& cell : page.stateCells)
        {
            cell.rgba4 = withWaterLevel(cell.rgba4, 0u);
            cell.rgba4 = withDryLineLevel(cell.rgba4, 0u);
        }
        page.atmosphericCarryM.fill(0.0);
        page.dryLineWashCarryM = 0.0;
        page.stepAccumulatorSeconds = 0.0;
        ++page.revision;
        refreshPageStats(page);
    }
    m_stats = {};
    refreshStats();
}

DynamicSurfaceHydrology::HydroPage* DynamicSurfaceHydrology::ensurePage(
    DynamicSurfaceSystem& owner,
    const VirtualPageAddress& requestedAddress,
    const SurfaceWeatherOutput* initialWeather)
{
    const VirtualPageAddress address = hydroAddressForChunk(requestedAddress.chunk);
    const DynamicSurfaceChunk* chunk = owner.findChunk(address.chunk);
    if (!chunk || !chunkHasHydroGeometry(*chunk))
        return nullptr;

    auto [it, inserted] = m_pages.try_emplace(address);
    HydroPage& page = it->second;
    if (inserted)
    {
        page.address = address;
        page.stateCells.resize(
            static_cast<std::size_t>(kHydroAuthorityResolution)
            * kHydroAuthorityResolution);
        page.cadenceHz = requestedHzForPage(owner, address);
        if (page.cadenceHz > 0.0)
        {
            page.stepAccumulatorSeconds =
                deterministicPagePhase01(address) / page.cadenceHz;
        }
    }

    if (!page.staticReady && !buildStaticPage(owner, page))
    {
        m_pages.erase(it);
        return nullptr;
    }

    if (inserted && initialWeather && initialWeather->valid)
    {
        const std::uint8_t initialWater = encodeWaterDepth4Bit(
            std::clamp(initialWeather->waterFilmDepthM, 0.0, kMaximumWaterDepthM));
        for (StateCell& cell : page.stateCells)
            cell.rgba4 = withWaterLevel(cell.rgba4, initialWater);
    }

    page.lastTouchedSeconds = m_elapsedSeconds;
    refreshPageStats(page);
    return &page;
}

bool DynamicSurfaceHydrology::buildStaticPage(
    const DynamicSurfaceSystem& owner,
    HydroPage& page) const
{
    const DynamicSurfaceChunk* chunk = owner.findChunk(page.address.chunk);
    if (!chunk || !chunkHasHydroGeometry(*chunk))
        return false;

    // Static flow guidance is derived once from the already-existing 64x64
    // X/Z triangle acceleration grid, then upsampled into the 256x256 RGBA4
    // state. No second 256x256 static/support page is retained in memory.
    constexpr std::size_t guideCellCount =
        static_cast<std::size_t>(kFlowGuideResolution) * kFlowGuideResolution;
    std::array<float, guideCellCount> support{};
    std::array<std::uint8_t, guideCellCount> valid{};
    support.fill(kInvalidSupportHeightM);

    for (std::uint32_t z = 0; z < kFlowGuideResolution; ++z)
    {
        for (std::uint32_t x = 0; x < kFlowGuideResolution; ++x)
        {
            float h = 0.0f;
            if (supportAtGuideCell(*chunk, x, z, h))
            {
                const std::size_t i = static_cast<std::size_t>(z)
                    * kFlowGuideResolution + x;
                support[i] = h;
                valid[i] = 1u;
            }
        }
    }

    double infiltrationSum = 0.0;
    double drainageSum = 0.0;
    double roughnessSum = 0.0;
    double depressionSum = 0.0;
    std::size_t parameterCount = 0u;
    for (const auto& triangle : chunk->staticTriangles())
    {
        infiltrationSum += std::max(
            0.0, static_cast<double>(triangle.infiltrationCapacityMmPerHour))
            * kMillimetresPerHourToMetresPerSecond;
        drainageSum += std::max(
            0.0, static_cast<double>(triangle.drainageCapacityMmPerHour))
            * kMillimetresPerHourToMetresPerSecond;
        roughnessSum += std::clamp(
            static_cast<double>(triangle.flowRoughness), 0.005, 1.0);
        depressionSum += std::max(
            0.0, static_cast<double>(triangle.depressionStorageM));
        ++parameterCount;
    }
    for (const auto& drain : chunk->staticDrains())
    {
        drainageSum += std::max(
            0.0, static_cast<double>(drain.capacityMmPerHour))
            * kMillimetresPerHourToMetresPerSecond;
    }
    const double invCount = parameterCount > 0u
        ? 1.0 / static_cast<double>(parameterCount)
        : 0.0;
    page.infiltrationMps = static_cast<float>(infiltrationSum * invCount);
    page.drainageMps = static_cast<float>(drainageSum * invCount);
    page.flowRoughness = static_cast<float>(parameterCount > 0u
        ? roughnessSum * invCount
        : 0.02);
    page.depressionStorageM = static_cast<float>(depressionSum * invCount);

    std::array<float, guideCellCount> flowX{};
    std::array<float, guideCellCount> flowZ{};
    const auto guideIndex = [](std::uint32_t x, std::uint32_t z) {
        return static_cast<std::size_t>(z) * kFlowGuideResolution + x;
    };
    for (std::uint32_t z = 0; z < kFlowGuideResolution; ++z)
    {
        for (std::uint32_t x = 0; x < kFlowGuideResolution; ++x)
        {
            const std::size_t centerIndex = guideIndex(x, z);
            if (!valid[centerIndex])
                continue;
            const std::uint32_t xl = x > 0u ? x - 1u : x;
            const std::uint32_t xr = std::min(x + 1u, kFlowGuideResolution - 1u);
            const std::uint32_t zd = z > 0u ? z - 1u : z;
            const std::uint32_t zu = std::min(z + 1u, kFlowGuideResolution - 1u);
            const float hc = support[centerIndex];
            const float hl = valid[guideIndex(xl, z)] ? support[guideIndex(xl, z)] : hc;
            const float hr = valid[guideIndex(xr, z)] ? support[guideIndex(xr, z)] : hc;
            const float hd = valid[guideIndex(x, zd)] ? support[guideIndex(x, zd)] : hc;
            const float hu = valid[guideIndex(x, zu)] ? support[guideIndex(x, zu)] : hc;
            float dx = hl - hr; // positive means downhill toward +X.
            float dz = hd - hu; // positive means downhill toward +Z.
            const float magnitude = std::hypot(dx, dz);
            if (magnitude > 1.0e-5f)
            {
                dx /= magnitude;
                dz /= magnitude;
                flowX[centerIndex] = dx;
                flowZ[centerIndex] = dz;
            }
        }
    }

    for (std::uint32_t z = 0; z < kHydroAuthorityResolution; ++z)
    {
        const std::uint32_t gz = std::min(
            z * kFlowGuideResolution / kHydroAuthorityResolution,
            kFlowGuideResolution - 1u);
        for (std::uint32_t x = 0; x < kHydroAuthorityResolution; ++x)
        {
            const std::uint32_t gx = std::min(
                x * kFlowGuideResolution / kHydroAuthorityResolution,
                kFlowGuideResolution - 1u);
            const std::size_t gi = guideIndex(gx, gz);
            page.stateCells[cellIndex(x, z)].rgba4 = packRgba4(
                0u,
                0u,
                encodeFlowComponent4Bit(flowX[gi]),
                encodeFlowComponent4Bit(flowZ[gi]));
        }
    }

    page.staticReady = true;
    return true;
}

DynamicSurfaceHydrology::LocatedCell DynamicSurfaceHydrology::locateMutable(
    DynamicSurfaceSystem& owner,
    const heritage::math::DVec3& globalPosition)
{
    LocatedCell result;
    if (!finite(globalPosition))
        return result;
    const ChunkAddress chunkAddress = DynamicSurfaceSystem::chunkAddressFor(globalPosition);
    HydroPage* page = ensurePage(owner, hydroAddressForChunk(chunkAddress), nullptr);
    if (!page)
        return result;
    const DynamicSurfaceChunk* chunk = owner.findChunk(chunkAddress);
    if (!chunk)
        return result;
    const heritage::math::DVec3 origin = chunk->globalOrigin();
    result.x = hydroCoordinate(globalPosition.x - origin.x);
    result.z = hydroCoordinate(globalPosition.z - origin.z);
    result.page = page;
    result.constPage = page;
    result.supportHeightM = static_cast<float>(globalPosition.y);
    return result;
}

DynamicSurfaceHydrology::LocatedCell DynamicSurfaceHydrology::locateConst(
    const DynamicSurfaceSystem& owner,
    const heritage::math::DVec3& globalPosition) const
{
    LocatedCell result;
    if (!finite(globalPosition))
        return result;
    const ChunkAddress chunkAddress = DynamicSurfaceSystem::chunkAddressFor(globalPosition);
    const auto found = m_pages.find(hydroAddressForChunk(chunkAddress));
    if (found == m_pages.end())
        return result;
    const DynamicSurfaceChunk* chunk = owner.findChunk(chunkAddress);
    if (!chunk)
        return result;
    const heritage::math::DVec3 origin = chunk->globalOrigin();
    result.x = hydroCoordinate(globalPosition.x - origin.x);
    result.z = hydroCoordinate(globalPosition.z - origin.z);
    result.constPage = &found->second;
    result.supportHeightM = static_cast<float>(globalPosition.y);
    return result;
}

std::optional<VirtualPageAddress> DynamicSurfaceHydrology::neighbourPage(
    const DynamicSurfaceSystem& owner,
    const VirtualPageAddress& address,
    int pageDx,
    int pageDz) const
{
    ChunkAddress chunkAddress = address.chunk;
    chunkAddress.x += pageDx;
    chunkAddress.z += pageDz;
    const DynamicSurfaceChunk* chunk = owner.findChunk(chunkAddress);
    if (!chunk || !chunkHasHydroGeometry(*chunk))
        return std::nullopt;
    return hydroAddressForChunk(chunkAddress);
}

bool DynamicSurfaceHydrology::simulateEnvironment(
    HydroPage& page,
    const SurfaceWeatherOutput& weatherOutput,
    double stepSeconds)
{
    if (!weatherOutput.valid || stepSeconds <= 0.0)
        return false;

    const double rainMps = std::max(
        weatherOutput.precipitationRateMmPerHour, 0.0)
        * kMillimetresPerHourToMetresPerSecond;
    const double weatherDrainMps = std::max(
        weatherOutput.drainageRateMmPerHour, 0.0)
        * kMillimetresPerHourToMetresPerSecond;
    const double evaporationMps = std::max(
        weatherOutput.evaporationRateMmPerHour, 0.0)
        * kMillimetresPerHourToMetresPerSecond;

    std::array<std::size_t, 16> present{};
    for (const StateCell& cell : page.stateCells)
        ++present[waterLevel(cell.rgba4)];

    std::array<std::uint8_t, 16> targetLevel{};
    for (std::uint8_t level = 0u; level < 16u; ++level)
    {
        targetLevel[level] = level;
        if (present[level] == 0u)
            continue;

        const double depth = decodeWaterDepth4Bit(level);
        double netMps = rainMps - static_cast<double>(page.infiltrationMps)
            - evaporationMps;
        if (depth > static_cast<double>(page.depressionStorageM))
            netMps -= static_cast<double>(page.drainageMps) + weatherDrainMps;

        page.atmosphericCarryM[level] += netMps * stepSeconds;
        double& carry = page.atmosphericCarryM[level];
        std::uint8_t target = level;
        int transitions = 0;
        while (carry > 0.0 && target < 15u && transitions < 8)
        {
            const double gap = kWaterDepthLevelsM[target + 1u]
                - kWaterDepthLevelsM[target];
            if (carry + 1.0e-12 < gap)
                break;
            carry -= gap;
            ++target;
            ++transitions;
        }
        while (carry < 0.0 && target > 0u && transitions < 8)
        {
            const double gap = kWaterDepthLevelsM[target]
                - kWaterDepthLevelsM[target - 1u];
            if (-carry + 1.0e-12 < gap)
                break;
            carry += gap;
            --target;
            ++transitions;
        }
        targetLevel[level] = target;
    }

    bool changed = false;
    const double cellAreaM2 =
        kHydroAuthorityTexelPitchM * kHydroAuthorityTexelPitchM;
    for (StateCell& cell : page.stateCells)
    {
        const std::uint8_t before = waterLevel(cell.rgba4);
        const std::uint8_t after = targetLevel[before];
        if (before == after)
            continue;
        const double beforeDepth = decodeWaterDepth4Bit(before);
        const double afterDepth = decodeWaterDepth4Bit(after);
        cell.rgba4 = withWaterLevel(cell.rgba4, after);
        const double volumeDelta = (afterDepth - beforeDepth) * cellAreaM2;
        if (volumeDelta > 0.0)
            m_stats.cumulativeRainVolumeM3 += volumeDelta;
        else
            m_stats.cumulativeDrainageVolumeM3 += -volumeDelta;
        changed = true;
    }

    // Rain progressively erases the persistent dry-line memory. One millimetre
    // of accumulated rainfall removes one of the 16 dry-line steps.
    page.dryLineWashCarryM += rainMps * stepSeconds;
    int washSteps = 0;
    while (page.dryLineWashCarryM >= 0.001 && washSteps < 4)
    {
        page.dryLineWashCarryM -= 0.001;
        ++washSteps;
    }
    if (washSteps > 0)
    {
        for (StateCell& cell : page.stateCells)
        {
            const std::uint8_t before = dryLineLevel(cell.rgba4);
            const std::uint8_t after = static_cast<std::uint8_t>(
                before > washSteps ? before - washSteps : 0u);
            if (after != before)
            {
                cell.rgba4 = withDryLineLevel(cell.rgba4, after);
                changed = true;
            }
        }
    }

    if (changed)
    {
        ++page.revision;
        refreshPageStats(page);
    }
    return changed;
}

bool DynamicSurfaceHydrology::simulateFlow(
    DynamicSurfaceSystem& owner,
    HydroPage& page,
    double stepSeconds,
    double pageHz)
{
    if (stepSeconds <= 0.0)
        return false;

    bool anyWet = page.wetCellCount > 0u;
    if (!anyWet)
        return false;

    bool changed = false;
    const double nearScale = pageHz >= 1.0 ? 1.0 : 0.45;
    const std::uint64_t parity = m_stats.simulationStepCount & 1ull;

    for (std::uint32_t z = 0; z < kHydroAuthorityResolution; ++z)
    {
        for (std::uint32_t x = 0; x < kHydroAuthorityResolution; ++x)
        {
            if (((static_cast<std::uint64_t>(x + z) + parity) & 1ull) != 0ull)
                continue;
            StateCell& donorCell = page.stateCells[cellIndex(x, z)];
            const std::uint8_t donorLevel = waterLevel(donorCell.rgba4);
            if (donorLevel <= 1u)
                continue;

            const double fx = decodeFlowComponent4Bit(flowXLevel(donorCell.rgba4));
            const double fz = decodeFlowComponent4Bit(flowZLevel(donorCell.rgba4));
            const double magnitude = std::hypot(fx, fz);
            if (magnitude < 0.08)
                continue;

            int dx = 0;
            int dz = 0;
            if (std::abs(fx) >= std::abs(fz))
                dx = fx >= 0.0 ? 1 : -1;
            else
                dz = fz >= 0.0 ? 1 : -1;

            int tx = static_cast<int>(x) + dx;
            int tz = static_cast<int>(z) + dz;
            HydroPage* targetPage = &page;
            if (tx < 0 || tx >= static_cast<int>(kHydroAuthorityResolution)
                || tz < 0 || tz >= static_cast<int>(kHydroAuthorityResolution))
            {
                const int pageDx = tx < 0 ? -1
                    : (tx >= static_cast<int>(kHydroAuthorityResolution) ? 1 : 0);
                const int pageDz = tz < 0 ? -1
                    : (tz >= static_cast<int>(kHydroAuthorityResolution) ? 1 : 0);
                const auto neighbour = neighbourPage(owner, page.address, pageDx, pageDz);
                if (!neighbour)
                    continue;
                auto found = m_pages.find(*neighbour);
                if (found == m_pages.end())
                    continue;
                targetPage = &found->second;
                if (tx < 0) tx += static_cast<int>(kHydroAuthorityResolution);
                if (tx >= static_cast<int>(kHydroAuthorityResolution)) tx -= static_cast<int>(kHydroAuthorityResolution);
                if (tz < 0) tz += static_cast<int>(kHydroAuthorityResolution);
                if (tz >= static_cast<int>(kHydroAuthorityResolution)) tz -= static_cast<int>(kHydroAuthorityResolution);
            }

            StateCell& receiverCell = targetPage->stateCells[cellIndex(
                static_cast<std::uint32_t>(tx), static_cast<std::uint32_t>(tz))];
            const double donorDepth = decodeWaterDepth4Bit(donorLevel);
            const double receiverDepth = decodeWaterDepth4Bit(waterLevel(receiverCell.rgba4));
            if (receiverDepth >= kMaximumWaterDepthM - 1.0e-9)
                continue;

            const double fraction = std::clamp(
                stepSeconds * (0.10 + 0.20 * magnitude) * nearScale,
                0.0,
                pageHz >= 1.0 ? 0.12 : 0.35);
            const double transferDepth = std::min(
                donorDepth * fraction,
                kMaximumWaterDepthM - receiverDepth);
            if (transferDepth < 0.00005)
                continue;

            const std::uint8_t donorAfter = encodeWaterDepth4Bit(
                std::max(donorDepth - transferDepth, 0.0));
            const std::uint8_t receiverAfter = encodeWaterDepth4Bit(
                std::min(receiverDepth + transferDepth, kMaximumWaterDepthM));
            const std::uint8_t receiverBefore = waterLevel(receiverCell.rgba4);
            if (donorAfter == donorLevel && receiverAfter == receiverBefore)
                continue;

            donorCell.rgba4 = withWaterLevel(donorCell.rgba4, donorAfter);
            receiverCell.rgba4 = withWaterLevel(receiverCell.rgba4, receiverAfter);
            changed = true;
            if (targetPage != &page)
            {
                ++targetPage->revision;
                refreshPageStats(*targetPage);
            }
        }
    }

    if (changed)
    {
        ++page.revision;
        refreshPageStats(page);
    }
    return changed;
}

void DynamicSurfaceHydrology::advance(
    DynamicSurfaceSystem& owner,
    const SurfaceWeatherDescription& weather,
    const SurfaceWeatherOutput& weatherOutput,
    double deltaTimeSeconds)
{
    (void)weather;
    if (!std::isfinite(deltaTimeSeconds) || deltaTimeSeconds <= 0.0)
        return;

    const auto started = std::chrono::steady_clock::now();
    m_elapsedSeconds += deltaTimeSeconds;

    struct MissingPageCandidate
    {
        VirtualPageAddress address{};
        double nearestInterestDistanceSquared = 0.0;
    };
    std::vector<MissingPageCandidate> missingPages;
    for (const auto& [chunkAddress, chunk] : owner.m_chunks)
    {
        if (!chunkHasHydroGeometry(chunk))
            continue;
        const VirtualPageAddress address = hydroAddressForChunk(chunkAddress);
        if (m_pages.find(address) != m_pages.end())
            continue;
        double nearest = (std::numeric_limits<double>::max)();
        if (owner.interestSources().empty())
            nearest = 0.0;
        else
        {
            for (const auto& source : owner.interestSources())
                nearest = std::min(nearest, distanceSquaredToPageBounds(source, address));
        }
        missingPages.push_back({ address, nearest });
    }
    std::sort(
        missingPages.begin(), missingPages.end(),
        [](const MissingPageCandidate& a, const MissingPageCandidate& b) {
            if (a.nearestInterestDistanceSquared != b.nearestInterestDistanceSquared)
                return a.nearestInterestDistanceSquared < b.nearestInterestDistanceSquared;
            return a.address < b.address;
        });
    const std::size_t pagesToBuild = std::min(
        missingPages.size(), kMaximumNewAuthorityPagesPerAdvance);
    for (std::size_t i = 0; i < pagesToBuild; ++i)
        ensurePage(owner, missingPages[i].address, &weatherOutput);

    m_stats.activePages = 0u;
    m_stats.cadence30HzPages = 0u;
    m_stats.cadence20HzPages = 0u;
    m_stats.cadence6HzPages = 0u;
    m_stats.cadence2HzPages = 0u;
    m_stats.cadenceDistantPages = 0u;
    m_stats.scheduledPagesThisAdvance = 0u;

    for (auto& [address, page] : m_pages)
    {
        const double hz = requestedHzForPage(owner, address);
        const double interval = 1.0 / hz;
        if (hz >= UpdateCadence::hydroNearHz - 1.0e-9)
        {
            ++m_stats.activePages;
            ++m_stats.cadence6HzPages;
        }
        else
        {
            ++m_stats.cadenceDistantPages;
        }

        if (std::abs(page.cadenceHz - hz) > 1.0e-12)
        {
            page.cadenceHz = hz;
            page.stepAccumulatorSeconds = std::min(
                page.stepAccumulatorSeconds,
                interval * deterministicPagePhase01(address));
        }
        page.stepAccumulatorSeconds += deltaTimeSeconds;
        page.lastTouchedSeconds = m_elapsedSeconds;

        int catchup = 0;
        const int maximumCatchup = hz >= 1.0 ? 3 : 1;
        while (page.stepAccumulatorSeconds + 1.0e-12 >= interval
            && catchup < maximumCatchup)
        {
            simulateEnvironment(page, weatherOutput, interval);
            simulateFlow(owner, page, interval, hz);
            page.stepAccumulatorSeconds -= interval;
            ++m_stats.simulationStepCount;
            ++catchup;
        }
        if (catchup > 0)
            ++m_stats.scheduledPagesThisAdvance;
        if (page.stepAccumulatorSeconds > interval * 2.0)
            page.stepAccumulatorSeconds = interval * 2.0;
    }

    refreshStats();
    m_stats.lastStepMilliseconds =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
}

DynamicSurfaceHydroSample DynamicSurfaceHydrology::sample(
    const DynamicSurfaceSystem& owner,
    const heritage::math::DVec3& globalPosition) const
{
    DynamicSurfaceHydroSample result;
    const LocatedCell located = locateConst(owner, globalPosition);
    if (!located.constPage)
        return result;

    const std::uint16_t packed = located.constPage->stateCells[
        cellIndex(located.x, located.z)].rgba4;
    const double depth = decodeWaterDepth4Bit(waterLevel(packed));
    const double dryLine = static_cast<double>(dryLineLevel(packed)) / 15.0;
    const double flowX = decodeFlowComponent4Bit(flowXLevel(packed));
    const double flowZ = decodeFlowComponent4Bit(flowZLevel(packed));

    result.valid = true;
    result.surfaceSheetId = 0u;
    result.surfaceElevationM = located.supportHeightM;
    result.waterDepthM = depth;
    result.moisture = std::clamp(
        (depth > 0.0 ? 0.65 + depth * 8.0 : 0.0) * (1.0 - 0.55 * dryLine),
        0.0,
        1.0);
    result.wetness = std::clamp(
        std::max(result.moisture, 1.0 - std::exp(-depth * 650.0))
            * (1.0 - 0.35 * dryLine),
        0.0,
        1.0);
    result.flowVelocityXMps = flowX;
    result.flowVelocityZMps = flowZ;
    return result;
}

DynamicSurfaceHydroTireResult DynamicSurfaceHydrology::applyTireContact(
    DynamicSurfaceSystem& owner,
    const heritage::math::DVec3& globalPosition,
    const DynamicSurfaceHydroTireInput& input)
{
    DynamicSurfaceHydroTireResult result;
    if (!std::isfinite(input.deltaTimeSeconds) || input.deltaTimeSeconds <= 0.0)
        return result;

    LocatedCell located = locateMutable(owner, globalPosition);
    if (!located.page)
        return result;

    StateCell& cell = located.page->stateCells[cellIndex(located.x, located.z)];
    const std::uint8_t initialLevel = waterLevel(cell.rgba4);
    const double initialDepth = decodeWaterDepth4Bit(initialLevel);
    result.valid = true;
    result.initialWaterDepthM = initialDepth;
    ++m_stats.tireContactCount;

    const double cellAreaM2 =
        kHydroAuthorityTexelPitchM * kHydroAuthorityTexelPitchM;
    if (initialDepth <= 1.0e-12)
    {
        result.finalWaterDepthM = 0.0;
        return result;
    }

    const double width = std::clamp(input.contactPatchWidthM, 0.03, 1.5);
    const double travel = (
        std::abs(input.forwardSpeedMps)
        + 0.35 * std::abs(input.lateralSpeedMps)) * input.deltaTimeSeconds;
    const double sweptCoverage = std::clamp(
        width * travel / std::max(cellAreaM2, 1.0e-9), 0.0, 1.0);
    const double loadScale = std::clamp(
        input.normalLoadN / std::max(input.nominalLoadN, 100.0),
        0.15,
        3.0);
    const double treadScale = std::clamp(
        0.25 + 1.60 * input.treadVoidRatio,
        0.25,
        1.60);
    const double clearingFraction = std::clamp(
        1.0 - std::exp(-2.25 * sweptCoverage * loadScale * treadScale),
        0.0,
        0.75);

    const double initialVolumeM3 = initialDepth * cellAreaM2;
    const double displacedVolumeM3 = initialVolumeM3 * clearingFraction;
    const double frictionEvaporationM3 = std::min(
        std::max(initialVolumeM3 - displacedVolumeM3, 0.0),
        std::max(input.slipDissipationWatts, 0.0)
            * input.deltaTimeSeconds * 0.02
            / (kLatentHeatJPerKg * kWaterDensityKgM3));
    const double removedVolumeM3 = displacedVolumeM3 + frictionEvaporationM3;
    const double remainingDepth = std::max(
        (initialVolumeM3 - removedVolumeM3) / cellAreaM2,
        0.0);
    const std::uint8_t finalLevel = encodeWaterDepth4Bit(remainingDepth);
    if (finalLevel != initialLevel)
    {
        cell.rgba4 = withWaterLevel(cell.rgba4, finalLevel);
        const int boost = std::clamp(
            static_cast<int>(initialLevel) - static_cast<int>(finalLevel),
            1,
            3);
        cell.rgba4 = withDryLineLevel(
            cell.rgba4,
            static_cast<std::uint8_t>(std::min<int>(
                15,
                static_cast<int>(dryLineLevel(cell.rgba4)) + boost)));
        ++located.page->revision;
    }

    int dx = 0;
    int dz = 0;
    if (std::abs(input.forward.x) >= std::abs(input.forward.z))
        dx = input.forward.x >= 0.0f ? 1 : -1;
    else
        dz = input.forward.z >= 0.0f ? 1 : -1;

    double redistributedM3 = 0.0;
    if (displacedVolumeM3 > 0.0 && (dx != 0 || dz != 0))
    {
        int tx = static_cast<int>(located.x) + dx;
        int tz = static_cast<int>(located.z) + dz;
        HydroPage* targetPage = located.page;
        if (tx < 0 || tx >= static_cast<int>(kHydroAuthorityResolution)
            || tz < 0 || tz >= static_cast<int>(kHydroAuthorityResolution))
        {
            const int pageDx = tx < 0 ? -1
                : (tx >= static_cast<int>(kHydroAuthorityResolution) ? 1 : 0);
            const int pageDz = tz < 0 ? -1
                : (tz >= static_cast<int>(kHydroAuthorityResolution) ? 1 : 0);
            const auto neighbour = neighbourPage(owner, located.page->address, pageDx, pageDz);
            if (neighbour)
                targetPage = ensurePage(owner, *neighbour, nullptr);
            if (tx < 0) tx += static_cast<int>(kHydroAuthorityResolution);
            if (tx >= static_cast<int>(kHydroAuthorityResolution)) tx -= static_cast<int>(kHydroAuthorityResolution);
            if (tz < 0) tz += static_cast<int>(kHydroAuthorityResolution);
            if (tz >= static_cast<int>(kHydroAuthorityResolution)) tz -= static_cast<int>(kHydroAuthorityResolution);
        }
        if (targetPage)
        {
            StateCell& target = targetPage->stateCells[cellIndex(
                static_cast<std::uint32_t>(tx), static_cast<std::uint32_t>(tz))];
            const double targetDepth = decodeWaterDepth4Bit(waterLevel(target.rgba4));
            const double targetCapacity =
                std::max(kMaximumWaterDepthM - targetDepth, 0.0) * cellAreaM2;
            redistributedM3 = std::min(displacedVolumeM3 * 0.35, targetCapacity);
            if (redistributedM3 > 1.0e-12)
            {
                const std::uint8_t targetLevel = encodeWaterDepth4Bit(
                    targetDepth + redistributedM3 / cellAreaM2);
                if (targetLevel != waterLevel(target.rgba4))
                {
                    target.rgba4 = withWaterLevel(target.rgba4, targetLevel);
                    ++targetPage->revision;
                    refreshPageStats(*targetPage);
                }
            }
        }
    }

    result.removedVolumeM3 = removedVolumeM3;
    result.redistributedVolumeM3 = redistributedM3;
    result.sprayVolumeM3 = std::max(displacedVolumeM3 - redistributedM3, 0.0);
    result.frictionEvaporatedVolumeM3 = frictionEvaporationM3;
    result.finalWaterDepthM = decodeWaterDepth4Bit(waterLevel(cell.rgba4));
    m_stats.cumulativeTireClearedVolumeM3 += removedVolumeM3;
    m_stats.cumulativeTireSprayVolumeM3 += result.sprayVolumeM3;
    refreshPageStats(*located.page);
    return result;
}

bool DynamicSurfaceHydrology::rasterPage(
    const DynamicSurfaceSystem& owner,
    const VirtualPageAddress& requestedAddress,
    std::uint32_t outputResolution,
    std::vector<std::uint16_t>& hydroRgba4) const
{
    (void)owner;
    hydroRgba4.clear();
    // LIVETRACK03 intentionally has no presentation resample or second Hydro
    // resolution. Simulation and renderer both consume the exact packed
    // 256x256 RGBA4 field. The upload path transfers these 16-bit texels
    // directly with GL_UNSIGNED_SHORT_4_4_4_4_REV, so there is no float
    // expansion or re-quantization between authority and rendering.
    if (outputResolution != kHydroAuthorityResolution)
        return false;

    const VirtualPageAddress address = hydroAddressForChunk(requestedAddress.chunk);
    const auto found = m_pages.find(address);
    if (found == m_pages.end())
        return false;
    const HydroPage& page = found->second;

    const std::size_t texelCount =
        static_cast<std::size_t>(kHydroAuthorityResolution)
        * kHydroAuthorityResolution;
    if (page.stateCells.size() != texelCount)
        return false;
    hydroRgba4.resize(texelCount);
    for (std::size_t i = 0; i < texelCount; ++i)
        hydroRgba4[i] = page.stateCells[i].rgba4;
    return true;
}

std::uint64_t DynamicSurfaceHydrology::pageRevision(
    const VirtualPageAddress& address) const
{
    if (address.page.sheet != 0u)
        return 0u;
    const auto found = m_pages.find(hydroAddressForChunk(address.chunk));
    return found == m_pages.end() ? 0u : found->second.revision;
}

bool DynamicSurfaceHydrology::setUniformWaterDepthForLab(
    DynamicSurfaceSystem& owner,
    double waterDepthM)
{
    if (!std::isfinite(waterDepthM) || waterDepthM < 0.0)
        return false;
    waterDepthM = std::clamp(waterDepthM, 0.0, kMaximumWaterDepthM);
    for (const auto& [chunkAddress, chunk] : owner.m_chunks)
    {
        if (!chunkHasHydroGeometry(chunk))
            continue;
        ensurePage(owner, hydroAddressForChunk(chunkAddress), nullptr);
    }
    const std::uint8_t level = encodeWaterDepth4Bit(waterDepthM);
    for (auto& [address, page] : m_pages)
    {
        (void)address;
        for (StateCell& cell : page.stateCells)
            cell.rgba4 = withWaterLevel(cell.rgba4, level);
        page.atmosphericCarryM.fill(0.0);
        ++page.revision;
        refreshPageStats(page);
    }
    refreshStats();
    return true;
}

void DynamicSurfaceHydrology::refreshPageStats(HydroPage& page)
{
    page.wetCellCount = 0u;
    page.cachedWaterVolumeM3 = 0.0;
    page.cachedMaximumWaterDepthM = 0.0;
    page.cachedMaximumFlowMagnitude = 0.0;
    const double cellAreaM2 =
        kHydroAuthorityTexelPitchM * kHydroAuthorityTexelPitchM;
    for (const StateCell& cell : page.stateCells)
    {
        const std::uint16_t packed = cell.rgba4;
        const std::uint8_t water = waterLevel(packed);
        const double depth = decodeWaterDepth4Bit(water);
        if (water != 0u)
            ++page.wetCellCount;
        page.cachedWaterVolumeM3 += depth * cellAreaM2;
        page.cachedMaximumWaterDepthM = std::max(
            page.cachedMaximumWaterDepthM,
            depth);
        page.cachedMaximumFlowMagnitude = std::max(
            page.cachedMaximumFlowMagnitude,
            std::hypot(
                decodeFlowComponent4Bit(flowXLevel(packed)),
                decodeFlowComponent4Bit(flowZLevel(packed))));
    }
}

void DynamicSurfaceHydrology::refreshStats()
{
    m_stats.available = !m_pages.empty();
    m_stats.authorityPages = m_pages.size();
    m_stats.validTexels = m_pages.size()
        * static_cast<std::size_t>(kHydroAuthorityResolution)
        * kHydroAuthorityResolution;
    m_stats.wetTexels = 0u;
    m_stats.waterVolumeM3 = 0.0;
    m_stats.maximumWaterDepthM = 0.0;
    m_stats.maximumFlowSpeedMps = 0.0;
    for (const auto& [address, page] : m_pages)
    {
        (void)address;
        m_stats.wetTexels += page.wetCellCount;
        m_stats.waterVolumeM3 += page.cachedWaterVolumeM3;
        m_stats.maximumWaterDepthM = std::max(
            m_stats.maximumWaterDepthM,
            page.cachedMaximumWaterDepthM);
        m_stats.maximumFlowSpeedMps = std::max(
            m_stats.maximumFlowSpeedMps,
            page.cachedMaximumFlowMagnitude);
    }
}

} // namespace heritage::physics::dynamicsurface

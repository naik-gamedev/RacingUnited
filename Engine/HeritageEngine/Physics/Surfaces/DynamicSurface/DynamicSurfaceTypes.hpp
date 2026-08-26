#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "../../../Core/Math/Math.hpp"

namespace heritage::physics::dynamicsurface {

inline constexpr double kChunkSizeM = 100.0;
// DSURF04C: the persistent world tile is the storage, simulation and texture
// unit. There is no hidden 4096x4096 logical domain and no 6.25m sub-page
// hierarchy in the active Hydro/Track path.
inline constexpr std::uint32_t kLogicalResolution = 64;
inline constexpr std::uint32_t kPhysicalPageResolution = 64;
inline constexpr std::uint32_t kPagesPerAxis = 1;
inline constexpr std::uint32_t kPagesPerSheet = 1;
inline constexpr double kLogicalTexelPitchM =
    kChunkSizeM / static_cast<double>(kLogicalResolution);
inline constexpr double kPhysicalPageWorldSizeM = kChunkSizeM;

static_assert(kLogicalResolution == kPhysicalPageResolution);
static_assert(kPagesPerAxis == 1);
static_assert(kPagesPerSheet == 1);
static_assert(kLogicalTexelPitchM == 1.5625);

struct ChunkAddress
{
    std::int64_t x = 0;
    std::int64_t z = 0;

    constexpr bool operator==(const ChunkAddress&) const = default;
    constexpr bool operator<(const ChunkAddress& other) const
    {
        return x < other.x || (x == other.x && z < other.z);
    }
};

struct PageAddress
{
    std::uint16_t sheet = 0;
    std::uint8_t x = 0;
    std::uint8_t z = 0;

    constexpr bool operator==(const PageAddress&) const = default;
};

// Logical physical state sampled by tires/rendering. Storage formats may be
// quantized or split into multiple textures, but APIs speak in physical units.
struct DynamicSurfaceSample
{
    bool valid = false;
    float waterDepthM = 0.0f;
    float moisture = 0.0f;
    float waterVelocityXMps = 0.0f;
    float waterVelocityZMps = 0.0f;

    float surfaceTemperatureC = 20.0f;
    float adheredRubber = 0.0f;
    float looseRubberMassKgPerM2 = 0.0f;
    float marbleMaturity = 0.0f;

    float dirt = 0.0f;
    float mud = 0.0f;
    float looseDebris = 0.0f;
    float reservedContamination = 0.0f;
};

enum class StatePlane : std::uint8_t
{
    Hydro = 0,
    Track,
    Contamination,
    Count
};

// LIVETRACK03: Hydro is a single packed RGBA4 field per 100m X/Z tile.
// 256x256 gives 39.0625cm/cell. Track/rubber/temperature remain independently
// 64x64 because they are a different state plane.
inline constexpr std::uint32_t kHydroAuthorityResolution = 256u;
inline constexpr double kHydroAuthorityTexelPitchM =
    kChunkSizeM / static_cast<double>(kHydroAuthorityResolution);
inline constexpr std::uint32_t kHydroAuthorityLevels = 16u;
inline constexpr std::uint32_t kTrackAuthorityResolution = 64u;
inline constexpr double kTrackAuthorityTexelPitchM =
    kChunkSizeM / static_cast<double>(kTrackAuthorityResolution);
static_assert(kTrackAuthorityResolution == kPhysicalPageResolution);
static_assert(kHydroAuthorityResolution == 256u);
static_assert(kHydroAuthorityTexelPitchM == 0.390625);

struct StatePlaneDescriptor
{
    StatePlane plane = StatePlane::Hydro;
    std::uint32_t resolution = kPhysicalPageResolution;
    std::uint8_t bytesPerTexel = 0;
    bool floatingPoint = false;
    bool mipmapped = false;
};

// GPU physical-slot accounting mirrors the actual live formats. Hydro is one
// base-level RGBA4 texel (16 bits total), while Track/Contamination retain the
// existing 64x64 mip chains.
inline constexpr std::array<StatePlaneDescriptor, 3> kStatePlaneLayout{{
    { StatePlane::Hydro, kHydroAuthorityResolution, 2u, false, false }, // RGBA4
    { StatePlane::Track, kPhysicalPageResolution, 8u, true, true },    // RGBA16F
    { StatePlane::Contamination, kPhysicalPageResolution, 4u, false, true } // RGBA8
}};

constexpr std::size_t mipTexelCount(std::uint32_t resolution)
{
    std::size_t total = 0;
    while (resolution > 0u)
    {
        total += static_cast<std::size_t>(resolution)
            * static_cast<std::size_t>(resolution);
        resolution >>= 1u;
    }
    return total;
}

constexpr std::size_t bytesPerPhysicalPage()
{
    std::size_t bytes = 0;
    for (const StatePlaneDescriptor& plane : kStatePlaneLayout)
    {
        bytes += static_cast<std::size_t>(plane.resolution)
            * static_cast<std::size_t>(plane.resolution)
            * plane.bytesPerTexel;
    }
    return bytes;
}

inline constexpr std::size_t kBytesPerPhysicalPage = bytesPerPhysicalPage();
static_assert(kBytesPerPhysicalPage == 180'224);

constexpr std::size_t physicalPageMipTexelCount()
{
    return mipTexelCount(kPhysicalPageResolution);
}

constexpr std::size_t bytesPerPhysicalPageWithMipChain()
{
    std::size_t bytes = 0;
    for (const StatePlaneDescriptor& plane : kStatePlaneLayout)
    {
        const std::size_t texels = plane.mipmapped
            ? mipTexelCount(plane.resolution)
            : static_cast<std::size_t>(plane.resolution) * plane.resolution;
        bytes += texels * plane.bytesPerTexel;
    }
    return bytes;
}

inline constexpr std::size_t kPhysicalPageMipLevels = 7u; // 64 -> 1.
inline constexpr std::size_t kBytesPerPhysicalPageWithMipChain =
    bytesPerPhysicalPageWithMipChain();
static_assert(kBytesPerPhysicalPageWithMipChain == 196'604);



struct DynamicSurfaceHydroSample
{
    bool valid = false;
    std::uint16_t surfaceSheetId = 0;
    double surfaceElevationM = 0.0;
    double waterDepthM = 0.0;
    double moisture = 0.0;
    double wetness = 0.0;
    double flowVelocityXMps = 0.0;
    double flowVelocityZMps = 0.0;
};

struct DynamicSurfaceHydroTireInput
{
    double deltaTimeSeconds = 0.001;
    double contactPatchLengthM = 0.12;
    double contactPatchWidthM = 0.20;
    double contactPatchAreaM2 = 0.024;
    double normalLoadN = 3500.0;
    double nominalLoadN = 3500.0;
    double forwardSpeedMps = 0.0;
    double lateralSpeedMps = 0.0;
    double treadVoidRatio = 0.30;
    double slipDissipationWatts = 0.0;
    heritage::math::Vec3 forward{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 right{ 1.0f, 0.0f, 0.0f };
};

struct DynamicSurfaceHydroTireResult
{
    bool valid = false;
    double initialWaterDepthM = 0.0;
    double finalWaterDepthM = 0.0;
    double removedVolumeM3 = 0.0;
    double redistributedVolumeM3 = 0.0;
    double sprayVolumeM3 = 0.0;
    double frictionEvaporatedVolumeM3 = 0.0;
};

struct DynamicSurfaceHydroStats
{
    bool available = false;
    std::size_t authorityPages = 0;
    std::size_t activePages = 0;
    std::size_t validTexels = 0;
    std::size_t wetTexels = 0;
    std::size_t cadence30HzPages = 0;
    std::size_t cadence20HzPages = 0;
    std::size_t cadence6HzPages = 0;
    std::size_t cadence2HzPages = 0;
    std::size_t cadenceDistantPages = 0;
    std::size_t scheduledPagesThisAdvance = 0;
    std::uint64_t simulationStepCount = 0;
    std::uint64_t tireContactCount = 0;
    double waterVolumeM3 = 0.0;
    double maximumWaterDepthM = 0.0;
    double cumulativeRainVolumeM3 = 0.0;
    double cumulativeInfiltrationVolumeM3 = 0.0;
    double cumulativeDrainageVolumeM3 = 0.0;
    double cumulativeEvaporationVolumeM3 = 0.0;
    double cumulativeRunoffVolumeM3 = 0.0;
    double cumulativeTireClearedVolumeM3 = 0.0;
    double cumulativeTireSprayVolumeM3 = 0.0;
    double maximumFlowSpeedMps = 0.0;
    double lastStepMilliseconds = 0.0;
};

struct DynamicSurfaceThermalSample
{
    bool valid = false;
    std::uint16_t surfaceSheetId = 0;
    double surfaceElevationM = 0.0;
    double surfaceTemperatureC = 20.0;
};

struct DynamicSurfaceThermalTireInput
{
    double deltaTimeSeconds = 0.001;
    double contactPatchAreaM2 = 0.024;
    double slipDissipationWatts = 0.0;
};

struct DynamicSurfaceThermalTireResult
{
    bool valid = false;
    double initialTemperatureC = 20.0;
    double finalTemperatureC = 20.0;
    double depositedHeatEnergyJ = 0.0;
};

struct DynamicSurfaceThermalStats
{
    bool available = false;
    std::size_t authorityPages = 0;
    std::size_t activePages = 0;
    std::size_t validTexels = 0;
    std::size_t cadence30HzPages = 0;
    std::size_t cadence20HzPages = 0;
    std::size_t cadence6HzPages = 0;
    std::size_t cadence2HzPages = 0;
    std::size_t cadenceDistantPages = 0;
    std::size_t scheduledPagesThisAdvance = 0;
    std::uint64_t simulationStepCount = 0;
    std::uint64_t tireContactCount = 0;
    double minimumTemperatureC = 0.0;
    double maximumTemperatureC = 0.0;
    double averageTemperatureC = 0.0;
    double cumulativeTireHeatEnergyJ = 0.0;
    double lastStepMilliseconds = 0.0;
};

struct SurfaceImpulse
{
    heritage::math::DVec3 globalPosition{ 0.0, 0.0, 0.0 };
    std::uint32_t surfaceSheetId = 0;
    float radiusM = 0.25f;
    float deltaTimeSeconds = 0.0f;

    // Positive adds free water; negative removes/displaces it.
    float waterVolumeDeltaM3 = 0.0f;
    float moistureDelta = 0.0f;
    float waterMomentumXKgMps = 0.0f;
    float waterMomentumZKgMps = 0.0f;

    float heatEnergyJ = 0.0f;
    float adheredRubberDelta = 0.0f;
    float looseRubberMassDeltaKg = 0.0f;
    float marbleMaturityTarget = 0.0f;

    float dirtDelta = 0.0f;
    float mudDelta = 0.0f;
    float looseDebrisDelta = 0.0f;
};

struct UpdateCadence
{
    // LIVETRACK10B: these Hydro values exist only for the bounded CPU fallback
    // and regression oracle. Production rain/puddles use the .hhyd prebake and
    // shader reconstruction instead of this cadence. CPU fallback detailed Hydro
    // is also clipped to the <=100m union; there is no distant update cadence.
    // Track thermal state keeps its independent 2Hz / 350m working set.
    static constexpr double hydroNearHz = 2.0;
    static constexpr double hydroDistantHz = 0.0;
    static constexpr double hydroTileHz = hydroNearHz; // compatibility alias
    static constexpr double trackTileHz = 2.0;
    static constexpr double trackActiveRadiusM = 350.0;
    static constexpr double puddleVisualRangeM = 100.0;
};

} // namespace heritage::physics::dynamicsurface

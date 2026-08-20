#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "DynamicSurfaceTypes.hpp"

namespace heritage::physics::dynamicsurface {

// DSURF01 immutable surface geometry cached from the authoritative static
// collision scene. These records are CPU-side bake data only; DSURF04C bins
// them into the 64x64 persistent tile cells without allocating a dense
// centimetre-scale CPU image per chunk/sheet.
struct StaticSurfacePatchTriangle
{
    heritage::math::DVec3 a{ 0.0, 0.0, 0.0 };
    heritage::math::DVec3 b{ 0.0, 0.0, 0.0 };
    heritage::math::DVec3 c{ 0.0, 0.0, 0.0 };
    heritage::math::Vec3 normal{ 0.0f, 1.0f, 0.0f };

    std::uint32_t sourceTriangleIndex = 0;
    std::uint32_t materialId = 0;
    std::uint16_t surfaceSheetId = 0;
    bool hydrologyAuthored = false;
    float authoredWetness = 0.0f;

    float infiltrationCapacityMmPerHour = 0.0f;
    float drainageCapacityMmPerHour = 0.0f;
    float flowRoughness = 0.02f;
    float depressionStorageM = 0.0f;

    bool hasAuthoredSurfaceTemperature = false;
    float authoredSurfaceTemperatureC = 20.0f;

    // Stable seed for future static microtopography generation. It is derived
    // solely from scene geometry identity + chunk identity and therefore does
    // not crawl when a camera moves or a floating origin rebases.
    std::uint64_t microtopographySeed = 0;
};

struct StaticSurfaceSheet
{
    std::uint16_t id = 0;
    std::uint32_t triangleCount = 0;
    heritage::math::DVec3 minimum{ 0.0, 0.0, 0.0 };
    heritage::math::DVec3 maximum{ 0.0, 0.0, 0.0 };
    heritage::math::Vec3 averageNormal{ 0.0f, 1.0f, 0.0f };

    // SurfaceMaterial currently fits comfortably inside 64 bits. The mask is
    // diagnostic/static metadata, not a connectivity rule: water may cross a
    // material seam when the actual geometry is continuous.
    std::uint64_t materialMask = 0;
    float averageInfiltrationCapacityMmPerHour = 0.0f;
    float maximumDrainageCapacityMmPerHour = 0.0f;
    std::uint64_t microtopographySeed = 0;
};

enum class StaticSurfaceBarrierKind : std::uint8_t
{
    // A real open/crease/step boundary in the wettable collision manifold.
    HardGeometry = 0,
    // An unmatched 100m chunk border. Normally matching geometry on the next
    // chunk turns this into a SurfaceSheetLink instead of a barrier.
    WorldOrSceneEdge = 1
};

struct StaticSurfaceBarrierSegment
{
    heritage::math::DVec3 a{ 0.0, 0.0, 0.0 };
    heritage::math::DVec3 b{ 0.0, 0.0, 0.0 };
    std::uint16_t surfaceSheetId = 0;
    StaticSurfaceBarrierKind kind = StaticSurfaceBarrierKind::HardGeometry;
};

struct StaticSurfaceDrainRegion
{
    std::uint64_t id = 0;
    std::uint16_t surfaceSheetId = 0;
    heritage::math::DVec3 center{ 0.0, 0.0, 0.0 };
    float radiusM = 0.0f;
    float capacityMmPerHour = 0.0f;
};

// Cross-chunk continuity is explicit and independent of camera position. A
// bridge deck and the road below can overlap in X/Z yet never share a link.
struct StaticSurfaceSheetLink
{
    ChunkAddress chunkA{};
    std::uint16_t sheetA = 0;
    ChunkAddress chunkB{};
    std::uint16_t sheetB = 0;
    heritage::math::DVec3 edgeA{ 0.0, 0.0, 0.0 };
    heritage::math::DVec3 edgeB{ 0.0, 0.0, 0.0 };
};

struct DynamicSurfaceStaticBakeReport
{
    bool valid = false;
    bool loadedFromCache = false;
    std::size_t sourceTriangleCount = 0;
    std::size_t acceptedTriangleCount = 0;
    std::size_t clippedPatchTriangleCount = 0;
    std::size_t chunkCount = 0;
    std::size_t surfaceSheetCount = 0;
    std::size_t hardBarrierSegmentCount = 0;
    std::size_t drainRegionCount = 0;
    std::size_t crossChunkSheetLinkCount = 0;
    std::uint64_t sourceFingerprint = 0;
    double elapsedMilliseconds = 0.0;
    std::filesystem::path cachePath;
    std::string message;
};

} // namespace heritage::physics::dynamicsurface

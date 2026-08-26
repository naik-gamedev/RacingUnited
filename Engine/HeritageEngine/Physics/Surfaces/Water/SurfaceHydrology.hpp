#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../CollisionSystem.hpp"

namespace heritage::jobs { class JobSystem; }

namespace heritage::physics::water {

// OPT02: SurfaceHydrology is now deliberately an immutable scene-hydrology
// topology service. Runtime water/moisture/flow authority lives in Dynamic
// Surface/GPU Hydro. The retired WATER14-WATER17 adaptive CPU solver, virtual
// pipes, cadence scheduler and presentation-cell mesh are no longer owned here.
struct SurfaceHydrologyDescription
{
    // Bounded support raster retained only for cache compatibility and the
    // legacy/headless precipitation-cover fallback. Production puddle/runoff
    // topology is reconstructed from the complete authored collision triangles.
    double cellSizeM = 0.50;
    double verticalLayerSizeM = 2.0;
    double minimumUpwardNormal = 0.15;
    std::size_t maximumCellCount = 262144;
};

struct SurfaceHydrologyBakeReport
{
    bool valid = false;
    bool loadedFromCache = false;
    std::size_t sourceTriangleCount = 0;
    std::size_t acceptedTriangleCount = 0;
    std::size_t cellCount = 0; // compatibility support-raster count, not a water solver
    std::size_t connectedCellCount = 0; // always zero after OPT02
    std::uint64_t sourceFingerprint = 0;
    double elapsedMilliseconds = 0.0;
    std::filesystem::path cachePath;
    std::string message;
};

// Tire-contact transport remains a SurfaceWorld API contract while runtime
// implementation is owned by Dynamic Surface/GPU Hydro. These types stay here
// temporarily to avoid coupling vehicle code to a renderer-owned GPU class.
struct SurfaceHydrologyTireInput
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
    SurfaceMaterial surfaceMaterial = SurfaceMaterial::Asphalt;
    heritage::math::Vec3 forward{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 right{ 1.0f, 0.0f, 0.0f };
};

struct SurfaceHydrologyTireResult
{
    bool valid = false;
    double initialWaterDepthM = 0.0;
    double finalWaterDepthM = 0.0;
    double removedVolumeM3 = 0.0;
    double redistributedVolumeM3 = 0.0;
    double sprayVolumeM3 = 0.0;
    double frictionEvaporatedVolumeM3 = 0.0;
};

struct SurfaceHydrologyStats
{
    bool available = false;
    bool loadedFromCache = false;
    bool debugVisualizationEnabled = false;
    std::size_t sourceTriangleCount = 0;
    std::size_t supportCellCount = 0;
    std::size_t cellCount = 0; // compatibility alias of supportCellCount
    std::size_t connectedCellCount = 0;
    std::size_t prebakedWorldTileCount = 0;
    std::uint64_t prebakedFarPayloadBytes = 0;
};

bool validSurfaceHydrologyDescription(
    const SurfaceHydrologyDescription& description);

class SurfaceHydrology
{
public:
    explicit SurfaceHydrology(const SurfaceHydrologyDescription& description = {});

    void setJobSystem(heritage::jobs::JobSystem* jobs) { m_jobSystem = jobs; }

    void clear();
    bool setDescription(const SurfaceHydrologyDescription& description);
    const SurfaceHydrologyDescription& description() const { return m_description; }

    bool loadOrBake(
        const std::vector<StaticSceneTriangle>& localTriangles,
        const heritage::math::DVec3& globalOrigin,
        const std::filesystem::path& cachePath,
        SurfaceHydrologyBakeReport& report);
    bool bake(
        const std::vector<StaticSceneTriangle>& localTriangles,
        const heritage::math::DVec3& globalOrigin,
        SurfaceHydrologyBakeReport& report);

    // .hhyd v15 immutable topology: near tiles reconstruct at the requested
    // resolution from authored triangles; far tiles fetch the precompressed
    // 32x32 runoff / standing-depth / downhill-flow payload directly.
    bool rasterPrebakedPuddleResponseTile(
        std::int32_t tileX,
        std::int32_t tileZ,
        std::uint32_t outputResolution,
        std::vector<std::uint8_t>& encodedCapacityAndFlow) const;
    bool prebakedFarPuddleResponseTile(
        std::int32_t tileX,
        std::int32_t tileZ,
        std::vector<std::uint8_t>& encodedCapacityAndFlow) const;

    // Exact same-column shelter query used by airborne precipitation. This
    // consults the complete triangle topology, not the old adaptive water grid.
    bool hasPrecipitationCoverAbove(
        const heritage::math::DVec3& globalPosition,
        double minimumClearanceM = 0.75,
        double maximumHeightM = 30.0) const;

    const SurfaceHydrologyStats& stats() const { return m_stats; }
    const SurfaceHydrologyBakeReport& lastBakeReport() const
    {
        return m_lastBakeReport;
    }

    // Compatibility control for the existing Lua setting. OPT02 removed the
    // CPU hydrology debug-cell renderer, so this flag is telemetry only.
    void setDebugVisualizationEnabled(bool enabled)
    {
        m_debugVisualizationEnabled = enabled;
        m_stats.debugVisualizationEnabled = enabled;
    }
    bool debugVisualizationEnabled() const { return m_debugVisualizationEnabled; }

private:
    struct CellKey
    {
        std::int64_t x = 0;
        std::int64_t z = 0;
        std::int64_t layer = 0;

        bool operator==(const CellKey& other) const
        {
            return x == other.x && z == other.z && layer == other.layer;
        }
        bool operator<(const CellKey& other) const
        {
            if (x != other.x) return x < other.x;
            if (z != other.z) return z < other.z;
            return layer < other.layer;
        }
    };

    struct CellKeyHash
    {
        std::size_t operator()(const CellKey& key) const;
    };

    // Compatibility support record kept byte-for-byte compatible with .hhyd
    // v15 CacheCell. Dynamic fields have been deleted; these values are static
    // authored/baked metadata only.
    struct Cell
    {
        CellKey key{};
        double elevationM = 0.0;
        heritage::math::Vec3 normal{ 0.0f, 1.0f, 0.0f };
        SurfaceMaterial material = SurfaceMaterial::Default;
        double infiltrationMmPerHour = 0.15;
        double drainageMmPerHour = 0.0;
        double roughness = 0.020;
        double depressionStorageM = 0.00020;
        bool precipitationExposed = true;
        double prebakedSpillElevationM = 0.0;
        float prebakedFlowX = 0.0f;
        float prebakedFlowZ = 0.0f;
    };

    struct PrebakedTriangle
    {
        heritage::math::DVec3 a{ 0.0, 0.0, 0.0 };
        heritage::math::DVec3 b{ 0.0, 0.0, 0.0 };
        heritage::math::DVec3 c{ 0.0, 0.0, 0.0 };
        heritage::math::Vec3 normal{ 0.0f, 1.0f, 0.0f };
        double depressionStorageM = 0.0001;
        double spillElevationA = 0.0;
        double spillElevationB = 0.0;
        double spillElevationC = 0.0;
        float flowAX = 0.0f;
        float flowAZ = 0.0f;
        float flowBX = 0.0f;
        float flowBZ = 0.0f;
        float flowCX = 0.0f;
        float flowCZ = 0.0f;
        float runoffAccumulationAM2 = 0.0f;
        float runoffAccumulationBM2 = 0.0f;
        float runoffAccumulationCM2 = 0.0f;
    };

    struct PrebakedTriangleTileSpan
    {
        std::uint64_t key = 0;
        std::uint64_t firstIndex = 0;
        std::uint32_t count = 0;
        std::uint32_t reserved = 0;
    };

    struct PrebakedFarTileIndex
    {
        std::uint64_t key = 0;
        std::uint64_t payloadOffset = 0;
        std::uint32_t payloadBytes = 0;
        std::uint8_t encoding = 0;
        std::uint8_t reserved0 = 0;
        std::uint8_t reserved1 = 0;
        std::uint8_t reserved2 = 0;
    };

    void rebuildLookupAndConnectivity();
    void rebuildPrebakedTriangleTopology(
        const std::vector<StaticSceneTriangle>& localTriangles,
        const heritage::math::DVec3& globalOrigin);
    void rebuildPrebakedTriangleTileLookup();
    const PrebakedTriangleTileSpan* prebakedTriangleTileSpan(
        std::int32_t tileX, std::int32_t tileZ) const;
    void rebuildPrebakedFarTileCache();
    bool rasterPrebakedPuddleResponseTileUncached(
        std::int32_t tileX,
        std::int32_t tileZ,
        std::uint32_t outputResolution,
        std::vector<std::uint8_t>& encodedCapacityAndFlow) const;

    std::uint64_t fingerprint(
        const std::vector<StaticSceneTriangle>& localTriangles,
        const heritage::math::DVec3& globalOrigin) const;
    bool loadCache(
        const std::filesystem::path& path,
        std::uint64_t expectedFingerprint,
        SurfaceHydrologyBakeReport& report);
    bool writeCache(
        const std::filesystem::path& path,
        std::uint64_t fingerprint) const;

    SurfaceHydrologyDescription m_description{};
    std::vector<Cell> m_cells;
    std::unordered_map<CellKey, std::int32_t, CellKeyHash> m_lookup;
    std::unordered_map<CellKey, std::int32_t, CellKeyHash> m_topSupportLookup;

    std::vector<PrebakedTriangle> m_prebakedTriangles;
    std::vector<PrebakedTriangleTileSpan> m_prebakedTriangleTileSpans;
    std::vector<std::int32_t> m_prebakedTriangleTileIndices;
    std::vector<PrebakedFarTileIndex> m_prebakedFarTiles;
    std::vector<std::uint8_t> m_prebakedFarPayload;

    heritage::jobs::JobSystem* m_jobSystem = nullptr;
    SurfaceHydrologyStats m_stats{};
    SurfaceHydrologyBakeReport m_lastBakeReport{};
    bool m_debugVisualizationEnabled = false;
};

} // namespace heritage::physics::water

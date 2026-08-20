#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../CollisionSystem.hpp"
#include "../SurfaceWeather.hpp"

namespace heritage::jobs { class JobSystem; }

namespace heritage::physics::water {

struct SurfaceHydrologyDescription
{
    // Half-metre cells retain meaningful road camber, gutters and racing-line
    // paths without attempting fluid simulation on every LiDAR triangle.
    // WATER14: cellSizeM is now the immutable terrain-support raster used by
    // the bake/cache only. The authoritative water solver runs on a separate
    // adaptive control-volume mesh built from that support field.
    double cellSizeM = 0.50;
    double adaptiveMinimumCellSizeM = 0.10;
    double adaptiveMaximumCellSizeM = 20.0;
    double adaptiveSurfaceErrorM = 0.020;
    // WATER14F: this is now the allowed normal deviation from the best-fit
    // support plane for a coarse candidate. The plane-fit residual is the
    // primary coarsening test, so a uniformly sloped road can stay coarse even
    // when source-triangle normals contain modest tessellation noise.
    double adaptiveNormalErrorDegrees = 10.0;
    // WATER14A/J: the expensive 0.10 m authoritative tier is reserved for
    // genuinely aggressive angular terrain geometry. Curb/sidewalk height
    // discontinuities are traced as directional presentation edges instead of
    // exploding whole 0.50 m supports. WATER14J then keeps the surrounding
    // 0.50 m+ solver hierarchy 2:1 balanced as it grows toward large cells.
    double adaptiveMinimumCellSlopeDegrees = 55.0;
    double adaptiveMinimumCellNormalBreakDegrees = 30.0;
    double verticalLayerSizeM = 2.0;
    // Maximum / near-field hydrology cadence. JOB03 adds distance-adaptive
    // scheduling around one or more world-space simulation-interest sources.
    double updateRateHz = 30.0;
    // PERF11 distance policy: keep the expensive solver concentrated around
    // players while preserving slow world-state persistence beyond the visible
    // water radius. Distances are evaluated to the nearest interest source.
    double nearCadenceRadiusM = 25.0;
    double mediumCadenceRadiusM = 50.0;
    double farCadenceRadiusM = 100.0;
    double distantCadenceRadiusM = 200.0;
    double nearCadenceHz = 30.0;
    double mediumCadenceHz = 20.0;
    double farCadenceHz = 6.0;
    double distantCadenceHz = 2.0;
    double backgroundCadenceHz = 0.5;
    double minimumUpwardNormal = 0.15;
    double maximumNeighbourStepM = 1.25;
    double maximumWaterDepthM = 0.30;
    // WATER09 virtual-pipe conductance multiplier. This scales hydraulic-head
    // acceleration of persistent N/E/S/W pipe flux; it is no longer a
    // per-step downhill release fraction.
    double flowCoefficient = 0.58;
    std::size_t maximumCellCount = 1500000;
};

struct SurfaceHydrologyBakeReport
{
    bool valid = false;
    bool loadedFromCache = false;
    std::size_t sourceTriangleCount = 0;
    std::size_t acceptedTriangleCount = 0;
    std::size_t cellCount = 0;
    std::size_t connectedCellCount = 0;
    std::uint64_t sourceFingerprint = 0;
    double elapsedMilliseconds = 0.0;
    std::filesystem::path cachePath;
    std::string message;
};

struct SurfaceHydrologySample
{
    bool valid = false;
    SurfaceMaterial material = SurfaceMaterial::Default;
    double surfaceElevationM = 0.0;
    double waterDepthM = 0.0;
    double wetness = 0.0;
    double flowVelocityXMps = 0.0;
    double flowVelocityZMps = 0.0;
};

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
    // WATER14: supportCellCount is the immutable 0.5 m terrain raster. cellCount
    // is the *authoritative adaptive simulation* count used for rain/flow/tire
    // water. This distinction is exposed so F8 can prove the fixed chessboard
    // is no longer being simulated.
    std::size_t supportCellCount = 0;
    std::size_t cellCount = 0;
    std::size_t connectedCellCount = 0;
    double adaptiveMinimumCellSizeM = 0.0;
    double adaptiveMaximumCellSizeM = 0.0;
    std::size_t adaptiveSubDecimetreCellCount = 0;
    std::size_t adaptiveLargeCellCount = 0;
    std::size_t wetCellCount = 0;
    std::uint64_t simulationStepCount = 0;
    std::uint64_t tireContactCount = 0;
    double updateRateHz = 0.0;
    double waterVolumeM3 = 0.0;
    double maximumWaterDepthM = 0.0;
    double cumulativeRainVolumeM3 = 0.0;
    double cumulativeInfiltrationVolumeM3 = 0.0;
    double cumulativeDrainageVolumeM3 = 0.0;
    double cumulativeEvaporationVolumeM3 = 0.0;
    double cumulativeRunoffVolumeM3 = 0.0;
    double cumulativeTireClearedVolumeM3 = 0.0;
    double cumulativeTireSprayVolumeM3 = 0.0;
    double lastStepMilliseconds = 0.0;
    // WATER09 diagnostics for the persistent virtual-pipe transport field.
    std::size_t activeVirtualPipeCount = 0;
    double maximumVirtualPipeFluxM3ps = 0.0;

    // JOB03 diagnostics. Classification is the UNION of per-source influence
    // regions: each hydrology chunk uses its minimum distance to any supplied
    // source. Sources are never averaged into a midpoint.
    std::size_t interestSourceCount = 0;
    std::size_t cadence30HzCellCount = 0;
    std::size_t cadence20HzCellCount = 0;
    std::size_t cadence6HzCellCount = 0;
    std::size_t cadence2HzCellCount = 0;
    std::size_t cadenceBackgroundCellCount = 0;
    std::size_t lastScheduledCellCount = 0;

    // WATER16: presentation basins are static drainage catchments derived once
    // from the immutable support raster. Dynamic puddle water levels are
    // updated from authoritative adaptive-cell excess volume, so the renderer
    // no longer exposes solver-cell footprints.
    std::size_t presentationBasinCount = 0;
    std::size_t activePresentationBasinCount = 0;
};

struct SurfaceHydrologyVisualCell
{
    heritage::math::DVec3 globalPosition{ 0.0, 0.0, 0.0 };
    heritage::math::Vec3 normal{ 0.0f, 1.0f, 0.0f };
    SurfaceMaterial material = SurfaceMaterial::Default;
    double cellSizeM = 0.5;
    double surfaceElevationM = 0.0;
    // PERF14 presentation support heights at the exact world-X/Z patch
    // corners, ordered (-X,-Z), (+X,-Z), (-X,+Z), (+X,+Z). These are
    // presentation-only collision-surface samples for the authoritative
    // adaptive control volume. Large flat regions still use two triangles;
    // local feature-edge hints may refine only the boundary that needs it.
    std::array<double, 4> cornerSurfaceElevationM{};
    // PERF18: sub-cell shoreline reconstruction samples. Water depth at the
    // exact patch corners, ordered identically to cornerSurfaceElevationM.
    // Fine 0.5 m cells derive these from compatible neighbouring hydrology
    // centres so the visible shoreline can move continuously through a cell
    // instead of switching an entire square on/off at the explicit-water
    // threshold. Adaptive interior patches may use their aggregate depth.
    std::array<double, 4> cornerWaterDepthM{};
    // WATER15F: presentation-only free-surface elevation reconstructed at each
    // adaptive-cell corner. Thin films follow the collider; standing water
    // progressively uses hydraulic head and is averaged only with compatible
    // neighbouring surfaces, so adaptive control-volume boundaries do not
    // become visible square puddle boundaries. Authoritative volume remains
    // waterDepthM/cornerWaterDepthM in the solver.
    std::array<double, 4> cornerWaterSurfaceElevationM{};
    double waterDepthM = 0.0;
    double flowVelocityXMps = 0.0;
    double flowVelocityZMps = 0.0;
    double cameraDistanceSquaredM2 = 0.0;
    // WATER14: normalized adaptive simulation-cell scale for diagnostics.
    // Zero is the 0.10 m minimum; one approaches the 20 m maximum.
    float presentationLodClass = 0.0f;
    // WATER14I/J: directional sharp-boundary hint for presentation-only edge
    // refinement. Curb/sidewalk steps no longer explode the whole adjacent
    // 0.50 m support square into 0.10 m authoritative cells. Only the exact
    // boundary edge is split at ~0.10 m; WATER14J grades and 2:1-balances the
    // surrounding solver cells outward.
    static constexpr std::uint8_t FineBoundaryLeft = 1u << 0;
    static constexpr std::uint8_t FineBoundaryTop = 1u << 1;
    static constexpr std::uint8_t FineBoundaryRight = 1u << 2;
    static constexpr std::uint8_t FineBoundaryBottom = 1u << 3;
    std::uint8_t fineBoundaryMask = 0u;
    // WATER17: immutable drainage-basin identity is presentation metadata only.
    // The GPU uses it solely as a stable seed for organic implicit shoreline
    // reconstruction; it never owns or changes authoritative water mass.
    std::int32_t presentationBasinId = -1;
    // PERF16: preserve the authoritative vertical topology layer through
    // presentation gathering. Connected water vertices may share X/Z only
    // when they belong to the same layered surface (road vs bridge/tunnel).
    std::int64_t presentationLayer = 0;
};

bool validSurfaceHydrologyDescription(
    const SurfaceHydrologyDescription& description);

class SurfaceHydrology
{
public:
    explicit SurfaceHydrology(const SurfaceHydrologyDescription& description = {});

    void setJobSystem(heritage::jobs::JobSystem* jobs) { m_jobSystem = jobs; }

    // World-space simulation-interest points. A single-player runtime normally
    // supplies the player vehicle. Split-screen/multiplayer may supply multiple
    // local/relevant vehicles. Hydrology always selects the NEAREST source for
    // each chunk; there is deliberately no averaged/midpoint source.
    void clearInterestSources();
    void setInterestSource(const heritage::math::DVec3& source);
    void setInterestSources(const std::vector<heritage::math::DVec3>& sources);
    const std::vector<heritage::math::DVec3>& interestSources() const
    {
        return m_interestSources;
    }

    void clear();
    void resetWater();
    bool setUniformWaterDepthForLab(double waterDepthM);
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

    void advance(
        const SurfaceWeatherDescription& weather,
        const SurfaceWeatherOutput& weatherOutput,
        double deltaTimeSeconds);

    SurfaceHydrologySample sample(
        const heritage::math::DVec3& globalPosition) const;

    // WEATHER06H: exact same-column precipitation cover query for presentation.
    // Unlike a radius gather, this cannot mistake nearby uphill terrain for a
    // roof. It searches the hydrology's layered X/Z column for an upward-facing
    // surface genuinely above the supplied world-space point.
    bool hasPrecipitationCoverAbove(
        const heritage::math::DVec3& globalPosition,
        double minimumClearanceM = 0.75,
        double maximumHeightM = 30.0) const;
    SurfaceHydrologyTireResult applyTireContact(
        const heritage::math::DVec3& globalPosition,
        const SurfaceHydrologyTireInput& input);

    const SurfaceHydrologyStats& stats() const { return m_stats; }
    const SurfaceHydrologyBakeReport& lastBakeReport() const
    {
        return m_lastBakeReport;
    }

    void setDebugVisualizationEnabled(bool enabled)
    {
        m_debugVisualizationEnabled = enabled;
        m_stats.debugVisualizationEnabled = enabled;
    }
    bool debugVisualizationEnabled() const { return m_debugVisualizationEnabled; }
    void collectVisualCells(
        const heritage::math::DVec3& globalCenter,
        double radiusM,
        std::size_t maximumCells,
        std::vector<SurfaceHydrologyVisualCell>& cells,
        double fullResolutionRadiusM = 100.0,
        bool includeDryCells = false) const;

    // WATER14 direct band gather. There is no presentation mode or patch-size
    // hierarchy anymore: callers receive authoritative adaptive simulation
    // control volumes (0.10..20 m) directly.
    void collectVisualCellsBand(
        const heritage::math::DVec3& globalCenter,
        double minimumRadiusM,
        double maximumRadiusM,
        std::size_t maximumCells,
        std::vector<SurfaceHydrologyVisualCell>& cells,
        bool includeDryCells = false,
        double minimumExplicitWaterDepthM = 0.00001) const;

    // WATER17 organic presentation gather. Authoritative adaptive cells remain
    // the water-mass solver, but graphics receives the immutable 0.50 m support
    // raster carrying drainage-basin free-surface levels. Adaptive solver leaves
    // therefore cannot define visible shapes. The GPU turns these support samples
    // into overlapping implicit radial bases while exact collider height/layer
    // tests preserve curbs and stacked surfaces.
    void collectPresentationBasinCellsBand(
        const heritage::math::DVec3& globalCenter,
        double minimumRadiusM,
        double maximumRadiusM,
        std::size_t maximumCells,
        std::vector<SurfaceHydrologyVisualCell>& cells,
        double shorelineInfluenceM = 0.035) const;

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
        // WATER14 support cells stop here. Dynamic depth, velocity, pipe state
        // and neighbour ownership live only in AdaptiveCell/AdaptivePipe.
        bool precipitationExposed = true;
        // WATER16 immutable drainage catchment used only for puddle
        // presentation. -1 means the support has not been assigned.
        std::int32_t presentationBasinId = -1;
    };

    // WATER14 authoritative adaptive shallow-water control volume. The fixed
    // Cell above is terrain support/cache data only. Adaptive cells may span
    // from 0.10 m to 20 m and preserve water as volume, so changing cell area
    // never changes mass.
    struct AdaptiveCell
    {
        std::int64_t minimumXUnits = 0; // 0.05 m lattice units
        std::int64_t minimumZUnits = 0;
        std::int64_t layer = 0;
        std::uint16_t spanUnits = 10;   // 10 == 0.50 m
        double centerX = 0.0;
        double centerZ = 0.0;
        double cellSizeM = 0.50;
        double areaM2 = 0.25;
        double elevationM = 0.0;
        heritage::math::Vec3 normal{ 0.0f, 1.0f, 0.0f };
        SurfaceMaterial material = SurfaceMaterial::Default;
        double infiltrationMmPerHour = 0.15;
        double drainageMmPerHour = 0.0;
        double roughness = 0.020;
        double depressionStorageM = 0.00020;
        bool precipitationExposed = true;
        // WATER14I/J presentation hint copied from the immutable 0.50 m
        // support raster when a real N/E/S/W height discontinuity lies on this
        // cell. It does not alter water volume or solver area.
        std::uint8_t fineBoundaryMask = 0u;
        // WATER16 presentation-only basin assignment sampled from the support
        // raster at this adaptive cell center. It never changes solver volume.
        std::int32_t presentationBasinId = -1;
        double waterVolumeM3 = 0.0;
        double flowVelocityXMps = 0.0;
        double flowVelocityZMps = 0.0;
        float boundaryOutflowM3ps = 0.0f;
        double openBoundaryLengthM = 0.0;
        std::vector<std::int32_t> pipeIndices;
    };

    struct AdaptivePipe
    {
        std::int32_t cellA = -1;
        std::int32_t cellB = -1;
        float edgeLengthM = 0.0f;
        float centerDistanceM = 0.0f;
        float directionX = 0.0f;
        float directionZ = 0.0f;
        double sillElevationM = 0.0;
        // Signed volumetric flux. Positive is A -> B, negative B -> A.
        float fluxM3ps = 0.0f;
        float candidateFluxM3ps = 0.0f;
        float deltaTimeSeconds = 0.0f;
        double transferredVolumeM3 = 0.0;
    };

    struct PresentationBasin
    {
        std::int32_t id = -1;
        std::int64_t layer = 0;
        std::vector<std::int32_t> supportIndices;
        std::vector<double> sortedSupportElevationsM;
        std::vector<double> prefixElevationSumM;
        double excessWaterVolumeM3 = 0.0;
        double waterSurfaceElevationM = -32768.0;
    };

    struct PresentationBasinConnection
    {
        std::int32_t basinA = -1;
        std::int32_t basinB = -1;
        // Lowest support saddle connecting the two drainage catchments. Curbs
        // never enter this graph because compatibleSupport rejects them.
        double spillElevationM = 0.0;
    };

    struct WeatherReduction
    {
        double rainVolumeM3 = 0.0;
        double infiltrationVolumeM3 = 0.0;
        double drainageVolumeM3 = 0.0;
        double evaporationVolumeM3 = 0.0;
    };

    struct StatsReduction
    {
        std::size_t wetCells = 0;
        double volumeM3 = 0.0;
        double maximumDepthM = 0.0;
        std::size_t activeVirtualPipes = 0u;
        double maximumVirtualPipeFluxM3ps = 0.0;
    };

    enum class CadenceBand : std::uint8_t
    {
        Near30Hz = 0,
        Medium20Hz,
        Far6Hz,
        Distant2Hz,
        Background
    };

    struct SimulationChunk
    {
        CellKey key{};
        std::vector<std::int32_t> cellIndices;
        // Schedule phase and physical elapsed time are separate. This allows
        // deterministic temporal staggering without inventing or losing rain/
        // drainage/flow time when a chunk finally runs.
        double cadenceAccumulatorSeconds = 0.0;
        double elapsedSinceUpdateSeconds = 0.0;
        bool cadenceInitialized = false;
        CadenceBand cadenceBand = CadenceBand::Near30Hz;
    };

    struct DueCell
    {
        std::int32_t index = -1;
        float deltaTimeSeconds = 0.0f;
    };

    std::int32_t findCellIndex(const heritage::math::DVec3& globalPosition) const;
    std::int32_t findAdaptiveCellIndex(const heritage::math::DVec3& globalPosition) const;
    void rebuildLookupAndConnectivity();
    void rebuildAdaptiveSimulationTopology();
    void rebuildAdaptivePipes();
    void rebuildPresentationBasins();
    void refreshPresentationBasinLevels(double smoothFilmDepthM);
    double adaptiveSurfaceElevationAt(const AdaptiveCell& cell, double x, double z) const;
    double adaptiveWaterDepth(const AdaptiveCell& cell) const;
    CadenceBand cadenceBandForChunk(const SimulationChunk& chunk) const;
    double cadenceHz(CadenceBand band) const;
    void rebuildDueCells(double baseStepSeconds);
    void simulateStep(
        const SurfaceWeatherDescription& weather,
        const SurfaceWeatherOutput& weatherOutput,
        double deltaTimeSeconds,
        bool refreshStatistics);
    void refreshStats();
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
    // Immutable support raster loaded/baked at m_description.cellSizeM. It is
    // never stepped by the water solver in WATER14.
    std::vector<Cell> m_cells;
    std::unordered_map<CellKey, std::int32_t, CellKeyHash> m_lookup;

    // Authoritative adaptive water topology and a world-space 20 m bucket index
    // used for point sampling/tire contact/presentation gathers.
    std::vector<AdaptiveCell> m_adaptiveCells;
    std::vector<AdaptivePipe> m_adaptivePipes;
    std::unordered_map<CellKey, std::vector<std::int32_t>, CellKeyHash>
        m_adaptiveSpatialBuckets;

    // WATER16: static drainage catchments + low-frequency dynamic free-surface
    // state. This is presentation metadata only; m_adaptiveCells remains the
    // sole conserved water authority.
    std::vector<PresentationBasin> m_presentationBasins;
    std::vector<PresentationBasinConnection> m_presentationBasinConnections;
    double m_presentationBasinRefreshAccumulatorSeconds = 0.0;
    double m_lastPresentationFilmDepthM = 0.0;

    // Deterministic 20 m simulation chunks schedule the adaptive cells using
    // the established 30/20/6/2/0.5 Hz nearest-interest-source policy.
    std::vector<SimulationChunk> m_simulationChunks;
    std::vector<heritage::math::DVec3> m_interestSources;
    std::vector<DueCell> m_dueCells;
    // WATER14 scratch state. Pipes are evaluated independently, then each cell
    // conservatively gathers its incident transfers. No fixed-grid color
    // buckets, neighbour arrays or pending-depth chessboard remain.
    std::vector<float> m_dueDeltaTimeByCell;
    std::vector<double> m_outflowScaleByCell;
    std::vector<double> m_boundaryCandidateFluxByCell;
    heritage::jobs::JobSystem* m_jobSystem = nullptr;
    // Reused per-step reductions; no heap allocation occurs in the steady-state
    // 30 Hz water solver once a scene has reached its maximum range count.
    std::vector<WeatherReduction> m_weatherReductionScratch;
    std::vector<double> m_flowRunoffReductionScratch;
    std::vector<double> m_overflowRunoffReductionScratch;
    std::vector<StatsReduction> m_statsReductionScratch;
    SurfaceHydrologyStats m_stats{};
    SurfaceHydrologyBakeReport m_lastBakeReport{};
    double m_stepAccumulatorSeconds = 0.0;
    double m_statsRefreshAccumulatorSeconds = 0.0;
    bool m_debugVisualizationEnabled = false;
};

} // namespace heritage::physics::water

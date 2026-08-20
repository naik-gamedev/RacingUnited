#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glad/glad.h>

namespace heritage::physics::dynamicsurface { class DynamicSurfaceSystem; }

namespace heritage::graphics::dynamicsurface {

struct DynamicSurfaceGpuLodPrototypeStats
{
    bool ready = false;
    bool waterReady = false;
    bool snowReady = false;
    bool mudReady = false;
    bool waterPresentationReady = false;
    bool authoritative = false;
    double cpuDispatchMs = 0.0;
    double gpuComputeMs = 0.0;
    double committedMiB = 0.0;
    double waterPresentationMiB = 0.0;
    std::uint64_t dispatchesThisFrame = 0;
    std::uint64_t waterPresentationDispatchesThisFrame = 0;
    std::uint64_t cellsThisFrame = 0;
    std::uint64_t waterPublishedCycles = 0;
    std::array<std::uint64_t, 4> waterLodDispatches{};
    std::array<std::uint64_t, 4> waterLodPublishedCycles{};
    std::array<std::uint32_t, 4> geometryValidTiles{};
    std::array<std::uint32_t, 4> activeTiles{};
    std::uint32_t residentTiles = 0;
    std::uint32_t prewarmTiles = 0;
    std::uint32_t worldTiles = 0;
    std::uint64_t worldTileDispatches = 0;
    double worldTileStateMiB = 0.0;
    std::uint32_t dueTiles = 0;
    std::uint32_t dispatchBacklogTiles = 0;
    std::int32_t centerTileX = 0;
    std::int32_t centerTileZ = 0;
    std::uint64_t cameraTileRebases = 0;
    std::uint64_t tireEventDispatches = 0;
    std::uint64_t tireEventCells = 0;
    bool exactGeometrySupportReady = false;
    std::uint64_t geometryTriangles = 0;
    std::uint64_t geometryBinReferences = 0;
    double geometryUploadMiB = 0.0;
    double cameraSpeedMps = 0.0;
    double predictivePrewarmM = 0.0;
    float precipitationRateMmPerHour = 0.0f;
    float drainageRateMmPerHour = 0.0f;
    float evaporationRateMmPerHour = 0.0f;
    float backgroundSeedDepthM = 0.0f;
    bool waterProbeValid = false;
    std::uint32_t waterProbeWetTexels = 0u;
    std::uint32_t waterProbeTexels = 0u;
    float waterProbeMeanDepthM = 0.0f;
    float waterProbeMaximumDepthM = 0.0f;
};

struct DynamicSurfaceGpuTireContactEvent
{
    double globalX = 0.0;
    double globalZ = 0.0;
    float patchLengthM = 0.0f;
    float patchWidthM = 0.0f;
    float forwardX = 0.0f;
    float forwardZ = 1.0f;
    float rightX = 1.0f;
    float rightZ = 0.0f;
    float normalLoadN = 0.0f;
    float speedMps = 0.0f;
    float accumulatedDtSeconds = 0.0f;
    bool mudDeformable = false;
};

// LIVETRACK04: GPU-only high-resolution Hydro working set.
// Every resident world tile is exactly 10m x 10m and 256x256 texels
// (~3.90625cm/texel). The expensive per-texel water simulation lives on the
// GPU; CPU code only schedules tiles and submits aggregated tire events.
//
// The sampled water atlas is GL_RGBA8 because OpenGL 4.6 image load/store does
// not expose rgba4 as a compute-writable image format. Each channel is still
// quantized to exactly 16 logical values (multiples of 17/255), so the state
// semantics remain RGBA4:
//   R water level | G dry-line strength | B/A downhill flow X/Z.
// GL_LINEAR is used directly for rendering. There is no second puddle texture.
// Detailed Hydro is presented in the bounded 100m simulation disk. Every tile
// in that disk receives authority. Exact collision geometry supplies downhill
// guidance; uncovered X/Z cells remain valid level state and are invisible
// unless an authored wetness-receiver mesh samples them. Spare atlas slots retain recently visited water state at
// a one-per-minute cadence so puddles do not vanish at the camera boundary.
class DynamicSurfaceGpuLodPrototype
{
public:
    DynamicSurfaceGpuLodPrototype() = default;
    DynamicSurfaceGpuLodPrototype(const DynamicSurfaceGpuLodPrototype&) = delete;
    DynamicSurfaceGpuLodPrototype& operator=(const DynamicSurfaceGpuLodPrototype&) = delete;

    bool initialize(std::string& errorMessage);
    void shutdown();

    void update(
        double elapsedSeconds,
        double cameraGlobalX,
        double cameraGlobalY,
        double cameraGlobalZ,
        float precipitationRateMmPerHour,
        float weatherDrainageRateMmPerHour,
        float evaporationRateMmPerHour,
        float ambientTemperatureC,
        const heritage::physics::dynamicsurface::DynamicSurfaceSystem* dynamicSurface,
        const std::vector<DynamicSurfaceGpuTireContactEvent>& tireEvents);

    void applyTireContactEvents(
        const std::vector<DynamicSurfaceGpuTireContactEvent>& events);

    const DynamicSurfaceGpuLodPrototypeStats& stats() const { return m_stats; }

    GLuint waterTexture(std::size_t = 0) const { return m_water.atlas; }
    GLuint waterPresentationTexture() const { return m_water.atlas; }
    bool waterPresentationReady() const { return m_water.allocated; }
    GLuint snowTexture(std::size_t = 0) const { return m_snow.atlas; }
    GLuint mudTexture(std::size_t = 0) const { return m_mud.atlas; }
    GLuint tileIndirectionTexture() const { return m_tileIndirectionTexture; }
    bool authoritativeReady() const
    {
        return m_stats.ready && m_stats.waterReady && m_geometryAtlasReady;
    }
    std::int32_t centerTileX() const { return m_centerTileX; }
    std::int32_t centerTileZ() const { return m_centerTileZ; }

    static constexpr std::uint32_t tileResolution() { return kTileResolution; }
    static constexpr float tileWorldSizeM() { return kTileWorldSizeM; }
    static constexpr std::uint32_t atlasColumns() { return kAtlasColumns; }
    static constexpr std::uint32_t tileMapResolution() { return kTileMapResolution; }
    static constexpr std::int32_t tileMapHalfSpan() { return kTileMapHalfSpan; }
    static constexpr float simulationRadiusM() { return kSimulationRadiusM; }
    static constexpr std::size_t maximumTireContactEventsPerFrame()
    {
        return kMaximumTireContactEventsPerFrame;
    }

private:
    static constexpr float kTileWorldSizeM = 10.0f;
    static constexpr std::uint32_t kTileResolution = 256u;
    static constexpr float kCellSizeM = kTileWorldSizeM / float(kTileResolution);
    // A 100m nearest-tile-edge disk contains at most 357 10m tiles. Keep a
    // small safety margin without reserving the old 1024-slot/256MiB atlas.
    static constexpr std::uint32_t kAtlasColumns = 20u;
    static constexpr std::uint32_t kAtlasRows = 20u;
    static constexpr std::uint32_t kMaximumTileSlots = kAtlasColumns * kAtlasRows;
    static constexpr std::uint32_t kMaximumResidentTiles = 357u;
    static_assert(kMaximumTileSlots >= kMaximumResidentTiles);
    // Scratch needs one layer per due resident tile, not 512 unrestricted layers.
    // Every due cadence band is still submitted as one Z-batched compute job.
    static constexpr std::uint32_t kMaximumBatchTiles = 384u;
    static_assert(kMaximumBatchTiles >= kMaximumResidentTiles);
    static constexpr std::uint32_t kAtlasWidth = kAtlasColumns * kTileResolution;
    static constexpr std::uint32_t kAtlasHeight = kAtlasRows * kTileResolution;
    static constexpr std::uint32_t kTileMapResolution = 64u;
    static constexpr std::int32_t kTileMapHalfSpan = 32;
    // The complete <=100m disk is one simulation cohort. Visibility/frustum
    // culling is presentation-only: it must never decide which nearby tiles
    // receive rain, flow, drainage or tire state. The bounded 384-layer scratch
    // already has room for the worst-case 357-tile disk, so initialize and
    // publish that disk atomically instead of revealing it over several frames.
    static constexpr std::uint32_t kMaximumTileUpdatesPerFrame = kMaximumResidentTiles;
    static constexpr std::uint32_t kMaximumNewTileInitializationsPerFrame =
        kMaximumResidentTiles;
    static_assert(kMaximumBatchTiles >= kMaximumTileUpdatesPerFrame);
    static constexpr float kSimulationRadiusM = 100.0f;
    static constexpr std::size_t kMaximumTireContactEventsPerFrame = 256u;
    // 12Hz master scheduler expresses 6/4/2/1/0.5Hz exactly as integer divisors.
    static constexpr float kWorldDispatchHz = 12.0f;
    static constexpr float kPrewarmSeconds = 0.0f;
    static constexpr float kMaximumPrewarmM = 0.0f;
    static constexpr float kPrewarmHalfWidthM = 0.0f;

    // Geometry lookup is initialization-only acceleration metadata. New
    // 10m/256x256 Hydro texels use it once to bake downhill guidance into B/A;
    // normal 6Hz water updates do not evaluate collision triangles per texel.
    static constexpr std::uint32_t kGeometryBinResolution = 64u;
    static constexpr std::uint32_t kGeometryGridResolution = 5u;
    static constexpr std::int32_t kGeometryGridHalfSpan = 2;

    struct alignas(16) GpuSurfaceTriangle
    {
        std::array<float, 4> a{}; // xyz + surfaceSheetId
        std::array<float, 4> b{};
        std::array<float, 4> c{};
        std::array<float, 4> hydro{}; // infiltration, drainage, roughness, depression
    };

    struct GpuBinHeader
    {
        std::uint32_t offset = 0;
        std::uint32_t count = 0;
    };

    struct GpuTileGeometryMeta
    {
        std::uint32_t binHeaderBase = 0;
        std::uint32_t valid = 0;
        std::uint32_t reserved0 = 0;
        std::uint32_t reserved1 = 0;
    };

    struct alignas(16) GpuHydroBatchTile
    {
        std::array<std::int32_t, 2> worldTile{};
        std::array<std::int32_t, 2> atlasOrigin{};
        std::uint32_t worldStateIndex = 0u;
        float cycleDtSeconds = 0.0f;
        std::uint32_t reserved0 = 0u;
        std::uint32_t reserved1 = 0u;
    };

    enum class StateKind : std::uint8_t
    {
        Water,
        Snow,
        Mud
    };

    struct StateRuntime
    {
        GLuint atlas = 0;
        GLuint scratch = 0;
        GLenum internalFormat = GL_R32UI;
        GLenum clearFormat = GL_RED_INTEGER;
        GLenum clearType = GL_UNSIGNED_INT;
        std::size_t bytesPerTexel = 0;
        bool allocated = false;
    };

    struct TileRuntime
    {
        std::int32_t x = 0;
        std::int32_t z = 0;
        std::uint16_t slot = 0;
        std::uint8_t cadenceBand = 3;
        bool prewarm = false;
        bool initialized = false;
        std::uint32_t worldStateIndex = 0u;
        double nextDueSeconds = 0.0;
        double lastUpdateSeconds = 0.0;
    };

    struct ProgramUniforms
    {
        GLint worldTile = -1;
        GLint atlasOrigin = -1;
        GLint tileMapOrigin = -1;
        GLint geometryCenterChunk = -1;
        GLint cellSizeM = -1;
        GLint cycleDtSeconds = -1;
        GLint precipitationRateMmPerHour = -1;
        GLint weatherDrainageRateMmPerHour = -1;
        GLint evaporationRateMmPerHour = -1;
        GLint ambientTemperatureC = -1;
        GLint tickIndex = -1;
        GLint worldStateIndex = -1;
        GLint stateAtlas = -1;
        GLint tileMap = -1;
        GLint batchTileCount = -1;
    };

    struct WaterPresentationUniforms
    {
        GLint stateAtlas = -1;
        GLint tileMap = -1;
        GLint worldTile = -1;
        GLint atlasOrigin = -1;
        GLint tileMapOrigin = -1;
        GLint cellSizeM = -1;
    };

    struct TireEventUniforms
    {
        GLint atlasOrigin = -1;
        GLint minTexel = -1;
        GLint extentTexels = -1;
        GLint cellSizeM = -1;
        GLint eventLocalXZ = -1;
        GLint forwardXZ = -1;
        GLint rightXZ = -1;
        GLint patchHalfLengthM = -1;
        GLint patchHalfWidthM = -1;
        GLint normalLoadN = -1;
        GLint speedMps = -1;
        GLint accumulatedDtSeconds = -1;
        GLint snowReady = -1;
        GLint mudReady = -1;
        GLint mudDeformable = -1;
    };

    static std::uint64_t tileKey(std::int32_t x, std::int32_t z);
    bool worldStateIndexForTile(
        std::int32_t tileX, std::int32_t tileZ, std::uint32_t& index) const;
    static float cadenceHzForDistance(float distanceM, bool prewarm);
    static std::uint8_t cadenceBandForDistance(float distanceM, bool prewarm);
    static float cadencePeriodForBand(std::uint8_t band);
    static std::array<std::uint32_t, 2> atlasSlotOrigin(std::uint16_t slot);
    static std::uint32_t packWaterSeed(float depthM);

    bool allocateState(
        StateRuntime& state,
        GLenum internalFormat,
        GLenum clearFormat,
        GLenum clearType,
        std::size_t bytesPerTexel,
        std::string& errorMessage);
    bool ensureSnowState(std::string& errorMessage);
    bool ensureMudState(std::string& errorMessage);
    void destroyState(StateRuntime& state);
    void clearStateSlot(StateRuntime& state, std::uint16_t slot, std::uint32_t clearValue);

    bool ensureWorldTileSimulation(
        const heritage::physics::dynamicsurface::DynamicSurfaceSystem* dynamicSurface,
        float backgroundSeedDepthM,
        std::string& errorMessage);
    void dispatchWorldTileSimulation(
        double elapsedSeconds,
        float precipitationRateMmPerHour,
        float weatherDrainageRateMmPerHour,
        float evaporationRateMmPerHour);
    void applyWorldTireEvents(
        const std::vector<DynamicSurfaceGpuTireContactEvent>& events);
    void destroyWorldTileSimulation();

    void refreshResidency(
        double elapsedSeconds,
        double cameraGlobalX,
        double cameraGlobalZ,
        float backgroundSeedDepthM);
    void rebuildTileIndirection();
    std::uint16_t allocateTileSlot();
    void releaseTileSlot(std::uint16_t slot);

    bool rebuildExactGeometryAtlas(
        const heritage::physics::dynamicsurface::DynamicSurfaceSystem* dynamicSurface,
        std::string& errorMessage);
    void destroyExactGeometryAtlas();

    void dispatchDueTiles(
        double elapsedSeconds,
        float precipitationRateMmPerHour,
        float weatherDrainageRateMmPerHour,
        float evaporationRateMmPerHour,
        float ambientTemperatureC);
    void dispatchWaterBatch(
        const std::vector<TileRuntime*>& tiles,
        double elapsedSeconds,
        float precipitationRateMmPerHour,
        float weatherDrainageRateMmPerHour,
        float evaporationRateMmPerHour,
        float ambientTemperatureC);
    void dispatchTileState(
        StateKind kind,
        StateRuntime& state,
        GLuint program,
        const ProgramUniforms& uniforms,
        const TileRuntime& tile,
        float cycleDtSeconds,
        float precipitationRateMmPerHour,
        float weatherDrainageRateMmPerHour,
        float evaporationRateMmPerHour,
        float ambientTemperatureC);

    void refreshWaterPresentationTile(const TileRuntime& tile);
    void clearWaterPresentationSlot(std::uint16_t slot);
    void refreshWaterDiagnostics(
        double elapsedSeconds,
        double cameraGlobalX,
        double cameraGlobalZ);

    void updateGpuTimerResult();
    bool beginGpuTimer();
    void endGpuTimer(bool began);

    GLuint m_waterProgram = 0;
    GLuint m_waterBatchScatterProgram = 0;
    GLuint m_waterBatchScratch = 0;
    GLuint m_waterBatchTileBuffer = 0;
    GLint m_waterBatchScatterCountLocation = -1;
    GLuint m_worldTileProgram = 0;
    GLuint m_worldTireProgram = 0;
    GLuint m_snowProgram = 0;
    GLuint m_mudProgram = 0;
    GLuint m_tireEventProgram = 0;
    GLuint m_waterPresentationProgram = 0; // retired; direct GL_LINEAR water atlas presentation
    ProgramUniforms m_waterUniforms{};
    ProgramUniforms m_snowUniforms{};
    ProgramUniforms m_mudUniforms{};
    TireEventUniforms m_tireEventUniforms{};
    WaterPresentationUniforms m_waterPresentationUniforms{};

    StateRuntime m_water{};
    StateRuntime m_snow{};
    StateRuntime m_mud{};

    GLuint m_waterPresentationAtlas = 0;

    GLuint m_worldTileStateBuffer = 0;
    GLuint m_worldTileCoordBuffer = 0;
    GLuint m_worldTireEventBuffer = 0;
    std::unordered_map<std::uint64_t, std::uint32_t> m_worldChunkBaseIndices;
    std::uint64_t m_worldLayoutFingerprint = 0u;
    std::uint32_t m_worldTileCount = 0u;
    bool m_worldCatalogReady = false;
    std::uint32_t m_worldCohortCursor = 0u;
    double m_nextWorldTileDueSeconds = 0.0;

    GLuint m_tileIndirectionTexture = 0;
    std::vector<std::uint16_t> m_tileIndirectionScratch;
    std::int32_t m_tileMapOriginX = 0;
    std::int32_t m_tileMapOriginZ = 0;

    std::unordered_map<std::uint64_t, TileRuntime> m_tiles;
    std::vector<std::uint16_t> m_freeSlots;
    std::vector<GpuHydroBatchTile> m_waterBatchTileScratch;

    GLuint m_geometryTriangleBuffer = 0;
    GLuint m_geometryBinHeaderBuffer = 0;
    GLuint m_geometryBinIndexBuffer = 0;
    GLuint m_geometryTileMetaBuffer = 0;
    bool m_geometryAtlasReady = false;
    std::int32_t m_geometryCenterChunkX = 0;
    std::int32_t m_geometryCenterChunkZ = 0;

    std::int32_t m_centerTileX = 0;
    std::int32_t m_centerTileZ = 0;
    bool m_centerTileValid = false;
    double m_lastCameraGlobalX = 0.0;
    double m_lastCameraGlobalZ = 0.0;
    double m_lastCameraSampleSeconds = -1.0;
    double m_cameraVelocityX = 0.0;
    double m_cameraVelocityZ = 0.0;
    double m_nextResidencyRefreshSeconds = 0.0;
    double m_nextWaterDiagnosticsSeconds = 0.0;
    float m_backgroundSeedDepthM = 0.0f;
    static constexpr std::uint32_t kWaterProbeResolution = 16u;
    std::array<std::uint8_t,
        kWaterProbeResolution * kWaterProbeResolution * 4u> m_waterProbeScratch{};

    std::array<GLuint, 4> m_gpuTimerStartQueries{};
    std::array<GLuint, 4> m_gpuTimerEndQueries{};
    std::array<bool, 4> m_gpuTimerPending{};
    std::size_t m_gpuTimerWriteIndex = 0;

    double m_lastElapsedSeconds = -1.0;
    double m_nextHighResolutionBatchDueSeconds = 0.0;
    DynamicSurfaceGpuLodPrototypeStats m_stats{};
};

} // namespace heritage::graphics::dynamicsurface

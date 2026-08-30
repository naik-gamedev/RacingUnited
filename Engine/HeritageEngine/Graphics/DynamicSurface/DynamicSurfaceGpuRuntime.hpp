#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glad/glad.h>


namespace heritage::physics { class SurfacePresentation; }
namespace heritage::physics::dynamicsurface { class DynamicSurfaceSystem; }
namespace heritage::physics::water { class SurfaceHydrology; }

namespace heritage::graphics::dynamicsurface {

struct DynamicSurfaceGpuRuntimeStats
{
    bool ready = false;
    bool waterReady = false;
    bool snowReady = false;
    bool mudReady = false;
    bool tireMarksReady = false;
    bool authoritative = false;
    double cpuDispatchMs = 0.0;
    double gpuComputeMs = 0.0;

    // PERF02: CPU/driver attribution for the Dynamic Surface GPU runtime.
    // These are wall-clock timings around existing work only. No glFinish,
    // blocking query, or wait-for-GPU behavior is introduced by the profiler.
    double cpuBookkeepingMs = 0.0;
    double cpuTireReadbackPollMs = 0.0;
    double cpuResidencyMs = 0.0;
    double cpuStateProvisionMs = 0.0;
    double cpuGeometryBindMs = 0.0;
    double cpuGpuTimerWallMs = 0.0;
    double cpuOptionalTileDispatchMs = 0.0;
    double cpuTireEventMs = 0.0;
    double cpuTireWaterDispatchMs = 0.0;
    double cpuResidualMs = 0.0;

    double nearResidencyBuildMs = 0.0;
    double nearTopologyRasterMs = 0.0;
    double nearTopologyUploadGlMs = 0.0;
    double tileIndirectionUploadGlMs = 0.0;
    double farTopologyTotalMs = 0.0;
    double farCandidateBuildMs = 0.0;
    double farCandidateSortMs = 0.0;
    double farMissingScanMs = 0.0;
    double farTileResolveMs = 0.0;
    double farAtlasUploadGlMs = 0.0;
    double farTagUploadGlMs = 0.0;
    std::uint32_t farCandidateTilesEvaluated = 0;
    bool residencyPolledThisFrame = false;

    // Persist the most recent 20 Hz topology-poll attribution so F8 screenshots
    // remain diagnostic even when captured on the interleaved no-poll frame.
    double lastResidencyPollMs = 0.0;
    double lastFarTopologyTotalMs = 0.0;
    double lastFarCandidateBuildMs = 0.0;
    double lastFarCandidateSortMs = 0.0;
    double lastFarMissingScanMs = 0.0;
    double lastFarTileResolveMs = 0.0;
    double lastFarAtlasUploadGlMs = 0.0;
    double lastFarTagUploadGlMs = 0.0;
    std::uint32_t lastFarCandidateTilesEvaluated = 0;

    double optionalTileDispatchGlMs = 0.0;
    double optionalTileCopyGlMs = 0.0;
    double optionalTileBarrierGlMs = 0.0;
    double tireEventSetupGlMs = 0.0;
    double tireEventUniformGlMs = 0.0;
    double tireEventDispatchGlMs = 0.0;
    double tireEventSlowestDispatchGlMs = 0.0;
    double tireEventBarrierGlMs = 0.0;
    double tireReadbackClientWaitGlMs = 0.0;
    double tireReadbackMapGlMs = 0.0;
    double tireReadbackUnmapGlMs = 0.0;
    double tireWaterUploadGlMs = 0.0;
    double tireWaterSetupGlMs = 0.0;
    double tireWaterDispatchGlMs = 0.0;
    double tireWaterBarrierGlMs = 0.0;
    double tireWaterFenceGlMs = 0.0;
    double committedMiB = 0.0;
    std::uint64_t dispatchesThisFrame = 0;
    std::uint64_t cellsThisFrame = 0;
    std::array<std::uint32_t, 4> geometryValidTiles{};
    std::array<std::uint32_t, 4> activeTiles{};
    std::uint32_t residentTiles = 0;
    std::uint32_t desiredTopologyTiles = 0;
    std::uint32_t visibleTopologyTiles = 0;
    std::uint32_t prebakedTopologyTiles = 0;
    std::uint32_t fallbackTopologyTiles = 0;
    std::uint32_t topologyUploadsThisFrame = 0;
    std::uint32_t farDesiredTopologyTiles = 0;
    std::uint32_t farResidentTopologyTiles = 0;
    std::uint32_t farTopologyUploadsThisFrame = 0;
    std::uint32_t farTopologyBacklogTiles = 0;
    std::uint32_t prewarmTiles = 0;
    std::uint32_t dueTiles = 0;
    std::uint32_t dispatchBacklogTiles = 0;
    std::int32_t centerTileX = 0;
    std::int32_t centerTileZ = 0;
    std::uint64_t cameraTileRebases = 0;
    std::uint64_t tireEventDispatches = 0;
    std::uint64_t tireEventCells = 0;
    std::uint64_t tireWaterSampleDispatches = 0;
    std::uint64_t tireWaterSamplesCompleted = 0;
    std::uint64_t tireWaterSampleReadbackDrops = 0;
    bool exactGeometrySupportReady = false;
    std::uint64_t geometryTriangles = 0;
    std::uint64_t geometryBinReferences = 0;
    double geometryUploadMiB = 0.0;
    double cameraSpeedMps = 0.0;
    double predictivePrewarmM = 0.0;
    float precipitationRateMmPerHour = 0.0f;
    float runoffDriverMmPerHour = 0.0f;
    float drainageRateMmPerHour = 0.0f;
    float evaporationRateMmPerHour = 0.0f;
    float backgroundSeedDepthM = 0.0f;
    float surfaceWettingExposureM = 0.0f;
};

struct DynamicSurfaceGpuTireWaterSampleRequest
{
    double globalX = 0.0;
    double globalZ = 0.0;
};

struct DynamicSurfaceGpuTireWaterSample
{
    double globalX = 0.0;
    double globalZ = 0.0;
    float waterDepthM = 0.0f;
    float dryLine = 0.0f;
    bool valid = false;
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

// LIVETRACK15: fixed production world-prebaked water working set.
// Every world tile is exactly 10m x 10m. The near atlas keeps 256x256 texels
// (~3.90625cm/texel) for tire interaction and close puddle edges; a separate
// 32x32 mesh-raster topology cache carries the same immutable basin response
// through 500m without turning 500m into a centimetre-resolution simulation.
// Rainfall accumulation remains one scene scalar and water depth is reconstructed
// in the material shader; there is no periodic full-field water compute solver.
//
// The sampled water atlas is GL_RGBA8 because OpenGL 4.6 image load/store does
// not expose rgba4 as a compute-writable image format. Each channel is still
// quantized to exactly 16 logical values (multiples of 17/255), so the state
// semantics remain four logical 4-bit channels:
//   R baked runoff accumulation | G dry-line strength | B puddle capacity | A downhill angle.
// GL_LINEAR is used directly for rendering. There is no second puddle texture.
// Detailed 256x256 topology is kept only through 100m. A mesh-prebaked 32x32
// presentation cache extends puddles through 500m, with a 10m prefetch halo.
// The far cache is a rolling world-tile cache, not a second simulation: it stores
// immutable runoff/standing-depth/flow. LIVETRACK18 admits the complete 500m set in one
// 20Hz poll from the scene-baked .hhyd v15 payload; there is no progressive backlog.
// Exact scene rain exposure stays scalar. A returning tile reconstructs from
// scene rain plus immutable .hhyd topology; collision geometry is never re-probed
// by the production runtime water path.
class DynamicSurfaceGpuRuntime
{
public:
    DynamicSurfaceGpuRuntime() = default;
    DynamicSurfaceGpuRuntime(const DynamicSurfaceGpuRuntime&) = delete;
    DynamicSurfaceGpuRuntime& operator=(const DynamicSurfaceGpuRuntime&) = delete;

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
        float windVelocityXMps,
        float windVelocityZMps,
        const heritage::physics::dynamicsurface::DynamicSurfaceSystem* dynamicSurface,
        const heritage::physics::water::SurfaceHydrology* prebakedHydrology,
        std::uint64_t hydrologyResetSerial,
        const std::vector<DynamicSurfaceGpuTireContactEvent>& tireEvents,
        const std::vector<DynamicSurfaceGpuTireWaterSampleRequest>& tireWaterSampleRequests);

    void applyTireContactEvents(
        const std::vector<DynamicSurfaceGpuTireContactEvent>& events);

    void consumeCompletedTireWaterSamples(
        std::vector<DynamicSurfaceGpuTireWaterSample>& outSamples);

    // LIVETRACK22: tire-mark persistence remains owned by SurfacePresentation,
    // but its near visual state is rasterized into the exact same 10m/256x256
    // resident tile slots and indirection used by Dynamic Surface water/snow/mud.
    // Returning tiles reconstruct from authoritative FP64 mark history instead
    // of popping because a presentation page happened to be evicted.
    void syncTireMarkPresentation(
        const heritage::physics::SurfacePresentation& presentation);

    const DynamicSurfaceGpuRuntimeStats& stats() const { return m_stats; }

    GLuint waterTexture(std::size_t = 0) const { return m_water.atlas; }
    GLuint farWaterTexture() const { return m_farWaterAtlas; }
    GLuint farTileTagTexture() const { return m_farTileTagTexture; }
    GLuint snowTexture(std::size_t = 0) const { return m_snow.atlas; }
    GLuint mudTexture(std::size_t = 0) const { return m_mud.atlas; }
    GLuint tireMarkTexture() const { return m_tireMarks.atlas; }
    GLuint tileIndirectionTexture() const { return m_tileIndirectionTexture; }
    bool authoritativeReady() const
    {
        return m_stats.ready && m_stats.waterReady && m_stats.authoritative;
    }
    std::int32_t centerTileX() const { return m_centerTileX; }
    std::int32_t centerTileZ() const { return m_centerTileZ; }

    static constexpr std::uint32_t tileResolution() { return kTileResolution; }
    static constexpr float tileWorldSizeM() { return kTileWorldSizeM; }
    static constexpr std::uint32_t atlasColumns() { return kAtlasColumns; }
    static constexpr std::uint32_t farTileResolution() { return kFarTileResolution; }
    static constexpr std::uint32_t farAtlasTilesPerAxis() { return kFarAtlasTilesPerAxis; }
    static constexpr std::uint32_t tileMapResolution() { return kTileMapResolution; }
    static constexpr std::int32_t tileMapHalfSpan() { return kTileMapHalfSpan; }
    static constexpr float simulationRadiusM() { return kSimulationRadiusM; }
    static constexpr float presentationRadiusM() { return kPresentationRadiusM; }
    static constexpr std::size_t maximumTireContactEventsPerFrame()
    {
        return kMaximumTireContactEventsPerFrame;
    }
    static constexpr std::size_t maximumTireWaterSampleRequestsPerFrame()
    {
        return kMaximumTireWaterSampleRequestsPerFrame;
    }

private:
    static constexpr float kTileWorldSizeM = 10.0f;
    static constexpr std::uint32_t kTileResolution = 256u;
    static constexpr float kCellSizeM = kTileWorldSizeM / float(kTileResolution);
    // 20x20 slots fit the exact-camera 105m high-resolution topology disk
    // (worst case <400 tiles). The far cache is independent and lower resolution.
    static constexpr std::uint32_t kAtlasColumns = 20u;
    static constexpr std::uint32_t kAtlasRows = 20u;
    static constexpr std::uint32_t kMaximumTileSlots = kAtlasColumns * kAtlasRows;
    static constexpr std::uint32_t kMaximumResidentTiles = kMaximumTileSlots;
    static constexpr float kTopologyPrefetchRadiusM = 105.0f;
    static constexpr float kPresentationRadiusM = 500.0f;
    static constexpr float kFarTopologyPrefetchRadiusM = 510.0f;
    static constexpr std::uint32_t kFarTileResolution = 32u;
    static constexpr std::uint32_t kFarAtlasTilesPerAxis = 128u;
    static constexpr std::uint32_t kFarAtlasWidth = kFarAtlasTilesPerAxis * kFarTileResolution;
    static constexpr std::uint32_t kFarAtlasHeight = kFarAtlasTilesPerAxis * kFarTileResolution;
    static constexpr double kPresentationPollIntervalSeconds = 1.0 / 20.0;
    static constexpr std::uint32_t kFarBulkUploadThreshold = 512u;
    static constexpr std::uint32_t kOptionalTileBudgetPerFrame = 12u;
    static constexpr std::uint32_t kAtlasWidth = kAtlasColumns * kTileResolution;
    static constexpr std::uint32_t kAtlasHeight = kAtlasRows * kTileResolution;
    static constexpr std::uint32_t kTileMapResolution = 64u;
    static constexpr std::int32_t kTileMapHalfSpan = 32;
    static constexpr float kSimulationRadiusM = 100.0f;
    static constexpr std::size_t kMaximumTireContactEventsPerFrame = 256u;
    static constexpr std::size_t kMaximumTireWaterSampleRequestsPerFrame = 1024u;
    // Geometry lookup is optional snow/mud acceleration metadata. Production
    // water topology comes only from the immutable .hhyd scene bake; runtime
    // water never evaluates collision triangles per texel.
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
        bool initialized = false;
        bool prebakedTopology = false;
        bool recentVehicleContact = false;
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
        GLint stateAtlas = -1;
        GLint tileMap = -1;
        GLint windVelocityXZ = -1;
    };

    struct TireWaterSampleUniforms
    {
        GLint waterAtlas = -1;
        GLint tileIndirection = -1;
        GLint tileResolution = -1;
        GLint atlasColumns = -1;
        GLint tileMapCenter = -1;
        GLint prebakedWaterExposureM = -1;
        GLint rainWettingExposureM = -1;
        GLint runoffDriverMmPerHour = -1;
        GLint sampleCount = -1;
    };

    struct TireWaterSampleReadbackSlot
    {
        GLuint inputBuffer = 0;
        GLuint outputBuffer = 0;
        GLsync fence = nullptr;
        std::uint32_t count = 0;
        std::uint64_t hydrologyResetSerial = 0;
        std::vector<std::array<double, 2>> globalPositions;
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
    std::uint8_t cadenceBandForDistance(float distanceM, bool prewarm) const;
    static std::array<std::uint32_t, 2> atlasSlotOrigin(std::uint16_t slot);

    bool allocateState(
        StateRuntime& state,
        GLenum internalFormat,
        GLenum clearFormat,
        GLenum clearType,
        std::size_t bytesPerTexel,
        bool allocateScratch,
        std::string& errorMessage);
    bool ensureSnowState(std::string& errorMessage);
    bool ensureMudState(std::string& errorMessage);
    bool initializeTireMarkState(std::string& errorMessage);
    void shutdownTireMarkState();
    void resetTireMarkAtlas();
    void indexNewTireMarkSegments(
        const heritage::physics::SurfacePresentation& presentation);
    void rasterizeTireMarkTile(
        const heritage::physics::SurfacePresentation& presentation,
        const TileRuntime& tile);
    void destroyState(StateRuntime& state);
    void clearStateSlot(StateRuntime& state, std::uint16_t slot, std::uint32_t clearValue);

    void refreshResidency(
        double elapsedSeconds,
        double cameraGlobalX,
        double cameraGlobalZ,
        float backgroundSeedDepthM,
        const heritage::physics::water::SurfaceHydrology* prebakedHydrology);
    void rebuildTileIndirection();
    void streamFarTopology(
        double cameraGlobalX,
        double cameraGlobalZ,
        const heritage::physics::water::SurfaceHydrology* prebakedHydrology);
    void invalidateFarTopologyCache();
    static std::uint32_t farAtlasSlotCoordinate(std::int32_t worldTile);
    std::uint16_t allocateTileSlot();
    void releaseTileSlot(std::uint16_t slot);

    bool rebuildExactGeometryAtlas(
        const heritage::physics::dynamicsurface::DynamicSurfaceSystem* dynamicSurface,
        std::string& errorMessage);
    void destroyExactGeometryAtlas();

    void resetFrameProfilingStats();

    void dispatchDueTiles(
        double elapsedSeconds,
        float precipitationRateMmPerHour,
        float weatherDrainageRateMmPerHour,
        float evaporationRateMmPerHour,
        float ambientTemperatureC);
    void dispatchTileState(
        StateRuntime& state,
        GLuint program,
        const ProgramUniforms& uniforms,
        const TileRuntime& tile,
        float cycleDtSeconds,
        float precipitationRateMmPerHour,
        float weatherDrainageRateMmPerHour,
        float evaporationRateMmPerHour,
        float ambientTemperatureC);

    bool initializeTireWaterSampleBridge(std::string& errorMessage);
    void shutdownTireWaterSampleBridge();
    void pollTireWaterSampleReadbacks();
    void dispatchTireWaterSamples(
        const std::vector<DynamicSurfaceGpuTireWaterSampleRequest>& requests);

    void updateGpuTimerResult();
    bool beginGpuTimer();
    void endGpuTimer(bool began);

    GLuint m_snowProgram = 0;
    GLuint m_mudProgram = 0;
    GLuint m_tireEventProgram = 0;
    GLuint m_tireWaterSampleProgram = 0;
    ProgramUniforms m_snowUniforms{};
    ProgramUniforms m_mudUniforms{};
    TireEventUniforms m_tireEventUniforms{};
    TireWaterSampleUniforms m_tireWaterSampleUniforms{};
    std::array<TireWaterSampleReadbackSlot, 3> m_tireWaterSampleReadback{};
    std::size_t m_tireWaterSampleWriteIndex = 0;
    std::vector<DynamicSurfaceGpuTireWaterSample> m_completedTireWaterSamples;

    StateRuntime m_water{};
    StateRuntime m_snow{};
    StateRuntime m_mud{};
    StateRuntime m_tireMarks{};

    GLuint m_tileIndirectionTexture = 0;
    GLuint m_farWaterAtlas = 0;
    GLuint m_farTileTagTexture = 0;
    std::vector<std::uint16_t> m_tileIndirectionScratch;
    std::vector<std::array<std::int32_t, 2>> m_farTileTags;
    std::int32_t m_tileMapOriginX = 0;
    std::int32_t m_tileMapOriginZ = 0;

    std::unordered_map<std::uint64_t, TileRuntime> m_tiles;
    std::vector<std::uint16_t> m_freeSlots;
    std::vector<std::uint8_t> m_prebakedTopologyScratch;
    std::vector<std::uint8_t> m_farAtlasCpuMirror;
    std::vector<std::uint8_t> m_prebakedRgbaScratch;
    std::vector<std::uint8_t> m_tireMarkRasterScratch;
    std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> m_tireMarkSerialsByTile;
    std::unordered_set<std::uint64_t> m_dirtyTireMarkTiles;
    std::uint64_t m_lastIndexedTireMarkSerial = 0u;
    std::uint64_t m_lastTireMarkFirstSerial = 0u;
    double m_lastTireMarkPresentationTime = -1.0;
    double m_lastTireMarkRetirementRefreshTime = -1.0;

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
    double m_lastResidencyCameraGlobalX = 0.0;
    double m_lastResidencyCameraGlobalZ = 0.0;
    bool m_residencyCameraValid = false;
    double m_lastPresentationPollSeconds = -1.0;
    double m_cameraVelocityX = 0.0;
    double m_cameraVelocityZ = 0.0;
    float m_windVelocityX = 0.0f;
    float m_windVelocityZ = 0.0f;
    float m_backgroundSeedDepthM = 0.0f;
    float m_surfaceWettingExposureM = 0.0f;
    float m_runoffDriverMmPerHour = 0.0f;
    std::uint64_t m_appliedHydrologyResetSerial = 0u;
    std::array<GLuint, 4> m_gpuTimerStartQueries{};
    std::array<GLuint, 4> m_gpuTimerEndQueries{};
    std::array<bool, 4> m_gpuTimerPending{};
    std::size_t m_gpuTimerWriteIndex = 0;

    double m_lastElapsedSeconds = -1.0;
    DynamicSurfaceGpuRuntimeStats m_stats{};
};

} // namespace heritage::graphics::dynamicsurface

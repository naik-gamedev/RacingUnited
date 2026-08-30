#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glad/glad.h>

#include "../../Camera/ChaseCamera.hpp"
#include "../../Core/Entities/EntityRegistry.hpp"
#include "../../Core/Math/Math.hpp"
#include "../../Core/Settings/VideoSettings.hpp"
#include "../EnvironmentMap.hpp"
#include "../EnvironmentSystem.hpp"
#include "../DynamicSurface/DynamicSurfaceGpuRuntime.hpp"
#include "SkyRenderer.hpp"
#include "../Mesh.hpp"
#include "../Texture2D.hpp"
#include "../../Physics/Surfaces/Water/SurfaceHydrology.hpp"
#include "../../Physics/Surfaces/SurfaceWorld.hpp"

namespace heritage::graphics {

// Visual-only flexible-ring mesh deformation LOD. Tire simulation itself is
// never distance gated.
inline constexpr float kTireVisualDeformationMaximumDistanceM = 50.0f;


struct EntityMeshRenderTargetState
{
    GLuint framebuffer = 0;
    GLint viewportX = 0;
    GLint viewportY = 0;
    GLsizei viewportWidth = 1;
    GLsizei viewportHeight = 1;
    GLsizei samples = 1;
    bool scissorEnabled = false;
    GLint scissorX = 0;
    GLint scissorY = 0;
    GLsizei scissorWidth = 1;
    GLsizei scissorHeight = 1;
};

struct EntityMeshRendererStats
{
    std::uint64_t drawCalls = 0;
    std::uint64_t triangles = 0;
    std::uint64_t meshInstances = 0;
    std::uint64_t candidateRanges = 0;
    std::uint64_t culledRanges = 0;
    std::uint64_t culledTriangles = 0;
    std::uint64_t skippedAuthoringRanges = 0;

    // PERF08 render forensics. These are CPU wall times only; they do not use
    // glFinish and therefore preserve normal frame behavior. If the OpenGL
    // driver blocks inside a draw, that wait naturally appears in the owning
    // mesh-instance time, which is exactly what we want to diagnose.
    double instanceGatherMs = 0.0;
    // PERF01: stage attribution inside EntityMeshRenderer::draw(). These are
    // wall-clock timings only and intentionally introduce no GPU waits.
    double framePreparationCpuMs = 0.0;
    double dynamicSurfaceUpdateCpuMs = 0.0;
    double regionalWeatherUpdateCpuMs = 0.0;
    double weatherLightingCpuMs = 0.0;
    double environmentUpdateMs = 0.0;
    double skyDrawMs = 0.0;
    // PERF03: fine-grained SkyRenderer::draw() wall-time attribution copied
    // from SkyRenderer so F8 can identify the ~20 ms sky/background owner.
    double skyBackgroundTotalCpuMs = 0.0;
    double skyGpuTimerPollTotalCpuMs = 0.0;
    double skyGpuTimerPollBackgroundCpuMs = 0.0;
    double skyGpuTimerPollCloudShadowCpuMs = 0.0;
    double skyGpuTimerPollSceneCopyCpuMs = 0.0;
    double skyGpuTimerPollRaymarchCpuMs = 0.0;
    double skyGpuTimerPollUpscaleCpuMs = 0.0;
    double skyGpuTimerPollTemporalCpuMs = 0.0;
    double skyGpuTimerPollPresentCpuMs = 0.0;
    double skyBackgroundTimerBeginCpuMs = 0.0;
    double skyBackgroundStateSetupCpuMs = 0.0;
    double skyBackgroundUniformUploadCpuMs = 0.0;
    double skyBackgroundTextureBindCpuMs = 0.0;
    double skyBackgroundDrawCallCpuMs = 0.0;
    double skyBackgroundTimerEndCpuMs = 0.0;
    double skyCloudShadowUpdateCpuMs = 0.0;
    // PERF04 cloud-shadow update attribution copied from SkyRenderer.
    double cloudShadowInternalTotalCpuMs = 0.0;
    double cloudShadowEligibilityCpuMs = 0.0;
    double cloudShadowTargetEnsureCpuMs = 0.0;
    double cloudShadowTimerBeginCpuMs = 0.0;
    double cloudShadowStateSetupCpuMs = 0.0;
    double cloudShadowRawAttachmentCpuMs = 0.0;
    double cloudShadowProgramBindCpuMs = 0.0;
    double cloudShadowUniformUploadCpuMs = 0.0;
    double cloudShadowTextureBindCpuMs = 0.0;
    double cloudShadowRawDrawCallCpuMs = 0.0;
    double cloudShadowFilterSetupCpuMs = 0.0;
    double cloudShadowFilterAttachmentCpuMs = 0.0;
    double cloudShadowFilterTextureBindCpuMs = 0.0;
    double cloudShadowFilterDrawCallCpuMs = 0.0;
    double cloudShadowCopyImageCpuMs = 0.0;
    double cloudShadowFinalizeCpuMs = 0.0;
    double cloudShadowTimerEndCpuMs = 0.0;
    double cloudShadowResidualCpuMs = 0.0;
    double skyBackgroundRestoreCpuMs = 0.0;
    double materialSetupCpuMs = 0.0;
    // OPT00 completed asynchronous GPU samples from SkyRenderer. These are
    // diagnostic children of the top-level mesh GPU pass.
    double skyBackgroundGpuMs = 0.0;
    double cloudShadowGpuMs = 0.0;
    double cloudSceneCopyGpuMs = 0.0;
    double cloudRaymarchGpuMs = 0.0;
    double cloudUpscaleGpuMs = 0.0;
    double cloudTemporalGpuMs = 0.0;
    double cloudPresentGpuMs = 0.0;

    // PERF01 cloud CPU submission attribution copied from SkyRenderer. Exact
    // blit/draw-call timers expose driver back-pressure without glFinish.
    double cloudAfterOpaqueCpuMs = 0.0;
    double cloudPipelineCpuMs = 0.0;
    double cloudTargetEnsureCpuMs = 0.0;
    double cloudSceneCopyCpuMs = 0.0;
    double cloudSceneColorBlitCpuMs = 0.0;
    double cloudSceneDepthBlitCpuMs = 0.0;
    double cloudRaymarchCpuMs = 0.0;
    double cloudRaymarchDrawCallCpuMs = 0.0;
    double cloudUpscaleCpuMs = 0.0;
    double cloudUpscaleDrawCallCpuMs = 0.0;
    double cloudTemporalCpuMs = 0.0;
    double cloudTemporalDrawCallCpuMs = 0.0;
    double cloudPresentCpuMs = 0.0;
    double cloudPresentDrawCallCpuMs = 0.0;
    double cloudDepthMergeDrawCallCpuMs = 0.0;
    double cloudRestoreCpuMs = 0.0;

    double meshInstancesCpuMs = 0.0;
    double meshVisibleInstancesCpuMs = 0.0;
    double meshDriverDrawCpuMs = 0.0;
    double slowestMeshDriverDrawCpuMs = 0.0;
    double rendererRestoreCpuMs = 0.0;
    double outerGpuTimerCpuMs = 0.0;
    double slowestMeshInstanceMs = 0.0;
    std::string slowestMeshAsset;
    std::uint64_t environmentRefreshes = 0;
    std::uint64_t vaoBinds = 0;
    std::uint64_t materialSwitches = 0;
    std::uint64_t textureBinds = 0;
    std::uint64_t frontFaceChanges = 0;
    std::uint64_t skinnedRanges = 0;
    std::uint64_t tireDeformationActiveRanges = 0;
    std::uint64_t tireDeformationDistanceCulledRanges = 0;

    // LIVETRACK11 GPU dynamic-surface diagnostics. Water topology is prebaked;
    // rain-to-puddle reconstruction is evaluated in the material shader. Only
    // localized tire clearing and optional snow/mud use compute dispatches.
    bool dynamicSurfaceGpuRuntimeReady = false;
    bool dynamicSurfaceGpuRuntimeWaterReady = false;
    bool dynamicSurfaceGpuRuntimeSnowReady = false;
    bool dynamicSurfaceGpuRuntimeMudReady = false;
    double dynamicSurfaceGpuRuntimeCpuMs = 0.0;
    double dynamicSurfaceGpuRuntimeGpuMs = 0.0;

    // PERF02 Dynamic Surface CPU/driver attribution. Mirrors the production
    // GPU runtime's current-frame wall timings for the F8 overlay.
    double dynamicSurfaceGpuBookkeepingCpuMs = 0.0;
    double dynamicSurfaceGpuTireReadbackPollCpuMs = 0.0;
    double dynamicSurfaceGpuResidencyCpuMs = 0.0;
    double dynamicSurfaceGpuStateProvisionCpuMs = 0.0;
    double dynamicSurfaceGpuGeometryBindCpuMs = 0.0;
    double dynamicSurfaceGpuTimerWallCpuMs = 0.0;
    double dynamicSurfaceGpuOptionalDispatchCpuMs = 0.0;
    double dynamicSurfaceGpuTireEventCpuMs = 0.0;
    double dynamicSurfaceGpuTireWaterDispatchCpuMs = 0.0;
    double dynamicSurfaceGpuResidualCpuMs = 0.0;

    double dynamicSurfaceNearResidencyBuildCpuMs = 0.0;
    double dynamicSurfaceNearTopologyRasterCpuMs = 0.0;
    double dynamicSurfaceNearTopologyUploadGlMs = 0.0;
    double dynamicSurfaceTileIndirectionUploadGlMs = 0.0;
    double dynamicSurfaceFarTopologyCpuMs = 0.0;
    double dynamicSurfaceFarCandidateBuildCpuMs = 0.0;
    double dynamicSurfaceFarCandidateSortCpuMs = 0.0;
    double dynamicSurfaceFarMissingScanCpuMs = 0.0;
    double dynamicSurfaceFarTileResolveCpuMs = 0.0;
    double dynamicSurfaceFarAtlasUploadGlMs = 0.0;
    double dynamicSurfaceFarTagUploadGlMs = 0.0;
    std::uint32_t dynamicSurfaceFarCandidateTilesEvaluated = 0;
    bool dynamicSurfaceResidencyPolledThisFrame = false;
    double dynamicSurfaceLastResidencyPollCpuMs = 0.0;
    double dynamicSurfaceLastFarTopologyCpuMs = 0.0;
    double dynamicSurfaceLastFarCandidateBuildCpuMs = 0.0;
    double dynamicSurfaceLastFarCandidateSortCpuMs = 0.0;
    double dynamicSurfaceLastFarMissingScanCpuMs = 0.0;
    double dynamicSurfaceLastFarTileResolveCpuMs = 0.0;
    double dynamicSurfaceLastFarAtlasUploadGlMs = 0.0;
    double dynamicSurfaceLastFarTagUploadGlMs = 0.0;
    std::uint32_t dynamicSurfaceLastFarCandidateTilesEvaluated = 0;

    double dynamicSurfaceOptionalDispatchGlMs = 0.0;
    double dynamicSurfaceOptionalCopyGlMs = 0.0;
    double dynamicSurfaceOptionalBarrierGlMs = 0.0;
    double dynamicSurfaceTireEventSetupGlMs = 0.0;
    double dynamicSurfaceTireEventUniformGlMs = 0.0;
    double dynamicSurfaceTireEventDispatchGlMs = 0.0;
    double dynamicSurfaceTireEventSlowestDispatchGlMs = 0.0;
    double dynamicSurfaceTireEventBarrierGlMs = 0.0;
    double dynamicSurfaceTireReadbackWaitGlMs = 0.0;
    double dynamicSurfaceTireReadbackMapGlMs = 0.0;
    double dynamicSurfaceTireReadbackUnmapGlMs = 0.0;
    double dynamicSurfaceTireWaterUploadGlMs = 0.0;
    double dynamicSurfaceTireWaterSetupGlMs = 0.0;
    double dynamicSurfaceTireWaterDispatchGlMs = 0.0;
    double dynamicSurfaceTireWaterBarrierGlMs = 0.0;
    double dynamicSurfaceTireWaterFenceGlMs = 0.0;

    double dynamicSurfaceGpuRuntimeCommittedMiB = 0.0;
    std::uint64_t dynamicSurfaceGpuRuntimeDispatches = 0;
    std::uint64_t dynamicSurfaceGpuRuntimeCells = 0;
    std::array<std::uint32_t, 4> dynamicSurfaceGpuGeometryValidTiles{};
    std::array<std::uint32_t, 4> dynamicSurfaceGpuActiveTiles{};
    std::uint32_t dynamicSurfaceGpuResidentTiles = 0;
    std::uint32_t dynamicSurfaceGpuDesiredTopologyTiles = 0;
    std::uint32_t dynamicSurfaceGpuVisibleTopologyTiles = 0;
    std::uint32_t dynamicSurfaceGpuPrebakedTopologyTiles = 0;
    std::uint32_t dynamicSurfaceGpuFallbackTopologyTiles = 0;
    std::uint32_t dynamicSurfaceGpuTopologyUploadsThisFrame = 0;
    std::uint32_t dynamicSurfaceGpuFarDesiredTopologyTiles = 0;
    std::uint32_t dynamicSurfaceGpuFarResidentTopologyTiles = 0;
    std::uint32_t dynamicSurfaceGpuFarTopologyUploadsThisFrame = 0;
    std::uint32_t dynamicSurfaceGpuFarTopologyBacklogTiles = 0;
    std::uint32_t dynamicSurfaceGpuPrewarmTiles = 0;
    std::uint32_t dynamicSurfaceGpuDueTiles = 0;
    std::uint32_t dynamicSurfaceGpuBacklogTiles = 0;
    double dynamicSurfaceGpuCameraSpeedMps = 0.0;
    double dynamicSurfaceGpuPrewarmDistanceM = 0.0;
    float dynamicSurfaceGpuRainMmPerHour = 0.0f;
    float dynamicSurfaceGpuRunoffDriverMmPerHour = 0.0f;
    float dynamicSurfaceGpuDrainageMmPerHour = 0.0f;
    float dynamicSurfaceGpuEvaporationMmPerHour = 0.0f;
    float dynamicSurfaceGpuBackgroundSeedDepthM = 0.0f;
    float dynamicSurfaceGpuSurfaceWettingExposureM = 0.0f;
    std::uint64_t dynamicSurfaceGpuRuntimeTireEventDispatches = 0;
    std::uint64_t dynamicSurfaceGpuRuntimeTireEventCells = 0;
    std::uint64_t dynamicSurfaceGpuRuntimeTireWaterSampleDispatches = 0;
    std::uint64_t dynamicSurfaceGpuRuntimeTireWaterSamplesCompleted = 0;
    std::uint64_t dynamicSurfaceGpuRuntimeTireWaterSampleReadbackDrops = 0;
    std::int32_t dynamicSurfaceGpuRuntimeCenterTileX = 0;
    std::int32_t dynamicSurfaceGpuRuntimeCenterTileZ = 0;
    std::uint64_t dynamicSurfaceGpuRuntimeCameraTileRebases = 0;
    bool dynamicSurfaceGpuExactGeometryReady = false;
    std::uint64_t dynamicSurfaceGpuGeometryTriangles = 0;
    std::uint64_t dynamicSurfaceGpuGeometryBinReferences = 0;
    double dynamicSurfaceGpuGeometryUploadMiB = 0.0;

    // SHADOW01/SHADOW05: real-time directional cascaded shadow-map diagnostics.
    // The detailed CPU stages distinguish actual engine work from OpenGL-driver
    // submission stalls. shadowGpuMs is an asynchronous timestamp result from a
    // completed earlier frame and never blocks the current frame.
    double shadowCpuMs = 0.0;
    double shadowSettingsCpuMs = 0.0;
    double shadowCascadeCpuMs = 0.0;
    double shadowPrepareCpuMs = 0.0;
    double shadowStateCpuMs = 0.0;
    double shadowDrawCpuMs = 0.0;
    double shadowDriverDrawCpuMs = 0.0;
    double shadowRestoreCpuMs = 0.0;
    double shadowGpuMs = 0.0;
    std::uint64_t shadowGpuTimerSamples = 0;
    std::uint64_t shadowDrawCalls = 0;
    std::uint64_t shadowTriangles = 0;
    std::uint64_t shadowCulledRanges = 0;
    int shadowCascadeCount = 0;
    int shadowResolution = 0;
    bool shadowsActive = false;
    int shadowFilterMode = 2;
};

// Draws module-owned mesh assets attached through Entity Mesh components.
// OBJ/MTL and glTF binary (.glb) assets are cached by safe module-relative
// paths; GLB node animation and skinning are evaluated per entity instance.
class EntityMeshRenderer
{
public:
    bool initialize(
        const std::filesystem::path& moduleAssetRoot,
        EnvironmentSystem* environmentSystem = nullptr);
    void shutdown();
    void clearCache();
    void requestHotReloadPoll();

    // TIRE26A/VIS19: developer-only dense on-tire probe diagnostics. INSERT toggles
    // the overlay through EngineHotkeys; the renderer keeps the state so no
    // gameplay/module API is involved.
    void setTireProbeDebugVisible(bool visible) { m_tireProbeDebugVisible = visible; }
    bool tireProbeDebugVisible() const { return m_tireProbeDebugVisible; }
    const EnvironmentMap& environmentMap() const { return m_environmentMap; }

    void draw(
        const heritage::entities::EntityRegistry& registry,
        const heritage::math::Mat4& projection,
        const heritage::settings::VideoSettings& videoSettings,
        float elapsedSeconds,
        const heritage::camera::CameraFrame& cameraFrame,
        const EntityMeshRenderTargetState& renderTargetState,
        bool wireframeVisible = false,
        const heritage::physics::SurfaceWorld* surfaceWorld = nullptr);

    std::size_t loadedAssetCount() const;
    void beginFrameStats() { m_frameStats = {}; }
    void recordOuterGpuTimerCpuMs(double milliseconds)
    {
        if (milliseconds > 0.0)
            m_frameStats.outerGpuTimerCpuMs += milliseconds;
    }
    const EntityMeshRendererStats& frameStats() const { return m_frameStats; }
    const std::string& lastError() const { return m_lastError; }

private:
    struct DependencyStamp
    {
        std::filesystem::path path;
        std::filesystem::file_time_type lastWriteTime{};
        bool exists = false;
    };

    struct CachedAsset
    {
        Mesh mesh;
        std::filesystem::file_time_type lastWriteTime{};
        std::vector<DependencyStamp> dependencies;
        std::uint64_t lastHotReloadEpoch = 0;
        bool attempted = false;
        bool loaded = false;
        std::string error;
    };

    struct ResolvedTexturePath
    {
        std::filesystem::path resolved;
        std::string error;
        std::uint64_t lastHotReloadEpoch = 0;
        bool valid = false;
        bool attempted = false;
    };

    struct UniformLocations
    {
        GLint baseColorMap = -1;
        GLint normalMap = -1;
        GLint roughnessMap = -1;
        GLint metallicMap = -1;
        GLint specularMap = -1;
        GLint ambientOcclusionMap = -1;
        GLint emissiveMap = -1;
        GLint opacityMap = -1;
        GLint specularFactorMap = -1;
        GLint environmentMap = -1;
        GLint shadowMap = -1;
        GLint shadowDepthMap = -1;
        GLint shadowFilterMode = -1;
        GLint shadowMatrices = -1;
        GLint shadowSplits = -1;
        GLint hasShadowMap = -1;
        GLint shadowStrength = -1;
        GLint view = -1;
        GLint projection = -1;
        GLint eye = -1;
        GLint sunDirection = -1;
        GLint sunRadiance = -1;
        GLint gamma = -1;
        GLint brightness = -1;
        GLint contrast = -1;
        GLint saturation = -1;
        GLint weatherFogDensity = -1;
        GLint weatherFogColor = -1;
        GLint regionalWeatherMap = -1;
        GLint regionalWeatherMapValid = -1;
        GLint regionalWeatherCameraOffsetXZ = -1;
        GLint regionalWeatherAdvectionXZ = -1;
        GLint regionalWeatherHalfRangeM = -1;
        GLint weatherCloudBaseM = -1;
        GLint volumetricCloudShadow = -1;
        GLint hasVolumetricCloudShadow = -1;
        GLint volumetricCloudShadowHalfRangeM = -1;
        // Scene material wetness uses the fixed LIVETRACK11 prebaked GPU path.
        // The legacy CPU Hydro page mirror is intentionally not bindable here.
        GLint surfaceWetnessReceiver = -1;
        GLint gpuDynamicSurfaceAuthorityActive = -1;
        GLint gpuWaterAtlas = -1;
        GLint gpuFarWaterAtlas = -1;
        GLint gpuFarTileTags = -1;
        GLint gpuSnowAtlas = -1;
        GLint gpuMudAtlas = -1;
        GLint gpuTireMarkAtlas = -1;
        GLint gpuTileIndirection = -1;
        GLint gpuDynamicSurfaceCenterOriginRelativeXZ = -1;
        GLint gpuDynamicSurfaceCenterWorldTile = -1;
        GLint gpuDynamicSurfaceTileMapCenter = -1;
        GLint gpuDynamicSurfaceTileResolution = -1;
        GLint gpuDynamicSurfaceAtlasColumns = -1;
        GLint gpuFarTileResolution = -1;
        GLint gpuFarAtlasTilesPerAxis = -1;
        GLint gpuDynamicSurfaceSnowReady = -1;
        GLint gpuDynamicSurfaceMudReady = -1;
        GLint gpuDynamicSurfaceTireMarksReady = -1;
        GLint surfaceWetnessBreakupMask = -1;
        GLint hasSurfaceWetnessBreakupMask = -1;
        GLint surfacePatternCameraModuloXZ = -1;
        GLint surfacePresentationTime = -1;
        GLint surfaceWeatherFilmWetness = -1;
        GLint prebakedWaterExposureM = -1;
        GLint rainWettingExposureM = -1;
        GLint rainRateMmPerHour = -1;
        GLint hasEnvironmentMap = -1;
        GLint environmentMaxLod = -1;
        GLint model = -1;
        GLint useSkinning = -1;
        GLint jointMatrices = -1;
        GLint tireVisualEnabled = -1;
        GLint tireVisualCenter = -1;
        GLint tireVisualAxleAxis = -1;
        GLint tireVisualHalfWidth = -1;
        GLint tireVisualInnerRadius = -1;
        GLint tireVisualOuterRadius = -1;
        GLint tireReferenceRadiusM = -1;
        GLint tireWheelForwardWorld = -1;
        GLint tireWheelRightWorld = -1;
        GLint tireWheelUpWorld = -1;
        GLint tireVisualDeformationFieldValid = -1;
        GLint tireVisualDisplacementM = -1;
        GLint tireFailureStage = -1;
        GLint tireFailureTreadAttachment = -1;
        GLint tireFailureStructuralIntegrity = -1;
        GLint tireFailureEventSeed = -1;
        GLint tireFailureEventAgeSeconds = -1;
        GLint tireFailureWheelAngularVelocity = -1;
        GLint tireFailureWheelRotationRadians = -1;
        GLint tireFailureRenderPass = -1;
        GLint tireProbeDebugVisible = -1;
        GLint materialBaseColor = -1;
        GLint materialSpecularColor = -1;
        GLint materialEmissiveColor = -1;
        GLint materialRoughness = -1;
        GLint materialMetallic = -1;
        GLint materialSpecularFactor = -1;
        GLint materialOpacity = -1;
        GLint roughnessChannel = -1;
        GLint metallicChannel = -1;
        GLint ambientOcclusionChannel = -1;
        GLint opacityChannel = -1;
        GLint specularFactorChannel = -1;
        GLint useVertexColor = -1;
        GLint tint = -1;
        GLint hasBaseColorMap = -1;
        GLint hasNormalMap = -1;
        GLint hasRoughnessMap = -1;
        GLint hasMetallicMap = -1;
        GLint hasSpecularMap = -1;
        GLint hasAmbientOcclusionMap = -1;
        GLint hasEmissiveMap = -1;
        GLint hasOpacityMap = -1;
        GLint hasSpecularFactorMap = -1;
    };

    struct ShadowUniformLocations
    {
        GLint model = -1;
        GLint lightViewProjection = -1;
        GLint cascadeMask = -1;
        GLint useSkinning = -1;
        GLint jointMatrices = -1;
        GLint tireVisualEnabled = -1;
        GLint tireVisualCenter = -1;
        GLint tireVisualAxleAxis = -1;
        GLint tireVisualHalfWidth = -1;
        GLint tireVisualInnerRadius = -1;
        GLint tireVisualOuterRadius = -1;
        GLint tireReferenceRadiusM = -1;
        GLint tireWheelForwardWorld = -1;
        GLint tireWheelRightWorld = -1;
        GLint tireWheelUpWorld = -1;
        GLint tireVisualDeformationFieldValid = -1;
        GLint tireVisualDisplacementM = -1;
    };

    struct AnimationRuntimeState
    {
        bool initialized = false;
        std::uint64_t playSerial = 0;
        std::uint64_t seekSerial = 0;
        std::string activeClip;
        std::string previousClip;
        double activeTimeSeconds = 0.0;
        double previousTimeSeconds = 0.0;
        double lastEngineTimeSeconds = 0.0;
        float blendDurationSeconds = 0.0f;
        float blendElapsedSeconds = 0.0f;
    };

    // OPT04C: one camera-relative mesh/node preparation is shared by the
    // layered shadow pass and the normal material pass. Before this cache, the
    // renderer acquired each asset and evaluated animation/node/tire overrides
    // twice in immediate succession whenever sun shadows were active.
    struct PreparedFrameInstance
    {
        const heritage::entities::MeshInstance* instance = nullptr;
        const Mesh* mesh = nullptr;
        heritage::math::Mat4 instanceModel = heritage::math::identity();
        std::vector<heritage::math::Mat4> nodeGlobals;
        std::vector<const heritage::entities::MeshNodeOverride*> tireVisualOverrides;
        double prepareCpuMs = 0.0;
    };

    bool resolveAsset(
        const std::string& relativePath,
        std::filesystem::path& resolved,
        std::string& error) const;
    bool resolveMaterialTexture(
        const std::filesystem::path& requested,
        std::filesystem::path& resolved,
        std::string& error);
    const Mesh* acquireMesh(
        const std::string& relativePath,
        bool normalize,
        bool blenderCoordinates);
    bool dependenciesChanged(const CachedAsset& asset) const;
    void rememberDependencies(CachedAsset& asset);
    void reportMaterialWarning(const std::string& warning);
    bool updateRegionalWeatherMap(
        const heritage::physics::SurfaceWorld* surfaceWorld,
        const heritage::math::DVec3& cameraGlobal,
        float elapsedSeconds);
    void shutdownRegionalWeatherMap();
    EnvironmentLighting weatherAdjustedLighting(
        EnvironmentLighting lighting,
        const heritage::physics::SurfaceWorld* surfaceWorld,
        const heritage::math::DVec3& cameraGlobal) const;
    void bindRegionalWeatherMaterialState(
        const heritage::physics::SurfaceWorld* surfaceWorld,
        const heritage::math::DVec3& cameraGlobalForSurface,
        const heritage::physics::weather::RegionalWeatherSample& cameraRegionalWeather,
        bool regionalWeatherMapReady,
        const EnvironmentLighting& lighting);
    bool initializeSurfaceWetnessMaterialBindings();
    void bindSurfaceWetnessMaterialState(
        const heritage::math::DVec3& cameraGlobal,
        float elapsedSeconds);
    void initializeDynamicSurfaceGpuRuntime();
    void shutdownDynamicSurfaceGpuRuntime();
    void updateDynamicSurfaceGpuRuntime(
        const heritage::physics::SurfaceWorld* surfaceWorld,
        const heritage::math::DVec3& cameraGlobal,
        float elapsedSeconds);

    bool initializeSurfaceWetnessResources();
    void shutdownSurfaceWetnessResources();
    std::vector<heritage::math::Mat4> animationTransformsForInstance(
        const Mesh& mesh,
        const heritage::entities::MeshInstance& instance,
        double elapsedSeconds);
    void prepareFrameInstances(
        const std::vector<heritage::entities::MeshInstance>& instances,
        const heritage::math::Vec3& eye,
        float elapsedSeconds);
    bool initializeShadowResources();
    bool synchronizeShadowSettings(const heritage::settings::VideoSettings& videoSettings);
    void shutdownShadowResources();
    bool buildShadowCascades(
        const heritage::math::Mat4& projection,
        const heritage::math::Mat4& view,
        const heritage::math::Vec3& sunDirection);
    void drawShadowMaps(
        const std::vector<PreparedFrameInstance>& preparedInstances,
        const EntityMeshRenderTargetState& renderTargetState);

    std::filesystem::path m_assetRoot;
    std::unordered_map<std::string, CachedAsset> m_cache;
    std::unordered_map<std::string, ResolvedTexturePath> m_resolvedTexturePaths;
    std::unordered_set<std::string> m_reportedMaterialWarnings;
    std::unordered_set<std::string> m_reportedAnimationWarnings;
    // TIRE24/VIS16 retained one-shot live-path diagnostics. These one-shot sets make the
    // console prove whether the player's tire draw and collider bridge reached
    // this renderer without spamming every frame.
    std::unordered_set<std::string> m_reportedTireVisualProofNodes;
    std::unordered_set<std::string> m_reportedTireColliderProofNodes;
    std::unordered_map<heritage::entities::EntityHandle, AnimationRuntimeState> m_animationStates;
    Texture2DCache m_textureCache;
    heritage::graphics::dynamicsurface::DynamicSurfaceGpuRuntime
        m_dynamicSurfaceGpuRuntime;
    EnvironmentSystem* m_environmentSystem = nullptr;
    EnvironmentMap m_environmentMap;
    SkyRenderer m_skyRenderer;
    GLuint m_regionalWeatherTexture = 0;
    int m_regionalWeatherResolution = 512;
    double m_regionalWeatherCenterX = 0.0;
    double m_regionalWeatherCenterZ = 0.0;
    double m_regionalWeatherHalfRangeM = 1000000.0;
    double m_regionalWeatherLastUpdateSeconds = -1.0;
    double m_regionalWeatherFieldElapsedAtUpload = 0.0;
    double m_regionalWeatherAuthoredRainMmPerHour = -1.0;
    double m_regionalWeatherAuthoredHumidity = -1.0;
    double m_regionalWeatherAuthoredCloudCover = -1.0;
    double m_regionalWeatherAuthoredWindSpeedMps = -1.0;
    double m_regionalWeatherAuthoredWindDirectionDeg = -1.0;
    std::vector<std::uint8_t> m_regionalWeatherPixels;
    GLuint m_program = 0;
    UniformLocations m_uniforms{};
    // Artist-authored shoreline breakup remains optical relief only. Water
    // topology/indirection belongs solely to DynamicSurfaceGpuRuntime.
    GLuint m_surfaceWetnessBreakupTexture = 0;
    float m_surfaceWeatherFilmWetness = 0.0f;

    GLuint m_shadowProgram = 0;
    GLuint m_shadowFramebuffer = 0;
    GLuint m_shadowTextureArray = 0;
    GLuint m_shadowCompareSampler = 0;
    GLuint m_shadowRawSampler = 0;
    ShadowUniformLocations m_shadowUniforms{};
    std::array<heritage::math::Mat4, 4> m_shadowMatrices{};
    std::array<float, 4> m_shadowSplits{ 20.0f, 70.0f, 220.0f, 800.0f };
    bool m_shadowResourcesValid = false;
    bool m_shadowsActive = false;
    bool m_tireProbeDebugVisible = false;
    int m_shadowResolution = 0;
    int m_shadowFilterIndex = 2;
    int m_shadowMaximumTextureSize = 0;
    static constexpr std::size_t kShadowGpuTimerRingSize = 4;
    std::array<GLuint, kShadowGpuTimerRingSize> m_shadowGpuTimerStartQueries{};
    std::array<GLuint, kShadowGpuTimerRingSize> m_shadowGpuTimerEndQueries{};
    std::array<bool, kShadowGpuTimerRingSize> m_shadowGpuTimerPending{};
    std::size_t m_shadowGpuTimerWriteIndex = 0;
    double m_shadowLastGpuMs = 0.0;
    std::uint64_t m_hotReloadEpoch = 1;
    std::string m_lastError;
    EntityMeshRendererStats m_frameStats{};
    std::vector<heritage::entities::MeshInstance> m_instanceScratch;
    std::vector<PreparedFrameInstance> m_preparedFrameInstanceScratch;
    // Reused every render frame so the 1000Hz->GPU contact handoff does not
    // repeatedly discard and rebuild vector capacity under large vehicle fields.
    std::vector<heritage::physics::GpuDynamicSurfaceTireEvent>
        m_dynamicSurfacePhysicsTireEventScratch;
    std::vector<heritage::graphics::dynamicsurface::DynamicSurfaceGpuTireContactEvent>
        m_dynamicSurfaceGpuTireEventScratch;
    std::vector<heritage::physics::GpuDynamicSurfaceWaterSampleRequest>
        m_dynamicSurfacePhysicsWaterSampleRequestScratch;
    std::vector<heritage::graphics::dynamicsurface::DynamicSurfaceGpuTireWaterSampleRequest>
        m_dynamicSurfaceGpuWaterSampleRequestScratch;
    std::vector<heritage::graphics::dynamicsurface::DynamicSurfaceGpuTireWaterSample>
        m_dynamicSurfaceGpuWaterSampleResultScratch;
    std::vector<heritage::physics::GpuDynamicSurfaceWaterSample>
        m_dynamicSurfacePhysicsWaterSampleResultScratch;
};

} // namespace heritage::graphics

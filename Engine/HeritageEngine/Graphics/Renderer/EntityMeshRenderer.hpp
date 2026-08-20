#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
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
#include "../DynamicSurface/DynamicSurfaceGpuPagePool.hpp"
#include "../DynamicSurface/DynamicSurfaceGpuLodPrototype.hpp"
#include "SkyRenderer.hpp"
#include "../Mesh.hpp"
#include "../Texture2D.hpp"
#include "../../Physics/Surfaces/Water/SurfaceHydrology.hpp"
#include "../../Physics/Surfaces/SurfaceWorld.hpp"

namespace heritage::graphics {

// Visual-only flexible-ring mesh deformation LOD. Tire simulation itself is
// never distance gated.
inline constexpr float kTireVisualDeformationMaximumDistanceM = 50.0f;

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
    double environmentUpdateMs = 0.0;
    double skyDrawMs = 0.0;
    double meshInstancesCpuMs = 0.0;
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

    // DSURF02 persistent software-virtual-texture diagnostics. This pool is
    // camera independent; resident pages are mirrored into GPU texture arrays.
    double dynamicSurfacePageSyncCpuMs = 0.0;
    std::uint64_t dynamicSurfaceResidentPages = 0;
    std::uint64_t dynamicSurfaceDirtyPages = 0;
    std::uint64_t dynamicSurfaceCapacityPages = 0;
    std::uint64_t dynamicSurfacePageTableGeneration = 0;
    std::uint64_t dynamicSurfacePageTableUploads = 0;
    std::uint64_t dynamicSurfaceInitializedPages = 0;
    std::uint64_t dynamicSurfaceMipRegenerations = 0;
    std::uint64_t dynamicSurfaceSupportPageUploads = 0;
    std::uint64_t dynamicSurfaceHydroPageUploads = 0;
    std::uint64_t dynamicSurfaceTrackPageUploads = 0;
    double dynamicSurfaceStateUploadCpuMs = 0.0;
    double dynamicSurfaceCommittedMiB = 0.0;
    bool dynamicSurfaceGpuReady = false;

    // DSURF04F: centimetre-scale GPU LOD authority. Water/Snow/Mud are
    // simulated in the exact GPU fields sampled by material rendering. F8
    // exposes dispatch, GPU time, VRAM and publication cadence.
    bool dynamicSurfaceGpuLodPrototypeReady = false;
    bool dynamicSurfaceGpuLodWaterReady = false;
    bool dynamicSurfaceGpuLodSnowReady = false;
    bool dynamicSurfaceGpuLodMudReady = false;
    bool dynamicSurfaceGpuWaterPresentationReady = false;
    double dynamicSurfaceGpuWaterPresentationMiB = 0.0;
    std::uint64_t dynamicSurfaceGpuWaterPresentationDispatches = 0;
    double dynamicSurfaceGpuLodCpuMs = 0.0;
    double dynamicSurfaceGpuLodGpuMs = 0.0;
    double dynamicSurfaceGpuLodCommittedMiB = 0.0;
    std::uint64_t dynamicSurfaceGpuLodDispatches = 0;
    std::uint64_t dynamicSurfaceGpuLodCells = 0;
    std::uint64_t dynamicSurfaceGpuLodWaterCycles = 0;
    std::array<std::uint64_t, 4> dynamicSurfaceGpuLodWaterDispatches{};
    std::array<std::uint64_t, 4> dynamicSurfaceGpuLodWaterPublishedCycles{};
    std::array<std::uint32_t, 4> dynamicSurfaceGpuGeometryValidTiles{};
    std::array<std::uint32_t, 4> dynamicSurfaceGpuActiveTiles{};
    std::uint32_t dynamicSurfaceGpuResidentTiles = 0;
    std::uint32_t dynamicSurfaceGpuPrewarmTiles = 0;
    std::uint32_t dynamicSurfaceGpuWorldTiles = 0;
    std::uint64_t dynamicSurfaceGpuWorldTileDispatches = 0;
    double dynamicSurfaceGpuWorldTileStateMiB = 0.0;
    std::uint32_t dynamicSurfaceGpuDueTiles = 0;
    std::uint32_t dynamicSurfaceGpuBacklogTiles = 0;
    double dynamicSurfaceGpuCameraSpeedMps = 0.0;
    double dynamicSurfaceGpuPrewarmDistanceM = 0.0;
    float dynamicSurfaceGpuRainMmPerHour = 0.0f;
    float dynamicSurfaceGpuDrainageMmPerHour = 0.0f;
    float dynamicSurfaceGpuEvaporationMmPerHour = 0.0f;
    float dynamicSurfaceGpuBackgroundSeedDepthM = 0.0f;
    bool dynamicSurfaceGpuWaterProbeValid = false;
    std::uint32_t dynamicSurfaceGpuWaterProbeWetTexels = 0u;
    std::uint32_t dynamicSurfaceGpuWaterProbeTexels = 0u;
    float dynamicSurfaceGpuWaterProbeMeanDepthM = 0.0f;
    float dynamicSurfaceGpuWaterProbeMaximumDepthM = 0.0f;
    std::uint64_t dynamicSurfaceGpuLodTireEventDispatches = 0;
    std::uint64_t dynamicSurfaceGpuLodTireEventCells = 0;
    std::int32_t dynamicSurfaceGpuLodCenterTileX = 0;
    std::int32_t dynamicSurfaceGpuLodCenterTileZ = 0;
    std::uint64_t dynamicSurfaceGpuLodCameraTileRebases = 0;
    bool dynamicSurfaceGpuExactGeometryReady = false;
    std::uint64_t dynamicSurfaceGpuGeometryTriangles = 0;
    std::uint64_t dynamicSurfaceGpuGeometryBinReferences = 0;
    double dynamicSurfaceGpuGeometryUploadMiB = 0.0;

    // SHADOW01: real-time directional cascaded shadow-map diagnostics.
    double shadowCpuMs = 0.0;
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
        bool wireframeVisible = false,
        const heritage::physics::SurfaceWorld* surfaceWorld = nullptr);

    std::size_t loadedAssetCount() const;
    void beginFrameStats() { m_frameStats = {}; }
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
        // DSURF03: the ordinary scene material samples the persistent
        // Heritage Dynamic Surface page pool. No WATER15-18 clipmap/puddle
        // canvas survives as a parallel presentation authority.
        GLint surfaceWetnessReceiver = -1;
        GLint dynamicSurfaceHydroPages = -1;
        GLint dynamicSurfacePageIndirection = -1;
        GLint dynamicSurfaceIndirectionOriginRelativeXZ = -1;
        GLint dynamicSurfacePageWorldSizeM = -1;
        GLint dynamicSurfaceIndirectionResolution = -1;
        GLint dynamicSurfaceHydroBaseMip = -1;
        GLint dynamicSurfaceCameraGlobalY = -1;
        GLint dynamicSurfaceActive = -1;
        GLint gpuDynamicSurfaceAuthorityActive = -1;
        GLint gpuWaterAtlas = -1;
        GLint gpuWaterPresentationAtlas = -1;
        GLint gpuWaterPresentationReady = -1;
        GLint gpuSnowAtlas = -1;
        GLint gpuMudAtlas = -1;
        GLint gpuTileIndirection = -1;
        GLint gpuDynamicSurfaceCenterOriginRelativeXZ = -1;
        GLint gpuDynamicSurfaceTileMapCenter = -1;
        GLint gpuDynamicSurfaceTileResolution = -1;
        GLint gpuDynamicSurfaceAtlasColumns = -1;
        GLint gpuDynamicSurfaceSnowReady = -1;
        GLint gpuDynamicSurfaceMudReady = -1;
        GLint surfaceWetnessBreakupMask = -1;
        GLint hasSurfaceWetnessBreakupMask = -1;
        GLint surfacePatternCameraModuloXZ = -1;
        GLint surfacePresentationTime = -1;
        GLint surfaceWeatherFilmWetness = -1;
        GLint surfaceWeatherFilmDepthM = -1;
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
    void updateDynamicSurfaceStatePages(
        const heritage::physics::SurfaceWorld* surfaceWorld,
        const heritage::math::DVec3& cameraGlobal,
        float elapsedSeconds);
    bool initializeSurfaceWetnessMaterialBindings();
    void bindSurfaceWetnessMaterialState(
        const heritage::math::DVec3& cameraGlobal,
        float elapsedSeconds);
    bool initializeDynamicSurfacePageResources();
    void shutdownDynamicSurfacePageResources();
    void synchronizeDynamicSurfacePageResources(
        const heritage::physics::SurfaceWorld* surfaceWorld);
    void initializeDynamicSurfaceGpuLodPrototype();
    void shutdownDynamicSurfaceGpuLodPrototype();
    void updateDynamicSurfaceGpuLodPrototype(
        const heritage::physics::SurfaceWorld* surfaceWorld,
        const heritage::math::DVec3& cameraGlobal,
        float elapsedSeconds);

    bool initializeSurfaceWetnessResources();
    void shutdownSurfaceWetnessResources();
    std::vector<heritage::math::Mat4> animationTransformsForInstance(
        const Mesh& mesh,
        const heritage::entities::MeshInstance& instance,
        double elapsedSeconds);
    bool initializeShadowResources();
    bool synchronizeShadowSettings(const heritage::settings::VideoSettings& videoSettings);
    void shutdownShadowResources();
    bool buildShadowCascades(
        const heritage::math::Mat4& projection,
        const heritage::math::Mat4& view,
        const heritage::math::Vec3& sunDirection);
    void drawShadowMaps(
        const std::vector<heritage::entities::MeshInstance>& instances,
        const heritage::math::Vec3& eye,
        float elapsedSeconds);

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
    heritage::graphics::dynamicsurface::DynamicSurfaceGpuPagePool
        m_dynamicSurfaceGpuPagePool;
    heritage::graphics::dynamicsurface::DynamicSurfaceGpuLodPrototype
        m_dynamicSurfaceGpuLodPrototype;
    EnvironmentSystem* m_environmentSystem = nullptr;
    EnvironmentMap m_environmentMap;
    SkyRenderer m_skyRenderer;
    GLuint m_program = 0;
    UniformLocations m_uniforms{};
    // DSURF03: the shoreline texture remains artist-authored optical relief,
    // but all spatial water/moisture state now lives in persistent Dynamic
    // Surface pages. The indirection texture maps a camera-local lookup grid to
    // world-anchored physical page slots; it owns no simulation state.
    GLuint m_surfaceWetnessBreakupTexture = 0;
    GLuint m_dynamicSurfacePageIndirectionTexture = 0;
    static constexpr int kDynamicSurfaceIndirectionResolution = 64;
    static constexpr std::uint32_t kDynamicSurfaceHydroBaseMip = 0u; // LIVETRACK03 Hydro: RGBA4 256x256 = 0.390625m/texel.
    heritage::math::DVec3 m_dynamicSurfaceIndirectionOriginGlobal{};
    std::uint64_t m_dynamicSurfaceIndirectionTableGeneration = 0u;
    std::vector<std::int32_t> m_dynamicSurfaceIndirectionScratch;
    std::vector<std::uint16_t> m_dynamicSurfaceHydroScratch;
    std::vector<float> m_dynamicSurfaceTrackScratch;
    std::map<heritage::physics::dynamicsurface::VirtualPageAddress, std::uint64_t>
        m_dynamicSurfaceHydroUploadedStep;
    std::map<heritage::physics::dynamicsurface::VirtualPageAddress, std::uint32_t>
        m_dynamicSurfaceHydroUploadedGeneration;
    std::map<heritage::physics::dynamicsurface::VirtualPageAddress, std::uint64_t>
        m_dynamicSurfaceTrackUploadedStep;
    std::map<heritage::physics::dynamicsurface::VirtualPageAddress, std::uint32_t>
        m_dynamicSurfaceTrackUploadedGeneration;
    std::map<heritage::physics::dynamicsurface::VirtualPageAddress, float>
        m_dynamicSurfaceHydroLastRefreshSeconds;
    std::map<heritage::physics::dynamicsurface::VirtualPageAddress, float>
        m_dynamicSurfaceTrackLastRefreshSeconds;
    std::size_t m_dynamicSurfaceHydroRefreshCursor = 0u;
    float m_surfaceWeatherFilmWetness = 0.0f;
    float m_surfaceWeatherFilmDepthM = 0.0f;

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
    std::uint64_t m_hotReloadEpoch = 1;
    std::string m_lastError;
    EntityMeshRendererStats m_frameStats{};
    std::vector<heritage::entities::MeshInstance> m_instanceScratch;
    // Reused every render frame so the 1000Hz->GPU contact handoff does not
    // repeatedly discard and rebuild vector capacity under large vehicle fields.
    std::vector<heritage::physics::GpuDynamicSurfaceTireEvent>
        m_dynamicSurfacePhysicsTireEventScratch;
    std::vector<heritage::graphics::dynamicsurface::DynamicSurfaceGpuTireContactEvent>
        m_dynamicSurfaceGpuTireEventScratch;
};

} // namespace heritage::graphics

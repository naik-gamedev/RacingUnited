#pragma once

#include <cstdint>
#include <filesystem>
#include <glad/glad.h>

#include "../../Core/Math/Math.hpp"
#include "../EnvironmentMap.hpp"
#include "../Texture2D.hpp"
#include "AsyncGpuTimer.hpp"

namespace heritage::graphics {

struct SkyRenderTargetState
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

struct SkyRendererGpuStats
{
    // OPT00 asynchronous pass timings. Values are from completed earlier
    // frames; reading them never blocks the frame currently being rendered.
    double backgroundMs = 0.0;
    double cloudShadowMs = 0.0;
    double cloudSceneCopyMs = 0.0;
    double cloudRaymarchMs = 0.0;
    double cloudUpscaleMs = 0.0;
    double cloudTemporalMs = 0.0;
    double cloudPresentMs = 0.0;
};

// PERF01: CPU wall-time attribution for the cloud submission path. These
// timers deliberately wrap ordinary GL calls without adding glFinish/query
// waits. If the driver back-pressures the CPU inside a blit/draw, the wait is
// charged to that exact call so F8 can identify it.
struct SkyRendererCpuStats
{
    // PERF03: background/sky CPU attribution. PERF02 proved that the earlier
    // Dynamic Surface suspicion was false on the measured frame: the ~20 ms
    // render-submit wall time moved into SkyRenderer::draw(). These timings
    // identify whether that cost is asynchronous-query polling, ordinary state
    // setup, the actual sky draw, cloud-shadow submission, or state restore.
    double backgroundTotalMs = 0.0;
    double gpuTimerPollTotalMs = 0.0;
    double gpuTimerPollBackgroundMs = 0.0;
    double gpuTimerPollCloudShadowMs = 0.0;
    double gpuTimerPollSceneCopyMs = 0.0;
    double gpuTimerPollRaymarchMs = 0.0;
    double gpuTimerPollUpscaleMs = 0.0;
    double gpuTimerPollTemporalMs = 0.0;
    double gpuTimerPollPresentMs = 0.0;
    double backgroundTimerBeginMs = 0.0;
    double backgroundStateSetupMs = 0.0;
    double backgroundUniformUploadMs = 0.0;
    double backgroundTextureBindMs = 0.0;
    double backgroundDrawCallMs = 0.0;
    double backgroundTimerEndMs = 0.0;
    double cloudShadowUpdateCpuMs = 0.0;
    // PERF04: attribution inside updateCloudShadows(). These timings wrap the
    // existing CPU/GL calls only; they do not add synchronization. The goal is
    // to identify the exact owner of the ~20 ms CPU wall time seen in PERF03.
    double cloudShadowInternalTotalMs = 0.0;
    double cloudShadowEligibilityMs = 0.0;
    double cloudShadowTargetEnsureMs = 0.0;
    double cloudShadowTimerBeginMs = 0.0;
    double cloudShadowStateSetupMs = 0.0;
    double cloudShadowRawAttachmentMs = 0.0;
    double cloudShadowProgramBindMs = 0.0;
    double cloudShadowUniformUploadMs = 0.0;
    double cloudShadowTextureBindMs = 0.0;
    double cloudShadowRawDrawCallMs = 0.0;
    double cloudShadowFilterSetupMs = 0.0;
    double cloudShadowFilterAttachmentMs = 0.0;
    double cloudShadowFilterTextureBindMs = 0.0;
    double cloudShadowFilterDrawCallMs = 0.0;
    double cloudShadowCopyImageMs = 0.0;
    double cloudShadowFinalizeMs = 0.0;
    double cloudShadowTimerEndMs = 0.0;
    double cloudShadowResidualMs = 0.0;
    double backgroundRestoreMs = 0.0;

    double cloudAfterOpaqueTotalMs = 0.0;
    double cloudTargetEnsureMs = 0.0;
    double cloudSceneCopyMs = 0.0;
    double cloudSceneColorBlitMs = 0.0;
    double cloudSceneDepthBlitMs = 0.0;
    double cloudRaymarchMs = 0.0;
    double cloudRaymarchDrawCallMs = 0.0;
    double cloudUpscaleMs = 0.0;
    double cloudUpscaleDrawCallMs = 0.0;
    double cloudTemporalMs = 0.0;
    double cloudTemporalDrawCallMs = 0.0;
    double cloudPresentMs = 0.0;
    double cloudPresentDrawCallMs = 0.0;
    double cloudDepthMergeDrawCallMs = 0.0;
    double cloudRestoreMs = 0.0;
};

struct SkyWeatherParameters
{
    bool enabled = false;
    // Camera-local regional coverage used by atmospheric colour/lighting.
    float cloudCover = 0.0f;
    // Scene-authored 0..1 coverage slider. CLOUDURP15C keeps this separate so
    // 100% can mean true overcast instead of merely sampling a cloudy FBM cell.
    float authoredCloudCover = 0.0f;
    float relativeHumidity = 0.55f;
    float precipitationRateMmPerHour = 0.0f;
    float windVelocityXMps = 0.0f;
    float windVelocityZMps = 0.0f;
    float cloudBaseWindVelocityXMps = 0.0f;
    float cloudBaseWindVelocityZMps = 0.0f;
    float cloudTopWindVelocityXMps = 0.0f;
    float cloudTopWindVelocityZMps = 0.0f;
    float elapsedSeconds = 0.0f;
    heritage::math::DVec3 cameraGlobal{ 0.0, 0.0, 0.0 };
    GLuint regionalWeatherTexture = 0;
    float regionalWeatherCameraOffsetX = 0.0f;
    float regionalWeatherCameraOffsetZ = 0.0f;
    float regionalWeatherAdvectionOffsetX = 0.0f;
    float regionalWeatherAdvectionOffsetZ = 0.0f;
    float regionalWeatherHalfRangeM = 0.0f;

    // UnityVolumetricCloudsURP feature equivalents. These are renderer
    // switches, not a second Heritage weather model.
    // CLOUDURP15L: local/world-space cloud semantics are now the default.
    // This keeps the camera's true FP64-derived altitude in the cloud-shell
    // intersection so detached free flight can enter and pass through the
    // 1.2-3.2 km volume instead of the sky remaining pinned to sea level.
    bool localVolumetricClouds = true;
    bool microErosion = false;
    bool physicallyBasedSun = true;
    bool bilateralUpscale = false;
    bool perceptualBlending = true;
    bool outputCloudDepth = true;
    bool outputCloudDepthToScene = false;
    bool cloudShadows = true;
};

class SkyRenderer
{
public:
    bool initialize(const std::filesystem::path& moduleAssetRoot);
    void shutdown();

    // Background sky only. The translated URP cloud renderer executes after
    // opaque geometry, matching RenderPassEvent.BeforeRenderingTransparents.
    void draw(
        const heritage::math::Mat4& view,
        const heritage::math::Mat4& projection,
        const EnvironmentMap& environmentMap,
        const EnvironmentLighting& lighting,
        const SkyWeatherParameters& weather,
        const SkyRenderTargetState& targetState,
        float gamma,
        float brightness,
        float contrast,
        float saturation);

    void drawVolumetricCloudsAfterOpaque(
        const heritage::math::Mat4& view,
        const heritage::math::Mat4& projection,
        const EnvironmentLighting& lighting,
        const SkyWeatherParameters& weather,
        const SkyRenderTargetState& targetState);

    bool valid() const { return m_program != 0 && m_vao != 0; }
    bool volumetricCloudsValid() const;

    GLuint cloudShadowTexture() const { return m_cloudShadowTexture; }
    bool cloudShadowValid() const { return m_cloudShadowReady && m_cloudShadowTexture != 0; }
    float cloudShadowHalfRangeM() const { return m_cloudShadowHalfRangeM; }
    GLuint cloudDepthTexture() const { return m_cloudRaymarchDepthTexture; }
    bool cloudDepthValid() const { return m_cloudRaymarchDepthTexture != 0; }
    const SkyRendererGpuStats& gpuStats() const { return m_gpuStats; }
    const SkyRendererCpuStats& cpuStats() const { return m_cpuStats; }

private:
    bool loadCloudVolumeTexture(const std::filesystem::path& path, GLuint& texture,
        int expectedWidth, int expectedHeight, int expectedDepth);
    bool ensureCloudLut();
    bool ensureCloudTargets(int viewportWidth, int viewportHeight, int sceneDepthSamples);
    bool ensureCloudShadowTargets();
    void destroyCloudTargets();
    void destroyCloudShadowTargets();
    void resetCloudHistory();
    void updateCloudShadows(const EnvironmentLighting& lighting, const SkyWeatherParameters& weather);
    void pollGpuTimers();
    bool initializePhysicallyBasedAtmosphere();
    void shutdownPhysicallyBasedAtmosphere();
    bool ensurePhysicallyBasedAtmosphereTargets();
    void updatePhysicallyBasedAtmosphereLuts(
        const EnvironmentLighting& lighting,
        const SkyWeatherParameters& weather);

    std::filesystem::path m_assetRoot;

    GLuint m_program = 0;

    // PBSKY01: Heritage-native OpenGL translation of jiaozi158's MIT-licensed
    // UnityPhysicallyBasedSkyURP architecture.  The physical atmosphere owns
    // immutable Earth coefficients plus a transmittance LUT, multiple-scattering
    // LUT and a sun-dependent sky-view LUT.  Clouds remain the existing renderer
    // until VCLOUD01 replaces them in a separately testable milestone.
    GLuint m_pbrAtmosphereTransmittanceProgram = 0;
    GLuint m_pbrAtmosphereMultiScatteringProgram = 0;
    GLuint m_pbrAtmosphereSkyViewProgram = 0;
    GLuint m_pbrAtmosphereFbo = 0;
    GLuint m_pbrTransmittanceTexture = 0;
    GLuint m_pbrMultiScatteringTexture = 0;
    GLuint m_pbrSkyViewTexture = 0;
    bool m_pbrAtmosphereProgramsLinked = false;
    bool m_pbrAtmosphereReady = false;
    float m_pbrLastAerosolMultiplier = -1.0f;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;

    GLuint m_cloudRaymarchProgram = 0;
    GLuint m_cloudCombineProgram = 0;
    GLuint m_cloudTemporalProgram = 0;
    GLuint m_cloudPresentProgram = 0;
    GLuint m_cloudGroundShadowProgram = 0;
    GLuint m_cloudDepthMergeProgram = 0;
    GLuint m_cloudShadowProgram = 0;
    GLuint m_cloudShadowFilterProgram = 0;

    // PERF05: program link state is immutable after initialization. Cache it
    // once instead of issuing synchronous glGetProgramiv(GL_LINK_STATUS)
    // queries from per-frame cloud eligibility/depth-merge paths.
    bool m_volumetricCloudProgramsLinked = false;
    bool m_cloudDepthMergeProgramLinked = false;
    bool m_cloudShadowProgramsLinked = false;

    GLuint m_cloudShapeTexture = 0;
    GLuint m_cloudErosionTexture = 0;
    GLuint m_cloudLutTexture = 0;
    GLuint m_environmentMapTexture = 0;
    Texture2DCache m_textureCache;
    GLuint m_moonTexture = 0;
    GLuint m_starMapTexture = 0;

    GLuint m_cloudRaymarchFbo = 0, m_cloudRaymarchTexture = 0;
    GLuint m_cloudRaymarchDepthTexture = 0;
    GLuint m_cloudSceneFbo = 0, m_cloudSceneTexture = 0;
    GLuint m_cloudSceneDepthFbo = 0, m_cloudSceneDepthTexture = 0;
    GLenum m_cloudSceneDepthTarget = GL_TEXTURE_2D;
    int m_cloudSceneDepthSamples = 1;
    GLuint m_cloudCombinedFbo = 0, m_cloudCombinedTexture = 0;
    // OPT05: temporal history is true ping-pong ownership. Each texture keeps its
    // own framebuffer so a completed temporal frame becomes history by swapping
    // handles rather than copying a full-resolution RGBA16F image every frame.
    GLuint m_cloudTemporalFbo = 0, m_cloudTemporalTexture = 0;
    GLuint m_cloudHistoryFbo = 0, m_cloudHistoryTexture = 0;
    GLuint m_cloudHistorySampler = 0;
    int m_cloudHalfWidth = 0, m_cloudHalfHeight = 0;
    int m_cloudFullWidth = 0, m_cloudFullHeight = 0;
    bool m_cloudHistoryValid = false;

    GLuint m_cloudShadowFbo = 0;
    GLuint m_cloudShadowRawTexture = 0;
    GLuint m_cloudShadowTempTexture = 0;
    GLuint m_cloudShadowTexture = 0;
    int m_cloudShadowResolution = 256;
    float m_cloudShadowHalfRangeM = 8000.0f;
    bool m_cloudShadowReady = false;

    SkyRendererGpuStats m_gpuStats{};
    SkyRendererCpuStats m_cpuStats{};
    AsyncGpuTimer m_backgroundGpuTimer;
    AsyncGpuTimer m_cloudShadowGpuTimer;
    AsyncGpuTimer m_cloudSceneCopyGpuTimer;
    AsyncGpuTimer m_cloudRaymarchGpuTimer;
    AsyncGpuTimer m_cloudUpscaleGpuTimer;
    AsyncGpuTimer m_cloudTemporalGpuTimer;
    AsyncGpuTimer m_cloudPresentGpuTimer;

    heritage::math::Mat4 m_previousCloudView = heritage::math::identity();
    heritage::math::Mat4 m_previousCloudProjection = heritage::math::identity();
    heritage::math::DVec3 m_previousCloudCameraGlobal{ 0.0, 0.0, 0.0 };
    std::uint32_t m_cloudTemporalFrameIndex = 0;

    struct RayUniforms {
        GLint view=-1, projection=-1, cameraGlobalXZ=-1, sunDirection=-1, sunColor=-1;
        GLint sunIntensity=-1, moonDirection=-1, moonColor=-1, moonIntensity=-1;
        GLint skyHorizon=-1, skyZenith=-1;
        GLint time=-1, temporalFrameIndex=-1, cloudCover=-1, humidity=-1, precipitation=-1;
        GLint windVelocityXZ=-1, baseWindXZ=-1, topWindXZ=-1;
        GLint regionalMap=-1, regionalMapValid=-1, regionalCameraOffsetXZ=-1;
        GLint regionalAdvectionXZ=-1, regionalHalfRange=-1;
        GLint shapeNoise=-1, erosionNoise=-1, curveLut=-1, environmentMap=-1;
        GLint pbrTransmittance=-1, pbrAtmosphereValid=-1;
        GLint microErosion=-1, physicallyBasedSun=-1, localClouds=-1;
        GLint sceneDepth=-1, sceneDepthMs=-1, sceneDepthSamples=-1;
        GLint sceneColor=-1, perceptual=-1;
    } m_ray;
    struct CombineUniforms { GLint cloud=-1, scene=-1, bilateral=-1; } m_combine;
    struct TemporalUniforms {
        GLint current=-1, history=-1, historyValid=-1;
        GLint currentView=-1, previousView=-1, currentProjection=-1, previousProjection=-1;
        GLint cameraDelta=-1, sceneDepth=-1, sceneDepthMs=-1, sceneDepthSamples=-1, localClouds=-1;
    } m_temporal;
    struct GroundShadowUniforms {
        GLint sceneDepth=-1, sceneDepthMs=-1, sceneDepthSamples=-1;
        GLint cloudShadow=-1, cloudShadowValid=-1, cloudShadowHalfRangeM=-1;
        GLint view=-1, projection=-1, celestialLightDirection=-1, daylightFactor=-1;
    } m_groundShadow;
    struct PresentUniforms {
        GLint source=-1;
        GLint sceneDepth=-1, sceneDepthMs=-1, sceneDepthSamples=-1;
        GLint sunScreenUv=-1, sunColor=-1, sunIntensity=-1, sunElevation=-1, sunScreenVisible=-1;
        GLint cloudCover=-1, humidity=-1, precipitation=-1;
    } m_present;
    struct DepthMergeUniforms {
        GLint sceneDepth=-1, sceneDepthMs=-1, sceneDepthSamples=-1, cloudDepth=-1;
    } m_depthMerge;
    struct ShadowUniforms {
        GLint cameraGlobalXZ=-1, cameraGlobalY=-1, sunDirection=-1, time=-1;
        GLint cloudCover=-1, humidity=-1, precipitation=-1;
        GLint baseWindXZ=-1, topWindXZ=-1, regionalMap=-1, regionalMapValid=-1;
        GLint regionalCameraOffsetXZ=-1, regionalAdvectionXZ=-1, regionalHalfRange=-1;
        GLint shapeNoise=-1, erosionNoise=-1, curveLut=-1, microErosion=-1, halfRange=-1;
    } m_shadow;
    struct ShadowFilterUniforms { GLint source=-1, texelSize=-1; } m_shadowFilter;

    GLint m_uniformView=-1, m_uniformProjection=-1, m_uniformGamma=-1, m_uniformBrightness=-1;
    GLint m_uniformContrast=-1, m_uniformSaturation=-1, m_uniformSunDirection=-1;
    GLint m_uniformSunColor=-1, m_uniformSunIntensity=-1, m_uniformDaylightFactor=-1;
    GLint m_uniformSkyHorizon=-1, m_uniformSkyZenith=-1, m_uniformSkyExposure=-1;
    GLint m_uniformAtmosphereThickness=-1, m_uniformStarIntensity=-1;
    GLint m_uniformWorldToCelestialRow0=-1, m_uniformWorldToCelestialRow1=-1, m_uniformWorldToCelestialRow2=-1;
    GLint m_uniformMoonDirection=-1, m_uniformMoonIntensity=-1, m_uniformMoonPhase=-1;
    GLint m_uniformWeatherCloudCover=-1, m_uniformWeatherHumidity=-1, m_uniformWeatherPrecipitation=-1;
    GLint m_uniformPbrSkyView=-1, m_uniformPbrSkyValid=-1, m_uniformPbrCameraAltitude=-1;

    struct PbrTransmittanceUniforms { GLint aerosolMultiplier=-1; } m_pbrTransmittance;
    struct PbrMultiScatteringUniforms { GLint transmittance=-1, aerosolMultiplier=-1; } m_pbrMultiScattering;
    struct PbrSkyViewUniforms {
        GLint transmittance=-1, multiScattering=-1;
        GLint sunDirection=-1, sunColor=-1, sunIntensity=-1;
        GLint cameraAltitude=-1, aerosolMultiplier=-1;
    } m_pbrSkyView;
};

} // namespace heritage::graphics

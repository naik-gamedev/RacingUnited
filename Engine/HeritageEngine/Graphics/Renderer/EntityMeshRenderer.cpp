#include "EntityMeshRenderer.hpp"
#include "EntityMeshRendererInternal.hpp"
#include "EntityMeshShadowConfig.hpp"
#include "EntityMeshShaders.hpp"
#include "../ShaderProgram.hpp"
#include "../../Physics/Surfaces/SurfaceWorld.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <utility>

namespace heritage::graphics {
using namespace entity_mesh_internal;
using namespace entity_mesh_shadow_config;
using namespace entity_mesh_shaders;
namespace {
constexpr const char* kTextureMapFoundationMarker =
    "HERITAGE_GFX2_STATIC_MESH_IMPORT HERITAGE_GFX3_GLB_SKINNING_ANIMATION HERITAGE_GFX4_ANIMATION_CONTROL HERITAGE_GFX5_GLB_SPECULAR_VERTEX_COLOR HERITAGE_GFX6_ENVIRONMENT_IBL HERITAGE_GFX7_SKY_DAY_NIGHT HERITAGE_GFX9_CASCADED_SUN_SHADOWS HERITAGE_VA02_GLB_NODE_BINDING HERITAGE_SC01_GLB_SCENE_COLLISION";

}

bool EntityMeshRenderer::initialize(
    const std::filesystem::path& moduleAssetRoot,
    EnvironmentSystem* environmentSystem)
{
    shutdown();

    m_environmentSystem = environmentSystem;

    std::error_code error;
    m_assetRoot =
        std::filesystem::weakly_canonical(moduleAssetRoot, error);
    if (error)
    {
        m_assetRoot =
            std::filesystem::absolute(moduleAssetRoot).lexically_normal();
    }

    m_program = buildShaderProgram(kVertexShader, kFragmentShader);
    if (!m_program)
    {
        m_lastError =
            "EntityMeshRenderer could not compile its material shader.";
        return false;
    }
    GLint materialLinked = GL_FALSE;
    glGetProgramiv(m_program, GL_LINK_STATUS, &materialLinked);
    if (materialLinked == GL_FALSE)
    {
        GLint logLength = 0;
        glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<std::size_t>(std::max(logLength, 1)), '\0');
        glGetProgramInfoLog(m_program, logLength, nullptr, log.data());
        m_lastError = "EntityMeshRenderer material shader link failed: " + log;
        std::cerr << m_lastError << '\n';
        glDeleteProgram(m_program);
        m_program = 0;
        return false;
    }

    glUseProgram(m_program);

    // Resolve uniform locations exactly once. glGetUniformLocation is a driver
    // query, and the old renderer repeated it thousands of times per frame
    // (for every material primitive). That cost showed up directly as the
    // 60+ ms "Render submit" CPU time in PERF01.
    auto uniform = [&](const char* name)
    {
        return glGetUniformLocation(m_program, name);
    };
    m_uniforms.baseColorMap = uniform("uBaseColorMap");
    m_uniforms.normalMap = uniform("uNormalMap");
    m_uniforms.roughnessMap = uniform("uRoughnessMap");
    m_uniforms.metallicMap = uniform("uMetallicMap");
    m_uniforms.specularMap = uniform("uSpecularMap");
    m_uniforms.ambientOcclusionMap = uniform("uAmbientOcclusionMap");
    m_uniforms.emissiveMap = uniform("uEmissiveMap");
    m_uniforms.opacityMap = uniform("uOpacityMap");
    m_uniforms.specularFactorMap = uniform("uSpecularFactorMap");
    m_uniforms.environmentMap = uniform("uEnvironmentMap");
    m_uniforms.shadowMap = uniform("uShadowMap");
    m_uniforms.shadowDepthMap = uniform("uShadowDepthMap");
    m_uniforms.shadowFilterMode = uniform("uShadowFilterMode");
    m_uniforms.shadowMatrices = uniform("uShadowMatrices[0]");
    m_uniforms.shadowSplits = uniform("uShadowSplits");
    m_uniforms.hasShadowMap = uniform("uHasShadowMap");
    m_uniforms.shadowStrength = uniform("uShadowStrength");
    m_uniforms.view = uniform("uView");
    m_uniforms.projection = uniform("uProjection");
    m_uniforms.eye = uniform("uEye");
    m_uniforms.sunDirection = uniform("uSunDirection");
    m_uniforms.sunRadiance = uniform("uSunRadiance");
    m_uniforms.gamma = uniform("uGamma");
    m_uniforms.brightness = uniform("uBrightness");
    m_uniforms.contrast = uniform("uContrast");
    m_uniforms.saturation = uniform("uSaturation");
    m_uniforms.weatherFogDensity = uniform("uWeatherFogDensity");
    m_uniforms.weatherFogColor = uniform("uWeatherFogColor");
    m_uniforms.regionalWeatherMap = uniform("uRegionalWeatherMap");
    m_uniforms.regionalWeatherMapValid = uniform("uRegionalWeatherMapValid");
    m_uniforms.regionalWeatherCameraOffsetXZ = uniform("uRegionalWeatherCameraOffsetXZ");
    m_uniforms.regionalWeatherAdvectionXZ = uniform("uRegionalWeatherAdvectionXZ");
    m_uniforms.regionalWeatherHalfRangeM = uniform("uRegionalWeatherHalfRangeM");
    m_uniforms.weatherCloudBaseM = uniform("uWeatherCloudBaseM");
    m_uniforms.volumetricCloudShadow = uniform("uVolumetricCloudShadow");
    m_uniforms.hasVolumetricCloudShadow = uniform("uHasVolumetricCloudShadow");
    m_uniforms.volumetricCloudShadowHalfRangeM = uniform("uVolumetricCloudShadowHalfRangeM");
    m_uniforms.hasEnvironmentMap = uniform("uHasEnvironmentMap");
    m_uniforms.environmentMaxLod = uniform("uEnvironmentMaxLod");
    m_uniforms.model = uniform("uModel");
    m_uniforms.useSkinning = uniform("uUseSkinning");
    m_uniforms.jointMatrices = uniform("uJointMatrices");
    m_uniforms.tireVisualEnabled = uniform("uTireVisualEnabled");
    m_uniforms.tireVisualCenter = uniform("uTireVisualCenter");
    m_uniforms.tireVisualAxleAxis = uniform("uTireVisualAxleAxis");
    m_uniforms.tireVisualHalfWidth = uniform("uTireVisualHalfWidth");
    m_uniforms.tireVisualInnerRadius = uniform("uTireVisualInnerRadius");
    m_uniforms.tireVisualOuterRadius = uniform("uTireVisualOuterRadius");
    m_uniforms.tireReferenceRadiusM = uniform("uTireReferenceRadiusM");
    m_uniforms.tireWheelForwardWorld = uniform("uTireWheelForwardWorld");
    m_uniforms.tireWheelRightWorld = uniform("uTireWheelRightWorld");
    m_uniforms.tireWheelUpWorld = uniform("uTireWheelUpWorld");
    m_uniforms.tireVisualDeformationFieldValid =
        uniform("uTireVisualDeformationFieldValid");
    m_uniforms.tireVisualDisplacementM =
        uniform("uTireVisualDisplacementM[0]");
    m_uniforms.tireFailureStage = uniform("uTireFailureStage");
    m_uniforms.tireFailureTreadAttachment =
        uniform("uTireFailureTreadAttachment");
    m_uniforms.tireFailureStructuralIntegrity =
        uniform("uTireFailureStructuralIntegrity");
    m_uniforms.tireFailureEventSeed = uniform("uTireFailureEventSeed");
    m_uniforms.tireFailureEventAgeSeconds =
        uniform("uTireFailureEventAgeSeconds");
    m_uniforms.tireFailureWheelAngularVelocity =
        uniform("uTireFailureWheelAngularVelocity");
    m_uniforms.tireFailureWheelRotationRadians =
        uniform("uTireFailureWheelRotationRadians");
    m_uniforms.tireFailureRenderPass = uniform("uTireFailureRenderPass");
    m_uniforms.tireProbeDebugVisible = uniform("uTireProbeDebugVisible");
    m_uniforms.materialBaseColor = uniform("uMaterialBaseColor");
    m_uniforms.materialSpecularColor = uniform("uMaterialSpecularColor");
    m_uniforms.materialEmissiveColor = uniform("uMaterialEmissiveColor");
    m_uniforms.materialRoughness = uniform("uMaterialRoughness");
    m_uniforms.materialMetallic = uniform("uMaterialMetallic");
    m_uniforms.materialSpecularFactor = uniform("uMaterialSpecularFactor");
    m_uniforms.materialOpacity = uniform("uMaterialOpacity");
    m_uniforms.roughnessChannel = uniform("uRoughnessChannel");
    m_uniforms.metallicChannel = uniform("uMetallicChannel");
    m_uniforms.ambientOcclusionChannel = uniform("uAmbientOcclusionChannel");
    m_uniforms.opacityChannel = uniform("uOpacityChannel");
    m_uniforms.specularFactorChannel = uniform("uSpecularFactorChannel");
    m_uniforms.useVertexColor = uniform("uUseVertexColor");
    m_uniforms.tint = uniform("uTint");
    m_uniforms.hasBaseColorMap = uniform("uHasBaseColorMap");
    m_uniforms.hasNormalMap = uniform("uHasNormalMap");
    m_uniforms.hasRoughnessMap = uniform("uHasRoughnessMap");
    m_uniforms.hasMetallicMap = uniform("uHasMetallicMap");
    m_uniforms.hasSpecularMap = uniform("uHasSpecularMap");
    m_uniforms.hasAmbientOcclusionMap = uniform("uHasAmbientOcclusionMap");
    m_uniforms.hasEmissiveMap = uniform("uHasEmissiveMap");
    m_uniforms.hasOpacityMap = uniform("uHasOpacityMap");
    m_uniforms.hasSpecularFactorMap = uniform("uHasSpecularFactorMap");

    glUniform1i(m_uniforms.baseColorMap, 0);
    glUniform1i(m_uniforms.normalMap, 1);
    glUniform1i(m_uniforms.roughnessMap, 2);
    glUniform1i(m_uniforms.metallicMap, 3);
    glUniform1i(m_uniforms.specularMap, 4);
    glUniform1i(m_uniforms.ambientOcclusionMap, 5);
    glUniform1i(m_uniforms.emissiveMap, 6);
    glUniform1i(m_uniforms.opacityMap, 7);
    glUniform1i(m_uniforms.specularFactorMap, 8);
    glUniform1i(m_uniforms.environmentMap, 9);
    glUniform1i(m_uniforms.regionalWeatherMap, 15);
    glUniform1i(m_uniforms.shadowMap, 10);
    glUniform1i(m_uniforms.shadowDepthMap, 11);
    glUniform1i(m_uniforms.volumetricCloudShadow, 12);
    // WATER15C1: surface-state shader plumbing lives with the wetness module,
    // keeping this file as lifecycle/draw orchestration only.
    initializeSurfaceWetnessMaterialBindings();

    m_hotReloadEpoch = 1;
    m_textureCache.setHotReloadEpoch(m_hotReloadEpoch);

    const EnvironmentLighting initialLighting = m_environmentSystem
        ? m_environmentSystem->lighting()
        : EnvironmentLighting{};
    if (!m_environmentMap.initializeProcedural(initialLighting))
    {
        std::cerr
            << "Heritage renderer warning: environment IBL unavailable: "
            << m_environmentMap.lastError() << '\n';
    }

    if (!m_skyRenderer.initialize(m_assetRoot))
    {
        std::cerr
            << "Heritage renderer warning: visible sky shader unavailable; "
            << "environment reflections remain active.\n";
    }

    if (!initializeShadowResources())
    {
        std::cerr
            << "Heritage renderer warning: real-time sun shadows unavailable; "
            << "rendering continues without shadows.\n";
    }

    // OPT03: the obsolete renderer-side DynamicSurfaceGpuPagePool has been
    // retired entirely. The production DynamicSurfaceGpuRuntime owns the GPU
    // presentation atlases; CPU Track/rubber/temperature authority remains in
    // SurfaceWorld without an unused duplicate GPU mirror.
    initializeDynamicSurfaceGpuRuntime();

    std::cout
        << "Heritage renderer: OBJ/MTL + GLB static mesh import enabled ["
        << kTextureMapFoundationMarker << "]\n";

    m_lastError.clear();
    return true;
}
void EntityMeshRenderer::shutdown()
{
    clearCache();
    m_skyRenderer.shutdown();
    m_environmentMap.shutdown();
    shutdownRegionalWeatherMap();
    shutdownShadowResources();
    shutdownDynamicSurfaceGpuRuntime();
    m_environmentSystem = nullptr;
    if (m_program)
    {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    shutdownSurfaceWetnessResources();
    m_assetRoot.clear();
    m_resolvedTexturePaths.clear();
    m_reportedTireVisualProofNodes.clear();
    m_reportedTireColliderProofNodes.clear();
    m_preparedFrameInstanceScratch.clear();
    m_uniforms = {};
    m_hotReloadEpoch = 1;
    m_lastError.clear();
}
void EntityMeshRenderer::draw(
    const heritage::entities::EntityRegistry& registry,
    const heritage::math::Mat4& projection,
    const heritage::settings::VideoSettings& videoSettings,
    float elapsedSeconds,
    const heritage::camera::CameraFrame& cameraFrame,
    const EntityMeshRenderTargetState& renderTargetState,
    bool wireframeVisible,
    const heritage::physics::SurfaceWorld* surfaceWorld)
{
    // WATER15 preserves universal-PBR isolation. Hydrology is consumed only
    // by the isolated exact-geometry surface-state pass after normal material rendering.
    if (!m_program)
        return;

    // PERF05: no periodic filesystem timestamp/hash polling in the draw path.
    // F5 calls requestHotReloadPoll(), preserving authoring hot reload without
    // introducing a rhythmic gameplay hitch.

    using PerfClock = std::chrono::steady_clock;
    const auto millisecondsSince = [](PerfClock::time_point start) -> double {
        return std::chrono::duration<double, std::milli>(PerfClock::now() - start).count();
    };

    const auto instanceGatherStart = PerfClock::now();
    registry.meshInstances(m_instanceScratch);
    m_frameStats.instanceGatherMs += millisecondsSince(instanceGatherStart);
    const auto& instances = m_instanceScratch;
    m_frameStats.meshInstances += static_cast<std::uint64_t>(instances.size());

    const heritage::math::Vec3 eye = cameraFrame.valid
        ? cameraFrame.eyeLocal
        : heritage::math::Vec3{ 0.0f, 3.4f, 8.5f };
    const heritage::math::Vec3 cameraTarget = cameraFrame.valid
        ? cameraFrame.targetLocal
        : heritage::math::Vec3{ 0.0f, 1.0f, 0.0f };
    const heritage::math::Vec3 cameraUp = cameraFrame.valid
        ? cameraFrame.up
        : heritage::math::Vec3{ 0.0f, 1.0f, 0.0f };

    // GPU camera-relative rendering: the CPU/local world may be hundreds of
    // metres from its floating origin, but shader coordinates are always built
    // around an exact camera (0,0,0). ChaseCamera itself keeps FP64 absolute
    // state so a CPU floating-origin rebase cannot kick the camera springs.
    const heritage::math::Vec3 cameraRelativeTarget{
        cameraTarget.x - eye.x,
        cameraTarget.y - eye.y,
        cameraTarget.z - eye.z
    };
    const heritage::math::Vec3 cameraRelativeEye{ 0.0f, 0.0f, 0.0f };
    const heritage::math::Mat4 view =
        lookAt(cameraRelativeEye, cameraRelativeTarget, cameraUp);
    const ViewFrustum viewFrustum = extractViewFrustum(projection, view);

    // OPT04C: acquire meshes and evaluate camera-relative node/animation/tire
    // transforms once. The same prepared data feeds both the layered shadow
    // pass and the normal material pass below.
    const auto framePreparationStart = PerfClock::now();
    prepareFrameInstances(instances, eye, elapsedSeconds);
    m_frameStats.framePreparationCpuMs += millisecondsSince(framePreparationStart);

    const heritage::math::DVec3 cameraGlobalForSurface = surfaceWorld
        ? surfaceWorld->localToGlobal(eye)
        : heritage::math::toDouble(eye);

    // LIVETRACK21 GPU atlas authority. PERF09 removes the obsolete persistent
    // GPU page mirror from the live renderer: no shader or presentation pass
    // consumes its Track/Hydro/contamination texture arrays or page-table SSBO.
    // Keeping that dead mirror synchronized was costing roughly 21 ms/frame on
    // the user's GTX 1660 Ti system. SurfaceWorld still owns Track/rubber/thermal
    // simulation; only the unused rendering copy is gone.
    const auto dynamicSurfaceUpdateStart = PerfClock::now();
    updateDynamicSurfaceGpuRuntime(
        surfaceWorld, cameraGlobalForSurface, elapsedSeconds);
    m_frameStats.dynamicSurfaceUpdateCpuMs += millisecondsSince(dynamicSurfaceUpdateStart);

    const auto regionalWeatherUpdateStart = PerfClock::now();
    const bool regionalWeatherMapReady = updateRegionalWeatherMap(
        surfaceWorld, cameraGlobalForSurface, elapsedSeconds);
    m_frameStats.regionalWeatherUpdateCpuMs += millisecondsSince(regionalWeatherUpdateStart);

    heritage::physics::weather::RegionalWeatherSample cameraRegionalWeather{};
    if (surfaceWorld)
    {
        cameraRegionalWeather = surfaceWorld->precipitation().regionalWeatherSample(
            cameraGlobalForSurface.x, cameraGlobalForSurface.z);
    }

    // Weather-film wetness used to be updated as a side effect of the obsolete
    // Track page uploader. Preserve that visible behavior directly and cheaply.
    m_surfaceWeatherFilmWetness = 0.0f;
    if (surfaceWorld)
    {
        const auto weatherOutput = surfaceWorld->weatherOutput();
        if (weatherOutput.valid)
        {
            m_surfaceWeatherFilmWetness = static_cast<float>(std::clamp(
                weatherOutput.effectiveWetness, 0.0, 1.0));
        }
    }

    const auto weatherLightingStart = PerfClock::now();
    const EnvironmentLighting lighting = weatherAdjustedLighting(
        m_environmentSystem ? m_environmentSystem->lighting() : EnvironmentLighting{},
        surfaceWorld,
        cameraGlobalForSurface);
    m_frameStats.weatherLightingCpuMs += millisecondsSince(weatherLightingStart);

    const auto shadowStart = PerfClock::now();
    const auto shadowSettingsStart = PerfClock::now();
    const bool shadowSettingsReady = synchronizeShadowSettings(videoSettings);
    m_frameStats.shadowSettingsCpuMs += millisecondsSince(shadowSettingsStart);

    bool shadowCascadesReady = false;
    if (shadowSettingsReady && lighting.keyLightIntensity > 0.01f)
    {
        const auto shadowCascadeStart = PerfClock::now();
        shadowCascadesReady = buildShadowCascades(
            projection, view, lighting.keyLightDirection);
        m_frameStats.shadowCascadeCpuMs += millisecondsSince(shadowCascadeStart);
    }
    m_shadowsActive = shadowSettingsReady
        && lighting.keyLightIntensity > 0.01f
        && shadowCascadesReady;
    if (m_shadowsActive)
        drawShadowMaps(m_preparedFrameInstanceScratch, renderTargetState);
    m_frameStats.shadowCpuMs += millisecondsSince(shadowStart);
    m_frameStats.shadowsActive = m_shadowsActive;
    m_frameStats.shadowCascadeCount = m_shadowsActive ? kCascadeCount : 0;
    m_frameStats.shadowResolution = m_shadowsActive ? m_shadowResolution : 0;
    m_frameStats.shadowFilterMode = m_shadowFilterIndex;

    const std::uint64_t environmentSerialBefore = m_environmentMap.generationSerial();
    const auto environmentUpdateStart = PerfClock::now();
    if (!m_environmentMap.updateProcedural(lighting))
    {
        const std::string warning =
            "Environment cubemap refresh failed: " + m_environmentMap.lastError();
        reportMaterialWarning(warning);
    }
    m_frameStats.environmentUpdateMs += millisecondsSince(environmentUpdateStart);
    if (m_environmentMap.generationSerial() != environmentSerialBefore)
        ++m_frameStats.environmentRefreshes;

    const auto skyDrawStart = PerfClock::now();
    SkyWeatherParameters skyWeather{};
    skyWeather.elapsedSeconds = elapsedSeconds;
    if (surfaceWorld)
    {
        const auto& weather = surfaceWorld->weather();
        skyWeather.enabled = weather.enabled;
        skyWeather.cloudCover = static_cast<float>(cameraRegionalWeather.valid
            ? cameraRegionalWeather.cloudCover : weather.cloudCover);
        skyWeather.authoredCloudCover = static_cast<float>(std::clamp(
            weather.cloudCover, 0.0, 1.0));
        skyWeather.relativeHumidity = static_cast<float>(cameraRegionalWeather.valid
            ? cameraRegionalWeather.relativeHumidity : weather.relativeHumidity);
        skyWeather.precipitationRateMmPerHour = static_cast<float>(
            cameraRegionalWeather.valid
                ? cameraRegionalWeather.currentRateMmPerHour
                : weather.precipitationRateMmPerHour);
        const auto weatherOutput = surfaceWorld->regionalWeatherOutputAt(
            cameraGlobalForSurface);
        skyWeather.windVelocityXMps = static_cast<float>(
            weatherOutput.windVelocityXMps);
        skyWeather.windVelocityZMps = static_cast<float>(
            weatherOutput.windVelocityZMps);
        const auto cloudBaseWind = surfaceWorld->precipitation().atmosphericWindVelocityMps(1000.0);
        const auto cloudTopWind = surfaceWorld->precipitation().atmosphericWindVelocityMps(3500.0);
        skyWeather.cloudBaseWindVelocityXMps = cloudBaseWind.x;
        skyWeather.cloudBaseWindVelocityZMps = cloudBaseWind.z;
        skyWeather.cloudTopWindVelocityXMps = cloudTopWind.x;
        skyWeather.cloudTopWindVelocityZMps = cloudTopWind.z;
        skyWeather.cameraGlobal = surfaceWorld->localToGlobal(eye);
        skyWeather.regionalWeatherTexture = regionalWeatherMapReady
            ? m_regionalWeatherTexture : 0;
        skyWeather.regionalWeatherCameraOffsetX = static_cast<float>(
            cameraGlobalForSurface.x - m_regionalWeatherCenterX);
        skyWeather.regionalWeatherCameraOffsetZ = static_cast<float>(
            cameraGlobalForSurface.z - m_regionalWeatherCenterZ);
        const double weatherFieldAgeSeconds = std::max(
            surfaceWorld->precipitation().elapsedSeconds()
                - m_regionalWeatherFieldElapsedAtUpload,
            0.0);
        const auto weatherFieldWind = surfaceWorld->precipitation().weatherSteeringWindVelocityMps();
        skyWeather.regionalWeatherAdvectionOffsetX = static_cast<float>(
            -static_cast<double>(weatherFieldWind.x) * weatherFieldAgeSeconds * 0.38);
        skyWeather.regionalWeatherAdvectionOffsetZ = static_cast<float>(
            -static_cast<double>(weatherFieldWind.z) * weatherFieldAgeSeconds * 0.38);
        skyWeather.regionalWeatherHalfRangeM = static_cast<float>(
            m_regionalWeatherHalfRangeM);
    }
    else
    {
        skyWeather.cameraGlobal = heritage::math::toDouble(eye);
    }

    m_skyRenderer.draw(
        view,
        projection,
        m_environmentMap,
        lighting,
        skyWeather,
        SkyRenderTargetState{
            renderTargetState.framebuffer,
            renderTargetState.viewportX, renderTargetState.viewportY,
            renderTargetState.viewportWidth, renderTargetState.viewportHeight,
            renderTargetState.samples,
            renderTargetState.scissorEnabled,
            renderTargetState.scissorX, renderTargetState.scissorY,
            renderTargetState.scissorWidth, renderTargetState.scissorHeight },
        videoSettings.gamma,
        videoSettings.brightness,
        videoSettings.contrast,
        videoSettings.saturation);
    m_frameStats.skyDrawMs += millisecondsSince(skyDrawStart);

    if (instances.empty())
    {
        const auto cloudAfterOpaqueStart = PerfClock::now();
        m_skyRenderer.drawVolumetricCloudsAfterOpaque(
            view, projection, lighting, skyWeather,
            SkyRenderTargetState{
                renderTargetState.framebuffer,
                renderTargetState.viewportX, renderTargetState.viewportY,
                renderTargetState.viewportWidth, renderTargetState.viewportHeight,
                renderTargetState.samples,
                renderTargetState.scissorEnabled,
                renderTargetState.scissorX, renderTargetState.scissorY,
                renderTargetState.scissorWidth, renderTargetState.scissorHeight });
        m_frameStats.cloudAfterOpaqueCpuMs += millisecondsSince(cloudAfterOpaqueStart);
        copySkyPerformanceStats(
            m_frameStats, m_skyRenderer.gpuStats(), m_skyRenderer.cpuStats());
        return;
    }

    const auto materialSetupStart = PerfClock::now();
    // DEBUG-WIREFRAME01: only visible authored entity geometry switches to
    // line rasterization. Restore fill before presentation/post/UI passes.
    if (wireframeVisible)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glUseProgram(m_program);
    glUniformMatrix4fv(
        m_uniforms.view,
        1, GL_FALSE, view.m);
    glUniformMatrix4fv(
        m_uniforms.projection,
        1, GL_FALSE, projection.m);
    glUniform3f(
        m_uniforms.eye,
        0.0f, 0.0f, 0.0f);
    glUniform3f(
        m_uniforms.sunDirection,
        lighting.keyLightDirection.x,
        lighting.keyLightDirection.y,
        lighting.keyLightDirection.z);
    glUniform3f(
        m_uniforms.sunRadiance,
        lighting.keyLightColor.x * lighting.keyLightIntensity,
        lighting.keyLightColor.y * lighting.keyLightIntensity,
        lighting.keyLightColor.z * lighting.keyLightIntensity);
    glUniform1f(
        m_uniforms.gamma,
        videoSettings.gamma);
    glUniform1f(
        m_uniforms.brightness,
        videoSettings.brightness);
    glUniform1f(
        m_uniforms.contrast,
        videoSettings.contrast);
    glUniform1f(
        m_uniforms.saturation,
        videoSettings.saturation);
    bindRegionalWeatherMaterialState(
        surfaceWorld,
        cameraGlobalForSurface,
        cameraRegionalWeather,
        regionalWeatherMapReady,
        lighting);
    glUniform1i(
        m_uniforms.tireProbeDebugVisible,
        m_tireProbeDebugVisible ? 1 : 0);

    glUniform1i(
        m_uniforms.hasEnvironmentMap,
        m_environmentMap.valid() ? 1 : 0);
    glUniform1f(
        m_uniforms.environmentMaxLod,
        m_environmentMap.maximumLod());
    if (m_environmentMap.valid())
    {
        glActiveTexture(GL_TEXTURE0 + 9);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_environmentMap.textureId());
    }

    glUniform1i(
        m_uniforms.hasVolumetricCloudShadow,
        m_skyRenderer.cloudShadowValid() ? 1 : 0);
    glUniform1f(
        m_uniforms.volumetricCloudShadowHalfRangeM,
        m_skyRenderer.cloudShadowHalfRangeM());
    glActiveTexture(GL_TEXTURE0 + 12);
    glBindTexture(
        GL_TEXTURE_2D,
        m_skyRenderer.cloudShadowValid() ? m_skyRenderer.cloudShadowTexture() : 0);
    glActiveTexture(GL_TEXTURE0);

    // WATER15C1: bind the Dynamic Track surface-state clipmaps through the
    // dedicated wetness module; this draw function only orchestrates the pass.
    bindSurfaceWetnessMaterialState(cameraGlobalForSurface, elapsedSeconds);

    glUniform1i(m_uniforms.hasShadowMap, m_shadowsActive ? 1 : 0);
    glUniform4f(
        m_uniforms.shadowSplits,
        m_shadowSplits[0],
        m_shadowSplits[1],
        m_shadowSplits[2],
        m_shadowSplits[3]);
    glUniform1f(
        m_uniforms.shadowStrength,
        std::clamp(lighting.daylightFactor, 0.0f, 1.0f));
    glUniform1i(m_uniforms.shadowFilterMode, m_shadowFilterIndex);
    if (m_shadowsActive && m_shadowTextureArray)
    {
        glUniformMatrix4fv(
            m_uniforms.shadowMatrices,
            kCascadeCount,
            GL_FALSE,
            m_shadowMatrices[0].m);
        glActiveTexture(GL_TEXTURE0 + 10);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_shadowTextureArray);
        glBindSampler(10, m_shadowCompareSampler);
        glActiveTexture(GL_TEXTURE0 + 11);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_shadowTextureArray);
        glBindSampler(11, m_shadowRawSampler);
    }
    glActiveTexture(GL_TEXTURE0);

    // OPT04B: renderer passes own deterministic state. Do not query the driver
    // for a previous blend-enable bit on this hot path.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);

    // WATER15B: receiver ownership no longer needs stencil. The water pass
    // resubmits only tagged authored geometry after the opaque pass and uses
    // GL_GEQUAL against the exact scene depth, so nearer cars/props occlude it
    // naturally.

    std::array<GLuint, 9> boundTexture2D{};
    boundTexture2D.fill(static_cast<GLuint>(~0u));
    int activeTextureUnit = 0;
    auto activateTextureUnit = [&](int unit)
    {
        if (activeTextureUnit == unit)
            return;
        glActiveTexture(GL_TEXTURE0 + unit);
        activeTextureUnit = unit;
    };

    auto bindMap = [&](
        const MaterialTextureReference& requested,
        TextureColorSpace colorSpace,
        int textureUnit,
        GLint presenceLocation,
        bool meshHasTexcoords) -> bool
    {

        if (requested.empty() || !meshHasTexcoords)
        {
            glUniform1i(presenceLocation, 0);
            return false;
        }

        std::string textureError;
        const Texture2D* texture = nullptr;
        if (requested.embedded)
        {
            texture = m_textureCache.acquireEmbedded(
                requested.embedded->key,
                requested.embedded->bytes,
                colorSpace,
                videoSettings.textureFilterIndex,
                requested.flipVerticalOnDecode,
                textureError);
        }
        else
        {
            std::filesystem::path resolved;
            std::string pathError;
            if (!resolveMaterialTexture(
                    requested.filePath, resolved, pathError))
            {
                reportMaterialWarning(pathError);
                glUniform1i(presenceLocation, 0);
                return false;
            }

            texture = m_textureCache.acquire(
                resolved,
                colorSpace,
                videoSettings.textureFilterIndex,
                requested.flipVerticalOnDecode,
                textureError);
        }

        if (!texture)
        {
            reportMaterialWarning(textureError);
            glUniform1i(presenceLocation, 0);
            return false;
        }

        if (textureUnit >= 0
            && textureUnit < static_cast<int>(boundTexture2D.size())
            && boundTexture2D[static_cast<std::size_t>(textureUnit)] != texture->id)
        {
            activateTextureUnit(textureUnit);
            glBindTexture(GL_TEXTURE_2D, texture->id);
            ++m_frameStats.textureBinds;
            boundTexture2D[static_cast<std::size_t>(textureUnit)] = texture->id;
        }
        glUniform1i(presenceLocation, 1);
        return true;
    };

    const MaterialDefinition fallbackMaterial{};
    const MaterialDefinition* lastMaterial = nullptr;
    bool lastMaterialHasBaseColorTexture = false;
    GLenum activeFrontFace = GL_CCW;
    glFrontFace(activeFrontFace);
    m_frameStats.materialSetupCpuMs += millisecondsSince(materialSetupStart);

    for (std::size_t instanceIndex = 0;
         instanceIndex < instances.size();
         ++instanceIndex)
    {
        const auto meshInstanceStart = PerfClock::now();
        const auto& instance = instances[instanceIndex];
        const PreparedFrameInstance& preparedInstance =
            m_preparedFrameInstanceScratch[instanceIndex];
        const Mesh* mesh = preparedInstance.mesh;
        if (!mesh)
        {
            const double visibleInstanceMs = millisecondsSince(meshInstanceStart);
            const double instanceMs = preparedInstance.prepareCpuMs + visibleInstanceMs;
            m_frameStats.meshVisibleInstancesCpuMs += visibleInstanceMs;
            m_frameStats.meshInstancesCpuMs += instanceMs;
            if (instanceMs > m_frameStats.slowestMeshInstanceMs)
            {
                m_frameStats.slowestMeshInstanceMs = instanceMs;
                m_frameStats.slowestMeshAsset = instance.assetPath;
            }
            continue;
        }

        const bool surfaceWetnessReceiver =
            registry.hasTag(instance.entity, "SurfaceWetnessReceiver");
        glUniform1i(
            m_uniforms.surfaceWetnessReceiver,
            surfaceWetnessReceiver ? 1 : 0);

        if (instance.doubleSided)
            glDisable(GL_CULL_FACE);
        else
            glEnable(GL_CULL_FACE);

        const heritage::math::Mat4& instanceModel = preparedInstance.instanceModel;
        const std::vector<heritage::math::Mat4>& nodeGlobals =
            preparedInstance.nodeGlobals;
        glBindVertexArray(mesh->vao);
        ++m_frameStats.vaoBinds;

        const auto drawRange =
            [&](const MeshDrawRange& range)
        {
            if (!nodeMatchesPrefixFilter(
                    *mesh,
                    range.nodeIndex,
                    instance.nodeNamePrefixFilter))
            {
                return;
            }

            ++m_frameStats.candidateRanges;
            if (range.hiddenByAuthoring)
            {
                ++m_frameStats.skippedAuthoringRanges;
                return;
            }

            heritage::math::Mat4 rangeModel = instanceModel;
            if (range.nodeIndex >= 0
                && static_cast<std::size_t>(range.nodeIndex) < nodeGlobals.size())
            {
                rangeModel = multiply(instanceModel, nodeGlobals[static_cast<std::size_t>(range.nodeIndex)]);
            }

            const MeshNode* tireVisualNode = nullptr;
            const heritage::entities::MeshNodeOverride* tireVisualState = nullptr;
            if (range.nodeIndex >= 0
                && static_cast<std::size_t>(range.nodeIndex) < mesh->nodes.size())
            {
                const MeshNode& candidate =
                    mesh->nodes[static_cast<std::size_t>(range.nodeIndex)];
                if (candidate.hasTireVisualGeometry)
                {
                    tireVisualNode = &candidate;
                    const std::size_t nodeIndex =
                        static_cast<std::size_t>(range.nodeIndex);
                    if (nodeIndex < preparedInstance.tireVisualOverrides.size())
                    {
                        tireVisualState =
                            preparedInstance.tireVisualOverrides[nodeIndex];
                    }
                }
            }

            // PERF03: reject off-screen static/non-skinned primitives before
            // touching shader uniforms, textures, material state or GL draws.
            // Skinned ranges stay conservative for now because rest-pose bounds
            // do not yet include arbitrary skin deformation.
            if (range.hasBounds && range.skinIndex < 0)
            {
                const heritage::math::Vec3 boundsCenter =
                    transformPoint(rangeModel, range.boundsCenter);
                const float tireBoundsInflation = tireVisualState ? 1.12f : 1.0f;
                const float boundsRadius =
                    range.boundsRadius * tireBoundsInflation
                    * maximumLinearScale(rangeModel);
                if (sphereOutsideFrustum(viewFrustum, boundsCenter, boundsRadius))
                {
                    ++m_frameStats.culledRanges;
                    m_frameStats.culledTriangles +=
                        static_cast<std::uint64_t>(range.indexCount / 3);
                    return;
                }
            }

            // glTF/Blender assets often create the opposite side of a vehicle
            // with a mirrored node transform (negative scale). A reflection
            // reverses triangle winding. OpenGL's global CCW front-face rule
            // would then make the mirrored wheel/rim look inside-out even
            // though Blender shows it correctly. Treat the complete node/model
            // determinant as authoritative and flip the front-face convention
            // per draw range instead of baking vehicle-specific mirror hacks.
            const bool reflectedRange =
                linearDeterminant3x3(rangeModel) < 0.0f;
            const GLenum requestedFrontFace = reflectedRange ? GL_CW : GL_CCW;
            if (requestedFrontFace != activeFrontFace)
            {
                glFrontFace(requestedFrontFace);
                ++m_frameStats.frontFaceChanges;
                activeFrontFace = requestedFrontFace;
            }

            glUniformMatrix4fv(
                m_uniforms.model,
                1, GL_FALSE, rangeModel.m);

            const bool useTireVisual =
                tireVisualNode != nullptr && tireVisualState != nullptr;
            const bool tireWithinDeformationLod =
                useTireVisual
                && tireVisualDeformationWithinDistance(
                    rangeModel, *tireVisualNode);
            const bool useTireDeformation =
                tireWithinDeformationLod
                && tireVisualState->tireVisualDeformationFieldValid;
            if (useTireDeformation)
                ++m_frameStats.tireDeformationActiveRanges;
            else if (useTireVisual
                && tireVisualState->tireVisualDeformationFieldValid)
                ++m_frameStats.tireDeformationDistanceCulledRanges;
            // Wheel assets expose the rubber tire and metal rim as separate
            // semantic nodes. Failure presentation is now handled by the same
            // tire shader: terminal loss masks the rubber while a short-lived
            // final strip/debris pass remains possible. Rim, hub and brakes are
            // independent ranges and continue through their ordinary draws.
            glUniform1i(
                m_uniforms.tireVisualEnabled,
                useTireVisual ? 1 : 0);
            glUniform1i(m_uniforms.tireFailureRenderPass, 0);
            if (useTireVisual)
            {
                if (m_reportedTireVisualProofNodes.insert(tireVisualNode->name).second)
                {
                    std::cout
                        << "TIRE25 VIS17: live tire shader node="
                        << tireVisualNode->name
                        << " outerRadiusLocal=" << tireVisualNode->tireVisualOuterRadius
                        << " innerRadiusLocal=" << tireVisualNode->tireVisualInnerRadius
                        << " halfWidthLocal=" << tireVisualNode->tireVisualHalfWidth
                        << '\n';
                }
                glUniform3f(
                    m_uniforms.tireVisualCenter,
                    tireVisualNode->tireVisualCenter[0],
                    tireVisualNode->tireVisualCenter[1],
                    tireVisualNode->tireVisualCenter[2]);
                glUniform1i(
                    m_uniforms.tireVisualAxleAxis,
                    tireVisualNode->tireVisualAxleAxis);
                glUniform1f(
                    m_uniforms.tireVisualHalfWidth,
                    tireVisualNode->tireVisualHalfWidth);
                glUniform1f(
                    m_uniforms.tireVisualInnerRadius,
                    tireVisualNode->tireVisualInnerRadius);
                glUniform1f(
                    m_uniforms.tireVisualOuterRadius,
                    tireVisualNode->tireVisualOuterRadius);
                glUniform1f(
                    m_uniforms.tireReferenceRadiusM,
                    tireVisualState->tireReferenceRadiusM);
                glUniform3f(
                    m_uniforms.tireWheelForwardWorld,
                    tireVisualState->tireWheelForwardWorld.x,
                    tireVisualState->tireWheelForwardWorld.y,
                    tireVisualState->tireWheelForwardWorld.z);
                glUniform3f(
                    m_uniforms.tireWheelRightWorld,
                    tireVisualState->tireWheelRightWorld.x,
                    tireVisualState->tireWheelRightWorld.y,
                    tireVisualState->tireWheelRightWorld.z);
                glUniform3f(
                    m_uniforms.tireWheelUpWorld,
                    tireVisualState->tireWheelUpWorld.x,
                    tireVisualState->tireWheelUpWorld.y,
                    tireVisualState->tireWheelUpWorld.z);
                glUniform1i(
                    m_uniforms.tireVisualDeformationFieldValid,
                    useTireDeformation ? 1 : 0);
                if (useTireDeformation)
                {
                    std::array<float,
                        heritage::entities::TireVisualDeformationFieldCount * 3>
                        packedDisplacementM{};
                    for (std::size_t fieldIndex = 0;
                         fieldIndex < heritage::entities::TireVisualDeformationFieldCount;
                         ++fieldIndex)
                    {
                        packedDisplacementM[fieldIndex * 3] =
                            tireVisualState->tireVisualForwardDisplacementM[fieldIndex];
                        packedDisplacementM[fieldIndex * 3 + 1] =
                            tireVisualState->tireVisualDownDisplacementM[fieldIndex];
                        packedDisplacementM[fieldIndex * 3 + 2] =
                            tireVisualState->tireVisualLateralDisplacementM[fieldIndex];
                    }
                    glUniform3fv(
                        m_uniforms.tireVisualDisplacementM,
                        static_cast<GLsizei>(
                            heritage::entities::TireVisualDeformationFieldCount),
                        packedDisplacementM.data());
                }
                glUniform1i(
                    m_uniforms.tireFailureStage,
                    static_cast<GLint>(tireVisualState->tireFailureVisualStage));
                glUniform1f(
                    m_uniforms.tireFailureTreadAttachment,
                    tireVisualState->tireFailureVisualTreadAttachment);
                glUniform1f(
                    m_uniforms.tireFailureStructuralIntegrity,
                    tireVisualState->tireFailureVisualStructuralIntegrity);
                glUniform1f(
                    m_uniforms.tireFailureEventSeed,
                    tireVisualState->tireFailureVisualEventSeed);
                glUniform1f(
                    m_uniforms.tireFailureEventAgeSeconds,
                    tireVisualState->tireFailureVisualEventAgeSeconds);
                glUniform1f(
                    m_uniforms.tireFailureWheelAngularVelocity,
                    tireVisualState->tireFailureVisualWheelAngularVelocity);
                glUniform1f(
                    m_uniforms.tireFailureWheelRotationRadians,
                    tireVisualState->tireFailureVisualWheelRotationRadians);
            }

            if (range.skinIndex >= 0
                && static_cast<std::size_t>(range.skinIndex) < mesh->skins.size()
                && mesh->skins[static_cast<std::size_t>(range.skinIndex)].joints.size()
                    > static_cast<std::size_t>(kMaxSkinJoints))
            {
                const std::string warning =
                    "Skin in " + instance.assetPath + " has "
                    + std::to_string(mesh->skins[static_cast<std::size_t>(range.skinIndex)].joints.size())
                    + " joints; current GPU palette uses the first "
                    + std::to_string(kMaxSkinJoints) + ".";
                if (m_reportedAnimationWarnings.insert(warning).second)
                    std::cerr << "GLB animation warning: " << warning << '\n';
            }

            std::vector<heritage::math::Mat4> palette = buildSkinPalette(*mesh, range, nodeGlobals);
            const bool useSkinning = !palette.empty();
            if (useSkinning)
                ++m_frameStats.skinnedRanges;
            glUniform1i(
                m_uniforms.useSkinning,
                useSkinning ? 1 : 0);
            if (useSkinning)
            {
                // OPT04C: upload only the palette entries this skin can index.
                // The old path repacked and submitted all 64 matrices for every
                // skinned range, padding unused joints with identity matrices.
                std::array<float, 16 * kMaxSkinJoints> jointData;
                for (std::size_t joint = 0; joint < palette.size(); ++joint)
                {
                    const heritage::math::Mat4& source = palette[joint];
                    for (int value = 0; value < 16; ++value)
                    {
                        jointData[joint * 16 + static_cast<std::size_t>(value)] =
                            source.m[value];
                    }
                }
                glUniformMatrix4fv(
                    m_uniforms.jointMatrices,
                    static_cast<GLsizei>(palette.size()),
                    GL_FALSE,
                    jointData.data());
            }
            const MaterialDefinition* material =
                &fallbackMaterial;
            if (!range.materialName.empty())
            {
                const auto found =
                    mesh->materials.find(range.materialName);
                if (found != mesh->materials.end())
                    material = &found->second;
            }

            if (material != lastMaterial)
            {
                ++m_frameStats.materialSwitches;
                glUniform3f(
                    m_uniforms.materialBaseColor,
                    material->baseColor.x,
                    material->baseColor.y,
                    material->baseColor.z);
                glUniform3f(
                    m_uniforms.materialSpecularColor,
                    material->specularColor.x,
                    material->specularColor.y,
                    material->specularColor.z);
                glUniform3f(
                    m_uniforms.materialEmissiveColor,
                    material->emissiveColor.x,
                    material->emissiveColor.y,
                    material->emissiveColor.z);
                glUniform1f(
                    m_uniforms.materialRoughness,
                    material->roughness);
                glUniform1f(
                    m_uniforms.materialMetallic,
                    material->metallic);
                glUniform1f(
                    m_uniforms.materialSpecularFactor,
                    material->specularFactor);
                glUniform1f(
                    m_uniforms.materialOpacity,
                    material->opacity);
                glUniform1i(
                    m_uniforms.roughnessChannel,
                    static_cast<int>(material->roughnessMap.channel));
                glUniform1i(
                    m_uniforms.metallicChannel,
                    static_cast<int>(material->metallicMap.channel));
                glUniform1i(
                    m_uniforms.ambientOcclusionChannel,
                    static_cast<int>(material->ambientOcclusionMap.channel));
                glUniform1i(
                    m_uniforms.opacityChannel,
                    static_cast<int>(material->opacityMap.channel));
                glUniform1i(
                    m_uniforms.specularFactorChannel,
                    static_cast<int>(material->specularFactorMap.channel));

                lastMaterialHasBaseColorTexture = bindMap(
                    material->baseColorMap,
                    TextureColorSpace::SRgb,
                    0,
                    m_uniforms.hasBaseColorMap,
                    mesh->hasTexcoords);
                bindMap(
                    material->normalMap,
                    TextureColorSpace::Linear,
                    1,
                    m_uniforms.hasNormalMap,
                    mesh->hasTexcoords);
                bindMap(
                    material->roughnessMap,
                    TextureColorSpace::Linear,
                    2,
                    m_uniforms.hasRoughnessMap,
                    mesh->hasTexcoords);
                bindMap(
                    material->metallicMap,
                    TextureColorSpace::Linear,
                    3,
                    m_uniforms.hasMetallicMap,
                    mesh->hasTexcoords);
                bindMap(
                    material->specularMap,
                    TextureColorSpace::SRgb,
                    4,
                    m_uniforms.hasSpecularMap,
                    mesh->hasTexcoords);
                bindMap(
                    material->ambientOcclusionMap,
                    TextureColorSpace::Linear,
                    5,
                    m_uniforms.hasAmbientOcclusionMap,
                    mesh->hasTexcoords);
                bindMap(
                    material->emissiveMap,
                    TextureColorSpace::SRgb,
                    6,
                    m_uniforms.hasEmissiveMap,
                    mesh->hasTexcoords);
                bindMap(
                    material->opacityMap,
                    TextureColorSpace::Linear,
                    7,
                    m_uniforms.hasOpacityMap,
                    mesh->hasTexcoords);
                bindMap(
                    material->specularFactorMap,
                    TextureColorSpace::Linear,
                    8,
                    m_uniforms.hasSpecularFactorMap,
                    mesh->hasTexcoords);
                lastMaterial = material;
            }

            glUniform1i(
                m_uniforms.useVertexColor,
                range.hasVertexColors ? 1 : 0);

            // Existing MeshComponent color remains the fallback/tint for
            // untextured geometry. Once an authored base-colour texture is
            // actually present, show its real colour instead of multiplying it
            // by legacy prototype tints (for example the very dark wheel tint).
            glUniform3f(
                m_uniforms.tint,
                lastMaterialHasBaseColorTexture ? 1.0f : instance.color.x,
                lastMaterialHasBaseColorTexture ? 1.0f : instance.color.y,
                lastMaterialHasBaseColorTexture ? 1.0f : instance.color.z);

            drawElementsProfiled(
                m_frameStats, GL_TRIANGLES,
                static_cast<GLsizei>(range.indexCount), GL_UNSIGNED_INT,
                reinterpret_cast<const void*>(range.firstIndex * sizeof(unsigned int)));
            ++m_frameStats.drawCalls;
            m_frameStats.triangles +=
                static_cast<std::uint64_t>(range.indexCount / 3);

            // A partially detached belt is a second rendering of only the
            // shader-selected torn sector. The original pass leaves the same
            // sector absent, so arbitrary authored tire topology can produce a
            // bounded tethered strip without runtime mesh surgery or new rigid
            // bodies. Bare-rim incidents retain the departing strip briefly.
            const bool drawFailureStrip = useTireVisual
                && (tireVisualState->tireFailureVisualTreadAttachment < 0.90f
                    || tireVisualState->tireFailureVisualStage >= 6)
                && (!tireVisualState->tireVisualBareRim
                    || tireVisualState->tireFailureVisualEventAgeSeconds < 2.2f);
            if (drawFailureStrip)
            {
                glUniform1i(m_uniforms.tireFailureRenderPass, 1);
                drawElementsProfiled(
                    m_frameStats, GL_TRIANGLES,
                    static_cast<GLsizei>(range.indexCount), GL_UNSIGNED_INT,
                    reinterpret_cast<const void*>(range.firstIndex * sizeof(unsigned int)));
                glUniform1i(m_uniforms.tireFailureRenderPass, 0);
                ++m_frameStats.drawCalls;
                m_frameStats.triangles +=
                    static_cast<std::uint64_t>(range.indexCount / 3);
            }
        };

        if (mesh->drawRanges.empty())
        {
            MeshDrawRange complete;
            complete.indexCount = mesh->indices.size();
            drawRange(complete);
        }
        else
        {
            for (const MeshDrawRange& range : mesh->drawRanges)
            {
                if (range.indexCount > 0)
                    drawRange(range);
            }
        }

        const double visibleInstanceMs = millisecondsSince(meshInstanceStart);
        const double instanceMs = preparedInstance.prepareCpuMs + visibleInstanceMs;
        m_frameStats.meshVisibleInstancesCpuMs += visibleInstanceMs;
        m_frameStats.meshInstancesCpuMs += instanceMs;
        if (instanceMs > m_frameStats.slowestMeshInstanceMs)
        {
            m_frameStats.slowestMeshInstanceMs = instanceMs;
            m_frameStats.slowestMeshAsset = instance.assetPath;
        }
    }

    // WATER15C: no second water draw. SurfaceWetnessReceiver fragments were
    // shaded with Dynamic Track state during their normal material draw above.

    const auto rendererRestoreStart = PerfClock::now();
    glBindVertexArray(0);

    // OPT04B: texture bindings are not global cleanliness requirements. Every
    // following pass explicitly binds the texture target/unit it consumes.
    // Avoid dozens of guaranteed-redundant glBindTexture(..., 0) calls here.
    // Sampler objects are different: units 10/11 are reused by later passes,
    // so release the shadow-specific sampler overrides before handing off.
    glBindSampler(10, 0);
    glBindSampler(11, 0);
    glActiveTexture(GL_TEXTURE0);

    glFrontFace(GL_CCW);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    if (wireframeVisible)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    m_frameStats.rendererRestoreCpuMs += millisecondsSince(rendererRestoreStart);

    // UnityVolumetricCloudsURP runs before transparents. At this point Heritage
    // has completed opaque scene shading and can temporally combine clouds with
    // the actual camera colour while keeping ordinary geometry occlusion.
    const auto cloudAfterOpaqueStart = PerfClock::now();
    m_skyRenderer.drawVolumetricCloudsAfterOpaque(
        view, projection, lighting, skyWeather,
        SkyRenderTargetState{
            renderTargetState.framebuffer,
            renderTargetState.viewportX, renderTargetState.viewportY,
            renderTargetState.viewportWidth, renderTargetState.viewportHeight,
            renderTargetState.samples,
            renderTargetState.scissorEnabled,
            renderTargetState.scissorX, renderTargetState.scissorY,
            renderTargetState.scissorWidth, renderTargetState.scissorHeight });
    m_frameStats.cloudAfterOpaqueCpuMs += millisecondsSince(cloudAfterOpaqueStart);
    copySkyPerformanceStats(
        m_frameStats, m_skyRenderer.gpuStats(), m_skyRenderer.cpuStats());
}
} // namespace heritage::graphics

#include "PerformanceOverlay.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <string>

namespace heritage::diagnostics {

void drawPerformanceOverlay(
    const PerformanceSnapshot& performance,
    const heritage::graphics::EntityMeshRendererStats& meshStats,
    const heritage::graphics::EntityDebugRendererStats& debugStats,
    const heritage::graphics::SurfacePresentationRendererStats& surfaceStats,
    const heritage::graphics::WeatherPresentationRendererStats& weatherStats,
    const heritage::graphics::VegetationStats& vegetationStats,
    std::size_t entityCount,
    std::size_t loadedAssetCount,
    const heritage::jobs::JobSystemStats& jobStats,
    int physicsWorldSteps,
    bool physicsOverloaded,
    bool vsyncEnabled,
    int fpsCap)
{
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize;

    if (ImGuiViewport* viewport = ImGui::GetMainViewport())
    {
        const ImVec2 workPos = viewport->WorkPos;
        const ImVec2 workSize = viewport->WorkSize;
        // PERF03: F8 used AlwaysAutoResize while every number changes every
        // frame. Because the window is right-anchored, even a one-character
        // width change moved the whole panel horizontally and looked like a
        // ~1 cm flicker. Give diagnostics stable geometry and let ImGui scroll
        // the long report instead of resizing the window around live text.
        const float availableWidth = (std::max)(320.0f, workSize.x - 24.0f);
        const float preferredWidth = (std::max)(760.0f, ImGui::GetFontSize() * 92.0f);
        const float stableWidth = (std::min)(availableWidth, preferredWidth);
        const float stableHeight = (std::max)(240.0f, workSize.y - 54.0f);
        ImGui::SetNextWindowSize(ImVec2(stableWidth, stableHeight), ImGuiCond_Always);
        ImGui::SetNextWindowPos(
            ImVec2(workPos.x + workSize.x - 12.0f, workPos.y + 42.0f),
            ImGuiCond_Always,
            ImVec2(1.0f, 0.0f));
    }

    ImGui::SetNextWindowBgAlpha(0.88f);
    if (!ImGui::Begin("HERITAGE PERFORMANCE [F8]", nullptr, flags))
    {
        ImGui::End();
        return;
    }

    ImGui::Text("%.1f FPS   %.2f ms/frame", performance.fps, performance.frameMs);
    ImGui::Text("1%% low: %.1f FPS   0.1%% low: %.1f FPS",
        performance.onePercentLowFps,
        performance.pointOnePercentLowFps);
    ImGui::Text("P99: %.2f ms   P99.9: %.2f ms   worst: %.2f ms",
        performance.p99FrameMs,
        performance.p999FrameMs,
        performance.worstRollingFrameMs);
    ImGui::TextDisabled("Rolling frame pacing window: %zu / 4096 frames",
        performance.statisticsSampleCount);
    if (fpsCap > 0)
        ImGui::TextDisabled("Pacing: VSync %s | FPS cap %d",
            vsyncEnabled ? "ON" : "off", fpsCap);
    else
        ImGui::TextDisabled("Pacing: VSync %s | FPS cap Unlimited",
            vsyncEnabled ? "ON" : "off");

    if (performance.frameTimeGraphCount > 1)
    {
        float graphMaxMs = 33.333f;
        for (std::size_t index = 0; index < performance.frameTimeGraphCount; ++index)
            graphMaxMs = (std::max)(graphMaxMs, performance.frameTimeGraphMs[index] * 1.10f);
        graphMaxMs = (std::min)(graphMaxMs, 100.0f);

        char graphOverlay[96]{};
        std::snprintf(
            graphOverlay,
            sizeof(graphOverlay),
            "FRAME TIME  |  16.67 ms = 60 FPS  |  scale %.1f ms",
            graphMaxMs);
        ImGui::PlotLines(
            "##HeritageFrameTimeGraph",
            performance.frameTimeGraphMs.data(),
            static_cast<int>(performance.frameTimeGraphCount),
            0,
            graphOverlay,
            0.0f,
            graphMaxMs,
            ImVec2(0.0f, 72.0f));
    }

    ImGui::Text("CPU active: %.2f ms", performance.frameActiveMs);
    if (performance.gpuFrameMs > 0.0)
        ImGui::Text("GPU frame: %.2f ms", performance.gpuFrameMs);
    else
        ImGui::TextDisabled("GPU frame: waiting for timer query...");

    if (performance.gpuNamedMs > 0.0)
    {
        ImGui::TextDisabled("GPU passes: module %.2f  mesh %.2f  surface %.2f  weather %.2f",
            performance.gpuModuleMs,
            performance.gpuMeshMs,
            performance.gpuSurfaceMs,
            performance.gpuWeatherMs);
        ImGui::TextDisabled("            debug %.2f  resolve %.2f  post %.2f  residual %.2f",
            performance.gpuDebugMs,
            performance.gpuMsaaResolveMs,
            performance.gpuPostProcessMs,
            performance.gpuResidualMs);
        ImGui::TextDisabled("            asynchronous timestamp pairs; no current-frame query wait");
    }
    else
    {
        ImGui::TextDisabled("GPU passes: waiting for asynchronous timestamp samples...");
    }

    ImGui::Separator();
    ImGui::Text("CPU BREAKDOWN (rolling)");
    ImGui::Text("Events / input %6.2f ms", performance.eventsInputMs);
    ImGui::Text("Audio          %6.2f ms", performance.audioMs);
    ImGui::Text("Housekeeping   %6.2f ms", performance.housekeepingMs);
    ImGui::Text("Physics        %6.2f ms", performance.physicsMs);
    ImGui::Text("Game update    %6.2f ms", performance.gameUpdateMs);
    ImGui::Text("Render submit  %6.2f ms", performance.renderCpuMs);
    ImGui::TextDisabled("  module %.2f  mesh %.2f  surface %.2f  weather %.2f",
        performance.renderModuleMs,
        performance.renderMeshMs,
        performance.renderSurfaceMs,
        performance.renderWeatherMs);
    ImGui::TextDisabled("  debug %.2f  fbo %.2f  resolve %.2f  post %.2f",
        performance.renderDebugMs,
        performance.renderFramebufferSetupMs,
        performance.renderMsaaResolveMs,
        performance.renderPostProcessMs);
    ImGui::TextDisabled("  span %.2f  residual %.2f",
        performance.renderSpanCompositeMs,
        performance.residualRenderCpuMs);
    ImGui::Text("UI             %6.2f ms", performance.uiCpuMs);
    ImGui::Text("Present/VSync  %6.2f ms", performance.presentMs);

    // PERF06: the deep report used to be assembled and submitted through more
    // than a hundred individual ImGui::Text calls every rendered frame.  The
    // measurements themselves are intentionally still collected every frame;
    // only the human-readable snapshot is formatted at 10 Hz and submitted as
    // one large unwrapped text item.  Dear ImGui's large-text path clips lines
    // outside the visible scroll region, so opening F8 no longer turns the
    // profiler itself into the dominant CPU workload.
    struct DetailedReportCache
    {
        std::string text;
        double nextRefreshTime = 0.0;
    };
    static DetailedReportCache reportCache;

    constexpr double kDetailedReportRefreshSeconds = 0.10; // 10 Hz
    const double overlayTime = ImGui::GetTime();
    if (reportCache.text.empty() || overlayTime >= reportCache.nextRefreshTime)
    {
        reportCache.nextRefreshTime = overlayTime + kDetailedReportRefreshSeconds;
        reportCache.text.clear();
        reportCache.text.reserve(24 * 1024);

        auto appendf = [&](const char* format, ...)
        {
            std::array<char, 2048> buffer{};
            va_list arguments;
            va_start(arguments, format);
            const int written = std::vsnprintf(buffer.data(), buffer.size(), format, arguments);
            va_end(arguments);
            if (written <= 0)
                return;
            const std::size_t count = (std::min)(
                static_cast<std::size_t>(written), buffer.size() - 1u);
            reportCache.text.append(buffer.data(), count);
            reportCache.text.push_back('\n');
        };

        appendf("LAST CPU HITCH (>= 20 ms active)");
        if (!performance.hasCpuHitch)
        {
            appendf("None captured yet.");
        }
        else
        {
            appendf("Frame %llu   total %.2f ms",
                static_cast<unsigned long long>(performance.hitchFrameNumber),
                performance.hitchActiveMs);
            appendf("Events/input %6.2f   Audio %6.2f",
                performance.hitchEventsInputMs,
                performance.hitchAudioMs);
            appendf("Housekeeping %6.2f   Physics %6.2f",
                performance.hitchHousekeepingMs,
                performance.hitchPhysicsMs);
            appendf("Game update  %6.2f   Render %6.2f",
                performance.hitchGameUpdateMs,
                performance.hitchRenderCpuMs);
            appendf("  hitch: module %.2f  mesh %.2f  surface %.2f  weather %.2f",
                performance.hitchRenderModuleMs,
                performance.hitchRenderMeshMs,
                performance.hitchRenderSurfaceMs,
                performance.hitchRenderWeatherMs);
            appendf("  hitch: debug %.2f  fbo %.2f  resolve %.2f  post %.2f",
                performance.hitchRenderDebugMs,
                performance.hitchRenderFramebufferSetupMs,
                performance.hitchRenderMsaaResolveMs,
                performance.hitchRenderPostProcessMs);
            appendf("  hitch: span %.2f  residual %.2f",
                performance.hitchRenderSpanCompositeMs,
                performance.hitchResidualRenderCpuMs);
            appendf("UI           %6.2f   Present %6.2f",
                performance.hitchUiCpuMs,
                performance.hitchPresentMs);
            appendf("Unattributed %6.2f ms", performance.hitchUnattributedMs);
            appendf("F12 capture stalls are excluded from gameplay timing and hitch diagnostics.");
        }

        appendf("");
        appendf("RENDER");
        appendf("Mesh draw calls: %llu", static_cast<unsigned long long>(meshStats.drawCalls));
        appendf("Mesh triangles:  %llu", static_cast<unsigned long long>(meshStats.triangles));
        appendf("Mesh instances:  %llu", static_cast<unsigned long long>(meshStats.meshInstances));
        appendf("Visible ranges:   %llu / %llu",
            static_cast<unsigned long long>(
                meshStats.candidateRanges - meshStats.culledRanges - meshStats.skippedAuthoringRanges),
            static_cast<unsigned long long>(meshStats.candidateRanges));
        appendf("Frustum culled:   %llu ranges / %llu tris",
            static_cast<unsigned long long>(meshStats.culledRanges),
            static_cast<unsigned long long>(meshStats.culledTriangles));
        appendf("Mesh CPU attribution (current sampled frame; no forced GPU waits)");
        appendf("  gather %.3f | prepare %.3f | dyn-surface %.3f | regional weather %.3f ms",
            meshStats.instanceGatherMs,
            meshStats.framePreparationCpuMs,
            meshStats.dynamicSurfaceUpdateCpuMs,
            meshStats.regionalWeatherUpdateCpuMs);
        appendf("  lighting %.3f | shadows %.3f | env %.3f | sky/background %.3f ms",
            meshStats.weatherLightingCpuMs,
            meshStats.shadowCpuMs,
            meshStats.environmentUpdateMs,
            meshStats.skyDrawMs);
        appendf("  material setup %.3f | visible instances %.3f | restore %.3f | post-opaque clouds %.3f ms",
            meshStats.materialSetupCpuMs,
            meshStats.meshVisibleInstancesCpuMs,
            meshStats.rendererRestoreCpuMs,
            meshStats.cloudAfterOpaqueCpuMs);
        appendf("PERF03 SKY/BACKGROUND CPU attribution (sampled frame)");
        appendf("  total %.3f | GPU-query polls %.3f | timer begin %.3f | state %.3f | uniforms %.3f | textures %.3f ms",
            meshStats.skyBackgroundTotalCpuMs,
            meshStats.skyGpuTimerPollTotalCpuMs,
            meshStats.skyBackgroundTimerBeginCpuMs,
            meshStats.skyBackgroundStateSetupCpuMs,
            meshStats.skyBackgroundUniformUploadCpuMs,
            meshStats.skyBackgroundTextureBindCpuMs);
        appendf("  sky draw GL %.3f | timer end %.3f | cloud-shadow update %.3f | restore %.3f ms",
            meshStats.skyBackgroundDrawCallCpuMs,
            meshStats.skyBackgroundTimerEndCpuMs,
            meshStats.skyCloudShadowUpdateCpuMs,
            meshStats.skyBackgroundRestoreCpuMs);
        appendf("  query polls: background %.3f | shadow %.3f | scene-copy %.3f | raymarch %.3f ms",
            meshStats.skyGpuTimerPollBackgroundCpuMs,
            meshStats.skyGpuTimerPollCloudShadowCpuMs,
            meshStats.skyGpuTimerPollSceneCopyCpuMs,
            meshStats.skyGpuTimerPollRaymarchCpuMs);
        appendf("               upscale %.3f | TAA %.3f | present %.3f ms",
            meshStats.skyGpuTimerPollUpscaleCpuMs,
            meshStats.skyGpuTimerPollTemporalCpuMs,
            meshStats.skyGpuTimerPollPresentCpuMs);
        appendf("PERF04 CLOUD-SHADOW UPDATE CPU attribution (sampled frame)");
        appendf("  outer %.3f | internal %.3f | eligibility %.3f | targets %.3f | timer begin %.3f ms",
            meshStats.skyCloudShadowUpdateCpuMs,
            meshStats.cloudShadowInternalTotalCpuMs,
            meshStats.cloudShadowEligibilityCpuMs,
            meshStats.cloudShadowTargetEnsureCpuMs,
            meshStats.cloudShadowTimerBeginCpuMs);
        appendf("  state %.3f | raw attachment %.3f | program %.3f | uniforms %.3f | textures %.3f ms",
            meshStats.cloudShadowStateSetupCpuMs,
            meshStats.cloudShadowRawAttachmentCpuMs,
            meshStats.cloudShadowProgramBindCpuMs,
            meshStats.cloudShadowUniformUploadCpuMs,
            meshStats.cloudShadowTextureBindCpuMs);
        appendf("  RAW DRAW GL %.3f | filter setup %.3f | filter attachment %.3f | filter texture %.3f ms",
            meshStats.cloudShadowRawDrawCallCpuMs,
            meshStats.cloudShadowFilterSetupCpuMs,
            meshStats.cloudShadowFilterAttachmentCpuMs,
            meshStats.cloudShadowFilterTextureBindCpuMs);
        appendf("  FILTER DRAW GL %.3f | COPY IMAGE GL %.3f | finalize %.3f | timer end %.3f | residual %.3f ms",
            meshStats.cloudShadowFilterDrawCallCpuMs,
            meshStats.cloudShadowCopyImageCpuMs,
            meshStats.cloudShadowFinalizeCpuMs,
            meshStats.cloudShadowTimerEndCpuMs,
            meshStats.cloudShadowResidualCpuMs);
        appendf("  outer async GPU timer begin/end %.3f ms", meshStats.outerGpuTimerCpuMs);
        appendf("  mesh glDrawElements wall %.3f ms | slowest single draw %.3f ms | instance total+prepare %.3f ms",
            meshStats.meshDriverDrawCpuMs,
            meshStats.slowestMeshDriverDrawCpuMs,
            meshStats.meshInstancesCpuMs);

        const double cloudDriverCallCpuMs =
            meshStats.cloudSceneColorBlitCpuMs
            + meshStats.cloudSceneDepthBlitCpuMs
            + meshStats.cloudRaymarchDrawCallCpuMs
            + meshStats.cloudUpscaleDrawCallCpuMs
            + meshStats.cloudTemporalDrawCallCpuMs
            + meshStats.cloudPresentDrawCallCpuMs
            + meshStats.cloudDepthMergeDrawCallCpuMs;
        const double cloudNamedCpuMs =
            meshStats.cloudTargetEnsureCpuMs
            + meshStats.cloudSceneCopyCpuMs
            + meshStats.cloudRaymarchCpuMs
            + meshStats.cloudUpscaleCpuMs
            + meshStats.cloudTemporalCpuMs
            + meshStats.cloudPresentCpuMs
            + meshStats.cloudRestoreCpuMs;
        const double cloudResidualCpuMs = (std::max)(0.0, meshStats.cloudPipelineCpuMs - cloudNamedCpuMs);
        appendf("Cloud CPU internal %.3f: target %.3f | scene copy %.3f | ray %.3f | upscale %.3f | TAA %.3f | present %.3f | restore %.3f ms",
            meshStats.cloudPipelineCpuMs,
            meshStats.cloudTargetEnsureCpuMs,
            meshStats.cloudSceneCopyCpuMs,
            meshStats.cloudRaymarchCpuMs,
            meshStats.cloudUpscaleCpuMs,
            meshStats.cloudTemporalCpuMs,
            meshStats.cloudPresentCpuMs,
            meshStats.cloudRestoreCpuMs);
        appendf("Cloud GL-call wall: color blit %.3f | depth blit %.3f | ray draw %.3f | TAA draw %.3f | present draw %.3f ms",
            meshStats.cloudSceneColorBlitCpuMs,
            meshStats.cloudSceneDepthBlitCpuMs,
            meshStats.cloudRaymarchDrawCallCpuMs,
            meshStats.cloudTemporalDrawCallCpuMs,
            meshStats.cloudPresentDrawCallCpuMs);
        appendf("                    upscale draw %.3f | depth-merge draw %.3f | measured GL-call sum %.3f | cloud residual %.3f ms",
            meshStats.cloudUpscaleDrawCallCpuMs,
            meshStats.cloudDepthMergeDrawCallCpuMs,
            cloudDriverCallCpuMs,
            cloudResidualCpuMs);
        appendf("Sky GPU async: background %.3f | cloud shadow %.3f | scene copy %.3f ms",
            meshStats.skyBackgroundGpuMs,
            meshStats.cloudShadowGpuMs,
            meshStats.cloudSceneCopyGpuMs);
        appendf("               raymarch %.3f | upscale %.3f | TAA %.3f | present %.3f ms",
            meshStats.cloudRaymarchGpuMs,
            meshStats.cloudUpscaleGpuMs,
            meshStats.cloudTemporalGpuMs,
            meshStats.cloudPresentGpuMs);
        appendf("Mesh CPU instances: %.3f ms  slowest: %.3f ms",
            meshStats.meshInstancesCpuMs,
            meshStats.slowestMeshInstanceMs);

        appendf("LIVETRACK21 STANDING + RUNNING WATER <=500m / 20Hz TOPOLOGY POLL");
        appendf("Near topology: %u/%u resident | %u visible <=100m | %u prefetch",
            meshStats.dynamicSurfaceGpuResidentTiles,
            meshStats.dynamicSurfaceGpuDesiredTopologyTiles,
            meshStats.dynamicSurfaceGpuVisibleTopologyTiles,
            meshStats.dynamicSurfaceGpuPrewarmTiles);
        appendf("Near source: %u prebaked | %u fallback | %u uploads this frame",
            meshStats.dynamicSurfaceGpuPrebakedTopologyTiles,
            meshStats.dynamicSurfaceGpuFallbackTopologyTiles,
            meshStats.dynamicSurfaceGpuTopologyUploadsThisFrame);
        appendf("Far topology <=500m: %u/%u resident | %u admitted this poll | unavailable %u",
            meshStats.dynamicSurfaceGpuFarResidentTopologyTiles,
            meshStats.dynamicSurfaceGpuFarDesiredTopologyTiles,
            meshStats.dynamicSurfaceGpuFarTopologyUploadsThisFrame,
            meshStats.dynamicSurfaceGpuFarTopologyBacklogTiles);
        appendf("Optional snow/mud due %u | backlog %u",
            meshStats.dynamicSurfaceGpuDueTiles,
            meshStats.dynamicSurfaceGpuBacklogTiles);
        appendf("Localized compute: %.3f ms GPU  %.3f ms CPU  dispatch %llu  texels %llu",
            meshStats.dynamicSurfaceGpuRuntimeGpuMs,
            meshStats.dynamicSurfaceGpuRuntimeCpuMs,
            static_cast<unsigned long long>(meshStats.dynamicSurfaceGpuRuntimeDispatches),
            static_cast<unsigned long long>(meshStats.dynamicSurfaceGpuRuntimeCells));
        appendf("PERF02 Dynamic Surface CPU attribution (sampled frame; no forced GPU waits)");
        appendf("  total %.3f | bookkeeping %.3f | readback poll %.3f | residency %.3f | state %.3f | geom bind %.3f | timer %.3f | residual %.3f ms",
            meshStats.dynamicSurfaceGpuRuntimeCpuMs,
            meshStats.dynamicSurfaceGpuBookkeepingCpuMs,
            meshStats.dynamicSurfaceGpuTireReadbackPollCpuMs,
            meshStats.dynamicSurfaceGpuResidencyCpuMs,
            meshStats.dynamicSurfaceGpuStateProvisionCpuMs,
            meshStats.dynamicSurfaceGpuGeometryBindCpuMs,
            meshStats.dynamicSurfaceGpuTimerWallCpuMs,
            meshStats.dynamicSurfaceGpuResidualCpuMs);
        appendf("  compute owners: optional %.3f | tire events %.3f | tire-water dispatch %.3f ms",
            meshStats.dynamicSurfaceGpuOptionalDispatchCpuMs,
            meshStats.dynamicSurfaceGpuTireEventCpuMs,
            meshStats.dynamicSurfaceGpuTireWaterDispatchCpuMs);
        appendf("  residency: near build %.3f | near raster %.3f | FAR TOTAL %.3f | far build %.3f | sort %.3f | scan %.3f | resolve %.3f ms",
            meshStats.dynamicSurfaceNearResidencyBuildCpuMs,
            meshStats.dynamicSurfaceNearTopologyRasterCpuMs,
            meshStats.dynamicSurfaceFarTopologyCpuMs,
            meshStats.dynamicSurfaceFarCandidateBuildCpuMs,
            meshStats.dynamicSurfaceFarCandidateSortCpuMs,
            meshStats.dynamicSurfaceFarMissingScanCpuMs,
            meshStats.dynamicSurfaceFarTileResolveCpuMs);
        appendf("  last 20Hz poll: total %.3f | far %.3f | candidates %u | build %.3f | sort %.3f | scan %.3f | resolve %.3f | atlasGL %.3f | tagsGL %.3f ms%s",
            meshStats.dynamicSurfaceLastResidencyPollCpuMs,
            meshStats.dynamicSurfaceLastFarTopologyCpuMs,
            meshStats.dynamicSurfaceLastFarCandidateTilesEvaluated,
            meshStats.dynamicSurfaceLastFarCandidateBuildCpuMs,
            meshStats.dynamicSurfaceLastFarCandidateSortCpuMs,
            meshStats.dynamicSurfaceLastFarMissingScanCpuMs,
            meshStats.dynamicSurfaceLastFarTileResolveCpuMs,
            meshStats.dynamicSurfaceLastFarAtlasUploadGlMs,
            meshStats.dynamicSurfaceLastFarTagUploadGlMs,
            meshStats.dynamicSurfaceResidencyPolledThisFrame ? "  [THIS FRAME]" : "");
        appendf("  topology GL wall: near atlas %.3f | indirection %.3f | far atlas %.3f | far tags %.3f ms",
            meshStats.dynamicSurfaceNearTopologyUploadGlMs,
            meshStats.dynamicSurfaceTileIndirectionUploadGlMs,
            meshStats.dynamicSurfaceFarAtlasUploadGlMs,
            meshStats.dynamicSurfaceFarTagUploadGlMs);
        appendf("  tire-event GL: setup %.3f | uniforms %.3f | dispatch %.3f (slowest %.3f) | barrier %.3f ms",
            meshStats.dynamicSurfaceTireEventSetupGlMs,
            meshStats.dynamicSurfaceTireEventUniformGlMs,
            meshStats.dynamicSurfaceTireEventDispatchGlMs,
            meshStats.dynamicSurfaceTireEventSlowestDispatchGlMs,
            meshStats.dynamicSurfaceTireEventBarrierGlMs);
        appendf("  sample poll GL: wait0 %.3f | map %.3f | unmap %.3f ms",
            meshStats.dynamicSurfaceTireReadbackWaitGlMs,
            meshStats.dynamicSurfaceTireReadbackMapGlMs,
            meshStats.dynamicSurfaceTireReadbackUnmapGlMs);
        appendf("  sample dispatch GL: upload %.3f | setup %.3f | dispatch %.3f | barrier %.3f | fence %.3f ms",
            meshStats.dynamicSurfaceTireWaterUploadGlMs,
            meshStats.dynamicSurfaceTireWaterSetupGlMs,
            meshStats.dynamicSurfaceTireWaterDispatchGlMs,
            meshStats.dynamicSurfaceTireWaterBarrierGlMs,
            meshStats.dynamicSurfaceTireWaterFenceGlMs);
        appendf("  optional tile GL: dispatch %.3f | copy %.3f | barriers %.3f ms",
            meshStats.dynamicSurfaceOptionalDispatchGlMs,
            meshStats.dynamicSurfaceOptionalCopyGlMs,
            meshStats.dynamicSurfaceOptionalBarrierGlMs);
        appendf("Legacy renderer page mirror: RETIRED (OPT03)");
        appendf("Scene water exposure: %.3f mm | rain %.1f runoff %.1f drain %.2f evaporate %.2f mm/h",
            meshStats.dynamicSurfaceGpuBackgroundSeedDepthM * 1000.0f,
            meshStats.dynamicSurfaceGpuRainMmPerHour,
            meshStats.dynamicSurfaceGpuRunoffDriverMmPerHour,
            meshStats.dynamicSurfaceGpuDrainageMmPerHour,
            meshStats.dynamicSurfaceGpuEvaporationMmPerHour);
        appendf("      rain water full-field CFD: OFF; priority-flood standing head + kinematic MFD runoff reconstruct directly in the GPU material shader");
        appendf("Tire water bridge: dispatch %llu | completed %llu | nonblocking drops %llu",
            static_cast<unsigned long long>(meshStats.dynamicSurfaceGpuRuntimeTireWaterSampleDispatches),
            static_cast<unsigned long long>(meshStats.dynamicSurfaceGpuRuntimeTireWaterSamplesCompleted),
            static_cast<unsigned long long>(meshStats.dynamicSurfaceGpuRuntimeTireWaterSampleReadbackDrops));
        appendf("      localized tire dry-line remains compute-shader driven; tire physics samples the same filtered GPU water field through a fenced 3-slot SSBO ring");
        appendf("      synchronous atlas readback: OFF; tire sample readback polls only already-signaled fences and never waits");
        appendf("      static .hhyd v15: priority-flood 4-bit standing-depth ceiling + total-contributing MFD catchment + flow direction; terminal minima retain catchment area");
        appendf("      water presentation: existing RGBA8 near atlas GL_LINEAR ON | 3x3 taps sample hardware-linear data | no extra water texture/data");
        appendf("      topology raster: authored triangles -> 10m/256x256 near + 10m/32x32 far; no 0.5m support-grid authority");
        appendf("      rain wetting: circular impact decals OFF; continuous film + rainfall-rate/catchment-driven running water + retained standing water");
        appendf("      retained puddle reservoir: monotonic until explicit Reset Surface Water; no automatic drain/evaporation");
        appendf("      standing solve: shared below-spill hydraulic head gives a horizontal free surface inside resolved basins; runoff remains road-following");
        appendf("      near RGBA8 + rolling far RGB8 topology cache: %.1f MiB committed; no water scratch solver",
            meshStats.dynamicSurfaceGpuRuntimeCommittedMiB);
        appendf("      100-500m: complete lower-resolution prebaked runoff/standing-depth/flow set; every desired tile admitted together");
        appendf("      tile membership poll: fixed 20Hz; no progressive far streamer; optical shading remains per-frame");
        appendf("      optional snow/mud budget: max 12 tiles/frame; no synchronized 2Hz full-field pulse");

        const double meshNamedCpuMs =
            meshStats.instanceGatherMs
            + meshStats.framePreparationCpuMs
            + meshStats.dynamicSurfaceUpdateCpuMs
            + meshStats.regionalWeatherUpdateCpuMs
            + meshStats.weatherLightingCpuMs
            + meshStats.shadowCpuMs
            + meshStats.environmentUpdateMs
            + meshStats.skyDrawMs
            + meshStats.materialSetupCpuMs
            + meshStats.meshVisibleInstancesCpuMs
            + meshStats.rendererRestoreCpuMs
            + meshStats.cloudAfterOpaqueCpuMs
            + meshStats.outerGpuTimerCpuMs;
        const double meshResidualCpuMs = (std::max)(0.0, performance.renderMeshMs - meshNamedCpuMs);
        appendf("Mesh submit residual/driver: %.3f ms (outer-scope wrapper / timer bookkeeping / uninstrumented)",
            meshResidualCpuMs);
        if (!meshStats.slowestMeshAsset.empty())
            appendf("Slowest asset: %s", meshStats.slowestMeshAsset.c_str());
        appendf("State: materials %llu  textures %llu  VAOs %llu",
            static_cast<unsigned long long>(meshStats.materialSwitches),
            static_cast<unsigned long long>(meshStats.textureBinds),
            static_cast<unsigned long long>(meshStats.vaoBinds));
        appendf("State: winding %llu  skinned %llu  env refresh %llu",
            static_cast<unsigned long long>(meshStats.frontFaceChanges),
            static_cast<unsigned long long>(meshStats.skinnedRanges),
            static_cast<unsigned long long>(meshStats.environmentRefreshes));
        appendf("Tire deform: %llu active  %llu beyond %.0f m",
            static_cast<unsigned long long>(meshStats.tireDeformationActiveRanges),
            static_cast<unsigned long long>(meshStats.tireDeformationDistanceCulledRanges),
            heritage::graphics::kTireVisualDeformationMaximumDistanceM);

        const char* shadowFilterLabel = "Nearest";
        if (meshStats.shadowFilterMode == 1)
            shadowFilterLabel = "Poisson PCF";
        else if (meshStats.shadowFilterMode >= 2)
            shadowFilterLabel = "PCSS+Poisson";
        appendf("Sun shadows: %s  %d x %dx%d  %s  CPU %.3f ms",
            meshStats.shadowsActive ? "ON" : "off",
            meshStats.shadowCascadeCount,
            meshStats.shadowResolution,
            meshStats.shadowResolution,
            shadowFilterLabel,
            meshStats.shadowCpuMs);
        appendf("Shadow draws: %llu  triangles: %llu  culled: %llu",
            static_cast<unsigned long long>(meshStats.shadowDrawCalls),
            static_cast<unsigned long long>(meshStats.shadowTriangles),
            static_cast<unsigned long long>(meshStats.shadowCulledRanges));
        appendf("      shadow CPU: settings %.3f | cascades %.3f | prepare %.3f | GL-state %.3f | submit %.3f (draw calls %.3f) | restore %.3f ms",
            meshStats.shadowSettingsCpuMs,
            meshStats.shadowCascadeCpuMs,
            meshStats.shadowPrepareCpuMs,
            meshStats.shadowStateCpuMs,
            meshStats.shadowDrawCpuMs,
            meshStats.shadowDriverDrawCpuMs,
            meshStats.shadowRestoreCpuMs);
        appendf("      shadow GPU async: %.3f ms | completed timer samples %llu (never blocks current frame)",
            meshStats.shadowGpuMs,
            static_cast<unsigned long long>(meshStats.shadowGpuTimerSamples));
        appendf("Debug draws:     %llu", static_cast<unsigned long long>(debugStats.drawCalls));
        appendf("Water physics: OPT03C single GPU authority");
        appendf("      CPU Hydro runtime: RETIRED | immutable .hhyd support cells %llu | CPU water step %.3f ms",
            static_cast<unsigned long long>(surfaceStats.waterTotalCells),
            surfaceStats.waterHydrologyStepMs);
        appendf("Rain: %llu draw  %llu GPU candidates  %llu dispatch  CPU %.3f ms",
            static_cast<unsigned long long>(weatherStats.rainDrawCalls),
            static_cast<unsigned long long>(weatherStats.rainComputeInstances),
            static_cast<unsigned long long>(weatherStats.rainComputeDispatches),
            weatherStats.rainCpuMs);
        appendf("      visibility is compacted + indirect-drawn entirely on GPU (no readback stall)");
        appendf("      authored textured rain: 0-2m 10k | 2-10m 100k | 10-100m 10k");
        appendf("      rate %.1f mm/h  renderer %s  overhead cover %s",
            weatherStats.precipitationRateMmPerHour,
            weatherStats.rendererReady ? "ready" : "NOT READY",
            weatherStats.suppressedByCover ? "YES" : "no");
        appendf("      physical mean %.2f mm  flux fall %.2f m/s  wind %.0f deg",
            weatherStats.physicalMeanDiameterMm,
            weatherStats.physicalFluxFallSpeedMps,
            weatherStats.physicalWindDirectionDegrees);
        appendf("      optical rain %s  material %dx%d",
            weatherStats.opticalTexturesReady ? "TEXTURED" : "fallback",
            weatherStats.opticalTextureWidth,
            weatherStats.opticalTextureHeight);
        appendf("Thermal: %.3f ms  Track cells %llu  min/avg/max %.1f / %.1f / %.1f C  tire heat contacts %llu",
            surfaceStats.surfaceThermalStepMs,
            static_cast<unsigned long long>(surfaceStats.surfaceThermalCells),
            surfaceStats.surfaceTemperatureMinimumC,
            surfaceStats.surfaceTemperatureAverageC,
            surfaceStats.surfaceTemperatureMaximumC,
            static_cast<unsigned long long>(surfaceStats.surfaceThermalTireContacts));
        appendf("Loaded assets:   %zu", loadedAssetCount);
        appendf("Entities:        %zu", entityCount);

        appendf("");
        appendf("JOB SYSTEM");
        appendf("Logical processors: %u   Workers: %u + caller",
            jobStats.hardwareThreadCount,
            jobStats.workerThreadCount);
        appendf("Parallel batches: %llu   ranges: %llu",
            static_cast<unsigned long long>(jobStats.parallelBatchCount),
            static_cast<unsigned long long>(jobStats.parallelRangeCount));
        appendf("Worker ranges: %llu   caller ranges: %llu",
            static_cast<unsigned long long>(jobStats.workerRangeCount),
            static_cast<unsigned long long>(jobStats.callerRangeCount));

        appendf("");
        appendf("PHYSICS");
        appendf("World steps this frame: %d", physicsWorldSteps);
        appendf("Overloaded: %s", physicsOverloaded ? "YES" : "no");

        appendf("");
        appendf("VEGETATION");
        appendf("Species: %zu   Instances: %zu",
            vegetationStats.speciesCount,
            vegetationStats.instanceCount);
        appendf("Occupied chunks: %zu   Packed: %.2f KiB",
            vegetationStats.occupiedChunkCount,
            static_cast<double>(vegetationStats.packedBytes) / 1024.0);
        appendf("");
        appendf("F8 hides/shows this overlay. Counters are diagnostic, not gameplay state.");
    }

    ImGui::Separator();
    ImGui::TextDisabled("PERF06: measurements remain live; deep report text refreshes at 10 Hz and is cached between refreshes.");
    ImGui::TextDisabled("        one large unwrapped text item enables line clipping outside the visible scroll region.");
    ImGui::TextUnformatted(reportCache.text.c_str(), reportCache.text.c_str() + reportCache.text.size());
    ImGui::End();
}


} // namespace heritage::diagnostics

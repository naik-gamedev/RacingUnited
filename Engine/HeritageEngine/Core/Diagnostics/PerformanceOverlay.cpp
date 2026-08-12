#include "PerformanceOverlay.hpp"

#include <imgui.h>

namespace heritage::diagnostics {

void drawPerformanceOverlay(
    const PerformanceSnapshot& performance,
    const heritage::graphics::EntityMeshRendererStats& meshStats,
    const heritage::graphics::EntityDebugRendererStats& debugStats,
    const heritage::graphics::VegetationStats& vegetationStats,
    std::size_t entityCount,
    std::size_t loadedAssetCount,
    int physicsWorldSteps,
    bool physicsOverloaded)
{
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoCollapse;

    if (ImGuiViewport* viewport = ImGui::GetMainViewport())
    {
        const ImVec2 workPos = viewport->WorkPos;
        const ImVec2 workSize = viewport->WorkSize;
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

    ImGui::Separator();
    ImGui::Text("CPU BREAKDOWN (rolling)");
    ImGui::Text("Events / input %6.2f ms", performance.eventsInputMs);
    ImGui::Text("Audio          %6.2f ms", performance.audioMs);
    ImGui::Text("Housekeeping   %6.2f ms", performance.housekeepingMs);
    ImGui::Text("Physics        %6.2f ms", performance.physicsMs);
    ImGui::Text("Game update    %6.2f ms", performance.gameUpdateMs);
    ImGui::Text("Render submit  %6.2f ms", performance.renderCpuMs);
    ImGui::TextDisabled("  module %.2f  mesh %.2f  debug %.2f",
        performance.renderModuleMs,
        performance.renderMeshMs,
        performance.renderDebugMs);
    ImGui::TextDisabled("  resolve %.2f  post %.2f  span %.2f  other %.2f",
        performance.renderMsaaResolveMs,
        performance.renderPostProcessMs,
        performance.renderSpanCompositeMs,
        performance.residualRenderCpuMs);
    ImGui::Text("UI             %6.2f ms", performance.uiCpuMs);
    ImGui::Text("Present/VSync  %6.2f ms", performance.presentMs);

    ImGui::Separator();
    ImGui::Text("LAST CPU HITCH (>= 25 ms active)");
    if (!performance.hasCpuHitch)
    {
        ImGui::TextDisabled("None captured yet.");
    }
    else
    {
        ImGui::Text("Frame %llu   total %.2f ms",
            static_cast<unsigned long long>(performance.hitchFrameNumber),
            performance.hitchActiveMs);
        ImGui::Text("Events/input %6.2f   Audio %6.2f",
            performance.hitchEventsInputMs,
            performance.hitchAudioMs);
        ImGui::Text("Housekeeping %6.2f   Physics %6.2f",
            performance.hitchHousekeepingMs,
            performance.hitchPhysicsMs);
        ImGui::Text("Game update  %6.2f   Render %6.2f",
            performance.hitchGameUpdateMs,
            performance.hitchRenderCpuMs);
        ImGui::TextDisabled("  hitch render: module %.2f  mesh %.2f  debug %.2f",
            performance.hitchRenderModuleMs,
            performance.hitchRenderMeshMs,
            performance.hitchRenderDebugMs);
        ImGui::TextDisabled("  resolve %.2f  post %.2f  span %.2f  other %.2f",
            performance.hitchRenderMsaaResolveMs,
            performance.hitchRenderPostProcessMs,
            performance.hitchRenderSpanCompositeMs,
            performance.hitchResidualRenderCpuMs);
        ImGui::Text("UI           %6.2f   Present %6.2f",
            performance.hitchUiCpuMs,
            performance.hitchPresentMs);
        ImGui::Text("Unattributed %6.2f ms", performance.hitchUnattributedMs);
        ImGui::TextDisabled("F12 capture stalls are excluded from gameplay timing and hitch diagnostics.");
    }

    ImGui::Separator();
    ImGui::Text("RENDER");
    ImGui::Text("Mesh draw calls: %llu",
        static_cast<unsigned long long>(meshStats.drawCalls));
    ImGui::Text("Mesh triangles:  %llu",
        static_cast<unsigned long long>(meshStats.triangles));
    ImGui::Text("Mesh instances:  %llu",
        static_cast<unsigned long long>(meshStats.meshInstances));
    ImGui::Text("Visible ranges:   %llu / %llu",
        static_cast<unsigned long long>(
            meshStats.candidateRanges - meshStats.culledRanges
            - meshStats.skippedAuthoringRanges),
        static_cast<unsigned long long>(meshStats.candidateRanges));
    ImGui::Text("Frustum culled:   %llu ranges / %llu tris",
        static_cast<unsigned long long>(meshStats.culledRanges),
        static_cast<unsigned long long>(meshStats.culledTriangles));
    ImGui::Text("Mesh CPU: gather %.3f ms  env %.3f ms  sky %.3f ms",
        meshStats.instanceGatherMs,
        meshStats.environmentUpdateMs,
        meshStats.skyDrawMs);
    ImGui::Text("Mesh CPU instances: %.3f ms  slowest: %.3f ms",
        meshStats.meshInstancesCpuMs,
        meshStats.slowestMeshInstanceMs);
    if (!meshStats.slowestMeshAsset.empty())
        ImGui::TextDisabled("Slowest asset: %s", meshStats.slowestMeshAsset.c_str());
    ImGui::Text("State: materials %llu  textures %llu  VAOs %llu",
        static_cast<unsigned long long>(meshStats.materialSwitches),
        static_cast<unsigned long long>(meshStats.textureBinds),
        static_cast<unsigned long long>(meshStats.vaoBinds));
    ImGui::Text("State: winding %llu  skinned %llu  env refresh %llu",
        static_cast<unsigned long long>(meshStats.frontFaceChanges),
        static_cast<unsigned long long>(meshStats.skinnedRanges),
        static_cast<unsigned long long>(meshStats.environmentRefreshes));
    const char* shadowFilterLabel = "Nearest";
    if (meshStats.shadowFilterMode == 1)
        shadowFilterLabel = "Poisson PCF";
    else if (meshStats.shadowFilterMode >= 2)
        shadowFilterLabel = "PCSS+Poisson";

    ImGui::Text("Sun shadows: %s  %d x %dx%d  %s  CPU %.3f ms",
        meshStats.shadowsActive ? "ON" : "off",
        meshStats.shadowCascadeCount,
        meshStats.shadowResolution,
        meshStats.shadowResolution,
        shadowFilterLabel,
        meshStats.shadowCpuMs);
    ImGui::Text("Shadow draws: %llu  triangles: %llu  culled: %llu",
        static_cast<unsigned long long>(meshStats.shadowDrawCalls),
        static_cast<unsigned long long>(meshStats.shadowTriangles),
        static_cast<unsigned long long>(meshStats.shadowCulledRanges));
    ImGui::Text("Debug draws:     %llu",
        static_cast<unsigned long long>(debugStats.drawCalls));
    ImGui::Text("Loaded assets:   %zu", loadedAssetCount);
    ImGui::Text("Entities:        %zu", entityCount);

    ImGui::Separator();
    ImGui::Text("PHYSICS");
    ImGui::Text("World steps this frame: %d", physicsWorldSteps);
    ImGui::Text("Overloaded: %s", physicsOverloaded ? "YES" : "no");

    ImGui::Separator();
    ImGui::Text("VEGETATION");
    ImGui::Text("Species: %zu   Instances: %zu",
        vegetationStats.speciesCount,
        vegetationStats.instanceCount);
    ImGui::Text("Occupied chunks: %zu   Packed: %.2f KiB",
        vegetationStats.occupiedChunkCount,
        static_cast<double>(vegetationStats.packedBytes) / 1024.0);

    ImGui::TextDisabled("F8 hides/shows this overlay. Counters are diagnostic, not gameplay state.");
    ImGui::End();
}


} // namespace heritage::diagnostics

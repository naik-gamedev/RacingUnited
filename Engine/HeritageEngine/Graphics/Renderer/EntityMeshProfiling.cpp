#include "EntityMeshRendererInternal.hpp"

#include <algorithm>
#include <chrono>

namespace heritage::graphics::entity_mesh_internal {

void copySkyPerformanceStats(
    EntityMeshRendererStats& destination,
    const SkyRendererGpuStats& gpuStats,
    const SkyRendererCpuStats& cpuStats)
{
    destination.skyBackgroundGpuMs = gpuStats.backgroundMs;
    destination.cloudShadowGpuMs = gpuStats.cloudShadowMs;

    destination.skyBackgroundTotalCpuMs = cpuStats.backgroundTotalMs;
    destination.skyGpuTimerPollTotalCpuMs = cpuStats.gpuTimerPollTotalMs;
    destination.skyGpuTimerPollBackgroundCpuMs = cpuStats.gpuTimerPollBackgroundMs;
    destination.skyGpuTimerPollCloudShadowCpuMs = cpuStats.gpuTimerPollCloudShadowMs;
    destination.skyGpuTimerPollSceneCopyCpuMs = cpuStats.gpuTimerPollSceneCopyMs;
    destination.skyGpuTimerPollRaymarchCpuMs = cpuStats.gpuTimerPollRaymarchMs;
    destination.skyGpuTimerPollUpscaleCpuMs = cpuStats.gpuTimerPollUpscaleMs;
    destination.skyGpuTimerPollTemporalCpuMs = cpuStats.gpuTimerPollTemporalMs;
    destination.skyGpuTimerPollPresentCpuMs = cpuStats.gpuTimerPollPresentMs;
    destination.skyBackgroundTimerBeginCpuMs = cpuStats.backgroundTimerBeginMs;
    destination.skyBackgroundStateSetupCpuMs = cpuStats.backgroundStateSetupMs;
    destination.skyBackgroundUniformUploadCpuMs = cpuStats.backgroundUniformUploadMs;
    destination.skyBackgroundTextureBindCpuMs = cpuStats.backgroundTextureBindMs;
    destination.skyBackgroundDrawCallCpuMs = cpuStats.backgroundDrawCallMs;
    destination.skyBackgroundTimerEndCpuMs = cpuStats.backgroundTimerEndMs;
    destination.skyCloudShadowUpdateCpuMs = cpuStats.cloudShadowUpdateCpuMs;
    destination.cloudShadowInternalTotalCpuMs = cpuStats.cloudShadowInternalTotalMs;
    destination.cloudShadowEligibilityCpuMs = cpuStats.cloudShadowEligibilityMs;
    destination.cloudShadowTargetEnsureCpuMs = cpuStats.cloudShadowTargetEnsureMs;
    destination.cloudShadowTimerBeginCpuMs = cpuStats.cloudShadowTimerBeginMs;
    destination.cloudShadowStateSetupCpuMs = cpuStats.cloudShadowStateSetupMs;
    destination.cloudShadowRawAttachmentCpuMs = cpuStats.cloudShadowRawAttachmentMs;
    destination.cloudShadowProgramBindCpuMs = cpuStats.cloudShadowProgramBindMs;
    destination.cloudShadowUniformUploadCpuMs = cpuStats.cloudShadowUniformUploadMs;
    destination.cloudShadowTextureBindCpuMs = cpuStats.cloudShadowTextureBindMs;
    destination.cloudShadowRawDrawCallCpuMs = cpuStats.cloudShadowRawDrawCallMs;
    destination.cloudShadowFilterSetupCpuMs = cpuStats.cloudShadowFilterSetupMs;
    destination.cloudShadowFilterAttachmentCpuMs = cpuStats.cloudShadowFilterAttachmentMs;
    destination.cloudShadowFilterTextureBindCpuMs = cpuStats.cloudShadowFilterTextureBindMs;
    destination.cloudShadowFilterDrawCallCpuMs = cpuStats.cloudShadowFilterDrawCallMs;
    destination.cloudShadowCopyImageCpuMs = cpuStats.cloudShadowCopyImageMs;
    destination.cloudShadowFinalizeCpuMs = cpuStats.cloudShadowFinalizeMs;
    destination.cloudShadowTimerEndCpuMs = cpuStats.cloudShadowTimerEndMs;
    destination.cloudShadowResidualCpuMs = cpuStats.cloudShadowResidualMs;
    destination.skyBackgroundRestoreCpuMs = cpuStats.backgroundRestoreMs;
    destination.cloudSceneCopyGpuMs = gpuStats.cloudSceneCopyMs;
    destination.cloudRaymarchGpuMs = gpuStats.cloudRaymarchMs;
    destination.cloudUpscaleGpuMs = gpuStats.cloudUpscaleMs;
    destination.cloudTemporalGpuMs = gpuStats.cloudTemporalMs;
    destination.cloudPresentGpuMs = gpuStats.cloudPresentMs;

    destination.cloudPipelineCpuMs = cpuStats.cloudAfterOpaqueTotalMs;
    destination.cloudTargetEnsureCpuMs = cpuStats.cloudTargetEnsureMs;
    destination.cloudSceneCopyCpuMs = cpuStats.cloudSceneCopyMs;
    destination.cloudSceneColorBlitCpuMs = cpuStats.cloudSceneColorBlitMs;
    destination.cloudSceneDepthBlitCpuMs = cpuStats.cloudSceneDepthBlitMs;
    destination.cloudRaymarchCpuMs = cpuStats.cloudRaymarchMs;
    destination.cloudRaymarchDrawCallCpuMs = cpuStats.cloudRaymarchDrawCallMs;
    destination.cloudUpscaleCpuMs = cpuStats.cloudUpscaleMs;
    destination.cloudUpscaleDrawCallCpuMs = cpuStats.cloudUpscaleDrawCallMs;
    destination.cloudTemporalCpuMs = cpuStats.cloudTemporalMs;
    destination.cloudTemporalDrawCallCpuMs = cpuStats.cloudTemporalDrawCallMs;
    destination.cloudPresentCpuMs = cpuStats.cloudPresentMs;
    destination.cloudPresentDrawCallCpuMs = cpuStats.cloudPresentDrawCallMs;
    destination.cloudDepthMergeDrawCallCpuMs = cpuStats.cloudDepthMergeDrawCallMs;
    destination.cloudRestoreCpuMs = cpuStats.cloudRestoreMs;
}

void drawElementsProfiled(
    EntityMeshRendererStats& stats,
    GLenum mode,
    GLsizei count,
    GLenum type,
    const void* indices)
{
    const auto start = std::chrono::steady_clock::now();
    glDrawElements(mode, count, type, indices);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const double milliseconds =
        std::chrono::duration<double, std::milli>(elapsed).count();
    stats.meshDriverDrawCpuMs += milliseconds;
    stats.slowestMeshDriverDrawCpuMs =
        (std::max)(stats.slowestMeshDriverDrawCpuMs, milliseconds);
}

} // namespace heritage::graphics::entity_mesh_internal

#include "DynamicSurfaceGpuRuntime.hpp"

namespace heritage::graphics::dynamicsurface {

void DynamicSurfaceGpuRuntime::resetFrameProfilingStats()
{
    m_stats.cpuBookkeepingMs = 0.0;
    m_stats.cpuTireReadbackPollMs = 0.0;
    m_stats.cpuResidencyMs = 0.0;
    m_stats.cpuStateProvisionMs = 0.0;
    m_stats.cpuGeometryBindMs = 0.0;
    m_stats.cpuGpuTimerWallMs = 0.0;
    m_stats.cpuOptionalTileDispatchMs = 0.0;
    m_stats.cpuTireEventMs = 0.0;
    m_stats.cpuTireWaterDispatchMs = 0.0;
    m_stats.cpuResidualMs = 0.0;

    m_stats.nearResidencyBuildMs = 0.0;
    m_stats.nearTopologyRasterMs = 0.0;
    m_stats.nearTopologyUploadGlMs = 0.0;
    m_stats.tileIndirectionUploadGlMs = 0.0;
    m_stats.farTopologyTotalMs = 0.0;
    m_stats.farCandidateBuildMs = 0.0;
    m_stats.farCandidateSortMs = 0.0;
    m_stats.farMissingScanMs = 0.0;
    m_stats.farTileResolveMs = 0.0;
    m_stats.farAtlasUploadGlMs = 0.0;
    m_stats.farTagUploadGlMs = 0.0;
    m_stats.farCandidateTilesEvaluated = 0u;
    m_stats.residencyPolledThisFrame = false;

    m_stats.optionalTileDispatchGlMs = 0.0;
    m_stats.optionalTileCopyGlMs = 0.0;
    m_stats.optionalTileBarrierGlMs = 0.0;
    m_stats.tireEventSetupGlMs = 0.0;
    m_stats.tireEventUniformGlMs = 0.0;
    m_stats.tireEventDispatchGlMs = 0.0;
    m_stats.tireEventSlowestDispatchGlMs = 0.0;
    m_stats.tireEventBarrierGlMs = 0.0;
    m_stats.tireReadbackClientWaitGlMs = 0.0;
    m_stats.tireReadbackMapGlMs = 0.0;
    m_stats.tireReadbackUnmapGlMs = 0.0;
    m_stats.tireWaterUploadGlMs = 0.0;
    m_stats.tireWaterSetupGlMs = 0.0;
    m_stats.tireWaterDispatchGlMs = 0.0;
    m_stats.tireWaterBarrierGlMs = 0.0;
    m_stats.tireWaterFenceGlMs = 0.0;
}

} // namespace heritage::graphics::dynamicsurface

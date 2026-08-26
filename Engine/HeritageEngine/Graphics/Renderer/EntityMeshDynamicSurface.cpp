#include "EntityMeshRenderer.hpp"

#include "../../Physics/Surfaces/SurfaceWorld.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace heritage::graphics {

void EntityMeshRenderer::initializeDynamicSurfaceGpuRuntime()
{
    std::string errorMessage;
    if (!m_dynamicSurfaceGpuRuntime.initialize(errorMessage))
    {
        std::cerr
            << "Heritage renderer warning: LIVETRACK21 standing + running surface water unavailable: "
            << errorMessage << '\n';
        return;
    }

    const auto& stats = m_dynamicSurfaceGpuRuntime.stats();
    std::cout
        << "LIVETRACK21 standing + running surface water: 10m tiles / near 256x256 + complete far 32x32 set, "
        << ".hhyd v15 priority-flood 4-bit standing-depth ceiling + MFD catchment/flow, same-domain hydraulic-head standing water + rainfall-driven kinematic runoff, no circular rain splats and no periodic full-field CFD; "
        << stats.committedMiB << " MiB committed.\n";
}

void EntityMeshRenderer::shutdownDynamicSurfaceGpuRuntime()
{
    m_dynamicSurfaceGpuRuntime.shutdown();
}

void EntityMeshRenderer::updateDynamicSurfaceGpuRuntime(
    const heritage::physics::SurfaceWorld* surfaceWorld,
    const heritage::math::DVec3& cameraGlobal,
    float elapsedSeconds)
{
    if (!surfaceWorld)
        return;

    auto* mutableSurfaceWorld =
        const_cast<heritage::physics::SurfaceWorld*>(surfaceWorld);
    auto& physicsTireEvents = m_dynamicSurfacePhysicsTireEventScratch;
    mutableSurfaceWorld->consumeGpuDynamicSurfaceTireEvents(physicsTireEvents);
    auto& physicsWaterSampleRequests = m_dynamicSurfacePhysicsWaterSampleRequestScratch;
    mutableSurfaceWorld->consumeGpuDynamicSurfaceWaterSampleRequests(physicsWaterSampleRequests);
    const double eventRadiusM =
        static_cast<double>(heritage::graphics::dynamicsurface::DynamicSurfaceGpuRuntime::simulationRadiusM())
        + 0.5 * heritage::graphics::dynamicsurface::DynamicSurfaceGpuRuntime::tileWorldSizeM();
    const double eventRadiusSquaredM2 = eventRadiusM * eventRadiusM;
    const auto eventDistanceSquared = [&cameraGlobal](
        const heritage::physics::GpuDynamicSurfaceTireEvent& event) {
        const double dx = event.globalPosition.x - cameraGlobal.x;
        const double dz = event.globalPosition.z - cameraGlobal.z;
        return dx * dx + dz * dz;
    };
    std::erase_if(physicsTireEvents, [&](const auto& event) {
        return eventDistanceSquared(event) > eventRadiusSquaredM2;
    });
    std::sort(physicsTireEvents.begin(), physicsTireEvents.end(),
        [&](const auto& a, const auto& b) {
            const double aDistance = eventDistanceSquared(a);
            const double bDistance = eventDistanceSquared(b);
            if (aDistance != bDistance)
                return aDistance < bDistance;
            if (a.globalPosition.z != b.globalPosition.z)
                return a.globalPosition.z < b.globalPosition.z;
            return a.globalPosition.x < b.globalPosition.x;
        });
    const std::size_t maximumEvents =
        heritage::graphics::dynamicsurface::DynamicSurfaceGpuRuntime::
            maximumTireContactEventsPerFrame();
    if (physicsTireEvents.size() > maximumEvents)
        physicsTireEvents.resize(maximumEvents);

    auto& gpuTireEvents = m_dynamicSurfaceGpuTireEventScratch;
    gpuTireEvents.clear();
    if (gpuTireEvents.capacity() < maximumEvents)
        gpuTireEvents.reserve(maximumEvents);
    for (const auto& event : physicsTireEvents)
    {
        heritage::graphics::dynamicsurface::DynamicSurfaceGpuTireContactEvent gpuEvent;
        gpuEvent.globalX = event.globalPosition.x;
        gpuEvent.globalZ = event.globalPosition.z;
        gpuEvent.patchLengthM = event.patchLengthM;
        gpuEvent.patchWidthM = event.patchWidthM;
        gpuEvent.forwardX = event.forwardX;
        gpuEvent.forwardZ = event.forwardZ;
        gpuEvent.rightX = event.rightX;
        gpuEvent.rightZ = event.rightZ;
        gpuEvent.normalLoadN = event.normalLoadN;
        gpuEvent.speedMps = event.speedMps;
        gpuEvent.accumulatedDtSeconds = event.accumulatedDtSeconds;
        gpuEvent.mudDeformable = event.mudDeformable;
        gpuTireEvents.push_back(gpuEvent);
    }

    std::erase_if(physicsWaterSampleRequests, [&](const auto& request) {
        const double dx = request.globalPosition.x - cameraGlobal.x;
        const double dz = request.globalPosition.z - cameraGlobal.z;
        return dx * dx + dz * dz > eventRadiusSquaredM2;
    });
    std::sort(physicsWaterSampleRequests.begin(), physicsWaterSampleRequests.end(),
        [&](const auto& a, const auto& b) {
            const double adx = a.globalPosition.x - cameraGlobal.x;
            const double adz = a.globalPosition.z - cameraGlobal.z;
            const double bdx = b.globalPosition.x - cameraGlobal.x;
            const double bdz = b.globalPosition.z - cameraGlobal.z;
            return adx * adx + adz * adz < bdx * bdx + bdz * bdz;
        });
    const std::size_t maximumWaterSamples =
        heritage::graphics::dynamicsurface::DynamicSurfaceGpuRuntime::
            maximumTireWaterSampleRequestsPerFrame();
    if (physicsWaterSampleRequests.size() > maximumWaterSamples)
        physicsWaterSampleRequests.resize(maximumWaterSamples);

    auto& gpuWaterSampleRequests = m_dynamicSurfaceGpuWaterSampleRequestScratch;
    gpuWaterSampleRequests.clear();
    if (gpuWaterSampleRequests.capacity() < maximumWaterSamples)
        gpuWaterSampleRequests.reserve(maximumWaterSamples);
    for (const auto& request : physicsWaterSampleRequests)
        gpuWaterSampleRequests.push_back({ request.globalPosition.x, request.globalPosition.z });

    const auto weatherOutput = surfaceWorld->regionalWeatherOutputAt(cameraGlobal);
    m_dynamicSurfaceGpuRuntime.update(
        static_cast<double>(elapsedSeconds),
        cameraGlobal.x,
        cameraGlobal.y,
        cameraGlobal.z,
        weatherOutput.valid
            ? static_cast<float>(weatherOutput.precipitationRateMmPerHour)
            : 0.0f,
        weatherOutput.valid
            ? static_cast<float>(weatherOutput.drainageRateMmPerHour)
            : 0.0f,
        weatherOutput.valid
            ? static_cast<float>(weatherOutput.evaporationRateMmPerHour)
            : 0.0f,
        static_cast<float>(surfaceWorld->environment().ambientTemperatureC),
        weatherOutput.valid ? static_cast<float>(weatherOutput.windVelocityXMps) : 0.0f,
        weatherOutput.valid ? static_cast<float>(weatherOutput.windVelocityZMps) : 0.0f,
        &surfaceWorld->dynamicSurface(),
        &surfaceWorld->hydrology(),
        surfaceWorld->hydrologyResetSerial(),
        gpuTireEvents,
        gpuWaterSampleRequests);

    auto& gpuWaterSamples = m_dynamicSurfaceGpuWaterSampleResultScratch;
    m_dynamicSurfaceGpuRuntime.consumeCompletedTireWaterSamples(gpuWaterSamples);
    auto& physicsWaterSamples = m_dynamicSurfacePhysicsWaterSampleResultScratch;
    physicsWaterSamples.clear();
    physicsWaterSamples.reserve(gpuWaterSamples.size());
    for (const auto& sample : gpuWaterSamples)
    {
        heritage::physics::GpuDynamicSurfaceWaterSample published;
        published.globalPosition = { sample.globalX, 0.0, sample.globalZ };
        published.waterDepthM = sample.waterDepthM;
        published.dryLine = sample.dryLine;
        published.valid = sample.valid;
        physicsWaterSamples.push_back(published);
    }
    mutableSurfaceWorld->publishGpuDynamicSurfaceWaterSamples(physicsWaterSamples);

    const auto& stats = m_dynamicSurfaceGpuRuntime.stats();
    mutableSurfaceWorld->setGpuDynamicSurfaceAuthorityEnabled(
        m_dynamicSurfaceGpuRuntime.authoritativeReady());
    m_frameStats.dynamicSurfaceGpuRuntimeReady = stats.ready;
    m_frameStats.dynamicSurfaceGpuRuntimeWaterReady = stats.waterReady;
    m_frameStats.dynamicSurfaceGpuRuntimeSnowReady = stats.snowReady;
    m_frameStats.dynamicSurfaceGpuRuntimeMudReady = stats.mudReady;
    m_frameStats.dynamicSurfaceGpuRuntimeCpuMs = stats.cpuDispatchMs;
    m_frameStats.dynamicSurfaceGpuRuntimeGpuMs = stats.gpuComputeMs;

    m_frameStats.dynamicSurfaceGpuBookkeepingCpuMs = stats.cpuBookkeepingMs;
    m_frameStats.dynamicSurfaceGpuTireReadbackPollCpuMs = stats.cpuTireReadbackPollMs;
    m_frameStats.dynamicSurfaceGpuResidencyCpuMs = stats.cpuResidencyMs;
    m_frameStats.dynamicSurfaceGpuStateProvisionCpuMs = stats.cpuStateProvisionMs;
    m_frameStats.dynamicSurfaceGpuGeometryBindCpuMs = stats.cpuGeometryBindMs;
    m_frameStats.dynamicSurfaceGpuTimerWallCpuMs = stats.cpuGpuTimerWallMs;
    m_frameStats.dynamicSurfaceGpuOptionalDispatchCpuMs = stats.cpuOptionalTileDispatchMs;
    m_frameStats.dynamicSurfaceGpuTireEventCpuMs = stats.cpuTireEventMs;
    m_frameStats.dynamicSurfaceGpuTireWaterDispatchCpuMs = stats.cpuTireWaterDispatchMs;
    m_frameStats.dynamicSurfaceGpuResidualCpuMs = stats.cpuResidualMs;

    m_frameStats.dynamicSurfaceNearResidencyBuildCpuMs = stats.nearResidencyBuildMs;
    m_frameStats.dynamicSurfaceNearTopologyRasterCpuMs = stats.nearTopologyRasterMs;
    m_frameStats.dynamicSurfaceNearTopologyUploadGlMs = stats.nearTopologyUploadGlMs;
    m_frameStats.dynamicSurfaceTileIndirectionUploadGlMs = stats.tileIndirectionUploadGlMs;
    m_frameStats.dynamicSurfaceFarTopologyCpuMs = stats.farTopologyTotalMs;
    m_frameStats.dynamicSurfaceFarCandidateBuildCpuMs = stats.farCandidateBuildMs;
    m_frameStats.dynamicSurfaceFarCandidateSortCpuMs = stats.farCandidateSortMs;
    m_frameStats.dynamicSurfaceFarMissingScanCpuMs = stats.farMissingScanMs;
    m_frameStats.dynamicSurfaceFarTileResolveCpuMs = stats.farTileResolveMs;
    m_frameStats.dynamicSurfaceFarAtlasUploadGlMs = stats.farAtlasUploadGlMs;
    m_frameStats.dynamicSurfaceFarTagUploadGlMs = stats.farTagUploadGlMs;
    m_frameStats.dynamicSurfaceFarCandidateTilesEvaluated = stats.farCandidateTilesEvaluated;
    m_frameStats.dynamicSurfaceResidencyPolledThisFrame = stats.residencyPolledThisFrame;
    m_frameStats.dynamicSurfaceLastResidencyPollCpuMs = stats.lastResidencyPollMs;
    m_frameStats.dynamicSurfaceLastFarTopologyCpuMs = stats.lastFarTopologyTotalMs;
    m_frameStats.dynamicSurfaceLastFarCandidateBuildCpuMs = stats.lastFarCandidateBuildMs;
    m_frameStats.dynamicSurfaceLastFarCandidateSortCpuMs = stats.lastFarCandidateSortMs;
    m_frameStats.dynamicSurfaceLastFarMissingScanCpuMs = stats.lastFarMissingScanMs;
    m_frameStats.dynamicSurfaceLastFarTileResolveCpuMs = stats.lastFarTileResolveMs;
    m_frameStats.dynamicSurfaceLastFarAtlasUploadGlMs = stats.lastFarAtlasUploadGlMs;
    m_frameStats.dynamicSurfaceLastFarTagUploadGlMs = stats.lastFarTagUploadGlMs;
    m_frameStats.dynamicSurfaceLastFarCandidateTilesEvaluated = stats.lastFarCandidateTilesEvaluated;

    m_frameStats.dynamicSurfaceOptionalDispatchGlMs = stats.optionalTileDispatchGlMs;
    m_frameStats.dynamicSurfaceOptionalCopyGlMs = stats.optionalTileCopyGlMs;
    m_frameStats.dynamicSurfaceOptionalBarrierGlMs = stats.optionalTileBarrierGlMs;
    m_frameStats.dynamicSurfaceTireEventSetupGlMs = stats.tireEventSetupGlMs;
    m_frameStats.dynamicSurfaceTireEventUniformGlMs = stats.tireEventUniformGlMs;
    m_frameStats.dynamicSurfaceTireEventDispatchGlMs = stats.tireEventDispatchGlMs;
    m_frameStats.dynamicSurfaceTireEventSlowestDispatchGlMs = stats.tireEventSlowestDispatchGlMs;
    m_frameStats.dynamicSurfaceTireEventBarrierGlMs = stats.tireEventBarrierGlMs;
    m_frameStats.dynamicSurfaceTireReadbackWaitGlMs = stats.tireReadbackClientWaitGlMs;
    m_frameStats.dynamicSurfaceTireReadbackMapGlMs = stats.tireReadbackMapGlMs;
    m_frameStats.dynamicSurfaceTireReadbackUnmapGlMs = stats.tireReadbackUnmapGlMs;
    m_frameStats.dynamicSurfaceTireWaterUploadGlMs = stats.tireWaterUploadGlMs;
    m_frameStats.dynamicSurfaceTireWaterSetupGlMs = stats.tireWaterSetupGlMs;
    m_frameStats.dynamicSurfaceTireWaterDispatchGlMs = stats.tireWaterDispatchGlMs;
    m_frameStats.dynamicSurfaceTireWaterBarrierGlMs = stats.tireWaterBarrierGlMs;
    m_frameStats.dynamicSurfaceTireWaterFenceGlMs = stats.tireWaterFenceGlMs;

    m_frameStats.dynamicSurfaceGpuRuntimeCommittedMiB = stats.committedMiB;
    m_frameStats.dynamicSurfaceGpuRuntimeDispatches = stats.dispatchesThisFrame;
    m_frameStats.dynamicSurfaceGpuRuntimeCells = stats.cellsThisFrame;
    m_frameStats.dynamicSurfaceGpuGeometryValidTiles = stats.geometryValidTiles;
    m_frameStats.dynamicSurfaceGpuActiveTiles = stats.activeTiles;
    m_frameStats.dynamicSurfaceGpuResidentTiles = stats.residentTiles;
    m_frameStats.dynamicSurfaceGpuDesiredTopologyTiles = stats.desiredTopologyTiles;
    m_frameStats.dynamicSurfaceGpuVisibleTopologyTiles = stats.visibleTopologyTiles;
    m_frameStats.dynamicSurfaceGpuPrebakedTopologyTiles = stats.prebakedTopologyTiles;
    m_frameStats.dynamicSurfaceGpuFallbackTopologyTiles = stats.fallbackTopologyTiles;
    m_frameStats.dynamicSurfaceGpuTopologyUploadsThisFrame = stats.topologyUploadsThisFrame;
    m_frameStats.dynamicSurfaceGpuFarDesiredTopologyTiles = stats.farDesiredTopologyTiles;
    m_frameStats.dynamicSurfaceGpuFarResidentTopologyTiles = stats.farResidentTopologyTiles;
    m_frameStats.dynamicSurfaceGpuFarTopologyUploadsThisFrame = stats.farTopologyUploadsThisFrame;
    m_frameStats.dynamicSurfaceGpuFarTopologyBacklogTiles = stats.farTopologyBacklogTiles;
    m_frameStats.dynamicSurfaceGpuPrewarmTiles = stats.prewarmTiles;
    m_frameStats.dynamicSurfaceGpuDueTiles = stats.dueTiles;
    m_frameStats.dynamicSurfaceGpuBacklogTiles = stats.dispatchBacklogTiles;
    m_frameStats.dynamicSurfaceGpuCameraSpeedMps = stats.cameraSpeedMps;
    m_frameStats.dynamicSurfaceGpuPrewarmDistanceM = stats.predictivePrewarmM;
    m_frameStats.dynamicSurfaceGpuRainMmPerHour = stats.precipitationRateMmPerHour;
    m_frameStats.dynamicSurfaceGpuRunoffDriverMmPerHour = stats.runoffDriverMmPerHour;
    m_frameStats.dynamicSurfaceGpuDrainageMmPerHour = stats.drainageRateMmPerHour;
    m_frameStats.dynamicSurfaceGpuEvaporationMmPerHour = stats.evaporationRateMmPerHour;
    m_frameStats.dynamicSurfaceGpuBackgroundSeedDepthM = stats.backgroundSeedDepthM;
    m_frameStats.dynamicSurfaceGpuSurfaceWettingExposureM = stats.surfaceWettingExposureM;
    m_frameStats.dynamicSurfaceGpuRuntimeTireEventDispatches = stats.tireEventDispatches;
    m_frameStats.dynamicSurfaceGpuRuntimeTireEventCells = stats.tireEventCells;
    m_frameStats.dynamicSurfaceGpuRuntimeTireWaterSampleDispatches = stats.tireWaterSampleDispatches;
    m_frameStats.dynamicSurfaceGpuRuntimeTireWaterSamplesCompleted = stats.tireWaterSamplesCompleted;
    m_frameStats.dynamicSurfaceGpuRuntimeTireWaterSampleReadbackDrops = stats.tireWaterSampleReadbackDrops;
    m_frameStats.dynamicSurfaceGpuRuntimeCenterTileX = stats.centerTileX;
    m_frameStats.dynamicSurfaceGpuRuntimeCenterTileZ = stats.centerTileZ;
    m_frameStats.dynamicSurfaceGpuRuntimeCameraTileRebases = stats.cameraTileRebases;
    m_frameStats.dynamicSurfaceGpuExactGeometryReady = stats.exactGeometrySupportReady;
    m_frameStats.dynamicSurfaceGpuGeometryTriangles = stats.geometryTriangles;
    m_frameStats.dynamicSurfaceGpuGeometryBinReferences = stats.geometryBinReferences;
    m_frameStats.dynamicSurfaceGpuGeometryUploadMiB = stats.geometryUploadMiB;
}

} // namespace heritage::graphics

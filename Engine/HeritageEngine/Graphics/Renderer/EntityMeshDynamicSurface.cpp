#include "EntityMeshRenderer.hpp"

#include "../../Physics/Surfaces/SurfaceWorld.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace heritage::graphics {

bool EntityMeshRenderer::initializeDynamicSurfacePageResources()
{
    // LIVETRACK04: this legacy persistent-page GPU pool remains for the
    // 64x64 Track/rubber/temperature mirror. High-resolution water no longer
    // comes from this CPU-rasterized Hydro array once GPU Hydro authority is
    // ready; the dedicated 10m/256x256 GPU atlas owns water simulation/rendering.
    const std::size_t requestedPages =
        heritage::physics::dynamicsurface::DynamicSurfacePagePool::
            kDefaultBudgetBytes
        / heritage::physics::dynamicsurface::
            kBytesPerPhysicalPageWithMipChain;

    std::string errorMessage;
    if (m_dynamicSurfaceGpuPagePool.initialize(requestedPages, errorMessage))
        return true;

    std::cerr
        << "Heritage renderer warning: Dynamic Surface GPU page pool "
        << "unavailable: " << errorMessage << '\n';
    return false;
}

void EntityMeshRenderer::shutdownDynamicSurfacePageResources()
{
    m_dynamicSurfaceGpuPagePool.shutdown();
}

void EntityMeshRenderer::synchronizeDynamicSurfacePageResources(
    const heritage::physics::SurfaceWorld* surfaceWorld)
{
    if (!surfaceWorld || !m_dynamicSurfaceGpuPagePool.ready())
        return;

    std::string errorMessage;
    if (!m_dynamicSurfaceGpuPagePool.synchronize(
            surfaceWorld->dynamicSurface().pagePool(), errorMessage)
        && !errorMessage.empty())
    {
        // DSURF02 remains non-fatal while no simulation authority has migrated
        // yet. DSURF03 will surface missing state more aggressively.
        static bool reportedFailure = false;
        if (!reportedFailure)
        {
            std::cerr
                << "Dynamic Surface GPU synchronization warning: "
                << errorMessage << '\n';
            reportedFailure = true;
        }
    }

    const auto& pageStats = m_dynamicSurfaceGpuPagePool.stats();
    m_frameStats.dynamicSurfacePageSyncCpuMs += pageStats.synchronizationCpuMs;
    m_frameStats.dynamicSurfaceResidentPages = pageStats.residentPages;
    m_frameStats.dynamicSurfaceDirtyPages = pageStats.dirtyPages;
    m_frameStats.dynamicSurfaceCapacityPages = pageStats.capacityPages;
    m_frameStats.dynamicSurfacePageTableGeneration = pageStats.pageTableGeneration;
    m_frameStats.dynamicSurfacePageTableUploads = pageStats.pageTableUploads;
    m_frameStats.dynamicSurfaceInitializedPages = pageStats.initializedPages;
    m_frameStats.dynamicSurfaceMipRegenerations = pageStats.mipRegenerations;
    m_frameStats.dynamicSurfaceCommittedMiB =
        static_cast<double>(pageStats.committedBytes) / (1024.0 * 1024.0);
    m_frameStats.dynamicSurfaceGpuReady = pageStats.ready;
}

void EntityMeshRenderer::initializeDynamicSurfaceGpuLodPrototype()
{
    std::string errorMessage;
    if (!m_dynamicSurfaceGpuLodPrototype.initialize(errorMessage))
    {
        std::cerr
            << "Heritage renderer warning: LIVETRACK07 GPU Hydro unavailable: "
            << errorMessage << '\n';
        return;
    }

    const auto& stats = m_dynamicSurfaceGpuLodPrototype.stats();
    std::cout
        << "LIVETRACK07 GPU Hydro: 10m tiles / 256x256 (3.90625cm), "
        << "100m-only distance cadence, bounded 20x20 atlas, one GL_LINEAR material lookup; "
        << stats.committedMiB << " MiB committed.\n";
}

void EntityMeshRenderer::shutdownDynamicSurfaceGpuLodPrototype()
{
    m_dynamicSurfaceGpuLodPrototype.shutdown();
}

void EntityMeshRenderer::updateDynamicSurfaceGpuLodPrototype(
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
    const double eventRadiusM =
        heritage::graphics::dynamicsurface::DynamicSurfaceGpuLodPrototype::simulationRadiusM()
        + 0.5 * heritage::graphics::dynamicsurface::DynamicSurfaceGpuLodPrototype::tileWorldSizeM();
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
        heritage::graphics::dynamicsurface::DynamicSurfaceGpuLodPrototype::
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

    const auto weatherOutput = surfaceWorld->weatherOutput();
    m_dynamicSurfaceGpuLodPrototype.update(
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
        &surfaceWorld->dynamicSurface(),
        gpuTireEvents);

    const auto& stats = m_dynamicSurfaceGpuLodPrototype.stats();
    mutableSurfaceWorld->setGpuDynamicSurfaceAuthorityEnabled(
        m_dynamicSurfaceGpuLodPrototype.authoritativeReady());
    m_frameStats.dynamicSurfaceGpuLodPrototypeReady = stats.ready;
    m_frameStats.dynamicSurfaceGpuLodWaterReady = stats.waterReady;
    m_frameStats.dynamicSurfaceGpuLodSnowReady = stats.snowReady;
    m_frameStats.dynamicSurfaceGpuLodMudReady = stats.mudReady;
    m_frameStats.dynamicSurfaceGpuWaterPresentationReady = stats.waterPresentationReady;
    m_frameStats.dynamicSurfaceGpuWaterPresentationMiB = stats.waterPresentationMiB;
    m_frameStats.dynamicSurfaceGpuWaterPresentationDispatches =
        stats.waterPresentationDispatchesThisFrame;
    m_frameStats.dynamicSurfaceGpuLodCpuMs = stats.cpuDispatchMs;
    m_frameStats.dynamicSurfaceGpuLodGpuMs = stats.gpuComputeMs;
    m_frameStats.dynamicSurfaceGpuLodCommittedMiB = stats.committedMiB;
    m_frameStats.dynamicSurfaceGpuLodDispatches = stats.dispatchesThisFrame;
    m_frameStats.dynamicSurfaceGpuLodCells = stats.cellsThisFrame;
    m_frameStats.dynamicSurfaceGpuLodWaterCycles = stats.waterPublishedCycles;
    m_frameStats.dynamicSurfaceGpuLodWaterDispatches = stats.waterLodDispatches;
    m_frameStats.dynamicSurfaceGpuLodWaterPublishedCycles = stats.waterLodPublishedCycles;
    m_frameStats.dynamicSurfaceGpuGeometryValidTiles = stats.geometryValidTiles;
    m_frameStats.dynamicSurfaceGpuActiveTiles = stats.activeTiles;
    m_frameStats.dynamicSurfaceGpuResidentTiles = stats.residentTiles;
    m_frameStats.dynamicSurfaceGpuPrewarmTiles = stats.prewarmTiles;
    m_frameStats.dynamicSurfaceGpuWorldTiles = stats.worldTiles;
    m_frameStats.dynamicSurfaceGpuWorldTileDispatches = stats.worldTileDispatches;
    m_frameStats.dynamicSurfaceGpuWorldTileStateMiB = stats.worldTileStateMiB;
    m_frameStats.dynamicSurfaceGpuDueTiles = stats.dueTiles;
    m_frameStats.dynamicSurfaceGpuBacklogTiles = stats.dispatchBacklogTiles;
    m_frameStats.dynamicSurfaceGpuCameraSpeedMps = stats.cameraSpeedMps;
    m_frameStats.dynamicSurfaceGpuPrewarmDistanceM = stats.predictivePrewarmM;
    m_frameStats.dynamicSurfaceGpuRainMmPerHour = stats.precipitationRateMmPerHour;
    m_frameStats.dynamicSurfaceGpuDrainageMmPerHour = stats.drainageRateMmPerHour;
    m_frameStats.dynamicSurfaceGpuEvaporationMmPerHour = stats.evaporationRateMmPerHour;
    m_frameStats.dynamicSurfaceGpuBackgroundSeedDepthM = stats.backgroundSeedDepthM;
    m_frameStats.dynamicSurfaceGpuWaterProbeValid = stats.waterProbeValid;
    m_frameStats.dynamicSurfaceGpuWaterProbeWetTexels = stats.waterProbeWetTexels;
    m_frameStats.dynamicSurfaceGpuWaterProbeTexels = stats.waterProbeTexels;
    m_frameStats.dynamicSurfaceGpuWaterProbeMeanDepthM = stats.waterProbeMeanDepthM;
    m_frameStats.dynamicSurfaceGpuWaterProbeMaximumDepthM = stats.waterProbeMaximumDepthM;
    m_frameStats.dynamicSurfaceGpuLodTireEventDispatches = stats.tireEventDispatches;
    m_frameStats.dynamicSurfaceGpuLodTireEventCells = stats.tireEventCells;
    m_frameStats.dynamicSurfaceGpuLodCenterTileX = stats.centerTileX;
    m_frameStats.dynamicSurfaceGpuLodCenterTileZ = stats.centerTileZ;
    m_frameStats.dynamicSurfaceGpuLodCameraTileRebases = stats.cameraTileRebases;
    m_frameStats.dynamicSurfaceGpuExactGeometryReady = stats.exactGeometrySupportReady;
    m_frameStats.dynamicSurfaceGpuGeometryTriangles = stats.geometryTriangles;
    m_frameStats.dynamicSurfaceGpuGeometryBinReferences = stats.geometryBinReferences;
    m_frameStats.dynamicSurfaceGpuGeometryUploadMiB = stats.geometryUploadMiB;
}

} // namespace heritage::graphics

#include "PhysicsRegressionCommon.hpp"

#include <cmath>
#include <filesystem>
#include <vector>

#include "../Physics/Surfaces/DynamicSurface/DynamicSurfaceSystem.hpp"
#include "../Physics/Surfaces/SurfaceMaterialProperties.hpp"

namespace heritage::tests {

namespace {

using heritage::physics::StaticSceneTriangle;
using heritage::physics::SurfaceMaterial;
using heritage::physics::dynamicsurface::ChunkAddress;
using heritage::physics::dynamicsurface::DynamicSurfaceStaticBakeReport;
using heritage::physics::dynamicsurface::DynamicSurfaceSystem;
using heritage::physics::dynamicsurface::StaticSurfaceBarrierKind;

void appendQuad(
    std::vector<StaticSceneTriangle>& triangles,
    float minimumX,
    float maximumX,
    float minimumZ,
    float maximumZ,
    float y,
    SurfaceMaterial material,
    double drainageMmPerHour = 0.0)
{
    auto properties = heritage::physics::defaultSurfaceMaterialProperties(material);
    properties.hydrology.drainageCapacityMmPerHour = drainageMmPerHour;
    properties.hydrology.authored = drainageMmPerHour > 0.0;

    StaticSceneTriangle a;
    a.a = { minimumX, y, minimumZ };
    a.b = { maximumX, y, minimumZ };
    a.c = { maximumX, y, maximumZ };
    a.normal = { 0.0f, 1.0f, 0.0f };
    a.surfaceMaterial = material;
    a.surfaceProperties = properties;

    StaticSceneTriangle b = a;
    b.a = { minimumX, y, minimumZ };
    b.b = { maximumX, y, maximumZ };
    b.c = { minimumX, y, maximumZ };
    triangles.push_back(a);
    triangles.push_back(b);
}

} // namespace

bool dynamicSurfaceStaticBakeSeparatesSheetsAndCaches()
{
    // One 20m road strip crosses the x=100m chunk cut. The artificial cut must
    // become a cross-chunk sheet link, not a hard barrier.
    std::vector<StaticSceneTriangle> spanningRoad;
    appendQuad(
        spanningRoad,
        90.0f,
        110.0f,
        0.0f,
        8.0f,
        0.0f,
        SurfaceMaterial::Asphalt);

    DynamicSurfaceSystem spanningSystem;
    DynamicSurfaceStaticBakeReport spanningReport;
    if (!spanningSystem.bakeStaticScene(
            spanningRoad, { 0.0, 0.0, 0.0 }, spanningReport)
        || spanningReport.chunkCount != 2u
        || spanningReport.surfaceSheetCount != 2u
        || spanningReport.crossChunkSheetLinkCount == 0u
        || spanningSystem.findChunk({ 0, 0 }) == nullptr
        || spanningSystem.findChunk({ 1, 0 }) == nullptr)
    {
        return false;
    }

    // A road and bridge occupy the same X/Z footprint but differ by 5m in Y.
    // Because sheet connectivity uses shared 3D manifold edges, the two layers
    // must remain independently paintable even though their UV/world footprint
    // is identical.
    std::vector<StaticSceneTriangle> stacked;
    appendQuad(
        stacked,
        10.0f,
        30.0f,
        10.0f,
        30.0f,
        0.0f,
        SurfaceMaterial::Asphalt);
    appendQuad(
        stacked,
        10.0f,
        30.0f,
        10.0f,
        30.0f,
        5.0f,
        SurfaceMaterial::Asphalt);

    DynamicSurfaceSystem stackedSystem;
    DynamicSurfaceStaticBakeReport stackedReport;
    if (!stackedSystem.bakeStaticScene(
            stacked, { 0.0, 0.0, 0.0 }, stackedReport))
    {
        return false;
    }
    const auto* stackedChunk = stackedSystem.findChunk({ 0, 0 });
    if (!stackedChunk
        || stackedChunk->staticSheets().size() != 2u
        || stackedChunk->staticTriangles().size() != 4u)
    {
        return false;
    }
    const double heightDelta = std::abs(
        stackedChunk->staticSheets()[1].minimum.y
        - stackedChunk->staticSheets()[0].minimum.y);
    if (heightDelta < 4.9)
        return false;

    // A 15cm sidewalk step is deliberately two disconnected upward-facing
    // manifolds. The rejected vertical face therefore cannot silently connect
    // the road's Dynamic Surface page to the sidewalk's page.
    std::vector<StaticSceneTriangle> curb;
    appendQuad(
        curb,
        0.0f,
        5.0f,
        40.0f,
        50.0f,
        0.0f,
        SurfaceMaterial::Asphalt,
        0.0);
    appendQuad(
        curb,
        5.0f,
        10.0f,
        40.0f,
        50.0f,
        0.15f,
        SurfaceMaterial::Default,
        250.0);

    DynamicSurfaceSystem curbSystem;
    DynamicSurfaceStaticBakeReport curbReport;
    if (!curbSystem.bakeStaticScene(curb, { 0.0, 0.0, 0.0 }, curbReport))
        return false;
    const auto* curbChunk = curbSystem.findChunk({ 0, 0 });
    if (!curbChunk
        || curbChunk->staticSheets().size() != 2u
        || curbChunk->staticDrains().empty())
    {
        return false;
    }
    std::size_t hardBarrierCount = 0;
    for (const auto& barrier : curbChunk->staticBarriers())
    {
        if (barrier.kind == StaticSurfaceBarrierKind::HardGeometry)
            ++hardBarrierCount;
    }
    if (hardBarrierCount == 0u)
        return false;

    // Deterministic static cache: the second system must restore identical
    // sheet/patch topology and seeds without rebaking the scene.
    const std::filesystem::path cachePath =
        std::filesystem::temp_directory_path()
        / "heritage_dsurf01_regression.hdsurf";
    std::error_code error;
    std::filesystem::remove(cachePath, error);

    DynamicSurfaceSystem cacheWriter;
    DynamicSurfaceStaticBakeReport firstReport;
    if (!cacheWriter.loadOrBakeStaticScene(
            curb, { 1234.0, 10.0, -4321.0 }, cachePath, firstReport)
        || firstReport.loadedFromCache)
    {
        std::filesystem::remove(cachePath, error);
        return false;
    }
    const auto* firstChunk = cacheWriter.findChunk({ 12, -43 });
    if (!firstChunk || firstChunk->staticSheets().empty())
    {
        std::filesystem::remove(cachePath, error);
        return false;
    }
    const std::uint64_t expectedSeed = firstChunk->staticSheets()[0].microtopographySeed;

    DynamicSurfaceSystem cacheReader;
    DynamicSurfaceStaticBakeReport secondReport;
    const bool cacheLoaded = cacheReader.loadOrBakeStaticScene(
        curb, { 1234.0, 10.0, -4321.0 }, cachePath, secondReport);
    std::filesystem::remove(cachePath, error);
    const auto* secondChunk = cacheReader.findChunk({ 12, -43 });
    if (!cacheLoaded
        || !secondReport.loadedFromCache
        || !secondChunk
        || secondChunk->staticSheets().empty()
        || secondChunk->staticSheets()[0].microtopographySeed != expectedSeed
        || secondReport.clippedPatchTriangleCount
            != firstReport.clippedPatchTriangleCount)
    {
        return false;
    }

    return true;
}

bool dynamicSurfacePagePoolIsPersistentBudgetedAndLruSafe()
{
    using namespace heritage::physics::dynamicsurface;

    std::vector<StaticSceneTriangle> scene;
    appendQuad(
        scene,
        0.0f,
        20.0f,
        0.0f,
        20.0f,
        0.0f,
        SurfaceMaterial::Asphalt);
    appendQuad(scene, 100.0f, 120.0f, 0.0f, 20.0f, 0.0f, SurfaceMaterial::Asphalt);
    appendQuad(scene, 200.0f, 220.0f, 0.0f, 20.0f, 0.0f, SurfaceMaterial::Asphalt);

    DynamicSurfaceSystem system;
    DynamicSurfaceStaticBakeReport report;
    if (!system.bakeStaticScene(scene, { 0.0, 0.0, 0.0 }, report))
        return false;

    // Two physical slots are enough to prove stable identity + clean-page LRU.
    system.pagePool().configure(2u * kBytesPerPhysicalPageWithMipChain, 2u);
    const VirtualPageAddress a{ { 0, 0 }, { 0, 0, 0 } };
    const VirtualPageAddress b{ { 1, 0 }, { 0, 0, 0 } };
    const VirtualPageAddress c{ { 2, 0 }, { 0, 0, 0 } };

    const auto firstA = system.ensurePageResident(a);
    const auto secondA = system.ensurePageResident(a);
    if (!firstA || !secondA
        || firstA->physicalSlot != secondA->physicalSlot
        || firstA->generation != secondA->generation)
    {
        return false;
    }
    if (!system.pagePool().acknowledgeClean(a))
        return false;

    const auto firstB = system.ensurePageResident(b);
    if (!firstB || !system.pagePool().acknowledgeClean(b))
        return false;

    // Touch B after A so A is the oldest clean/unpinned page.
    if (!system.ensurePageResident(b))
        return false;
    const auto firstC = system.ensurePageResident(c);
    if (!firstC || system.pagePool().find(a) != nullptr
        || system.pagePool().find(b) == nullptr
        || system.pagePool().find(c) == nullptr)
    {
        return false;
    }

    const auto telemetry = system.pagePool().telemetry();
    if (telemetry.capacityPages != 2u
        || telemetry.residentPages != 2u
        || telemetry.evictionCount != 1u
        || telemetry.committedBytes
            != 2u * kBytesPerPhysicalPageWithMipChain)
    {
        return false;
    }

    // Dirty state is never silently evicted. This is crucial once DSURF03+
    // starts holding authoritative water/rubber/temperature in these pages.
    DynamicSurfacePagePool dirtyProtected;
    dirtyProtected.configure(kBytesPerPhysicalPageWithMipChain, 1u);
    const auto dirtyA = dirtyProtected.ensureResident(a);
    if (!dirtyA || !dirtyProtected.markDirty(a, PagePlaneMask::Hydro)
        || dirtyProtected.ensureResident(b).has_value())
    {
        return false;
    }
    if (!dirtyProtected.acknowledgeClean(a, PagePlaneMask::Hydro))
        return false;
    const auto replacement = dirtyProtected.ensureResident(b);
    if (!replacement || replacement->physicalSlot != dirtyA->physicalSlot
        || replacement->generation == dirtyA->generation)
    {
        return false;
    }

    // Pinning independently protects a page even after it becomes clean.
    DynamicSurfacePagePool pinnedProtected;
    pinnedProtected.configure(kBytesPerPhysicalPageWithMipChain, 1u);
    const auto pinnedA = pinnedProtected.ensureResident(a, true);
    if (!pinnedA || !pinnedProtected.acknowledgeClean(a)
        || pinnedProtected.ensureResident(b).has_value())
    {
        return false;
    }
    if (!pinnedProtected.setPinned(a, false))
        return false;
    if (!pinnedProtected.ensureResident(b))
        return false;

    // A scene reload clears residency but must not recycle the same generation
    // number, or the GPU mirror could mistake stale physical contents as valid.
    DynamicSurfacePagePool reloadSafe;
    reloadSafe.configure(kBytesPerPhysicalPageWithMipChain, 1u);
    const auto beforeClear = reloadSafe.ensureResident(a);
    if (!beforeClear)
        return false;
    reloadSafe.clear();
    const auto afterClear = reloadSafe.ensureResident(a);
    if (!afterClear || afterClear->generation == beforeClear->generation)
        return false;

    return true;
}

bool dynamicSurfaceHydroResidencyUsesRealSurfacePagesAndNearestSources()
{
    using namespace heritage::physics::dynamicsurface;

    std::vector<StaticSceneTriangle> scene;
    appendQuad(scene, 0.0f, 40.0f, 0.0f, 20.0f, 0.0f, SurfaceMaterial::Asphalt);
    appendQuad(scene, 260.0f, 300.0f, 0.0f, 20.0f, 0.0f, SurfaceMaterial::Asphalt);

    DynamicSurfaceSystem system;
    DynamicSurfaceStaticBakeReport report;
    if (!system.bakeStaticScene(scene, { 0.0, 0.0, 0.0 }, report))
        return false;

    system.pagePool().configure(
        1u * kBytesPerPhysicalPageWithMipChain,
        1u);
    system.setInterestSources({
        { 10.0, 0.0, 10.0 },
        { 290.0, 0.0, 10.0 } });
    system.refreshHydroResidency();

    const auto& migration = system.hydroMigrationStats();
    const auto resident = system.pagePool().residentAssignments();
    if (migration.candidatePages == 0u
        || migration.residentHydroPages == 0u
        || resident.empty()
        || resident.size() > 1u)
    {
        return false;
    }

    // Every resident page must correspond to DSURF01-covered geometry. No
    // camera-centred empty-square allocation is allowed.
    for (const auto& assignment : resident)
    {
        const auto* chunk = system.findChunk(assignment.virtualAddress.chunk);
        if (!chunk)
            return false;
        const auto& covered = chunk->coveredPages();
        if (std::find(covered.begin(), covered.end(), assignment.virtualAddress.page)
            == covered.end())
        {
            return false;
        }
    }

    // The synthetic midpoint lies around x=150m and contains no surface at all;
    // two real sources must never fabricate residency there.
    for (const auto& assignment : resident)
    {
        const double pageCenterX =
            static_cast<double>(assignment.virtualAddress.chunk.x) * kChunkSizeM
            + (static_cast<double>(assignment.virtualAddress.page.x) + 0.5)
                * kPhysicalPageWorldSizeM;
        if (pageCenterX > 120.0 && pageCenterX < 200.0)
            return false;
    }

    // LIVETRACK03: GPU residency remains bounded to nearby real surfaces, while
    // Hydro authority can materialize beyond the render working set. Near Hydro
    // pages run at 6Hz and distant pages persist at the one-per-minute cadence;
    // Track thermal remains bounded to residency.
    heritage::physics::SurfaceWeatherDescription weather;
    weather.enabled = true;
    heritage::physics::SurfaceWeatherOutput output;
    output.valid = true;
    system.advanceHydro(weather, output, 1.0 / 30.0);
    system.advanceThermal(weather, output, 20.0, false, 20.0, 1.0 / 30.0);
    if (system.hydroStats().authorityPages <= resident.size()
        || system.hydroStats().activePages != system.hydroStats().authorityPages
        || system.thermalStats().authorityPages > resident.size()
        || system.thermalStats().activePages > resident.size())
    {
        return false;
    }

    return true;
}


bool dynamicSurfaceHydrologyConservesCappedVolume()
{
    using namespace heritage::physics::dynamicsurface;

    // Near-full water on a continuous slope is an intentionally hostile case:
    // internal shallow-flow transfers must be limited by receiver capacity or
    // the donor loses mass when the receiver hits the 0.032 m LIVETRACK03 water ceiling.
    const auto iceProperties =
        heritage::physics::defaultSurfaceMaterialProperties(SurfaceMaterial::Ice);
    std::vector<StaticSceneTriangle> scene;
    StaticSceneTriangle first;
    first.a = { 0.0f, 1.0f, 0.0f };
    first.b = { 12.0f, 0.0f, 12.0f };
    first.c = { 12.0f, 1.0f, 0.0f };
    first.normal = { 0.0f, 0.9965458f, 0.0830455f };
    first.surfaceMaterial = SurfaceMaterial::Ice;
    first.surfaceProperties = iceProperties;
    scene.push_back(first);

    StaticSceneTriangle second = first;
    second.a = { 0.0f, 1.0f, 0.0f };
    second.b = { 0.0f, 0.0f, 12.0f };
    second.c = { 12.0f, 0.0f, 12.0f };
    scene.push_back(second);

    DynamicSurfaceSystem system;
    DynamicSurfaceStaticBakeReport report;
    if (!system.bakeStaticScene(scene, { 0.0, 0.0, 0.0 }, report))
        return false;
    system.setInterestSources({ { 6.0, 0.5, 6.0 } });

    heritage::physics::SurfaceWeatherDescription weather;
    weather.enabled = true;
    heritage::physics::SurfaceWeatherOutput dry;
    dry.valid = true;

    // Materialize the authoritative pages before the lab fill.
    system.advanceHydro(weather, dry, 0.5);
    if (!system.setUniformHydroDepthForLab(0.0305))
        return false;
    const double beforeFlowM3 = system.hydroStats().waterVolumeM3;
    if (!(beforeFlowM3 > 0.0))
        return false;

    for (int i = 0; i < 4; ++i)
        system.advanceHydro(weather, dry, 0.5);
    const auto& afterFlowStats = system.hydroStats();
    const double afterFlowM3 = afterFlowStats.waterVolumeM3;
    const double flowToleranceM3 = std::max(1.0e-7, beforeFlowM3 * 2.0e-6);
    if (std::abs(afterFlowM3 - beforeFlowM3) > flowToleranceM3
        || afterFlowStats.maximumFlowSpeedMps <= 0.0)
    {
        return false;
    }

    // Saturating rainfall must account only for water that actually fits. A
    // clamp may not silently report/request more mass than entered the field.
    const double beforeRainM3 = system.hydroStats().waterVolumeM3;
    const double cumulativeRainBeforeM3 =
        system.hydroStats().cumulativeRainVolumeM3;
    heritage::physics::SurfaceWeatherOutput storm = dry;
    storm.precipitationRateMmPerHour = 360000.0;
    system.advanceHydro(weather, storm, 0.5);
    const auto& stats = system.hydroStats();
    const double actualRainGainM3 = stats.waterVolumeM3 - beforeRainM3;
    const double accountedRainGainM3 =
        stats.cumulativeRainVolumeM3 - cumulativeRainBeforeM3;
    const double rainToleranceM3 = std::max(2.0e-6, std::abs(actualRainGainM3) * 2.0e-4);
    return actualRainGainM3 >= -rainToleranceM3
        && std::abs(actualRainGainM3 - accountedRainGainM3) <= rainToleranceM3;
}

bool dynamicSurfaceHydrologyOwnsRainCoverAndTireClearing()
{
    using namespace heritage::physics::dynamicsurface;

    // LIVETRACK03 intentionally removes Hydro surface-sheet ownership. Stacked
    // surfaces at the same X/Z therefore share one 256x256 water field. This is
    // deliberate: no bridge/road duplicate water pages.
    std::vector<StaticSceneTriangle> scene;
    appendQuad(
        scene,
        0.0f, 12.0f,
        0.0f, 12.0f,
        0.0f,
        SurfaceMaterial::Asphalt);
    appendQuad(
        scene,
        0.0f, 12.0f,
        0.0f, 12.0f,
        5.0f,
        SurfaceMaterial::Asphalt);

    DynamicSurfaceSystem system;
    DynamicSurfaceStaticBakeReport report;
    if (!system.bakeStaticScene(scene, { 0.0, 0.0, 0.0 }, report))
        return false;

    system.setInterestSources({ { 6.0, 5.0, 6.0 } });

    heritage::physics::SurfaceWeatherDescription weather;
    weather.enabled = true;
    weather.precipitationRateMmPerHour = 3600.0;
    weather.drainageRateMmPerHour = 0.0;
    weather.referenceEvaporationRateMmPerHour = 0.0;

    heritage::physics::SurfaceWeatherOutput output;
    output.valid = true;
    output.precipitationRateMmPerHour = 3600.0;
    output.drainageRateMmPerHour = 0.0;
    output.evaporationRateMmPerHour = 0.0;
    output.effectiveWetness = 0.0;
    output.waterFilmDepthM = 0.0;

    for (int i = 0; i < 4; ++i)
        system.advanceHydro(weather, output, 0.5);

    const DynamicSurfaceHydroSample lower =
        system.sampleHydro({ 6.0, 0.0, 6.0 });
    const DynamicSurfaceHydroSample upper =
        system.sampleHydro({ 6.0, 5.0, 6.0 });
    if (!lower.valid || !upper.valid
        || lower.waterDepthM <= 2.0e-5
        || upper.waterDepthM <= 2.0e-5
        || std::abs(lower.waterDepthM - upper.waterDepthM) > 1.0e-9
        || lower.surfaceSheetId != 0u
        || upper.surfaceSheetId != 0u
        || system.hydroStats().simulationStepCount == 0u)
    {
        return false;
    }

    DynamicSurfaceHydroTireInput tire;
    tire.deltaTimeSeconds = 0.01;
    tire.contactPatchWidthM = 0.20;
    tire.normalLoadN = 3500.0;
    tire.nominalLoadN = 3500.0;
    tire.forwardSpeedMps = 20.0;
    tire.treadVoidRatio = 0.35;
    tire.forward = { 1.0f, 0.0f, 0.0f };
    DynamicSurfaceHydroTireResult tireResult{};
    double totalRemovedVolumeM3 = 0.0;
    double firstPublicDepthM = -1.0;
    for (int pass = 0; pass < 800; ++pass)
    {
        tireResult = system.applyHydroTireContact({ 6.0, 5.0, 6.0 }, tire);
        if (!tireResult.valid)
            return false;
        if (pass == 0)
            firstPublicDepthM = tireResult.initialWaterDepthM;
        totalRemovedVolumeM3 += tireResult.removedVolumeM3;
    }
    const auto tireClearedUpper = system.sampleHydro({ 6.0, 5.0, 6.0 });
    const auto tireClearedLower = system.sampleHydro({ 6.0, 0.0, 6.0 });
    if (!(firstPublicDepthM > 0.0)
        || totalRemovedVolumeM3 <= 0.0
        || !tireClearedUpper.valid
        || !tireClearedLower.valid
        || tireClearedUpper.waterDepthM >= firstPublicDepthM
        || std::abs(tireClearedUpper.waterDepthM - tireClearedLower.waterDepthM) > 1.0e-9)
    {
        return false;
    }

    // The renderer API exposes exactly one canonical X/Z Hydro page per chunk.
    // Sheet zero is only the page-pool carrier address; water itself has no
    // vertical sheet identity.
    const auto* chunk = system.findChunk({ 0, 0 });
    if (!chunk || chunk->staticTriangles().empty())
        return false;
    const VirtualPageAddress hydroPage{ { 0, 0 }, { 0u, 0u, 0u } };

    std::vector<std::uint16_t> hydroRgba4;
    if (!system.rasterHydroPage(
            hydroPage, kHydroAuthorityResolution, hydroRgba4)
        || hydroRgba4.size() != static_cast<std::size_t>(kHydroAuthorityResolution)
            * kHydroAuthorityResolution)
    {
        return false;
    }
    // Same-resolution contract: renderer-facing raster is the exact packed
    // authority; a lower-resolution request must not silently create another
    // Hydro representation.
    std::vector<std::uint16_t> wrongResolutionHydro;
    if (system.rasterHydroPage(hydroPage, 64u, wrongResolutionHydro))
        return false;


    return true;
}

bool dynamicSurfaceThermalIsSheetAwareAndTireHeated()
{
    using namespace heritage::physics::dynamicsurface;

    std::vector<StaticSceneTriangle> scene;
    appendQuad(
        scene,
        0.0f, 12.0f,
        0.0f, 12.0f,
        0.0f,
        SurfaceMaterial::Asphalt);
    scene[0].surfaceProperties.hasAuthoredSurfaceTemperature = true;
    scene[0].surfaceProperties.authoredSurfaceTemperatureC = 20.0;
    scene[1].surfaceProperties = scene[0].surfaceProperties;
    appendQuad(
        scene,
        0.0f, 12.0f,
        0.0f, 12.0f,
        5.0f,
        SurfaceMaterial::Asphalt);
    scene[2].surfaceProperties.hasAuthoredSurfaceTemperature = true;
    scene[2].surfaceProperties.authoredSurfaceTemperatureC = 20.0;
    scene[3].surfaceProperties = scene[2].surfaceProperties;

    DynamicSurfaceSystem system;
    DynamicSurfaceStaticBakeReport report;
    if (!system.bakeStaticScene(scene, { 0.0, 0.0, 0.0 }, report))
        return false;
    system.setInterestSources({ { 6.0, 5.0, 6.0 } });

    heritage::physics::SurfaceWeatherDescription weather;
    weather.enabled = true;
    weather.cloudCover = 0.0;
    weather.maximumSolarHeatingC = 24.0;
    weather.roadThermalTimeConstantSeconds = 10.0;
    weather.precipitationRateMmPerHour = 0.0;
    weather.referenceEvaporationRateMmPerHour = 0.0;

    heritage::physics::SurfaceWeatherOutput output;
    output.valid = true;
    output.effectiveWetness = 0.0;
    output.waterFilmDepthM = 0.0;
    output.precipitationRateMmPerHour = 0.0;
    output.evaporationRateMmPerHour = 0.0;

    for (int i = 0; i < 20; ++i)
        system.advanceThermal(weather, output, 10.0, false, 20.0, 0.5);

    const DynamicSurfaceThermalSample lower =
        system.sampleThermal({ 6.0, 0.0, 6.0 });
    const DynamicSurfaceThermalSample upper =
        system.sampleThermal({ 6.0, 5.0, 6.0 });
    if (!lower.valid || !upper.valid
        || upper.surfaceSheetId == lower.surfaceSheetId
        || upper.surfaceTemperatureC <= lower.surfaceTemperatureC + 8.0
        || system.thermalStats().simulationStepCount == 0u)
    {
        return false;
    }

    DynamicSurfaceThermalTireInput tire;
    tire.deltaTimeSeconds = 0.1;
    tire.contactPatchAreaM2 = 0.024;
    tire.slipDissipationWatts = 120000.0;
    const DynamicSurfaceThermalTireResult heated =
        system.applyThermalTireContact({ 6.0, 5.0, 6.0 }, tire);
    if (!heated.valid
        || heated.depositedHeatEnergyJ <= 0.0
        || heated.finalTemperatureC <= heated.initialTemperatureC)
    {
        return false;
    }

    const auto* chunk = system.findChunk({ 0, 0 });
    if (!chunk)
        return false;
    VirtualPageAddress upperPage{};
    bool foundUpperPage = false;
    for (const PageAddress& page : chunk->coveredPages())
    {
        if (page.sheet == upper.surfaceSheetId)
        {
            upperPage = { { 0, 0 }, page };
            foundUpperPage = true;
            break;
        }
    }
    if (!foundUpperPage)
        return false;

    std::vector<float> track;
    if (!system.rasterTrackPage(upperPage, 64u, track)
        || track.size() != 64u * 64u * 4u)
    {
        return false;
    }
    bool foundTemperature = false;
    // DSURF04 reserves Track G/B/A for DSURF05 rubber/marbles; thermal may
    // populate R only until that authority migration lands.
    for (std::size_t i = 0; i < track.size(); i += 4u)
    {
        foundTemperature = foundTemperature || track[i] > 0.0f;
        if (track[i + 1u] != 0.0f
            || track[i + 2u] != 0.0f
            || track[i + 3u] != 0.0f)
        {
            return false;
        }
    }
    return foundTemperature;
}

} // namespace heritage::tests

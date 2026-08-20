#include "PhysicsRegressionCommon.hpp"

#include "../Physics/Surfaces/SurfaceField.hpp"
#include "../Physics/Surfaces/SurfaceWorld.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

namespace heritage::tests {

bool surfaceWorldGlobalAddressingAndChunkCacheBehave()
{
    using heritage::math::DVec3;
    using heritage::math::Vec3;
    using heritage::physics::SurfaceField;
    using heritage::physics::SurfaceFieldChunkSnapshot;
    using heritage::physics::SurfaceFieldDescription;
    using heritage::physics::SurfaceFieldInitialState;
    using heritage::physics::SurfaceFieldUpdate;
    using heritage::physics::SurfaceMaterial;
    using heritage::physics::SurfaceWorld;
    using heritage::physics::SurfaceWorldEnvironment;
    using heritage::physics::SurfaceWeatherDescription;

    SurfaceWorld world;

    // TIRE15B live weather and local authored conditions share one world-owned
    // resolution path. Authored wetness combines with global rain, while road
    // temperature precedence is global override -> local authored -> material
    // family fallback.
    SurfaceWorldEnvironment climate;
    climate.wetness = 0.40;
    climate.ambientTemperatureC = 7.0;
    climate.surfaceTemperatureOverrideEnabled = false;
    climate.surfaceTemperatureC = 11.0;
    if (!world.setEnvironment(climate))
        return false;

    const auto asphaltDefaults =
        heritage::physics::defaultSurfaceMaterialProperties(SurfaceMaterial::Asphalt);
    const auto wetAsphalt = world.localConditions(
        { 0.0f, 0.0f, 0.0f }, SurfaceMaterial::Asphalt, 0.25, asphaltDefaults);
    if (std::abs(wetAsphalt.wetness - 0.55) > 1.0e-9
        || std::abs(wetAsphalt.ambientTemperatureC - 7.0) > 1.0e-9
        || std::abs(wetAsphalt.surfaceTemperatureC - 20.0) > 1.0e-9)
    {
        return false;
    }

    auto authoredAsphalt = asphaltDefaults;
    authoredAsphalt.hasAuthoredSurfaceTemperature = true;
    authoredAsphalt.authoredSurfaceTemperatureC = 32.0;
    const auto authoredConditions = world.localConditions(
        { 0.0f, 0.0f, 0.0f }, SurfaceMaterial::Asphalt, 0.0, authoredAsphalt);
    const auto frozenFallback = world.localConditions(
        { 0.0f, 0.0f, 0.0f }, SurfaceMaterial::Ice, 0.0,
        heritage::physics::defaultSurfaceMaterialProperties(SurfaceMaterial::Ice));
    if (std::abs(authoredConditions.surfaceTemperatureC - 32.0) > 1.0e-9
        || std::abs(frozenFallback.surfaceTemperatureC + 5.0) > 1.0e-9)
    {
        return false;
    }

    climate.surfaceTemperatureOverrideEnabled = true;
    climate.surfaceTemperatureC = 4.5;
    if (!world.setEnvironment(climate))
        return false;
    const auto overriddenConditions = world.localConditions(
        { 0.0f, 0.0f, 0.0f }, SurfaceMaterial::Ice, 0.0, authoredAsphalt);
    if (std::abs(overriddenConditions.surfaceTemperatureC - 4.5) > 1.0e-9)
        return false;

    SurfaceWorldEnvironment invalidClimate = climate;
    invalidClimate.wetness = 1.5;
    if (world.setEnvironment(invalidClimate))
        return false;

    // Executable global weather: rain must build a bounded hard-surface film,
    // feed the local tire conditions with its exact depth, then drain and
    // evaporate under dry wind. Reset must preserve configuration but remove
    // accumulated state.
    SurfaceWeatherDescription weather;
    weather.enabled = true;
    weather.precipitationRateMmPerHour = 30.0;
    weather.relativeHumidity = 0.90;
    weather.windSpeedMps = 7.0;
    weather.cloudCover = 1.0;
    weather.drainageRateMmPerHour = 3.0;
    weather.referenceEvaporationRateMmPerHour = 0.20;
    weather.maximumWaterFilmDepthM = 0.006;
    if (!world.setWeather(weather))
        return false;
    world.advancePresentation(60.0f);
    const auto rainy = world.weatherOutput();
    const auto rainyAsphalt = world.localConditions(
        { 0.0f, 0.0f, 0.0f }, SurfaceMaterial::Asphalt, 0.0,
        asphaltDefaults);
    const auto rainyGrass = world.localConditions(
        { 0.0f, 0.0f, 0.0f }, SurfaceMaterial::Grass, 0.0,
        heritage::physics::defaultSurfaceMaterialProperties(
            SurfaceMaterial::Grass));
    if (!rainy.valid || rainy.waterFilmDepthM <= 0.0001
        || rainy.waterFilmDepthM > weather.maximumWaterFilmDepthM
        || rainy.effectiveWetness <= 0.0
        || std::abs(rainyAsphalt.waterFilmDepthM - rainy.waterFilmDepthM)
            > 1.0e-12
        || rainyAsphalt.ambientAirSpeedMps != weather.windSpeedMps
        || rainyGrass.waterFilmDepthM != 0.0)
    {
        return false;
    }

    weather.precipitationRateMmPerHour = 0.0;
    weather.relativeHumidity = 0.10;
    weather.windSpeedMps = 25.0;
    weather.drainageRateMmPerHour = 30.0;
    weather.referenceEvaporationRateMmPerHour = 2.0;
    if (!world.setWeather(weather))
        return false;
    const double wetDepth = rainy.waterFilmDepthM;
    world.advancePresentation(600.0f);
    const auto drying = world.weatherOutput();
    if (!drying.valid || drying.waterFilmDepthM >= wetDepth)
        return false;
    world.resetWeatherState();
    if (world.weatherState().initialized
        || world.weatherOutput().waterFilmDepthM != 0.0)
    {
        return false;
    }

    // WEATHER02 spatial hydrology: a collision-baked depression must collect
    // more water than its raised neighbours, expose exact local depth to the
    // tire, and be dynamically cleared by whichever path a tire traverses.
    SurfaceWorld hydrologyWorld;
    std::vector<heritage::physics::StaticSceneTriangle> hydrologyTriangles;
    const auto appendTile = [&](float x0, float x1, float elevation) {
        heritage::physics::StaticSceneTriangle first;
        first.a = { x0, elevation, 0.0f };
        first.b = { x1, elevation, 0.0f };
        first.c = { x1, elevation, 0.5f };
        first.normal = { 0.0f, 1.0f, 0.0f };
        first.surfaceMaterial = SurfaceMaterial::Asphalt;
        first.surfaceProperties = asphaltDefaults;
        heritage::physics::StaticSceneTriangle second = first;
        second.a = { x0, elevation, 0.0f };
        second.b = { x1, elevation, 0.5f };
        second.c = { x0, elevation, 0.5f };
        hydrologyTriangles.push_back(first);
        hydrologyTriangles.push_back(second);
    };
    // LIVETRACK03 Hydro cells are ~0.390625m. Keep this historical broad
    // depression test several metres wide so it validates pooling without
    // depending on a single sub-metre texel.
    appendTile(0.0f, 6.25f, 0.05f);
    appendTile(6.25f, 12.50f, 0.00f);
    appendTile(12.50f, 18.75f, 0.05f);
    heritage::physics::water::SurfaceHydrologyBakeReport hydrologyBake;
    heritage::physics::dynamicsurface::DynamicSurfaceStaticBakeReport
        dynamicHydrologyBake;
    if (!hydrologyWorld.loadOrBakeHydrology(
            hydrologyTriangles, {}, hydrologyBake)
        || !hydrologyBake.valid || hydrologyBake.cellCount < 3u
        || !hydrologyWorld.dynamicSurface().bakeStaticScene(
            hydrologyTriangles, { 0.0, 0.0, 0.0 }, dynamicHydrologyBake))
    {
        return false;
    }
    hydrologyWorld.setHydrologyInterestSource({ 9.375, 0.0, 0.25 });
    SurfaceWeatherDescription spatialRain;
    spatialRain.enabled = true;
    spatialRain.precipitationRateMmPerHour = 120.0;
    spatialRain.relativeHumidity = 0.95;
    spatialRain.windSpeedMps = 2.0;
    spatialRain.cloudCover = 1.0;
    spatialRain.drainageRateMmPerHour = 0.0;
    spatialRain.referenceEvaporationRateMmPerHour = 0.0;
    if (!hydrologyWorld.setWeather(spatialRain))
        return false;
    for (int step = 0; step < 120; ++step)
        hydrologyWorld.advancePresentation(0.5f);

    const auto leftWater = hydrologyWorld.dynamicSurface().sampleHydro(
        { 3.125, 0.05, 0.25 });
    const auto pooledWater = hydrologyWorld.dynamicSurface().sampleHydro(
        { 9.375, 0.00, 0.25 });
    const auto spatialConditions = hydrologyWorld.localConditions(
        { 9.375f, 0.0f, 0.25f }, SurfaceMaterial::Asphalt, 0.0,
        asphaltDefaults);
    const auto spatialThermal = hydrologyWorld.dynamicSurface().sampleThermal(
        { 9.375, 0.0, 0.25 });
    const auto dynamicHydroStats = hydrologyWorld.dynamicSurface().hydroStats();
    if (!leftWater.valid || !pooledWater.valid
        || leftWater.waterDepthM <= 0.0
        || pooledWater.waterDepthM <= 0.0
        || !spatialConditions.waterFilmDepthValid
        || std::abs(spatialConditions.waterFilmDepthM
            - pooledWater.waterDepthM) > 1.0e-7
        || !spatialThermal.valid
        || std::abs(spatialConditions.surfaceTemperatureC
            - spatialThermal.surfaceTemperatureC) > 1.0e-7
        || dynamicHydroStats.simulationStepCount == 0u
        || dynamicHydroStats.activePages == 0u)
    {
        return false;
    }

    // WATER14: microscopic film remains authoritative and is available to the
    // wetness atlas through collectVisualCells(), while the explicit 3D water
    // renderer requests its own depth threshold directly from the same adaptive
    // simulation volumes. There is no separate legacy puddle collector anymore.
    std::vector<heritage::physics::water::SurfaceHydrologyVisualCell>
        visibleWater;
    hydrologyWorld.hydrology().collectVisualCellsBand(
        { 9.375, 1.0, 0.25 },
        0.0,
        10.0,
        64u,
        visibleWater,
        false,
        0.0080);
    if (hydrologyWorld.hydrology().debugVisualizationEnabled()
        || pooledWater.waterDepthM >= 0.0080
        || !visibleWater.empty())
    {
        return false;
    }

    // WATER14: authoritative water simulation topology itself is adaptive.
    // A broad planar 32x32 m surface should collapse from thousands of immutable
    // 0.5 m support samples into a much smaller set of large simulation control
    // volumes. This is no longer a presentation-only merge.
    SurfaceWorld adaptiveWaterWorld;
    std::vector<heritage::physics::StaticSceneTriangle> adaptiveWaterTriangles;
    heritage::physics::StaticSceneTriangle adaptiveFirst;
    adaptiveFirst.a = { 0.0f, 0.0f, 0.0f };
    adaptiveFirst.b = { 32.0f, 0.0f, 0.0f };
    adaptiveFirst.c = { 32.0f, 0.0f, 32.0f };
    adaptiveFirst.normal = { 0.0f, 1.0f, 0.0f };
    adaptiveFirst.surfaceMaterial = SurfaceMaterial::Asphalt;
    adaptiveFirst.surfaceProperties = asphaltDefaults;
    heritage::physics::StaticSceneTriangle adaptiveSecond = adaptiveFirst;
    adaptiveSecond.a = { 0.0f, 0.0f, 0.0f };
    adaptiveSecond.b = { 32.0f, 0.0f, 32.0f };
    adaptiveSecond.c = { 0.0f, 0.0f, 32.0f };
    adaptiveWaterTriangles.push_back(adaptiveFirst);
    adaptiveWaterTriangles.push_back(adaptiveSecond);
    heritage::physics::water::SurfaceHydrologyBakeReport adaptiveWaterBake;
    if (!adaptiveWaterWorld.loadOrBakeHydrology(
            adaptiveWaterTriangles, {}, adaptiveWaterBake)
        || !adaptiveWaterBake.valid)
    {
        return false;
    }
    const auto adaptiveFlatStats = adaptiveWaterWorld.hydrology().stats();
    if (adaptiveFlatStats.supportCellCount <= adaptiveFlatStats.cellCount
        || adaptiveFlatStats.adaptiveMaximumCellSizeM < 8.0
        || adaptiveFlatStats.adaptiveMaximumCellSizeM > 20.0001
        || adaptiveFlatStats.adaptiveMinimumCellSizeM < 0.0999)
    {
        return false;
    }

    // WATER14F: a uniformly sloped plane must remain aggressively coarse even
    // when source-triangle normals contain modest tessellation noise. The
    // support elevations are exactly planar; alternating normals intentionally
    // disagree by ~11 degrees so the retired anchor-normal gate would fragment
    // this surface despite there being no geometric curvature.
    SurfaceWorld planarSlopeWaterWorld;
    std::vector<heritage::physics::StaticSceneTriangle> planarSlopeTriangles;
    const auto normalizeTestNormal = [](float x, float y, float z) {
        const float length = std::sqrt(x * x + y * y + z * z);
        return heritage::math::Vec3{ x / length, y / length, z / length };
    };
    const heritage::math::Vec3 shallowNormal = normalizeTestNormal(-0.05f, 1.0f, 0.0f);
    const heritage::math::Vec3 steepNormal = normalizeTestNormal(-0.25f, 1.0f, 0.0f);
    for (int z = 0; z < 16; ++z)
    {
        for (int x = 0; x < 16; ++x)
        {
            const float x0 = static_cast<float>(x) * 0.5f;
            const float x1 = x0 + 0.5f;
            const float z0 = static_cast<float>(z) * 0.5f;
            const float z1 = z0 + 0.5f;
            const float y0 = x0 * 0.15f;
            const float y1 = x1 * 0.15f;
            const heritage::math::Vec3 noisyNormal = ((x + z) & 1) == 0
                ? shallowNormal : steepNormal;
            heritage::physics::StaticSceneTriangle first;
            first.a = { x0, y0, z0 };
            first.b = { x1, y1, z0 };
            first.c = { x1, y1, z1 };
            first.normal = noisyNormal;
            first.surfaceMaterial = SurfaceMaterial::Asphalt;
            first.surfaceProperties = asphaltDefaults;
            heritage::physics::StaticSceneTriangle second = first;
            second.a = { x0, y0, z0 };
            second.b = { x1, y1, z1 };
            second.c = { x0, y0, z1 };
            planarSlopeTriangles.push_back(first);
            planarSlopeTriangles.push_back(second);
        }
    }
    heritage::physics::water::SurfaceHydrologyBakeReport planarSlopeBake;
    if (!planarSlopeWaterWorld.loadOrBakeHydrology(
            planarSlopeTriangles, {}, planarSlopeBake)
        || !planarSlopeBake.valid)
    {
        return false;
    }
    const auto planarSlopeStats = planarSlopeWaterWorld.hydrology().stats();
    if (planarSlopeStats.supportCellCount <= planarSlopeStats.cellCount
        || planarSlopeStats.adaptiveMaximumCellSizeM < 7.9)
    {
        return false;
    }

    // WATER14F/I curb-local packing: a 15 cm step must reject candidates that
    // cross it, but WATER14I must NOT explode every adjacent 0.50 m support cell
    // into 25 authoritative 0.10 m cells. Instead, every half-metre section of
    // the curb carries a directional fine-boundary hint for one presentation
    // strip, while farther planar asphalt still recovers large control volumes.
    SurfaceWorld curbLocalWaterWorld;
    std::vector<heritage::physics::StaticSceneTriangle> curbLocalTriangles;
    const auto appendPlanarRegion = [&](float x0, float x1, float elevation) {
        heritage::physics::StaticSceneTriangle first;
        first.a = { x0, elevation, 0.0f };
        first.b = { x1, elevation, 0.0f };
        first.c = { x1, elevation, 4.0f };
        first.normal = { 0.0f, 1.0f, 0.0f };
        first.surfaceMaterial = SurfaceMaterial::Asphalt;
        first.surfaceProperties = asphaltDefaults;
        heritage::physics::StaticSceneTriangle second = first;
        second.a = { x0, elevation, 0.0f };
        second.b = { x1, elevation, 4.0f };
        second.c = { x0, elevation, 4.0f };
        curbLocalTriangles.push_back(first);
        curbLocalTriangles.push_back(second);
    };
    appendPlanarRegion(0.0f, 1.5f, 0.0f);
    appendPlanarRegion(1.5f, 14.0f, 0.15f);
    heritage::physics::water::SurfaceHydrologyBakeReport curbLocalBake;
    if (!curbLocalWaterWorld.loadOrBakeHydrology(
            curbLocalTriangles, {}, curbLocalBake)
        || !curbLocalBake.valid)
    {
        return false;
    }
    const auto curbLocalStats = curbLocalWaterWorld.hydrology().stats();
    if (curbLocalStats.adaptiveMaximumCellSizeM < 1.9
        || curbLocalStats.adaptiveMinimumCellSizeM < 0.4999
        || curbLocalStats.adaptiveSubDecimetreCellCount != 0u)
    {
        return false;
    }
    if (!curbLocalWaterWorld.hydrology().setUniformWaterDepthForLab(0.010))
        return false;
    std::vector<heritage::physics::water::SurfaceHydrologyVisualCell>
        curbLocalVolumes;
    curbLocalWaterWorld.hydrology().collectVisualCellsBand(
        { 1.5, 1.0, 2.0 },
        0.0,
        30.0,
        100000u,
        curbLocalVolumes,
        false,
        0.0);
    std::array<bool, 8> curbFineBandCovered{};
    std::size_t curbBoundaryCellCount = 0u;
    std::size_t curbUnexpectedTinyCellCount = 0u;
    bool water15fRoadHeadStayedBelowCurb = true;
    bool water15fSidewalkHeadStayedAboveRoad = true;
    for (const auto& volume : curbLocalVolumes)
    {
        // WATER15F hydraulic-head reconstruction must never average the 15 cm
        // road/sidewalk discontinuity into one presentation surface. With a
        // uniform 10 mm lab film the road-side corner head stays near 10 mm,
        // while the raised side stays near 160 mm. This protects the exact
        // fragment-height subtraction used by the material shader.
        if (volume.surfaceElevationM < 0.05)
        {
            for (const double headY : volume.cornerWaterSurfaceElevationM)
                water15fRoadHeadStayedBelowCurb &= headY < 0.04;
        }
        else if (volume.surfaceElevationM > 0.10)
        {
            for (const double headY : volume.cornerWaterSurfaceElevationM)
                water15fSidewalkHeadStayedAboveRoad &= headY > 0.12;
        }
        if (volume.cellSizeM < 0.20)
            ++curbUnexpectedTinyCellCount;
        if (volume.fineBoundaryMask == 0u
            || std::abs(volume.globalPosition.x - 1.5) > 0.55)
        {
            continue;
        }
        const int zBand = static_cast<int>(std::floor(
            volume.globalPosition.z / 0.5));
        if (zBand >= 0 && zBand < static_cast<int>(curbFineBandCovered.size()))
            curbFineBandCovered[static_cast<std::size_t>(zBand)] = true;
        ++curbBoundaryCellCount;
    }
    if (curbBoundaryCellCount == 0u
        || curbUnexpectedTinyCellCount != 0u
        || !water15fRoadHeadStayedBelowCurb
        || !water15fSidewalkHeadStayedAboveRoad
        || !std::all_of(curbFineBandCovered.begin(), curbFineBandCovered.end(),
            [](bool covered) { return covered; }))
    {
        return false;
    }


    // WATER16 LiveSurface regression: presentation heads are solved per drainage
    // catchment, not per adaptive control volume. The 15 cm curb must create
    // separate road/sidewalk catchments, while every visible tile on one flat
    // side shares one basin head. This is the source-level guarantee that a
    // renderer cannot rediscover the adaptive chessboard from hydraulic head.
    std::vector<heritage::physics::water::SurfaceHydrologyVisualCell>
        water16BasinVolumes;
    curbLocalWaterWorld.hydrology().collectPresentationBasinCellsBand(
        { 1.5, 1.0, 2.0 },
        0.0,
        30.0,
        100000u,
        water16BasinVolumes,
        0.05);
    if (water16BasinVolumes.empty())
        return false;
    double water16RoadMinimumHead = std::numeric_limits<double>::max();
    double water16RoadMaximumHead = -std::numeric_limits<double>::max();
    double water16SidewalkMinimumHead = std::numeric_limits<double>::max();
    std::size_t water16RoadTileCount = 0u;
    std::size_t water16SidewalkTileCount = 0u;
    bool water17UsesImmutableSupportParameterization = true;
    bool water17EveryTileHasStableBasinIdentity = true;
    for (const auto& volume : water16BasinVolumes)
    {
        // WATER17 optical topology must be independent from the adaptive solver.
        // The visible basin atlas is seeded exclusively by immutable 0.50 m
        // support samples, then converted to organic overlapping radial bases on
        // the GPU. A 2/8/20 m adaptive control volume must never become an
        // equally shaped presentation primitive again.
        water17UsesImmutableSupportParameterization &=
            std::abs(volume.cellSizeM - 0.50) <= 1.0e-9;
        water17EveryTileHasStableBasinIdentity &=
            volume.presentationBasinId >= 0;
        if (volume.surfaceElevationM < 0.05)
        {
            ++water16RoadTileCount;
            water16RoadMinimumHead = std::min(
                water16RoadMinimumHead,
                volume.cornerWaterSurfaceElevationM[0]);
            water16RoadMaximumHead = std::max(
                water16RoadMaximumHead,
                volume.cornerWaterSurfaceElevationM[0]);
        }
        else if (volume.surfaceElevationM > 0.10)
        {
            ++water16SidewalkTileCount;
            water16SidewalkMinimumHead = std::min(
                water16SidewalkMinimumHead,
                volume.cornerWaterSurfaceElevationM[0]);
        }
    }
    const auto water16Stats = curbLocalWaterWorld.hydrology().stats();
    if (water16RoadTileCount < 2u
        || water16SidewalkTileCount < 2u
        || water16Stats.presentationBasinCount < 2u
        || water16Stats.activePresentationBasinCount < 2u
        || !water17UsesImmutableSupportParameterization
        || !water17EveryTileHasStableBasinIdentity
        || water16RoadMaximumHead - water16RoadMinimumHead > 1.0e-8
        || water16RoadMaximumHead >= 0.04
        || water16SidewalkMinimumHead <= 0.12)
    {
        return false;
    }

    // WATER14J restricted-quadtree regression: every actual shared face in the
    // 0.50 m+ curb topology must obey a 2:1 size ratio. This specifically
    // catches an unaligned orphan 0.50 m leaf sitting directly beside a 4/8 m
    // greedy patch, which was the source of the visually bunched transition.
    for (std::size_t aIndex = 0u; aIndex < curbLocalVolumes.size(); ++aIndex)
    {
        const auto& a = curbLocalVolumes[aIndex];
        if (a.cellSizeM < 0.4999)
            continue;
        const double aHalf = a.cellSizeM * 0.5;
        const double aMinX = a.globalPosition.x - aHalf;
        const double aMaxX = a.globalPosition.x + aHalf;
        const double aMinZ = a.globalPosition.z - aHalf;
        const double aMaxZ = a.globalPosition.z + aHalf;
        for (std::size_t bIndex = aIndex + 1u;
             bIndex < curbLocalVolumes.size(); ++bIndex)
        {
            const auto& b = curbLocalVolumes[bIndex];
            if (b.cellSizeM < 0.4999
                || a.presentationLayer != b.presentationLayer)
            {
                continue;
            }
            const double bHalf = b.cellSizeM * 0.5;
            const double bMinX = b.globalPosition.x - bHalf;
            const double bMaxX = b.globalPosition.x + bHalf;
            const double bMinZ = b.globalPosition.z - bHalf;
            const double bMaxZ = b.globalPosition.z + bHalf;
            const double overlapX = std::min(aMaxX, bMaxX)
                - std::max(aMinX, bMinX);
            const double overlapZ = std::min(aMaxZ, bMaxZ)
                - std::max(aMinZ, bMinZ);
            const bool touchesVerticalFace = overlapZ > 1.0e-6
                && (std::abs(aMaxX - bMinX) <= 1.0e-6
                    || std::abs(bMaxX - aMinX) <= 1.0e-6);
            const bool touchesHorizontalFace = overlapX > 1.0e-6
                && (std::abs(aMaxZ - bMinZ) <= 1.0e-6
                    || std::abs(bMaxZ - aMinZ) <= 1.0e-6);
            if (!touchesVerticalFace && !touchesHorizontalFace)
                continue;
            const double larger = std::max(a.cellSizeM, b.cellSizeM);
            const double smaller = std::min(a.cellSizeM, b.cellSizeM);
            if (larger > smaller * 2.0 + 1.0e-6)
                return false;
        }
    }

    // WATER14G/J graded transition regression: a narrow aggressive-normal strip
    // still earns the 0.10 m tier, but nearby planar supports are prevented from
    // jumping immediately to multi-metre cells. Farther planar asphalt must
    // recover coarse 4 m control volumes so the detail halo stays local.
    SurfaceWorld gradedWaterWorld;
    std::vector<heritage::physics::StaticSceneTriangle> gradedTriangles;
    const heritage::math::Vec3 gradingAggressiveNormal = normalizeTestNormal(
        0.85f, 0.5268f, 0.0f);
    for (int z = 0; z < 8; ++z)
    {
        for (int x = 0; x < 32; ++x)
        {
            const float x0 = static_cast<float>(x) * 0.5f;
            const float x1 = x0 + 0.5f;
            const float z0 = static_cast<float>(z) * 0.5f;
            const float z1 = z0 + 0.5f;
            const heritage::math::Vec3 normal = x == 16
                ? gradingAggressiveNormal : heritage::math::Vec3{ 0.0f, 1.0f, 0.0f };
            heritage::physics::StaticSceneTriangle first;
            first.a = { x0, 0.0f, z0 };
            first.b = { x1, 0.0f, z0 };
            first.c = { x1, 0.0f, z1 };
            first.normal = normal;
            first.surfaceMaterial = SurfaceMaterial::Asphalt;
            first.surfaceProperties = asphaltDefaults;
            heritage::physics::StaticSceneTriangle second = first;
            second.a = { x0, 0.0f, z0 };
            second.b = { x1, 0.0f, z1 };
            second.c = { x0, 0.0f, z1 };
            gradedTriangles.push_back(first);
            gradedTriangles.push_back(second);
        }
    }
    heritage::physics::water::SurfaceHydrologyBakeReport gradedBake;
    if (!gradedWaterWorld.loadOrBakeHydrology(
            gradedTriangles, {}, gradedBake)
        || !gradedBake.valid
        || !gradedWaterWorld.hydrology().setUniformWaterDepthForLab(0.010))
    {
        return false;
    }
    std::vector<heritage::physics::water::SurfaceHydrologyVisualCell>
        gradedVolumes;
    gradedWaterWorld.hydrology().collectVisualCellsBand(
        { 8.0, 1.0, 2.0 },
        0.0,
        30.0,
        100000u,
        gradedVolumes,
        false,
        0.0);
    double largestNearFineM = 0.0;
    double largestFarFromFineM = 0.0;
    std::size_t fineVolumeCount = 0u;
    for (const auto& volume : gradedVolumes)
    {
        const double distanceX = std::abs(volume.globalPosition.x - 8.25);
        if (distanceX < 1.5)
            largestNearFineM = std::max(largestNearFineM, volume.cellSizeM);
        if (distanceX > 4.0)
            largestFarFromFineM = std::max(
                largestFarFromFineM, volume.cellSizeM);
        if (volume.cellSizeM < 0.20)
            ++fineVolumeCount;
    }
    if (fineVolumeCount == 0u
        || largestNearFineM > 1.0001
        || largestFarFromFineM < 3.9)
    {
        return false;
    }

    std::vector<heritage::physics::water::SurfaceHydrologyVisualCell>
        adaptiveWaterVolumes;
    if (!adaptiveWaterWorld.hydrology().setUniformWaterDepthForLab(0.001))
        return false;
    adaptiveWaterWorld.hydrology().collectVisualCellsBand(
        { 16.0, 1.0, 16.0 },
        0.0,
        20.0,
        10000u,
        adaptiveWaterVolumes,
        false,
        0.0080);
    // One millimetre remains authoritative water but does not become explicit
    // 3D surface geometry when the renderer asks for an 8 mm hand-off.
    if (!adaptiveWaterVolumes.empty())
        return false;

    if (!adaptiveWaterWorld.hydrology().setUniformWaterDepthForLab(0.020))
        return false;
    adaptiveWaterWorld.hydrology().collectVisualCellsBand(
        { 16.0, 1.0, 16.0 },
        0.0,
        20.0,
        10000u,
        adaptiveWaterVolumes,
        false,
        0.0080);
    double largestAdaptiveSimulationCellM = 0.0;
    for (const auto& volume : adaptiveWaterVolumes)
        largestAdaptiveSimulationCellM = std::max(
            largestAdaptiveSimulationCellM, volume.cellSizeM);
    if (adaptiveWaterVolumes.empty()
        || largestAdaptiveSimulationCellM < 8.0
        || largestAdaptiveSimulationCellM > 20.0001)
    {
        return false;
    }

    // WATER14A: a material boundary may prevent coarse merging, but it must
    // NOT spend the 0.10 m authoritative tier by itself. Those tiny cells are
    // reserved for genuinely aggressive terrain angles.
    SurfaceWorld materialBoundaryWaterWorld;
    std::vector<heritage::physics::StaticSceneTriangle> materialBoundaryTriangles;
    const auto appendMaterialBoundaryTile = [&](float x0, SurfaceMaterial material) {
        heritage::physics::StaticSceneTriangle first;
        first.a = { x0, 0.0f, 0.0f };
        first.b = { x0 + 0.5f, 0.0f, 0.0f };
        first.c = { x0 + 0.5f, 0.0f, 0.5f };
        first.normal = { 0.0f, 1.0f, 0.0f };
        first.surfaceMaterial = material;
        first.surfaceProperties = asphaltDefaults;
        heritage::physics::StaticSceneTriangle second = first;
        second.a = { x0, 0.0f, 0.0f };
        second.b = { x0 + 0.5f, 0.0f, 0.5f };
        second.c = { x0, 0.0f, 0.5f };
        materialBoundaryTriangles.push_back(first);
        materialBoundaryTriangles.push_back(second);
    };
    appendMaterialBoundaryTile(0.0f, SurfaceMaterial::Asphalt);
    appendMaterialBoundaryTile(0.5f, SurfaceMaterial::Gravel);
    heritage::physics::water::SurfaceHydrologyBakeReport materialBoundaryBake;
    if (!materialBoundaryWaterWorld.loadOrBakeHydrology(
            materialBoundaryTriangles, {}, materialBoundaryBake)
        || !materialBoundaryBake.valid)
    {
        return false;
    }
    const auto materialBoundaryStats =
        materialBoundaryWaterWorld.hydrology().stats();
    if (materialBoundaryStats.adaptiveMinimumCellSizeM < 0.4999
        || materialBoundaryStats.adaptiveSubDecimetreCellCount != 0u)
    {
        return false;
    }

    // A genuinely aggressive ~60 degree support slope DOES qualify for the
    // 0.10 m tier. This protects the intended use case without carpeting
    // ordinary road camber and broad hillsides in tiny control volumes.
    SurfaceWorld aggressiveSlopeWaterWorld;
    std::vector<heritage::physics::StaticSceneTriangle> aggressiveSlopeTriangles;
    constexpr float aggressiveRise = 0.8660254f; // tan(60deg) * 0.5m
    const heritage::math::Vec3 aggressiveNormal{ -0.8660254f, 0.5f, 0.0f };
    heritage::physics::StaticSceneTriangle aggressiveFirst;
    aggressiveFirst.a = { 0.0f, 0.0f, 0.0f };
    aggressiveFirst.b = { 0.5f, aggressiveRise, 0.0f };
    aggressiveFirst.c = { 0.5f, aggressiveRise, 0.5f };
    aggressiveFirst.normal = aggressiveNormal;
    aggressiveFirst.surfaceMaterial = SurfaceMaterial::Asphalt;
    aggressiveFirst.surfaceProperties = asphaltDefaults;
    heritage::physics::StaticSceneTriangle aggressiveSecond = aggressiveFirst;
    aggressiveSecond.a = { 0.0f, 0.0f, 0.0f };
    aggressiveSecond.b = { 0.5f, aggressiveRise, 0.5f };
    aggressiveSecond.c = { 0.0f, 0.0f, 0.5f };
    aggressiveSlopeTriangles.push_back(aggressiveFirst);
    aggressiveSlopeTriangles.push_back(aggressiveSecond);
    heritage::physics::water::SurfaceHydrologyBakeReport aggressiveSlopeBake;
    if (!aggressiveSlopeWaterWorld.loadOrBakeHydrology(
            aggressiveSlopeTriangles, {}, aggressiveSlopeBake)
        || !aggressiveSlopeBake.valid)
    {
        return false;
    }
    const auto aggressiveSlopeStats = aggressiveSlopeWaterWorld.hydrology().stats();
    if (aggressiveSlopeStats.adaptiveMinimumCellSizeM > 0.1001
        || aggressiveSlopeStats.adaptiveSubDecimetreCellCount == 0u)
    {
        return false;
    }

    heritage::physics::water::SurfaceHydrologyTireInput tireWater;
    tireWater.deltaTimeSeconds = 0.01;
    tireWater.contactPatchWidthM = 0.20;
    tireWater.contactPatchLengthM = 0.12;
    tireWater.contactPatchAreaM2 = 0.024;
    tireWater.normalLoadN = 3500.0;
    tireWater.nominalLoadN = 3500.0;
    tireWater.forwardSpeedMps = 30.0;
    tireWater.treadVoidRatio = 0.30;
    const double depthBeforeTire = pooledWater.waterDepthM;
    for (int contact = 0; contact < 800; ++contact)
    {
        hydrologyWorld.applyHydrologyTireContact(
            { 9.375f, 0.0f, 0.25f }, tireWater);
    }
    const auto clearedWater = hydrologyWorld.dynamicSurface().sampleHydro(
        { 9.375, 0.00, 0.25 });
    if (!clearedWater.valid || clearedWater.waterDepthM >= depthBeforeTire
        || hydrologyWorld.dynamicSurface().hydroStats().tireContactCount != 800u
        || hydrologyWorld.dynamicSurface().hydroStats().cumulativeTireClearedVolumeM3 <= 0.0)
    {
        return false;
    }
    hydrologyWorld.resetHydrologyWater();
    if (hydrologyWorld.dynamicSurface().hydroStats().waterVolumeM3 != 0.0)
        return false;

    // LIVETRACK03 precipitation exposure at the packed 4-bit water threshold:
    // the top bridge deck and an adjacent open road receive direct rain, while
    // the road directly beneath the bridge remains dry. Covered/open spans are
    // the one-X/Z Hydro authority intentionally ignores vertical water sheets.
    // Run long enough to cross the first 0.1mm packed water level robustly.
    SurfaceWorld coverWorld;
    std::vector<heritage::physics::StaticSceneTriangle> coverTriangles;
    const auto appendCoverTile = [&](float x0, float x1, float elevation) {
        heritage::physics::StaticSceneTriangle first;
        first.a = { x0, elevation, 0.0f };
        first.b = { x1, elevation, 0.0f };
        first.c = { x1, elevation, 0.5f };
        first.normal = { 0.0f, 1.0f, 0.0f };
        first.surfaceMaterial = SurfaceMaterial::Asphalt;
        first.surfaceProperties = asphaltDefaults;
        heritage::physics::StaticSceneTriangle second = first;
        second.a = { x0, elevation, 0.0f };
        second.b = { x1, elevation, 0.5f };
        second.c = { x0, elevation, 0.5f };
        coverTriangles.push_back(first);
        coverTriangles.push_back(second);
    };
    appendCoverTile(0.0f, 12.0f, 0.0f); // lower road beneath bridge
    appendCoverTile(0.0f, 12.0f, 5.0f); // bridge deck / cover
    appendCoverTile(18.75f, 31.25f, 0.0f); // open road in separate Hydro cells
    heritage::physics::water::SurfaceHydrologyBakeReport coverBake;
    heritage::physics::dynamicsurface::DynamicSurfaceStaticBakeReport
        dynamicCoverBake;
    if (!coverWorld.loadOrBakeHydrology(coverTriangles, {}, coverBake)
        || !coverBake.valid
        || !coverWorld.dynamicSurface().bakeStaticScene(
            coverTriangles, { 0.0, 0.0, 0.0 }, dynamicCoverBake))
    {
        return false;
    }
    coverWorld.setHydrologyInterestSource({ 3.125, 5.0, 0.25 });
    // WEATHER06H presentation shelter query must distinguish a true same-column
    // bridge/roof from adjacent open terrain. This is the exact query consumed
    // by the rain renderer and prevents steep hillsides from suppressing rain.
    if (!coverWorld.hydrology().hasPrecipitationCoverAbove(
            { 3.125, 1.0, 0.25 }, 1.0, 24.0)
        || coverWorld.hydrology().hasPrecipitationCoverAbove(
            { 21.875, 1.0, 0.25 }, 1.0, 24.0)
        || coverWorld.hydrology().hasPrecipitationCoverAbove(
            { 3.125, 5.5, 0.25 }, 1.0, 24.0))
    {
        return false;
    }

    if (!coverWorld.setWeather(spatialRain))
        return false;
    for (int step = 0; step < 24; ++step)
        coverWorld.advancePresentation(0.25f);
    const auto coveredLower = coverWorld.dynamicSurface().sampleHydro(
        { 3.125, 0.0, 0.25 });
    const auto exposedUpper = coverWorld.dynamicSurface().sampleHydro(
        { 3.125, 5.0, 0.25 });
    const auto exposedOpen = coverWorld.dynamicSurface().sampleHydro(
        { 21.875, 0.0, 0.25 });
    if (!coveredLower.valid || !exposedUpper.valid || !exposedOpen.valid
        || exposedUpper.waterDepthM <= 0.0
        || exposedOpen.waterDepthM <= 0.0
        // LIVETRACK03 has one X/Z Hydro layer. The rain renderer may still know
        // about physical cover, but water authority intentionally does not keep
        // separate lower/upper pages at its 256x256 resolution.
        || std::abs(coveredLower.waterDepthM - exposedUpper.waterDepthM) > 1.0e-9)
    {
        return false;
    }

    // WATER05C stability recovery keeps every discrete hydrology presentation
    // record at the known-good fixed 6 mm separation from dense authored
    // collision geometry. Dry-cell gathering remains available for future
    // isolated wetness reconstruction, but it must obey the same stable lift.
    std::vector<heritage::physics::water::SurfaceHydrologyVisualCell>
        dryWetnessReceiverCells;
    hydrologyWorld.hydrology().collectVisualCells(
        { 0.75, 1.0, 0.25 },
        10.0,
        64u,
        dryWetnessReceiverCells,
        10.0,
        true);
    if (dryWetnessReceiverCells.empty())
        return false;
    for (const auto& cell : dryWetnessReceiverCells)
    {
        if (cell.waterDepthM != 0.0
            || std::abs(cell.globalPosition.y
                - (cell.surfaceElevationM + 0.000000001)) > 1.0e-7)
        {
            return false;
        }
    }

    SurfaceFieldDescription worldFieldDescription;
    worldFieldDescription.cellSizeM = 0.25f;
    worldFieldDescription.maximumCellCount = 1024u;
    worldFieldDescription.chunkSizeCells = 8u;
    worldFieldDescription.maximumResidentChunkCount = 8u;
    world.deformableTerrain().setDescription(worldFieldDescription);

    const SurfaceFieldInitialState virgin{
        0.08f, 0.10f, 0.25f, 0.0f
    };
    SurfaceFieldUpdate update;
    update.material = SurfaceMaterial::Mud;
    update.initialState = virgin;
    update.rutDepthTargetM = 0.03f;
    update.rutDepthDeltaM = 0.012f;
    update.compactionDelta = 0.15f;
    update.longitudinalShearHistoryDeltaM = 0.4f;
    update.displacedVolumeDeltaM3 = 0.002f;
    update.countPass = true;

    const DVec3 firstOrigin{ 100000.0, 0.0, -75000.0 };
    const Vec3 firstLocal{ 12.375f, 0.0f, -7.625f };
    world.setGlobalOrigin(firstOrigin);
    const auto written = world.applyDeformable(firstLocal, update);
    if (!written.valid || written.rutDepthM <= 0.0f
        || written.passCount != 1u)
    {
        return false;
    }

    // Simulate a floating-origin rebase. Local coordinates change, but the
    // absolute contact point is identical and therefore must address the same
    // persistent surface cell without rebasing/rekeying the field itself.
    const DVec3 secondOrigin{ 104096.0, 0.0, -79096.0 };
    const Vec3 secondLocal{
        firstLocal.x - 4096.0f,
        firstLocal.y,
        firstLocal.z + 4096.0f
    };
    world.setGlobalOrigin(secondOrigin);
    const auto afterRebase = world.sampleDeformable(
        secondLocal, SurfaceMaterial::Mud, virgin);
    if (!afterRebase.valid
        || std::abs(afterRebase.rutDepthM - written.rutDepthM) > 1.0e-6f
        || std::abs(afterRebase.compaction - written.compaction) > 1.0e-6f
        || afterRebase.passCount != written.passCount)
    {
        return false;
    }

    // X/Z-only fields alias stacked roads. The coarse global-Y layer keeps a
    // bridge/tunnel surface independent without making centimetres of contact
    // movement or rut depth create a new identity.
    const Vec3 stackedLocal{
        secondLocal.x,
        secondLocal.y + 5.0f,
        secondLocal.z
    };
    const auto stackedVirgin = world.sampleDeformable(
        stackedLocal, SurfaceMaterial::Mud, virgin);
    if (stackedVirgin.rutDepthM != virgin.rutDepthM
        || stackedVirgin.passCount != 0u)
    {
        return false;
    }

    // Verify bounded chunk LRU behavior and the persistence seam. With 1 m
    // chunks, touching three distant positions while only two chunks may be
    // resident must evict a complete least-recently-used chunk in O(1)-style
    // cache bookkeeping rather than scanning every cell in the world.
    SurfaceFieldDescription cacheDescription;
    cacheDescription.cellSizeM = 0.25f;
    cacheDescription.maximumCellCount = 64u;
    cacheDescription.chunkSizeCells = 4u;
    cacheDescription.maximumResidentChunkCount = 2u;
    SurfaceField cache(cacheDescription);

    std::size_t evictionCount = 0;
    SurfaceFieldChunkSnapshot evicted;
    cache.setChunkEvictionCallback(
        [&](const SurfaceFieldChunkSnapshot& snapshot) {
            ++evictionCount;
            evicted = snapshot;
        });

    SurfaceFieldUpdate cacheUpdate;
    cacheUpdate.material = SurfaceMaterial::Sand;
    cacheUpdate.initialState = { 0.12f, 0.05f, 0.10f, 0.0f };
    cacheUpdate.rutDepthTargetM = 0.02f;
    cacheUpdate.rutDepthDeltaM = 0.01f;
    cacheUpdate.countPass = true;

    const DVec3 pointA{ 0.10, 0.0, 0.10 };
    const DVec3 pointB{ 1.10, 0.0, 0.10 };
    const DVec3 pointC{ 2.10, 0.0, 0.10 };
    cache.apply(pointA, cacheUpdate);
    cache.apply(pointB, cacheUpdate);
    cache.apply(pointC, cacheUpdate);

    if (cache.residentChunkCount() != 2u
        || cache.cellCount() > cache.description().maximumCellCount
        || evictionCount == 0u
        || evicted.cells.empty())
    {
        return false;
    }

    const std::int64_t evictedChunkX = evicted.chunkX;
    const std::int64_t evictedChunkZ = evicted.chunkZ;
    if (!cache.restoreChunk(evicted))
        return false;

    SurfaceFieldChunkSnapshot restored;
    if (!cache.snapshotChunk(evictedChunkX, evictedChunkZ, restored)
        || restored.cells.empty()
        || cache.residentChunkCount() > cache.description().maximumResidentChunkCount)
    {
        return false;
    }

    // JOB03 multi-view safety: two interest sources create the UNION of two
    // local cadence regions. They must never be averaged into a midpoint that
    // accidentally makes water between distant players run at near cadence.
    SurfaceWorld multiSourceHydrologyWorld;
    std::vector<heritage::physics::StaticSceneTriangle> multiSourceTriangles;
    heritage::physics::StaticSceneTriangle multiA;
    multiA.a = { -20.0f, 0.0f, 0.0f };
    multiA.b = { 3020.0f, 0.0f, 0.0f };
    multiA.c = { 3020.0f, 0.0f, 1.0f };
    multiA.normal = { 0.0f, 1.0f, 0.0f };
    multiA.surfaceMaterial = SurfaceMaterial::Asphalt;
    multiA.surfaceProperties = asphaltDefaults;
    heritage::physics::StaticSceneTriangle multiB = multiA;
    multiB.a = { -20.0f, 0.0f, 0.0f };
    multiB.b = { 820.0f, 0.0f, 1.0f };
    multiB.c = { -20.0f, 0.0f, 1.0f };
    multiSourceTriangles.push_back(multiA);
    multiSourceTriangles.push_back(multiB);
    heritage::physics::water::SurfaceHydrologyBakeReport multiSourceBake;
    heritage::physics::dynamicsurface::DynamicSurfaceStaticBakeReport
        dynamicMultiSourceBake;
    if (!multiSourceHydrologyWorld.loadOrBakeHydrology(
            multiSourceTriangles, {}, multiSourceBake)
        || !multiSourceHydrologyWorld.dynamicSurface().bakeStaticScene(
            multiSourceTriangles, { 0.0, 0.0, 0.0 }, dynamicMultiSourceBake))
    {
        return false;
    }
    multiSourceHydrologyWorld.setHydrologyInterestSources({
        { 0.0, 0.0, 0.25 },
        { 3000.0, 0.0, 0.25 }
    });
    SurfaceWeatherDescription multiSourceRain;
    multiSourceRain.enabled = true;
    multiSourceRain.precipitationRateMmPerHour = 120.0;
    multiSourceRain.relativeHumidity = 0.95;
    multiSourceRain.cloudCover = 1.0;
    multiSourceRain.drainageRateMmPerHour = 0.0;
    multiSourceRain.referenceEvaporationRateMmPerHour = 0.0;
    if (!multiSourceHydrologyWorld.setWeather(multiSourceRain))
        return false;
    // LIVETRACK03 near pages tick at 6Hz while distant materialized pages tick
    // once per minute. Run just beyond one distant cadence interval so the unseen
    // midpoint proves that its water history advances without presentation residency.
    for (int step = 0; step < 122; ++step)
        multiSourceHydrologyWorld.advancePresentation(0.5f);
    const auto multiNearA = multiSourceHydrologyWorld.dynamicSurface().sampleHydro(
        { 0.25, 0.0, 0.25 });
    const auto multiMidpoint = multiSourceHydrologyWorld.dynamicSurface().sampleHydro(
        { 1500.25, 0.0, 0.25 });
    const auto multiNearB = multiSourceHydrologyWorld.dynamicSurface().sampleHydro(
        { 2999.75, 0.0, 0.25 });
    const auto multiStats = multiSourceHydrologyWorld.dynamicSurface().hydroStats();
    if (!multiNearA.valid || !multiMidpoint.valid || !multiNearB.valid
        || multiNearA.waterDepthM <= 0.0
        || multiMidpoint.waterDepthM <= 0.0
        || multiNearB.waterDepthM <= 0.0
        // LIVETRACK03 keeps whole-scene water persistent: near pages run at
        // 6Hz, while unseen materialized pages advance at 1/60Hz.
        || multiSourceHydrologyWorld.dynamicSurface().interestSources().size() != 2u
        || multiStats.cadence30HzPages != 0u
        || multiStats.cadence20HzPages != 0u
        || multiStats.cadence6HzPages == 0u
        || multiStats.cadenceDistantPages == 0u
        || multiStats.cadence2HzPages != 0u)
    {
        return false;
    }

    // WATER15 presentation regression boundary: settled water no longer has a
    // renderer-owned tessellation/stitching topology. The portable regression
    // therefore stops at authoritative hydrology state; OpenGL presentation is
    // protected by source-level architecture validation and the Windows smoke run.

    return true;
}

} // namespace heritage::tests

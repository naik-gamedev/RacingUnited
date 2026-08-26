#include "PhysicsRegressionCommon.hpp"

#include "../Physics/Surfaces/SurfaceField.hpp"
#include "../Physics/Surfaces/SurfaceWorld.hpp"
#include "Reference/DynamicSurfaceHydrologyReference.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
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
    heritage::tests::reference::DynamicSurfaceHydrologyReference hydrologyReference;

    // LIVETRACK08/LIVETRACK21I production regression: the static .hhyd
    // response must see a broad shallow bowl whose spill rim is metres away.
    // The retired GPU initializer looked only about 8 cm around each detailed
    // texel, so the middle of a parking-lot-scale depression appeared locally
    // flat and got no standing-puddle capacity. Keep this synthetic bowl inside
    // LIVETRACK21I's deliberate 0..0.70 mm 4-bit standing-depth ladder so the
    // packed regression can still verify center > shoulder > edge. Deeper
    // geometric basins intentionally saturate at code 15 and are tested by the
    // topology itself rather than by demanding nonexistent >0.70 mm code detail.
    SurfaceWorld prebakedBasinWorld;
    std::vector<heritage::physics::StaticSceneTriangle> prebakedBasinTriangles;
    const auto prebakedBasinHeight = [](float x, float z) {
        const float dx = x - 5.0f;
        const float dz = z - 5.0f;
        const float radiusM = std::sqrt(dx * dx + dz * dz);
        const float normalizedRadius = std::min(radiusM / 4.0f, 1.0f);
        return 0.00060f * normalizedRadius * normalizedRadius;
    };
    for (int z = 0; z < 10; ++z)
    {
        for (int x = 0; x < 10; ++x)
        {
            const float x0 = static_cast<float>(x);
            const float x1 = x0 + 1.0f;
            const float z0 = static_cast<float>(z);
            const float z1 = z0 + 1.0f;
            heritage::physics::StaticSceneTriangle first;
            first.a = { x0, prebakedBasinHeight(x0, z0), z0 };
            first.b = { x1, prebakedBasinHeight(x1, z0), z0 };
            first.c = { x1, prebakedBasinHeight(x1, z1), z1 };
            first.normal = { 0.0f, 1.0f, 0.0f };
            first.surfaceMaterial = SurfaceMaterial::Asphalt;
            first.surfaceProperties = asphaltDefaults;
            heritage::physics::StaticSceneTriangle second = first;
            second.a = { x0, prebakedBasinHeight(x0, z0), z0 };
            second.b = { x1, prebakedBasinHeight(x1, z1), z1 };
            second.c = { x0, prebakedBasinHeight(x0, z1), z1 };
            prebakedBasinTriangles.push_back(first);
            prebakedBasinTriangles.push_back(second);
        }
    }
    heritage::physics::water::SurfaceHydrologyBakeReport prebakedBasinBake;
    if (!prebakedBasinWorld.loadOrBakeHydrology(
            prebakedBasinTriangles, {}, prebakedBasinBake)
        || !prebakedBasinBake.valid)
    {
        return false;
    }
    std::vector<std::uint8_t> prebakedPuddleTile;
    constexpr std::uint32_t kPrebakedRegressionResolution = 256u;
    if (!prebakedBasinWorld.hydrology().rasterPrebakedPuddleResponseTile(
            0, 0, kPrebakedRegressionResolution, prebakedPuddleTile)
        || prebakedPuddleTile.size()
            != static_cast<std::size_t>(kPrebakedRegressionResolution)
                * kPrebakedRegressionResolution * 3u)
    {
        return false;
    }
    const auto prebakedCapacityAt = [&](double worldX, double worldZ) {
        constexpr double tileSizeM = 10.0;
        const auto x = static_cast<std::uint32_t>(std::clamp(
            static_cast<int>(worldX / tileSizeM
                * kPrebakedRegressionResolution),
            0, static_cast<int>(kPrebakedRegressionResolution) - 1));
        const auto z = static_cast<std::uint32_t>(std::clamp(
            static_cast<int>(worldZ / tileSizeM
                * kPrebakedRegressionResolution),
            0, static_cast<int>(kPrebakedRegressionResolution) - 1));
        const std::size_t offset = (static_cast<std::size_t>(z)
            * kPrebakedRegressionResolution + x) * 3u;
        return prebakedPuddleTile[offset + 1u];
    };
    const std::uint8_t bowlEdgeCapacity = prebakedCapacityAt(0.25, 5.0);
    const std::uint8_t bowlShoulderCapacity = prebakedCapacityAt(3.0, 5.0);
    const std::uint8_t bowlCenterCapacity = prebakedCapacityAt(5.0, 5.0);
    // 0.60 mm is code 13 in the authoritative ladder, stored as 13*17 = 221.
    // This exact assertion prevents the bake from silently reverting to a
    // different depth curve while still passing a mere center>shoulder test.
    if (bowlCenterCapacity != 221u
        || bowlCenterCapacity <= bowlShoulderCapacity
        || bowlShoulderCapacity <= bowlEdgeCapacity)
    {
        return false;
    }

    // LIVETRACK15 regression: sub-millimetre authored depressions must survive
    // the mesh bake. The old 1 mm plateau tolerance could arbitrarily drain
    // these shallow minima by triangle index, which made the scene prebake too
    // sparse even though the 0.001 mm renderer could display them.
    SurfaceWorld shallowBasinWorld;
    std::vector<heritage::physics::StaticSceneTriangle> shallowBasinTriangles;
    const auto shallowHeight = [](float x, float z) {
        const float dx = x - 5.0f;
        const float dz = z - 5.0f;
        const float radiusSquared = dx * dx + dz * dz;
        return radiusSquared < 16.0f
            ? -0.00035f * (1.0f - radiusSquared / 16.0f)
            : 0.0f;
    };
    for (int z = 0; z < 10; ++z)
    {
        for (int x = 0; x < 10; ++x)
        {
            const float x0 = static_cast<float>(x);
            const float x1 = x0 + 1.0f;
            const float z0 = static_cast<float>(z);
            const float z1 = z0 + 1.0f;
            heritage::physics::StaticSceneTriangle first;
            first.a = { x0, shallowHeight(x0, z0), z0 };
            first.b = { x1, shallowHeight(x1, z0), z0 };
            first.c = { x1, shallowHeight(x1, z1), z1 };
            first.normal = { 0.0f, 1.0f, 0.0f };
            first.surfaceMaterial = SurfaceMaterial::Asphalt;
            first.surfaceProperties = asphaltDefaults;
            heritage::physics::StaticSceneTriangle second = first;
            second.a = { x0, shallowHeight(x0, z0), z0 };
            second.b = { x1, shallowHeight(x1, z1), z1 };
            second.c = { x0, shallowHeight(x0, z1), z1 };
            shallowBasinTriangles.push_back(first);
            shallowBasinTriangles.push_back(second);
        }
    }
    heritage::physics::water::SurfaceHydrologyBakeReport shallowBasinBake;
    if (!shallowBasinWorld.loadOrBakeHydrology(
            shallowBasinTriangles, {}, shallowBasinBake)
        || !shallowBasinBake.valid)
    {
        return false;
    }
    std::vector<std::uint8_t> shallowPuddleTile;
    if (!shallowBasinWorld.hydrology().rasterPrebakedPuddleResponseTile(
            0, 0, kPrebakedRegressionResolution, shallowPuddleTile))
    {
        return false;
    }
    const auto shallowCapacityAt = [&](double worldX, double worldZ) {
        const auto x = static_cast<std::uint32_t>(std::clamp(
            static_cast<int>(worldX / 10.0 * kPrebakedRegressionResolution),
            0, static_cast<int>(kPrebakedRegressionResolution) - 1));
        const auto z = static_cast<std::uint32_t>(std::clamp(
            static_cast<int>(worldZ / 10.0 * kPrebakedRegressionResolution),
            0, static_cast<int>(kPrebakedRegressionResolution) - 1));
        return shallowPuddleTile[(static_cast<std::size_t>(z)
            * kPrebakedRegressionResolution + x) * 3u + 1u];
    };
    if (shallowCapacityAt(5.0, 5.0) == 0u
        || shallowCapacityAt(5.0, 5.0) <= shallowCapacityAt(0.2, 5.0))
    {
        return false;
    }

    // LIVETRACK19 regression: the whole-scene bake also owns a compact 32x32
    // runoff-accumulation/capacity/flow payload for every authored 10 m tile. The 500 m renderer
    // consumes this cached payload directly; it must not raster collision
    // triangles progressively while driving.
    std::vector<std::uint8_t> prebakedFarPuddleTile;
    if (!prebakedBasinWorld.hydrology().prebakedFarPuddleResponseTile(
            0, 0, prebakedFarPuddleTile)
        || prebakedFarPuddleTile.size() != 32u * 32u * 3u
        || prebakedBasinWorld.hydrology().stats().prebakedWorldTileCount == 0u
        || prebakedBasinWorld.hydrology().stats().prebakedFarPayloadBytes == 0u
        || prebakedBasinWorld.hydrology().stats().prebakedFarPayloadBytes
            > prebakedBasinWorld.hydrology().stats().prebakedWorldTileCount * 1536ull)
    {
        return false;
    }

    // LIVETRACK11 regression: microscopic/depression storage on an ordinary
    // draining slope is rasterized directly from authored collision triangles.
    // The retired 0.5 m expansion produced a repeating 34/85/119/... capacity
    // ramp on this simple plane, visible in-game as tiled stripes and diamonds.
    // A non-basin slope must remain zero FREE-WATER capacity even across
    // triangle seams and 10 m tile ownership boundaries. The 0.1 mm material
    // film is now a separate runtime wetting state.
    SurfaceWorld slopedPrebakeWorld;
    std::vector<heritage::physics::StaticSceneTriangle> slopedPrebakeTriangles;
    constexpr float kSlopeRisePerMetre = 0.020f;
    const float slopeNormalInvLength = 1.0f / std::sqrt(
        1.0f + kSlopeRisePerMetre * kSlopeRisePerMetre);
    const heritage::math::Vec3 slopeNormal{
        -kSlopeRisePerMetre * slopeNormalInvLength,
        slopeNormalInvLength,
        0.0f };
    for (int z = 0; z < 10; ++z)
    {
        for (int x = 0; x < 20; ++x)
        {
            const float x0 = static_cast<float>(x);
            const float x1 = x0 + 1.0f;
            const float z0 = static_cast<float>(z);
            const float z1 = z0 + 1.0f;
            const float y0 = kSlopeRisePerMetre * x0;
            const float y1 = kSlopeRisePerMetre * x1;
            heritage::physics::StaticSceneTriangle first;
            first.a = { x0, y0, z0 };
            first.b = { x1, y1, z0 };
            first.c = { x1, y1, z1 };
            first.normal = slopeNormal;
            first.surfaceMaterial = SurfaceMaterial::Asphalt;
            first.surfaceProperties = asphaltDefaults;
            heritage::physics::StaticSceneTriangle second = first;
            second.a = { x0, y0, z0 };
            second.b = { x1, y1, z1 };
            second.c = { x0, y0, z1 };
            slopedPrebakeTriangles.push_back(first);
            slopedPrebakeTriangles.push_back(second);
        }
    }
    heritage::physics::water::SurfaceHydrologyBakeReport slopedPrebakeReport;
    if (!slopedPrebakeWorld.loadOrBakeHydrology(
            slopedPrebakeTriangles, {}, slopedPrebakeReport)
        || !slopedPrebakeReport.valid)
    {
        return false;
    }
    std::vector<std::uint8_t> slopedPuddleTile;
    if (!slopedPrebakeWorld.hydrology().rasterPrebakedPuddleResponseTile(
            0, 0, kPrebakedRegressionResolution, slopedPuddleTile))
    {
        return false;
    }
    std::uint8_t slopedMinimumCapacity = 255u;
    std::uint8_t slopedMaximumCapacity = 0u;
    for (std::size_t texel = 0;
         texel < slopedPuddleTile.size() / 3u; ++texel)
    {
        const std::uint8_t capacity = slopedPuddleTile[texel * 3u + 1u];
        slopedMinimumCapacity = std::min(slopedMinimumCapacity, capacity);
        slopedMaximumCapacity = std::max(slopedMaximumCapacity, capacity);
    }
    if (slopedMinimumCapacity != 0u
        || slopedMaximumCapacity != 0u)
    {
        return false;
    }

    // LIVETRACK19: a draining surface may have zero standing-water capacity but
    // still carry runoff. The new first topology byte is logarithmic upstream
    // flow accumulation; at least part of this long slope must collect more
    // upstream area than the source edge.
    std::uint8_t slopedMinimumRunoff = 255u;
    std::uint8_t slopedMaximumRunoff = 0u;
    for (std::size_t texel = 0; texel < slopedPuddleTile.size() / 3u; ++texel)
    {
        const std::uint8_t runoff = slopedPuddleTile[texel * 3u];
        slopedMinimumRunoff = std::min(slopedMinimumRunoff, runoff);
        slopedMaximumRunoff = std::max(slopedMaximumRunoff, runoff);
    }
    // Every genuinely sloped authored texel receives rainfall on itself, so
    // the baked runoff route must be nonzero even at the source side. Downstream
    // catchment accumulation must still increase above that local contribution.
    if (slopedMinimumRunoff == 0u || slopedMaximumRunoff <= slopedMinimumRunoff)
        return false;

    // LIVETRACK19 regression: the deterministic plateau links used internally
    // to make the basin graph acyclic are not physical flow. A perfectly flat
    // surface must therefore bake zero visible runoff accumulation and zero flow
    // direction everywhere; otherwise triangle order becomes a fake river.
    SurfaceWorld flatRunoffWorld;
    std::vector<heritage::physics::StaticSceneTriangle> flatRunoffTriangles(2u);
    flatRunoffTriangles[0].a = { 0.0f, 0.0f, 0.0f };
    flatRunoffTriangles[0].b = { 10.0f, 0.0f, 0.0f };
    flatRunoffTriangles[0].c = { 10.0f, 0.0f, 10.0f };
    flatRunoffTriangles[0].normal = { 0.0f, 1.0f, 0.0f };
    flatRunoffTriangles[0].surfaceMaterial = SurfaceMaterial::Asphalt;
    flatRunoffTriangles[0].surfaceProperties = asphaltDefaults;
    flatRunoffTriangles[1] = flatRunoffTriangles[0];
    flatRunoffTriangles[1].a = { 0.0f, 0.0f, 0.0f };
    flatRunoffTriangles[1].b = { 10.0f, 0.0f, 10.0f };
    flatRunoffTriangles[1].c = { 0.0f, 0.0f, 10.0f };
    heritage::physics::water::SurfaceHydrologyBakeReport flatRunoffBake;
    if (!flatRunoffWorld.loadOrBakeHydrology(
            flatRunoffTriangles, {}, flatRunoffBake)
        || !flatRunoffBake.valid)
    {
        return false;
    }
    std::vector<std::uint8_t> flatRunoffTile;
    if (!flatRunoffWorld.hydrology().rasterPrebakedPuddleResponseTile(
            0, 0, 32u, flatRunoffTile)
        || flatRunoffTile.size() != 32u * 32u * 3u)
    {
        return false;
    }
    for (std::size_t texel = 0; texel < flatRunoffTile.size() / 3u; ++texel)
    {
        if (flatRunoffTile[texel * 3u] != 0u
            || flatRunoffTile[texel * 3u + 2u] != 0u)
        {
            return false;
        }
    }

    std::vector<std::uint8_t> slopedPuddleTileNext;
    if (!slopedPrebakeWorld.hydrology().rasterPrebakedPuddleResponseTile(
            1, 0, kPrebakedRegressionResolution, slopedPuddleTileNext)
        || slopedPuddleTileNext.size() != slopedPuddleTile.size())
    {
        return false;
    }
    for (std::size_t texel = 0; texel < slopedPuddleTile.size() / 3u; ++texel)
    {
        if (slopedPuddleTile[texel * 3u + 1u]
            != slopedPuddleTileNext[texel * 3u + 1u])
        {
            return false;
        }
    }


    // LIVETRACK19 regression: runoff potential is not merely a yes/no flow
    // direction. A cambered draining road should accumulate progressively more
    // upstream catchment in its gutter/low channel than on the crown. This is
    // the static quantity the renderer uses to make curb-side runoff stronger
    // without inventing a second runtime water solver.
    SurfaceWorld gutterPrebakeWorld;
    std::vector<heritage::physics::StaticSceneTriangle> gutterTriangles;
    const auto gutterHeight = [](float x, float z) {
        return 0.015f * x + 0.004f * std::abs(z - 1.0f);
    };
    for (int z = 0; z < 10; ++z)
    {
        for (int x = 0; x < 20; ++x)
        {
            const float x0 = static_cast<float>(x);
            const float x1 = x0 + 1.0f;
            const float z0 = static_cast<float>(z);
            const float z1 = z0 + 1.0f;
            heritage::physics::StaticSceneTriangle first;
            first.a = { x0, gutterHeight(x0, z0), z0 };
            first.b = { x1, gutterHeight(x1, z0), z0 };
            first.c = { x1, gutterHeight(x1, z1), z1 };
            first.normal = { 0.0f, 1.0f, 0.0f };
            first.surfaceMaterial = SurfaceMaterial::Asphalt;
            first.surfaceProperties = asphaltDefaults;
            heritage::physics::StaticSceneTriangle second = first;
            second.a = { x0, gutterHeight(x0, z0), z0 };
            second.b = { x1, gutterHeight(x1, z1), z1 };
            second.c = { x0, gutterHeight(x0, z1), z1 };
            gutterTriangles.push_back(first);
            gutterTriangles.push_back(second);
        }
    }
    heritage::physics::water::SurfaceHydrologyBakeReport gutterBake;
    if (!gutterPrebakeWorld.loadOrBakeHydrology(gutterTriangles, {}, gutterBake)
        || !gutterBake.valid)
    {
        return false;
    }
    std::vector<std::uint8_t> gutterTile;
    if (!gutterPrebakeWorld.hydrology().rasterPrebakedPuddleResponseTile(
            0, 0, kPrebakedRegressionResolution, gutterTile)
        || gutterTile.size() != static_cast<std::size_t>(kPrebakedRegressionResolution)
            * kPrebakedRegressionResolution * 3u)
    {
        return false;
    }
    const auto gutterRunoffAt = [&](double worldX, double worldZ) {
        const auto sx = static_cast<std::uint32_t>(std::clamp(
            static_cast<int>(worldX / 10.0 * kPrebakedRegressionResolution),
            0, static_cast<int>(kPrebakedRegressionResolution) - 1));
        const auto sz = static_cast<std::uint32_t>(std::clamp(
            static_cast<int>(worldZ / 10.0 * kPrebakedRegressionResolution),
            0, static_cast<int>(kPrebakedRegressionResolution) - 1));
        return gutterTile[(static_cast<std::size_t>(sz)
            * kPrebakedRegressionResolution + sx) * 3u];
    };
    if (gutterRunoffAt(7.8, 1.0) <= gutterRunoffAt(7.8, 5.0))
        return false;

    // The production topology may change triangles at arbitrary authored mesh
    // edges, but a continuous plane must not reveal those edges in water depth.
    // Sample both sides of several diagonal splits and demand identical packed
    // capacity. This catches accidental return of per-cell or per-triangle
    // local water ceilings while still allowing exact mesh-shaped basins.
    for (int seam = 1; seam < 9; ++seam)
    {
        const double x = static_cast<double>(seam) + 0.48;
        const double zA = 4.48;
        const double zB = 4.52;
        if (prebakedCapacityAt(5.0, 5.0) == 0u) // keep basin lambda live above
            return false;
        const auto slopeCapacityAt = [&](double worldX, double worldZ) {
            const auto sx = static_cast<std::uint32_t>(std::clamp(
                static_cast<int>(worldX / 10.0 * kPrebakedRegressionResolution),
                0, static_cast<int>(kPrebakedRegressionResolution) - 1));
            const auto sz = static_cast<std::uint32_t>(std::clamp(
                static_cast<int>(worldZ / 10.0 * kPrebakedRegressionResolution),
                0, static_cast<int>(kPrebakedRegressionResolution) - 1));
            return slopedPuddleTile[(static_cast<std::size_t>(sz)
                * kPrebakedRegressionResolution + sx) * 3u + 1u];
        };
        if (slopeCapacityAt(x, zA) != slopeCapacityAt(x, zB))
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
    {
        hydrologyWorld.advancePresentation(0.5f);
        hydrologyReference.advance(
            hydrologyWorld.dynamicSurface(), spatialRain,
            hydrologyWorld.weatherOutput(), 0.5);
    }

    const auto leftWater = hydrologyReference.sample(
        hydrologyWorld.dynamicSurface(), { 3.125, 0.05, 0.25 });
    const auto pooledWater = hydrologyReference.sample(
        hydrologyWorld.dynamicSurface(), { 9.375, 0.00, 0.25 });
    hydrologyWorld.setGpuDynamicSurfaceAuthorityEnabled(true);
    hydrologyWorld.publishGpuDynamicSurfaceWaterSamples({
        { { 9.375, 0.00, 0.25 }, pooledWater.waterDepthM, 0.0, pooledWater.valid }
    });
    const auto spatialConditions = hydrologyWorld.localConditions(
        { 9.375f, 0.0f, 0.25f }, SurfaceMaterial::Asphalt, 0.0,
        asphaltDefaults);
    const auto spatialThermal = hydrologyWorld.dynamicSurface().sampleThermal(
        { 9.375, 0.0, 0.25 });
    const auto dynamicHydroStats = hydrologyReference.stats();
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

    // OPT02: the retired WATER14-WATER17 adaptive SurfaceHydrology solver and
    // its presentation-cell regressions are intentionally gone. Runtime water
    // behavior is validated through DynamicSurface below; immutable .hhyd v15
    // topology is validated by the prebaked basin/runoff tests above.

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
    const auto gpuContactResult = hydrologyWorld.applyHydrologyTireContact(
        { 9.375f, 0.0f, 0.25f }, tireWater);
    std::vector<heritage::physics::GpuDynamicSurfaceTireEvent> gpuTireEvents;
    hydrologyWorld.consumeGpuDynamicSurfaceTireEvents(gpuTireEvents);
    if (!gpuContactResult.valid
        || std::abs(gpuContactResult.initialWaterDepthM - depthBeforeTire) > 1.0e-9
        || gpuTireEvents.empty())
    {
        return false;
    }

    heritage::physics::dynamicsurface::DynamicSurfaceHydroTireInput referenceTire;
    referenceTire.deltaTimeSeconds = tireWater.deltaTimeSeconds;
    referenceTire.contactPatchLengthM = tireWater.contactPatchLengthM;
    referenceTire.contactPatchWidthM = tireWater.contactPatchWidthM;
    referenceTire.contactPatchAreaM2 = tireWater.contactPatchAreaM2;
    referenceTire.normalLoadN = tireWater.normalLoadN;
    referenceTire.nominalLoadN = tireWater.nominalLoadN;
    referenceTire.forwardSpeedMps = tireWater.forwardSpeedMps;
    referenceTire.lateralSpeedMps = tireWater.lateralSpeedMps;
    referenceTire.treadVoidRatio = tireWater.treadVoidRatio;
    referenceTire.slipDissipationWatts = tireWater.slipDissipationWatts;
    referenceTire.forward = tireWater.forward;
    referenceTire.right = tireWater.right;
    for (int contact = 0; contact < 800; ++contact)
    {
        hydrologyReference.applyTireContact(
            hydrologyWorld.dynamicSurface(), { 9.375, 0.0, 0.25 }, referenceTire);
    }
    const auto clearedWater = hydrologyReference.sample(
        hydrologyWorld.dynamicSurface(), { 9.375, 0.00, 0.25 });
    if (!clearedWater.valid || clearedWater.waterDepthM >= depthBeforeTire
        || hydrologyReference.stats().tireContactCount != 800u
        || hydrologyReference.stats().cumulativeTireClearedVolumeM3 <= 0.0)
    {
        return false;
    }
    // Production water strategy is fixed in LIVETRACK10. Runtime strategy
    // mutation/reset tests were removed with the Water Laboratory API; the
    // hydrology conservation and tire-clearing regressions above remain.

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
    heritage::tests::reference::DynamicSurfaceHydrologyReference coverHydroReference;
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
    {
        coverWorld.advancePresentation(0.25f);
        coverHydroReference.advance(
            coverWorld.dynamicSurface(), spatialRain,
            coverWorld.weatherOutput(), 0.25);
    }
    const auto coveredLower = coverHydroReference.sample(
        coverWorld.dynamicSurface(), { 3.125, 0.0, 0.25 });
    const auto exposedUpper = coverHydroReference.sample(
        coverWorld.dynamicSurface(), { 3.125, 5.0, 0.25 });
    const auto exposedOpen = coverHydroReference.sample(
        coverWorld.dynamicSurface(), { 21.875, 0.0, 0.25 });
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

    // OPT02 removed the old CPU hydrology presentation-cell gather. Water
    // presentation is GPU/material reconstruction from immutable .hhyd topology.

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
    heritage::tests::reference::DynamicSurfaceHydrologyReference multiHydroReference;
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
    // LIVETRACK10B multi-source bounded fallback: the two <=100m interest
    // regions are a union, not a midpoint average. Detailed CPU Hydro exists
    // only around those sources; the unseen point 1.5 km between them must not
    // materialize a 256x256 page or accumulate hidden background water.
    for (int step = 0; step < 122; ++step)
    {
        multiSourceHydrologyWorld.advancePresentation(0.5f);
        multiHydroReference.advance(
            multiSourceHydrologyWorld.dynamicSurface(), multiSourceRain,
            multiSourceHydrologyWorld.weatherOutput(), 0.5);
    }
    const auto multiNearA = multiHydroReference.sample(
        multiSourceHydrologyWorld.dynamicSurface(), { 0.25, 0.0, 0.25 });
    const auto multiMidpoint = multiHydroReference.sample(
        multiSourceHydrologyWorld.dynamicSurface(), { 1500.25, 0.0, 0.25 });
    const auto multiNearB = multiHydroReference.sample(
        multiSourceHydrologyWorld.dynamicSurface(), { 2999.75, 0.0, 0.25 });
    const auto multiStats = multiHydroReference.stats();
    if (!multiNearA.valid || multiMidpoint.valid || !multiNearB.valid
        || multiNearA.waterDepthM <= 0.0
        || multiNearB.waterDepthM <= 0.0
        || multiSourceHydrologyWorld.dynamicSurface().interestSources().size() != 2u
        || multiStats.cadence30HzPages != 0u
        || multiStats.cadence20HzPages != 0u
        || multiStats.cadence6HzPages == 0u
        || multiStats.cadenceDistantPages != 0u
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

#include "PhysicsRegressionCommon.hpp"

#include "../Physics/Surfaces/SurfaceField.hpp"
#include "../Physics/Surfaces/SurfaceWorld.hpp"

#include <cmath>
#include <cstddef>

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

    return true;
}

} // namespace heritage::tests

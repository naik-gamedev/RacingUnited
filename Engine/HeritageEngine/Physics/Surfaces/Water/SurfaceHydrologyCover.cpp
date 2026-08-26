#include "SurfaceHydrology.hpp"

#include <cmath>
#include <cstdint>

namespace heritage::physics::water {
namespace {

bool finitePosition(const heritage::math::DVec3& value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

} // namespace

bool SurfaceHydrology::hasPrecipitationCoverAbove(
    const heritage::math::DVec3& globalPosition,
    double minimumClearanceM,
    double maximumHeightM) const
{
    if (!finitePosition(globalPosition)
        || !std::isfinite(minimumClearanceM)
        || !std::isfinite(maximumHeightM)
        || minimumClearanceM < 0.0
        || maximumHeightM <= minimumClearanceM)
    {
        return false;
    }

    // LIVETRACK15 world-scale shelter query. The mesh prebake covers the full
    // authored scene even when the legacy 0.5 m CPU fallback reaches its safety
    // cap, so precipitation cover must use the same triangle-space world cache
    // rather than a partial support raster. This keeps bridges/tunnels/roofs
    // correct on 200 km^2 scenes without allocating a fine world grid.
    constexpr double kTileWorldSizeM = 10.0;
    const std::int32_t tileX = static_cast<std::int32_t>(
        std::floor(globalPosition.x / kTileWorldSizeM));
    const std::int32_t tileZ = static_cast<std::int32_t>(
        std::floor(globalPosition.z / kTileWorldSizeM));
    const PrebakedTriangleTileSpan* bucket = prebakedTriangleTileSpan(tileX, tileZ);
    if (bucket)
    {
        const double minimumElevation = globalPosition.y + minimumClearanceM;
        const double maximumElevation = globalPosition.y + maximumHeightM;
        for (std::uint32_t bucketIndex = 0u; bucketIndex < bucket->count; ++bucketIndex)
        {
            const std::uint64_t flatIndex = bucket->firstIndex + bucketIndex;
            if (flatIndex >= m_prebakedTriangleTileIndices.size())
                continue;
            const std::int32_t triangleIndex = m_prebakedTriangleTileIndices[
                static_cast<std::size_t>(flatIndex)];
            if (triangleIndex < 0
                || static_cast<std::size_t>(triangleIndex) >= m_prebakedTriangles.size())
            {
                continue;
            }
            const PrebakedTriangle& triangle = m_prebakedTriangles[
                static_cast<std::size_t>(triangleIndex)];
            const double denominator =
                (triangle.b.z - triangle.c.z) * (triangle.a.x - triangle.c.x)
                + (triangle.c.x - triangle.b.x) * (triangle.a.z - triangle.c.z);
            if (!std::isfinite(denominator) || std::abs(denominator) <= 1.0e-12)
                continue;
            const double wa = ((triangle.b.z - triangle.c.z)
                    * (globalPosition.x - triangle.c.x)
                + (triangle.c.x - triangle.b.x)
                    * (globalPosition.z - triangle.c.z)) / denominator;
            const double wb = ((triangle.c.z - triangle.a.z)
                    * (globalPosition.x - triangle.c.x)
                + (triangle.a.x - triangle.c.x)
                    * (globalPosition.z - triangle.c.z)) / denominator;
            const double wc = 1.0 - wa - wb;
            constexpr double kInsideEpsilon = -1.0e-9;
            if (wa < kInsideEpsilon || wb < kInsideEpsilon || wc < kInsideEpsilon)
                continue;
            const double elevationM = wa * triangle.a.y
                + wb * triangle.b.y + wc * triangle.c.y;
            if (elevationM >= minimumElevation && elevationM <= maximumElevation)
                return true;
        }
        return false;
    }

    // Legacy/headless fallback for caches produced without mesh topology.
    if (m_cells.empty())
        return false;
    const std::int64_t x = static_cast<std::int64_t>(
        std::floor(globalPosition.x / m_description.cellSizeM));
    const std::int64_t z = static_cast<std::int64_t>(
        std::floor(globalPosition.z / m_description.cellSizeM));
    const std::int64_t firstLayer = static_cast<std::int64_t>(
        std::floor((globalPosition.y + minimumClearanceM)
            / m_description.verticalLayerSizeM));
    const std::int64_t lastLayer = static_cast<std::int64_t>(
        std::floor((globalPosition.y + maximumHeightM)
            / m_description.verticalLayerSizeM));
    const double minimumElevation = globalPosition.y + minimumClearanceM;
    const double maximumElevation = globalPosition.y + maximumHeightM;
    for (std::int64_t layer = firstLayer; layer <= lastLayer; ++layer)
    {
        const auto found = m_lookup.find({ x, z, layer });
        if (found == m_lookup.end())
            continue;
        const Cell& cell = m_cells[static_cast<std::size_t>(found->second)];
        if (cell.elevationM >= minimumElevation
            && cell.elevationM <= maximumElevation
            && cell.normal.y > static_cast<float>(m_description.minimumUpwardNormal))
        {
            return true;
        }
    }
    return false;
}

} // namespace heritage::physics::water

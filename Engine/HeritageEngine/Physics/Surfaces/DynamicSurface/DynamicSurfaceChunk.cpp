#include "DynamicSurfaceChunk.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace heritage::physics::dynamicsurface {

namespace {

std::uint8_t pageCoordinate(double localM)
{
    (void)localM;
    return 0u;
}

std::uint32_t stateCellCoordinate(double localM)
{
    const double bounded = std::clamp(
        localM, 0.0, std::nextafter(kChunkSizeM, 0.0));
    const auto index = static_cast<std::uint32_t>(
        std::floor(bounded / kTrackAuthorityTexelPitchM));
    return std::min(index, kTrackAuthorityResolution - 1u);
}

std::size_t stateCellIndex(std::uint32_t x, std::uint32_t z)
{
    return static_cast<std::size_t>(z) * kTrackAuthorityResolution + x;
}

std::uint32_t hydroCellCoordinate(double localM)
{
    const double bounded = std::clamp(
        localM, 0.0, std::nextafter(kChunkSizeM, 0.0));
    const auto index = static_cast<std::uint32_t>(
        std::floor(bounded / kHydroAuthorityTexelPitchM));
    return std::min(index, kHydroAuthorityResolution - 1u);
}

std::size_t hydroCellIndex(std::uint32_t x, std::uint32_t z)
{
    return static_cast<std::size_t>(z) * kHydroAuthorityResolution + x;
}

} // namespace

DynamicSurfaceChunk::DynamicSurfaceChunk(ChunkAddress address)
    : m_address(address)
{
}

heritage::math::DVec3 DynamicSurfaceChunk::globalOrigin() const
{
    return {
        static_cast<double>(m_address.x) * kChunkSizeM,
        0.0,
        static_cast<double>(m_address.z) * kChunkSizeM
    };
}

void DynamicSurfaceChunk::setSurfaceSheetCount(std::uint32_t count)
{
    m_surfaceSheetCount = std::max<std::uint32_t>(1, count);
    m_dirtyBySheet.resize(m_surfaceSheetCount);
}

PageAddress DynamicSurfaceChunk::pageForGlobalPosition(
    const heritage::math::DVec3& globalPosition,
    std::uint16_t surfaceSheet) const
{
    const heritage::math::DVec3 origin = globalOrigin();
    PageAddress result;
    result.sheet = static_cast<std::uint16_t>(
        std::min<std::uint32_t>(surfaceSheet, m_surfaceSheetCount - 1));
    result.x = pageCoordinate(globalPosition.x - origin.x);
    result.z = pageCoordinate(globalPosition.z - origin.z);
    return result;
}

void DynamicSurfaceChunk::markPageDirty(const PageAddress& page)
{
    if (page.sheet >= m_dirtyBySheet.size())
        return;
    m_dirtyBySheet[page.sheet].set(pageIndex(page.x, page.z));
}

bool DynamicSurfaceChunk::isPageDirty(const PageAddress& page) const
{
    if (page.sheet >= m_dirtyBySheet.size())
        return false;
    return m_dirtyBySheet[page.sheet].test(pageIndex(page.x, page.z));
}

std::vector<PageAddress> DynamicSurfaceChunk::dirtyPages() const
{
    std::vector<PageAddress> result;
    for (std::size_t sheet = 0; sheet < m_dirtyBySheet.size(); ++sheet)
    {
        const DirtyMask& mask = m_dirtyBySheet[sheet];
        for (std::uint32_t z = 0; z < kPagesPerAxis; ++z)
        {
            for (std::uint32_t x = 0; x < kPagesPerAxis; ++x)
            {
                if (!mask.test(pageIndex(
                        static_cast<std::uint8_t>(x),
                        static_cast<std::uint8_t>(z))))
                {
                    continue;
                }
                result.push_back({
                    static_cast<std::uint16_t>(sheet),
                    static_cast<std::uint8_t>(x),
                    static_cast<std::uint8_t>(z)
                });
            }
        }
    }
    return result;
}

void DynamicSurfaceChunk::setStaticSurfaceData(
    std::vector<StaticSurfacePatchTriangle> triangles,
    std::vector<StaticSurfaceSheet> sheets,
    std::vector<StaticSurfaceBarrierSegment> barriers,
    std::vector<StaticSurfaceDrainRegion> drains)
{
    m_staticTriangles = std::move(triangles);
    m_staticSheets = std::move(sheets);
    m_staticBarriers = std::move(barriers);
    m_staticDrains = std::move(drains);
    setSurfaceSheetCount(static_cast<std::uint32_t>(m_staticSheets.size()));

    m_triangleIndicesBySheetPage.assign(
        static_cast<std::size_t>(m_surfaceSheetCount) * kPagesPerSheet,
        {});
    m_triangleIndicesByXzPage.assign(kPagesPerSheet, {});
    const std::size_t trackCellsPerTile =
        static_cast<std::size_t>(kTrackAuthorityResolution)
        * kTrackAuthorityResolution;
    m_triangleIndicesBySheetStateCell.assign(
        static_cast<std::size_t>(m_surfaceSheetCount) * trackCellsPerTile, {});
    m_triangleIndicesByXzStateCell.assign(trackCellsPerTile, {});

    // One persistent page covers the whole 100m sheet. Static geometry gets a
    // 64x64 Track acceleration bin. Hydro has no separate persistent triangle-bin grid.
    std::vector<std::bitset<kPagesPerSheet>> coverage(m_surfaceSheetCount);
    const heritage::math::DVec3 origin = globalOrigin();
    for (std::size_t triangleIndex = 0;
         triangleIndex < m_staticTriangles.size();
         ++triangleIndex)
    {
        const StaticSurfacePatchTriangle& triangle = m_staticTriangles[triangleIndex];
        if (triangle.surfaceSheetId >= coverage.size())
            continue;
        const double minX = std::min({ triangle.a.x, triangle.b.x, triangle.c.x }) - origin.x;
        const double maxX = std::max({ triangle.a.x, triangle.b.x, triangle.c.x }) - origin.x;
        const double minZ = std::min({ triangle.a.z, triangle.b.z, triangle.c.z }) - origin.z;
        const double maxZ = std::max({ triangle.a.z, triangle.b.z, triangle.c.z }) - origin.z;
        const std::uint8_t x0 = pageCoordinate(minX);
        const std::uint8_t x1 = pageCoordinate(maxX);
        const std::uint8_t z0 = pageCoordinate(minZ);
        const std::uint8_t z1 = pageCoordinate(maxZ);
        auto& mask = coverage[triangle.surfaceSheetId];
        for (std::uint32_t z = z0; z <= z1; ++z)
        {
            for (std::uint32_t x = x0; x <= x1; ++x)
            {
                mask.set(pageIndex(static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(z)));
                const std::size_t xzIndex = pageIndex(
                    static_cast<std::uint8_t>(x),
                    static_cast<std::uint8_t>(z));
                const std::size_t sheetPageIndex =
                    static_cast<std::size_t>(triangle.surfaceSheetId)
                        * kPagesPerSheet
                    + xzIndex;
                m_triangleIndicesBySheetPage[sheetPageIndex].push_back(
                    static_cast<std::uint32_t>(triangleIndex));
                m_triangleIndicesByXzPage[xzIndex].push_back(
                    static_cast<std::uint32_t>(triangleIndex));
            }
        }

        const std::uint32_t cellX0 = stateCellCoordinate(minX);
        const std::uint32_t cellX1 = stateCellCoordinate(maxX);
        const std::uint32_t cellZ0 = stateCellCoordinate(minZ);
        const std::uint32_t cellZ1 = stateCellCoordinate(maxZ);
        for (std::uint32_t z = cellZ0; z <= cellZ1; ++z)
        {
            for (std::uint32_t x = cellX0; x <= cellX1; ++x)
            {
                const std::size_t cell = stateCellIndex(x, z);
                const std::size_t sheetCell =
                    static_cast<std::size_t>(triangle.surfaceSheetId)
                        * trackCellsPerTile
                    + cell;
                m_triangleIndicesBySheetStateCell[sheetCell].push_back(
                    static_cast<std::uint32_t>(triangleIndex));
                m_triangleIndicesByXzStateCell[cell].push_back(
                    static_cast<std::uint32_t>(triangleIndex));
            }
        }

    }

    m_coveredPages.clear();
    for (std::uint32_t sheet = 0; sheet < coverage.size(); ++sheet)
    {
        for (std::uint32_t z = 0; z < kPagesPerAxis; ++z)
        {
            for (std::uint32_t x = 0; x < kPagesPerAxis; ++x)
            {
                if (!coverage[sheet].test(pageIndex(
                        static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(z))))
                    continue;
                m_coveredPages.push_back({
                    static_cast<std::uint16_t>(sheet),
                    static_cast<std::uint8_t>(x),
                    static_cast<std::uint8_t>(z) });
            }
        }
    }
}

void DynamicSurfaceChunk::clearStaticSurfaceData()
{
    m_staticTriangles.clear();
    m_staticSheets.clear();
    m_staticBarriers.clear();
    m_staticDrains.clear();
    m_coveredPages.clear();
    m_triangleIndicesBySheetPage.clear();
    m_triangleIndicesByXzPage.clear();
    m_triangleIndicesBySheetStateCell.clear();
    m_triangleIndicesByXzStateCell.clear();
    setSurfaceSheetCount(1);
}

const std::vector<std::uint32_t>& DynamicSurfaceChunk::staticTriangleIndicesForPage(
    const PageAddress& page) const
{
    static const std::vector<std::uint32_t> empty;
    if (page.sheet >= m_surfaceSheetCount
        || page.x >= kPagesPerAxis
        || page.z >= kPagesPerAxis)
    {
        return empty;
    }
    const std::size_t index =
        static_cast<std::size_t>(page.sheet) * kPagesPerSheet
        + pageIndex(page.x, page.z);
    if (index >= m_triangleIndicesBySheetPage.size())
        return empty;
    return m_triangleIndicesBySheetPage[index];
}

const std::vector<std::uint32_t>& DynamicSurfaceChunk::staticTriangleIndicesForXzPage(
    std::uint8_t x,
    std::uint8_t z) const
{
    static const std::vector<std::uint32_t> empty;
    if (x >= kPagesPerAxis || z >= kPagesPerAxis)
        return empty;
    const std::size_t index = pageIndex(x, z);
    if (index >= m_triangleIndicesByXzPage.size())
        return empty;
    return m_triangleIndicesByXzPage[index];
}


const std::vector<std::uint32_t>& DynamicSurfaceChunk::staticTriangleIndicesForStateCell(
    std::uint16_t sheet,
    std::uint32_t x,
    std::uint32_t z) const
{
    static const std::vector<std::uint32_t> empty;
    if (sheet >= m_surfaceSheetCount
        || x >= kTrackAuthorityResolution
        || z >= kTrackAuthorityResolution)
    {
        return empty;
    }
    const std::size_t cellsPerTile =
        static_cast<std::size_t>(kTrackAuthorityResolution)
        * kTrackAuthorityResolution;
    const std::size_t index = static_cast<std::size_t>(sheet) * cellsPerTile
        + stateCellIndex(x, z);
    if (index >= m_triangleIndicesBySheetStateCell.size())
        return empty;
    return m_triangleIndicesBySheetStateCell[index];
}

const std::vector<std::uint32_t>& DynamicSurfaceChunk::staticTriangleIndicesForXzStateCell(
    std::uint32_t x,
    std::uint32_t z) const
{
    static const std::vector<std::uint32_t> empty;
    if (x >= kTrackAuthorityResolution || z >= kTrackAuthorityResolution)
        return empty;
    const std::size_t index = stateCellIndex(x, z);
    if (index >= m_triangleIndicesByXzStateCell.size())
        return empty;
    return m_triangleIndicesByXzStateCell[index];
}

void DynamicSurfaceChunk::clearDirtyPages()
{
    for (DirtyMask& mask : m_dirtyBySheet)
        mask.reset();
}

std::size_t DynamicSurfaceChunk::pageIndex(std::uint8_t x, std::uint8_t z)
{
    const std::size_t boundedX = std::min<std::size_t>(x, kPagesPerAxis - 1);
    const std::size_t boundedZ = std::min<std::size_t>(z, kPagesPerAxis - 1);
    return boundedZ * kPagesPerAxis + boundedX;
}

} // namespace heritage::physics::dynamicsurface

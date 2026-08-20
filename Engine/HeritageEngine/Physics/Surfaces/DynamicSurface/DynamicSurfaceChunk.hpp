#pragma once

#include <array>
#include <bitset>
#include <cstdint>
#include <vector>

#include "DynamicSurfaceTypes.hpp"
#include "DynamicSurfaceStaticData.hpp"

namespace heritage::physics::dynamicsurface {

// CPU-side identity/dirty-state object for a persistent 100m world chunk.
// DSURF02 maps these identities through the software virtual page pool into
// bounded GPU texture-array layers. No 4096x4096 CPU arrays are allocated.
class DynamicSurfaceChunk
{
public:
    explicit DynamicSurfaceChunk(ChunkAddress address = {});

    ChunkAddress address() const { return m_address; }
    heritage::math::DVec3 globalOrigin() const;

    std::uint32_t surfaceSheetCount() const { return m_surfaceSheetCount; }
    void setSurfaceSheetCount(std::uint32_t count);

    PageAddress pageForGlobalPosition(
        const heritage::math::DVec3& globalPosition,
        std::uint16_t surfaceSheet) const;

    void markPageDirty(const PageAddress& page);
    bool isPageDirty(const PageAddress& page) const;
    std::vector<PageAddress> dirtyPages() const;
    void clearDirtyPages();

    void setStaticSurfaceData(
        std::vector<StaticSurfacePatchTriangle> triangles,
        std::vector<StaticSurfaceSheet> sheets,
        std::vector<StaticSurfaceBarrierSegment> barriers,
        std::vector<StaticSurfaceDrainRegion> drains);
    void clearStaticSurfaceData();

    const std::vector<StaticSurfacePatchTriangle>& staticTriangles() const
    {
        return m_staticTriangles;
    }
    // LIVETRACK03 water deliberately has no surface-sheet binning: one 256x256
    // X/Z Hydro field is shared by every rendered/collidable surface inside the
    // 100m chunk. Track/thermal keeps its separate 64x64 sheet-aware bins.
    const std::vector<std::uint32_t>& staticTriangleIndicesForPage(
        const PageAddress& page) const;
    const std::vector<std::uint32_t>& staticTriangleIndicesForXzPage(
        std::uint8_t x,
        std::uint8_t z) const;
    const std::vector<std::uint32_t>& staticTriangleIndicesForStateCell(
        std::uint16_t sheet,
        std::uint32_t x,
        std::uint32_t z) const;
    const std::vector<std::uint32_t>& staticTriangleIndicesForXzStateCell(
        std::uint32_t x,
        std::uint32_t z) const;
    const std::vector<StaticSurfaceSheet>& staticSheets() const
    {
        return m_staticSheets;
    }
    const std::vector<StaticSurfaceBarrierSegment>& staticBarriers() const
    {
        return m_staticBarriers;
    }
    const std::vector<StaticSurfaceDrainRegion>& staticDrains() const
    {
        return m_staticDrains;
    }
    const std::vector<PageAddress>& coveredPages() const
    {
        return m_coveredPages;
    }

private:
    using DirtyMask = std::bitset<kPagesPerSheet>;

    static std::size_t pageIndex(std::uint8_t x, std::uint8_t z);

    ChunkAddress m_address{};
    std::uint32_t m_surfaceSheetCount = 1;
    std::vector<DirtyMask> m_dirtyBySheet{ 1 };
    std::vector<StaticSurfacePatchTriangle> m_staticTriangles;
    std::vector<StaticSurfaceSheet> m_staticSheets;
    std::vector<StaticSurfaceBarrierSegment> m_staticBarriers;
    std::vector<StaticSurfaceDrainRegion> m_staticDrains;
    std::vector<PageAddress> m_coveredPages;
    std::vector<std::vector<std::uint32_t>> m_triangleIndicesBySheetPage;
    std::vector<std::vector<std::uint32_t>> m_triangleIndicesByXzPage;
    std::vector<std::vector<std::uint32_t>> m_triangleIndicesBySheetStateCell;
    std::vector<std::vector<std::uint32_t>> m_triangleIndicesByXzStateCell;
};

} // namespace heritage::physics::dynamicsurface

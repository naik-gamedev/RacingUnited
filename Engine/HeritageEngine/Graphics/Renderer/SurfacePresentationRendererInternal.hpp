#pragma once

#include <cstddef>
#include <cstdint>

namespace heritage::graphics::surface_presentation_detail {

struct TireMarkGpuRecord
{
    float startLocal[3]{};
    float endLocal[3]{};
    float startNormal[3]{};
    float endNormal[3]{};
    float startRight[3]{};
    float endRight[3]{};
    float startData[4]{};
    float endData[4]{};
    float misc[4]{};
};

static_assert(sizeof(TireMarkGpuRecord) == sizeof(float) * 30);

struct MarbleCellGpuRecord
{
    float centerLocal[3]{};
    float normal[3]{};
    float forward[3]{};
    float state[4]{};
    float misc[4]{};
};

static_assert(sizeof(MarbleCellGpuRecord) == sizeof(float) * 17);

struct MovingRubberGpuRecord
{
    float centerRelative[3]{};
    float axisRight[3]{};
    float axisForward[3]{};
    float axisNormal[3]{};
    float shape[4]{};
    float state[4]{};
    float misc[4]{};
};

static_assert(sizeof(MovingRubberGpuRecord) == sizeof(float) * 24);

inline constexpr std::uint32_t kTireMarkGpuMaximumSegmentsPerPage = 8192;
inline constexpr std::uint32_t kMarbleGpuMaximumCellsPerPage = 8192;

inline std::uint32_t tireMarkGpuPageCapacity(std::size_t pageIndex)
{
    // Sparse 100 m chunks should not reserve a full megabyte just because one
    // tire touched them once. Pages grow geometrically; dense skid pads still
    // converge on 8192-record pages and keep draw-call count bounded.
    if (pageIndex == 0)
        return 256;
    if (pageIndex == 1)
        return 1024;
    if (pageIndex == 2)
        return 4096;
    return kTireMarkGpuMaximumSegmentsPerPage;
}
inline std::uint32_t marbleGpuPageCapacity(std::size_t pageIndex)
{
    if (pageIndex == 0)
        return 256;
    if (pageIndex == 1)
        return 1024;
    if (pageIndex == 2)
        return 4096;
    return kMarbleGpuMaximumCellsPerPage;
}

inline constexpr double kMarbleGpuDrawDistanceM = 220.0;
inline constexpr double kMarbleGpuDetailedDistanceM = 55.0;
inline constexpr double kMovingRubberGpuDrawDistanceM = 120.0;

inline constexpr double kTireMarkGpuDrawDistanceM = 500.0;
inline constexpr double kTireMarkGpuDetailedDistanceM = 200.0;
inline constexpr double kTireMarkGpuCapDistanceM = 100.0;

} // namespace heritage::graphics::surface_presentation_detail

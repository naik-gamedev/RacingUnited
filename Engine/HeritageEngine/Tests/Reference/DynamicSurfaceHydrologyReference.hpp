#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "../../Physics/Surfaces/DynamicSurface/DynamicSurfaceSystem.hpp"
#include "../../Physics/Surfaces/SurfaceWeather.hpp"

namespace heritage::tests::reference {

using namespace heritage::physics::dynamicsurface;
using heritage::physics::SurfaceWeatherDescription;
using heritage::physics::SurfaceWeatherOutput;

using heritage::physics::dynamicsurface::DynamicSurfaceSystem;

// OPT03C TEST-ONLY REFERENCE: retired CPU Hydro model preserved outside production.
// One persistent X/Z Hydro field per 100m chunk. The historical authoritative
// dynamic state is exactly one packed RGBA4 texel per 256x256 cell:
//   R = water level (16 levels, dry..32mm)
//   G = dry-line strength (16 levels)
//   B = flow X (signed 4-bit direction component)
//   A = flow Z (signed 4-bit direction component)
// No vertical Hydro sheets and no second Hydro-resolution hierarchy exist.
class DynamicSurfaceHydrologyReference
{
public:
    void clear();
    void resetWater();

    void advance(
        DynamicSurfaceSystem& owner,
        const SurfaceWeatherDescription& weather,
        const SurfaceWeatherOutput& weatherOutput,
        double deltaTimeSeconds);

    DynamicSurfaceHydroSample sample(
        const DynamicSurfaceSystem& owner,
        const heritage::math::DVec3& globalPosition) const;

    DynamicSurfaceHydroTireResult applyTireContact(
        DynamicSurfaceSystem& owner,
        const heritage::math::DVec3& globalPosition,
        const DynamicSurfaceHydroTireInput& input);

    bool rasterPage(
        const DynamicSurfaceSystem& owner,
        const VirtualPageAddress& address,
        std::uint32_t outputResolution,
        std::vector<std::uint16_t>& hydroRgba4) const;

    std::uint64_t pageRevision(const VirtualPageAddress& address) const;

    bool setUniformWaterDepthForLab(
        DynamicSurfaceSystem& owner,
        double waterDepthM);

    const DynamicSurfaceHydroStats& stats() const { return m_stats; }

private:
    struct StateCell
    {
        std::uint16_t rgba4 = 0u;
    };

    struct HydroPage
    {
        VirtualPageAddress address{};
        std::vector<StateCell> stateCells;

        // Hydrology parameters are page-wide in LIVETRACK03. The 256x256
        // dynamic field carries the detailed water/dry-line/flow state; keeping
        // another 256x256 static float raster would defeat the memory goal.
        float infiltrationMps = 0.0f;
        float drainageMps = 0.0f;
        float flowRoughness = 0.02f;
        float depressionStorageM = 0.0f;

        // Quantized atmospheric forcing retains sub-level accumulation per
        // water code. This avoids light rain disappearing without adding a
        // hidden full-resolution float water plane.
        std::array<double, 16> atmosphericCarryM{};
        double dryLineWashCarryM = 0.0;

        std::uint64_t revision = 1;
        double stepAccumulatorSeconds = 0.0;
        double cadenceHz = 0.0;
        double lastTouchedSeconds = 0.0;
        bool staticReady = false;

        // OPT02C: exact O(1) water-stat maintenance for tire contacts. The
        // histogram avoids rescanning all 65,536 texels after every wheel
        // contact while still keeping wet-count/volume/max-depth exact.
        std::array<std::size_t, 16> waterLevelHistogram{};
        std::size_t wetCellCount = 0u;
        double cachedWaterVolumeM3 = 0.0;
        double cachedMaximumWaterDepthM = 0.0;
        double cachedMaximumFlowMagnitude = 0.0;
    };

    struct LocatedCell
    {
        HydroPage* page = nullptr;
        const HydroPage* constPage = nullptr;
        std::uint32_t x = 0;
        std::uint32_t z = 0;
        float supportHeightM = 0.0f;
    };

    HydroPage* ensurePage(
        DynamicSurfaceSystem& owner,
        const VirtualPageAddress& address,
        const SurfaceWeatherOutput* initialWeather = nullptr);
    bool buildStaticPage(
        const DynamicSurfaceSystem& owner,
        HydroPage& page) const;

    LocatedCell locateMutable(
        DynamicSurfaceSystem& owner,
        const heritage::math::DVec3& globalPosition);
    LocatedCell locateConst(
        const DynamicSurfaceSystem& owner,
        const heritage::math::DVec3& globalPosition) const;

    std::optional<VirtualPageAddress> neighbourPage(
        const DynamicSurfaceSystem& owner,
        const VirtualPageAddress& address,
        int pageDx,
        int pageDz) const;

    bool simulateEnvironment(
        HydroPage& page,
        const SurfaceWeatherOutput& weatherOutput,
        double stepSeconds);
    bool simulateFlow(
        DynamicSurfaceSystem& owner,
        HydroPage& page,
        double stepSeconds,
        double pageHz);

    void refreshPageStats(HydroPage& page);
    void updateWaterStatsForCellChange(
        HydroPage& page,
        std::uint16_t beforePacked,
        std::uint16_t afterPacked);
    void refreshStats();

    static std::size_t cellIndex(std::uint32_t x, std::uint32_t z)
    {
        return static_cast<std::size_t>(z) * kHydroAuthorityResolution + x;
    }

    std::map<VirtualPageAddress, HydroPage> m_pages;
    DynamicSurfaceHydroStats m_stats{};
    double m_elapsedSeconds = 0.0;
};

} // namespace heritage::tests::reference

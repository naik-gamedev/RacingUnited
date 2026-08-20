#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include "DynamicSurfacePagePool.hpp"
#include "../SurfaceWeather.hpp"

namespace heritage::physics::dynamicsurface {

class DynamicSurfaceSystem;

// DSURF04 authoritative surface-temperature state. Temperature is stored
// against the same persistent VirtualPageAddress as Hydro/Track state and is
// therefore tied to the world surface sheet rather than a camera, vehicle or
// global weather scalar. Thermal/Track stays at 64x64 (1.5625m/cell);
// LIVETRACK03 water is independently 256x256 RGBA4 (0.390625m/cell).
class DynamicSurfaceThermal
{
public:
    void clear();
    void reset();

    void advance(
        DynamicSurfaceSystem& owner,
        const SurfaceWeatherDescription& weather,
        const SurfaceWeatherOutput& weatherOutput,
        double ambientTemperatureC,
        bool surfaceTemperatureOverrideEnabled,
        double surfaceTemperatureOverrideC,
        double deltaTimeSeconds);

    DynamicSurfaceThermalSample sample(
        const DynamicSurfaceSystem& owner,
        const heritage::math::DVec3& globalPosition) const;

    DynamicSurfaceThermalTireResult applyTireContact(
        DynamicSurfaceSystem& owner,
        const heritage::math::DVec3& globalPosition,
        const DynamicSurfaceThermalTireInput& input);

    bool rasterTrackPage(
        const DynamicSurfaceSystem& owner,
        const VirtualPageAddress& address,
        std::uint32_t outputResolution,
        std::vector<float>& trackRgba) const;

    std::uint64_t pageRevision(const VirtualPageAddress& address) const;

    const DynamicSurfaceThermalStats& stats() const { return m_stats; }

private:
    struct StaticCell
    {
        float supportHeightM = 0.0f;
        float initialTemperatureC = 20.0f;
        float thermalTimeScale = 1.0f;
        float heatCapacityJPerM2K = 220000.0f;
        bool valid = false;
        bool skyExposed = true;
    };

    struct StateCell
    {
        float surfaceTemperatureC = 20.0f;
    };

    struct ThermalPage
    {
        VirtualPageAddress address{};
        std::vector<StaticCell> staticCells;
        std::vector<StateCell> stateCells;
        std::uint64_t revision = 1;
        double stepAccumulatorSeconds = 0.0;
        double cadenceHz = 0.0;
        double lastTouchedSeconds = 0.0;
        bool staticReady = false;
    };

    struct LocatedCell
    {
        ThermalPage* page = nullptr;
        const ThermalPage* constPage = nullptr;
        std::uint32_t x = 0;
        std::uint32_t z = 0;
        float supportHeightM = 0.0f;
    };

    ThermalPage* ensurePage(
        DynamicSurfaceSystem& owner,
        const VirtualPageAddress& address);
    bool buildStaticPage(
        const DynamicSurfaceSystem& owner,
        ThermalPage& page) const;

    LocatedCell locateMutable(
        DynamicSurfaceSystem& owner,
        const heritage::math::DVec3& globalPosition);
    LocatedCell locateConst(
        const DynamicSurfaceSystem& owner,
        const heritage::math::DVec3& globalPosition) const;

    bool simulateEnvironment(
        const DynamicSurfaceSystem& owner,
        ThermalPage& page,
        const SurfaceWeatherDescription& weather,
        const SurfaceWeatherOutput& weatherOutput,
        double ambientTemperatureC,
        bool surfaceTemperatureOverrideEnabled,
        double surfaceTemperatureOverrideC,
        double stepSeconds);

    void refreshStats();

    static std::size_t cellIndex(std::uint32_t x, std::uint32_t z)
    {
        return static_cast<std::size_t>(z) * kTrackAuthorityResolution + x;
    }

    std::map<VirtualPageAddress, ThermalPage> m_pages;
    DynamicSurfaceThermalStats m_stats{};
    double m_elapsedSeconds = 0.0;
};

} // namespace heritage::physics::dynamicsurface

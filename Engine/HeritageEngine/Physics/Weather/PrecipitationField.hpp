#pragma once

#include <cstdint>
#include <vector>

#include "RainMicrophysics.hpp"
#include "../../Core/Math/Math.hpp"

namespace heritage::physics::weather {

struct PrecipitationFieldDescription
{
    std::uint64_t seed = 0x4845524954414745ull; // "HERITAGE"
    double horizontalCellSizeM = 2.5;
    double verticalCellSizeM = 5.0;
};


struct RegionalWeatherSample
{
    bool valid = false;
    double cloudCover = 0.0;
    double relativeHumidity = 0.55;
    double currentRateMmPerHour = 0.0;
    double stormIntensity = 0.0;
};

struct RainRadarSnapshot
{
    bool valid = false;
    std::uint32_t resolution = 0;
    double centerGlobalX = 0.0;
    double centerGlobalZ = 0.0;
    double halfRangeM = 0.0;
    std::vector<float> currentRateMmPerHour;
    std::vector<float> cumulativePrecipitationMm;
    double maximumCurrentRateMmPerHour = 0.0;
    double maximumCumulativePrecipitationMm = 0.0;
};

struct RainRepresentative
{
    bool valid = false;
    std::uint64_t identity = 0;
    heritage::math::DVec3 globalPosition{ 0.0, 0.0, 0.0 };
    heritage::math::Vec3 velocityMps{ 0.0f, 0.0f, 0.0f };
    double diameterMm = 0.0;
    double massKg = 0.0;
    double terminalVelocityMps = 0.0;
    double phase01 = 0.0;
};

// WEATHER07A deterministic world precipitation field. It does not allocate one
// object per real raindrop. Instead, world-cell identity + weather seed + time
// reconstruct statistically correct representative trajectories from the shared
// RainDropPopulation. Cameras merely choose which representatives to render.
class PrecipitationField
{
public:
    bool setDescription(const PrecipitationFieldDescription& description);
    const PrecipitationFieldDescription& description() const
    {
        return m_description;
    }

    void clear();
    void configureRain(
        double rainfallRateMmPerHour,
        double windSpeedMps,
        double windDirectionDegrees);
    void configureWeather(
        double rainfallRateMmPerHour,
        double relativeHumidity,
        double cloudCover,
        double windSpeedMps,
        double windDirectionDegrees);
    void setElapsedSeconds(double elapsedSeconds);
    void advance(double deltaTimeSeconds);

    bool raining() const { return m_population.valid; }
    const RainDropPopulation& rainPopulation() const { return m_population; }
    double windSpeedMps() const { return m_windSpeedMps; }
    double windDirectionDegrees() const { return m_windDirectionDegrees; }
    heritage::math::Vec3 windVelocityMps() const { return m_windVelocityMps; }
    // WEATHER10A first-order atmospheric wind profile. The authored wind is
    // the near-surface vector; speed and heading vary smoothly with height so
    // storm cells/cloud layers need not translate as one rigid sheet.
    heritage::math::Vec3 atmosphericWindVelocityMps(double heightAboveSurfaceM) const;
    heritage::math::Vec3 weatherSteeringWindVelocityMps() const
    {
        return atmosphericWindVelocityMps(2000.0);
    }
    double elapsedSeconds() const { return m_elapsedSeconds; }

    RegionalWeatherSample regionalWeatherSample(
        double globalX,
        double globalZ) const;
    void buildRainRadarSnapshot(
        double centerGlobalX,
        double centerGlobalZ,
        double halfRangeM,
        std::uint32_t resolution,
        RainRadarSnapshot& out) const;

    RainRepresentative sampleRainRepresentative(
        std::int64_t cellX,
        std::int64_t cellY,
        std::int64_t cellZ,
        std::uint32_t lane = 0) const;

private:
    PrecipitationFieldDescription m_description{};
    RainDropPopulation m_population{};
    double m_authoredRainfallRateMmPerHour = 0.0;
    double m_authoredRelativeHumidity = 0.55;
    double m_authoredCloudCover = 0.20;
    double m_windSpeedMps = 0.0;
    double m_windDirectionDegrees = 45.0;
    heritage::math::Vec3 m_windVelocityMps{ 0.0f, 0.0f, 0.0f };
    double m_elapsedSeconds = 0.0;
};

} // namespace heritage::physics::weather

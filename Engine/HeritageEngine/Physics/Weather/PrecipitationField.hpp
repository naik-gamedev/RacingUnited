#pragma once

#include <cstdint>

#include "RainMicrophysics.hpp"
#include "../../Core/Math/Math.hpp"

namespace heritage::physics::weather {

struct PrecipitationFieldDescription
{
    std::uint64_t seed = 0x4845524954414745ull; // "HERITAGE"
    double horizontalCellSizeM = 2.5;
    double verticalCellSizeM = 5.0;
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
    void setElapsedSeconds(double elapsedSeconds);
    void advance(double deltaTimeSeconds);

    bool raining() const { return m_population.valid; }
    const RainDropPopulation& rainPopulation() const { return m_population; }
    double windSpeedMps() const { return m_windSpeedMps; }
    double windDirectionDegrees() const { return m_windDirectionDegrees; }
    heritage::math::Vec3 windVelocityMps() const { return m_windVelocityMps; }
    double elapsedSeconds() const { return m_elapsedSeconds; }

    RainRepresentative sampleRainRepresentative(
        std::int64_t cellX,
        std::int64_t cellY,
        std::int64_t cellZ,
        std::uint32_t lane = 0) const;

private:
    PrecipitationFieldDescription m_description{};
    RainDropPopulation m_population{};
    double m_windSpeedMps = 0.0;
    double m_windDirectionDegrees = 45.0;
    heritage::math::Vec3 m_windVelocityMps{ 0.0f, 0.0f, 0.0f };
    double m_elapsedSeconds = 0.0;
};

} // namespace heritage::physics::weather

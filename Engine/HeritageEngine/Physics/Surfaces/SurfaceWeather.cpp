#include "SurfaceWeather.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::physics {
namespace {

constexpr double kMillimetresPerMetre = 1000.0;
constexpr double kSecondsPerHour = 3600.0;

double clamp01(double value)
{
    return std::clamp(value, 0.0, 1.0);
}

double smoothStep(double edge0, double edge1, double value)
{
    const double t = clamp01((value - edge0) / (edge1 - edge0));
    return t * t * (3.0 - 2.0 * t);
}

double millimetresPerHourToMetresPerSecond(double value)
{
    return value / (kMillimetresPerMetre * kSecondsPerHour);
}

double metresPerSecondToMillimetresPerHour(double value)
{
    return value * kMillimetresPerMetre * kSecondsPerHour;
}

struct WeatherRates
{
    double precipitationMps = 0.0;
    double drainageMps = 0.0;
    double evaporationMps = 0.0;
};

WeatherRates ratesFor(
    const SurfaceWeatherDescription& d,
    double ambientTemperatureC,
    double waterFilmDepthM)
{
    WeatherRates result;
    result.precipitationMps = millimetresPerHourToMetresPerSecond(
        d.precipitationRateMmPerHour);

    // A thin film cannot immediately drain at the full authored drainage
    // capacity. Capacity approaches the authored value once water begins to
    // connect across the road texture and drainage paths.
    const double drainageActivation = waterFilmDepthM
        / std::max(waterFilmDepthM + 0.00050, 1.0e-9);
    result.drainageMps = millimetresPerHourToMetresPerSecond(
        d.drainageRateMmPerHour) * drainageActivation;

    const double exposedFilm = smoothStep(0.00001, 0.00030, waterFilmDepthM);
    const double temperatureScale = std::clamp(
        0.35 + 0.035 * (ambientTemperatureC + 10.0), 0.15, 2.25);
    const double windScale = std::clamp(1.0 + 0.12 * d.windSpeedMps, 1.0, 5.0);
    result.evaporationMps = millimetresPerHourToMetresPerSecond(
        d.referenceEvaporationRateMmPerHour)
        * (1.0 - d.relativeHumidity) * temperatureScale * windScale
        * exposedFilm;
    return result;
}

} // namespace

bool validSurfaceWeatherDescription(const SurfaceWeatherDescription& d)
{
    const double values[] = {
        d.precipitationRateMmPerHour,
        d.relativeHumidity,
        d.windSpeedMps,
        d.windDirectionDegrees,
        d.cloudCover,
        d.drainageRateMmPerHour,
        d.referenceEvaporationRateMmPerHour,
        d.maximumWaterFilmDepthM,
        d.roadThermalTimeConstantSeconds,
        d.maximumSolarHeatingC
    };
    for (double value : values)
    {
        if (!std::isfinite(value))
            return false;
    }
    return d.precipitationRateMmPerHour >= 0.0
        && d.precipitationRateMmPerHour <= 500.0
        && d.relativeHumidity >= 0.0 && d.relativeHumidity <= 1.0
        && d.windSpeedMps >= 0.0 && d.windSpeedMps <= 100.0
        && d.windDirectionDegrees >= 0.0 && d.windDirectionDegrees <= 360.0
        && d.cloudCover >= 0.0 && d.cloudCover <= 1.0
        && d.drainageRateMmPerHour >= 0.0
        && d.drainageRateMmPerHour <= 500.0
        && d.referenceEvaporationRateMmPerHour >= 0.0
        && d.referenceEvaporationRateMmPerHour <= 50.0
        && d.maximumWaterFilmDepthM >= 0.0001
        && d.maximumWaterFilmDepthM <= 0.050
        && d.roadThermalTimeConstantSeconds >= 1.0
        && d.roadThermalTimeConstantSeconds <= 86400.0
        && d.maximumSolarHeatingC >= 0.0
        && d.maximumSolarHeatingC <= 60.0;
}

SurfaceWeatherOutput evaluateSurfaceWeather(
    const SurfaceWeatherDescription& d,
    const SurfaceWeatherState& state)
{
    SurfaceWeatherOutput out;
    if (!d.enabled || !validSurfaceWeatherDescription(d))
        return out;

    const WeatherRates rates = ratesFor(
        d, state.roadTemperatureC, std::max(state.waterFilmDepthM, 0.0));
    out.valid = true;
    out.waterFilmDepthM = std::clamp(
        state.waterFilmDepthM, 0.0, d.maximumWaterFilmDepthM);
    out.effectiveWetness = smoothStep(0.00003, 0.00150, out.waterFilmDepthM);
    out.roadTemperatureC = state.roadTemperatureC;
    out.windSpeedMps = d.windSpeedMps;
    out.windDirectionDegrees = d.windDirectionDegrees;
    const double windRadians = d.windDirectionDegrees
        * (3.14159265358979323846 / 180.0);
    out.windVelocityXMps = std::sin(windRadians) * d.windSpeedMps;
    out.windVelocityZMps = std::cos(windRadians) * d.windSpeedMps;
    out.precipitationRateMmPerHour = d.precipitationRateMmPerHour;
    out.drainageRateMmPerHour = metresPerSecondToMillimetresPerHour(
        rates.drainageMps);
    out.evaporationRateMmPerHour = metresPerSecondToMillimetresPerHour(
        rates.evaporationMps);
    return out;
}

SurfaceWeatherOutput advanceSurfaceWeather(
    const SurfaceWeatherDescription& d,
    double ambientTemperatureC,
    double initialRoadTemperatureC,
    double deltaTimeSeconds,
    SurfaceWeatherState& state)
{
    if (!d.enabled || !validSurfaceWeatherDescription(d)
        || !std::isfinite(ambientTemperatureC)
        || !std::isfinite(initialRoadTemperatureC)
        || !std::isfinite(deltaTimeSeconds) || deltaTimeSeconds <= 0.0)
    {
        return evaluateSurfaceWeather(d, state);
    }

    if (!state.initialized)
    {
        state = {};
        state.initialized = true;
        state.roadTemperatureC = initialRoadTemperatureC;
    }

    // Bounded internal steps make long laboratory advances and ordinary frame
    // updates converge on the same result without allowing a film to go
    // negative during a dry/windy interval.
    double remaining = std::min(deltaTimeSeconds, 3600.0);
    while (remaining > 0.0)
    {
        const double dt = std::min(remaining, 0.25);
        const WeatherRates rates = ratesFor(
            d, ambientTemperatureC, state.waterFilmDepthM);
        const double availableRemovalMps = rates.drainageMps
            + rates.evaporationMps;
        const double previousDepth = state.waterFilmDepthM;
        const double rawDepth = std::max(
            previousDepth + (rates.precipitationMps - availableRemovalMps) * dt,
            0.0);
        state.waterFilmDepthM = std::min(rawDepth, d.maximumWaterFilmDepthM);

        const double actualAdded = rates.precipitationMps * dt;
        const double availableDepth = previousDepth + actualAdded;
        const double desiredDrainage = rates.drainageMps * dt;
        const double actualDrainage = std::min(desiredDrainage, availableDepth);
        const double remainingForEvaporation = std::max(
            availableDepth - actualDrainage, 0.0);
        const double actualEvaporation = std::min(
            rates.evaporationMps * dt, remainingForEvaporation);
        state.cumulativePrecipitationMm += actualAdded * 1000.0;
        state.cumulativeDrainageMm += actualDrainage * 1000.0;
        state.cumulativeEvaporationMm += actualEvaporation * 1000.0;
        state.cumulativeOverflowMm += std::max(
            rawDepth - d.maximumWaterFilmDepthM, 0.0) * 1000.0;

        const double wetness = smoothStep(0.00003, 0.00150,
            state.waterFilmDepthM);
        const double rainCoolingC = std::min(
            d.precipitationRateMmPerHour / 40.0, 4.0);
        const double evaporationCoolingC = std::min(
            metresPerSecondToMillimetresPerHour(rates.evaporationMps) * 0.8,
            3.0);
        const double targetRoadTemperatureC = ambientTemperatureC
            + d.maximumSolarHeatingC * (1.0 - d.cloudCover)
                * (1.0 - 0.75 * wetness)
            - rainCoolingC * wetness - evaporationCoolingC;
        // DSURF04 moved persistent thermal inertia into the page-addressed
        // Dynamic Surface Track plane. Keep this legacy/public weather field
        // as an instantaneous environmental reference only, so there is no
        // second independent road-temperature authority evolving in parallel.
        state.roadTemperatureC = targetRoadTemperatureC;
        state.elapsedSeconds += dt;
        remaining -= dt;
    }
    return evaluateSurfaceWeather(d, state);
}

} // namespace heritage::physics

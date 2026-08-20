#pragma once

namespace heritage::physics {

// Deterministic world-scale liquid-weather baseline. This intentionally owns
// rainfall, a bounded road-water film, drainage, evaporation, wind exposure
// and road temperature. Spatial puddle flow and snow accumulation remain
// separate future surface-field providers.
struct SurfaceWeatherDescription
{
    bool enabled = false;
    double precipitationRateMmPerHour = 0.0;
    double relativeHumidity = 0.55;
    double windSpeedMps = 2.0;
    // WEATHER07A heading of horizontal air motion: 0 deg -> world +Z,
    // 90 deg -> world +X. Kept explicit so rain trajectories never rely on a
    // hidden hard-coded renderer direction.
    double windDirectionDegrees = 45.0;
    double cloudCover = 0.20;
    double drainageRateMmPerHour = 4.0;
    double referenceEvaporationRateMmPerHour = 0.35;
    double maximumWaterFilmDepthM = 0.006;
    double roadThermalTimeConstantSeconds = 300.0;
    double maximumSolarHeatingC = 18.0;
};

struct SurfaceWeatherState
{
    bool initialized = false;
    double elapsedSeconds = 0.0;
    double waterFilmDepthM = 0.0;
    // DSURF04 compatibility/environment reference only. Persistent local road
    // temperature authority lives in Dynamic Surface Track pages; this scalar
    // is no longer an independently relaxed road-temperature state.
    double roadTemperatureC = 20.0;
    double cumulativePrecipitationMm = 0.0;
    double cumulativeDrainageMm = 0.0;
    double cumulativeEvaporationMm = 0.0;
    double cumulativeOverflowMm = 0.0;
};

struct SurfaceWeatherOutput
{
    bool valid = false;
    double waterFilmDepthM = 0.0;
    double effectiveWetness = 0.0;
    // Compatibility/reference temperature for callers without a baked Dynamic
    // Surface. Tires on baked scene surfaces use DSURF04 local Track state.
    double roadTemperatureC = 20.0;
    double windSpeedMps = 0.0;
    double windDirectionDegrees = 45.0;
    double windVelocityXMps = 0.0;
    double windVelocityZMps = 0.0;
    double precipitationRateMmPerHour = 0.0;
    double drainageRateMmPerHour = 0.0;
    double evaporationRateMmPerHour = 0.0;
};

bool validSurfaceWeatherDescription(const SurfaceWeatherDescription& value);

SurfaceWeatherOutput evaluateSurfaceWeather(
    const SurfaceWeatherDescription& description,
    const SurfaceWeatherState& state);

SurfaceWeatherOutput advanceSurfaceWeather(
    const SurfaceWeatherDescription& description,
    double ambientTemperatureC,
    double initialRoadTemperatureC,
    double deltaTimeSeconds,
    SurfaceWeatherState& state);

} // namespace heritage::physics

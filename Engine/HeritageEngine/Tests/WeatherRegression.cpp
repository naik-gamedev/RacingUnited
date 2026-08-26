#include "PhysicsRegressionCommon.hpp"

#include "../Physics/Surfaces/SurfaceWorld.hpp"
#include "../Physics/Weather/PrecipitationField.hpp"
#include "../Physics/Weather/RainMicrophysics.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace heritage::tests {
namespace {

bool closeEnough(double a, double b, double tolerance)
{
    return std::abs(a - b) <= tolerance;
}

} // namespace

bool physicalRainPopulationAndWorldFieldAreDeterministic()
{
    using namespace heritage::physics;
    using namespace heritage::physics::weather;

    const RainDropPopulation light = buildRainDropPopulation(2.0);
    const RainDropPopulation storm = buildRainDropPopulation(80.0);
    if (!light.valid || !storm.valid)
        return false;

    // 1 mm/h of water equals 1 kg/m^2/hour. The population's integrated liquid
    // mass flux must therefore agree with the authoritative hydrology input.
    const double expectedStormMassFlux = 80.0 / 3600.0;
    const bool massFluxMatches = closeEnough(
        storm.massFluxKgPerM2PerSecond,
        expectedStormMassFlux,
        expectedStormMassFlux * 1.0e-9 + 1.0e-12);

    // Heavier rain should flatten the Marshall-Palmer slope and shift the
    // population toward larger drops, never toward impossible 20-40 m/s streaks.
    const bool populationTrend =
        storm.lambdaPerMm < light.lambdaPerMm
        && storm.numberWeightedMeanDiameterMm
            > light.numberWeightedMeanDiameterMm
        && storm.fluxWeightedMeanTerminalVelocityMps
            > light.fluxWeightedMeanTerminalVelocityMps
        && storm.fluxWeightedMeanTerminalVelocityMps > 2.0
        && storm.fluxWeightedMeanTerminalVelocityMps < 10.0;

    const double halfMmSpeed = rainDropTerminalVelocityMps(0.5);
    const double oneMmSpeed = rainDropTerminalVelocityMps(1.0);
    const double sixMmSpeed = rainDropTerminalVelocityMps(6.0);
    const bool terminalVelocityCurve =
        halfMmSpeed > 1.5 && halfMmSpeed < 2.5
        && oneMmSpeed > halfMmSpeed
        && sixMmSpeed > oneMmSpeed
        && sixMmSpeed < 10.0;

    SurfaceWorld world;
    SurfaceWeatherDescription weather;
    weather.enabled = true;
    weather.precipitationRateMmPerHour = 25.0;
    weather.relativeHumidity = 0.90;
    weather.windSpeedMps = 12.0;
    weather.windDirectionDegrees = 90.0;
    weather.cloudCover = 1.0;
    if (!world.setWeather(weather))
        return false;

    world.advancePresentation(1.0f);
    const auto& field = world.precipitation();
    const RainRepresentative a = field.sampleRainRepresentative(17, 4, -9, 2);
    const RainRepresentative b = field.sampleRainRepresentative(17, 4, -9, 2);
    if (!a.valid || !b.valid)
        return false;

    const bool deterministicIdentity =
        a.identity == b.identity
        && closeEnough(a.globalPosition.x, b.globalPosition.x, 1.0e-12)
        && closeEnough(a.globalPosition.y, b.globalPosition.y, 1.0e-12)
        && closeEnough(a.globalPosition.z, b.globalPosition.z, 1.0e-12)
        && closeEnough(a.diameterMm, b.diameterMm, 1.0e-12);

    // Heading 90 deg is +X in Heritage weather convention. Representative
    // drops must inherit horizontal air motion without corrupting fall speed.
    const bool windTrajectory =
        a.velocityMps.x > 0.1f
        && std::abs(a.velocityMps.z) < 0.001f
        && a.velocityMps.y < -0.1f
        && a.terminalVelocityMps < 10.0;

    const RainRepresentative laterBefore =
        field.sampleRainRepresentative(17, 4, -9, 2);
    world.advancePresentation(0.5f);
    const RainRepresentative laterAfter =
        world.precipitation().sampleRainRepresentative(17, 4, -9, 2);
    const bool worldTimeMovesDrop =
        laterAfter.valid
        && laterAfter.identity == laterBefore.identity
        && !closeEnough(
            laterAfter.globalPosition.y,
            laterBefore.globalPosition.y,
            1.0e-9);

    PrecipitationField regionalField;
    regionalField.configureWeather(80.0, 0.82, 0.58, 12.0, 90.0);
    regionalField.setElapsedSeconds(1.5);
    const auto regionalA = regionalField.regionalWeatherSample(1200.0, -3400.0);
    const auto regionalB = regionalField.regionalWeatherSample(1200.0, -3400.0);
    const auto regionalFar = regionalField.regionalWeatherSample(19400.0, 15100.0);
    const auto regionalFarther = regionalField.regionalWeatherSample(-22100.0, 27600.0);
    const bool regionalDeterministic =
        regionalA.valid && regionalB.valid && regionalFar.valid && regionalFarther.valid
        && closeEnough(regionalA.cloudCover, regionalB.cloudCover, 1.0e-12)
        && closeEnough(regionalA.relativeHumidity, regionalB.relativeHumidity, 1.0e-12)
        && closeEnough(regionalA.currentRateMmPerHour,
            regionalB.currentRateMmPerHour, 1.0e-12)
        && regionalA.cloudCover >= 0.0 && regionalA.cloudCover <= 1.0
        && regionalA.relativeHumidity >= 0.0 && regionalA.relativeHumidity <= 1.0
        && regionalA.currentRateMmPerHour >= 0.0
        && regionalA.currentRateMmPerHour <= 80.0 + 1.0e-9;
    // CLOUDURP15H4A: H4 intentionally remaps authored cloud coverage so
    // 82% + 80 mm/h is allowed to saturate to a fully overcast storm cell.
    // Spatial variation is therefore tested under a moderate non-saturated
    // weather state instead of incorrectly requiring variation at the storm
    // ceiling. This keeps the spatial-field safety net meaningful.
    PrecipitationField variationField;
    variationField.configureWeather(20.0, 0.02, 0.72, 12.0, 90.0);
    variationField.setElapsedSeconds(1.5);
    const auto variationA = variationField.regionalWeatherSample(1200.0, -3400.0);
    const auto variationFar = variationField.regionalWeatherSample(19400.0, 15100.0);
    const auto variationFarther = variationField.regionalWeatherSample(-22100.0, 27600.0);
    const double regionalSpan = std::max({
        std::abs(variationA.cloudCover - variationFar.cloudCover),
        std::abs(variationA.cloudCover - variationFarther.cloudCover),
        std::abs(variationA.currentRateMmPerHour
            - variationFar.currentRateMmPerHour) / 80.0,
        std::abs(variationA.currentRateMmPerHour
            - variationFarther.currentRateMmPerHour) / 80.0 });
    const bool regionalSpatialVariation = variationA.valid
        && variationFar.valid
        && variationFarther.valid
        && regionalSpan > 0.01;

    const auto surfaceWind = regionalField.atmosphericWindVelocityMps(0.0);
    const auto cloudWind = regionalField.atmosphericWindVelocityMps(2500.0);
    const auto upperWind = regionalField.atmosphericWindVelocityMps(5500.0);
    const double surfaceWindSpeed = std::hypot(surfaceWind.x, surfaceWind.z);
    const double cloudWindSpeed = std::hypot(cloudWind.x, cloudWind.z);
    const double upperWindSpeed = std::hypot(upperWind.x, upperWind.z);
    const bool atmosphericWindShear =
        closeEnough(surfaceWindSpeed, 12.0, 1.0e-5)
        && cloudWindSpeed > surfaceWindSpeed
        && upperWindSpeed > cloudWindSpeed
        && std::abs(cloudWind.z) > 0.01f;

    heritage::physics::weather::RainRadarSnapshot radar;
    regionalField.buildRainRadarSnapshot(
        0.0, 0.0, 10000.0, 16u, radar);
    const bool radarValid = radar.valid
        && radar.resolution == 16u
        && radar.currentRateMmPerHour.size() == 256u
        && radar.cumulativePrecipitationMm.size() == 256u
        && radar.maximumCurrentRateMmPerHour >= 0.0
        && radar.maximumCurrentRateMmPerHour <= 80.0 + 1.0e-6
        && radar.maximumCumulativePrecipitationMm >= 0.0;

    std::cout
        << "rain_microphysics light_mean_mm="
        << light.numberWeightedMeanDiameterMm
        << " storm_mean_mm=" << storm.numberWeightedMeanDiameterMm
        << " storm_flux_v_mps=" << storm.fluxWeightedMeanTerminalVelocityMps
        << " storm_mass_flux=" << storm.massFluxKgPerM2PerSecond
        << " sample_d_mm=" << a.diameterMm
        << " sample_vy_mps=" << a.velocityMps.y
        << " sample_vx_mps=" << a.velocityMps.x
        << " regional_cloud=" << regionalA.cloudCover
        << " regional_rain_mmph=" << regionalA.currentRateMmPerHour
        << " radar_peak_mmph=" << radar.maximumCurrentRateMmPerHour
        << " moderate_weather_span=" << regionalSpan
        << " surface_wind_mps=" << surfaceWindSpeed
        << " cloud_wind_mps=" << cloudWindSpeed
        << " upper_wind_mps=" << upperWindSpeed
        << "\n";

    return massFluxMatches
        && populationTrend
        && terminalVelocityCurve
        && deterministicIdentity
        && windTrajectory
        && worldTimeMovesDrop
        && regionalDeterministic
        && regionalSpatialVariation
        && atmosphericWindShear
        && radarValid;
}

} // namespace heritage::tests

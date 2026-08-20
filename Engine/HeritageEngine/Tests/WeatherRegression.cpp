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

    std::cout
        << "rain_microphysics light_mean_mm="
        << light.numberWeightedMeanDiameterMm
        << " storm_mean_mm=" << storm.numberWeightedMeanDiameterMm
        << " storm_flux_v_mps=" << storm.fluxWeightedMeanTerminalVelocityMps
        << " storm_mass_flux=" << storm.massFluxKgPerM2PerSecond
        << " sample_d_mm=" << a.diameterMm
        << " sample_vy_mps=" << a.velocityMps.y
        << " sample_vx_mps=" << a.velocityMps.x
        << "\n";

    return massFluxMatches
        && populationTrend
        && terminalVelocityCurve
        && deterministicIdentity
        && windTrajectory
        && worldTimeMovesDrop;
}

} // namespace heritage::tests

#include "RainMicrophysics.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::physics::weather {
namespace {

constexpr double kWaterDensityKgPerM3 = 1000.0;
constexpr double kPi = 3.1415926535897932384626433832795;
constexpr double kMarshallPalmerN0PerM3PerMm = 8000.0;
constexpr double kMarshallPalmerLambdaCoefficient = 4.1;
constexpr double kMarshallPalmerLambdaRainExponent = -0.21;
constexpr int kIntegrationBins = 512;

constexpr double clamp01(double value)
{
    return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
}

double dropVolumeM3(double diameterMm)
{
    const double diameterM = std::max(diameterMm, 0.0) * 0.001;
    return (kPi / 6.0) * diameterM * diameterM * diameterM;
}

} // namespace

double rainDropTerminalVelocityMps(double diameterMm)
{
    if (!std::isfinite(diameterMm) || diameterMm <= 0.0)
        return 0.0;

    // Atlas, Srivastava & Sekhon (1973) is accurate for ordinary raindrops at
    // and above roughly 0.5 mm and naturally approaches ~9.65 m/s for the
    // largest stable drops. Do not use its negative small-D extrapolation.
    constexpr double kAtlasTransitionDiameterMm = 0.50;
    const auto atlas = [](double dMm) {
        return 9.65 - 10.3 * std::exp(-0.6 * dMm);
    };

    if (diameterMm < kAtlasTransitionDiameterMm)
    {
        // Continuous drizzle continuation. WEATHER07A intentionally keeps this
        // conservative until the later atmospheric microphysics tier introduces
        // Beard-style density/viscosity corrections for tiny cloud/drizzle drops.
        const double transitionSpeed = atlas(kAtlasTransitionDiameterMm);
        return std::max(
            transitionSpeed * diameterMm / kAtlasTransitionDiameterMm,
            0.0);
    }
    return std::clamp(atlas(diameterMm), 0.0, 9.65);
}

RainDropPopulation buildRainDropPopulation(double rainfallRateMmPerHour)
{
    RainDropPopulation result;
    if (!std::isfinite(rainfallRateMmPerHour)
        || rainfallRateMmPerHour <= 0.0)
    {
        return result;
    }

    result.rainfallRateMmPerHour = rainfallRateMmPerHour;
    result.lambdaPerMm = kMarshallPalmerLambdaCoefficient
        * std::pow(rainfallRateMmPerHour, kMarshallPalmerLambdaRainExponent);
    if (!std::isfinite(result.lambdaPerMm) || result.lambdaPerMm <= 0.0)
        return {};

    const double minD = result.minimumDiameterMm;
    const double maxD = result.maximumDiameterMm;
    const double step = (maxD - minD) / static_cast<double>(kIntegrationBins);

    double baseNumber = 0.0;
    double baseNumberDiameter = 0.0;
    double baseNumberVelocity = 0.0;
    double baseVolume = 0.0;
    double baseVolumeDiameter = 0.0;
    double baseVolumeFlux = 0.0;
    double baseFluxVelocity = 0.0;

    for (int i = 0; i < kIntegrationBins; ++i)
    {
        const double diameterMm = minD + (static_cast<double>(i) + 0.5) * step;
        const double concentrationPerM3PerMm = kMarshallPalmerN0PerM3PerMm
            * std::exp(-result.lambdaPerMm * diameterMm);
        const double countPerM3 = concentrationPerM3PerMm * step;
        const double volumeM3 = dropVolumeM3(diameterMm);
        const double terminalVelocityMps = rainDropTerminalVelocityMps(diameterMm);
        const double volumeFluxMps = countPerM3 * volumeM3 * terminalVelocityMps;

        baseNumber += countPerM3;
        baseNumberDiameter += countPerM3 * diameterMm;
        baseNumberVelocity += countPerM3 * terminalVelocityMps;
        baseVolume += countPerM3 * volumeM3;
        baseVolumeDiameter += countPerM3 * volumeM3 * diameterMm;
        baseVolumeFlux += volumeFluxMps;
        baseFluxVelocity += volumeFluxMps * terminalVelocityMps;
    }

    // R mm/h -> metres of liquid water per second.
    const double requestedVolumeFluxMps = rainfallRateMmPerHour / 3.6e6;
    if (baseVolumeFlux <= 1.0e-18 || baseNumber <= 1.0e-18)
        return {};

    const double populationScale = requestedVolumeFluxMps / baseVolumeFlux;
    result.numberInterceptPerM3PerMm =
        kMarshallPalmerN0PerM3PerMm * populationScale;
    result.numberConcentrationPerM3 = baseNumber * populationScale;
    result.numberWeightedMeanDiameterMm = baseNumberDiameter / baseNumber;
    result.numberWeightedMeanTerminalVelocityMps =
        baseNumberVelocity / baseNumber;
    result.volumeWeightedMeanDiameterMm = baseVolume > 1.0e-18
        ? baseVolumeDiameter / baseVolume
        : result.numberWeightedMeanDiameterMm;
    result.fluxWeightedMeanTerminalVelocityMps =
        baseFluxVelocity / baseVolumeFlux;
    result.massFluxKgPerM2PerSecond =
        requestedVolumeFluxMps * kWaterDensityKgPerM3;
    result.valid = true;
    return result;
}

RainDropSample sampleRainDrop(
    const RainDropPopulation& population,
    double uniform01)
{
    RainDropSample result;
    if (!population.valid || population.lambdaPerMm <= 0.0)
        return result;

    const double u = clamp01(std::isfinite(uniform01) ? uniform01 : 0.5);
    const double lambda = population.lambdaPerMm;
    const double expMin = std::exp(-lambda * population.minimumDiameterMm);
    const double expMax = std::exp(-lambda * population.maximumDiameterMm);
    const double expSample = expMin - u * (expMin - expMax);
    const double diameterMm = -std::log(std::max(expSample, 1.0e-15)) / lambda;

    result.diameterMm = std::clamp(
        diameterMm,
        population.minimumDiameterMm,
        population.maximumDiameterMm);
    result.radiusM = result.diameterMm * 0.0005;
    result.volumeM3 = dropVolumeM3(result.diameterMm);
    result.massKg = result.volumeM3 * kWaterDensityKgPerM3;
    result.terminalVelocityMps = rainDropTerminalVelocityMps(result.diameterMm);

    // A first-order horizontal-air coupling for representative trajectories:
    // drizzle follows the wind closely, while large drops retain more inertia.
    result.horizontalWindCoupling = std::clamp(
        1.08 - result.diameterMm * 0.105,
        0.38,
        1.0);
    result.valid = true;
    return result;
}

} // namespace heritage::physics::weather

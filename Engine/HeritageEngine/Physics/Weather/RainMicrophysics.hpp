#pragma once

#include <cstdint>

namespace heritage::physics::weather {

// WEATHER07A: statistical ground-level liquid-rain population derived from the
// same physical rainfall rate (mm/h) that feeds SurfaceHydrology. Heritage does
// not pretend that one rendered streak equals one real drop; this population is
// the physical/statistical authority from which bounded representatives are
// sampled for presentation and impact work.
struct RainDropPopulation
{
    bool valid = false;
    double rainfallRateMmPerHour = 0.0;

    // Marshall-Palmer exponential distribution N(D) = N0 exp(-lambda D), with
    // D in millimetres. N0 is rescaled after numerical integration so the
    // resulting liquid-water flux exactly matches rainfallRateMmPerHour while
    // preserving the Marshall-Palmer size-distribution shape.
    double lambdaPerMm = 0.0;
    double numberInterceptPerM3PerMm = 0.0;
    double numberConcentrationPerM3 = 0.0;

    double minimumDiameterMm = 0.20;
    double maximumDiameterMm = 6.00;
    double numberWeightedMeanDiameterMm = 0.0;
    double volumeWeightedMeanDiameterMm = 0.0;
    double numberWeightedMeanTerminalVelocityMps = 0.0;
    double fluxWeightedMeanTerminalVelocityMps = 0.0;

    // Liquid mass crossing one square metre per second. For water this is
    // numerically tied to the requested mm/h rate (1000 kg/m^3 density).
    double massFluxKgPerM2PerSecond = 0.0;
};

struct RainDropSample
{
    bool valid = false;
    double diameterMm = 0.0;
    double radiusM = 0.0;
    double volumeM3 = 0.0;
    double massKg = 0.0;
    double terminalVelocityMps = 0.0;

    // Small drops follow horizontal air motion more strongly than large drops.
    // This scalar is deliberately separated from wind speed/direction so later
    // atmospheric drag can replace this first-order coupling without changing
    // the drop-size authority.
    double horizontalWindCoupling = 0.0;
};

// Ground-level terminal fall speed in still air. For D >= 0.5 mm this uses the
// well-established Atlas/Srivastava/Sekhon exponential approximation. Smaller
// drizzle drops use a continuous bounded continuation to zero rather than
// applying the large-drop fit outside its reliable range.
double rainDropTerminalVelocityMps(double diameterMm);

// Build a deterministic statistical rain population for a requested rainfall
// rate. Zero rain produces an invalid/empty population.
RainDropPopulation buildRainDropPopulation(double rainfallRateMmPerHour);

// Sample the truncated exponential number distribution with one uniform random
// variate in [0,1]. This is deterministic and allocation-free.
RainDropSample sampleRainDrop(
    const RainDropPopulation& population,
    double uniform01);

} // namespace heritage::physics::weather

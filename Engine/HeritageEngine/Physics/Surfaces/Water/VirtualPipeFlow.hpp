#pragma once

#include <algorithm>
#include <cmath>

namespace heritage::physics::water::detail {

// WATER14 authoritative variable-face virtual pipe. One persistent signed link
// connects two adaptive control volumes, which may have different areas/sizes.
// edgeLengthM is the shared hydraulic face width; centerDistanceM is the
// head-gradient distance. Positive flux travels A -> B and negative flux B -> A.
struct AdaptiveVirtualPipeFluxInput
{
    double previousFluxM3ps = 0.0;
    double hydraulicHeadDifferenceM = 0.0;
    double availableFlowDepthM = 0.0;
    double edgeLengthM = 0.5;
    double centerDistanceM = 0.5;
    double deltaTimeSeconds = 1.0 / 30.0;
    double roughness = 0.020;
    double conductance = 0.58;
    double gravityMps2 = 9.80665;
};

inline double integrateAdaptiveVirtualPipeFlux(
    const AdaptiveVirtualPipeFluxInput& input)
{
    const double flowDepthM = std::max(input.availableFlowDepthM, 0.0);
    const double widthM = std::max(input.edgeLengthM, 0.001);
    const double distanceM = std::max(input.centerDistanceM, 0.001);
    const double crossSectionM2 = widthM * flowDepthM;
    const double roughnessScale = std::clamp(input.roughness / 0.020, 0.20, 10.0);
    const double dampingPerSecond = 1.45 * roughnessScale;
    const double damping = 1.0
        / (1.0 + dampingPerSecond * input.deltaTimeSeconds);
    const double dampedFlux = input.previousFluxM3ps * damping;
    const double accelerationM3ps2 = std::max(input.conductance, 0.0)
        * input.gravityMps2
        * crossSectionM2
        * (input.hydraulicHeadDifferenceM / distanceM);
    return dampedFlux + accelerationM3ps2 * input.deltaTimeSeconds;
}

inline double velocityFromNetFlux(
    double netFluxM3ps,
    double sectionWidthM,
    double waterDepthM)
{
    const double sectionM2 = std::max(sectionWidthM, 0.001)
        * std::max(waterDepthM, 0.00025);
    return std::clamp(netFluxM3ps / sectionM2, -35.0, 35.0);
}

} // namespace heritage::physics::water::detail

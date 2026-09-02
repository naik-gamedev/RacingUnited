#include "../Engine/HeritageEngine/Vehicles/Suspension/MultiLinkKinematics.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace heritage::vehicles::suspension;

static MultiLinkDescription fixture()
{
    MultiLinkDescription d;
    d.restWheelCentre = {0.72, 0.42, 0.0};
    d.travelAxis = {0.0, 1.0, 0.0};
    d.constraintCount = 5;
    d.maxIterations = 20;
    d.maximumCorrectionStepMetres = 0.05;
    d.maximumRotationStepRadians = 0.15;
    d.convergenceToleranceMetres = 2.0e-7;

    const MultiLinkVec3 b[5] = {
        {-0.04, +0.16, +0.11},
        {-0.05, +0.13, -0.13},
        {-0.06, -0.15, +0.12},
        {-0.05, -0.12, -0.14},
        {-0.03, +0.01, +0.18}
    };
    const MultiLinkVec3 a[5] = {
        {0.18, 0.62, +0.20},
        {0.16, 0.55, -0.24},
        {0.13, 0.28, +0.24},
        {0.12, 0.31, -0.25},
        {0.22, 0.43, +0.31}
    };
    for (int i = 0; i < 5; ++i)
    {
        d.constraints[static_cast<std::size_t>(i)].chassisPoint = a[i];
        d.constraints[static_cast<std::size_t>(i)].uprightPoint = b[i];
    }
    if (!MultiLinkKinematics::initialize(d)) std::abort();
    return d;
}

int main()
{
    auto d = fixture();
    const MultiLinkState rest = MultiLinkKinematics::solve(d, 0.0);
    if (!rest.converged || rest.maximumLinkErrorMetres > 3e-7) return 2;

    const MultiLinkState bump = MultiLinkKinematics::solve(d, 0.030, &rest);
    if (!bump.converged || bump.maximumLinkErrorMetres > 3e-7) return 3;

    const MultiLinkState droop = MultiLinkKinematics::solve(d, -0.025, &rest);
    if (!droop.converged || droop.maximumLinkErrorMetres > 3e-7) return 4;

    // Steering/toe-link rack movement is represented by moving one inboard joint.
    auto steered = d;
    steered.constraints[4].chassisPointOffset = {0.0, 0.0, 0.006};
    const MultiLinkState rack = MultiLinkKinematics::solve(steered, 0.0, &rest);
    if (!rack.converged || rack.maximumLinkErrorMetres > 3e-7) return 5;

    const MultiLinkVec3 restFwd = rotate(rest.uprightOrientation, {0.0,0.0,1.0});
    const MultiLinkVec3 rackFwd = rotate(rack.uprightOrientation, {0.0,0.0,1.0});
    const double orientationDelta = length(rackFwd - restFwd);
    if (orientationDelta < 1e-5) return 6;

    std::cout << "PASS SUSP14 multi-link closure\n";
    std::cout << "bump_scrub_m=" << bump.wheelCentreScrub.x << "," << bump.wheelCentreScrub.y << "," << bump.wheelCentreScrub.z
              << " max_error_m=" << bump.maximumLinkErrorMetres << " iterations=" << bump.iterations << "\n";
    std::cout << "droop_scrub_m=" << droop.wheelCentreScrub.x << "," << droop.wheelCentreScrub.y << "," << droop.wheelCentreScrub.z
              << " max_error_m=" << droop.maximumLinkErrorMetres << " iterations=" << droop.iterations << "\n";
    std::cout << "rack_orientation_delta=" << orientationDelta << " max_error_m=" << rack.maximumLinkErrorMetres << "\n";
    return 0;
}

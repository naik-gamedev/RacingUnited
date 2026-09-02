#include "LeafSpringAxleDynamics.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {

bool validLeafSpringAxleWrapDescription(
    const LeafSpringAxleWrapDescription& d)
{
    return std::isfinite(d.stiffnessNmPerRad) && d.stiffnessNmPerRad >= 0.0
        && std::isfinite(d.dampingNmsPerRad) && d.dampingNmsPerRad >= 0.0
        && std::isfinite(d.inertiaKgM2) && d.inertiaKgM2 > 0.01
        && std::isfinite(d.maximumAngleRadians)
        && d.maximumAngleRadians >= 0.01 && d.maximumAngleRadians <= 1.0;
}

LeafSpringAxleWrapState advanceLeafSpringAxleWrap(
    const LeafSpringAxleWrapDescription& d,
    const LeafSpringAxleWrapState& initial,
    const LeafSpringAxleWrapInput& input)
{
    if (!validLeafSpringAxleWrapDescription(d)
        || !std::isfinite(initial.angleRadians)
        || !std::isfinite(initial.rateRadiansPerSecond)
        || !std::isfinite(input.reactionTorqueNm)
        || !std::isfinite(input.deltaTimeSeconds)
        || input.deltaTimeSeconds <= 0.0)
    {
        return initial;
    }

    LeafSpringAxleWrapState state = initial;
    const VehicleScalar naturalFrequency = d.stiffnessNmPerRad > 0.0
        ? std::sqrt(d.stiffnessNmPerRad / d.inertiaKgM2)
        : 0.0;
    // Keep a stiff historical leaf pack stable even if a future vehicle uses a
    // lower suspension tick rate. At Heritage's normal 1 kHz this is one step.
    const VehicleScalar maxStep = naturalFrequency > 0.001
        ? std::min(VehicleScalar{0.0025}, VehicleScalar{0.18} / naturalFrequency)
        : VehicleScalar{0.0025};
    const int steps = std::clamp(
        static_cast<int>(std::ceil(input.deltaTimeSeconds / maxStep)), 1, 16);
    const VehicleScalar dt = input.deltaTimeSeconds / static_cast<VehicleScalar>(steps);

    for (int step = 0; step < steps; ++step)
    {
        const VehicleScalar acceleration = (
            input.reactionTorqueNm
            - d.stiffnessNmPerRad * state.angleRadians
            - d.dampingNmsPerRad * state.rateRadiansPerSecond)
            / d.inertiaKgM2;
        // Semi-implicit Euler is dissipative for the damped rotational spring
        // and behaves well at the bounded substep above.
        state.rateRadiansPerSecond += acceleration * dt;
        state.angleRadians += state.rateRadiansPerSecond * dt;
        if (state.angleRadians > d.maximumAngleRadians
            || state.angleRadians < -d.maximumAngleRadians)
        {
            state.angleRadians = std::clamp(
                state.angleRadians,
                -d.maximumAngleRadians,
                d.maximumAngleRadians);
            if ((state.angleRadians >= d.maximumAngleRadians
                    && state.rateRadiansPerSecond > 0.0)
                || (state.angleRadians <= -d.maximumAngleRadians
                    && state.rateRadiansPerSecond < 0.0))
            {
                state.rateRadiansPerSecond *= VehicleScalar{0.15};
            }
        }
    }
    return state;
}

} // namespace heritage::vehicles

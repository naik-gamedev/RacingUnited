#include "UnsprungMassModel.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {

VehicleScalar boundedNormalForce(
    const UnsprungMassDescription& description,
    VehicleScalar deflectionM,
    VehicleScalar deflectionVelocityMps)
{
    if (deflectionM <= 0.0 && deflectionVelocityMps <= 0.0)
        return 0.0;
    const VehicleScalar force = description.tireRadialStiffnessNPerM
            * std::max(deflectionM, 0.0)
        + description.tireRadialDampingNsPerM
            * deflectionVelocityMps;
    return std::clamp(
        force,
        0.0,
        std::max(description.maximumNormalForceN, 0.0));
}

} // namespace

UnsprungMassOutput advanceUnsprungMassModel(
    const UnsprungMassDescription& description,
    const UnsprungMassInput& input,
    UnsprungMassState& state)
{
    UnsprungMassOutput output;
    const VehicleScalar deltaTime = std::max(input.deltaTimeSeconds, 0.0);
    const VehicleScalar minimumLength = std::min(
        input.minimumLengthM,
        input.maximumLengthM);
    const VehicleScalar maximumLength = std::max(
        input.minimumLengthM,
        input.maximumLengthM);
    if (description.effectiveMassKg <= 0.0 || deltaTime <= 0.0)
    {
        output.suspensionLengthM = std::clamp(
            input.roadAvailable ? input.roadHubLengthM : input.restLengthM,
            minimumLength,
            maximumLength);
        return output;
    }

    if (!state.initialized)
    {
        state.suspensionLengthM = std::clamp(
            input.roadAvailable
                ? input.roadHubLengthM
                : input.restLengthM,
            minimumLength,
            maximumLength);
        state.suspensionLengthVelocityMps = input.roadAvailable
            ? input.roadHubLengthVelocityMps
            : 0.0;
        state.initialized = true;
    }

    VehicleScalar tireDeflection = input.roadAvailable
        ? state.suspensionLengthM - input.roadHubLengthM
        : 0.0;
    VehicleScalar tireDeflectionVelocity = input.roadAvailable
        ? state.suspensionLengthVelocityMps
            - input.roadHubLengthVelocityMps
        : 0.0;
    const VehicleScalar normalForce = input.roadAvailable
        ? boundedNormalForce(
            description,
            tireDeflection,
            tireDeflectionVelocity)
        : 0.0;
    const VehicleScalar normalAlignment = std::clamp(
        input.roadNormalAlignment,
        0.05,
        1.0);
    const VehicleScalar acceleration = (
        input.suspensionForceN - normalForce * normalAlignment)
        / description.effectiveMassKg;
    state.suspensionLengthVelocityMps += acceleration * deltaTime;
    state.suspensionLengthM +=
        state.suspensionLengthVelocityMps * deltaTime;

    if (state.suspensionLengthM <= minimumLength)
    {
        state.suspensionLengthM = minimumLength;
        state.suspensionLengthVelocityMps = std::max(
            state.suspensionLengthVelocityMps,
            0.0);
    }
    if (state.suspensionLengthM >= maximumLength)
    {
        state.suspensionLengthM = maximumLength;
        state.suspensionLengthVelocityMps = std::min(
            state.suspensionLengthVelocityMps,
            0.0);
    }

    if (input.roadAvailable)
    {
        const VehicleScalar maximumTireDeflection = std::max(
            description.maximumTireDeflectionM,
            0.0);
        const VehicleScalar maximumHubLength = std::clamp(
            input.roadHubLengthM + maximumTireDeflection,
            minimumLength,
            maximumLength);
        if (state.suspensionLengthM > maximumHubLength)
        {
            state.suspensionLengthM = maximumHubLength;
            state.suspensionLengthVelocityMps = std::min(
                state.suspensionLengthVelocityMps,
                input.roadHubLengthVelocityMps);
        }
    }

    tireDeflection = input.roadAvailable
        ? std::max(state.suspensionLengthM - input.roadHubLengthM, 0.0)
        : 0.0;
    tireDeflectionVelocity = input.roadAvailable
        ? state.suspensionLengthVelocityMps
            - input.roadHubLengthVelocityMps
        : 0.0;
    output.suspensionLengthM = state.suspensionLengthM;
    output.suspensionLengthVelocityMps =
        state.suspensionLengthVelocityMps;
    output.tireDeflectionM = tireDeflection;
    output.tireDeflectionVelocityMps = tireDeflectionVelocity;
    output.normalForceN = input.roadAvailable
        ? boundedNormalForce(
            description,
            tireDeflection,
            tireDeflectionVelocity)
        : 0.0;
    output.grounded = output.normalForceN > 0.0;
    output.tireRadialDissipationW = output.grounded
        ? std::max(
            description.tireRadialDampingNsPerM
                * tireDeflectionVelocity * tireDeflectionVelocity,
            0.0)
        : 0.0;
    return output;
}

} // namespace heritage::vehicles

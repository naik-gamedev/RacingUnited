#include "UnsprungMassModel.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {

float boundedNormalForce(
    const UnsprungMassDescription& description,
    float deflectionM,
    float deflectionVelocityMps)
{
    if (deflectionM <= 0.0f && deflectionVelocityMps <= 0.0f)
        return 0.0f;
    const float force = description.tireRadialStiffnessNPerM
            * std::max(deflectionM, 0.0f)
        + description.tireRadialDampingNsPerM
            * deflectionVelocityMps;
    return std::clamp(
        force,
        0.0f,
        std::max(description.maximumNormalForceN, 0.0f));
}

} // namespace

UnsprungMassOutput advanceUnsprungMassModel(
    const UnsprungMassDescription& description,
    const UnsprungMassInput& input,
    UnsprungMassState& state)
{
    UnsprungMassOutput output;
    const float deltaTime = std::max(input.deltaTimeSeconds, 0.0f);
    const float minimumLength = std::min(
        input.minimumLengthM,
        input.maximumLengthM);
    const float maximumLength = std::max(
        input.minimumLengthM,
        input.maximumLengthM);
    if (description.effectiveMassKg <= 0.0f || deltaTime <= 0.0f)
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
            : 0.0f;
        state.initialized = true;
    }

    float tireDeflection = input.roadAvailable
        ? state.suspensionLengthM - input.roadHubLengthM
        : 0.0f;
    float tireDeflectionVelocity = input.roadAvailable
        ? state.suspensionLengthVelocityMps
            - input.roadHubLengthVelocityMps
        : 0.0f;
    const float normalForce = input.roadAvailable
        ? boundedNormalForce(
            description,
            tireDeflection,
            tireDeflectionVelocity)
        : 0.0f;
    const float normalAlignment = std::clamp(
        input.roadNormalAlignment,
        0.05f,
        1.0f);
    const float acceleration = (
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
            0.0f);
    }
    if (state.suspensionLengthM >= maximumLength)
    {
        state.suspensionLengthM = maximumLength;
        state.suspensionLengthVelocityMps = std::min(
            state.suspensionLengthVelocityMps,
            0.0f);
    }

    if (input.roadAvailable)
    {
        const float maximumTireDeflection = std::max(
            description.maximumTireDeflectionM,
            0.0f);
        const float maximumHubLength = std::clamp(
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
        ? std::max(state.suspensionLengthM - input.roadHubLengthM, 0.0f)
        : 0.0f;
    tireDeflectionVelocity = input.roadAvailable
        ? state.suspensionLengthVelocityMps
            - input.roadHubLengthVelocityMps
        : 0.0f;
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
        : 0.0f;
    output.grounded = output.normalForceN > 0.0f;
    output.tireRadialDissipationW = output.grounded
        ? std::max(
            description.tireRadialDampingNsPerM
                * tireDeflectionVelocity * tireDeflectionVelocity,
            0.0f)
        : 0.0f;
    return output;
}

} // namespace heritage::vehicles

#pragma once

namespace heritage::vehicles {

// A bounded one-degree-of-freedom wheel/upright model. The scalar mass moves
// only along the authored suspension axis, preserving wheel-hop and radial-tire
// compliance without creating a free rigid body for every contact unit.
struct UnsprungMassDescription
{
    // Zero retains the legacy massless raycast-wheel behavior.
    float effectiveMassKg = 0.0f;
    float tireRadialStiffnessNPerM = 220000.0f;
    float tireRadialDampingNsPerM = 1800.0f;
    float maximumTireDeflectionM = 0.08f;
    float maximumNormalForceN = 250000.0f;
};

struct UnsprungMassState
{
    bool initialized = false;
    float suspensionLengthM = 0.0f;
    // Positive velocity extends the wheel away from the chassis mount.
    float suspensionLengthVelocityMps = 0.0f;
};

struct UnsprungMassInput
{
    float deltaTimeSeconds = 0.001f;
    float restLengthM = 0.50f;
    float minimumLengthM = 0.32f;
    float maximumLengthM = 0.65f;
    float suspensionForceN = 0.0f;
    bool roadAvailable = false;
    // Mount-to-hub length at which an undeformed tire just touches the road.
    float roadHubLengthM = 0.65f;
    float roadHubLengthVelocityMps = 0.0f;
    // Projection of the road normal onto the opposite suspension direction.
    float roadNormalAlignment = 1.0f;
};

struct UnsprungMassOutput
{
    float suspensionLengthM = 0.0f;
    float suspensionLengthVelocityMps = 0.0f;
    float tireDeflectionM = 0.0f;
    float tireDeflectionVelocityMps = 0.0f;
    float normalForceN = 0.0f;
    float tireRadialDissipationW = 0.0f;
    bool grounded = false;
};

UnsprungMassOutput advanceUnsprungMassModel(
    const UnsprungMassDescription& description,
    const UnsprungMassInput& input,
    UnsprungMassState& state);

} // namespace heritage::vehicles

#pragma once

#include "VehiclePrecision.hpp"

namespace heritage::vehicles {

// A bounded one-degree-of-freedom wheel/upright model. The scalar mass moves
// only along the authored suspension axis, preserving wheel-hop and radial-tire
// compliance without creating a free rigid body for every contact unit.
struct UnsprungMassDescription
{
    // Zero retains the legacy massless raycast-wheel behavior.
    VehicleScalar effectiveMassKg = 0.0;
    VehicleScalar tireRadialStiffnessNPerM = 220000.0;
    VehicleScalar tireRadialDampingNsPerM = 1800.0;
    VehicleScalar maximumTireDeflectionM = 0.08;
    VehicleScalar maximumNormalForceN = 250000.0;
};

struct UnsprungMassState
{
    bool initialized = false;
    VehicleScalar suspensionLengthM = 0.0;
    // Positive velocity extends the wheel away from the chassis mount.
    VehicleScalar suspensionLengthVelocityMps = 0.0;
};

struct UnsprungMassInput
{
    VehicleScalar deltaTimeSeconds = 0.001;
    VehicleScalar restLengthM = 0.50;
    VehicleScalar minimumLengthM = 0.32;
    VehicleScalar maximumLengthM = 0.65;
    VehicleScalar suspensionForceN = 0.0;
    bool roadAvailable = false;
    // Mount-to-hub length at which an undeformed tire just touches the road.
    VehicleScalar roadHubLengthM = 0.65;
    VehicleScalar roadHubLengthVelocityMps = 0.0;
    // Projection of the road normal onto the opposite suspension direction.
    VehicleScalar roadNormalAlignment = 1.0;
};

struct UnsprungMassOutput
{
    VehicleScalar suspensionLengthM = 0.0;
    VehicleScalar suspensionLengthVelocityMps = 0.0;
    VehicleScalar tireDeflectionM = 0.0;
    VehicleScalar tireDeflectionVelocityMps = 0.0;
    VehicleScalar normalForceN = 0.0;
    VehicleScalar tireRadialDissipationW = 0.0;
    bool grounded = false;
};

UnsprungMassOutput advanceUnsprungMassModel(
    const UnsprungMassDescription& description,
    const UnsprungMassInput& input,
    UnsprungMassState& state);

} // namespace heritage::vehicles

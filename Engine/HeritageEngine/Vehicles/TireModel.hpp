#pragma once

namespace heritage::vehicles {

// Step 29G advanced road-tire data. The force law is an original native
// implementation built from public generalized sine/arctangent tire-curve
// principles. It is deliberately data-driven and independent of Lua so later
// road, motorcycle-profile, low-pressure and deformable-terrain providers can
// share a stable vehicle/contact boundary.
struct TireModelDescription
{
    float nominalLoad = 3500.0f;
    float peakFriction = 1.15f;
    float longitudinalStiffness = 90000.0f;
    float corneringStiffness = 80000.0f;
    float loadSensitivity = 0.12f;
    float longitudinalRelaxationLength = 0.35f;
    float lateralRelaxationLength = 0.45f;
    float wheelInertia = 1.55f;
    float pneumaticTrail = 0.075f;

    // Advanced curve controls. Existing SetTireModel callers remain valid
    // because the Lua binding appends these as optional arguments.
    float stiffnessLoadExponent = 0.85f;
    float longitudinalShapeFactor = 1.65f;
    float lateralShapeFactor = 1.30f;
    float longitudinalCurvatureFactor = 0.20f;
    float lateralCurvatureFactor = 0.15f;
    float combinedSlipExponent = 2.0f;
    float pneumaticTrailFalloff = 0.70f;
};

struct TireContactInput
{
    float normalLoad = 0.0f;
    float longitudinalSlip = 0.0f;
    float slipAngleRadians = 0.0f;
    float frictionMultiplier = 1.0f;
    float stiffnessMultiplier = 1.0f;
};

struct TireForceResult
{
    float longitudinalForce = 0.0f;
    float lateralForce = 0.0f;
    float pureLongitudinalForce = 0.0f;
    float pureLateralForce = 0.0f;
    float effectiveFriction = 0.0f;
    float gripUtilization = 0.0f;
    float combinedSlipScale = 1.0f;
    float pneumaticTrail = 0.0f;
    float aligningTorque = 0.0f;
};

bool validTireModelDescription(const TireModelDescription& value);

// Evaluates the advanced road-tire provider at one already-relaxed contact
// state. Loose/deformable terrain still uses temporary surface multipliers in
// Step 29G; a later terramechanics provider will replace that approximation.
TireForceResult evaluateAdvancedRoadTire(
    const TireModelDescription& description,
    const TireContactInput& input);

} // namespace heritage::vehicles

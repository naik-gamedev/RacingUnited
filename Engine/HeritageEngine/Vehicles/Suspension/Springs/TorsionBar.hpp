#pragma once

namespace heritage::vehicles {

// A torsion bar is a rotational spring. Heritage can author it directly later,
// but the current road-vehicle contract supplies a reference wheel rate. This
// helper converts that familiar wheel-rate target into torsional stiffness at
// the reference arm leverage, then evaluates the true angular spring torque as
// the arm moves through travel.
struct TorsionBarEquivalentDescription
{
    double referenceWheelPreloadN = 0.0;
    double referenceWheelRateNPerM = 35000.0;
    double referenceWheelProgressionNPerM2 = 0.0;
    double maximumWheelForceN = 250000.0;
};

struct TorsionBarEquivalentInput
{
    double twistRadians = 0.0;
    double angularMotionRatioRadPerM = 0.0;
    double referenceAngularMotionRatioRadPerM = 0.0;
};

struct TorsionBarEquivalentOutput
{
    bool valid = false;
    double preloadTorqueNm = 0.0;
    double stiffnessNmPerRad = 0.0;
    double progressionNmPerRad2 = 0.0;
    double springTorqueNm = 0.0;
    double wheelForceN = 0.0;
};

TorsionBarEquivalentOutput evaluateEquivalentTorsionBar(
    const TorsionBarEquivalentDescription& description,
    const TorsionBarEquivalentInput& input);

} // namespace heritage::vehicles

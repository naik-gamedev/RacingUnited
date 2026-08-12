#include "SuspensionModel.hpp"
#include "Suspension/Springs/TorsionBar.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {

VehicleScalar digressiveDamperForce(
    VehicleScalar shaftVelocityMps,
    VehicleScalar lowSpeedDampingNsPerM,
    VehicleScalar highSpeedDampingNsPerM,
    VehicleScalar kneeVelocityMps)
{
    const VehicleScalar speed = std::abs(shaftVelocityMps);
    const VehicleScalar knee = std::max(kneeVelocityMps, 0.0);
    const VehicleScalar forceMagnitude = speed <= knee
        ? lowSpeedDampingNsPerM * speed
        : lowSpeedDampingNsPerM * knee
            + highSpeedDampingNsPerM * (speed - knee);
    return std::copysign(forceMagnitude, shaftVelocityMps);
}

} // namespace

const char* suspensionProviderId(SuspensionProviderKind provider)
{
    switch (provider)
    {
    case SuspensionProviderKind::LinearRaycastV1:
        return "linear_raycast_v1";
    case SuspensionProviderKind::MacPhersonStrutV1:
        return "macpherson_strut_v1";
    case SuspensionProviderKind::TrailingArmTorsionBarV1:
        return "trailing_arm_torsion_bar_v1";
    }
    return "unknown";
}

bool parseSuspensionProvider(
    std::string_view id,
    SuspensionProviderKind& provider)
{
    if (id == "linear_raycast_v1")
    {
        provider = SuspensionProviderKind::LinearRaycastV1;
        return true;
    }
    if (id == "macpherson_strut_v1")
    {
        provider = SuspensionProviderKind::MacPhersonStrutV1;
        return true;
    }
    if (id == "trailing_arm_torsion_bar_v1")
    {
        provider = SuspensionProviderKind::TrailingArmTorsionBarV1;
        return true;
    }
    return false;
}

SuspensionModelOutput evaluateSuspensionModel(
    const SuspensionModelDescription& description,
    const SuspensionModelInput& input)
{
    SuspensionModelOutput output;
    if (description.provider != SuspensionProviderKind::LinearRaycastV1
        && description.provider != SuspensionProviderKind::MacPhersonStrutV1
        && description.provider != SuspensionProviderKind::TrailingArmTorsionBarV1)
        return output;

    const VehicleScalar motionRatio = description.provider
            == SuspensionProviderKind::TrailingArmTorsionBarV1
        ? description.motionRatio
        : std::max(description.motionRatio, 0.0);
    if (description.provider == SuspensionProviderKind::TrailingArmTorsionBarV1)
    {
        const TorsionBarEquivalentOutput torsion = evaluateEquivalentTorsionBar(
            { description.springPreloadN,
              description.springRateNPerM,
              description.springProgressionNPerM2,
              description.maximumForceN },
            { input.springTwistRadians,
              input.springAngularMotionRatioRadPerM,
              input.referenceSpringAngularMotionRatioRadPerM });
        if (!torsion.valid)
            return output;
        output.springForceN = torsion.wheelForceN;
    }
    else
    {
        const VehicleScalar forceRatio = motionRatio * motionRatio;
        const VehicleScalar progressiveForceRatio = forceRatio * motionRatio;
        output.springForceN = description.springPreloadN * motionRatio
            + description.springRateNPerM * input.compressionM * forceRatio
            + 0.5 * description.springProgressionNPerM2
                * input.compressionM * std::abs(input.compressionM)
                * progressiveForceRatio;
    }

    const VehicleScalar shaftVelocity = input.compressionVelocityMps * motionRatio;
    const bool bump = shaftVelocity >= 0.0;
    const VehicleScalar damperForceAtShaft = digressiveDamperForce(
        shaftVelocity,
        bump ? description.bumpDampingNsPerM
            : description.reboundDampingNsPerM,
        bump ? description.bumpHighSpeedDampingNsPerM
            : description.reboundHighSpeedDampingNsPerM,
        bump ? description.bumpDampingKneeVelocityMps
            : description.reboundDampingKneeVelocityMps);
    output.dampingForceN = damperForceAtShaft * motionRatio;
    output.damperDissipationW = std::max(
        damperForceAtShaft * shaftVelocity,
        0.0);

    const VehicleScalar bumpStopTravel = std::max(
        input.compressionM - description.bumpStopEngagementM,
        0.0);
    output.bumpStopForceN = description.bumpStopRateNPerM * bumpStopTravel
        + 0.5 * description.bumpStopProgressionNPerM2
            * bumpStopTravel * bumpStopTravel;
    const VehicleScalar droopStopTravel = std::max(
        -input.compressionM - description.droopStopEngagementM,
        0.0);
    output.droopStopForceN = description.droopStopRateNPerM
        * droopStopTravel;
    output.unclampedForceN = output.springForceN + output.dampingForceN
        + output.bumpStopForceN - output.droopStopForceN;
    output.normalForceN = std::clamp(
        output.unclampedForceN,
        0.0,
        std::max(description.maximumForceN, 0.0));
    return output;
}

} // namespace heritage::vehicles

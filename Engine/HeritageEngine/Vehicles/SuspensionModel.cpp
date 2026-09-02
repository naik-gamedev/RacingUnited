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
    case SuspensionProviderKind::DoubleWishboneV1:
        return "double_wishbone_v1";
    case SuspensionProviderKind::PushrodDoubleWishboneV1:
        return "pushrod_double_wishbone_v1";
    case SuspensionProviderKind::LiveAxleV1:
        return "live_axle_v1";
    case SuspensionProviderKind::LeafSpringLiveAxleV1:
        return "live_axle_leaf_v1";
    case SuspensionProviderKind::MotorcycleTelescopicForkV1:
        return "motorcycle_telescopic_fork_v1";
    case SuspensionProviderKind::MotorcycleSwingarmLinkageV1:
        return "motorcycle_swingarm_linkage_v1";
    case SuspensionProviderKind::KartChassisFlexV1:
        return "kart_chassis_flex_v1";
    case SuspensionProviderKind::MultiLinkV1:
        return "multilink_v1";
    case SuspensionProviderKind::SemiTrailingArmV1:
        return "semi_trailing_arm_v1";
    case SuspensionProviderKind::TwistBeamV1:
        return "twist_beam_v1";
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
    if (id == "double_wishbone_v1")
    {
        provider = SuspensionProviderKind::DoubleWishboneV1;
        return true;
    }
    if (id == "pushrod_double_wishbone_v1")
    {
        provider = SuspensionProviderKind::PushrodDoubleWishboneV1;
        return true;
    }
    if (id == "live_axle_v1")
    {
        provider = SuspensionProviderKind::LiveAxleV1;
        return true;
    }
    if (id == "live_axle_leaf_v1")
    {
        provider = SuspensionProviderKind::LeafSpringLiveAxleV1;
        return true;
    }
    if (id == "motorcycle_telescopic_fork_v1")
    {
        provider = SuspensionProviderKind::MotorcycleTelescopicForkV1;
        return true;
    }
    if (id == "motorcycle_swingarm_linkage_v1")
    {
        provider = SuspensionProviderKind::MotorcycleSwingarmLinkageV1;
        return true;
    }
    if (id == "kart_chassis_flex_v1")
    {
        provider = SuspensionProviderKind::KartChassisFlexV1;
        return true;
    }
    if (id == "multilink_v1")
    {
        provider = SuspensionProviderKind::MultiLinkV1;
        return true;
    }
    if (id == "semi_trailing_arm_v1")
    {
        provider = SuspensionProviderKind::SemiTrailingArmV1;
        return true;
    }
    if (id == "twist_beam_v1")
    {
        provider = SuspensionProviderKind::TwistBeamV1;
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
        && description.provider != SuspensionProviderKind::TrailingArmTorsionBarV1
        && description.provider != SuspensionProviderKind::DoubleWishboneV1
        && description.provider != SuspensionProviderKind::PushrodDoubleWishboneV1
        && description.provider != SuspensionProviderKind::LiveAxleV1
        && description.provider != SuspensionProviderKind::LeafSpringLiveAxleV1
        && description.provider != SuspensionProviderKind::MotorcycleTelescopicForkV1
        && description.provider != SuspensionProviderKind::MotorcycleSwingarmLinkageV1
        && description.provider != SuspensionProviderKind::KartChassisFlexV1
        && description.provider != SuspensionProviderKind::MultiLinkV1
        && description.provider != SuspensionProviderKind::SemiTrailingArmV1
        && description.provider != SuspensionProviderKind::TwistBeamV1)
        return output;

    // SUSP11: karts have no conventional wheel spring/damper. Their wheel hub
    // is rigidly attached to the frame/axle and vertical compliance comes from
    // the pneumatic tire plus chassis_torsional_mode_v1. The high-rate contact
    // phase transmits tire radial force directly into the chassis for this
    // provider, so returning zero here prevents a hidden coilover from being
    // introduced through the generic effective wheel-rate contract.
    if (description.provider == SuspensionProviderKind::KartChassisFlexV1)
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
    else if (description.provider == SuspensionProviderKind::PushrodDoubleWishboneV1
        || description.provider == SuspensionProviderKind::LiveAxleV1
        || description.provider == SuspensionProviderKind::LeafSpringLiveAxleV1
        || description.provider == SuspensionProviderKind::MotorcycleSwingarmLinkageV1
        || description.provider == SuspensionProviderKind::MultiLinkV1
        || description.provider == SuspensionProviderKind::SemiTrailingArmV1
        || description.provider == SuspensionProviderKind::TwistBeamV1)
    {
        const VehicleScalar springRatio = std::clamp(
            input.springMotionRatio, VehicleScalar{0.02}, VehicleScalar{8.0});
        const VehicleScalar shaftCompression = input.springCompressionM;
        const VehicleScalar springForceAtShaft = description.springPreloadN
            + description.springRateNPerM * shaftCompression
            + 0.5 * description.springProgressionNPerM2
                * shaftCompression * std::abs(shaftCompression);
        output.springForceN = springForceAtShaft * springRatio;
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

    // SUSP09: interleaf friction is genuine hysteresis, not viscous damper
    // tuning. It opposes the leaf generalized velocity with a smooth Coulomb
    // transition plus a small viscous term. Housing wind-up is integrated once
    // per paired axle before the wheel loop; only a deliberately bounded
    // jacking component enters vertical support here.
    VehicleScalar leafHysteresisWheelForce = 0.0;
    VehicleScalar leafWrapJackingForce = 0.0;
    VehicleScalar motorcycleChainJackingForce = 0.0;
    VehicleScalar twistBeamCouplingForce = 0.0;
    if (description.provider == SuspensionProviderKind::LeafSpringLiveAxleV1)
    {
        const VehicleScalar leafRatio = std::clamp(
            input.springMotionRatio, VehicleScalar{0.02}, VehicleScalar{8.0});
        const VehicleScalar leafVelocity = input.compressionVelocityMps * leafRatio;
        const VehicleScalar velocityScale = std::max(
            description.leafInterleafVelocityScaleMps, VehicleScalar{0.0001});
        const VehicleScalar frictionAtLeaf = description.leafInterleafFrictionN
            * std::tanh(leafVelocity / velocityScale)
            + description.leafInterleafViscousNsPerM * leafVelocity;
        leafHysteresisWheelForce = frictionAtLeaf * leafRatio;
        output.leafInterleafForceN = leafHysteresisWheelForce;
        output.leafInterleafDissipationW = std::max(
            frictionAtLeaf * leafVelocity, VehicleScalar{0.0});
        leafWrapJackingForce = description.leafAxleWrapJackingNPerRad
            * std::abs(input.axleWrapAngleRadians);
        output.leafAxleWrapJackingForceN = leafWrapJackingForce;
    }


    if (description.provider == SuspensionProviderKind::TwistBeamV1)
    {
        const VehicleScalar torque = description.twistBeamTorsionalStiffnessNmPerRad
                * input.twistBeamTwistRadians
            + description.twistBeamTorsionalDampingNmsPerRad
                * input.twistBeamTwistRateRadiansPerSecond;
        twistBeamCouplingForce = std::clamp(
            torque * input.twistBeamAngularMotionRatioRadPerM,
            VehicleScalar{-50000.0}, VehicleScalar{50000.0});
        output.twistBeamCouplingForceN = twistBeamCouplingForce;
        output.twistBeamDissipationW = std::max(
            description.twistBeamTorsionalDampingNmsPerRad
                * input.twistBeamTwistRateRadiansPerSecond
                * input.twistBeamTwistRateRadiansPerSecond,
            VehicleScalar{0.0});
    }

    if (description.provider == SuspensionProviderKind::MotorcycleSwingarmLinkageV1)
    {
        // SUSP10 anti-squat/chain-jacking virtual work. Use the previous 1 kHz
        // tire force to avoid an algebraic contact/suspension loop. Chain
        // tension is wheel torque divided by rear sprocket pitch radius, while
        // d(chain center distance)/d(wheel compression) supplies the exact
        // kinematic generalized-force leverage. The broad clamp only protects
        // malformed/near-dead-center authoring; normal motorcycle geometry is
        // far inside it.
        const VehicleScalar sprocketRadius = std::clamp(
            description.motorcycleRearSprocketPitchRadiusM,
            VehicleScalar{0.02}, VehicleScalar{0.30});
        const VehicleScalar wheelRadius = std::clamp(
            input.wheelEffectiveRadiusM, VehicleScalar{0.10}, VehicleScalar{0.50});
        const VehicleScalar chainTensionEquivalent =
            input.previousLongitudinalTireForceN * wheelRadius / sprocketRadius;
        motorcycleChainJackingForce = std::clamp(
            -chainTensionEquivalent * input.motorcycleChainDistanceMotionRatio,
            VehicleScalar{-20000.0}, VehicleScalar{20000.0});
        output.motorcycleChainJackingForceN = motorcycleChainJackingForce;
    }

    const VehicleScalar damperRatio = (description.provider
            == SuspensionProviderKind::PushrodDoubleWishboneV1
        || description.provider == SuspensionProviderKind::LiveAxleV1
        || description.provider == SuspensionProviderKind::LeafSpringLiveAxleV1
        || description.provider == SuspensionProviderKind::MotorcycleSwingarmLinkageV1
        || description.provider == SuspensionProviderKind::MultiLinkV1
        || description.provider == SuspensionProviderKind::SemiTrailingArmV1
        || description.provider == SuspensionProviderKind::TwistBeamV1)
        ? std::clamp(
            input.damperMotionRatio, VehicleScalar{0.02}, VehicleScalar{8.0})
        : motionRatio;
    const VehicleScalar shaftVelocity = input.compressionVelocityMps * damperRatio;
    const bool bump = shaftVelocity >= 0.0;
    const VehicleScalar damperForceAtShaft = digressiveDamperForce(
        shaftVelocity,
        bump ? description.bumpDampingNsPerM
            : description.reboundDampingNsPerM,
        bump ? description.bumpHighSpeedDampingNsPerM
            : description.reboundHighSpeedDampingNsPerM,
        bump ? description.bumpDampingKneeVelocityMps
            : description.reboundDampingKneeVelocityMps);
    output.dampingForceN = damperForceAtShaft * damperRatio
        + leafHysteresisWheelForce;
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
        + leafWrapJackingForce + motorcycleChainJackingForce
        + twistBeamCouplingForce
        + output.bumpStopForceN - output.droopStopForceN;
    output.normalForceN = std::clamp(
        output.unclampedForceN,
        0.0,
        std::max(description.maximumForceN, 0.0));
    return output;
}

StaticRideHeightOutput solveStaticRideHeight(
    const StaticRideHeightInput& input)
{
    StaticRideHeightOutput output;
    const bool supportedProvider =
        input.provider == SuspensionProviderKind::LinearRaycastV1
        || input.provider == SuspensionProviderKind::MacPhersonStrutV1
        || input.provider == SuspensionProviderKind::TrailingArmTorsionBarV1;
    if (!supportedProvider)
    {
        output.diagnostic =
            "provider requires geometry-aware spring coordinates";
        return output;
    }
    if (!std::isfinite(input.supportedLoadN)
        || !std::isfinite(input.targetBodyOffsetM)
        || !std::isfinite(input.mountHeightFromAuthoredGroundM)
        || !std::isfinite(input.unloadedTireRadiusM)
        || !std::isfinite(input.suspensionRestLengthM)
        || !std::isfinite(input.maximumCompressionM)
        || !std::isfinite(input.maximumDroopM)
        || !std::isfinite(input.springRateNPerM)
        || !std::isfinite(input.springProgressionNPerM2)
        || !std::isfinite(input.motionRatio)
        || !std::isfinite(input.tireVerticalStiffnessNPerM)
        || input.supportedLoadN <= 0.0
        || input.unloadedTireRadiusM <= 0.0
        || input.suspensionRestLengthM <= 0.0
        || input.maximumCompressionM < 0.0
        || input.maximumDroopM < 0.0
        || input.springRateNPerM <= 0.0
        || input.springProgressionNPerM2 < 0.0
        || input.motionRatio <= 0.0
        || input.tireVerticalStiffnessNPerM <= 0.0)
    {
        output.diagnostic = "non-finite or non-positive calibration input";
        return output;
    }

    output.staticTireDeflectionM =
        input.supportedLoadN / input.tireVerticalStiffnessNPerM;
    const VehicleScalar unloadedRoadHubLength =
        input.mountHeightFromAuthoredGroundM
        + input.targetBodyOffsetM
        - input.unloadedTireRadiusM;
    output.targetSuspensionLengthM = unloadedRoadHubLength
        + output.staticTireDeflectionM;
    output.targetCompressionM = input.suspensionRestLengthM
        - output.targetSuspensionLengthM;
    if (output.targetCompressionM > input.maximumCompressionM
        || output.targetCompressionM < -input.maximumDroopM)
    {
        output.diagnostic = "target datum is outside suspension travel";
        return output;
    }

    const VehicleScalar compression = output.targetCompressionM;
    const VehicleScalar progressiveTerm = 0.5
        * input.springProgressionNPerM2
        * compression * std::abs(compression);
    if (input.provider == SuspensionProviderKind::TrailingArmTorsionBarV1)
    {
        // The current trailing-arm contract authors an equivalent wheel rate
        // at its reference arm leverage. Around that reference pose the wheel
        // preload is therefore solved directly in wheel-force coordinates.
        output.requiredSpringPreloadN = input.supportedLoadN
            - input.springRateNPerM * compression
            - progressiveTerm;
        output.reconstructedSupportForceN = output.requiredSpringPreloadN
            + input.springRateNPerM * compression
            + progressiveTerm;
    }
    else
    {
        const VehicleScalar motionRatio = input.motionRatio;
        const VehicleScalar rateForce = input.springRateNPerM
            * compression * motionRatio * motionRatio;
        const VehicleScalar progressionForce = progressiveTerm
            * motionRatio * motionRatio * motionRatio;
        output.requiredSpringPreloadN = (
            input.supportedLoadN - rateForce - progressionForce)
            / motionRatio;
        output.reconstructedSupportForceN =
            output.requiredSpringPreloadN * motionRatio
            + rateForce + progressionForce;
    }

    if (!std::isfinite(output.requiredSpringPreloadN)
        || output.requiredSpringPreloadN < 0.0)
    {
        output.diagnostic = "target datum requires negative spring preload";
        return output;
    }
    output.valid = true;
    output.diagnostic = "ok";
    return output;
}

} // namespace heritage::vehicles

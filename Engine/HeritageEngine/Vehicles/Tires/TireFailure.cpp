#include "TireFailure.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles::tires {
namespace {

constexpr VehicleScalar kKelvinOffset = 273.15;
constexpr VehicleScalar kEpsilon = 1.0e-12;
constexpr VehicleScalar kPi = 3.14159265358979323846;

bool finiteValue(VehicleScalar value)
{
    return std::isfinite(static_cast<double>(value));
}

VehicleScalar clamp01(VehicleScalar value)
{
    return std::clamp(value, VehicleScalar{0.0}, VehicleScalar{1.0});
}

VehicleScalar smoothStep01(VehicleScalar value)
{
    const VehicleScalar x = clamp01(value);
    return x * x * (VehicleScalar{3.0} - VehicleScalar{2.0} * x);
}

VehicleScalar referenceAbsolutePressurePa(const TireFailureInput& input)
{
    return std::max(
        input.ambientPressurePa + std::max(input.referenceGaugePressurePa, VehicleScalar{0.0}),
        VehicleScalar{1000.0});
}

VehicleScalar equilibriumMassRatio(const TireFailureInput& input)
{
    const VehicleScalar referenceKelvin = std::max(
        input.referenceTemperatureC + kKelvinOffset, VehicleScalar{1.0});
    const VehicleScalar gasKelvin = std::max(
        input.gasTemperatureC + kKelvinOffset, VehicleScalar{1.0});
    return std::clamp(
        input.ambientPressurePa / referenceAbsolutePressurePa(input)
            * (referenceKelvin / gasKelvin),
        VehicleScalar{0.0}, VehicleScalar{1.0});
}

VehicleScalar compressibleOrificeMassFlowKgPerSecond(
    const TireFailureDescription& d,
    VehicleScalar upstreamAbsolutePressurePa,
    VehicleScalar downstreamAbsolutePressurePa,
    VehicleScalar upstreamTemperatureK,
    VehicleScalar areaM2)
{
    if (areaM2 <= 0.0 || upstreamAbsolutePressurePa <= downstreamAbsolutePressurePa)
        return 0.0;

    const VehicleScalar gamma = d.airHeatCapacityRatio;
    const VehicleScalar gasConstant = d.airSpecificGasConstantJPerKgK;
    const VehicleScalar pressureRatio = std::clamp(
        downstreamAbsolutePressurePa / upstreamAbsolutePressurePa,
        VehicleScalar{0.0}, VehicleScalar{1.0});
    const VehicleScalar criticalRatio = std::pow(
        VehicleScalar{2.0} / (gamma + VehicleScalar{1.0}),
        gamma / (gamma - VehicleScalar{1.0}));

    VehicleScalar flowFactor = 0.0;
    if (pressureRatio <= criticalRatio)
    {
        flowFactor = std::sqrt(gamma / (gasConstant * upstreamTemperatureK))
            * std::pow(
                VehicleScalar{2.0} / (gamma + VehicleScalar{1.0}),
                (gamma + VehicleScalar{1.0})
                    / (VehicleScalar{2.0} * (gamma - VehicleScalar{1.0})));
    }
    else
    {
        const VehicleScalar term = std::max(
            VehicleScalar{2.0} * gamma
                / (gasConstant * upstreamTemperatureK * (gamma - VehicleScalar{1.0}))
                * (std::pow(pressureRatio, VehicleScalar{2.0} / gamma)
                    - std::pow(
                        pressureRatio,
                        (gamma + VehicleScalar{1.0}) / gamma)),
            VehicleScalar{0.0});
        flowFactor = std::sqrt(term);
    }
    return std::max(
        d.dischargeCoefficient * areaM2 * upstreamAbsolutePressurePa * flowFactor,
        VehicleScalar{0.0});
}

void initializeState(TireFailureState& state)
{
    state = {};
    state.initialized = true;
}

VehicleScalar physicalSlipPowerWatts(const TireFailureInput& input)
{
    const VehicleScalar supplied = std::max(input.slipDissipationWatts, VehicleScalar{0.0});
    const VehicleScalar reconstructed =
        std::abs(input.longitudinalForceN * input.longitudinalSlipVelocityMps)
        + std::abs(input.lateralForceN * input.lateralSlipVelocityMps);
    return std::max(supplied, reconstructed);
}

VehicleScalar effectiveConstructionIntegrity(const TireFailureState& state)
{
    // The weakest construction family matters most, but use a weighted mean so
    // a small local insult does not instantly make the whole tire numerically
    // collapse. The persistent structuralIntegrity remains a global envelope.
    const VehicleScalar construction =
        state.beltIntegrity * VehicleScalar{0.28}
        + state.cordIntegrity * VehicleScalar{0.30}
        + state.sidewallIntegrity * VehicleScalar{0.26}
        + state.beadRetention * VehicleScalar{0.16};
    return std::min(state.structuralIntegrity, construction);
}

TireFailureOutput outputFromState(
    const TireFailureDescription& d,
    const TireFailureInput& input,
    const TireFailureState& source)
{
    TireFailureState state = source;
    if (!state.initialized)
        initializeState(state);

    TireFailureOutput out;
    out.valid = d.enabled;
    out.stage = state.stage;
    out.containedGasMassRatio = state.containedGasMassRatio;
    out.pressurizedGasFraction = state.pressurizedGasFraction;
    out.effectiveLeakAreaM2 = state.effectiveLeakAreaM2;
    out.leakMassFlowKgPerSecond = state.leakMassFlowKgPerSecond;
    out.structuralIntegrity = effectiveConstructionIntegrity(state);
    out.beltIntegrity = state.beltIntegrity;
    out.cordIntegrity = state.cordIntegrity;
    out.sidewallIntegrity = state.sidewallIntegrity;
    out.beadRetention = state.beadRetention;
    out.treadAttachment = state.treadAttachment;
    out.rimIntegrity = state.rimIntegrity;
    out.runFlatSupportHealth = state.runFlatSupportHealth;
    out.treadGraining = state.treadGraining;
    out.treadBlistering = state.treadBlistering;
    out.delaminationFraction = state.delaminationFraction;
    out.rimContactFraction = state.rimContactFraction;
    out.eventElapsedSeconds = state.eventElapsedSeconds;

    const VehicleScalar structuralLoss = VehicleScalar{1.0} - out.structuralIntegrity;
    const VehicleScalar treadLoss = VehicleScalar{1.0} - state.treadAttachment;
    const VehicleScalar rimLoss = VehicleScalar{1.0} - state.rimIntegrity;
    const VehicleScalar surfaceDistress = std::clamp(
        VehicleScalar{0.10} * state.treadGraining
        + VehicleScalar{0.16} * state.treadBlistering
        + VehicleScalar{0.22} * state.delaminationFraction,
        VehicleScalar{0.0}, VehicleScalar{0.42});

    out.forceCapacityScale = std::clamp(
        VehicleScalar{1.0}
            - VehicleScalar{0.52} * state.rimContactFraction
            - VehicleScalar{0.24} * structuralLoss
            - VehicleScalar{0.18} * treadLoss
            - VehicleScalar{0.16} * rimLoss
            - surfaceDistress,
        VehicleScalar{0.12}, VehicleScalar{1.0});
    out.carcassSupportScale = std::clamp(
        out.structuralIntegrity
            * (VehicleScalar{1.0} - VehicleScalar{0.55} * state.rimContactFraction)
            * (VehicleScalar{0.82} + VehicleScalar{0.18} * state.rimIntegrity),
        VehicleScalar{0.10}, VehicleScalar{1.0});
    out.rollingResistanceScale = std::clamp(
        VehicleScalar{1.0}
            + VehicleScalar{5.0} * state.rimContactFraction
            + VehicleScalar{2.2} * structuralLoss
            + VehicleScalar{1.5} * treadLoss
            + VehicleScalar{2.0} * rimLoss
            + VehicleScalar{0.8} * state.treadGraining
            + VehicleScalar{1.2} * state.treadBlistering,
        VehicleScalar{1.0}, VehicleScalar{12.0});
    (void)input;
    return out;
}

} // namespace

const char* tireFailureStageName(TireFailureStage stage)
{
    switch (stage)
    {
    case TireFailureStage::Healthy: return "Healthy";
    case TireFailureStage::SlowPuncture: return "Slow puncture";
    case TireFailureStage::RapidPressureLoss: return "Rapid pressure loss";
    case TireFailureStage::Blowout: return "Blowout";
    case TireFailureStage::PartiallyDetachedTread: return "Partially detached tread";
    case TireFailureStage::CollapsedCarcass: return "Collapsed carcass";
    case TireFailureStage::BareRimRunning: return "Bare-rim running";
    }
    return "Unknown";
}

const char* tireDamageIncidentName(TireDamageIncident incident)
{
    switch (incident)
    {
    case TireDamageIncident::TreadPuncture: return "Tread puncture";
    case TireDamageIncident::TreadCut: return "Tread cut";
    case TireDamageIncident::SidewallCut: return "Sidewall cut";
    case TireDamageIncident::ValveLeak: return "Valve leak";
    case TireDamageIncident::BeadLeak: return "Bead leak";
    case TireDamageIncident::BeadUnseat: return "Bead unseat";
    case TireDamageIncident::BeltSeparation: return "Belt separation";
    case TireDamageIncident::Impact: return "Tire impact";
    case TireDamageIncident::RimImpact: return "Rim impact";
    case TireDamageIncident::RepairPuncture: return "Puncture repair";
    }
    return "Unknown";
}

VehicleScalar estimatedTireContainedAirVolumeM3(
    VehicleScalar unloadedRadiusM,
    VehicleScalar sectionWidthM,
    VehicleScalar rimRadiusM)
{
    if (!finiteValue(unloadedRadiusM)
        || !finiteValue(sectionWidthM)
        || !finiteValue(rimRadiusM)
        || unloadedRadiusM <= rimRadiusM
        || sectionWidthM <= 0.0
        || rimRadiusM <= 0.0)
    {
        return VehicleScalar{0.025};
    }

    const VehicleScalar sidewallHeightM = unloadedRadiusM - rimRadiusM;
    const VehicleScalar cavityMajorRadiusM =
        (rimRadiusM + unloadedRadiusM) * VehicleScalar{0.5};
    const VehicleScalar cavityRadialRadiusM =
        std::max(sidewallHeightM * VehicleScalar{0.55}, VehicleScalar{0.008});
    const VehicleScalar cavityLateralRadiusM =
        std::max(sectionWidthM * VehicleScalar{0.32}, VehicleScalar{0.012});
    return std::clamp(
        VehicleScalar{2.0} * kPi * cavityMajorRadiusM
            * kPi * cavityRadialRadiusM * cavityLateralRadiusM,
        VehicleScalar{0.001}, VehicleScalar{0.35});
}

bool validTireFailureDescription(const TireFailureDescription& d)
{
    if (!d.enabled)
        return true;
    const VehicleScalar values[] = {
        d.containedAirVolumeM3, d.dischargeCoefficient,
        d.airSpecificGasConstantJPerKgK, d.airHeatCapacityRatio,
        d.slowPunctureAreaM2, d.rapidPressureLossAreaM2, d.blowoutAreaM2,
        d.treadCutAreaM2, d.sidewallCutAreaM2, d.valveLeakAreaM2, d.beadLeakAreaM2,
        d.minimumEmbeddedSealOpeningFraction, d.flexOpeningGain,
        d.rapidLossAreaThresholdM2, d.underinflationDamageStartRatio,
        d.collapsedPressureRatio, d.collapseLoadedDelaySeconds,
        d.maximumSafeCarcassTemperatureC,
        d.severeUnderinflationIntegrityLossPerSecond,
        d.collapsedRunningIntegrityLossPerSecond,
        d.collapsedRunningTreadLossPerSecond,
        d.blowoutRunningTreadLossPerSecond,
        d.bareRimIntegrityThreshold, d.bareRimTreadAttachmentThreshold,
        d.beltFatigueReferenceEnergyJ, d.cordFatigueReferenceEnergyJ,
        d.sidewallFatigueReferenceEnergyJ, d.fatigueHeatAccelerationPerC,
        d.fatigueOverloadExponent, d.grainingColdThresholdBelowOptimumC,
        d.grainingBuildPerKJ, d.grainingRecoveryPerSecond,
        d.blisterTemperatureC, d.blisterBuildPerKJ,
        d.blisterRecoveryPerSecond, d.delaminationTemperatureC,
        d.delaminationBuildPerSecond, d.beadUnseatPressureRatio,
        d.beadUnseatLateralForceRatio, d.beadDamageRatePerSecond,
        d.rimDamagePowerThresholdW, d.rimDamageRatePerSecond,
        d.runFlatSupportLoadFraction, d.runFlatMaximumSpeedMps,
        d.runFlatMaximumTemperatureC, d.runFlatHealthLossPerSecond
    };
    for (VehicleScalar value : values)
    {
        if (!finiteValue(value))
            return false;
    }
    return d.containedAirVolumeM3 > 1.0e-4
        && d.dischargeCoefficient > 0.0 && d.dischargeCoefficient <= 1.5
        && d.airSpecificGasConstantJPerKgK > 10.0
        && d.airHeatCapacityRatio > 1.0
        && d.slowPunctureAreaM2 >= 0.0
        && d.rapidPressureLossAreaM2 >= d.slowPunctureAreaM2
        && d.blowoutAreaM2 >= d.rapidPressureLossAreaM2
        && d.treadCutAreaM2 >= d.slowPunctureAreaM2
        && d.sidewallCutAreaM2 >= d.slowPunctureAreaM2
        && d.valveLeakAreaM2 >= 0.0
        && d.beadLeakAreaM2 >= 0.0
        && d.minimumEmbeddedSealOpeningFraction >= 0.0
        && d.minimumEmbeddedSealOpeningFraction <= 1.0
        && d.rapidLossAreaThresholdM2 >= 0.0
        && d.underinflationDamageStartRatio > d.collapsedPressureRatio
        && d.collapsedPressureRatio >= 0.0
        && d.collapseLoadedDelaySeconds >= 0.0
        && d.beltFatigueReferenceEnergyJ > 1.0
        && d.cordFatigueReferenceEnergyJ > 1.0
        && d.sidewallFatigueReferenceEnergyJ > 1.0
        && d.fatigueOverloadExponent >= 0.5
        && d.grainingColdThresholdBelowOptimumC >= 0.0
        && d.grainingBuildPerKJ >= 0.0
        && d.grainingRecoveryPerSecond >= 0.0
        && d.blisterBuildPerKJ >= 0.0
        && d.blisterRecoveryPerSecond >= 0.0
        && d.delaminationBuildPerSecond >= 0.0
        && d.beadUnseatPressureRatio >= 0.0
        && d.beadUnseatPressureRatio <= 1.0
        && d.beadUnseatLateralForceRatio >= 0.0
        && d.beadDamageRatePerSecond >= 0.0
        && d.rimDamagePowerThresholdW >= 0.0
        && d.rimDamageRatePerSecond >= 0.0
        && d.runFlatSupportLoadFraction >= 0.0
        && d.runFlatSupportLoadFraction <= 1.0
        && d.runFlatMaximumSpeedMps >= 0.0
        && d.runFlatHealthLossPerSecond >= 0.0;
}

TireFailureOutput evaluateTireFailureState(
    const TireFailureDescription& description,
    const TireFailureInput& input,
    const TireFailureState& state)
{
    if (!description.enabled || !validTireFailureDescription(description))
        return {};
    return outputFromState(description, input, state);
}

TireFailureOutput advanceTireFailure(
    const TireFailureDescription& d,
    const TireFailureInput& input,
    VehicleScalar dt,
    TireFailureState& state)
{
    if (!d.enabled || !validTireFailureDescription(d)
        || !finiteValue(dt) || dt <= 0.0)
    {
        return {};
    }
    if (!state.initialized)
        initializeState(state);
    dt = std::min(dt, VehicleScalar{0.05});
    state.eventElapsedSeconds = std::min(
        state.eventElapsedSeconds + dt, VehicleScalar{3600.0});

    const VehicleScalar loadRatio = std::max(input.normalLoadN, VehicleScalar{0.0})
        / std::max(input.nominalLoadN, VehicleScalar{1.0});
    const VehicleScalar pressureRatio = std::clamp(
        input.inflationGaugePressurePa
            / std::max(input.identifiedReferencePressurePa, VehicleScalar{1.0}),
        VehicleScalar{0.0}, VehicleScalar{2.5});
    const VehicleScalar underinflation = smoothStep01(
        (d.underinflationDamageStartRatio - pressureRatio)
            / std::max(d.underinflationDamageStartRatio, VehicleScalar{0.01}));
    const VehicleScalar slipFlex = std::clamp(
        (std::abs(input.longitudinalSlipVelocityMps)
            + std::abs(input.lateralSlipVelocityMps)) / VehicleScalar{8.0},
        VehicleScalar{0.0}, VehicleScalar{1.5});
    const VehicleScalar radialFlex = std::clamp(
        std::max(input.radialDissipationWatts, VehicleScalar{0.0})
            / VehicleScalar{12000.0},
        VehicleScalar{0.0}, VehicleScalar{1.5});
    const VehicleScalar flexDemand = std::clamp(
        VehicleScalar{0.18} * std::max(loadRatio - VehicleScalar{0.4}, VehicleScalar{0.0})
            + VehicleScalar{0.42} * underinflation
            + VehicleScalar{0.25} * slipFlex
            + VehicleScalar{0.15} * radialFlex,
        VehicleScalar{0.0}, VehicleScalar{1.0});
    const VehicleScalar sealOpening = std::clamp(
        d.minimumEmbeddedSealOpeningFraction + d.flexOpeningGain * flexDemand,
        d.minimumEmbeddedSealOpeningFraction, VehicleScalar{1.0});
    const VehicleScalar sealedFraction = std::clamp(
        state.embeddedObjectSealFraction, VehicleScalar{0.0}, VehicleScalar{0.98});
    const VehicleScalar effectiveTreadLeak = std::max(
        state.punctureAreaM2
            * (VehicleScalar{1.0} - sealedFraction
                * (VehicleScalar{1.0} - sealOpening)),
        VehicleScalar{0.0});
    state.effectiveLeakAreaM2 = effectiveTreadLeak
        + std::max(state.valveLeakAreaM2, VehicleScalar{0.0})
        + std::max(state.beadLeakAreaM2, VehicleScalar{0.0})
        + std::max(state.sidewallLeakAreaM2, VehicleScalar{0.0});

    const VehicleScalar gasKelvin = std::max(
        input.gasTemperatureC + kKelvinOffset, VehicleScalar{1.0});
    const VehicleScalar upstreamAbsolutePa = std::max(
        input.ambientPressurePa + std::max(input.inflationGaugePressurePa, VehicleScalar{0.0}),
        input.ambientPressurePa);
    state.leakMassFlowKgPerSecond = compressibleOrificeMassFlowKgPerSecond(
        d, upstreamAbsolutePa, input.ambientPressurePa,
        gasKelvin, state.effectiveLeakAreaM2);
    const VehicleScalar referenceKelvin = std::max(
        input.referenceTemperatureC + kKelvinOffset, VehicleScalar{1.0});
    const VehicleScalar initialGasMassKg = referenceAbsolutePressurePa(input)
        * d.containedAirVolumeM3
        / std::max(d.airSpecificGasConstantJPerKgK * referenceKelvin, kEpsilon);
    const VehicleScalar ambientEquilibriumRatio = equilibriumMassRatio(input);
    state.containedGasMassRatio = std::clamp(
        state.containedGasMassRatio
            - state.leakMassFlowKgPerSecond * dt
                / std::max(initialGasMassKg, kEpsilon),
        ambientEquilibriumRatio, VehicleScalar{1.0});
    state.pressurizedGasFraction = std::clamp(
        (state.containedGasMassRatio - ambientEquilibriumRatio)
            / std::max(VehicleScalar{1.0} - ambientEquilibriumRatio, kEpsilon),
        VehicleScalar{0.0}, VehicleScalar{1.0});

    const VehicleScalar speedUse = smoothStep01(
        (std::abs(input.forwardSpeedMps) - VehicleScalar{0.5}) / VehicleScalar{12.0});
    const VehicleScalar heatDamage = smoothStep01(
        (input.carcassTemperatureC - d.maximumSafeCarcassTemperatureC)
            / VehicleScalar{45.0});
    state.structuralIntegrity = std::clamp(
        state.structuralIntegrity
            - d.severeUnderinflationIntegrityLossPerSecond
                * (underinflation * (VehicleScalar{0.25} + loadRatio + speedUse)
                    + VehicleScalar{2.0} * heatDamage) * dt,
        VehicleScalar{0.0}, VehicleScalar{1.0});

    // TIRE46 endurance: load, heat and dissipated deformation energy consume
    // independent belt/cord/sidewall integrity. Estimated reference energies
    // are authoring data and remain provenance-labelled at the tire definition.
    const VehicleScalar slipPower = physicalSlipPowerWatts(input);
    const VehicleScalar deformationPower = std::max(
        input.radialDissipationWatts, VehicleScalar{0.0});
    const VehicleScalar fatigueEnergyJ = (deformationPower
        + VehicleScalar{0.35} * slipPower) * dt;
    const VehicleScalar overload = std::pow(
        std::max(loadRatio, VehicleScalar{0.15}), d.fatigueOverloadExponent);
    const VehicleScalar heatAcceleration = std::clamp(
        std::exp(std::max(input.carcassTemperatureC
                - d.maximumSafeCarcassTemperatureC, VehicleScalar{0.0})
            * d.fatigueHeatAccelerationPerC),
        VehicleScalar{1.0}, VehicleScalar{8.0});
    const VehicleScalar fatigueSeverity = overload * heatAcceleration;
    state.beltIntegrity = std::clamp(
        state.beltIntegrity
            - fatigueEnergyJ / d.beltFatigueReferenceEnergyJ
                * fatigueSeverity,
        VehicleScalar{0.0}, VehicleScalar{1.0});
    state.cordIntegrity = std::clamp(
        state.cordIntegrity
            - fatigueEnergyJ / d.cordFatigueReferenceEnergyJ
                * fatigueSeverity * (VehicleScalar{0.75} + VehicleScalar{0.25} * underinflation),
        VehicleScalar{0.0}, VehicleScalar{1.0});
    const VehicleScalar sidewallCamberDemand = VehicleScalar{1.0}
        + std::min(std::abs(input.camberAngleRadians), VehicleScalar{1.2})
            * VehicleScalar{0.65};
    state.sidewallIntegrity = std::clamp(
        state.sidewallIntegrity
            - fatigueEnergyJ / d.sidewallFatigueReferenceEnergyJ
                * fatigueSeverity * sidewallCamberDemand
                * (VehicleScalar{0.65} + VehicleScalar{0.70} * underinflation),
        VehicleScalar{0.0}, VehicleScalar{1.0});

    // Cold tearing/graining and hot blistering consume real slip energy. These
    // surface distress coordinates feed force capacity but do not pretend to be
    // extra MF coefficient sets.
    const VehicleScalar slipEnergyKJ = slipPower * dt / VehicleScalar{1000.0};
    const VehicleScalar coldDeficit = std::max(
        (input.optimumTreadTemperatureC - d.grainingColdThresholdBelowOptimumC)
            - input.treadTemperatureC,
        VehicleScalar{0.0});
    const VehicleScalar coldSeverity = smoothStep01(coldDeficit / VehicleScalar{35.0});
    state.treadGraining = clamp01(
        state.treadGraining
            + d.grainingBuildPerKJ * slipEnergyKJ
                * coldSeverity * std::max(loadRatio, VehicleScalar{0.2})
            - d.grainingRecoveryPerSecond
                * (VehicleScalar{1.0} - coldSeverity) * dt);

    const VehicleScalar hotSeverity = smoothStep01(
        (input.treadTemperatureC - d.blisterTemperatureC) / VehicleScalar{35.0});
    state.treadBlistering = clamp01(
        state.treadBlistering
            + d.blisterBuildPerKJ * slipEnergyKJ
                * hotSeverity * std::max(loadRatio, VehicleScalar{0.2})
            - d.blisterRecoveryPerSecond
                * (VehicleScalar{1.0} - hotSeverity) * dt);

    const VehicleScalar delamHeat = smoothStep01(
        (std::max(input.treadTemperatureC, input.carcassTemperatureC)
            - d.delaminationTemperatureC) / VehicleScalar{35.0});
    const VehicleScalar delamFatigue = smoothStep01(
        (VehicleScalar{0.72} - state.beltIntegrity) / VehicleScalar{0.45});
    const VehicleScalar delamDemand = std::max({
        delamHeat,
        state.treadBlistering * VehicleScalar{0.85},
        delamFatigue });
    const VehicleScalar previousDelamination = state.delaminationFraction;
    state.delaminationFraction = clamp01(
        state.delaminationFraction + d.delaminationBuildPerSecond
            * delamDemand * (VehicleScalar{0.35} + speedUse + loadRatio * VehicleScalar{0.35}) * dt);
    const VehicleScalar delaminationGrowth = std::max(
        state.delaminationFraction - previousDelamination, VehicleScalar{0.0});
    state.treadAttachment = std::clamp(
        state.treadAttachment - delaminationGrowth * VehicleScalar{1.35},
        VehicleScalar{0.0}, VehicleScalar{1.0});

    // Bead retention is challenged only when pressure is low and lateral shear
    // approaches/exceeds normal load. This gives motorcycle lean and curb hits
    // the correct failure pathway without a special game-only trigger.
    const VehicleScalar lateralForceRatio = std::abs(input.lateralForceN)
        / std::max(input.normalLoadN, VehicleScalar{100.0});
    const VehicleScalar beadLowPressure = smoothStep01(
        (d.beadUnseatPressureRatio - pressureRatio)
            / std::max(d.beadUnseatPressureRatio, VehicleScalar{0.01}));
    const VehicleScalar beadLateralDemand = smoothStep01(
        (lateralForceRatio - d.beadUnseatLateralForceRatio)
            / std::max(VehicleScalar{1.8} - d.beadUnseatLateralForceRatio,
                VehicleScalar{0.1}));
    const VehicleScalar beadDemand = input.grounded
        ? beadLowPressure * beadLateralDemand
            * (VehicleScalar{0.65} + VehicleScalar{0.35} * speedUse)
        : VehicleScalar{0.0};
    state.beadRetention = std::clamp(
        state.beadRetention - d.beadDamageRatePerSecond * beadDemand * dt,
        VehicleScalar{0.0}, VehicleScalar{1.0});
    if (state.beadRetention < VehicleScalar{0.78})
    {
        state.beadLeakAreaM2 = std::max(
            state.beadLeakAreaM2,
            d.beadLeakAreaM2 * smoothStep01(
                (VehicleScalar{0.78} - state.beadRetention) / VehicleScalar{0.78}));
    }

    if (input.grounded && loadRatio > 0.05 && pressureRatio <= d.collapsedPressureRatio)
        state.lowPressureLoadedSeconds += dt;
    else
        state.lowPressureLoadedSeconds = std::max(
            state.lowPressureLoadedSeconds - VehicleScalar{0.5} * dt,
            VehicleScalar{0.0});
    if (state.blowoutLatched)
        state.blowoutElapsedSeconds += dt;

    // Reinforced/run-flat construction can carry some low-pressure load. It is
    // thermally and speed limited, and once consumed it cannot magically heal.
    if (d.runFlatSupportEnabled && pressureRatio < d.underinflationDamageStartRatio
        && input.grounded)
    {
        const VehicleScalar speedExcess = smoothStep01(
            (std::abs(input.forwardSpeedMps) - d.runFlatMaximumSpeedMps)
                / std::max(d.runFlatMaximumSpeedMps, VehicleScalar{1.0}));
        const VehicleScalar heatExcess = smoothStep01(
            (std::max(input.carcassTemperatureC, input.rimTemperatureC)
                - d.runFlatMaximumTemperatureC) / VehicleScalar{40.0});
        const VehicleScalar runFlatDemand = underinflation
            * std::clamp(loadRatio, VehicleScalar{0.2}, VehicleScalar{3.0})
            * (VehicleScalar{0.20} + speedUse + speedExcess + heatExcess);
        state.runFlatSupportHealth = std::clamp(
            state.runFlatSupportHealth
                - d.runFlatHealthLossPerSecond * runFlatDemand * dt,
            VehicleScalar{0.0}, VehicleScalar{1.0});
    }

    const bool collapseActive = state.lowPressureLoadedSeconds
        >= d.collapseLoadedDelaySeconds;
    if (input.grounded && speedUse > 0.0 && (collapseActive || state.blowoutLatched))
    {
        const VehicleScalar runningSeverity = speedUse
            * std::clamp(loadRatio, VehicleScalar{0.15}, VehicleScalar{2.5});
        state.structuralIntegrity = std::max(
            state.structuralIntegrity
                - d.collapsedRunningIntegrityLossPerSecond * runningSeverity * dt,
            VehicleScalar{0.0});
        state.sidewallIntegrity = std::max(
            state.sidewallIntegrity
                - d.collapsedRunningIntegrityLossPerSecond
                    * runningSeverity * VehicleScalar{0.75} * dt,
            VehicleScalar{0.0});
        state.treadAttachment = std::max(
            state.treadAttachment
                - (d.collapsedRunningTreadLossPerSecond
                    + (state.blowoutLatched
                        ? d.blowoutRunningTreadLossPerSecond : VehicleScalar{0.0}))
                    * runningSeverity * dt,
            VehicleScalar{0.0});
    }

    VehicleScalar rimContact = smoothStep01(
        (d.collapsedPressureRatio - pressureRatio)
            / std::max(d.collapsedPressureRatio, VehicleScalar{0.01}))
        * smoothStep01((loadRatio - VehicleScalar{0.02}) / VehicleScalar{0.65});
    rimContact = std::clamp(
        rimContact + (VehicleScalar{1.0} - effectiveConstructionIntegrity(state))
            * VehicleScalar{0.45},
        VehicleScalar{0.0}, VehicleScalar{1.0});
    if (d.runFlatSupportEnabled && state.runFlatSupportHealth > 0.0)
    {
        rimContact *= VehicleScalar{1.0}
            - d.runFlatSupportLoadFraction * state.runFlatSupportHealth;
    }
    state.rimContactFraction = clamp01(rimContact);

    const VehicleScalar rimPowerSeverity = smoothStep01(
        (std::max(input.radialDissipationWatts, VehicleScalar{0.0})
            - d.rimDamagePowerThresholdW)
        / std::max(d.rimDamagePowerThresholdW, VehicleScalar{100.0}));
    const VehicleScalar rimDamageDemand = state.rimContactFraction
        * std::clamp(loadRatio, VehicleScalar{0.2}, VehicleScalar{3.0})
        * (VehicleScalar{0.25} + speedUse + rimPowerSeverity);
    state.rimIntegrity = std::clamp(
        state.rimIntegrity - d.rimDamageRatePerSecond * rimDamageDemand * dt,
        VehicleScalar{0.0}, VehicleScalar{1.0});

    // Fold construction coordinates into the aggregate integrity envelope.
    state.structuralIntegrity = std::min(
        state.structuralIntegrity,
        effectiveConstructionIntegrity(state));

    TireFailureStage candidate = TireFailureStage::Healthy;
    if (state.punctureAreaM2 > 0.0 || state.valveLeakAreaM2 > 0.0
        || state.beadLeakAreaM2 > 0.0 || state.sidewallLeakAreaM2 > 0.0)
    {
        candidate = TireFailureStage::SlowPuncture;
    }
    if (state.effectiveLeakAreaM2 >= d.rapidLossAreaThresholdM2
        || (state.effectiveLeakAreaM2 > 0.0 && pressureRatio < 0.35))
    {
        candidate = TireFailureStage::RapidPressureLoss;
    }
    if (state.blowoutLatched && state.blowoutElapsedSeconds < d.collapseLoadedDelaySeconds)
        candidate = TireFailureStage::Blowout;
    if (state.treadAttachment < 0.72 || state.delaminationFraction > 0.42)
        candidate = std::max(candidate, TireFailureStage::PartiallyDetachedTread);
    if (collapseActive || state.sidewallIntegrity < 0.28
        || state.cordIntegrity < 0.22 || state.beadRetention < 0.12)
    {
        candidate = std::max(candidate, TireFailureStage::CollapsedCarcass);
    }
    if (state.structuralIntegrity <= d.bareRimIntegrityThreshold
        || state.treadAttachment <= d.bareRimTreadAttachmentThreshold)
    {
        candidate = TireFailureStage::BareRimRunning;
    }
    state.stage = std::max(state.stage, candidate);
    return outputFromState(d, input, state);
}

void triggerTireFailure(
    const TireFailureDescription& d,
    const TireFailureInput& input,
    TireFailureStage requested,
    TireFailureState& state)
{
    if (!state.initialized)
        initializeState(state);
    ++state.eventSerial;
    state.eventElapsedSeconds = 0.0;
    switch (requested)
    {
    case TireFailureStage::Healthy:
        initializeState(state);
        ++state.eventSerial;
        break;
    case TireFailureStage::SlowPuncture:
        state.punctureAreaM2 = std::max(state.punctureAreaM2, d.slowPunctureAreaM2);
        state.embeddedObjectSealFraction = std::max(
            state.embeddedObjectSealFraction, VehicleScalar{0.82});
        state.stage = std::max(state.stage, requested);
        break;
    case TireFailureStage::RapidPressureLoss:
        state.punctureAreaM2 = std::max(
            state.punctureAreaM2, d.rapidPressureLossAreaM2);
        state.embeddedObjectSealFraction = std::min(
            state.embeddedObjectSealFraction, VehicleScalar{0.25});
        state.stage = std::max(state.stage, requested);
        break;
    case TireFailureStage::Blowout:
        state.punctureAreaM2 = std::max(state.punctureAreaM2, d.blowoutAreaM2);
        state.embeddedObjectSealFraction = 0.0;
        state.blowoutLatched = true;
        state.blowoutElapsedSeconds = 0.0;
        state.structuralIntegrity = std::min(state.structuralIntegrity, VehicleScalar{0.72});
        state.beltIntegrity = std::min(state.beltIntegrity, VehicleScalar{0.72});
        state.cordIntegrity = std::min(state.cordIntegrity, VehicleScalar{0.78});
        state.stage = std::max(state.stage, requested);
        break;
    case TireFailureStage::PartiallyDetachedTread:
        state.treadAttachment = std::min(state.treadAttachment, VehicleScalar{0.55});
        state.delaminationFraction = std::max(state.delaminationFraction, VehicleScalar{0.45});
        state.stage = std::max(state.stage, requested);
        break;
    case TireFailureStage::CollapsedCarcass:
        state.containedGasMassRatio = equilibriumMassRatio(input);
        state.lowPressureLoadedSeconds = d.collapseLoadedDelaySeconds;
        state.structuralIntegrity = std::min(state.structuralIntegrity, VehicleScalar{0.48});
        state.sidewallIntegrity = std::min(state.sidewallIntegrity, VehicleScalar{0.42});
        state.cordIntegrity = std::min(state.cordIntegrity, VehicleScalar{0.48});
        state.rimContactFraction = std::max(state.rimContactFraction, VehicleScalar{0.65});
        state.stage = std::max(state.stage, requested);
        break;
    case TireFailureStage::BareRimRunning:
        state.containedGasMassRatio = equilibriumMassRatio(input);
        state.structuralIntegrity = std::min(
            state.structuralIntegrity, d.bareRimIntegrityThreshold * VehicleScalar{0.5});
        state.beltIntegrity = std::min(state.beltIntegrity, VehicleScalar{0.05});
        state.cordIntegrity = std::min(state.cordIntegrity, VehicleScalar{0.05});
        state.sidewallIntegrity = std::min(state.sidewallIntegrity, VehicleScalar{0.05});
        state.treadAttachment = std::min(
            state.treadAttachment, d.bareRimTreadAttachmentThreshold * VehicleScalar{0.5});
        state.rimContactFraction = 1.0;
        state.stage = TireFailureStage::BareRimRunning;
        break;
    }
}

void triggerTireDamageIncident(
    const TireFailureDescription& d,
    const TireFailureInput& input,
    TireDamageIncident incident,
    VehicleScalar severity01,
    TireFailureState& state)
{
    if (!state.initialized)
        initializeState(state);
    const VehicleScalar severity = clamp01(severity01);
    ++state.eventSerial;
    state.eventElapsedSeconds = 0.0;

    switch (incident)
    {
    case TireDamageIncident::TreadPuncture:
        state.punctureAreaM2 = std::max(
            state.punctureAreaM2,
            d.slowPunctureAreaM2
                + (d.rapidPressureLossAreaM2 - d.slowPunctureAreaM2) * severity);
        state.embeddedObjectSealFraction = std::max(
            state.embeddedObjectSealFraction,
            VehicleScalar{0.90} - VehicleScalar{0.55} * severity);
        break;
    case TireDamageIncident::TreadCut:
        state.punctureAreaM2 = std::max(
            state.punctureAreaM2, d.treadCutAreaM2 * std::max(severity, VehicleScalar{0.15}));
        state.treadAttachment = std::min(
            state.treadAttachment, VehicleScalar{1.0} - VehicleScalar{0.25} * severity);
        break;
    case TireDamageIncident::SidewallCut:
        state.sidewallLeakAreaM2 = std::max(
            state.sidewallLeakAreaM2,
            d.sidewallCutAreaM2 * std::max(severity, VehicleScalar{0.15}));
        state.sidewallIntegrity = std::min(
            state.sidewallIntegrity, VehicleScalar{1.0} - VehicleScalar{0.62} * severity);
        state.cordIntegrity = std::min(
            state.cordIntegrity, VehicleScalar{1.0} - VehicleScalar{0.32} * severity);
        break;
    case TireDamageIncident::ValveLeak:
        state.valveLeakAreaM2 = std::max(
            state.valveLeakAreaM2, d.valveLeakAreaM2 * std::max(severity, VehicleScalar{0.1}));
        break;
    case TireDamageIncident::BeadLeak:
        state.beadLeakAreaM2 = std::max(
            state.beadLeakAreaM2, d.beadLeakAreaM2 * std::max(severity, VehicleScalar{0.1}));
        state.beadRetention = std::min(
            state.beadRetention, VehicleScalar{1.0} - VehicleScalar{0.20} * severity);
        break;
    case TireDamageIncident::BeadUnseat:
        state.beadLeakAreaM2 = std::max(
            state.beadLeakAreaM2, d.rapidPressureLossAreaM2
                * (VehicleScalar{0.4} + severity));
        state.beadRetention = std::min(
            state.beadRetention, VehicleScalar{0.22} * (VehicleScalar{1.0} - severity));
        break;
    case TireDamageIncident::BeltSeparation:
        state.beltIntegrity = std::min(
            state.beltIntegrity, VehicleScalar{1.0} - VehicleScalar{0.80} * severity);
        state.delaminationFraction = std::max(
            state.delaminationFraction, VehicleScalar{0.20} + VehicleScalar{0.75} * severity);
        state.treadAttachment = std::min(
            state.treadAttachment, VehicleScalar{1.0} - VehicleScalar{0.52} * severity);
        break;
    case TireDamageIncident::Impact:
        state.sidewallIntegrity = std::min(
            state.sidewallIntegrity, VehicleScalar{1.0} - VehicleScalar{0.42} * severity);
        state.cordIntegrity = std::min(
            state.cordIntegrity, VehicleScalar{1.0} - VehicleScalar{0.34} * severity);
        state.beltIntegrity = std::min(
            state.beltIntegrity, VehicleScalar{1.0} - VehicleScalar{0.20} * severity);
        if (severity > VehicleScalar{0.82})
            state.sidewallLeakAreaM2 = std::max(state.sidewallLeakAreaM2, d.sidewallCutAreaM2 * 0.25);
        break;
    case TireDamageIncident::RimImpact:
        state.rimIntegrity = std::min(
            state.rimIntegrity, VehicleScalar{1.0} - VehicleScalar{0.70} * severity);
        state.beadRetention = std::min(
            state.beadRetention, VehicleScalar{1.0} - VehicleScalar{0.38} * severity);
        if (severity > VehicleScalar{0.35})
            state.beadLeakAreaM2 = std::max(state.beadLeakAreaM2, d.beadLeakAreaM2 * severity);
        break;
    case TireDamageIncident::RepairPuncture:
        // Repair can close tread/valve leaks, but it cannot restore structural
        // cords, belt, sidewall, bead or rim integrity. Those require replacing
        // the component rather than a magical health reset.
        state.punctureAreaM2 *= VehicleScalar{1.0} - severity;
        state.valveLeakAreaM2 *= VehicleScalar{1.0} - severity;
        state.embeddedObjectSealFraction = std::max(
            state.embeddedObjectSealFraction, severity);
        break;
    }

    // Establish a conservative immediate leak estimate so incident staging is
    // correct before the next dynamic advance recomputes flex-open area.
    const VehicleScalar immediateSeal = std::clamp(
        state.embeddedObjectSealFraction, VehicleScalar{0.0}, VehicleScalar{0.98});
    const VehicleScalar immediateTreadLeak = state.punctureAreaM2
        * (VehicleScalar{1.0} - immediateSeal
            * (VehicleScalar{1.0} - d.minimumEmbeddedSealOpeningFraction));
    state.effectiveLeakAreaM2 = std::max(immediateTreadLeak, VehicleScalar{0.0})
        + std::max(state.valveLeakAreaM2, VehicleScalar{0.0})
        + std::max(state.beadLeakAreaM2, VehicleScalar{0.0})
        + std::max(state.sidewallLeakAreaM2, VehicleScalar{0.0});
    if (state.effectiveLeakAreaM2 >= d.rapidLossAreaThresholdM2)
        state.stage = std::max(state.stage, TireFailureStage::RapidPressureLoss);
    else if (state.punctureAreaM2 > 0.0 || state.valveLeakAreaM2 > 0.0
        || state.beadLeakAreaM2 > 0.0 || state.sidewallLeakAreaM2 > 0.0)
        state.stage = std::max(state.stage, TireFailureStage::SlowPuncture);
    if (state.delaminationFraction > VehicleScalar{0.42}
        || state.treadAttachment < VehicleScalar{0.72})
        state.stage = std::max(state.stage, TireFailureStage::PartiallyDetachedTread);
}

} // namespace heritage::vehicles::tires

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

VehicleScalar smoothStep01(VehicleScalar value)
{
    const VehicleScalar x = std::clamp(value, VehicleScalar{0.0}, VehicleScalar{1.0});
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
    out.structuralIntegrity = state.structuralIntegrity;
    out.treadAttachment = state.treadAttachment;
    out.rimContactFraction = state.rimContactFraction;
    out.eventElapsedSeconds = state.eventElapsedSeconds;

    const VehicleScalar structuralLoss = VehicleScalar{1.0} - state.structuralIntegrity;
    const VehicleScalar treadLoss = VehicleScalar{1.0} - state.treadAttachment;
    out.forceCapacityScale = std::clamp(
        VehicleScalar{1.0}
            - VehicleScalar{0.52} * state.rimContactFraction
            - VehicleScalar{0.22} * structuralLoss
            - VehicleScalar{0.18} * treadLoss,
        VehicleScalar{0.20}, VehicleScalar{1.0});
    out.carcassSupportScale = std::clamp(
        state.structuralIntegrity
            * (VehicleScalar{1.0} - VehicleScalar{0.55} * state.rimContactFraction),
        VehicleScalar{0.18}, VehicleScalar{1.0});
    out.rollingResistanceScale = std::clamp(
        VehicleScalar{1.0}
            + VehicleScalar{5.0} * state.rimContactFraction
            + VehicleScalar{2.0} * structuralLoss
            + VehicleScalar{1.5} * treadLoss,
        VehicleScalar{1.0}, VehicleScalar{9.0});
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
        d.minimumEmbeddedSealOpeningFraction, d.flexOpeningGain,
        d.rapidLossAreaThresholdM2, d.underinflationDamageStartRatio,
        d.collapsedPressureRatio, d.collapseLoadedDelaySeconds,
        d.maximumSafeCarcassTemperatureC,
        d.severeUnderinflationIntegrityLossPerSecond,
        d.collapsedRunningIntegrityLossPerSecond,
        d.collapsedRunningTreadLossPerSecond,
        d.blowoutRunningTreadLossPerSecond,
        d.bareRimIntegrityThreshold, d.bareRimTreadAttachmentThreshold
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
        && d.minimumEmbeddedSealOpeningFraction >= 0.0
        && d.minimumEmbeddedSealOpeningFraction <= 1.0
        && d.rapidLossAreaThresholdM2 >= 0.0
        && d.underinflationDamageStartRatio > d.collapsedPressureRatio
        && d.collapsedPressureRatio >= 0.0
        && d.collapseLoadedDelaySeconds >= 0.0;
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
    state.effectiveLeakAreaM2 = std::max(
        state.punctureAreaM2
            * (VehicleScalar{1.0} - sealedFraction
                * (VehicleScalar{1.0} - sealOpening)),
        VehicleScalar{0.0});

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

    if (input.grounded && loadRatio > 0.05 && pressureRatio <= d.collapsedPressureRatio)
        state.lowPressureLoadedSeconds += dt;
    else
        state.lowPressureLoadedSeconds = std::max(
            state.lowPressureLoadedSeconds - VehicleScalar{0.5} * dt,
            VehicleScalar{0.0});
    if (state.blowoutLatched)
        state.blowoutElapsedSeconds += dt;

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
        state.treadAttachment = std::max(
            state.treadAttachment
                - (d.collapsedRunningTreadLossPerSecond
                    + (state.blowoutLatched
                        ? d.blowoutRunningTreadLossPerSecond : VehicleScalar{0.0}))
                    * runningSeverity * dt,
            VehicleScalar{0.0});
    }

    state.rimContactFraction = smoothStep01(
        (d.collapsedPressureRatio - pressureRatio)
            / std::max(d.collapsedPressureRatio, VehicleScalar{0.01}))
        * smoothStep01((loadRatio - VehicleScalar{0.02}) / VehicleScalar{0.65});
    state.rimContactFraction = std::clamp(
        state.rimContactFraction
            + (VehicleScalar{1.0} - state.structuralIntegrity) * VehicleScalar{0.45},
        VehicleScalar{0.0}, VehicleScalar{1.0});

    TireFailureStage candidate = TireFailureStage::Healthy;
    if (state.punctureAreaM2 > 0.0)
        candidate = TireFailureStage::SlowPuncture;
    if (state.effectiveLeakAreaM2 >= d.rapidLossAreaThresholdM2
        || (state.punctureAreaM2 > 0.0 && pressureRatio < 0.35))
    {
        candidate = TireFailureStage::RapidPressureLoss;
    }
    if (state.blowoutLatched && state.blowoutElapsedSeconds < d.collapseLoadedDelaySeconds)
        candidate = TireFailureStage::Blowout;
    if (state.treadAttachment < 0.72)
        candidate = std::max(candidate, TireFailureStage::PartiallyDetachedTread);
    if (collapseActive)
        candidate = std::max(candidate, TireFailureStage::CollapsedCarcass);
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
        state.stage = std::max(state.stage, requested);
        break;
    case TireFailureStage::PartiallyDetachedTread:
        state.treadAttachment = std::min(state.treadAttachment, VehicleScalar{0.55});
        state.stage = std::max(state.stage, requested);
        break;
    case TireFailureStage::CollapsedCarcass:
        state.containedGasMassRatio = equilibriumMassRatio(input);
        state.lowPressureLoadedSeconds = d.collapseLoadedDelaySeconds;
        state.structuralIntegrity = std::min(state.structuralIntegrity, VehicleScalar{0.48});
        state.rimContactFraction = std::max(state.rimContactFraction, VehicleScalar{0.65});
        state.stage = std::max(state.stage, requested);
        break;
    case TireFailureStage::BareRimRunning:
        state.containedGasMassRatio = equilibriumMassRatio(input);
        state.structuralIntegrity = std::min(
            state.structuralIntegrity, d.bareRimIntegrityThreshold * VehicleScalar{0.5});
        state.treadAttachment = std::min(
            state.treadAttachment, d.bareRimTreadAttachmentThreshold * VehicleScalar{0.5});
        state.rimContactFraction = 1.0;
        state.stage = TireFailureStage::BareRimRunning;
        break;
    }
}

} // namespace heritage::vehicles::tires

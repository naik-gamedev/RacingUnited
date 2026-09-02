#include "TireThermal.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles::tires {
namespace {

constexpr VehicleScalar kKelvinOffset = 273.15;
constexpr VehicleScalar kEpsilon = 1.0e-9;

bool finiteValue(VehicleScalar value)
{
    return std::isfinite(static_cast<double>(value));
}

VehicleScalar clampTemperature(
    const TireThermalDescription& d,
    VehicleScalar temperatureC)
{
    return std::clamp(temperatureC, d.minimumTemperatureC, d.maximumTemperatureC);
}

VehicleScalar rawFrictionTemperatureFactor(
    const TireThermalDescription& d,
    VehicleScalar treadTemperatureC)
{
    const VehicleScalar delta = treadTemperatureC - d.optimumTreadTemperatureC;
    if (delta < 0.0)
    {
        const VehicleScalar x = std::clamp(
            -delta / std::max(d.coldTemperatureSpanC, VehicleScalar{1.0}),
            VehicleScalar{0.0}, VehicleScalar{2.0});
        return std::max(
            VehicleScalar{0.05},
            VehicleScalar{1.0} - d.maximumColdFrictionLoss * x * x);
    }

    const VehicleScalar x = std::clamp(
        delta / std::max(d.hotTemperatureSpanC, VehicleScalar{1.0}),
        VehicleScalar{0.0}, VehicleScalar{2.0});
    return std::max(
        VehicleScalar{0.05},
        VehicleScalar{1.0} - d.maximumHotFrictionLoss * x * x);
}

VehicleScalar frictionScale(
    const TireThermalDescription& d,
    VehicleScalar treadTemperatureC)
{
    const VehicleScalar reference = rawFrictionTemperatureFactor(
        d, d.referenceTemperatureC);
    const VehicleScalar current = rawFrictionTemperatureFactor(
        d, treadTemperatureC);
    return std::clamp(
        current / std::max(reference, VehicleScalar{0.05}),
        d.minimumFrictionScale,
        d.maximumFrictionScale);
}

VehicleScalar stiffnessScale(
    const TireThermalDescription& d,
    VehicleScalar structuralTemperatureC)
{
    const VehicleScalar scale = VehicleScalar{1.0}
        + d.stiffnessTemperatureSlopePerC
            * (structuralTemperatureC - d.referenceTemperatureC);
    return std::clamp(scale, d.minimumStiffnessScale, d.maximumStiffnessScale);
}

VehicleScalar constructionTemperatureC(const TireThermalState& state)
{
    // Belt/carcass dominate cornering/radial stiffness; sidewalls are retained
    // explicitly so asymmetric motorcycle/car camber heating can alter the
    // structural response without forcing the tread temperature to do that job.
    return state.beltTemperatureC * VehicleScalar{0.24}
        + state.carcassTemperatureC * VehicleScalar{0.42}
        + state.innerSidewallTemperatureC * VehicleScalar{0.17}
        + state.outerSidewallTemperatureC * VehicleScalar{0.17};
}

VehicleScalar idealGasGaugePressurePa(
    const TireThermalDescription& d,
    VehicleScalar gasTemperatureC,
    VehicleScalar containedGasMassRatio)
{
    const VehicleScalar referenceKelvin = std::max(
        d.referenceTemperatureC + kKelvinOffset, VehicleScalar{1.0});
    const VehicleScalar gasKelvin = std::max(
        gasTemperatureC + kKelvinOffset, VehicleScalar{1.0});
    const VehicleScalar referenceAbsolutePa = std::max(
        d.referenceGaugePressurePa + d.ambientPressurePa,
        VehicleScalar{1000.0});
    const VehicleScalar absolutePa = std::max(
        d.ambientPressurePa,
        referenceAbsolutePa
            * std::max(containedGasMassRatio, VehicleScalar{0.0})
            * (gasKelvin / referenceKelvin));
    return std::clamp(
        absolutePa - d.ambientPressurePa,
        d.minimumGaugePressurePa,
        d.maximumGaugePressurePa);
}

void initializeState(
    const TireThermalDescription& d,
    TireThermalState& state)
{
    const VehicleScalar requestedGasMassRatio = finiteValue(
        state.containedGasMassRatio)
        ? std::clamp(state.containedGasMassRatio,
            VehicleScalar{0.0}, VehicleScalar{1.0})
        : VehicleScalar{1.0};
    state.initialized = true;
    state.treadTemperatureC = clampTemperature(d, d.initialTreadTemperatureC);
    state.beltTemperatureC = clampTemperature(d, d.initialBeltTemperatureC);
    state.carcassTemperatureC = clampTemperature(d, d.initialCarcassTemperatureC);
    state.innerSidewallTemperatureC = clampTemperature(
        d, d.initialInnerSidewallTemperatureC);
    state.outerSidewallTemperatureC = clampTemperature(
        d, d.initialOuterSidewallTemperatureC);
    state.gasTemperatureC = clampTemperature(d, d.initialGasTemperatureC);
    state.rimTemperatureC = clampTemperature(d, d.initialRimTemperatureC);
    state.containedGasMassRatio = requestedGasMassRatio;
    state.inflationPressurePa = idealGasGaugePressurePa(
        d, state.gasTemperatureC, state.containedGasMassRatio);
}

TireThermalOutput outputFromState(
    const TireThermalDescription& d,
    const TireThermalState& state)
{
    TireThermalOutput out;
    if (!d.enabled)
        return out;

    TireThermalState readable = state;
    if (!readable.initialized)
        initializeState(d, readable);

    out.valid = true;
    out.treadTemperatureC = readable.treadTemperatureC;
    out.beltTemperatureC = readable.beltTemperatureC;
    out.carcassTemperatureC = readable.carcassTemperatureC;
    out.innerSidewallTemperatureC = readable.innerSidewallTemperatureC;
    out.outerSidewallTemperatureC = readable.outerSidewallTemperatureC;
    out.gasTemperatureC = readable.gasTemperatureC;
    out.rimTemperatureC = readable.rimTemperatureC;
    out.inflationPressurePa = idealGasGaugePressurePa(
        d, readable.gasTemperatureC, readable.containedGasMassRatio);
    out.frictionScale = frictionScale(d, readable.treadTemperatureC);
    out.stiffnessScale = stiffnessScale(d, constructionTemperatureC(readable));
    return out;
}

} // namespace

VehicleScalar tireThermalFrictionScaleForTemperature(
    const TireThermalDescription& description,
    VehicleScalar treadTemperatureC)
{
    if (!description.enabled || !validTireThermalDescription(description)
        || !finiteValue(treadTemperatureC))
    {
        return VehicleScalar{1.0};
    }
    return frictionScale(description, treadTemperatureC);
}

VehicleScalar tireThermalStiffnessScaleForTemperature(
    const TireThermalDescription& description,
    VehicleScalar structuralTemperatureC)
{
    if (!description.enabled || !validTireThermalDescription(description)
        || !finiteValue(structuralTemperatureC))
    {
        return VehicleScalar{1.0};
    }
    return stiffnessScale(description, structuralTemperatureC);
}

bool validTireThermalDescription(const TireThermalDescription& d)
{
    if (!d.enabled)
        return true;

    const VehicleScalar values[] = {
        d.referenceTemperatureC,
        d.initialTreadTemperatureC,
        d.initialBeltTemperatureC,
        d.initialCarcassTemperatureC,
        d.initialInnerSidewallTemperatureC,
        d.initialOuterSidewallTemperatureC,
        d.initialGasTemperatureC,
        d.initialRimTemperatureC,
        d.ambientTemperatureC,
        d.roadTemperatureC,
        d.ambientPressurePa,
        d.referenceGaugePressurePa,
        d.treadHeatCapacityJPerK,
        d.beltHeatCapacityJPerK,
        d.carcassHeatCapacityJPerK,
        d.innerSidewallHeatCapacityJPerK,
        d.outerSidewallHeatCapacityJPerK,
        d.gasHeatCapacityJPerK,
        d.rimHeatCapacityJPerK,
        d.treadToBeltConductanceWPerK,
        d.treadToCarcassConductanceWPerK,
        d.beltToCarcassConductanceWPerK,
        d.carcassToInnerSidewallConductanceWPerK,
        d.carcassToOuterSidewallConductanceWPerK,
        d.treadToRoadConductanceWPerK,
        d.treadToAirConductanceWPerK,
        d.carcassToAirConductanceWPerK,
        d.innerSidewallToAirConductanceWPerK,
        d.outerSidewallToAirConductanceWPerK,
        d.carcassToGasConductanceWPerK,
        d.innerSidewallToGasConductanceWPerK,
        d.outerSidewallToGasConductanceWPerK,
        d.gasToAmbientConductanceWPerK,
        d.carcassToRimConductanceWPerK,
        d.innerSidewallToRimConductanceWPerK,
        d.outerSidewallToRimConductanceWPerK,
        d.rimToAirConductanceWPerK,
        d.treadAirSpeedConductanceWPerKPerMps,
        d.carcassAirSpeedConductanceWPerKPerMps,
        d.sidewallAirSpeedConductanceWPerKPerMps,
        d.rimAirSpeedConductanceWPerKPerMps,
        d.slipHeatFractionToTread,
        d.slipHeatFractionToBelt,
        d.slipHeatEfficiency,
        d.carcassLossHeatEfficiency,
        d.carcassLossHeatFractionToBelt,
        d.sidewallFlexHeatFraction,
        d.brakeHeatFractionToRim,
        d.optimumTreadTemperatureC,
        d.coldTemperatureSpanC,
        d.hotTemperatureSpanC,
        d.maximumColdFrictionLoss,
        d.maximumHotFrictionLoss,
        d.minimumFrictionScale,
        d.maximumFrictionScale,
        d.stiffnessTemperatureSlopePerC,
        d.minimumStiffnessScale,
        d.maximumStiffnessScale,
        d.minimumTemperatureC,
        d.maximumTemperatureC,
        d.minimumGaugePressurePa,
        d.maximumGaugePressurePa
    };
    for (VehicleScalar value : values)
    {
        if (!finiteValue(value))
            return false;
    }

    return d.ambientPressurePa > 10000.0
        && d.referenceGaugePressurePa >= 0.0
        && d.treadHeatCapacityJPerK > 10.0
        && d.beltHeatCapacityJPerK > 10.0
        && d.carcassHeatCapacityJPerK > 10.0
        && d.innerSidewallHeatCapacityJPerK > 10.0
        && d.outerSidewallHeatCapacityJPerK > 10.0
        && d.gasHeatCapacityJPerK > 1.0
        && d.rimHeatCapacityJPerK > 10.0
        && d.treadToBeltConductanceWPerK >= 0.0
        && d.treadToCarcassConductanceWPerK >= 0.0
        && d.beltToCarcassConductanceWPerK >= 0.0
        && d.carcassToInnerSidewallConductanceWPerK >= 0.0
        && d.carcassToOuterSidewallConductanceWPerK >= 0.0
        && d.treadToRoadConductanceWPerK >= 0.0
        && d.treadToAirConductanceWPerK >= 0.0
        && d.carcassToAirConductanceWPerK >= 0.0
        && d.innerSidewallToAirConductanceWPerK >= 0.0
        && d.outerSidewallToAirConductanceWPerK >= 0.0
        && d.carcassToGasConductanceWPerK >= 0.0
        && d.innerSidewallToGasConductanceWPerK >= 0.0
        && d.outerSidewallToGasConductanceWPerK >= 0.0
        && d.gasToAmbientConductanceWPerK >= 0.0
        && d.carcassToRimConductanceWPerK >= 0.0
        && d.innerSidewallToRimConductanceWPerK >= 0.0
        && d.outerSidewallToRimConductanceWPerK >= 0.0
        && d.rimToAirConductanceWPerK >= 0.0
        && d.slipHeatFractionToTread >= 0.0
        && d.slipHeatFractionToTread <= 1.0
        && d.slipHeatFractionToBelt >= 0.0
        && d.slipHeatFractionToBelt <= 1.0
        && d.slipHeatFractionToTread + d.slipHeatFractionToBelt <= 1.0
        && d.slipHeatEfficiency >= 0.0
        && d.slipHeatEfficiency <= 1.5
        && d.carcassLossHeatEfficiency >= 0.0
        && d.carcassLossHeatEfficiency <= 1.5
        && d.carcassLossHeatFractionToBelt >= 0.0
        && d.carcassLossHeatFractionToBelt <= 1.0
        && d.sidewallFlexHeatFraction >= 0.0
        && d.sidewallFlexHeatFraction <= 1.0
        && d.carcassLossHeatFractionToBelt + d.sidewallFlexHeatFraction <= 1.0
        && d.brakeHeatFractionToRim >= 0.0
        && d.brakeHeatFractionToRim <= 1.0
        && d.coldTemperatureSpanC > 1.0
        && d.hotTemperatureSpanC > 1.0
        && d.minimumFrictionScale > 0.0
        && d.maximumFrictionScale >= d.minimumFrictionScale
        && d.minimumStiffnessScale > 0.0
        && d.maximumStiffnessScale >= d.minimumStiffnessScale
        && d.maximumTemperatureC > d.minimumTemperatureC
        && d.maximumGaugePressurePa > d.minimumGaugePressurePa;
}

TireThermalOutput evaluateTireThermalState(
    const TireThermalDescription& description,
    const TireThermalState& state)
{
    if (!validTireThermalDescription(description))
        return {};
    return outputFromState(description, state);
}

TireThermalOutput advanceTireThermal(
    const TireThermalDescription& d,
    const TireThermalInput& input,
    VehicleScalar dt,
    TireThermalState& state)
{
    TireThermalOutput out;
    if (!d.enabled || !validTireThermalDescription(d)
        || !finiteValue(dt) || dt <= 0.0)
    {
        return out;
    }

    if (!state.initialized)
        initializeState(d, state);

    dt = std::min(dt, VehicleScalar{0.05});

    const VehicleScalar slipPower = d.slipHeatEfficiency * (
        std::abs(input.longitudinalForceN * input.longitudinalSlipVelocityMps)
        + std::abs(input.lateralForceN * input.lateralSlipVelocityMps));
    const VehicleScalar carcassLossPower = d.carcassLossHeatEfficiency
        * (std::max(input.radialDissipationWatts, VehicleScalar{0.0})
            + std::max(input.rollingResistanceDissipationWatts, VehicleScalar{0.0}));
    const VehicleScalar rimBrakeSource = d.brakeHeatFractionToRim
        * std::max(input.brakeDissipationWatts, VehicleScalar{0.0});

    const VehicleScalar treadSource = slipPower * d.slipHeatFractionToTread;
    const VehicleScalar beltSource = slipPower * d.slipHeatFractionToBelt
        + carcassLossPower * d.carcassLossHeatFractionToBelt;
    const VehicleScalar remainingSlipFraction = std::max(
        VehicleScalar{1.0} - d.slipHeatFractionToTread - d.slipHeatFractionToBelt,
        VehicleScalar{0.0});
    const VehicleScalar sidewallSourceTotal = carcassLossPower
        * d.sidewallFlexHeatFraction;
    const VehicleScalar carcassLossFraction = std::max(
        VehicleScalar{1.0} - d.carcassLossHeatFractionToBelt
            - d.sidewallFlexHeatFraction,
        VehicleScalar{0.0});
    const VehicleScalar carcassSource = slipPower * remainingSlipFraction
        + carcassLossPower * carcassLossFraction;

    // Lean/camber makes one sidewall flex harder. The exact construction is an
    // authoring parameter problem, but the sign and bounded energy split are
    // physical and essential for motorcycles/high-camber tires.
    const VehicleScalar camberBias = std::clamp(
        std::tanh(input.camberAngleRadians * VehicleScalar{2.2})
            * VehicleScalar{0.34},
        VehicleScalar{-0.34}, VehicleScalar{0.34});
    const VehicleScalar innerSidewallSource = sidewallSourceTotal
        * (VehicleScalar{0.5} - camberBias);
    const VehicleScalar outerSidewallSource = sidewallSourceTotal
        * (VehicleScalar{0.5} + camberBias);

    const VehicleScalar speed = std::hypot(
        input.forwardSpeedMps, input.ambientAirSpeedMps);
    const VehicleScalar treadAirConductance = std::max(
        d.treadToAirConductanceWPerK
            + d.treadAirSpeedConductanceWPerKPerMps * speed,
        VehicleScalar{0.0});
    const VehicleScalar carcassAirConductance = std::max(
        d.carcassToAirConductanceWPerK
            + d.carcassAirSpeedConductanceWPerKPerMps * speed,
        VehicleScalar{0.0});
    const VehicleScalar sidewallAirConductance = std::max(
        d.sidewallAirSpeedConductanceWPerKPerMps * speed,
        VehicleScalar{0.0});
    const VehicleScalar innerSidewallAirConductance = std::max(
        d.innerSidewallToAirConductanceWPerK + sidewallAirConductance,
        VehicleScalar{0.0});
    const VehicleScalar outerSidewallAirConductance = std::max(
        d.outerSidewallToAirConductanceWPerK + sidewallAirConductance,
        VehicleScalar{0.0});
    const VehicleScalar rimAirConductance = std::max(
        d.rimToAirConductanceWPerK
            + d.rimAirSpeedConductanceWPerKPerMps * speed,
        VehicleScalar{0.0});

    const VehicleScalar ambientTemperatureC =
        input.environmentTemperatureOverride && finiteValue(input.ambientTemperatureC)
        ? input.ambientTemperatureC : d.ambientTemperatureC;
    const VehicleScalar roadTemperatureC =
        input.environmentTemperatureOverride && finiteValue(input.roadTemperatureC)
        ? input.roadTemperatureC : d.roadTemperatureC;

    const VehicleScalar qTreadBelt = d.treadToBeltConductanceWPerK
        * (state.treadTemperatureC - state.beltTemperatureC);
    const VehicleScalar qTreadCarcass = d.treadToCarcassConductanceWPerK
        * (state.treadTemperatureC - state.carcassTemperatureC);
    const VehicleScalar qBeltCarcass = d.beltToCarcassConductanceWPerK
        * (state.beltTemperatureC - state.carcassTemperatureC);
    const VehicleScalar qCarcassInnerSidewall =
        d.carcassToInnerSidewallConductanceWPerK
        * (state.carcassTemperatureC - state.innerSidewallTemperatureC);
    const VehicleScalar qCarcassOuterSidewall =
        d.carcassToOuterSidewallConductanceWPerK
        * (state.carcassTemperatureC - state.outerSidewallTemperatureC);

    const VehicleScalar areaScale = input.grounded
        ? std::clamp(input.contactPatchAreaM2 / VehicleScalar{0.020},
            VehicleScalar{0.15}, VehicleScalar{1.75})
        : VehicleScalar{0.0};
    const VehicleScalar roadHeatScale = std::clamp(
        input.roadHeatTransferScale, VehicleScalar{0.0}, VehicleScalar{2.0});
    const VehicleScalar qTreadRoad = d.treadToRoadConductanceWPerK
        * areaScale * roadHeatScale
        * (state.treadTemperatureC - roadTemperatureC);
    const VehicleScalar qTreadAir = treadAirConductance
        * (state.treadTemperatureC - ambientTemperatureC);
    const VehicleScalar qCarcassAir = carcassAirConductance
        * (state.carcassTemperatureC - ambientTemperatureC);
    const VehicleScalar qInnerSidewallAir = innerSidewallAirConductance
        * (state.innerSidewallTemperatureC - ambientTemperatureC);
    const VehicleScalar qOuterSidewallAir = outerSidewallAirConductance
        * (state.outerSidewallTemperatureC - ambientTemperatureC);

    const VehicleScalar qCarcassGas = d.carcassToGasConductanceWPerK
        * (state.carcassTemperatureC - state.gasTemperatureC);
    const VehicleScalar qInnerSidewallGas = d.innerSidewallToGasConductanceWPerK
        * (state.innerSidewallTemperatureC - state.gasTemperatureC);
    const VehicleScalar qOuterSidewallGas = d.outerSidewallToGasConductanceWPerK
        * (state.outerSidewallTemperatureC - state.gasTemperatureC);
    const VehicleScalar qGasAmbient = d.gasToAmbientConductanceWPerK
        * (state.gasTemperatureC - ambientTemperatureC);

    const VehicleScalar qCarcassRim = d.carcassToRimConductanceWPerK
        * (state.carcassTemperatureC - state.rimTemperatureC);
    const VehicleScalar qInnerSidewallRim = d.innerSidewallToRimConductanceWPerK
        * (state.innerSidewallTemperatureC - state.rimTemperatureC);
    const VehicleScalar qOuterSidewallRim = d.outerSidewallToRimConductanceWPerK
        * (state.outerSidewallTemperatureC - state.rimTemperatureC);
    const VehicleScalar qRimAir = rimAirConductance
        * (state.rimTemperatureC - ambientTemperatureC);

    const VehicleScalar treadNet = treadSource
        - qTreadBelt - qTreadCarcass - qTreadRoad - qTreadAir;
    const VehicleScalar beltNet = beltSource + qTreadBelt - qBeltCarcass;
    const VehicleScalar carcassNet = carcassSource
        + qTreadCarcass + qBeltCarcass
        - qCarcassInnerSidewall - qCarcassOuterSidewall
        - qCarcassAir - qCarcassGas - qCarcassRim;
    const VehicleScalar innerSidewallNet = innerSidewallSource
        + qCarcassInnerSidewall - qInnerSidewallAir
        - qInnerSidewallGas - qInnerSidewallRim;
    const VehicleScalar outerSidewallNet = outerSidewallSource
        + qCarcassOuterSidewall - qOuterSidewallAir
        - qOuterSidewallGas - qOuterSidewallRim;
    const VehicleScalar gasNet = qCarcassGas + qInnerSidewallGas
        + qOuterSidewallGas - qGasAmbient;
    const VehicleScalar rimNet = rimBrakeSource + qCarcassRim
        + qInnerSidewallRim + qOuterSidewallRim - qRimAir;

    state.treadTemperatureC = clampTemperature(
        d, state.treadTemperatureC
            + treadNet / std::max(d.treadHeatCapacityJPerK, kEpsilon) * dt);
    state.beltTemperatureC = clampTemperature(
        d, state.beltTemperatureC
            + beltNet / std::max(d.beltHeatCapacityJPerK, kEpsilon) * dt);
    state.carcassTemperatureC = clampTemperature(
        d, state.carcassTemperatureC
            + carcassNet / std::max(d.carcassHeatCapacityJPerK, kEpsilon) * dt);
    state.innerSidewallTemperatureC = clampTemperature(
        d, state.innerSidewallTemperatureC
            + innerSidewallNet
                / std::max(d.innerSidewallHeatCapacityJPerK, kEpsilon) * dt);
    state.outerSidewallTemperatureC = clampTemperature(
        d, state.outerSidewallTemperatureC
            + outerSidewallNet
                / std::max(d.outerSidewallHeatCapacityJPerK, kEpsilon) * dt);
    state.gasTemperatureC = clampTemperature(
        d, state.gasTemperatureC
            + gasNet / std::max(d.gasHeatCapacityJPerK, kEpsilon) * dt);
    state.rimTemperatureC = clampTemperature(
        d, state.rimTemperatureC
            + rimNet / std::max(d.rimHeatCapacityJPerK, kEpsilon) * dt);
    state.inflationPressurePa = idealGasGaugePressurePa(
        d, state.gasTemperatureC, state.containedGasMassRatio);

    out = outputFromState(d, state);
    out.slipDissipationWatts = slipPower;
    out.carcassDissipationWatts = carcassLossPower;
    out.sidewallDissipationWatts = sidewallSourceTotal;
    out.roadHeatFlowWatts = qTreadRoad;
    out.airHeatFlowWatts = qTreadAir + qCarcassAir
        + qInnerSidewallAir + qOuterSidewallAir + qGasAmbient + qRimAir;
    out.brakeHeatInputWatts = rimBrakeSource;
    out.rimToCarcassHeatFlowWatts = -(
        qCarcassRim + qInnerSidewallRim + qOuterSidewallRim);
    return out;
}

} // namespace heritage::vehicles::tires

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
    return std::clamp(
        temperatureC,
        d.minimumTemperatureC,
        d.maximumTemperatureC);
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
    VehicleScalar carcassTemperatureC)
{
    const VehicleScalar scale = VehicleScalar{1.0}
        + d.stiffnessTemperatureSlopePerC
            * (carcassTemperatureC - d.referenceTemperatureC);
    return std::clamp(
        scale,
        d.minimumStiffnessScale,
        d.maximumStiffnessScale);
}

VehicleScalar idealGasGaugePressurePa(
    const TireThermalDescription& d,
    VehicleScalar gasTemperatureC)
{
    const VehicleScalar referenceKelvin = std::max(
        d.referenceTemperatureC + kKelvinOffset,
        VehicleScalar{1.0});
    const VehicleScalar gasKelvin = std::max(
        gasTemperatureC + kKelvinOffset,
        VehicleScalar{1.0});
    const VehicleScalar referenceAbsolutePa = std::max(
        d.referenceGaugePressurePa + d.ambientPressurePa,
        VehicleScalar{1000.0});
    const VehicleScalar absolutePa = referenceAbsolutePa
        * (gasKelvin / referenceKelvin);
    return std::clamp(
        absolutePa - d.ambientPressurePa,
        d.minimumGaugePressurePa,
        d.maximumGaugePressurePa);
}

void initializeState(
    const TireThermalDescription& d,
    TireThermalState& state)
{
    state.initialized = true;
    state.treadTemperatureC = clampTemperature(d, d.initialTreadTemperatureC);
    state.carcassTemperatureC = clampTemperature(d, d.initialCarcassTemperatureC);
    state.gasTemperatureC = clampTemperature(d, d.initialGasTemperatureC);
    state.inflationPressurePa = idealGasGaugePressurePa(
        d, state.gasTemperatureC);
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
    out.carcassTemperatureC = readable.carcassTemperatureC;
    out.gasTemperatureC = readable.gasTemperatureC;
    out.inflationPressurePa = idealGasGaugePressurePa(
        d, readable.gasTemperatureC);
    out.frictionScale = frictionScale(d, readable.treadTemperatureC);
    out.stiffnessScale = stiffnessScale(d, readable.carcassTemperatureC);
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
    VehicleScalar carcassTemperatureC)
{
    if (!description.enabled || !validTireThermalDescription(description)
        || !finiteValue(carcassTemperatureC))
    {
        return VehicleScalar{1.0};
    }
    return stiffnessScale(description, carcassTemperatureC);
}

bool validTireThermalDescription(const TireThermalDescription& d)
{
    if (!d.enabled)
        return true;

    const VehicleScalar values[] = {
        d.referenceTemperatureC,
        d.initialTreadTemperatureC,
        d.initialCarcassTemperatureC,
        d.initialGasTemperatureC,
        d.ambientTemperatureC,
        d.roadTemperatureC,
        d.ambientPressurePa,
        d.referenceGaugePressurePa,
        d.treadHeatCapacityJPerK,
        d.carcassHeatCapacityJPerK,
        d.gasHeatCapacityJPerK,
        d.treadToCarcassConductanceWPerK,
        d.treadToRoadConductanceWPerK,
        d.treadToAirConductanceWPerK,
        d.carcassToAirConductanceWPerK,
        d.carcassToGasConductanceWPerK,
        d.gasToAmbientConductanceWPerK,
        d.treadAirSpeedConductanceWPerKPerMps,
        d.carcassAirSpeedConductanceWPerKPerMps,
        d.slipHeatFractionToTread,
        d.slipHeatEfficiency,
        d.carcassLossHeatEfficiency,
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
        && d.carcassHeatCapacityJPerK > 10.0
        && d.gasHeatCapacityJPerK > 1.0
        && d.treadToCarcassConductanceWPerK >= 0.0
        && d.treadToRoadConductanceWPerK >= 0.0
        && d.treadToAirConductanceWPerK >= 0.0
        && d.carcassToAirConductanceWPerK >= 0.0
        && d.carcassToGasConductanceWPerK >= 0.0
        && d.gasToAmbientConductanceWPerK >= 0.0
        && d.slipHeatFractionToTread >= 0.0
        && d.slipHeatFractionToTread <= 1.0
        && d.slipHeatEfficiency >= 0.0
        && d.slipHeatEfficiency <= 1.5
        && d.carcassLossHeatEfficiency >= 0.0
        && d.carcassLossHeatEfficiency <= 1.5
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

    const VehicleScalar treadSource = slipPower * d.slipHeatFractionToTread;
    const VehicleScalar carcassSource = slipPower
        * (VehicleScalar{1.0} - d.slipHeatFractionToTread)
        + carcassLossPower;

    const VehicleScalar speed = std::abs(input.forwardSpeedMps);
    const VehicleScalar treadAirConductance = std::max(
        d.treadToAirConductanceWPerK
            + d.treadAirSpeedConductanceWPerKPerMps * speed,
        VehicleScalar{0.0});
    const VehicleScalar carcassAirConductance = std::max(
        d.carcassToAirConductanceWPerK
            + d.carcassAirSpeedConductanceWPerKPerMps * speed,
        VehicleScalar{0.0});

    const VehicleScalar ambientTemperatureC =
        input.environmentTemperatureOverride
            && finiteValue(input.ambientTemperatureC)
        ? input.ambientTemperatureC
        : d.ambientTemperatureC;
    const VehicleScalar roadTemperatureC =
        input.environmentTemperatureOverride
            && finiteValue(input.roadTemperatureC)
        ? input.roadTemperatureC
        : d.roadTemperatureC;

    const VehicleScalar qTreadCarcass = d.treadToCarcassConductanceWPerK
        * (state.treadTemperatureC - state.carcassTemperatureC);
    // Contact-area scale keeps a barely loaded/airborne tire from conducting
    // as much heat to the road as a fully developed footprint.
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
    const VehicleScalar qCarcassGas = d.carcassToGasConductanceWPerK
        * (state.carcassTemperatureC - state.gasTemperatureC);
    const VehicleScalar qGasAmbient = d.gasToAmbientConductanceWPerK
        * (state.gasTemperatureC - ambientTemperatureC);

    const VehicleScalar treadNet = treadSource
        - qTreadCarcass - qTreadRoad - qTreadAir;
    const VehicleScalar carcassNet = carcassSource
        + qTreadCarcass - qCarcassAir - qCarcassGas;
    const VehicleScalar gasNet = qCarcassGas - qGasAmbient;

    state.treadTemperatureC = clampTemperature(
        d,
        state.treadTemperatureC
            + treadNet / std::max(d.treadHeatCapacityJPerK, kEpsilon) * dt);
    state.carcassTemperatureC = clampTemperature(
        d,
        state.carcassTemperatureC
            + carcassNet / std::max(d.carcassHeatCapacityJPerK, kEpsilon) * dt);
    state.gasTemperatureC = clampTemperature(
        d,
        state.gasTemperatureC
            + gasNet / std::max(d.gasHeatCapacityJPerK, kEpsilon) * dt);
    state.inflationPressurePa = idealGasGaugePressurePa(
        d, state.gasTemperatureC);

    out = outputFromState(d, state);
    out.slipDissipationWatts = slipPower;
    out.carcassDissipationWatts = carcassLossPower;
    out.roadHeatFlowWatts = qTreadRoad;
    out.airHeatFlowWatts = qTreadAir + qCarcassAir + qGasAmbient;
    return out;
}

} // namespace heritage::vehicles::tires

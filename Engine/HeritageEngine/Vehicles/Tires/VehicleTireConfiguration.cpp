#include "../VehicleSystem.hpp"
#include "../VehicleSystemInternal.hpp"
#include "../Tires/TireSlipDynamics.hpp"
#include "../Tires/TireContactPatch.hpp"
#include "../Tires/Authoring/TirePartResolver.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::vehicles {
using namespace vehicle_system_detail;

namespace {

constexpr VehicleScalar kPascalsPerPsi = VehicleScalar{6894.757293168};
constexpr VehicleScalar kMaximumLivePressurePa = VehicleScalar{150.0}
    * kPascalsPerPsi;

std::pair<VehicleScalar, VehicleScalar> livePressureRange()
{
    // The live laboratory range intentionally exceeds a fitted MF dataset.
    // Each fitted provider clamps only its coefficients internally; structural,
    // thermal and visual systems continue to consume the actual gauge pressure.
    return { VehicleScalar{0.0}, kMaximumLivePressurePa };
}

bool pressureInsideLiveRange(VehicleScalar pressurePa)
{
    if (!std::isfinite(static_cast<double>(pressurePa)))
        return false;
    const auto [minimumPa, maximumPa] = livePressureRange();
    return pressurePa >= minimumPa && pressurePa <= maximumPa;
}

void applyLegacyForceTuning(
    TireModelDescription& target,
    const TireModelDescription& tuning)
{
    target.nominalLoad = tuning.nominalLoad;
    target.peakFriction = tuning.peakFriction;
    target.longitudinalStiffness = tuning.longitudinalStiffness;
    target.corneringStiffness = tuning.corneringStiffness;
    target.loadSensitivity = tuning.loadSensitivity;
    target.longitudinalRelaxationLength = tuning.longitudinalRelaxationLength;
    target.lateralRelaxationLength = tuning.lateralRelaxationLength;
    target.wheelInertia = tuning.wheelInertia;
    target.pneumaticTrail = tuning.pneumaticTrail;
    target.stiffnessLoadExponent = tuning.stiffnessLoadExponent;
    target.longitudinalShapeFactor = tuning.longitudinalShapeFactor;
    target.lateralShapeFactor = tuning.lateralShapeFactor;
    target.longitudinalCurvatureFactor = tuning.longitudinalCurvatureFactor;
    target.lateralCurvatureFactor = tuning.lateralCurvatureFactor;
    target.combinedSlipExponent = tuning.combinedSlipExponent;
    target.pneumaticTrailFalloff = tuning.pneumaticTrailFalloff;

    // This API is the compatibility/manual curve editor, so its numbers must
    // become the active MF seed. Keep fitted geometry and native thermal,
    // failure, wear and surface providers: changing a force curve must not
    // silently make punctures, pressure or tread state cease to exist.
    target.magicFormulaUsesLegacySeed = true;
    target.importedPropertyFile = false;
    target.importedFitType = 0;
    target.parameterSource = "Heritage manual tire tuning";
    target.parameterProvenance = "manual_legacy_force_override";
    target.parameterTireSide.clear();
    target.parameterConfidence = 0.0;
    target.importedMappedParameterCount = 0;
    target.importedUnsupportedParameterCount = 0;
}

} // namespace

bool VehicleSystem::setTireModel(
    VehicleHandle handle,
    VehicleScalar nominalLoad,
    VehicleScalar peakFriction,
    VehicleScalar longitudinalStiffness,
    VehicleScalar corneringStiffness,
    VehicleScalar loadSensitivity,
    VehicleScalar longitudinalRelaxationLength,
    VehicleScalar lateralRelaxationLength,
    VehicleScalar wheelInertia,
    VehicleScalar pneumaticTrail,
    VehicleScalar stiffnessLoadExponent,
    VehicleScalar longitudinalShapeFactor,
    VehicleScalar lateralShapeFactor,
    VehicleScalar longitudinalCurvatureFactor,
    VehicleScalar lateralCurvatureFactor,
    VehicleScalar combinedSlipExponent,
    VehicleScalar pneumaticTrailFalloff)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetTireModel received an invalid or stale vehicle handle.");
        return false;
    }

    TireModelDescription value;
    value.nominalLoad = nominalLoad;
    value.peakFriction = peakFriction;
    value.longitudinalStiffness = longitudinalStiffness;
    value.corneringStiffness = corneringStiffness;
    value.loadSensitivity = loadSensitivity;
    value.longitudinalRelaxationLength = longitudinalRelaxationLength;
    value.lateralRelaxationLength = lateralRelaxationLength;
    value.wheelInertia = wheelInertia;
    value.pneumaticTrail = pneumaticTrail;
    value.stiffnessLoadExponent = stiffnessLoadExponent;
    value.longitudinalShapeFactor = longitudinalShapeFactor;
    value.lateralShapeFactor = lateralShapeFactor;
    value.longitudinalCurvatureFactor = longitudinalCurvatureFactor;
    value.lateralCurvatureFactor = lateralCurvatureFactor;
    value.combinedSlipExponent = combinedSlipExponent;
    value.pneumaticTrailFalloff = pneumaticTrailFalloff;
    if (!validTireModelDescription(value))
    {
        setError("Vehicle.SetTireModel Step 29H default road-tire data is outside the supported range.");
        return false;
    }

    TireModelDescription defaultValue = slot->record.tireModel;
    applyLegacyForceTuning(defaultValue, value);
    if (!validTireModelDescription(defaultValue))
    {
        setError("Vehicle.SetTireModel could not preserve the fitted tire providers with this tuning.");
        return false;
    }
    slot->record.tireModel = defaultValue;
    slot->record.description.tireFriction = static_cast<float>(value.peakFriction);
    // Preserve the legacy/global meaning of SetTireModel: it updates the
    // default profile and every wheel already attached to the vehicle.
    for (WheelRecord& wheel : slot->record.wheels)
    {
        TireModelDescription wheelValue = wheel.tireModel;
        applyLegacyForceTuning(wheelValue, value);
        if (!validTireModelDescription(wheelValue))
        {
            setError("Vehicle.SetTireModel could not preserve one fitted wheel's providers with this tuning.");
            return false;
        }
        wheel.tireModel = wheelValue;
        wheel.tirePartAssignment = {};
        wheel.thermalState = {};
        wheel.failureState = {};
        wheel.wearState = {};
    }
    clearError();
    return true;
}

bool VehicleSystem::setWheelTireModel(
    VehicleHandle handle,
    std::size_t wheelIndex,
    VehicleScalar nominalLoad,
    VehicleScalar peakFriction,
    VehicleScalar longitudinalStiffness,
    VehicleScalar corneringStiffness,
    VehicleScalar loadSensitivity,
    VehicleScalar longitudinalRelaxationLength,
    VehicleScalar lateralRelaxationLength,
    VehicleScalar wheelInertia,
    VehicleScalar pneumaticTrail,
    VehicleScalar stiffnessLoadExponent,
    VehicleScalar longitudinalShapeFactor,
    VehicleScalar lateralShapeFactor,
    VehicleScalar longitudinalCurvatureFactor,
    VehicleScalar lateralCurvatureFactor,
    VehicleScalar combinedSlipExponent,
    VehicleScalar pneumaticTrailFalloff)
{
    Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.SetWheelTireModel received an invalid vehicle handle or wheel index.");
        return false;
    }

    TireModelDescription value;
    value.nominalLoad = nominalLoad;
    value.peakFriction = peakFriction;
    value.longitudinalStiffness = longitudinalStiffness;
    value.corneringStiffness = corneringStiffness;
    value.loadSensitivity = loadSensitivity;
    value.longitudinalRelaxationLength = longitudinalRelaxationLength;
    value.lateralRelaxationLength = lateralRelaxationLength;
    value.wheelInertia = wheelInertia;
    value.pneumaticTrail = pneumaticTrail;
    value.stiffnessLoadExponent = stiffnessLoadExponent;
    value.longitudinalShapeFactor = longitudinalShapeFactor;
    value.lateralShapeFactor = lateralShapeFactor;
    value.longitudinalCurvatureFactor = longitudinalCurvatureFactor;
    value.lateralCurvatureFactor = lateralCurvatureFactor;
    value.combinedSlipExponent = combinedSlipExponent;
    value.pneumaticTrailFalloff = pneumaticTrailFalloff;
    if (!validTireModelDescription(value))
    {
        setError("Vehicle.SetWheelTireModel Step 29H per-wheel tire data is outside the supported range.");
        return false;
    }

    WheelRecord& wheel = slot->record.wheels[wheelIndex];
    TireModelDescription wheelValue = wheel.tireModel;
    applyLegacyForceTuning(wheelValue, value);
    if (!validTireModelDescription(wheelValue))
    {
        setError("Vehicle.SetWheelTireModel could not preserve the fitted wheel's providers with this tuning.");
        return false;
    }
    wheel.tireModel = wheelValue;
    wheel.tirePartAssignment = {};
    wheel.thermalState = {};
    wheel.failureState = {};
    wheel.wearState = {};
    clearError();
    return true;
}

bool VehicleSystem::setWheelTireProvider(
    VehicleHandle handle,
    std::size_t wheelIndex,
    TireProviderKind provider)
{
    Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.SetWheelTireProvider received an invalid vehicle handle or wheel index.");
        return false;
    }

    TireModelDescription value = slot->record.wheels[wheelIndex].tireModel;
    value.provider = provider;
    if (!validTireModelDescription(value))
    {
        setError("Vehicle.SetWheelTireProvider received an invalid tire-provider configuration.");
        return false;
    }

    slot->record.wheels[wheelIndex].tireModel = value;
    slot->record.wheels[wheelIndex].tirePartAssignment = {};
    clearError();
    return true;
}

bool VehicleSystem::setWheelTireDescription(
    VehicleHandle handle,
    std::size_t wheelIndex,
    const TireModelDescription& description)
{
    Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.SetWheelTireDescription received an invalid vehicle handle or wheel index.");
        return false;
    }
    if (!validTireModelDescription(description))
    {
        setError("Vehicle.SetWheelTireDescription received invalid tire data.");
        return false;
    }
    slot->record.wheels[wheelIndex].tireModel = description;
    slot->record.wheels[wheelIndex].tirePartAssignment = {};
    slot->record.wheels[wheelIndex].thermalState = {};
    slot->record.wheels[wheelIndex].failureState = {};
    slot->record.wheels[wheelIndex].wearState = {};
    clearError();
    return true;
}

bool VehicleSystem::loadWheelTirePropertyFile(
    VehicleHandle handle,
    std::size_t wheelIndex,
    const std::filesystem::path& path,
    const std::string& provenance,
    VehicleScalar confidence)
{
    Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.LoadWheelTirePropertyFile received an invalid vehicle handle or wheel index.");
        return false;
    }

    const tires::TirePropertyFileLoadResult loaded = tires::loadTirePropertyFile(path);
    if (!loaded.success)
    {
        setError("Vehicle.LoadWheelTirePropertyFile: " + loaded.errorMessage);
        return false;
    }

    const TireModelDescription value = tireModelDescriptionFromPropertyFile(
        loaded.data,
        slot->record.wheels[wheelIndex].tireModel.provider,
        path.string(),
        provenance,
        confidence,
        slot->record.wheels[wheelIndex].tireModel);
    if (!validTireModelDescription(value))
    {
        setError("Vehicle.LoadWheelTirePropertyFile mapped the .tir file to an invalid tire description.");
        return false;
    }

    slot->record.wheels[wheelIndex].tireModel = value;
    slot->record.wheels[wheelIndex].tirePartAssignment = {};
    slot->record.wheels[wheelIndex].thermalState = {};
    slot->record.wheels[wheelIndex].failureState = {};
    slot->record.wheels[wheelIndex].wearState = {};
    clearError();
    return true;
}

bool VehicleSystem::assignWheelTirePart(
    VehicleHandle handle,
    std::size_t wheelIndex,
    const tires::TirePartDefinition& definition,
    const std::filesystem::path& propertyRoot,
    const tires::TirePartFitment& fitment)
{
    Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.AssignWheelTirePart received an invalid vehicle handle or wheel index.");
        return false;
    }

    const tires::TirePartResolutionResult resolvedPart = tires::resolveTirePart(
        definition, propertyRoot, fitment);
    if (!resolvedPart.valid)
    {
        setError("Vehicle.AssignWheelTirePart: " + resolvedPart.errorMessage);
        return false;
    }

    WheelRecord& wheel = slot->record.wheels[wheelIndex];
    wheel.tireModel = resolvedPart.model;
    wheel.thermalState = {};
    wheel.failureState = {};
    wheel.wearState = {};
    wheel.tirePartAssignment.assigned = true;
    wheel.tirePartAssignment.partId = definition.id;
    wheel.tirePartAssignment.displayName = definition.displayName;
    wheel.tirePartAssignment.family = definition.family;
    wheel.tirePartAssignment.source = resolvedPart.source;
    wheel.tirePartAssignment.coldInflationPressurePa = resolvedPart.model.inflationPressurePa;
    clearError();
    return true;
}

bool VehicleSystem::setWheelTireColdInflationPressure(
    VehicleHandle handle,
    std::size_t wheelIndex,
    VehicleScalar pressurePa)
{
    Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.SetWheelTireColdInflationPressure received an invalid vehicle handle or wheel index.");
        return false;
    }

    WheelRecord& wheel = slot->record.wheels[wheelIndex];
    if (!pressureInsideLiveRange(pressurePa))
    {
        const auto [minimumPa, maximumPa] = livePressureRange();
        setError("Vehicle.SetWheelTireColdInflationPressure pressure is outside the live test range ("
            + std::to_string(static_cast<double>(minimumPa)) + ".."
            + std::to_string(static_cast<double>(maximumPa)) + " Pa).");
        return false;
    }

    wheel.tireModel.inflationPressurePa = pressurePa;
    if (wheel.tireModel.thermal.enabled)
    {
        wheel.tireModel.thermal.referenceGaugePressurePa = pressurePa;
        wheel.tireModel.thermal.minimumGaugePressurePa = std::min(
            wheel.tireModel.thermal.minimumGaugePressurePa, pressurePa);
        wheel.tireModel.thermal.maximumGaugePressurePa = std::max(
            wheel.tireModel.thermal.maximumGaugePressurePa,
            std::min(VehicleScalar{2000000.0},
                pressurePa * VehicleScalar{1.50} + VehicleScalar{10000.0}));
    }
    wheel.thermalState = {};
    // A pressure command represents refilling the fitted tire, not repairing
    // its puncture/carcass. Preserve the failure while resetting gas inventory.
    wheel.failureState.containedGasMassRatio = 1.0;
    wheel.failureState.pressurizedGasFraction = 1.0;
    if (wheel.tirePartAssignment.assigned)
        wheel.tirePartAssignment.coldInflationPressurePa = pressurePa;
    clearError();
    return true;
}

bool VehicleSystem::setTireColdInflationPressure(
    VehicleHandle handle,
    VehicleScalar pressurePa)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetTireColdInflationPressure received an invalid or stale vehicle handle.");
        return false;
    }
    if (slot->record.wheels.empty())
    {
        setError("Vehicle.SetTireColdInflationPressure requires at least one wheel.");
        return false;
    }
    if (!pressureInsideLiveRange(pressurePa))
    {
        setError("Vehicle.SetTireColdInflationPressure is outside the 0..150 PSI live test range.");
        return false;
    }
    for (WheelRecord& wheel : slot->record.wheels)
    {
        wheel.tireModel.inflationPressurePa = pressurePa;
        if (wheel.tireModel.thermal.enabled)
        {
            wheel.tireModel.thermal.referenceGaugePressurePa = pressurePa;
            wheel.tireModel.thermal.minimumGaugePressurePa = std::min(
                wheel.tireModel.thermal.minimumGaugePressurePa, pressurePa);
            wheel.tireModel.thermal.maximumGaugePressurePa = std::max(
                wheel.tireModel.thermal.maximumGaugePressurePa,
                std::min(VehicleScalar{2000000.0},
                    pressurePa * VehicleScalar{1.50} + VehicleScalar{10000.0}));
        }
        wheel.thermalState = {};
        wheel.failureState.containedGasMassRatio = 1.0;
        wheel.failureState.pressurizedGasFraction = 1.0;
        if (wheel.tirePartAssignment.assigned)
            wheel.tirePartAssignment.coldInflationPressurePa = pressurePa;
    }
    clearError();
    return true;
}

bool VehicleSystem::triggerWheelTireFailure(
    VehicleHandle handle,
    std::size_t wheelIndex,
    tires::TireFailureStage stage)
{
    Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.TriggerWheelTireFailure received an invalid vehicle handle or wheel index.");
        return false;
    }

    WheelRecord& wheel = slot->record.wheels[wheelIndex];
    if (!wheel.tireModel.failure.enabled || !wheel.tireModel.thermal.enabled)
    {
        setError("Vehicle.TriggerWheelTireFailure requires the fitted tire's thermal and failure providers.");
        return false;
    }
    const tires::TireThermalOutput thermal = tires::evaluateTireThermalState(
        wheel.tireModel.thermal, wheel.thermalState);
    tires::TireFailureInput input;
    input.ambientPressurePa = wheel.tireModel.thermal.ambientPressurePa;
    input.referenceGaugePressurePa = wheel.tireModel.thermal.referenceGaugePressurePa;
    input.referenceTemperatureC = wheel.tireModel.thermal.referenceTemperatureC;
    input.gasTemperatureC = thermal.valid
        ? thermal.gasTemperatureC : wheel.tireModel.thermal.initialGasTemperatureC;
    input.carcassTemperatureC = thermal.valid
        ? thermal.carcassTemperatureC : wheel.tireModel.thermal.initialCarcassTemperatureC;
    input.inflationGaugePressurePa = thermal.valid
        ? thermal.inflationPressurePa : wheel.tireModel.inflationPressurePa;
    input.identifiedReferencePressurePa = wheel.tireModel.referenceInflationPressurePa;
    input.nominalLoadN = wheel.tireModel.nominalLoad;
    input.normalLoadN = wheel.state.normalForce;
    input.grounded = wheel.state.grounded;
    tires::triggerTireFailure(
        wheel.tireModel.failure, input, stage, wheel.failureState);
    wheel.thermalState.containedGasMassRatio =
        wheel.failureState.containedGasMassRatio;
    clearError();
    return true;
}

bool VehicleSystem::triggerTireFailure(
    VehicleHandle handle,
    tires::TireFailureStage stage)
{
    Slot* slot = resolve(handle);
    if (!slot || slot->record.wheels.empty())
    {
        setError("Vehicle.TriggerTireFailure requires a valid vehicle with wheels.");
        return false;
    }
    for (std::size_t wheelIndex = 0;
        wheelIndex < slot->record.wheels.size(); ++wheelIndex)
    {
        if (!triggerWheelTireFailure(handle, wheelIndex, stage))
            return false;
    }
    clearError();
    return true;
}

bool VehicleSystem::tireColdInflationPressureRange(
    VehicleHandle handle,
    VehicleScalar& minimumPa,
    VehicleScalar& maximumPa,
    VehicleScalar& representativePressurePa) const
{
    const Slot* slot = resolve(handle);
    if (!slot || slot->record.wheels.empty())
    {
        setError("Vehicle.GetTireColdInflationPressureRange requires a valid vehicle with wheels.");
        return false;
    }

    const auto [liveMinimumPa, liveMaximumPa] = livePressureRange();
    minimumPa = liveMinimumPa;
    maximumPa = liveMaximumPa;
    representativePressurePa = VehicleScalar{0.0};
    for (const WheelRecord& wheel : slot->record.wheels)
    {
        representativePressurePa += wheel.tireModel.inflationPressurePa;
    }
    representativePressurePa /= static_cast<VehicleScalar>(slot->record.wheels.size());
    if (maximumPa < minimumPa)
    {
        setError("Vehicle.GetTireColdInflationPressureRange found no common pressure range across fitted tires.");
        return false;
    }
    clearError();
    return true;
}

bool VehicleSystem::wheelTirePartAssignment(
    VehicleHandle handle,
    std::size_t wheelIndex,
    tires::TirePartAssignmentInfo& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.GetWheelTirePartAssignment received an invalid vehicle handle or wheel index.");
        return false;
    }
    value = slot->record.wheels[wheelIndex].tirePartAssignment;
    clearError();
    return true;
}

bool VehicleSystem::wheelTireModel(
    VehicleHandle handle,
    std::size_t wheelIndex,
    TireModelDescription& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.GetWheelTireModel received an invalid vehicle handle or wheel index.");
        return false;
    }
    value = slot->record.wheels[wheelIndex].tireModel;
    clearError();
    return true;
}

bool VehicleSystem::resetTirePhysicalState(VehicleHandle handle)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.ResetTirePhysicalState received an invalid or stale vehicle handle.");
        return false;
    }
    for (WheelRecord& wheel : slot->record.wheels)
    {
        wheel.thermalState = {};
        wheel.failureState = {};
        wheel.wearState = {};
    }
    clearError();
    return true;
}

bool VehicleSystem::setTireContactFidelity(
    VehicleHandle handle,
    TireContactFidelity fidelity)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetTireContactFidelity received an invalid or stale vehicle handle.");
        return false;
    }
    if (fidelity != TireContactFidelity::Aggregate
        && fidelity != TireContactFidelity::Distributed3x3)
    {
        setError("Vehicle.SetTireContactFidelity received an unknown fidelity tier.");
        return false;
    }
    slot->record.tireContactFidelity = fidelity;
    // Force the next road-envelope refresh to populate the complete spatial
    // cache when the distributed tier is selected.
    for (WheelRecord& wheel : slot->record.wheels)
    {
        wheel.roadEnvelopeInitialized = false;
        wheel.cachedDistributedContactValid = false;
    }
    clearError();
    return true;
}

TireContactFidelity VehicleSystem::tireContactFidelity(
    VehicleHandle handle) const
{
    const Slot* slot = resolve(handle);
    return slot ? slot->record.tireContactFidelity
        : TireContactFidelity::Aggregate;
}

bool VehicleSystem::setSurfacePreset(
    VehicleHandle handle,
    TireSurface surface)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetSurfacePreset received an invalid or stale vehicle handle.");
        return false;
    }
    if (!validSurface(surface))
    {
        setError("Vehicle.SetSurfacePreset received an unknown surface preset.");
        return false;
    }

    slot->record.surface = surface;
    clearError();
    return true;
}

TireSurface VehicleSystem::surfacePreset(VehicleHandle handle) const
{
    const Slot* slot = resolve(handle);
    return slot ? slot->record.surface : TireSurface::DryAsphalt;
}

} // namespace heritage::vehicles

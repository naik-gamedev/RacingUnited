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

std::pair<VehicleScalar, VehicleScalar> operationalPressureRange(
    const TireModelDescription& model)
{
    VehicleScalar minimumPa = VehicleScalar{20000.0};
    VehicleScalar maximumPa = VehicleScalar{2000000.0};
    if (model.magicFormula.minimumPressurePa > VehicleScalar{0.0}
        && model.magicFormula.maximumPressurePa >= model.magicFormula.minimumPressurePa)
    {
        minimumPa = std::max(minimumPa, model.magicFormula.minimumPressurePa);
        maximumPa = std::min(maximumPa, model.magicFormula.maximumPressurePa);
    }
    if (model.thermal.enabled)
    {
        minimumPa = std::max(minimumPa, model.thermal.minimumGaugePressurePa);
        maximumPa = std::min(maximumPa, model.thermal.maximumGaugePressurePa);
    }
    if (maximumPa < minimumPa)
        maximumPa = minimumPa;
    return { minimumPa, maximumPa };
}

bool pressureInsideRange(const TireModelDescription& model, VehicleScalar pressurePa)
{
    if (!std::isfinite(static_cast<double>(pressurePa)))
        return false;
    const auto [minimumPa, maximumPa] = operationalPressureRange(model);
    return pressurePa >= minimumPa && pressurePa <= maximumPa;
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

    slot->record.tireModel = value;
    slot->record.description.tireFriction = static_cast<float>(value.peakFriction);
    // Preserve the legacy/global meaning of SetTireModel: it updates the
    // default profile and every wheel already attached to the vehicle.
    for (WheelRecord& wheel : slot->record.wheels)
    {
        wheel.tireModel = value;
        wheel.tirePartAssignment = {};
        wheel.thermalState = {};
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

    slot->record.wheels[wheelIndex].tireModel = value;
    slot->record.wheels[wheelIndex].tirePartAssignment = {};
    slot->record.wheels[wheelIndex].thermalState = {};
    slot->record.wheels[wheelIndex].wearState = {};
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
    if (!pressureInsideRange(wheel.tireModel, pressurePa))
    {
        const auto [minimumPa, maximumPa] = operationalPressureRange(wheel.tireModel);
        setError("Vehicle.SetWheelTireColdInflationPressure pressure is outside this tire's supported range ("
            + std::to_string(static_cast<double>(minimumPa)) + ".."
            + std::to_string(static_cast<double>(maximumPa)) + " Pa).");
        return false;
    }

    wheel.tireModel.inflationPressurePa = pressurePa;
    if (wheel.tireModel.thermal.enabled)
        wheel.tireModel.thermal.referenceGaugePressurePa = pressurePa;
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
    for (const WheelRecord& wheel : slot->record.wheels)
    {
        if (!pressureInsideRange(wheel.tireModel, pressurePa))
        {
            setError("Vehicle.SetTireColdInflationPressure is outside at least one fitted tire's supported range.");
            return false;
        }
    }
    for (WheelRecord& wheel : slot->record.wheels)
    {
        wheel.tireModel.inflationPressurePa = pressurePa;
        if (wheel.tireModel.thermal.enabled)
            wheel.tireModel.thermal.referenceGaugePressurePa = pressurePa;
        if (wheel.tirePartAssignment.assigned)
            wheel.tirePartAssignment.coldInflationPressurePa = pressurePa;
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

    minimumPa = VehicleScalar{0.0};
    maximumPa = VehicleScalar{2000000.0};
    representativePressurePa = VehicleScalar{0.0};
    for (const WheelRecord& wheel : slot->record.wheels)
    {
        const auto [wheelMinimumPa, wheelMaximumPa] = operationalPressureRange(wheel.tireModel);
        minimumPa = std::max(minimumPa, wheelMinimumPa);
        maximumPa = std::min(maximumPa, wheelMaximumPa);
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
        wheel.wearState = {};
    }
    clearError();
    return true;
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

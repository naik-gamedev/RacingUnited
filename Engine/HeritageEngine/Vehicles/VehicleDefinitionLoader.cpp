#include "VehicleDefinitionLoader.hpp"

namespace heritage::vehicles {

VehicleHandle VehicleDefinitionLoader::create(
    const CompiledVehicleDefinition& definition,
    const VehicleDefinitionLoadSettings& settings,
    const heritage::physics::RigidBodySystem& bodies,
    VehicleSystem& vehicles,
    std::string& errorMessage)
{
    errorMessage.clear();
    if (definition.runtimeProvider != "raycast_wheel_v1")
    {
        errorMessage = "Compiled definition has no supported runtime provider.";
        return InvalidVehicle;
    }
    if (definition.powerUnits.size() != 1
        || definition.transmissions.size() != 1
        || definition.contactUnits.size() != 4)
    {
        errorMessage = "raycast_wheel_v1 received an incompatible compiled topology.";
        return InvalidVehicle;
    }

    const VehicleHandle handle = vehicles.create(settings.vehicle, bodies);
    if (handle == InvalidVehicle)
    {
        errorMessage = vehicles.lastError();
        return InvalidVehicle;
    }

    const auto rollback = [&]() -> VehicleHandle {
        vehicles.destroy(handle);
        return InvalidVehicle;
    };

    const VehiclePowerUnitDefinition& power =
        definition.powerUnits.front().authored;
    const VehicleTransmissionDefinition& transmission =
        definition.transmissions.front().authored;
    if (!vehicles.setPowertrain(
            handle,
            power.idleRpm,
            power.redlineRpm,
            power.maximumTorqueNm,
            power.engineBrakingTorqueNm,
            transmission.finalDriveRatio,
            transmission.efficiency,
            transmission.shiftDurationSeconds,
            transmission.clutchEngagementRate))
    {
        errorMessage = vehicles.lastError();
        return rollback();
    }
    if (!vehicles.setGearRatios(
            handle,
            transmission.reverseRatio,
            transmission.forwardRatios))
    {
        errorMessage = vehicles.lastError();
        return rollback();
    }

    for (const CompiledVehicleContactUnit& contact : definition.contactUnits)
    {
        if (contact.suspensionIndex >= definition.suspensions.size())
        {
            errorMessage = "Compiled contact has no resolved suspension component.";
            return rollback();
        }
        const VehicleSuspensionDefinition& suspension =
            definition.suspensions[contact.suspensionIndex].authored;
        SuspensionProviderKind suspensionProvider{};
        if (!parseSuspensionProvider(suspension.provider, suspensionProvider))
        {
            errorMessage = "Compiled suspension provider is not available.";
            return rollback();
        }

        WheelDescription wheel;
        wheel.localMount = contact.authored.localMount;
        wheel.localSuspensionDirection = contact.authored.suspensionDirection;
        wheel.radius = contact.authored.radiusM;
        wheel.restLength = suspension.restLengthM;
        wheel.maximumCompression = suspension.maximumCompressionM;
        wheel.maximumDroop = suspension.maximumDroopM;
        wheel.springPreload = suspension.springPreloadN;
        wheel.springRate = suspension.springRateNPerM;
        wheel.springProgression = suspension.springProgressionNPerM2;
        wheel.bumpDamping = suspension.bumpDampingNsPerM;
        wheel.bumpHighSpeedDamping =
            suspension.bumpHighSpeedDampingNsPerM;
        wheel.bumpDampingKneeVelocity =
            suspension.bumpDampingKneeVelocityMps;
        wheel.reboundDamping = suspension.reboundDampingNsPerM;
        wheel.reboundHighSpeedDamping =
            suspension.reboundHighSpeedDampingNsPerM;
        wheel.reboundDampingKneeVelocity =
            suspension.reboundDampingKneeVelocityMps;
        wheel.bumpStopEngagement = suspension.bumpStopEngagementM;
        wheel.bumpStopRate = suspension.bumpStopRateNPerM;
        wheel.bumpStopProgression = suspension.bumpStopProgressionNPerM2;
        wheel.droopStopEngagement = suspension.droopStopEngagementM;
        wheel.droopStopRate = suspension.droopStopRateNPerM;
        wheel.suspensionProvider = suspensionProvider;
        wheel.suspensionMotionRatio = suspension.motionRatio;
        wheel.maximumSuspensionForce = suspension.maximumForceN;
        wheel.effectiveUnsprungMass =
            contact.authored.effectiveUnsprungMassKg;
        wheel.tireRadialStiffness =
            contact.authored.tireRadialStiffnessNPerM;
        wheel.tireRadialDamping =
            contact.authored.tireRadialDampingNsPerM;
        wheel.maximumTireDeflection =
            contact.authored.maximumTireDeflectionM;
        wheel.maximumTireNormalForce =
            contact.authored.maximumTireNormalForceN;
        wheel.driveFactor = contact.driveFactor;
        wheel.steerFactor = contact.authored.steering ? 1.0f : 0.0f;
        wheel.brakeFactor = contact.authored.serviceBrake
            ? contact.authored.serviceBrakeFactor : 0.0f;
        wheel.handbrakeFactor = contact.authored.parkingBrake
            ? contact.authored.parkingBrakeFactor : 0.0f;
        if (!vehicles.addWheel(handle, wheel))
        {
            errorMessage = vehicles.lastError();
            return rollback();
        }
    }

    errorMessage = "Compiled definition loaded with raycast_wheel_v1.";
    return handle;
}

} // namespace heritage::vehicles

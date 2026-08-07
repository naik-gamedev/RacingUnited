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
        WheelDescription wheel;
        wheel.localMount = contact.authored.localMount;
        wheel.localSuspensionDirection = contact.authored.suspensionDirection;
        wheel.radius = contact.authored.radiusM;
        wheel.restLength = contact.authored.restLengthM;
        wheel.maximumCompression = contact.authored.maximumCompressionM;
        wheel.maximumDroop = contact.authored.maximumDroopM;
        wheel.springRate = contact.authored.springRateNPerM;
        wheel.bumpDamping = contact.authored.bumpDampingNsPerM;
        wheel.reboundDamping = contact.authored.reboundDampingNsPerM;
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

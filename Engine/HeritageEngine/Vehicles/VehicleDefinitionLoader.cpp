#include "VehicleDefinitionLoader.hpp"

#include <string_view>

namespace heritage::vehicles {
namespace {

bool copySuspensionHardpoint(
    const VehicleSuspensionDefinition& suspension,
    std::string_view id,
    heritage::math::Vec3& destination)
{
    for (const VehicleSuspensionHardpointDefinition& hardpoint
         : suspension.hardpoints)
    {
        if (hardpoint.id == id)
        {
            destination = hardpoint.localPosition;
            return true;
        }
    }
    return false;
}

bool buildMacPhersonHardpoints(
    const VehicleSuspensionDefinition& suspension,
    MacPhersonHardpoints& hardpoints)
{
    hardpoints = {};
    const bool complete =
        copySuspensionHardpoint(
            suspension, "strut_top_mount", hardpoints.strutTopMount)
        && copySuspensionHardpoint(
            suspension, "strut_upright_mount", hardpoints.strutUprightMount)
        && copySuspensionHardpoint(
            suspension, "lower_arm_inner_front",
            hardpoints.lowerArmInnerFront)
        && copySuspensionHardpoint(
            suspension, "lower_arm_inner_rear",
            hardpoints.lowerArmInnerRear)
        && copySuspensionHardpoint(
            suspension, "lower_ball_joint", hardpoints.lowerBallJoint)
        && copySuspensionHardpoint(
            suspension, "tie_rod_inner", hardpoints.tieRodInner)
        && copySuspensionHardpoint(
            suspension, "tie_rod_outer", hardpoints.tieRodOuter)
        && copySuspensionHardpoint(
            suspension, "wheel_center", hardpoints.wheelCenter);
    hardpoints.authored = complete;
    return complete && validMacPhersonHardpoints(hardpoints);
}

bool safeModuleRelativePath(const std::filesystem::path& path)
{
    if (path.empty() || path.is_absolute() || path.has_root_name() || path.has_root_directory())
        return false;
    for (const auto& component : path)
    {
        if (component == "..")
            return false;
    }
    return true;
}

bool parseTireProvider(
    std::string_view provider,
    TireProviderKind& value)
{
    if (provider == "advanced_road" || provider == "mf62_road")
    {
        value = TireProviderKind::MagicFormula62;
        return true;
    }
    if (provider == "motorcycle_profile" || provider == "mf62_motorcycle")
    {
        value = TireProviderKind::MagicFormula62Motorcycle;
        return true;
    }
    if (provider == "legacy_generalized_road")
    {
        value = TireProviderKind::LegacyGeneralizedRoad;
        return true;
    }
    return false;
}

bool buildTrailingArmHardpoints(
    const VehicleSuspensionDefinition& suspension,
    TrailingArmHardpoints& hardpoints)
{
    hardpoints = {};
    const bool complete =
        copySuspensionHardpoint(
            suspension, "arm_pivot_inner", hardpoints.armPivotInner)
        && copySuspensionHardpoint(
            suspension, "arm_pivot_outer", hardpoints.armPivotOuter)
        && copySuspensionHardpoint(
            suspension, "wheel_center", hardpoints.wheelCenter)
        && copySuspensionHardpoint(
            suspension, "damper_upper_mount", hardpoints.damperUpperMount)
        && copySuspensionHardpoint(
            suspension, "damper_lower_mount", hardpoints.damperLowerMount);
    hardpoints.authored = complete;
    return complete && validTrailingArmHardpoints(hardpoints);
}

} // namespace

VehicleHandle VehicleDefinitionLoader::create(
    const CompiledVehicleDefinition& definition,
    const VehicleDefinitionLoadSettings& settings,
    heritage::physics::RigidBodySystem& bodies,
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

    const VehicleBodyDefinition* primaryBody = nullptr;
    for (const VehicleBodyDefinition& body : definition.bodies)
    {
        if (body.role == "primary")
        {
            primaryBody = &body;
            break;
        }
    }
    if (!primaryBody)
    {
        errorMessage = "Compiled definition has no primary body.";
        return InvalidVehicle;
    }
    if (!bodies.setMass(settings.vehicle.chassisBody, primaryBody->massKg))
    {
        errorMessage = bodies.lastError();
        return InvalidVehicle;
    }
    const heritage::math::Vec3 desiredCenterOfMass =
        primaryBody->hasCenterOfMassLocal
        ? primaryBody->centerOfMassLocal
        : heritage::math::Vec3{};
    if (!bodies.setCenterOfMassLocal(
            settings.vehicle.chassisBody, desiredCenterOfMass))
    {
        errorMessage = bodies.lastError();
        return InvalidVehicle;
    }
    if (primaryBody->hasInertiaLocalKgM2)
    {
        if (!bodies.setInertiaLocal(
                settings.vehicle.chassisBody, primaryBody->inertiaLocalKgM2))
        {
            errorMessage = bodies.lastError();
            return InvalidVehicle;
        }
    }
    else if (!bodies.clearInertiaLocalOverride(settings.vehicle.chassisBody))
    {
        // A compiled definition is authoritative. Loading one without explicit
        // inertia must not inherit an override from a previously loaded vehicle
        // that happened to reuse the same rigid body. Collider-derived inertia
        // will be rebuilt on the next physics step.
        errorMessage = bodies.lastError();
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

    for (std::size_t contactIndex = 0;
         contactIndex < definition.contactUnits.size();
         ++contactIndex)
    {
        const CompiledVehicleContactUnit& contact =
            definition.contactUnits[contactIndex];
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
        wheel.localSteeringAxis = suspension.localSteeringAxis;
        wheel.staticCamberDegrees = suspension.staticCamberDegrees;
        wheel.camberGainDegreesPerM = suspension.camberGainDegreesPerM;
        wheel.camberProgressionDegreesPerM2 =
            suspension.camberProgressionDegreesPerM2;
        wheel.staticToeDegrees = suspension.staticToeDegrees;
        wheel.toeGainDegreesPerM = suspension.toeGainDegreesPerM;
        wheel.toeProgressionDegreesPerM2 =
            suspension.toeProgressionDegreesPerM2;
        wheel.suspensionProvider = suspensionProvider;
        if (suspensionProvider == SuspensionProviderKind::MacPhersonStrutV1
            && !buildMacPhersonHardpoints(
                suspension, wheel.macPhersonHardpoints))
        {
            errorMessage = "MacPherson suspension '" + suspension.id
                + "' is missing valid required hardpoints.";
            return rollback();
        }
        if (suspensionProvider
                == SuspensionProviderKind::TrailingArmTorsionBarV1
            && !buildTrailingArmHardpoints(
                suspension, wheel.trailingArmHardpoints))
        {
            errorMessage = "Trailing-arm torsion-bar suspension '"
                + suspension.id + "' is missing valid required hardpoints.";
            return rollback();
        }
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

        TireProviderKind tireProvider{};
        if (!parseTireProvider(contact.authored.tireProvider, tireProvider)
            || !vehicles.setWheelTireProvider(
                handle, contactIndex, tireProvider))
        {
            errorMessage = vehicles.lastError().empty()
                ? "Compiled tire provider is not available."
                : vehicles.lastError();
            return rollback();
        }

        if (!contact.authored.tireParameterFile.empty())
        {
            const std::filesystem::path relative = contact.authored.tireParameterFile;
            if (settings.moduleRoot.empty() || !safeModuleRelativePath(relative))
            {
                errorMessage = "Tire property path must be a safe module-relative path: "
                    + contact.authored.tireParameterFile;
                return rollback();
            }
            const std::filesystem::path resolved =
                (settings.moduleRoot / relative).lexically_normal();
            if (!vehicles.loadWheelTirePropertyFile(
                    handle,
                    contactIndex,
                    resolved,
                    contact.authored.tireParameterProvenance,
                    contact.authored.tireParameterConfidence))
            {
                errorMessage = vehicles.lastError();
                return rollback();
            }
        }
    }

    for (std::size_t index = 0; index < definition.antiRollBars.size(); ++index)
    {
        const CompiledVehicleAntiRollBar& source = definition.antiRollBars[index];
        SuspensionAntiRollBarDescription bar;
        bar.leftWheelIndex = source.leftContactUnitIndex;
        bar.rightWheelIndex = source.rightContactUnitIndex;
        bar.enabled = source.authored.enabled;
        bar.torsionalStiffnessNmPerRad = source.authored.torsionalStiffnessNmPerRad;
        bar.torsionalDampingNmsPerRad = source.authored.torsionalDampingNmsPerRad;
        bar.leftLeverArmM = source.authored.leftLeverArmM;
        bar.rightLeverArmM = source.authored.rightLeverArmM;
        bar.leftLinkMotionRatio = source.authored.leftLinkMotionRatio;
        bar.rightLinkMotionRatio = source.authored.rightLinkMotionRatio;
        bar.maximumWheelForceN = source.authored.maximumWheelForceN;
        if (!vehicles.setAntiRollBar(handle, index, bar))
        {
            errorMessage = vehicles.lastError();
            return rollback();
        }
    }

    if (definition.chassisFlex.authored.enabled)
    {
        const VehicleChassisFlexDefinition& source =
            definition.chassisFlex.authored;
        ChassisTorsionalComplianceDescription flex;
        flex.enabled = true;
        flex.torsionalRigidityNmPerDegree =
            source.torsionalRigidityNmPerDegree;
        flex.torsionalDampingNmsPerRad = source.torsionalDampingNmsPerRad;
        flex.effectiveTorsionalInertiaKgM2 =
            source.effectiveTorsionalInertiaKgM2;
        flex.torsionAxisLocalY = source.torsionAxisLocalY;
        flex.frontReferenceLocalZ = source.frontReferenceLocalZ;
        flex.rearReferenceLocalZ = source.rearReferenceLocalZ;
        flex.maximumTwistDegrees = source.maximumTwistDegrees;
        if (!vehicles.setChassisTorsionalCompliance(handle, flex))
        {
            errorMessage = vehicles.lastError();
            return rollback();
        }
    }

    errorMessage = "Compiled definition loaded with raycast_wheel_v1.";
    return handle;
}

} // namespace heritage::vehicles

#include "VehicleSystem.hpp"
#include "VehicleSystemInternal.hpp"
#include "Tires/TireSlipDynamics.hpp"
#include "Tires/TireContactPatch.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>

namespace heritage::vehicles {
using namespace vehicle_system_detail;

const char* wheelContactStatusName(WheelContactStatus value)
{
    switch (value)
    {
    case WheelContactStatus::Supported: return "supported";
    case WheelContactStatus::SuspensionBottomed:
        return "suspension_bottomed";
    case WheelContactStatus::RoadDetectedNoLoad:
        return "road_detected_no_load";
    case WheelContactStatus::SurfaceBehindRayOrigin:
        return "surface_behind_ray_origin";
    case WheelContactStatus::OutsideStaticSceneBounds:
        return "outside_static_scene_bounds";
    case WheelContactStatus::NoWorldGeometry:
        return "no_world_geometry";
    case WheelContactStatus::NoRayCandidates:
        return "no_ray_candidates";
    case WheelContactStatus::RayCandidatesMissed:
        return "ray_candidates_missed";
    case WheelContactStatus::BeyondSuspensionReach:
        return "beyond_suspension_reach";
    case WheelContactStatus::NoSupportHit:
        return "no_support_hit";
    }
    return "unknown";
}

void VehicleSystem::clear()
{
    m_slots.clear();
    m_freeIndices.clear();
    m_aliveCount = 0;
    m_lastError.clear();
}

void VehicleSystem::resetClock()
{
    for (Slot& slot : m_slots)
    {
        if (!slot.alive)
            continue;
        slot.record.highRateAccumulator = 0.0;
        slot.record.lastHighRateStepCount = 0;
        slot.record.totalHighRateStepCount = 0;
        slot.record.currentSteerCenterDegrees = 0.0f;
        slot.record.targetSteerCenterDegrees = 0.0f;
        slot.record.innerSteerAngleDegrees = 0.0f;
        slot.record.outerSteerAngleDegrees = 0.0f;
        slot.record.currentSteeringRateFactor = 1.0f;
        slot.record.currentGear = 1;
        slot.record.requestedGear = 1;
        slot.record.shifting = false;
        slot.record.shiftTimeRemaining = 0.0f;
        slot.record.engineRpm = slot.record.powertrain.idleRpm;
        slot.record.engineTorque = 0.0f;
        slot.record.clutchEngagement = 0.0f;
        slot.record.clutchSlipRpm = 0.0f;
        slot.record.wheelCoupledRpm = 0.0f;
        slot.record.selectedGearRatio = selectedGearRatio(
            slot.record.powertrain,
            slot.record.currentGear);
        slot.record.outputTorque = 0.0f;
        slot.record.drivenWheelSpeedDifferenceRpm = 0.0f;
        slot.record.antiLockActiveWheelCount = 0;
        slot.record.tractionControlActiveWheelCount = 0;
        slot.record.restTimer = 0.0f;
        slot.record.parkedResting = false;
        slot.record.parkedRestRequiresBrake = false;
        slot.record.parkedRestBrakeInput = 0.0f;
        slot.record.parkedRestHandbrakeInput = 0.0f;
        slot.record.restCandidate = false;
        slot.record.requiredHoldForce = 0.0f;
        slot.record.availableBrakeHoldForce = 0.0f;
        slot.record.dynamicsLab.clear();
        for (WheelRecord& wheel : slot.record.wheels)
        {
            wheel.state.wheelRotationDegrees = 0.0f;
            wheel.state.wheelAngularVelocity = 0.0f;
            wheel.state.appliedDriveTorque = 0.0f;
            wheel.state.appliedBrakeTorque = 0.0f;
            wheel.state.serviceBrakeTorque = 0.0f;
            wheel.state.handbrakeTorque = 0.0f;
            wheel.state.antiLockModulation = 1.0f;
            wheel.state.tractionControlModulation = 1.0f;
            wheel.state.antiLockActive = false;
            wheel.state.tractionControlActive = false;
            wheel.state.longitudinalSpeed = 0.0f;
            wheel.state.lateralSpeed = 0.0f;
            wheel.state.slipRatio = 0.0f;
            wheel.state.slipAngleDegrees = 0.0f;
            wheel.state.relaxedSlipRatio = 0.0f;
            wheel.state.relaxedSlipAngleDegrees = 0.0f;
            wheel.state.effectiveFriction = 0.0f;
            wheel.state.gripUtilization = 0.0f;
            wheel.state.pureLongitudinalForce = 0.0f;
            wheel.state.pureLateralForce = 0.0f;
            wheel.state.combinedSlipScale = 1.0f;
            wheel.state.pneumaticTrail = 0.0f;
            wheel.state.aligningTorque = 0.0f;
            wheel.state.overturningMoment = 0.0f;
            wheel.state.rollingResistanceMoment = 0.0f;
            wheel.state.residualAligningTorque = 0.0f;
            wheel.state.longitudinalSlipStiffness = 0.0f;
            wheel.state.corneringStiffness = 0.0f;
            wheel.state.camberStiffness = 0.0f;
            wheel.state.motorcycleContourValid = false;
            wheel.state.motorcycleContactLateralOffset = 0.0f;
            wheel.state.motorcycleCenterToRoad = 0.0f;
            wheel.previousSuspensionLength = wheel.description.restLength;
        }
    }
}

VehicleHandle VehicleSystem::create(
    const VehicleDescription& description,
    const heritage::physics::RigidBodySystem& bodies)
{
    if (!bodies.exists(description.chassisBody))
    {
        setError("Vehicle.Create requires a valid native chassis body.");
        return InvalidVehicle;
    }

    heritage::physics::BodyMotionType motionType{};
    if (!bodies.motionType(description.chassisBody, motionType)
        || motionType != heritage::physics::BodyMotionType::Dynamic)
    {
        setError("Vehicle.Create requires a dynamic chassis body.");
        return InvalidVehicle;
    }
    if (!validVehicleDescription(description))
    {
        setError("Vehicle tuning values are outside the supported range.");
        return InvalidVehicle;
    }

    for (const Slot& slot : m_slots)
    {
        if (slot.alive && slot.record.description.chassisBody == description.chassisBody)
        {
            setError("A chassis body can belong to only one native vehicle.");
            return InvalidVehicle;
        }
    }

    std::uint32_t index = 0;
    if (!m_freeIndices.empty())
    {
        index = m_freeIndices.back();
        m_freeIndices.pop_back();
    }
    else
    {
        if (m_slots.size() >= static_cast<std::size_t>(
                (std::numeric_limits<std::uint32_t>::max)() - 1u))
        {
            setError("Vehicle storage exhausted its handle index space.");
            return InvalidVehicle;
        }
        index = static_cast<std::uint32_t>(m_slots.size());
        m_slots.emplace_back();
    }

    Slot& slot = m_slots[index];
    slot.alive = true;
    slot.record = {};
    slot.record.description = description;
    slot.record.tireModel.peakFriction = description.tireFriction;
    slot.record.tireModel.corneringStiffness = std::max(
        1000.0f,
        description.lateralStiffness * 7.25f);
    slot.record.engineRpm = slot.record.powertrain.idleRpm;
    slot.record.selectedGearRatio = selectedGearRatio(
        slot.record.powertrain,
        slot.record.currentGear);
    ++m_aliveCount;
    clearError();
    return makeHandle(index, slot.generation);
}

bool VehicleSystem::destroy(VehicleHandle handle)
{
    std::uint32_t index = 0;
    std::uint32_t generation = 0;
    if (!decodeHandle(handle, index, generation) || index >= m_slots.size())
    {
        setError("Vehicle.Destroy received an invalid or stale vehicle handle.");
        return false;
    }
    Slot& slot = m_slots[index];
    if (!slot.alive || slot.generation != generation)
    {
        setError("Vehicle.Destroy received an invalid or stale vehicle handle.");
        return false;
    }
    return destroyResolved(index, slot);
}

bool VehicleSystem::exists(VehicleHandle handle) const
{
    return resolve(handle) != nullptr;
}

void VehicleSystem::destroyForBody(heritage::physics::BodyHandle body)
{
    for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(m_slots.size()); ++index)
    {
        Slot& slot = m_slots[index];
        if (slot.alive && slot.record.description.chassisBody == body)
            destroyResolved(index, slot);
    }
}

void VehicleSystem::removeInvalidBodies(
    const heritage::physics::RigidBodySystem& bodies)
{
    for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(m_slots.size()); ++index)
    {
        Slot& slot = m_slots[index];
        if (slot.alive && !bodies.exists(slot.record.description.chassisBody))
            destroyResolved(index, slot);
    }
}

bool VehicleSystem::addWheel(
    VehicleHandle handle,
    const WheelDescription& description)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.AddWheel received an invalid or stale vehicle handle.");
        return false;
    }
    if (!validWheelDescription(description))
    {
        setError("Vehicle.AddWheel received invalid geometry or tuning values.");
        return false;
    }
    if (slot->record.wheels.size() >= 32)
    {
        setError("A native vehicle currently supports at most 32 wheel/contact units.");
        return false;
    }

    WheelRecord wheel;
    wheel.description = description;
    wheel.description.localSuspensionDirection = normalized(
        description.localSuspensionDirection,
        { 0.0f, -1.0f, 0.0f });
    wheel.description.localSteeringAxis = normalized(
        description.localSteeringAxis,
        { 0.0f, 1.0f, 0.0f });
    wheel.state.suspensionLength = description.restLength;
    wheel.state.antiLockModulation = 1.0f;
    wheel.state.tractionControlModulation = 1.0f;
    // New wheels inherit the vehicle default tire profile. Step 29H can then
    // override any individual wheel without changing the others.
    wheel.tireModel = slot->record.tireModel;
    wheel.previousSuspensionLength = description.restLength;
    const std::uint64_t wheelOrdinal = static_cast<std::uint64_t>(
        slot->record.wheels.size() + 1u);
    wheel.tireMarkStreamId = handle
        ^ (0x9e3779b97f4a7c15ULL * wheelOrdinal);
    if (wheel.tireMarkStreamId == 0)
        wheel.tireMarkStreamId = wheelOrdinal;
    slot->record.wheels.push_back(wheel);
    clearError();
    return true;
}

std::size_t VehicleSystem::wheelCount(VehicleHandle handle) const
{
    const Slot* slot = resolve(handle);
    return slot ? slot->record.wheels.size() : 0;
}

bool VehicleSystem::wheelState(
    VehicleHandle handle,
    std::size_t wheelIndex,
    WheelState& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.GetWheelState received an invalid vehicle handle or wheel index.");
        return false;
    }
    value = slot->record.wheels[wheelIndex].state;
    clearError();
    return true;
}

bool VehicleSystem::measureWheelGeometry(
    VehicleHandle handle,
    std::size_t frontLeftWheelIndex,
    std::size_t frontRightWheelIndex,
    std::size_t rearLeftWheelIndex,
    std::size_t rearRightWheelIndex,
    VehicleWheelGeometryMeasurement& value) const
{
    value = {};
    const Slot* slot = resolve(handle);
    const std::size_t wheelCountValue = slot ? slot->record.wheels.size() : 0;
    const std::size_t indices[] = {
        frontLeftWheelIndex,
        frontRightWheelIndex,
        rearLeftWheelIndex,
        rearRightWheelIndex
    };
    if (!slot
        || frontLeftWheelIndex == frontRightWheelIndex
        || frontLeftWheelIndex == rearLeftWheelIndex
        || frontLeftWheelIndex == rearRightWheelIndex
        || frontRightWheelIndex == rearLeftWheelIndex
        || frontRightWheelIndex == rearRightWheelIndex
        || rearLeftWheelIndex == rearRightWheelIndex
        || std::any_of(std::begin(indices), std::end(indices),
            [wheelCountValue](std::size_t index) {
                return index >= wheelCountValue;
            }))
    {
        setError("Vehicle.MeasureWheelGeometry received an invalid vehicle or four-corner wheel mapping.");
        return false;
    }

    const WheelState& frontLeft = slot->record.wheels[frontLeftWheelIndex].state;
    const WheelState& frontRight = slot->record.wheels[frontRightWheelIndex].state;
    const WheelState& rearLeft = slot->record.wheels[rearLeftWheelIndex].state;
    const WheelState& rearRight = slot->record.wheels[rearRightWheelIndex].state;

    value.frontAxleCenterWorld = scale(
        add(frontLeft.worldCenter, frontRight.worldCenter), 0.5);
    value.rearAxleCenterWorld = scale(
        add(rearLeft.worldCenter, rearRight.worldCenter), 0.5);

    const heritage::math::Vec3 frontSpan = subtract(
        frontRight.worldCenter, frontLeft.worldCenter);
    const heritage::math::Vec3 rearSpan = subtract(
        rearRight.worldCenter, rearLeft.worldCenter);
    const heritage::math::Vec3 axleSpan = subtract(
        value.frontAxleCenterWorld, value.rearAxleCenterWorld);
    // Centre spans are unaffected by steering angle. They provide a much more
    // stable lateral measurement axis than averaging steered front uprights.
    // Symmetric camber cancels in the averaged up axis; their cross product is
    // the chassis-forward axis used for projected wheelbase.
    const heritage::math::Vec3 rightAxis = normalized(
        add(frontSpan, rearSpan), { 1.0f, 0.0f, 0.0f });
    const heritage::math::Vec3 upAxis = normalized(
        add(add(frontLeft.worldWheelUp, frontRight.worldWheelUp),
            add(rearLeft.worldWheelUp, rearRight.worldWheelUp)),
        { 0.0f, 1.0f, 0.0f });
    const heritage::math::Vec3 forwardAxis = normalized(
        cross(rightAxis, upAxis), { 0.0f, 0.0f, 1.0f });
    value.frontTrackM = std::abs(static_cast<VehicleScalar>(
        dot(frontSpan, rightAxis)));
    value.rearTrackM = std::abs(static_cast<VehicleScalar>(
        dot(rearSpan, rightAxis)));
    value.wheelbaseM = std::abs(static_cast<VehicleScalar>(
        dot(axleSpan, forwardAxis)));
    value.spatialFrontTrackM = static_cast<VehicleScalar>(length(frontSpan));
    value.spatialRearTrackM = static_cast<VehicleScalar>(length(rearSpan));
    value.spatialWheelbaseM = static_cast<VehicleScalar>(length(axleSpan));
    value.valid = finiteFloat(static_cast<float>(value.frontTrackM))
        && finiteFloat(static_cast<float>(value.rearTrackM))
        && finiteFloat(static_cast<float>(value.wheelbaseM))
        && value.frontTrackM > 0.0
        && value.rearTrackM > 0.0
        && value.wheelbaseM > 0.0;
    if (!value.valid)
    {
        setError("Vehicle.MeasureWheelGeometry could not reconstruct finite four-corner geometry.");
        return false;
    }
    clearError();
    return true;
}

bool VehicleSystem::wheelTireFlexibleRingField(
    VehicleHandle handle,
    std::size_t wheelIndex,
    tires::TireFlexibleRingFieldOutput& value)
{
    Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle tire-carcass field query received an invalid vehicle handle or wheel index.");
        return false;
    }
    auto& wheel = slot->record.wheels[wheelIndex];
    if (wheel.flexibleRingDemandSeconds <= VehicleScalar{0.0})
    {
        // Do not resurrect a stale shape after a long cull. The next physics
        // step starts the structural state from its undeformed rest condition.
        wheel.flexibleRingState = {};
        wheel.flexibleRingOutput = {};
        wheel.flexibleRingAccumulatorSeconds = 0.0;
    }
    wheel.flexibleRingDemandSeconds = VehicleScalar{0.25};
    value = wheel.flexibleRingOutput;
    clearError();
    return true;
}


namespace {

tires::TireCarcassSyntheticInput makeTireCarcassSyntheticInput(
    const WheelDescription& description,
    const WheelState& state,
    const TireModelDescription& tireModel,
    std::size_t integrationSteps)
{
    tires::TireCarcassSyntheticInput input;
    input.description.unloadedRadiusM =
        tireModel.contactGeometry.unloadedRadiusM > 0.05
            ? tireModel.contactGeometry.unloadedRadiusM
            : static_cast<VehicleScalar>(description.radius);
    input.description.rimRadiusM = tireModel.contactGeometry.rimRadiusM;
    if (input.description.rimRadiusM <= 0.01)
    {
        const VehicleScalar rimDiameterIn = description.fitment.tireRimDiameterIn > 1.0f
            ? static_cast<VehicleScalar>(description.fitment.tireRimDiameterIn)
            : static_cast<VehicleScalar>(description.fitment.rimDiameterIn);
        input.description.rimRadiusM = rimDiameterIn > 1.0
            ? rimDiameterIn * VehicleScalar{0.0127}
            : input.description.unloadedRadiusM * VehicleScalar{0.60};
    }
    input.description.rimRadiusM = std::clamp(
        input.description.rimRadiusM,
        VehicleScalar{0.01},
        input.description.unloadedRadiusM - VehicleScalar{0.005});
    input.description.sectionWidthM =
        tireModel.contactGeometry.nominalWidthM > 0.03
            ? tireModel.contactGeometry.nominalWidthM
            : description.fitment.tireWidthMm > 30.0f
                ? static_cast<VehicleScalar>(description.fitment.tireWidthMm) * VehicleScalar{0.001}
                : VehicleScalar{0.205};
    input.description.maximumDeflectionM = std::clamp(
        static_cast<VehicleScalar>(description.maximumTireDeflection),
        VehicleScalar{0.015},
        input.description.unloadedRadiusM - input.description.rimRadiusM);
    input.description.referencePressurePa =
        tireModel.referenceInflationPressurePa > 20000.0
            ? tireModel.referenceInflationPressurePa : VehicleScalar{220000.0};
    input.description.verticalStiffnessNPerM =
        tireModel.contactGeometry.verticalStiffnessNPerM > 1000.0
            ? tireModel.contactGeometry.verticalStiffnessNPerM
            : std::max(static_cast<VehicleScalar>(description.tireRadialStiffness),
                VehicleScalar{1000.0});
    input.referencePressurePa = input.description.referencePressurePa;
    input.normalLoadN = state.normalForce > 100.0
        ? state.normalForce
        : tireModel.nominalLoad > 100.0
            ? tireModel.nominalLoad : VehicleScalar{3200.0};
    // This is wheel-centre overlap for the isolated scenario, not a carcass
    // vertex target. The structural solver still discovers its own shape.
    input.roadOverlapM = std::clamp(
        input.normalLoadN / input.description.verticalStiffnessNPerM,
        VehicleScalar{0.004},
        std::min(input.description.maximumDeflectionM,
            VehicleScalar{0.045}));
    input.integrationSteps = std::clamp<std::size_t>(integrationSteps, 4, 240);
    return input;
}

} // namespace

bool VehicleSystem::setTireCarcassDevelopmentEnabled(
    VehicleHandle handle, std::size_t wheelIndex, bool enabled)
{
    Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle tire-carcass lab received an invalid vehicle handle or wheel index.");
        return false;
    }
    auto& wheel = slot->record.wheels[wheelIndex];
    wheel.flexibleRingDevelopmentTuning.enabled = enabled;
    wheel.flexibleRingState = {};
    wheel.flexibleRingOutput = {};
    wheel.flexibleRingAccumulatorSeconds = 0.0;
    wheel.flexibleRingDemandSeconds = VehicleScalar{0.50};
    clearError();
    return true;
}

bool VehicleSystem::tireCarcassDevelopmentEnabled(
    VehicleHandle handle, std::size_t wheelIndex, bool& enabled) const
{
    const Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle tire-carcass lab received an invalid vehicle handle or wheel index.");
        return false;
    }
    enabled = slot->record.wheels[wheelIndex].flexibleRingDevelopmentTuning.enabled;
    clearError();
    return true;
}

bool VehicleSystem::tireCarcassDevelopmentParameter(
    VehicleHandle handle, std::size_t wheelIndex,
    std::size_t parameterIndex, VehicleScalar& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size()
        || parameterIndex >= tires::tireCarcassDevelopmentParameterCount())
    {
        setError("Vehicle tire-carcass lab parameter query is invalid.");
        return false;
    }
    value = tires::tireCarcassDevelopmentParameterValue(
        slot->record.wheels[wheelIndex].flexibleRingDevelopmentTuning,
        parameterIndex);
    clearError();
    return true;
}

bool VehicleSystem::setTireCarcassDevelopmentParameter(
    VehicleHandle handle, std::size_t wheelIndex,
    std::size_t parameterIndex, VehicleScalar value)
{
    Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle tire-carcass lab parameter write is invalid.");
        return false;
    }
    auto& wheel = slot->record.wheels[wheelIndex];
    if (!tires::setTireCarcassDevelopmentParameterValue(
            wheel.flexibleRingDevelopmentTuning, parameterIndex, value))
    {
        setError("Vehicle tire-carcass lab parameter index is invalid.");
        return false;
    }
    wheel.flexibleRingDevelopmentTuning.enabled = true;
    wheel.flexibleRingState = {};
    wheel.flexibleRingOutput = {};
    wheel.flexibleRingAccumulatorSeconds = 0.0;
    wheel.flexibleRingDemandSeconds = VehicleScalar{0.50};
    clearError();
    return true;
}

bool VehicleSystem::resetTireCarcassDevelopmentTuning(
    VehicleHandle handle, std::size_t wheelIndex)
{
    Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle tire-carcass lab reset is invalid.");
        return false;
    }
    auto& wheel = slot->record.wheels[wheelIndex];
    tires::resetTireCarcassDevelopmentTuning(
        wheel.flexibleRingDevelopmentTuning, true);
    wheel.flexibleRingState = {};
    wheel.flexibleRingOutput = {};
    wheel.flexibleRingAccumulatorSeconds = 0.0;
    wheel.flexibleRingDemandSeconds = VehicleScalar{0.50};
    clearError();
    return true;
}

bool VehicleSystem::resetTireCarcassDevelopmentState(
    VehicleHandle handle, std::size_t wheelIndex)
{
    Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle tire-carcass dynamic-state reset is invalid.");
        return false;
    }
    auto& wheel = slot->record.wheels[wheelIndex];
    wheel.flexibleRingState = {};
    wheel.flexibleRingOutput = {};
    wheel.flexibleRingAccumulatorSeconds = 0.0;
    wheel.flexibleRingDemandSeconds = VehicleScalar{0.50};
    clearError();
    return true;
}

bool VehicleSystem::copyTireCarcassDevelopmentTuningToAllWheels(
    VehicleHandle handle, std::size_t sourceWheelIndex)
{
    Slot* slot = resolve(handle);
    if (!slot || sourceWheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle tire-carcass lab copy source is invalid.");
        return false;
    }
    const auto source =
        slot->record.wheels[sourceWheelIndex].flexibleRingDevelopmentTuning;
    for (auto& wheel : slot->record.wheels)
    {
        wheel.flexibleRingDevelopmentTuning = source;
        wheel.flexibleRingState = {};
        wheel.flexibleRingOutput = {};
        wheel.flexibleRingAccumulatorSeconds = 0.0;
        wheel.flexibleRingDemandSeconds = VehicleScalar{0.50};
    }
    clearError();
    return true;
}

bool VehicleSystem::runTireCarcassSyntheticScenario(
    VehicleHandle handle, std::size_t wheelIndex,
    tires::TireCarcassSyntheticScenario scenario,
    std::size_t integrationSteps,
    tires::TireCarcassSyntheticResult& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle tire-carcass synthetic scenario is invalid.");
        return false;
    }
    const auto& wheel = slot->record.wheels[wheelIndex];
    const auto input = makeTireCarcassSyntheticInput(
        wheel.description, wheel.state, wheel.tireModel, integrationSteps);
    value = tires::runTireCarcassSyntheticScenario(
        input, wheel.flexibleRingDevelopmentTuning, scenario);
    if (!value.valid)
    {
        setError("Vehicle tire-carcass synthetic scenario failed.");
        return false;
    }
    clearError();
    return true;
}

bool VehicleSystem::runTireCarcassSearchBatch(
    VehicleHandle handle, std::size_t wheelIndex,
    tires::TireCarcassSyntheticScenario scenario,
    std::uint64_t seed, std::uint64_t firstTrialIndex,
    std::size_t trialCount, VehicleScalar spread,
    std::uint32_t groupMask,
    std::size_t integrationSteps,
    tires::TireCarcassSearchBatchResult& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle tire-carcass search batch is invalid.");
        return false;
    }
    const auto& wheel = slot->record.wheels[wheelIndex];
    const auto input = makeTireCarcassSyntheticInput(
        wheel.description, wheel.state, wheel.tireModel, integrationSteps);
    value = tires::runTireCarcassSearchBatch(
        input, wheel.flexibleRingDevelopmentTuning, scenario,
        seed, firstTrialIndex, trialCount, spread, groupMask);
    if (!value.valid)
    {
        setError("Vehicle tire-carcass search batch produced no valid candidate.");
        return false;
    }
    clearError();
    return true;
}

bool VehicleSystem::applyTireCarcassSearchTrial(
    VehicleHandle handle, std::size_t wheelIndex,
    std::uint64_t seed, std::uint64_t trialIndex,
    VehicleScalar spread, std::uint32_t groupMask)
{
    Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle tire-carcass search apply is invalid.");
        return false;
    }
    auto& wheel = slot->record.wheels[wheelIndex];
    wheel.flexibleRingDevelopmentTuning = tires::tireCarcassDevelopmentTrialTuning(
        wheel.flexibleRingDevelopmentTuning,
        seed, trialIndex, spread, groupMask);
    wheel.flexibleRingDevelopmentTuning.enabled = true;
    wheel.flexibleRingState = {};
    wheel.flexibleRingOutput = {};
    wheel.flexibleRingAccumulatorSeconds = 0.0;
    wheel.flexibleRingDemandSeconds = VehicleScalar{0.50};
    clearError();
    return true;
}

bool VehicleSystem::wheelDescription(
    VehicleHandle handle,
    std::size_t wheelIndex,
    WheelDescription& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle wheel-description query received an invalid vehicle handle or wheel index.");
        return false;
    }
    value = slot->record.wheels[wheelIndex].description;
    clearError();
    return true;
}


float VehicleSystem::speed(VehicleHandle handle) const
{
    const Slot* slot = resolve(handle);
    return slot ? slot->record.speed : 0.0f;
}

std::size_t VehicleSystem::groundedWheelCount(VehicleHandle handle) const
{
    const Slot* slot = resolve(handle);
    return slot ? slot->record.groundedWheelCount : 0;
}

int VehicleSystem::lastHighRateStepCount(VehicleHandle handle) const
{
    const Slot* slot = resolve(handle);
    return slot ? slot->record.lastHighRateStepCount : 0;
}

std::uint64_t VehicleSystem::totalHighRateStepCount(VehicleHandle handle) const
{
    const Slot* slot = resolve(handle);
    return slot ? slot->record.totalHighRateStepCount : 0;
}

float VehicleSystem::highRateHertz(VehicleHandle handle) const
{
    const Slot* slot = resolve(handle);
    return slot ? slot->record.description.highRateHertz : 0.0f;
}

heritage::physics::BodyHandle VehicleSystem::chassisBody(VehicleHandle handle) const
{
    const Slot* slot = resolve(handle);
    return slot
        ? slot->record.description.chassisBody
        : heritage::physics::InvalidBody;
}

bool VehicleSystem::restState(
    VehicleHandle handle,
    VehicleRestState& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle rest-state query received an invalid or stale handle.");
        return false;
    }

    value.resting = slot->record.parkedResting;
    value.candidate = slot->record.restCandidate;
    value.requiresBrake = slot->record.parkedRestRequiresBrake;
    value.quietTimeSeconds = slot->record.restTimer;
    value.requiredHoldForce = slot->record.requiredHoldForce;
    value.availableBrakeHoldForce = slot->record.availableBrakeHoldForce;
    clearError();
    return true;
}


VehicleHandle VehicleSystem::makeHandle(
    std::uint32_t index,
    std::uint32_t generation)
{
    return (static_cast<VehicleHandle>(generation) << 32u)
        | static_cast<VehicleHandle>(index + 1u);
}

bool VehicleSystem::decodeHandle(
    VehicleHandle handle,
    std::uint32_t& index,
    std::uint32_t& generation)
{
    if (handle == InvalidVehicle)
        return false;
    const std::uint32_t encodedIndex = static_cast<std::uint32_t>(
        handle & 0xffffffffull);
    generation = static_cast<std::uint32_t>(handle >> 32u);
    if (encodedIndex == 0 || generation == 0)
        return false;
    index = encodedIndex - 1u;
    return true;
}

VehicleSystem::Slot* VehicleSystem::resolve(VehicleHandle handle)
{
    std::uint32_t index = 0;
    std::uint32_t generation = 0;
    if (!decodeHandle(handle, index, generation) || index >= m_slots.size())
        return nullptr;
    Slot& slot = m_slots[index];
    return slot.alive && slot.generation == generation ? &slot : nullptr;
}

const VehicleSystem::Slot* VehicleSystem::resolve(VehicleHandle handle) const
{
    std::uint32_t index = 0;
    std::uint32_t generation = 0;
    if (!decodeHandle(handle, index, generation) || index >= m_slots.size())
        return nullptr;
    const Slot& slot = m_slots[index];
    return slot.alive && slot.generation == generation ? &slot : nullptr;
}

bool VehicleSystem::destroyResolved(std::uint32_t index, Slot& slot)
{
    slot.alive = false;
    slot.record = {};
    ++slot.generation;
    if (slot.generation == 0)
        slot.generation = 1;
    m_freeIndices.push_back(index);
    if (m_aliveCount > 0)
        --m_aliveCount;
    clearError();
    return true;
}

void VehicleSystem::setError(const std::string& message) const
{
    m_lastError = message;
}

void VehicleSystem::clearError() const
{
    m_lastError.clear();
}


} // namespace heritage::vehicles

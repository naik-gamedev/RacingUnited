#include "VehicleSystem.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::vehicles {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kMinimumHighRateHertz = 120.0f;
constexpr float kMaximumHighRateHertz = 2000.0f;
constexpr int kMaximumHighRateStepsPerWorldStep = 32;
constexpr float kVectorEpsilon = 1.0e-6f;
constexpr float kMaximumSuspensionForce = 250000.0f;
constexpr float kVehicleRestDelaySeconds = 0.75f;
constexpr float kVehicleRestLinearSpeed = 0.04f;
constexpr float kVehicleRestAngularSpeedDegrees = 1.0f;
constexpr float kVehicleRestWheelSpeed = 0.15f;
constexpr float kVehicleRestFlatSlopeDegrees = 0.5f;
constexpr float kLowSpeedTireBlendStart = 0.50f;
constexpr float kLowSpeedTireBlendEnd = 2.00f;

bool finiteFloat(float value)
{
    return std::isfinite(static_cast<double>(value));
}

bool finiteVec3(const heritage::math::Vec3& value)
{
    return finiteFloat(value.x) && finiteFloat(value.y) && finiteFloat(value.z);
}

heritage::math::Vec3 add(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

heritage::math::Vec3 subtract(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

heritage::math::Vec3 scale(
    const heritage::math::Vec3& value,
    float scalar)
{
    return { value.x * scalar, value.y * scalar, value.z * scalar };
}

float dot(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

heritage::math::Vec3 cross(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float lengthSquared(const heritage::math::Vec3& value)
{
    return dot(value, value);
}

float length(const heritage::math::Vec3& value)
{
    return std::sqrt(lengthSquared(value));
}

float smoothStep01(float value)
{
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

heritage::math::Vec3 normalized(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& fallback)
{
    const float magnitude = length(value);
    return magnitude > kVectorEpsilon
        ? scale(value, 1.0f / magnitude)
        : fallback;
}

float radians(float degrees)
{
    return degrees * (kPi / 180.0f);
}

float degrees(float radiansValue)
{
    return radiansValue * (180.0f / kPi);
}

struct Quaternion
{
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

Quaternion quaternionFromEulerDegrees(const heritage::math::Vec3& value)
{
    const float halfX = radians(value.x) * 0.5f;
    const float halfY = radians(value.y) * 0.5f;
    const float halfZ = radians(value.z) * 0.5f;
    const float cx = std::cos(halfX);
    const float sx = std::sin(halfX);
    const float cy = std::cos(halfY);
    const float sy = std::sin(halfY);
    const float cz = std::cos(halfZ);
    const float sz = std::sin(halfZ);

    Quaternion result{
        cz * cy * cx + sz * sy * sx,
        cz * cy * sx - sz * sy * cx,
        cz * sy * cx + sz * cy * sx,
        sz * cy * cx - cz * sy * sx
    };
    const float magnitude = std::sqrt(
        result.w * result.w + result.x * result.x
        + result.y * result.y + result.z * result.z);
    if (magnitude <= kVectorEpsilon)
        return {};
    result.w /= magnitude;
    result.x /= magnitude;
    result.y /= magnitude;
    result.z /= magnitude;
    return result;
}

heritage::math::Vec3 rotateVector(
    const Quaternion& rotation,
    const heritage::math::Vec3& value)
{
    const heritage::math::Vec3 qv{ rotation.x, rotation.y, rotation.z };
    const heritage::math::Vec3 first = scale(cross(qv, value), 2.0f);
    return add(value, add(scale(first, rotation.w), cross(qv, first)));
}

heritage::math::Vec3 pointVelocity(
    const heritage::math::Vec3& linearVelocity,
    const heritage::math::Vec3& angularVelocityDegrees,
    const heritage::math::Vec3& bodyPosition,
    const heritage::math::Vec3& worldPoint)
{
    const heritage::math::Vec3 angularRadians{
        radians(angularVelocityDegrees.x),
        radians(angularVelocityDegrees.y),
        radians(angularVelocityDegrees.z)
    };
    return add(
        linearVelocity,
        cross(angularRadians, subtract(worldPoint, bodyPosition)));
}

float signOrZero(float value)
{
    if (value > 0.0001f)
        return 1.0f;
    if (value < -0.0001f)
        return -1.0f;
    return 0.0f;
}

float moveTowards(float current, float target, float maximumDelta)
{
    if (maximumDelta <= 0.0f)
        return current;
    const float difference = target - current;
    if (std::abs(difference) <= maximumDelta)
        return target;
    return current + signOrZero(difference) * maximumDelta;
}

bool validVehicleDescription(const VehicleDescription& value)
{
    return finiteFloat(value.highRateHertz)
        && value.highRateHertz >= kMinimumHighRateHertz
        && value.highRateHertz <= kMaximumHighRateHertz
        && finiteFloat(value.maximumDriveForce)
        && value.maximumDriveForce >= 0.0f
        && finiteFloat(value.maximumBrakeForce)
        && value.maximumBrakeForce >= 0.0f
        && finiteFloat(value.maximumSteerAngleDegrees)
        && value.maximumSteerAngleDegrees >= 0.0f
        && value.maximumSteerAngleDegrees <= 85.0f
        && finiteFloat(value.ackermannPercent)
        && value.ackermannPercent >= -1.0f
        && value.ackermannPercent <= 2.0f
        && finiteFloat(value.steeringRateDegreesPerSecond)
        && value.steeringRateDegreesPerSecond >= 1.0f
        && value.steeringRateDegreesPerSecond <= 1440.0f
        && finiteFloat(value.steeringReturnRateDegreesPerSecond)
        && value.steeringReturnRateDegreesPerSecond >= 1.0f
        && value.steeringReturnRateDegreesPerSecond <= 1440.0f
        && finiteFloat(value.highSpeedSteeringRateFactor)
        && value.highSpeedSteeringRateFactor >= 0.05f
        && value.highSpeedSteeringRateFactor <= 1.0f
        && finiteFloat(value.highSpeedReferenceMps)
        && value.highSpeedReferenceMps >= 1.0f
        && value.highSpeedReferenceMps <= 150.0f
        && finiteFloat(value.tireFriction)
        && value.tireFriction >= 0.0f
        && value.tireFriction <= 5.0f
        && finiteFloat(value.lateralStiffness)
        && value.lateralStiffness >= 0.0f
        && finiteFloat(value.rollingResistance)
        && value.rollingResistance >= 0.0f;
}

bool validWheelDescription(const WheelDescription& value)
{
    return finiteVec3(value.localMount)
        && finiteVec3(value.localSuspensionDirection)
        && lengthSquared(value.localSuspensionDirection) > kVectorEpsilon
        && finiteFloat(value.radius) && value.radius > 0.01f && value.radius <= 5.0f
        && finiteFloat(value.restLength) && value.restLength >= 0.01f && value.restLength <= 5.0f
        && finiteFloat(value.maximumCompression) && value.maximumCompression >= 0.0f
        && finiteFloat(value.maximumDroop) && value.maximumDroop >= 0.0f
        && value.maximumCompression < value.restLength
        && finiteFloat(value.springRate) && value.springRate >= 0.0f
        && finiteFloat(value.bumpDamping) && value.bumpDamping >= 0.0f
        && finiteFloat(value.reboundDamping) && value.reboundDamping >= 0.0f
        && finiteFloat(value.driveFactor) && value.driveFactor >= 0.0f
        && finiteFloat(value.steerFactor) && value.steerFactor >= -1.0f && value.steerFactor <= 1.0f
        && finiteFloat(value.brakeFactor) && value.brakeFactor >= 0.0f
        && finiteFloat(value.handbrakeFactor) && value.handbrakeFactor >= 0.0f;
}


bool validPowertrainDescription(const PowertrainDescription& value)
{
    if (!finiteFloat(value.idleRpm)
        || !finiteFloat(value.redlineRpm)
        || value.idleRpm < 300.0f
        || value.idleRpm > 4000.0f
        || value.redlineRpm <= value.idleRpm + 250.0f
        || value.redlineRpm > 30000.0f
        || !finiteFloat(value.maximumTorque)
        || value.maximumTorque < 0.0f
        || value.maximumTorque > 10000.0f
        || !finiteFloat(value.engineBrakingTorque)
        || value.engineBrakingTorque < 0.0f
        || value.engineBrakingTorque > 5000.0f
        || !finiteFloat(value.engineResponse)
        || value.engineResponse < 0.1f
        || value.engineResponse > 100.0f
        || !finiteFloat(value.finalDriveRatio)
        || value.finalDriveRatio < 0.05f
        || value.finalDriveRatio > 30.0f
        || !finiteFloat(value.drivetrainEfficiency)
        || value.drivetrainEfficiency < 0.05f
        || value.drivetrainEfficiency > 1.0f
        || !finiteFloat(value.shiftDurationSeconds)
        || value.shiftDurationSeconds < 0.0f
        || value.shiftDurationSeconds > 5.0f
        || !finiteFloat(value.clutchEngagementRate)
        || value.clutchEngagementRate < 0.1f
        || value.clutchEngagementRate > 100.0f
        || !finiteFloat(value.reverseGearRatio)
        || value.reverseGearRatio >= -0.05f
        || value.reverseGearRatio < -30.0f
        || !finiteFloat(value.differentialBiasRatio)
        || value.differentialBiasRatio < 1.0f
        || value.differentialBiasRatio > 20.0f
        || value.forwardGearRatios.empty()
        || value.forwardGearRatios.size() > 16)
    {
        return false;
    }

    for (float ratio : value.forwardGearRatios)
    {
        if (!finiteFloat(ratio) || ratio <= 0.05f || ratio > 30.0f)
            return false;
    }
    return true;
}

struct SurfaceProfile
{
    float frictionMultiplier = 1.0f;
    float stiffnessMultiplier = 1.0f;
    float rollingResistanceMultiplier = 1.0f;
    float relaxationMultiplier = 1.0f;
};

bool validDriverAidDescription(const DriverAidDescription& value)
{
    return finiteFloat(value.antiLockTargetSlip)
        && value.antiLockTargetSlip >= 0.02f
        && value.antiLockTargetSlip <= 1.0f
        && finiteFloat(value.tractionControlTargetSlip)
        && value.tractionControlTargetSlip >= 0.02f
        && value.tractionControlTargetSlip <= 2.0f
        && finiteFloat(value.minimumActivationSpeed)
        && value.minimumActivationSpeed >= 0.0f
        && value.minimumActivationSpeed <= 50.0f
        && finiteFloat(value.modulationRate)
        && value.modulationRate >= 0.5f
        && value.modulationRate <= 200.0f
        && finiteFloat(value.maximumHandbrakeTorque)
        && value.maximumHandbrakeTorque >= 0.0f
        && value.maximumHandbrakeTorque <= 50000.0f;
}

bool validSurface(TireSurface surface)
{
    const int value = static_cast<int>(surface);
    return value >= static_cast<int>(TireSurface::DryAsphalt)
        && value <= static_cast<int>(TireSurface::Ice);
}

SurfaceProfile legacySurfaceProfile(TireSurface surface)
{
    switch (surface)
    {
    case TireSurface::WetAsphalt:
        return { 0.74f, 0.86f, 1.15f, 1.15f };
    case TireSurface::Gravel:
        return { 0.62f, 0.58f, 2.10f, 1.40f };
    case TireSurface::Dirt:
        return { 0.54f, 0.48f, 2.55f, 1.60f };
    case TireSurface::Snow:
        return { 0.31f, 0.31f, 3.10f, 1.85f };
    case TireSurface::Ice:
        return { 0.095f, 0.18f, 1.35f, 2.20f };
    case TireSurface::DryAsphalt:
    default:
        return {};
    }
}

SurfaceProfile blendSurfaceProfile(
    const SurfaceProfile& dry,
    const SurfaceProfile& wet,
    float wetness)
{
    const float amount = std::clamp(wetness, 0.0f, 1.0f);
    const auto blend = [amount](float first, float second) {
        return first + (second - first) * amount;
    };
    return {
        blend(dry.frictionMultiplier, wet.frictionMultiplier),
        blend(dry.stiffnessMultiplier, wet.stiffnessMultiplier),
        blend(dry.rollingResistanceMultiplier, wet.rollingResistanceMultiplier),
        blend(dry.relaxationMultiplier, wet.relaxationMultiplier)
    };
}

SurfaceProfile surfaceProfile(
    heritage::physics::SurfaceMaterial material,
    float wetness,
    TireSurface fallback)
{
    using heritage::physics::SurfaceMaterial;
    switch (material)
    {
    case SurfaceMaterial::Default:
        return legacySurfaceProfile(fallback);
    case SurfaceMaterial::Asphalt:
        return blendSurfaceProfile(
            {},
            { 0.74f, 0.86f, 1.15f, 1.15f },
            wetness);
    case SurfaceMaterial::Gravel:
        return blendSurfaceProfile(
            { 0.62f, 0.58f, 2.10f, 1.40f },
            { 0.54f, 0.50f, 2.35f, 1.55f },
            wetness);
    case SurfaceMaterial::Dirt:
        return blendSurfaceProfile(
            { 0.54f, 0.48f, 2.55f, 1.60f },
            { 0.45f, 0.40f, 2.90f, 1.75f },
            wetness);
    case SurfaceMaterial::Grass:
        return blendSurfaceProfile(
            { 0.46f, 0.38f, 3.10f, 1.75f },
            { 0.31f, 0.29f, 3.65f, 1.95f },
            wetness);
    case SurfaceMaterial::Snow:
        return { 0.31f, 0.31f, 3.10f, 1.85f };
    case SurfaceMaterial::Ice:
        return { 0.095f, 0.18f, 1.35f, 2.20f };
    case SurfaceMaterial::Kerb:
        return blendSurfaceProfile(
            { 0.94f, 0.98f, 1.25f, 1.05f },
            { 0.68f, 0.82f, 1.35f, 1.18f },
            wetness);
    case SurfaceMaterial::PaintedLine:
        return blendSurfaceProfile(
            { 0.82f, 0.88f, 1.05f, 1.10f },
            { 0.55f, 0.70f, 1.10f, 1.25f },
            wetness);
    default:
        return legacySurfaceProfile(fallback);
    }
}

float relaxationBlend(float speed, float lengthValue, float deltaTime)
{
    const float transportSpeed = std::max(std::abs(speed), 0.50f);
    return 1.0f - std::exp(
        -(transportSpeed / std::max(lengthValue, 0.01f)) * deltaTime);
}

float selectedGearRatio(
    const PowertrainDescription& powertrain,
    int gear)
{
    if (gear < 0)
        return powertrain.reverseGearRatio;
    if (gear == 0)
        return 0.0f;
    const std::size_t index = static_cast<std::size_t>(gear - 1);
    return index < powertrain.forwardGearRatios.size()
        ? powertrain.forwardGearRatios[index]
        : 0.0f;
}

float engineTorqueCurveFactor(
    float engineRpm,
    float idleRpm,
    float redlineRpm)
{
    const float normalizedRpm = std::clamp(
        (engineRpm - idleRpm) / std::max(redlineRpm - idleRpm, 1.0f),
        0.0f,
        1.0f);
    constexpr float peakLocation = 0.55f;
    if (normalizedRpm <= peakLocation)
    {
        return 0.64f + 0.36f * (normalizedRpm / peakLocation);
    }
    const float falling = (normalizedRpm - peakLocation)
        / (1.0f - peakLocation);
    return 1.0f - 0.42f * std::pow(falling, 1.25f);
}

} // namespace

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
    wheel.state.suspensionLength = description.restLength;
    wheel.state.antiLockModulation = 1.0f;
    wheel.state.tractionControlModulation = 1.0f;
    // New wheels inherit the vehicle default tire profile. Step 29H can then
    // override any individual wheel without changing the others.
    wheel.tireModel = slot->record.tireModel;
    wheel.previousSuspensionLength = description.restLength;
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

bool VehicleSystem::setInputs(
    VehicleHandle handle,
    float throttle,
    float brake,
    float steering,
    float handbrake)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetInputs received an invalid or stale vehicle handle.");
        return false;
    }
    if (!finiteFloat(throttle)
        || !finiteFloat(brake)
        || !finiteFloat(steering)
        || !finiteFloat(handbrake))
    {
        setError("Vehicle inputs must be finite numbers.");
        return false;
    }
    slot->record.throttle = std::clamp(throttle, 0.0f, 1.0f);
    slot->record.brake = std::clamp(brake, 0.0f, 1.0f);
    slot->record.steering = std::clamp(steering, -1.0f, 1.0f);
    slot->record.handbrake = std::clamp(handbrake, 0.0f, 1.0f);
    clearError();
    return true;
}

bool VehicleSystem::setTuning(
    VehicleHandle handle,
    float maximumDriveForce,
    float maximumBrakeForce,
    float maximumSteerAngleDegrees,
    float tireFriction,
    float lateralStiffness,
    float rollingResistance)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetTuning received an invalid or stale vehicle handle.");
        return false;
    }
    VehicleDescription value = slot->record.description;
    value.maximumDriveForce = maximumDriveForce;
    value.maximumBrakeForce = maximumBrakeForce;
    value.maximumSteerAngleDegrees = maximumSteerAngleDegrees;
    value.tireFriction = tireFriction;
    value.lateralStiffness = lateralStiffness;
    value.rollingResistance = rollingResistance;
    if (!validVehicleDescription(value))
    {
        setError("Vehicle.SetTuning received values outside the supported range.");
        return false;
    }
    slot->record.description = value;
    clearError();
    return true;
}

bool VehicleSystem::setHighRateHertz(VehicleHandle handle, float hertz)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetHighRateHertz received an invalid or stale vehicle handle.");
        return false;
    }
    if (!finiteFloat(hertz)
        || hertz < kMinimumHighRateHertz
        || hertz > kMaximumHighRateHertz)
    {
        setError("Vehicle high-rate solver must be between 120 and 2000 Hz.");
        return false;
    }
    slot->record.description.highRateHertz = hertz;
    slot->record.highRateAccumulator = 0.0;
    clearError();
    return true;
}

bool VehicleSystem::setSteeringGeometry(
    VehicleHandle handle,
    float ackermannPercent,
    float steeringRateDegreesPerSecond,
    float steeringReturnRateDegreesPerSecond,
    float highSpeedSteeringRateFactor,
    float highSpeedReferenceMps)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetSteeringGeometry received an invalid or stale vehicle handle.");
        return false;
    }

    VehicleDescription value = slot->record.description;
    value.ackermannPercent = ackermannPercent;
    value.steeringRateDegreesPerSecond = steeringRateDegreesPerSecond;
    value.steeringReturnRateDegreesPerSecond = steeringReturnRateDegreesPerSecond;
    value.highSpeedSteeringRateFactor = highSpeedSteeringRateFactor;
    value.highSpeedReferenceMps = highSpeedReferenceMps;
    if (!validVehicleDescription(value))
    {
        setError("Vehicle.SetSteeringGeometry received values outside the supported range.");
        return false;
    }

    slot->record.description = value;
    clearError();
    return true;
}

bool VehicleSystem::steeringState(
    VehicleHandle handle,
    SteeringState& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.GetSteeringState received an invalid or stale vehicle handle.");
        return false;
    }

    value.input = slot->record.steering;
    value.targetCenterAngleDegrees = slot->record.targetSteerCenterDegrees;
    value.currentCenterAngleDegrees = slot->record.currentSteerCenterDegrees;
    value.innerWheelAngleDegrees = slot->record.innerSteerAngleDegrees;
    value.outerWheelAngleDegrees = slot->record.outerSteerAngleDegrees;
    value.detectedWheelbase = slot->record.detectedWheelbase;
    value.detectedSteerTrack = slot->record.detectedSteerTrack;
    value.currentRateFactor = slot->record.currentSteeringRateFactor;
    clearError();
    return true;
}


bool VehicleSystem::setWheelBrakeFactors(
    VehicleHandle handle,
    std::size_t wheelIndex,
    float serviceBrakeFactor,
    float handbrakeFactor)
{
    Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.SetWheelBrakeFactors received an invalid vehicle handle or wheel index.");
        return false;
    }
    if (!finiteFloat(serviceBrakeFactor)
        || serviceBrakeFactor < 0.0f
        || !finiteFloat(handbrakeFactor)
        || handbrakeFactor < 0.0f)
    {
        setError("Vehicle wheel brake factors must be finite non-negative numbers.");
        return false;
    }

    slot->record.wheels[wheelIndex].description.brakeFactor =
        serviceBrakeFactor;
    slot->record.wheels[wheelIndex].description.handbrakeFactor =
        handbrakeFactor;
    clearError();
    return true;
}

bool VehicleSystem::setDriverAids(
    VehicleHandle handle,
    bool antiLockBrakesEnabled,
    bool tractionControlEnabled,
    float antiLockTargetSlip,
    float tractionControlTargetSlip,
    float minimumActivationSpeed,
    float modulationRate,
    float maximumHandbrakeTorque)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetDriverAids received an invalid or stale vehicle handle.");
        return false;
    }

    DriverAidDescription value;
    value.antiLockBrakesEnabled = antiLockBrakesEnabled;
    value.tractionControlEnabled = tractionControlEnabled;
    value.antiLockTargetSlip = antiLockTargetSlip;
    value.tractionControlTargetSlip = tractionControlTargetSlip;
    value.minimumActivationSpeed = minimumActivationSpeed;
    value.modulationRate = modulationRate;
    value.maximumHandbrakeTorque = maximumHandbrakeTorque;
    if (!validDriverAidDescription(value))
    {
        setError("Vehicle.SetDriverAids received values outside the supported range.");
        return false;
    }

    slot->record.driverAids = value;
    if (!antiLockBrakesEnabled)
    {
        for (WheelRecord& wheel : slot->record.wheels)
        {
            wheel.state.antiLockModulation = 1.0f;
            wheel.state.antiLockActive = false;
        }
    }
    if (!tractionControlEnabled)
    {
        for (WheelRecord& wheel : slot->record.wheels)
        {
            wheel.state.tractionControlModulation = 1.0f;
            wheel.state.tractionControlActive = false;
        }
    }
    clearError();
    return true;
}

bool VehicleSystem::driverAidState(
    VehicleHandle handle,
    DriverAidState& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.GetDriverAidState received an invalid or stale vehicle handle.");
        return false;
    }

    value.antiLockBrakesEnabled =
        slot->record.driverAids.antiLockBrakesEnabled;
    value.tractionControlEnabled =
        slot->record.driverAids.tractionControlEnabled;
    value.antiLockActiveWheelCount =
        slot->record.antiLockActiveWheelCount;
    value.tractionControlActiveWheelCount =
        slot->record.tractionControlActiveWheelCount;
    value.antiLockTargetSlip =
        slot->record.driverAids.antiLockTargetSlip;
    value.tractionControlTargetSlip =
        slot->record.driverAids.tractionControlTargetSlip;
    value.minimumActivationSpeed =
        slot->record.driverAids.minimumActivationSpeed;
    value.handbrakeInput = slot->record.handbrake;
    clearError();
    return true;
}

bool VehicleSystem::setPowertrain(
    VehicleHandle handle,
    float idleRpm,
    float redlineRpm,
    float maximumTorque,
    float engineBrakingTorque,
    float finalDriveRatio,
    float drivetrainEfficiency,
    float shiftDurationSeconds,
    float clutchEngagementRate)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetPowertrain received an invalid or stale vehicle handle.");
        return false;
    }

    PowertrainDescription value = slot->record.powertrain;
    value.idleRpm = idleRpm;
    value.redlineRpm = redlineRpm;
    value.maximumTorque = maximumTorque;
    value.engineBrakingTorque = engineBrakingTorque;
    value.finalDriveRatio = finalDriveRatio;
    value.drivetrainEfficiency = drivetrainEfficiency;
    value.shiftDurationSeconds = shiftDurationSeconds;
    value.clutchEngagementRate = clutchEngagementRate;
    if (!validPowertrainDescription(value))
    {
        setError("Vehicle.SetPowertrain received values outside the supported range.");
        return false;
    }

    slot->record.powertrain = value;
    slot->record.engineRpm = std::clamp(
        slot->record.engineRpm,
        value.idleRpm,
        value.redlineRpm + 1000.0f);
    slot->record.selectedGearRatio = selectedGearRatio(
        value,
        slot->record.currentGear);
    clearError();
    return true;
}

bool VehicleSystem::setGearRatios(
    VehicleHandle handle,
    float reverseGearRatio,
    const std::vector<float>& forwardGearRatios)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetGearRatios received an invalid or stale vehicle handle.");
        return false;
    }

    PowertrainDescription value = slot->record.powertrain;
    value.reverseGearRatio = reverseGearRatio;
    value.forwardGearRatios = forwardGearRatios;
    if (!validPowertrainDescription(value))
    {
        setError("Vehicle.SetGearRatios requires one to sixteen positive forward ratios and one negative reverse ratio.");
        return false;
    }

    slot->record.powertrain = value;
    const int maximumGear = static_cast<int>(value.forwardGearRatios.size());
    if (slot->record.currentGear > maximumGear)
        slot->record.currentGear = 0;
    if (slot->record.requestedGear > maximumGear)
        slot->record.requestedGear = slot->record.currentGear;
    slot->record.selectedGearRatio = selectedGearRatio(
        value,
        slot->record.currentGear);
    clearError();
    return true;
}

bool VehicleSystem::setDifferential(
    VehicleHandle handle,
    DifferentialMode mode,
    float biasRatio)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetDifferential received an invalid or stale vehicle handle.");
        return false;
    }
    if (!finiteFloat(biasRatio) || biasRatio < 1.0f || biasRatio > 20.0f)
    {
        setError("Vehicle differential bias ratio must be between 1 and 20.");
        return false;
    }

    slot->record.powertrain.differentialMode = mode;
    slot->record.powertrain.differentialBiasRatio = biasRatio;
    clearError();
    return true;
}

bool VehicleSystem::setGear(VehicleHandle handle, int gear)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetGear received an invalid or stale vehicle handle.");
        return false;
    }

    const int maximumGear = static_cast<int>(
        slot->record.powertrain.forwardGearRatios.size());
    if (gear < -1 || gear > maximumGear)
    {
        setError("Vehicle.SetGear received a gear outside reverse, neutral and the configured forward gears.");
        return false;
    }

    const int activeGear = slot->record.shifting
        ? slot->record.requestedGear
        : slot->record.currentGear;
    if (gear == activeGear)
    {
        clearError();
        return true;
    }

    const bool directionChange = gear != 0
        && activeGear != 0
        && ((gear < 0) != (activeGear < 0));
    if (directionChange && slot->record.speed > 1.5f)
    {
        setError("Vehicle.SetGear blocked a forward/reverse direction change above 1.5 m/s.");
        return false;
    }

    slot->record.requestedGear = gear;
    slot->record.outputTorque = 0.0f;
    if (slot->record.powertrain.shiftDurationSeconds <= 0.0001f)
    {
        slot->record.currentGear = gear;
        slot->record.shifting = false;
        slot->record.shiftTimeRemaining = 0.0f;
    }
    else
    {
        slot->record.shifting = true;
        slot->record.shiftTimeRemaining =
            slot->record.powertrain.shiftDurationSeconds;
    }
    clearError();
    return true;
}

bool VehicleSystem::shiftUp(VehicleHandle handle)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.ShiftUp received an invalid or stale vehicle handle.");
        return false;
    }
    const int baseGear = slot->record.shifting
        ? slot->record.requestedGear
        : slot->record.currentGear;
    const int maximumGear = static_cast<int>(
        slot->record.powertrain.forwardGearRatios.size());
    return setGear(handle, std::min(baseGear + 1, maximumGear));
}

bool VehicleSystem::shiftDown(VehicleHandle handle)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.ShiftDown received an invalid or stale vehicle handle.");
        return false;
    }
    const int baseGear = slot->record.shifting
        ? slot->record.requestedGear
        : slot->record.currentGear;
    return setGear(handle, std::max(baseGear - 1, -1));
}

bool VehicleSystem::drivetrainState(
    VehicleHandle handle,
    DrivetrainState& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.GetDrivetrainState received an invalid or stale vehicle handle.");
        return false;
    }

    value.currentGear = slot->record.currentGear;
    value.requestedGear = slot->record.requestedGear;
    value.shifting = slot->record.shifting;
    value.shiftTimeRemaining = slot->record.shiftTimeRemaining;
    value.engineRpm = slot->record.engineRpm;
    value.engineTorque = slot->record.engineTorque;
    value.clutchEngagement = slot->record.clutchEngagement;
    value.clutchSlipRpm = slot->record.clutchSlipRpm;
    value.wheelCoupledRpm = slot->record.wheelCoupledRpm;
    value.selectedGearRatio = slot->record.selectedGearRatio;
    value.finalDriveRatio = slot->record.powertrain.finalDriveRatio;
    value.outputTorque = slot->record.outputTorque;
    value.drivenWheelSpeedDifferenceRpm =
        slot->record.drivenWheelSpeedDifferenceRpm;
    value.differentialMode = slot->record.powertrain.differentialMode;
    clearError();
    return true;
}

std::size_t VehicleSystem::forwardGearCount(VehicleHandle handle) const
{
    const Slot* slot = resolve(handle);
    return slot ? slot->record.powertrain.forwardGearRatios.size() : 0;
}

bool VehicleSystem::setTireModel(
    VehicleHandle handle,
    float nominalLoad,
    float peakFriction,
    float longitudinalStiffness,
    float corneringStiffness,
    float loadSensitivity,
    float longitudinalRelaxationLength,
    float lateralRelaxationLength,
    float wheelInertia,
    float pneumaticTrail,
    float stiffnessLoadExponent,
    float longitudinalShapeFactor,
    float lateralShapeFactor,
    float longitudinalCurvatureFactor,
    float lateralCurvatureFactor,
    float combinedSlipExponent,
    float pneumaticTrailFalloff)
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
    slot->record.description.tireFriction = value.peakFriction;
    // Preserve the legacy/global meaning of SetTireModel: it updates the
    // default profile and every wheel already attached to the vehicle.
    for (WheelRecord& wheel : slot->record.wheels)
        wheel.tireModel = value;
    clearError();
    return true;
}

bool VehicleSystem::setWheelTireModel(
    VehicleHandle handle,
    std::size_t wheelIndex,
    float nominalLoad,
    float peakFriction,
    float longitudinalStiffness,
    float corneringStiffness,
    float loadSensitivity,
    float longitudinalRelaxationLength,
    float lateralRelaxationLength,
    float wheelInertia,
    float pneumaticTrail,
    float stiffnessLoadExponent,
    float longitudinalShapeFactor,
    float lateralShapeFactor,
    float longitudinalCurvatureFactor,
    float lateralCurvatureFactor,
    float combinedSlipExponent,
    float pneumaticTrailFalloff)
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

void VehicleSystem::simulate(
    heritage::physics::RigidBodySystem& bodies,
    const heritage::physics::CollisionSystem& collisions,
    float worldDeltaTime,
    const heritage::math::Vec3& gravity)
{
    if (!finiteFloat(worldDeltaTime) || worldDeltaTime <= 0.0f
        || !finiteVec3(gravity))
        return;

    removeInvalidBodies(bodies);
    for (Slot& slot : m_slots)
    {
        if (!slot.alive)
            continue;

        Record& vehicle = slot.record;
        vehicle.lastHighRateStepCount = 0;

        bool chassisSleeping = false;
        bodies.sleeping(vehicle.description.chassisBody, chassisSleeping);
        if (vehicle.parkedResting && !chassisSleeping)
        {
            // A reset, impulse, collision, or authored pose change woke the
            // chassis. The parked lock is no longer authoritative.
            vehicle.parkedResting = false;
            vehicle.parkedRestRequiresBrake = false;
            vehicle.parkedRestBrakeInput = 0.0f;
            vehicle.parkedRestHandbrakeInput = 0.0f;
            vehicle.restTimer = 0.0f;
        }
        if (chassisSleeping)
        {
            const bool requiredBrakeReleased =
                vehicle.brake + 0.001f < vehicle.parkedRestBrakeInput
                || vehicle.handbrake + 0.001f
                    < vehicle.parkedRestHandbrakeInput;
            const bool shouldWake = vehicle.throttle > 0.001f
                || (vehicle.parkedResting
                    && vehicle.parkedRestRequiresBrake
                    && requiredBrakeReleased);
            if (!shouldWake)
            {
                vehicle.highRateAccumulator = 0.0;
                vehicle.speed = 0.0f;
                continue;
            }

            bodies.wake(vehicle.description.chassisBody);
            vehicle.parkedResting = false;
            vehicle.parkedRestRequiresBrake = false;
            vehicle.parkedRestBrakeInput = 0.0f;
            vehicle.parkedRestHandbrakeInput = 0.0f;
            vehicle.restTimer = 0.0f;
        }

        const double highRateDelta = 1.0
            / static_cast<double>(vehicle.description.highRateHertz);
        vehicle.highRateAccumulator += static_cast<double>(worldDeltaTime);

        while (vehicle.highRateAccumulator + 1.0e-12 >= highRateDelta
            && vehicle.lastHighRateStepCount < kMaximumHighRateStepsPerWorldStep)
        {
            vehicle.highRateAccumulator -= highRateDelta;
            if (vehicle.highRateAccumulator < 0.0)
                vehicle.highRateAccumulator = 0.0;
            simulateVehicleSubstep(
                vehicle,
                bodies,
                collisions,
                static_cast<float>(highRateDelta));
            ++vehicle.lastHighRateStepCount;
            ++vehicle.totalHighRateStepCount;
        }

        // A malformed rate or a debugger stall must not create an unbounded
        // high-rate backlog inside one vehicle.
        const double maximumBacklog = highRateDelta * 2.0;
        if (vehicle.highRateAccumulator > maximumBacklog)
            vehicle.highRateAccumulator = std::fmod(
                vehicle.highRateAccumulator,
                highRateDelta);

        heritage::math::Vec3 linearVelocity{};
        heritage::math::Vec3 angularVelocityDegrees{};
        if (bodies.linearVelocity(
                vehicle.description.chassisBody,
                linearVelocity))
        {
            vehicle.speed = length(linearVelocity);
        }
        bodies.angularVelocityDegrees(
            vehicle.description.chassisBody,
            angularVelocityDegrees);

        // A tire is a static-friction contact while it is parked, not merely a
        // velocity-dependent force curve. High-rate suspension/tire impulses
        // intentionally wake the rigid body, so the generic collision-island
        // sleep timer cannot settle a raycast-supported vehicle by itself.
        // Detect a physically supportable rest state here, then let the rigid
        // body sleep until propulsion, brake release on a slope, an impact, or
        // an authored transform wakes it.
        const bool allWheelsGrounded = !vehicle.wheels.empty()
            && vehicle.groundedWheelCount == vehicle.wheels.size();
        heritage::math::Vec3 weightedNormal{};
        float normalLoadTotal = 0.0f;
        float availableBrakeHoldForce = 0.0f;
        float maximumWheelSpeed = 0.0f;
        for (const WheelRecord& wheel : vehicle.wheels)
        {
            maximumWheelSpeed = std::max(
                maximumWheelSpeed,
                std::abs(wheel.state.wheelAngularVelocity));
            if (!wheel.state.grounded || wheel.state.normalForce <= 0.0f)
                continue;

            weightedNormal = add(
                weightedNormal,
                scale(wheel.state.contactNormal, wheel.state.normalForce));
            normalLoadTotal += wheel.state.normalForce;
            if (wheel.state.appliedBrakeTorque > 0.0f)
            {
                const float tireHoldLimit = wheel.state.effectiveFriction
                    * wheel.state.normalForce;
                const float brakeHoldLimit = wheel.state.appliedBrakeTorque
                    / std::max(wheel.description.radius, 0.01f);
                availableBrakeHoldForce += std::min(
                    tireHoldLimit,
                    brakeHoldLimit);
            }
        }

        float chassisMass = 0.0f;
        float gravityFactor = 0.0f;
        bodies.mass(vehicle.description.chassisBody, chassisMass);
        bodies.gravityFactor(vehicle.description.chassisBody, gravityFactor);
        const heritage::math::Vec3 supportNormal = normalized(
            weightedNormal,
            { 0.0f, 1.0f, 0.0f });
        const heritage::math::Vec3 bodyGravity = scale(gravity, gravityFactor);
        const heritage::math::Vec3 predictedLinearVelocity = add(
            linearVelocity,
            scale(bodyGravity, worldDeltaTime));
        const float predictedSpeed = length(predictedLinearVelocity);
        vehicle.speed = predictedSpeed;
        const heritage::math::Vec3 tangentialGravity = subtract(
            bodyGravity,
            scale(supportNormal, dot(bodyGravity, supportNormal)));
        const float gravityMagnitude = length(bodyGravity);
        const float tangentialGravityMagnitude = length(tangentialGravity);
        const float flatSlopeLimit = gravityMagnitude * std::sin(
            radians(kVehicleRestFlatSlopeDegrees));
        const bool effectivelyFlat = tangentialGravityMagnitude
            <= flatSlopeLimit + 0.001f;
        const float requiredHoldForce = chassisMass
            * tangentialGravityMagnitude;
        const bool brakesCanHold = availableBrakeHoldForce
            >= requiredHoldForce * 1.05f;
        const bool quietEnough = predictedSpeed <= kVehicleRestLinearSpeed
            && length(angularVelocityDegrees)
                <= kVehicleRestAngularSpeedDegrees
            && maximumWheelSpeed <= kVehicleRestWheelSpeed;
        const bool canRest = allWheelsGrounded
            && normalLoadTotal > 0.0f
            && vehicle.throttle <= 0.001f
            && quietEnough
            && (effectivelyFlat || brakesCanHold);
        vehicle.restCandidate = canRest;
        vehicle.requiredHoldForce = requiredHoldForce;
        vehicle.availableBrakeHoldForce = availableBrakeHoldForce;

        if (canRest)
            vehicle.restTimer += worldDeltaTime;
        else
            vehicle.restTimer = 0.0f;

        if (vehicle.restTimer >= kVehicleRestDelaySeconds
            && bodies.setSleeping(vehicle.description.chassisBody, true))
        {
            vehicle.speed = 0.0f;
            vehicle.parkedResting = true;
            vehicle.parkedRestRequiresBrake = !effectivelyFlat;
            vehicle.parkedRestBrakeInput = vehicle.brake;
            vehicle.parkedRestHandbrakeInput = vehicle.handbrake;
        }
    }
}

void VehicleSystem::simulateVehicleSubstep(
    Record& vehicle,
    heritage::physics::RigidBodySystem& bodies,
    const heritage::physics::CollisionSystem& collisions,
    float substepDeltaTime)
{
    heritage::physics::RigidBodyPose pose;
    heritage::math::Vec3 linearVelocity{};
    heritage::math::Vec3 angularVelocityDegrees{};
    if (!bodies.pose(vehicle.description.chassisBody, pose)
        || !bodies.linearVelocity(vehicle.description.chassisBody, linearVelocity)
        || !bodies.angularVelocityDegrees(
            vehicle.description.chassisBody,
            angularVelocityDegrees))
    {
        return;
    }

    const Quaternion chassisRotation = quaternionFromEulerDegrees(
        pose.rotationDegrees);

    const float chassisSpeed = length(linearVelocity);
    vehicle.targetSteerCenterDegrees =
        vehicle.description.maximumSteerAngleDegrees * vehicle.steering;

    const float speedBlend = std::clamp(
        chassisSpeed / vehicle.description.highSpeedReferenceMps,
        0.0f,
        1.0f);
    vehicle.currentSteeringRateFactor =
        1.0f + (vehicle.description.highSpeedSteeringRateFactor - 1.0f)
        * speedBlend;
    const bool returningToCenter = std::abs(vehicle.steering) < 0.0001f;
    const float steeringRate = returningToCenter
        ? vehicle.description.steeringReturnRateDegreesPerSecond
        : vehicle.description.steeringRateDegreesPerSecond
            * vehicle.currentSteeringRateFactor;
    vehicle.currentSteerCenterDegrees = moveTowards(
        vehicle.currentSteerCenterDegrees,
        vehicle.targetSteerCenterDegrees,
        steeringRate * substepDeltaTime);

    float steeredWeight = 0.0f;
    float steeredZ = 0.0f;
    float steeredMinX = (std::numeric_limits<float>::max)();
    float steeredMaxX = (std::numeric_limits<float>::lowest)();
    float referenceWeight = 0.0f;
    float referenceZ = 0.0f;
    for (const WheelRecord& wheel : vehicle.wheels)
    {
        const float steerWeight = std::abs(wheel.description.steerFactor);
        if (steerWeight > 0.0001f)
        {
            steeredWeight += steerWeight;
            steeredZ += wheel.description.localMount.z * steerWeight;
            steeredMinX = std::min(steeredMinX, wheel.description.localMount.x);
            steeredMaxX = std::max(steeredMaxX, wheel.description.localMount.x);
        }
        else
        {
            referenceWeight += 1.0f;
            referenceZ += wheel.description.localMount.z;
        }
    }

    float steeringAxleCenterX = 0.0f;
    vehicle.detectedWheelbase = 0.0f;
    vehicle.detectedSteerTrack = 0.0f;
    if (steeredWeight > 0.0001f)
    {
        steeringAxleCenterX = 0.5f * (steeredMinX + steeredMaxX);
        vehicle.detectedSteerTrack = std::max(0.0f, steeredMaxX - steeredMinX);
        if (referenceWeight > 0.0001f)
        {
            const float steeringAxleZ = steeredZ / steeredWeight;
            const float referenceAxleZ = referenceZ / referenceWeight;
            vehicle.detectedWheelbase = std::abs(
                steeringAxleZ - referenceAxleZ);
        }
    }

    const float centerMagnitude = std::abs(
        vehicle.currentSteerCenterDegrees);
    float innerMagnitude = centerMagnitude;
    float outerMagnitude = centerMagnitude;
    if (centerMagnitude > 0.001f
        && vehicle.detectedWheelbase > 0.01f
        && vehicle.detectedSteerTrack > 0.01f)
    {
        const float centerRadians = radians(centerMagnitude);
        const float centerRadius = vehicle.detectedWheelbase
            / std::max(std::tan(centerRadians), 0.0001f);
        const float halfTrack = vehicle.detectedSteerTrack * 0.5f;
        const float innerRadius = std::max(0.05f, centerRadius - halfTrack);
        const float outerRadius = centerRadius + halfTrack;
        const float idealInner = degrees(std::atan(
            vehicle.detectedWheelbase / innerRadius));
        const float idealOuter = degrees(std::atan(
            vehicle.detectedWheelbase / outerRadius));
        innerMagnitude = centerMagnitude
            + (idealInner - centerMagnitude)
                * vehicle.description.ackermannPercent;
        outerMagnitude = centerMagnitude
            + (idealOuter - centerMagnitude)
                * vehicle.description.ackermannPercent;
    }

    const float centerSign = signOrZero(
        vehicle.currentSteerCenterDegrees);
    vehicle.innerSteerAngleDegrees = centerSign * innerMagnitude;
    vehicle.outerSteerAngleDegrees = centerSign * outerMagnitude;

    if (vehicle.shifting)
    {
        vehicle.shiftTimeRemaining = std::max(
            0.0f,
            vehicle.shiftTimeRemaining - substepDeltaTime);
        if (vehicle.shiftTimeRemaining <= 0.0f)
        {
            vehicle.currentGear = vehicle.requestedGear;
            vehicle.shifting = false;
        }
    }

    vehicle.selectedGearRatio = vehicle.shifting
        ? 0.0f
        : selectedGearRatio(vehicle.powertrain, vehicle.currentGear);

    float drivenWeight = 0.0f;
    float drivenOmega = 0.0f;
    float drivenRadius = 0.0f;
    float minimumDrivenOmega = (std::numeric_limits<float>::max)();
    float maximumDrivenOmega = (std::numeric_limits<float>::lowest)();
    for (const WheelRecord& wheel : vehicle.wheels)
    {
        if (wheel.description.driveFactor <= 0.0f)
            continue;
        const float weight = wheel.description.driveFactor;
        drivenWeight += weight;
        drivenOmega += wheel.state.wheelAngularVelocity * weight;
        drivenRadius += wheel.description.radius * weight;
        const float absoluteOmega = std::abs(wheel.state.wheelAngularVelocity);
        minimumDrivenOmega = std::min(minimumDrivenOmega, absoluteOmega);
        maximumDrivenOmega = std::max(maximumDrivenOmega, absoluteOmega);
    }
    if (drivenWeight <= 0.0f)
        drivenWeight = 1.0f;
    drivenOmega /= drivenWeight;
    drivenRadius /= drivenWeight;
    if (drivenRadius <= 0.01f)
        drivenRadius = 0.35f;

    vehicle.drivenWheelSpeedDifferenceRpm =
        maximumDrivenOmega >= minimumDrivenOmega
        ? (maximumDrivenOmega - minimumDrivenOmega)
            * (60.0f / (2.0f * kPi))
        : 0.0f;
    vehicle.wheelCoupledRpm = std::abs(
        drivenOmega
        * vehicle.selectedGearRatio
        * vehicle.powertrain.finalDriveRatio)
        * (60.0f / (2.0f * kPi));

    const bool drivelineConnected = !vehicle.shifting
        && std::abs(vehicle.selectedGearRatio) > 0.0001f;
    float clutchTarget = 0.0f;
    if (drivelineConnected)
    {
        const float launchBlend = std::clamp(
            vehicle.wheelCoupledRpm
                / std::max(vehicle.powertrain.idleRpm * 1.35f, 1.0f),
            0.0f,
            1.0f);
        clutchTarget = vehicle.throttle > 0.02f
            ? 0.25f + 0.75f * launchBlend
            : 1.0f;
    }
    vehicle.clutchEngagement = moveTowards(
        vehicle.clutchEngagement,
        clutchTarget,
        vehicle.powertrain.clutchEngagementRate * substepDeltaTime);

    const float freeRevTarget = vehicle.powertrain.idleRpm
        + vehicle.throttle
            * (vehicle.powertrain.redlineRpm - vehicle.powertrain.idleRpm)
            * 0.96f;
    const float coupledTarget = std::max(
        vehicle.powertrain.idleRpm,
        vehicle.wheelCoupledRpm);
    const float engineTarget = drivelineConnected
        ? freeRevTarget
            + (coupledTarget - freeRevTarget) * vehicle.clutchEngagement
        : freeRevTarget;
    const float engineResponse = 1.0f - std::exp(
        -vehicle.powertrain.engineResponse * substepDeltaTime);
    vehicle.engineRpm += (engineTarget - vehicle.engineRpm) * engineResponse;
    vehicle.engineRpm = std::clamp(
        vehicle.engineRpm,
        vehicle.powertrain.idleRpm,
        vehicle.powertrain.redlineRpm + 750.0f);
    vehicle.clutchSlipRpm = drivelineConnected
        ? vehicle.engineRpm - vehicle.wheelCoupledRpm
        : vehicle.engineRpm;

    const float torqueFactor = engineTorqueCurveFactor(
        vehicle.engineRpm,
        vehicle.powertrain.idleRpm,
        vehicle.powertrain.redlineRpm);
    const bool revLimiter = vehicle.engineRpm
        >= vehicle.powertrain.redlineRpm;
    const float combustionTorque = revLimiter
        ? 0.0f
        : vehicle.powertrain.maximumTorque
            * torqueFactor
            * vehicle.throttle;
    const float engineBrakeBlend = std::clamp(
        (vehicle.engineRpm - vehicle.powertrain.idleRpm)
            / std::max(
                vehicle.powertrain.redlineRpm
                    - vehicle.powertrain.idleRpm,
                1.0f),
        0.0f,
        1.0f);
    const float engineBrakeTorque = vehicle.powertrain.engineBrakingTorque
        * (1.0f - vehicle.throttle)
        * engineBrakeBlend;
    vehicle.engineTorque = combustionTorque - engineBrakeTorque;
    vehicle.outputTorque = drivelineConnected
        ? vehicle.engineTorque
            * vehicle.selectedGearRatio
            * vehicle.powertrain.finalDriveRatio
            * vehicle.powertrain.drivetrainEfficiency
            * vehicle.clutchEngagement
        : 0.0f;
    const float maximumOutputTorque = vehicle.description.maximumDriveForce
        * drivenRadius;
    vehicle.outputTorque = std::clamp(
        vehicle.outputTorque,
        -maximumOutputTorque,
        maximumOutputTorque);

    std::vector<float> driveShares(vehicle.wheels.size(), 0.0f);
    float driveShareTotal = 0.0f;
    const float averageDrivenAbsoluteOmega = std::abs(drivenOmega);
    for (std::size_t wheelIndex = 0;
        wheelIndex < vehicle.wheels.size();
        ++wheelIndex)
    {
        const WheelRecord& wheel = vehicle.wheels[wheelIndex];
        if (wheel.description.driveFactor <= 0.0f)
            continue;

        float share = wheel.description.driveFactor;
        if (vehicle.powertrain.differentialMode
            == DifferentialMode::LimitedSlip)
        {
            const float wheelSpeed = std::abs(
                wheel.state.wheelAngularVelocity);
            const float speedError = (averageDrivenAbsoluteOmega - wheelSpeed)
                / std::max(averageDrivenAbsoluteOmega + 1.0f, 1.0f);
            const float multiplier = std::clamp(
                1.0f + speedError
                    * (vehicle.powertrain.differentialBiasRatio - 1.0f),
                1.0f / vehicle.powertrain.differentialBiasRatio,
                vehicle.powertrain.differentialBiasRatio);
            share *= multiplier;
        }
        driveShares[wheelIndex] = share;
        driveShareTotal += share;
    }
    if (driveShareTotal > 0.0f)
    {
        for (float& share : driveShares)
            share /= driveShareTotal;
    }

    float totalBrakeFactor = 0.0f;
    float totalHandbrakeFactor = 0.0f;
    for (const WheelRecord& wheel : vehicle.wheels)
    {
        totalBrakeFactor += wheel.description.brakeFactor;
        totalHandbrakeFactor += wheel.description.handbrakeFactor;
    }
    if (totalBrakeFactor <= 0.0f)
        totalBrakeFactor = 1.0f;
    if (totalHandbrakeFactor <= 0.0f)
        totalHandbrakeFactor = 1.0f;

    vehicle.antiLockActiveWheelCount = 0;
    vehicle.tractionControlActiveWheelCount = 0;
    std::size_t groundedCount = 0;
    for (std::size_t wheelIndex = 0;
        wheelIndex < vehicle.wheels.size();
        ++wheelIndex)
    {
        WheelRecord& wheel = vehicle.wheels[wheelIndex];
        WheelState& state = wheel.state;
        const WheelDescription& description = wheel.description;
        const float previousSlipRatio = state.slipRatio;
        const float previousLongitudinalSpeed = state.longitudinalSpeed;

        const heritage::math::Vec3 mountWorld = add(
            pose.position,
            rotateVector(chassisRotation, description.localMount));
        const heritage::math::Vec3 suspensionDirection = normalized(
            rotateVector(
                chassisRotation,
                description.localSuspensionDirection),
            { 0.0f, -1.0f, 0.0f });

        const float minimumLength = description.restLength
            - description.maximumCompression;
        const float maximumLength = description.restLength
            + description.maximumDroop;
        const float rayDistance = maximumLength + description.radius;

        heritage::physics::CollisionQueryFilter filter;
        filter.layerMask = 0xffffffffu;
        filter.includeTriggers = false;
        filter.ignoredBody = vehicle.description.chassisBody;
        heritage::physics::RaycastHit hit;
        const bool hitGround = collisions.raycast(
            mountWorld,
            suspensionDirection,
            rayDistance,
            filter,
            bodies,
            hit);

        state.grounded = false;
        state.normalForce = 0.0f;
        state.longitudinalForce = 0.0f;
        state.lateralForce = 0.0f;
        state.appliedDriveTorque = 0.0f;
        state.appliedBrakeTorque = 0.0f;
        state.serviceBrakeTorque = 0.0f;
        state.handbrakeTorque = 0.0f;
        state.antiLockActive = false;
        state.tractionControlActive = false;
        state.compressionVelocity = 0.0f;
        state.longitudinalSpeed = 0.0f;
        state.lateralSpeed = 0.0f;
        state.slipRatio = 0.0f;
        state.slipAngleDegrees = 0.0f;
        state.effectiveFriction = 0.0f;
        state.gripUtilization = 0.0f;
        state.pureLongitudinalForce = 0.0f;
        state.pureLateralForce = 0.0f;
        state.combinedSlipScale = 1.0f;
        state.pneumaticTrail = 0.0f;
        state.aligningTorque = 0.0f;
        state.contactCollider = heritage::physics::InvalidCollider;
        state.surfaceMaterial = heritage::physics::SurfaceMaterial::Default;
        state.surfaceWetness = 0.0f;
        state.contactNormal = { 0.0f, 1.0f, 0.0f };
        const float steerFactorMagnitude = std::abs(
            description.steerFactor);
        const float steerFactorSign = signOrZero(description.steerFactor);
        state.steerAngleDegrees = vehicle.currentSteerCenterDegrees
            * description.steerFactor;
        if (steerFactorMagnitude > 0.0001f
            && centerMagnitude > 0.001f
            && vehicle.detectedWheelbase > 0.01f
            && vehicle.detectedSteerTrack > 0.01f)
        {
            const float wheelTurnSign = centerSign * steerFactorSign;
            const bool isInsideWheel = wheelTurnSign > 0.0f
                ? description.localMount.x > steeringAxleCenterX
                : description.localMount.x < steeringAxleCenterX;
            const float ackermannMagnitude = isInsideWheel
                ? innerMagnitude
                : outerMagnitude;
            state.steerAngleDegrees = wheelTurnSign
                * ackermannMagnitude
                * steerFactorMagnitude;
        }

        const float gearDirection = signOrZero(vehicle.selectedGearRatio);
        const float drivenSlip = previousSlipRatio * gearDirection;
        float tractionTargetModulation = 1.0f;
        if (vehicle.driverAids.tractionControlEnabled
            && description.driveFactor > 0.0f
            && vehicle.throttle > 0.01f
            && std::abs(previousLongitudinalSpeed)
                >= vehicle.driverAids.minimumActivationSpeed
            && drivenSlip > vehicle.driverAids.tractionControlTargetSlip)
        {
            tractionTargetModulation = std::clamp(
                vehicle.driverAids.tractionControlTargetSlip
                    / std::max(drivenSlip, 0.001f),
                0.05f,
                1.0f);
            state.tractionControlActive = true;
            ++vehicle.tractionControlActiveWheelCount;
        }
        state.tractionControlModulation = moveTowards(
            state.tractionControlModulation,
            tractionTargetModulation,
            vehicle.driverAids.modulationRate * substepDeltaTime);

        const float driveTorque = vehicle.outputTorque
            * driveShares[wheelIndex]
            * state.tractionControlModulation;

        float antiLockTargetModulation = 1.0f;
        const float brakingDirection = signOrZero(previousLongitudinalSpeed);
        const float brakingSlip = -previousSlipRatio * brakingDirection;
        if (vehicle.driverAids.antiLockBrakesEnabled
            && vehicle.brake > 0.01f
            && description.brakeFactor > 0.0f
            && std::abs(previousLongitudinalSpeed)
                >= vehicle.driverAids.minimumActivationSpeed
            && brakingSlip > vehicle.driverAids.antiLockTargetSlip)
        {
            antiLockTargetModulation = std::clamp(
                vehicle.driverAids.antiLockTargetSlip
                    / std::max(brakingSlip, 0.001f),
                0.05f,
                1.0f);
            state.antiLockActive = true;
            ++vehicle.antiLockActiveWheelCount;
        }
        state.antiLockModulation = moveTowards(
            state.antiLockModulation,
            antiLockTargetModulation,
            vehicle.driverAids.modulationRate * substepDeltaTime);

        const float serviceBrakeTorque =
            vehicle.description.maximumBrakeForce
            * vehicle.brake
            * (description.brakeFactor / totalBrakeFactor)
            * description.radius
            * state.antiLockModulation;
        const float handbrakeTorque =
            vehicle.driverAids.maximumHandbrakeTorque
            * vehicle.handbrake
            * (description.handbrakeFactor / totalHandbrakeFactor);
        const float brakeTorqueMagnitude =
            serviceBrakeTorque + handbrakeTorque;
        state.appliedDriveTorque = driveTorque;
        state.appliedBrakeTorque = brakeTorqueMagnitude;
        state.serviceBrakeTorque = serviceBrakeTorque;
        state.handbrakeTorque = handbrakeTorque;

        if (!hitGround)
        {
            state.suspensionLength = maximumLength;
            state.compression = description.restLength - maximumLength;
            state.compressionVelocity = (
                wheel.previousSuspensionLength - maximumLength)
                / substepDeltaTime;
            state.worldCenter = add(
                mountWorld,
                scale(suspensionDirection, maximumLength));
            state.contactPoint = add(
                state.worldCenter,
                scale(suspensionDirection, description.radius));
            wheel.previousSuspensionLength = maximumLength;

            const float projectedAngularVelocity = state.wheelAngularVelocity
                + (driveTorque / wheel.tireModel.wheelInertia)
                    * substepDeltaTime;
            const float brakeTorque = brakeTorqueMagnitude > 0.0f
                ? std::clamp(
                    -projectedAngularVelocity * wheel.tireModel.wheelInertia
                        / substepDeltaTime,
                    -brakeTorqueMagnitude,
                    brakeTorqueMagnitude)
                : 0.0f;
            const float angularAcceleration = (driveTorque + brakeTorque)
                / wheel.tireModel.wheelInertia;
            state.wheelAngularVelocity += angularAcceleration * substepDeltaTime;
            state.wheelAngularVelocity *= std::exp(-0.35f * substepDeltaTime);
            state.relaxedSlipRatio *= std::exp(-4.0f * substepDeltaTime);
            state.relaxedSlipAngleDegrees *= std::exp(-4.0f * substepDeltaTime);
            state.wheelRotationDegrees += degrees(
                state.wheelAngularVelocity * substepDeltaTime);
            continue;
        }

        float suspensionLength = hit.distance - description.radius;
        if (suspensionLength > maximumLength)
        {
            state.suspensionLength = maximumLength;
            state.compression = description.restLength - maximumLength;
            state.worldCenter = add(
                mountWorld,
                scale(suspensionDirection, maximumLength));
            state.contactPoint = hit.point;
            wheel.previousSuspensionLength = maximumLength;
            continue;
        }

        suspensionLength = std::clamp(
            suspensionLength,
            minimumLength,
            maximumLength);
        state.grounded = true;
        ++groundedCount;
        state.suspensionLength = suspensionLength;
        state.compression = description.restLength - suspensionLength;
        wheel.previousSuspensionLength = suspensionLength;
        state.worldCenter = add(
            mountWorld,
            scale(suspensionDirection, suspensionLength));
        state.contactPoint = hit.point;
        state.contactNormal = normalized(hit.normal, { 0.0f, 1.0f, 0.0f });
        state.contactCollider = hit.collider;
        state.surfaceMaterial = hit.surfaceMaterial;
        state.surfaceWetness = hit.surfaceWetness;

        // The vehicle loop may run at 1000 Hz inside a 120 Hz rigid-body
        // world step. The chassis pose therefore remains fixed across several
        // vehicle substeps even though tire/suspension impulses immediately
        // change its velocity. A finite difference of ray length would report
        // damper motion only during the first substep and zero for the rest,
        // effectively removing most of the authored damping. Measure the
        // attachment-point velocity along the suspension axis instead. This is
        // also the physical relative velocity used by a massless raycast wheel.
        bodies.linearVelocity(vehicle.description.chassisBody, linearVelocity);
        bodies.angularVelocityDegrees(
            vehicle.description.chassisBody,
            angularVelocityDegrees);
        const heritage::math::Vec3 mountVelocity = pointVelocity(
            linearVelocity,
            angularVelocityDegrees,
            pose.position,
            mountWorld);
        heritage::math::Vec3 supportVelocity{};
        if (hit.body != heritage::physics::InvalidBody)
        {
            heritage::physics::RigidBodyPose supportPose;
            heritage::math::Vec3 supportLinearVelocity{};
            heritage::math::Vec3 supportAngularVelocityDegrees{};
            if (bodies.pose(hit.body, supportPose)
                && bodies.linearVelocity(hit.body, supportLinearVelocity)
                && bodies.angularVelocityDegrees(
                    hit.body,
                    supportAngularVelocityDegrees))
            {
                supportVelocity = pointVelocity(
                    supportLinearVelocity,
                    supportAngularVelocityDegrees,
                    supportPose.position,
                    hit.point);
            }
        }
        state.compressionVelocity = dot(
            subtract(mountVelocity, supportVelocity),
            suspensionDirection);

        const float damping = state.compressionVelocity >= 0.0f
            ? description.bumpDamping
            : description.reboundDamping;
        const float suspensionForce = std::clamp(
            description.springRate * state.compression
                + damping * state.compressionVelocity,
            0.0f,
            kMaximumSuspensionForce);
        state.normalForce = suspensionForce;
        if (suspensionForce > 0.0f)
        {
            const heritage::math::Vec3 suspensionImpulse = scale(
                suspensionDirection,
                -suspensionForce * substepDeltaTime);
            bodies.applyImpulseAtPoint(
                vehicle.description.chassisBody,
                suspensionImpulse,
                mountWorld);
        }

        // Refresh velocities because earlier wheels in this same 1 ms substep
        // may already have applied impulses to the chassis.
        bodies.linearVelocity(vehicle.description.chassisBody, linearVelocity);
        bodies.angularVelocityDegrees(
            vehicle.description.chassisBody,
            angularVelocityDegrees);
        const heritage::math::Vec3 contactVelocity = pointVelocity(
            linearVelocity,
            angularVelocityDegrees,
            pose.position,
            state.contactPoint);

        const float steerRadians = radians(state.steerAngleDegrees);
        const heritage::math::Vec3 localForward{
            std::sin(steerRadians), 0.0f, std::cos(steerRadians)
        };
        heritage::math::Vec3 wheelForward = rotateVector(
            chassisRotation,
            localForward);
        wheelForward = subtract(
            wheelForward,
            scale(state.contactNormal, dot(wheelForward, state.contactNormal)));
        wheelForward = normalized(wheelForward, { 0.0f, 0.0f, 1.0f });
        heritage::math::Vec3 wheelRight = normalized(
            cross(state.contactNormal, wheelForward),
            { 1.0f, 0.0f, 0.0f });

        const float longitudinalSpeed = dot(contactVelocity, wheelForward);
        const float lateralSpeed = dot(contactVelocity, wheelRight);
        state.longitudinalSpeed = longitudinalSpeed;
        state.lateralSpeed = lateralSpeed;

        float differentialLockTorque = 0.0f;
        if (vehicle.powertrain.differentialMode == DifferentialMode::Locked
            && description.driveFactor > 0.0f)
        {
            constexpr float lockingStrength = 180.0f;
            differentialLockTorque = -(
                state.wheelAngularVelocity - drivenOmega)
                * lockingStrength;
        }

        const float circumferentialSpeed =
            state.wheelAngularVelocity * description.radius;
        const float slipDenominator = std::max(
            std::max(std::abs(longitudinalSpeed),
                std::abs(circumferentialSpeed)),
            1.0f);
        state.slipRatio = std::clamp(
            (circumferentialSpeed - longitudinalSpeed) / slipDenominator,
            -5.0f,
            5.0f);
        const float slipAngleRadians = std::atan2(
            lateralSpeed,
            std::max(std::abs(longitudinalSpeed), 0.50f));
        state.slipAngleDegrees = degrees(slipAngleRadians);

        const SurfaceProfile surface = surfaceProfile(
            hit.surfaceMaterial,
            hit.surfaceWetness,
            vehicle.surface);
        const float longitudinalRelaxation =
            wheel.tireModel.longitudinalRelaxationLength
            * surface.relaxationMultiplier;
        const float lateralRelaxation =
            wheel.tireModel.lateralRelaxationLength
            * surface.relaxationMultiplier;
        const float longitudinalBlend = relaxationBlend(
            longitudinalSpeed,
            longitudinalRelaxation,
            substepDeltaTime);
        const float lateralBlend = relaxationBlend(
            longitudinalSpeed,
            lateralRelaxation,
            substepDeltaTime);
        state.relaxedSlipRatio += (state.slipRatio
            - state.relaxedSlipRatio) * longitudinalBlend;
        state.relaxedSlipAngleDegrees += (state.slipAngleDegrees
            - state.relaxedSlipAngleDegrees) * lateralBlend;

        const float relaxedAngleRadians = radians(
            std::clamp(state.relaxedSlipAngleDegrees, -75.0f, 75.0f));
        TireContactInput tireInput;
        tireInput.normalLoad = suspensionForce;
        tireInput.longitudinalSlip = state.relaxedSlipRatio;
        tireInput.slipAngleRadians = relaxedAngleRadians;
        tireInput.frictionMultiplier = surface.frictionMultiplier;
        tireInput.stiffnessMultiplier = surface.stiffnessMultiplier;
        const TireForceResult tireResult = evaluateAdvancedRoadTire(
            wheel.tireModel,
            tireInput);

        float longitudinalForce = tireResult.longitudinalForce;
        // Slip angle becomes ill-conditioned as longitudinal speed approaches
        // zero. Retaining the normal relaxation-length force in that region
        // lets a previous cornering load repeatedly overshoot from side to
        // side while the car is braking to rest. Blend to a direct contact-
        // velocity damper below walking pace; the final friction-circle clamp
        // still bounds this force by the tire's current normal load and grip.
        const float lowSpeedBlend = smoothStep01(
            (std::abs(longitudinalSpeed) - kLowSpeedTireBlendStart)
            / (kLowSpeedTireBlendEnd - kLowSpeedTireBlendStart));
        const float lowSpeedLateralForce =
            -vehicle.description.lateralStiffness * lateralSpeed;
        float lateralForce = lowSpeedLateralForce
            + (tireResult.lateralForce - lowSpeedLateralForce)
                * lowSpeedBlend;
        longitudinalForce += -vehicle.description.rollingResistance
            * surface.rollingResistanceMultiplier
            * longitudinalSpeed;

        // Preserve the contact-force safety bound after the legacy rolling
        // resistance term is applied. Rolling resistance is kept separate
        // from pure/combined-slip telemetry, but it must not push the final
        // contact vector beyond the tire's available friction force.
        const float forceLimit = tireResult.effectiveFriction * suspensionForce;
        const float finalMagnitude = std::sqrt(
            longitudinalForce * longitudinalForce
            + lateralForce * lateralForce);
        if (finalMagnitude > forceLimit && finalMagnitude > kVectorEpsilon)
        {
            const float scaleToLimit = forceLimit / finalMagnitude;
            longitudinalForce *= scaleToLimit;
            lateralForce *= scaleToLimit;
        }

        state.effectiveFriction = tireResult.effectiveFriction;
        state.gripUtilization = tireResult.gripUtilization;
        state.pureLongitudinalForce = tireResult.pureLongitudinalForce;
        state.pureLateralForce = tireResult.pureLateralForce;
        state.combinedSlipScale = tireResult.combinedSlipScale;
        state.pneumaticTrail = tireResult.pneumaticTrail;
        state.aligningTorque = tireResult.aligningTorque;
        state.longitudinalForce = longitudinalForce;
        state.lateralForce = lateralForce;

        const heritage::math::Vec3 tireForce = add(
            scale(wheelForward, longitudinalForce),
            scale(wheelRight, lateralForce));
        bodies.applyImpulseAtPoint(
            vehicle.description.chassisBody,
            scale(tireForce, substepDeltaTime),
            state.contactPoint);

        const float tireReactionTorque = -longitudinalForce
            * description.radius;
        const float torqueWithoutBrake = driveTorque
            + differentialLockTorque
            + tireReactionTorque;
        const float projectedAngularVelocity = state.wheelAngularVelocity
            + (torqueWithoutBrake / wheel.tireModel.wheelInertia)
                * substepDeltaTime;
        // A brake is a bounded constraint, not a torque that is allowed to
        // overshoot zero and reverse the wheel every millisecond. Clamp the
        // requested reaction to exactly the torque needed to stop this step.
        const float brakeTorque = brakeTorqueMagnitude > 0.0f
            ? std::clamp(
                -projectedAngularVelocity * wheel.tireModel.wheelInertia
                    / substepDeltaTime,
                -brakeTorqueMagnitude,
                brakeTorqueMagnitude)
            : 0.0f;
        const float netWheelTorque = torqueWithoutBrake + brakeTorque;
        state.wheelAngularVelocity += (netWheelTorque
            / wheel.tireModel.wheelInertia) * substepDeltaTime;
        state.wheelAngularVelocity = std::clamp(
            state.wheelAngularVelocity,
            -4000.0f,
            4000.0f);
        if (brakeTorqueMagnitude > 0.0f
            && std::abs(state.wheelAngularVelocity) < 0.03f
            && std::abs(longitudinalSpeed) < 0.10f)
        {
            state.wheelAngularVelocity = 0.0f;
        }
        state.wheelRotationDegrees += degrees(
            state.wheelAngularVelocity * substepDeltaTime);
        if (std::abs(state.wheelRotationDegrees) > 36000.0f)
            state.wheelRotationDegrees = std::fmod(
                state.wheelRotationDegrees,
                360.0f);
    }

    vehicle.groundedWheelCount = groundedCount;
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

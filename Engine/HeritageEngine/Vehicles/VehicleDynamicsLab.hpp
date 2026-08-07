#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "../Core/Math/Math.hpp"

namespace heritage::vehicles {

// Quantities retained by the native high-rate vehicle recorder. Global
// metrics ignore wheelIndex; wheel metrics require a valid zero-based wheel.
enum class DynamicsLabMetric
{
    SpeedKph,
    LongitudinalVelocityMps,
    LateralVelocityMps,
    VerticalVelocityMps,
    RollRateDegreesPerSecond,
    PitchRateDegreesPerSecond,
    YawRateDegreesPerSecond,
    SteeringAngleDegrees,
    EngineRpm,
    WheelCompressionMillimeters,
    WheelSuspensionVelocityMps,
    WheelSuspensionSpringForceNewtons,
    WheelSuspensionDampingForceNewtons,
    WheelSuspensionBumpStopForceNewtons,
    WheelSuspensionDroopStopForceNewtons,
    WheelDamperDissipationWatts,
    WheelUnsprungVelocityMps,
    WheelTireDeflectionMillimeters,
    WheelTireDeflectionVelocityMps,
    WheelTireRadialDissipationWatts,
    WheelCamberDegrees,
    WheelToeDegrees,
    WheelNormalForceNewtons,
    WheelLongitudinalForceNewtons,
    WheelLateralForceNewtons,
    WheelSlipRatio,
    WheelSlipAngleDegrees,
    WheelGripUtilizationPercent,
    WheelAligningTorqueNewtonMeters
};

struct DynamicsLabWheelSample
{
    bool grounded = false;
    float compression = 0.0f;
    float suspensionVelocity = 0.0f;
    float suspensionSpringForce = 0.0f;
    float suspensionDampingForce = 0.0f;
    float suspensionBumpStopForce = 0.0f;
    float suspensionDroopStopForce = 0.0f;
    float damperDissipationWatts = 0.0f;
    float unsprungVelocity = 0.0f;
    float tireDeflection = 0.0f;
    float tireDeflectionVelocity = 0.0f;
    float tireRadialDissipationWatts = 0.0f;
    float camberDegrees = 0.0f;
    float toeDegrees = 0.0f;
    float normalForce = 0.0f;
    float longitudinalForce = 0.0f;
    float lateralForce = 0.0f;
    float steerAngleDegrees = 0.0f;
    float wheelAngularVelocity = 0.0f;
    float slipRatio = 0.0f;
    float slipAngleDegrees = 0.0f;
    float gripUtilization = 0.0f;
    float aligningTorque = 0.0f;
};

struct DynamicsLabFrame
{
    heritage::math::Vec3 position{};
    // Vehicle-local axes: X right, Y up, Z forward.
    heritage::math::Vec3 localVelocity{};
    float rollRateDegreesPerSecond = 0.0f;
    float pitchRateDegreesPerSecond = 0.0f;
    float yawRateDegreesPerSecond = 0.0f;
    float speedMps = 0.0f;
    float steeringInput = 0.0f;
    float steeringAngleDegrees = 0.0f;
    float throttleInput = 0.0f;
    float brakeInput = 0.0f;
    float handbrakeInput = 0.0f;
    float engineRpm = 0.0f;
    std::vector<DynamicsLabWheelSample> wheels;
};

struct DynamicsLabSample : DynamicsLabFrame
{
    double timeSeconds = 0.0;
};

struct DynamicsLabSummary
{
    bool recording = false;
    bool captureComplete = false;
    std::size_t sampleCount = 0;
    std::size_t sampleCapacity = 0;
    std::size_t wheelCount = 0;
    double durationSeconds = 0.0;
    float requestedCaptureHertz = 0.0f;
    float peakSpeedKph = 0.0f;
    float peakAbsoluteRollRateDegreesPerSecond = 0.0f;
    float peakAbsolutePitchRateDegreesPerSecond = 0.0f;
    float peakAbsoluteYawRateDegreesPerSecond = 0.0f;
    float peakAbsoluteSuspensionVelocityMps = 0.0f;
    float peakSuspensionTravelStopForceNewtons = 0.0f;
    float peakDamperDissipationWatts = 0.0f;
    float peakAbsoluteUnsprungVelocityMps = 0.0f;
    float peakTireDeflectionMillimeters = 0.0f;
    float peakTireRadialDissipationWatts = 0.0f;
    float peakAbsoluteCamberDegrees = 0.0f;
    float peakAbsoluteToeDegrees = 0.0f;
    float peakAbsoluteSlipRatio = 0.0f;
    float peakAbsoluteSlipAngleDegrees = 0.0f;
    float peakGripUtilizationPercent = 0.0f;
    float minimumGroundedNormalForceNewtons = 0.0f;
    float maximumGroundedNormalForceNewtons = 0.0f;
    std::size_t groundContactLossEvents = 0;
};

// An opt-in bounded recorder. When it is inactive, VehicleSystem pays only one
// predictable branch per high-rate vehicle step. Captures remain in memory
// until explicitly cleared or replaced and can be exported as ordinary CSV.
class VehicleDynamicsLab
{
public:
    bool start(
        float maximumDurationSeconds,
        float captureHertz,
        std::size_t wheelCount);
    void stop();
    void clear();

    void capture(float sourceDeltaTime, const DynamicsLabFrame& frame);

    bool recording() const { return m_recording; }
    bool captureComplete() const { return m_captureComplete; }
    std::size_t sampleCount() const { return m_samples.size(); }
    const std::vector<DynamicsLabSample>& samples() const { return m_samples; }
    DynamicsLabSummary summary() const;

    bool metricSeries(
        DynamicsLabMetric metric,
        std::size_t wheelIndex,
        std::size_t maximumPoints,
        std::vector<float>& values) const;
    bool exportCsv(const std::filesystem::path& path);

    const std::string& lastError() const { return m_lastError; }

private:
    static bool wheelMetric(DynamicsLabMetric metric);
    static float metricValue(
        const DynamicsLabSample& sample,
        DynamicsLabMetric metric,
        std::size_t wheelIndex);
    void updateSummary(const DynamicsLabSample& sample);
    void setError(const std::string& message);

    std::vector<DynamicsLabSample> m_samples;
    std::vector<bool> m_previousGrounded;
    DynamicsLabSummary m_summary;
    double m_captureAccumulatorSeconds = 0.0;
    double m_captureIntervalSeconds = 0.001;
    bool m_haveGroundContactHistory = false;
    bool m_recording = false;
    bool m_captureComplete = false;
    std::string m_lastError;
};

const char* dynamicsLabMetricName(DynamicsLabMetric metric);
bool parseDynamicsLabMetric(const std::string& text, DynamicsLabMetric& metric);

} // namespace heritage::vehicles

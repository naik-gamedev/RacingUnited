#include "VehicleDynamicsLab.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <system_error>

namespace heritage::vehicles {
namespace {

constexpr float kMinimumDurationSeconds = 0.25f;
constexpr float kMaximumDurationSeconds = 120.0f;
constexpr float kMinimumCaptureHertz = 30.0f;
constexpr float kMaximumCaptureHertz = 2000.0f;
constexpr std::size_t kMaximumSamples = 240000;
constexpr std::size_t kMaximumWheels = 32;
constexpr double kCaptureEpsilon = 1.0e-12;

bool finiteFloat(float value)
{
    return std::isfinite(static_cast<double>(value));
}

float absolute(float value)
{
    return std::abs(value);
}

std::string normalizedMetricName(const std::string& text)
{
    std::string result;
    result.reserve(text.size());
    for (const unsigned char character : text)
    {
        if (character >= 'A' && character <= 'Z')
            result.push_back(static_cast<char>(character - 'A' + 'a'));
        else if (character == '-' || character == ' ')
            result.push_back('_');
        else
            result.push_back(static_cast<char>(character));
    }
    return result;
}

} // namespace

bool VehicleDynamicsLab::start(
    float maximumDurationSeconds,
    float captureHertz,
    std::size_t wheelCount)
{
    if (!finiteFloat(maximumDurationSeconds)
        || maximumDurationSeconds < kMinimumDurationSeconds
        || maximumDurationSeconds > kMaximumDurationSeconds)
    {
        setError("Dynamics lab duration must be between 0.25 and 120 seconds.");
        return false;
    }
    if (!finiteFloat(captureHertz)
        || captureHertz < kMinimumCaptureHertz
        || captureHertz > kMaximumCaptureHertz)
    {
        setError("Dynamics lab capture rate must be between 30 and 2000 Hz.");
        return false;
    }
    if (wheelCount == 0 || wheelCount > kMaximumWheels)
    {
        setError("Dynamics lab capture requires between 1 and 32 wheels.");
        return false;
    }

    const double requestedSamples = std::ceil(
        static_cast<double>(maximumDurationSeconds)
        * static_cast<double>(captureHertz));
    if (requestedSamples < 1.0
        || requestedSamples > static_cast<double>(kMaximumSamples))
    {
        setError("Dynamics lab capture would exceed the 240000-sample safety limit.");
        return false;
    }

    clear();
    m_summary.sampleCapacity = static_cast<std::size_t>(requestedSamples);
    m_summary.wheelCount = wheelCount;
    m_summary.requestedCaptureHertz = captureHertz;
    m_captureIntervalSeconds = 1.0 / static_cast<double>(captureHertz);
    m_samples.reserve(m_summary.sampleCapacity);
    m_previousGrounded.assign(wheelCount, false);
    m_recording = true;
    m_summary.recording = true;
    return true;
}

void VehicleDynamicsLab::stop()
{
    m_recording = false;
    m_summary.recording = false;
}

void VehicleDynamicsLab::clear()
{
    m_samples.clear();
    m_samples.shrink_to_fit();
    m_previousGrounded.clear();
    m_summary = {};
    m_captureAccumulatorSeconds = 0.0;
    m_captureIntervalSeconds = 0.001;
    m_haveGroundContactHistory = false;
    m_recording = false;
    m_captureComplete = false;
    m_lastError.clear();
}

void VehicleDynamicsLab::capture(
    float sourceDeltaTime,
    const DynamicsLabFrame& frame)
{
    if (!m_recording)
        return;
    if (!finiteFloat(sourceDeltaTime) || sourceDeltaTime <= 0.0f)
        return;
    if (frame.wheels.size() != m_summary.wheelCount)
    {
        setError("Dynamics lab wheel count changed during a capture.");
        stop();
        return;
    }

    m_captureAccumulatorSeconds += static_cast<double>(sourceDeltaTime);
    while (m_recording
        && m_captureAccumulatorSeconds + kCaptureEpsilon
            >= m_captureIntervalSeconds)
    {
        m_captureAccumulatorSeconds -= m_captureIntervalSeconds;
        if (m_captureAccumulatorSeconds < 0.0)
            m_captureAccumulatorSeconds = 0.0;

        DynamicsLabSample sample;
        static_cast<DynamicsLabFrame&>(sample) = frame;
        sample.timeSeconds = static_cast<double>(m_samples.size() + 1)
            * m_captureIntervalSeconds;
        m_samples.push_back(std::move(sample));
        updateSummary(m_samples.back());

        if (m_samples.size() >= m_summary.sampleCapacity)
        {
            m_captureComplete = true;
            m_summary.captureComplete = true;
            stop();
        }
    }
}

DynamicsLabSummary VehicleDynamicsLab::summary() const
{
    DynamicsLabSummary result = m_summary;
    result.recording = m_recording;
    result.captureComplete = m_captureComplete;
    result.sampleCount = m_samples.size();
    result.durationSeconds = m_samples.empty()
        ? 0.0
        : m_samples.back().timeSeconds;
    return result;
}

bool VehicleDynamicsLab::metricSeries(
    DynamicsLabMetric metric,
    std::size_t wheelIndex,
    std::size_t maximumPoints,
    std::vector<float>& values) const
{
    values.clear();
    if (m_samples.empty())
        return true;
    if (maximumPoints == 0)
        return false;
    if (wheelMetric(metric) && wheelIndex >= m_summary.wheelCount)
        return false;

    const std::size_t bucketSize = std::max<std::size_t>(
        1,
        (m_samples.size() + maximumPoints - 1) / maximumPoints);
    values.reserve((m_samples.size() + bucketSize - 1) / bucketSize);
    for (std::size_t begin = 0; begin < m_samples.size(); begin += bucketSize)
    {
        const std::size_t end = std::min(m_samples.size(), begin + bucketSize);
        float selected = metricValue(m_samples[begin], metric, wheelIndex);
        for (std::size_t index = begin + 1; index < end; ++index)
        {
            const float candidate = metricValue(
                m_samples[index], metric, wheelIndex);
            if (std::abs(candidate) > std::abs(selected))
                selected = candidate;
        }
        values.push_back(selected);
    }
    return true;
}

bool VehicleDynamicsLab::exportCsv(const std::filesystem::path& path)
{
    if (path.empty())
    {
        setError("Dynamics lab export requires a non-empty path.");
        return false;
    }
    if (m_samples.empty())
    {
        setError("Dynamics lab has no samples to export.");
        return false;
    }

    std::error_code error;
    if (!path.parent_path().empty())
    {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
        {
            setError("Could not create the dynamics lab export directory: "
                + error.message());
            return false;
        }
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        setError("Could not open the dynamics lab CSV export.");
        return false;
    }

    output << "time_s,position_x_m,position_y_m,position_z_m"
        << ",local_longitudinal_mps,local_lateral_mps,local_vertical_mps"
        << ",speed_kph,roll_rate_degps,pitch_rate_degps,yaw_rate_degps"
        << ",steering_input,steering_angle_deg,throttle,brake,handbrake,engine_rpm";
    for (std::size_t wheel = 0; wheel < m_summary.wheelCount; ++wheel)
    {
        const std::string prefix = ",wheel_" + std::to_string(wheel + 1);
        output << prefix << "_grounded"
            << prefix << "_compression_m"
            << prefix << "_suspension_velocity_mps"
            << prefix << "_suspension_spring_force_n"
            << prefix << "_suspension_damping_force_n"
            << prefix << "_suspension_bump_stop_force_n"
            << prefix << "_suspension_droop_stop_force_n"
            << prefix << "_damper_dissipation_w"
            << prefix << "_unsprung_velocity_mps"
            << prefix << "_tire_deflection_m"
            << prefix << "_tire_deflection_velocity_mps"
            << prefix << "_tire_radial_dissipation_w"
            << prefix << "_normal_force_n"
            << prefix << "_longitudinal_force_n"
            << prefix << "_lateral_force_n"
            << prefix << "_steer_angle_deg"
            << prefix << "_angular_velocity_radps"
            << prefix << "_slip_ratio"
            << prefix << "_slip_angle_deg"
            << prefix << "_grip_utilization"
            << prefix << "_aligning_torque_nm";
    }
    output << '\n' << std::fixed << std::setprecision(6);

    for (const DynamicsLabSample& sample : m_samples)
    {
        output << sample.timeSeconds
            << ',' << sample.position.x
            << ',' << sample.position.y
            << ',' << sample.position.z
            << ',' << sample.localVelocity.z
            << ',' << sample.localVelocity.x
            << ',' << sample.localVelocity.y
            << ',' << sample.speedMps * 3.6f
            << ',' << sample.rollRateDegreesPerSecond
            << ',' << sample.pitchRateDegreesPerSecond
            << ',' << sample.yawRateDegreesPerSecond
            << ',' << sample.steeringInput
            << ',' << sample.steeringAngleDegrees
            << ',' << sample.throttleInput
            << ',' << sample.brakeInput
            << ',' << sample.handbrakeInput
            << ',' << sample.engineRpm;
        for (const DynamicsLabWheelSample& wheel : sample.wheels)
        {
            output << ',' << (wheel.grounded ? 1 : 0)
                << ',' << wheel.compression
                << ',' << wheel.suspensionVelocity
                << ',' << wheel.suspensionSpringForce
                << ',' << wheel.suspensionDampingForce
                << ',' << wheel.suspensionBumpStopForce
                << ',' << wheel.suspensionDroopStopForce
                << ',' << wheel.damperDissipationWatts
                << ',' << wheel.unsprungVelocity
                << ',' << wheel.tireDeflection
                << ',' << wheel.tireDeflectionVelocity
                << ',' << wheel.tireRadialDissipationWatts
                << ',' << wheel.normalForce
                << ',' << wheel.longitudinalForce
                << ',' << wheel.lateralForce
                << ',' << wheel.steerAngleDegrees
                << ',' << wheel.wheelAngularVelocity
                << ',' << wheel.slipRatio
                << ',' << wheel.slipAngleDegrees
                << ',' << wheel.gripUtilization
                << ',' << wheel.aligningTorque;
        }
        output << '\n';
    }

    if (!output)
    {
        setError("Writing the dynamics lab CSV export failed.");
        return false;
    }

    m_lastError.clear();
    return true;
}

bool VehicleDynamicsLab::wheelMetric(DynamicsLabMetric metric)
{
    return static_cast<int>(metric)
        >= static_cast<int>(DynamicsLabMetric::WheelCompressionMillimeters);
}

float VehicleDynamicsLab::metricValue(
    const DynamicsLabSample& sample,
    DynamicsLabMetric metric,
    std::size_t wheelIndex)
{
    switch (metric)
    {
    case DynamicsLabMetric::SpeedKph:
        return sample.speedMps * 3.6f;
    case DynamicsLabMetric::LongitudinalVelocityMps:
        return sample.localVelocity.z;
    case DynamicsLabMetric::LateralVelocityMps:
        return sample.localVelocity.x;
    case DynamicsLabMetric::VerticalVelocityMps:
        return sample.localVelocity.y;
    case DynamicsLabMetric::RollRateDegreesPerSecond:
        return sample.rollRateDegreesPerSecond;
    case DynamicsLabMetric::PitchRateDegreesPerSecond:
        return sample.pitchRateDegreesPerSecond;
    case DynamicsLabMetric::YawRateDegreesPerSecond:
        return sample.yawRateDegreesPerSecond;
    case DynamicsLabMetric::SteeringAngleDegrees:
        return sample.steeringAngleDegrees;
    case DynamicsLabMetric::EngineRpm:
        return sample.engineRpm;
    default:
        break;
    }

    if (wheelIndex >= sample.wheels.size())
        return 0.0f;
    const DynamicsLabWheelSample& wheel = sample.wheels[wheelIndex];
    switch (metric)
    {
    case DynamicsLabMetric::WheelCompressionMillimeters:
        return wheel.compression * 1000.0f;
    case DynamicsLabMetric::WheelSuspensionVelocityMps:
        return wheel.suspensionVelocity;
    case DynamicsLabMetric::WheelSuspensionSpringForceNewtons:
        return wheel.suspensionSpringForce;
    case DynamicsLabMetric::WheelSuspensionDampingForceNewtons:
        return wheel.suspensionDampingForce;
    case DynamicsLabMetric::WheelSuspensionBumpStopForceNewtons:
        return wheel.suspensionBumpStopForce;
    case DynamicsLabMetric::WheelSuspensionDroopStopForceNewtons:
        return wheel.suspensionDroopStopForce;
    case DynamicsLabMetric::WheelDamperDissipationWatts:
        return wheel.damperDissipationWatts;
    case DynamicsLabMetric::WheelUnsprungVelocityMps:
        return wheel.unsprungVelocity;
    case DynamicsLabMetric::WheelTireDeflectionMillimeters:
        return wheel.tireDeflection * 1000.0f;
    case DynamicsLabMetric::WheelTireDeflectionVelocityMps:
        return wheel.tireDeflectionVelocity;
    case DynamicsLabMetric::WheelTireRadialDissipationWatts:
        return wheel.tireRadialDissipationWatts;
    case DynamicsLabMetric::WheelNormalForceNewtons:
        return wheel.normalForce;
    case DynamicsLabMetric::WheelLongitudinalForceNewtons:
        return wheel.longitudinalForce;
    case DynamicsLabMetric::WheelLateralForceNewtons:
        return wheel.lateralForce;
    case DynamicsLabMetric::WheelSlipRatio:
        return wheel.slipRatio;
    case DynamicsLabMetric::WheelSlipAngleDegrees:
        return wheel.slipAngleDegrees;
    case DynamicsLabMetric::WheelGripUtilizationPercent:
        return wheel.gripUtilization * 100.0f;
    case DynamicsLabMetric::WheelAligningTorqueNewtonMeters:
        return wheel.aligningTorque;
    default:
        return 0.0f;
    }
}

void VehicleDynamicsLab::updateSummary(const DynamicsLabSample& sample)
{
    m_summary.peakSpeedKph = std::max(
        m_summary.peakSpeedKph,
        sample.speedMps * 3.6f);
    m_summary.peakAbsoluteRollRateDegreesPerSecond = std::max(
        m_summary.peakAbsoluteRollRateDegreesPerSecond,
        absolute(sample.rollRateDegreesPerSecond));
    m_summary.peakAbsolutePitchRateDegreesPerSecond = std::max(
        m_summary.peakAbsolutePitchRateDegreesPerSecond,
        absolute(sample.pitchRateDegreesPerSecond));
    m_summary.peakAbsoluteYawRateDegreesPerSecond = std::max(
        m_summary.peakAbsoluteYawRateDegreesPerSecond,
        absolute(sample.yawRateDegreesPerSecond));

    for (std::size_t index = 0; index < sample.wheels.size(); ++index)
    {
        const DynamicsLabWheelSample& wheel = sample.wheels[index];
        m_summary.peakAbsoluteSuspensionVelocityMps = std::max(
            m_summary.peakAbsoluteSuspensionVelocityMps,
            absolute(wheel.suspensionVelocity));
        m_summary.peakSuspensionTravelStopForceNewtons = std::max(
            m_summary.peakSuspensionTravelStopForceNewtons,
            std::max(
                wheel.suspensionBumpStopForce,
                wheel.suspensionDroopStopForce));
        m_summary.peakDamperDissipationWatts = std::max(
            m_summary.peakDamperDissipationWatts,
            wheel.damperDissipationWatts);
        m_summary.peakAbsoluteUnsprungVelocityMps = std::max(
            m_summary.peakAbsoluteUnsprungVelocityMps,
            absolute(wheel.unsprungVelocity));
        m_summary.peakTireDeflectionMillimeters = std::max(
            m_summary.peakTireDeflectionMillimeters,
            wheel.tireDeflection * 1000.0f);
        m_summary.peakTireRadialDissipationWatts = std::max(
            m_summary.peakTireRadialDissipationWatts,
            wheel.tireRadialDissipationWatts);
        m_summary.peakAbsoluteSlipRatio = std::max(
            m_summary.peakAbsoluteSlipRatio,
            absolute(wheel.slipRatio));
        m_summary.peakAbsoluteSlipAngleDegrees = std::max(
            m_summary.peakAbsoluteSlipAngleDegrees,
            absolute(wheel.slipAngleDegrees));
        m_summary.peakGripUtilizationPercent = std::max(
            m_summary.peakGripUtilizationPercent,
            wheel.gripUtilization * 100.0f);
        if (wheel.grounded)
        {
            if (m_summary.maximumGroundedNormalForceNewtons <= 0.0f)
                m_summary.minimumGroundedNormalForceNewtons = wheel.normalForce;
            m_summary.minimumGroundedNormalForceNewtons = std::min(
                m_summary.minimumGroundedNormalForceNewtons,
                wheel.normalForce);
            m_summary.maximumGroundedNormalForceNewtons = std::max(
                m_summary.maximumGroundedNormalForceNewtons,
                wheel.normalForce);
        }
        if (m_haveGroundContactHistory
            && index < m_previousGrounded.size()
            && m_previousGrounded[index]
            && !wheel.grounded)
        {
            ++m_summary.groundContactLossEvents;
        }
        if (index < m_previousGrounded.size())
            m_previousGrounded[index] = wheel.grounded;
    }
    m_haveGroundContactHistory = true;
}

void VehicleDynamicsLab::setError(const std::string& message)
{
    m_lastError = message;
}

const char* dynamicsLabMetricName(DynamicsLabMetric metric)
{
    switch (metric)
    {
    case DynamicsLabMetric::SpeedKph: return "speed_kph";
    case DynamicsLabMetric::LongitudinalVelocityMps: return "longitudinal_velocity_mps";
    case DynamicsLabMetric::LateralVelocityMps: return "lateral_velocity_mps";
    case DynamicsLabMetric::VerticalVelocityMps: return "vertical_velocity_mps";
    case DynamicsLabMetric::RollRateDegreesPerSecond: return "roll_rate_degps";
    case DynamicsLabMetric::PitchRateDegreesPerSecond: return "pitch_rate_degps";
    case DynamicsLabMetric::YawRateDegreesPerSecond: return "yaw_rate_degps";
    case DynamicsLabMetric::SteeringAngleDegrees: return "steering_angle_deg";
    case DynamicsLabMetric::EngineRpm: return "engine_rpm";
    case DynamicsLabMetric::WheelCompressionMillimeters: return "wheel_compression_mm";
    case DynamicsLabMetric::WheelSuspensionVelocityMps: return "wheel_suspension_velocity_mps";
    case DynamicsLabMetric::WheelSuspensionSpringForceNewtons: return "wheel_suspension_spring_force_n";
    case DynamicsLabMetric::WheelSuspensionDampingForceNewtons: return "wheel_suspension_damping_force_n";
    case DynamicsLabMetric::WheelSuspensionBumpStopForceNewtons: return "wheel_suspension_bump_stop_force_n";
    case DynamicsLabMetric::WheelSuspensionDroopStopForceNewtons: return "wheel_suspension_droop_stop_force_n";
    case DynamicsLabMetric::WheelDamperDissipationWatts: return "wheel_damper_dissipation_w";
    case DynamicsLabMetric::WheelUnsprungVelocityMps: return "wheel_unsprung_velocity_mps";
    case DynamicsLabMetric::WheelTireDeflectionMillimeters: return "wheel_tire_deflection_mm";
    case DynamicsLabMetric::WheelTireDeflectionVelocityMps: return "wheel_tire_deflection_velocity_mps";
    case DynamicsLabMetric::WheelTireRadialDissipationWatts: return "wheel_tire_radial_dissipation_w";
    case DynamicsLabMetric::WheelNormalForceNewtons: return "wheel_normal_force_n";
    case DynamicsLabMetric::WheelLongitudinalForceNewtons: return "wheel_longitudinal_force_n";
    case DynamicsLabMetric::WheelLateralForceNewtons: return "wheel_lateral_force_n";
    case DynamicsLabMetric::WheelSlipRatio: return "wheel_slip_ratio";
    case DynamicsLabMetric::WheelSlipAngleDegrees: return "wheel_slip_angle_deg";
    case DynamicsLabMetric::WheelGripUtilizationPercent: return "wheel_grip_percent";
    case DynamicsLabMetric::WheelAligningTorqueNewtonMeters: return "wheel_aligning_torque_nm";
    default: return "unknown";
    }
}

bool parseDynamicsLabMetric(
    const std::string& text,
    DynamicsLabMetric& metric)
{
    const std::string normalized = normalizedMetricName(text);
    for (int value = static_cast<int>(DynamicsLabMetric::SpeedKph);
        value <= static_cast<int>(
            DynamicsLabMetric::WheelAligningTorqueNewtonMeters);
        ++value)
    {
        const auto candidate = static_cast<DynamicsLabMetric>(value);
        if (normalized == dynamicsLabMetricName(candidate))
        {
            metric = candidate;
            return true;
        }
    }
    return false;
}

} // namespace heritage::vehicles
